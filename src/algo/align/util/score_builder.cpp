/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/align/util/score_builder.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_options.h>

#include <util/sequtil/sequtil_manip.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/feat_ci.hpp>

#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objects/seqfeat/Genetic_code_table.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

void CScoreBuilder::x_Initialize(CBlastOptionsHandle& options)
{
    Int2 status;
    const CBlastOptions& opts = options.GetOptions();

    m_GapOpen = opts.GetGapOpeningCost();
    m_GapExtend = opts.GetGapExtensionCost();

    // alignments are either protein-protein or nucl-nucl

    m_BlastType = opts.GetProgram();
    if (m_BlastType == eBlastn || m_BlastType == eMegablast ||
                                m_BlastType == eDiscMegablast) {
        m_BlastType = eBlastn;
    } else {
        m_BlastType = eBlastp;
    }

    if (m_BlastType == eBlastn) {
        m_ScoreBlk = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1);
    } else {
        m_ScoreBlk = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    }
    if (m_ScoreBlk == NULL) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize blast score block");
    }

    // fill in the score matrix

    EBlastProgramType core_type = EProgramToEBlastProgramType(m_BlastType);
    BlastScoringOptions *score_options;
    BlastScoringOptionsNew(core_type, &score_options);
    BLAST_FillScoringOptions(score_options, core_type, TRUE,
                            opts.GetMismatchPenalty(),
                            opts.GetMatchReward(),
                            opts.GetMatrixName(),
                            m_GapOpen, m_GapExtend);
    status = Blast_ScoreBlkMatrixInit(core_type, score_options,
                                      m_ScoreBlk, NULL);
    score_options = BlastScoringOptionsFree(score_options);
    if (status) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize score matrix");
    }

    // fill in Karlin blocks

    m_ScoreBlk->kbp_gap_std[0] = Blast_KarlinBlkNew();
    if (m_BlastType == eBlastn) {
        // the following computes the same Karlin blocks as blast
        // if the gap penalties are not large. When the penalties are
        // large the ungapped Karlin blocks are used, but these require
        // sequence data to be computed exactly. Instead we build an
        // ideal ungapped Karlin block to approximate the exact answer
        Blast_ScoreBlkKbpIdealCalc(m_ScoreBlk);
        status = Blast_KarlinBlkNuclGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                               m_GapOpen, m_GapExtend,
                                               m_ScoreBlk->reward,
                                               m_ScoreBlk->penalty,
                                               m_ScoreBlk->kbp_ideal,
                                               &(m_ScoreBlk->round_down),
                                               NULL);
    }
    else {
        status = Blast_KarlinBlkGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                           m_GapOpen, m_GapExtend,
                                           m_ScoreBlk->name, NULL);
    }
    if (status || m_ScoreBlk->kbp_gap_std[0] == NULL ||
        m_ScoreBlk->kbp_gap_std[0]->Lambda <= 0.0) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize Karlin blocks");
    }
}

/// Default constructor
CScoreBuilder::CScoreBuilder()
    : m_ScoreBlk(0),
      m_EffectiveSearchSpace(0)
{
}

/// Constructor (uses blast program defaults)
CScoreBuilder::CScoreBuilder(EProgram blast_program)
    : m_EffectiveSearchSpace(0)
{
    CRef<CBlastOptionsHandle>
                        options(CBlastOptionsFactory::Create(blast_program));
    x_Initialize(*options);
}

/// Constructor (uses previously configured blast options)
CScoreBuilder::CScoreBuilder(CBlastOptionsHandle& options)
    : m_EffectiveSearchSpace(0)
{
    x_Initialize(options);
}

/// Destructor
CScoreBuilder::~CScoreBuilder()
{
    m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
}

///
/// calculate mismatches and identities in a seq-align
///

static void s_GetNucIdentityMismatch(const vector<string>& data,
                                     int* identities,
                                     int* mismatches)
{
    for (size_t a = 0;  a < data[0].size();  ++a) {
        bool is_mismatch = false;
        for (size_t b = 1;  b < data.size();  ++b) {
            if (data[b][a] != data[0][a]) {
                is_mismatch = true;
                break;
            }
        }

        if (is_mismatch) {
            ++(*mismatches);
        } else {
            ++(*identities);
        }
    }
}


static void s_GetSplicedSegIdentityMismatch(CScope& scope,
                                            const CSeq_align& align,
                                            int* identities,
                                            int* mismatches)
{
    ///
    /// easy route
    /// use the alignment manager
    ///
    TAlnSeqIdIRef id1(new CAlnSeqId(align.GetSeq_id(0)));
    TAlnSeqIdIRef id2(new CAlnSeqId(align.GetSeq_id(1)));
    CRef<CPairwiseAln> pairwise(new CPairwiseAln(id1, id2));
    ConvertSeqAlignToPairwiseAln(*pairwise, align, 0, 1);

    CBioseq_Handle prod_bsh    = scope.GetBioseqHandle(align.GetSeq_id(0));
    CBioseq_Handle genomic_bsh = scope.GetBioseqHandle(align.GetSeq_id(1));
    if ( !prod_bsh  ||  !genomic_bsh ) {
        return;
    }

    CSeqVector prod(prod_bsh, CBioseq_Handle::eCoding_Iupac);

    const CTrans_table& tbl = CGen_code_table::GetTransTable(1);

    switch (align.GetSegs().GetSpliced().GetProduct_type()) {
    case CSpliced_seg::eProduct_type_transcript:
        {{
             CSeqVector gen(genomic_bsh, CBioseq_Handle::eCoding_Iupac);
             ITERATE (CPairwiseAln, it, *pairwise) {
                 const CPairwiseAln::TAlnRng& range = *it;
                 TSeqRange r1(range.GetFirstFrom(), range.GetFirstTo());
                 TSeqRange r2(range.GetSecondFrom(), range.GetSecondTo());

                 string prod_data;
                 prod.GetSeqData(r1.GetFrom(), r1.GetTo() + 1,
                                 prod_data);
                 string gen_data;
                 gen.GetSeqData(r2.GetFrom(), r2.GetTo() + 1,
                                 gen_data);
                 if (range.IsReversed()) {
                     CSeqManip::ReverseComplement(gen_data,
                                                  CSeqUtil::e_Iupacna,
                                                  0, gen_data.size());
                 }

                 string::const_iterator pit = prod_data.begin();
                 string::const_iterator pit_end = prod_data.end();
                 string::const_iterator git = gen_data.begin();
                 string::const_iterator git_end = gen_data.end();

                 for ( ;  pit != pit_end  &&  git != git_end;  ++pit, ++git) {
                     bool match = (*pit == *git);
                     *identities +=  match;
                     *mismatches += !match;
                 }
             }
         }}
        break;

    case CSpliced_seg::eProduct_type_protein:
        {{
             char codon[3];
             codon[0] = codon[1] = codon[2] = 'N';

             TSeqRange last_r1(0, 0);
             ITERATE (CPairwiseAln, it, *pairwise) {
                 const CPairwiseAln::TAlnRng& range = *it;
                 TSeqRange r1(range.GetFirstFrom(), range.GetFirstTo());
                 TSeqRange r2(range.GetSecondFrom(), range.GetSecondTo());

                 if (last_r1.GetTo() + 1 != r1.GetFrom()) {
                     size_t i = last_r1.GetTo() + 1;
                     size_t count = 0;
                     for ( ;  i != r1.GetFrom()  &&  count < 3;  ++i, ++count) {
                         codon[ i % 3 ] = 'N';
                     }
                 }
                 last_r1 = r1;

                 string gen_data;
                 CSeqVector gen(genomic_bsh, CBioseq_Handle::eCoding_Iupac);
                 gen.GetSeqData(r2.GetFrom(), r2.GetTo() + 1, gen_data);
                 if (range.IsReversed()) {
                     CSeqManip::ReverseComplement(gen_data,
                                                  CSeqUtil::e_Iupacna,
                                                  0, gen_data.size());

                     //LOG_POST(Error << "reverse range: [" << r1.GetFrom() << ", " << r1.GetTo() << "] - [" << r2.GetFrom() << ", " << r2.GetTo() << "]");
                 } else {
                     //LOG_POST(Error << "forward range: [" << r1.GetFrom() << ", " << r1.GetTo() << "] - [" << r2.GetFrom() << ", " << r2.GetTo() << "]");
                 }

                 /// compare product range to conceptual translation
                 TSeqPos prod_pos = r1.GetFrom();
                 //LOG_POST(Error << "  genomic = " << gen_data);
                 for (size_t i = 0;  i < gen_data.size();  ++i, ++prod_pos) {
                     codon[ prod_pos % 3 ] = gen_data[i];
                     //LOG_POST(Error << "    filling: " << prod_pos << ": " << prod_pos % 3 << ": " << gen_data[i]);

                     if (prod_pos % 3 == 2) {
                         char residue = tbl.GetCodonResidue
                             (tbl.SetCodonState(codon[0], codon[1], codon[2]));

                         /// NOTE:
                         /// we increment identities/mismatches by 3 here,
                         /// counting identities in nucleotide space!!
                         bool is_match = false;
                         if (residue == prod[prod_pos / 3]  &&
                             residue != 'X'  &&  residue != '-') {
                             *identities += 3;
                             is_match = true;
                         } else {
                             *mismatches += 3;
                         }

                         /**
                         LOG_POST(Error << "  codon = "
                                  << codon[0] << '-'
                                  << codon[1] << '-'
                                  << codon[2]
                                  << "  residue = " << residue
                                  << "  actual = " << prod[prod_pos / 3]
                                  << (is_match ? " (match)" : ""));
                                  **/
                     }
                 }
             }
         }}
        break;

    default:
        break;
    }

    /*
     * NB: leave this here; it's useful for validation
    int actual_identities = 0;
    if (align.GetNamedScore("N of matches", actual_identities)) {
        if (actual_identities != *identities) {
            LOG_POST(Error << "actual identities: " << actual_identities
                     << "  computed identities: " << *identities);

            //cerr << MSerial_AsnText << align;
        }
    }
    **/
}


static void s_GetCountIdentityMismatch(CScope& scope, const CSeq_align& align,
                                       int* identities, int* mismatches)
{
    _ASSERT(identities  &&  mismatches);

    {{
         ///
         /// shortcut: if 'num_ident' is present, we trust it
         ///
         int num_ident = 0;
         if (align.GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident)) {
             size_t len     = align.GetAlignLength(false /*ignore gaps*/);
             *identities += num_ident;
             *mismatches += (len - num_ident);
             return;
         }
     }}

    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            CAlnVec vec(ds, scope);
            for (int seg = 0;  seg < vec.GetNumSegs();  ++seg) {
                vector<string> data;
                for (int i = 0;  i < vec.GetNumRows();  ++i) {
                    TSeqPos start = vec.GetStart(i, seg);
                    if (start == (TSeqPos)(-1)) {
                        /// we compute ungapped identities
                        /// gap on at least one row, so we skip this segment
                        break;
                    }
                    TSeqPos stop  = vec.GetStop(i, seg);
                    if (start == stop) {
                        break;
                    }

                    data.push_back(string());
                    vec.GetSeqString(data.back(), i, start, stop);
                }

                if (data.size() == (size_t)ds.GetDim()) {
                    s_GetNucIdentityMismatch(data, identities, mismatches);
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter,
                     align.GetSegs().GetDisc().Get()) {
                s_GetCountIdentityMismatch(scope, **iter,
                                           identities, mismatches);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        NCBI_THROW(CException, eUnknown,
                   "identity + mismatch function unimplemented for std-seg");
        break;

    case CSeq_align::TSegs::e_Spliced:
        {{
             int aln_identities = 0;
             int aln_mismatches = 0;
             bool has_non_standard = false;
             ITERATE (CSpliced_seg::TExons, iter,
                      align.GetSegs().GetSpliced().GetExons()) {
                 const CSpliced_exon& exon = **iter;
                 if (exon.IsSetParts()) {
                     ITERATE (CSpliced_exon::TParts, it, exon.GetParts()) {
                         const CSpliced_exon_chunk& chunk = **it;
                         switch (chunk.Which()) {
                         case CSpliced_exon_chunk::e_Match:
                             aln_identities += chunk.GetMatch();
                             break;
                         case CSpliced_exon_chunk::e_Mismatch:
                             aln_mismatches += chunk.GetMismatch();
                             break;

                         case CSpliced_exon_chunk::e_Diag:
                             has_non_standard = true;
                             break;

                         default:
                             break;
                         }
                     }
                 } else {
                     aln_identities +=
                         exon.GetGenomic_end() - exon.GetGenomic_start() + 1;
                 }
             }

             if ( !has_non_standard ) {
                 *identities += aln_identities;
                 *mismatches += aln_mismatches;
             } else {
                 /// we must compute match and mismatch based on first
                 /// prinicples
                 s_GetSplicedSegIdentityMismatch(scope, align,
                                                 identities, mismatches);
             }
         }}
        break;

    default:
        _ASSERT(false);
        break;
    }
}



///
/// calculate the percent identity
/// we also return the count of identities and mismatches
///
static void s_GetPercentIdentity(CScope& scope, const CSeq_align& align,
                                 int* identities,
                                 int* mismatches,
                                 double* pct_identity,
                                 CScoreBuilder::EPercentIdentityType type)
{
    size_t count_aligned = 0;
    switch (type) {
    case CScoreBuilder::eGapped:
        count_aligned = align.GetAlignLength(true /* include gaps */);
        break;

    case CScoreBuilder::eUngapped:
        count_aligned = align.GetAlignLength(false /* omit gaps */);
        break;

    case CScoreBuilder::eGBDNA:
        count_aligned  = align.GetAlignLength(false /* omit gaps */);
        count_aligned += align.GetNumGapOpenings();
        break;
    }

    s_GetCountIdentityMismatch(scope, align, identities, mismatches);
    if (count_aligned) {
        *pct_identity = 100.0f * double(*identities) / count_aligned;
    } else {
        *pct_identity = 0;
    }
}


///
/// calculate the percent coverage
///
static void s_GetPercentCoverage(CScope& scope, const CSeq_align& align,
                                 const TSeqRange& range, double* pct_coverage)
{
    if (range.IsWhole() &&
        align.GetNamedScore(CSeq_align::eScore_PercentCoverage,
                            *pct_coverage)) {
        return;
    }

    size_t covered_bases = align.GetAlignLengthWithinRange(range, false /* don't include gaps */);
    size_t seq_len = 0;
    if(!range.IsWhole()){
        seq_len = range.GetLength();
    } else {
        if (align.GetSegs().IsSpliced()  &&
            align.GetSegs().GetSpliced().IsSetPoly_a()) {
            seq_len = align.GetSegs().GetSpliced().GetPoly_a();
    
            if (align.GetSegs().GetSpliced().IsSetProduct_strand()  &&
                align.GetSegs().GetSpliced().GetProduct_strand() == eNa_strand_minus) {
                if (align.GetSegs().GetSpliced().IsSetProduct_length()) {
                    seq_len = align.GetSegs().GetSpliced().GetProduct_length() - seq_len;
                } else {
                    CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(0));
                    seq_len = bsh.GetBioseqLength() - seq_len;
                }
            }
    
            if (align.GetSegs().GetSpliced().GetProduct_type() ==
                CSpliced_seg::eProduct_type_protein) {
                /// NOTE: alignment length is always reported in nucleotide
                /// coordinates
                seq_len *= 3;
            }
        }
    
        if ( !seq_len ) {
            CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(0));
            CBioseq_Handle subject_bsh = scope.GetBioseqHandle(align.GetSeq_id(1));
            seq_len = bsh.GetBioseqLength();
            if (bsh.IsAa() &&  !subject_bsh.IsAa() ) {
                /// NOTE: alignment length is always reported in nucleotide
                /// coordinates
                seq_len *= 3;
                if (align.GetSegs().IsStd()) {
                    /// odd corner case:
                    /// std-seg alignments of protein to nucleotide
                    covered_bases *= 3;
                }
            }
        }
    }

    if (covered_bases) {
        *pct_coverage = 100.0f * double(covered_bases) / double(seq_len);
    } else {
        *pct_coverage = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////


double CScoreBuilder::GetPercentIdentity(CScope& scope,
                                         const CSeq_align& align,
                                         EPercentIdentityType type)
{
    int identities = 0;
    int mismatches = 0;
    double pct_identity = 0;
    s_GetPercentIdentity(scope, align,
                         &identities, &mismatches, &pct_identity, type);
    return pct_identity;
}


double CScoreBuilder::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align, TSeqRange::GetWhole(), &pct_coverage);
    return pct_coverage;
}

double CScoreBuilder::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align,
                                         const TSeqRange& range)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align, range, &pct_coverage);
    return pct_coverage;
}

int CScoreBuilder::GetIdentityCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
    return identities;
}


int CScoreBuilder::GetMismatchCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches);
    return mismatches;
}


void CScoreBuilder::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                     int& identities, int& mismatches)
{
    identities = 0;
    mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
}


int CScoreBuilder::GetGapBaseCount(const CSeq_align& align)
{
    return align.GetTotalGapCount();
}


int CScoreBuilder::GetGapCount(const CSeq_align& align)
{
    return align.GetNumGapOpenings();
}


TSeqPos CScoreBuilder::GetAlignLength(const CSeq_align& align, bool ungapped)
{
    return align.GetAlignLength( !ungapped /* true = include gaps = !ungapped */);
}


/////////////////////////////////////////////////////////////////////////////

static const unsigned char reverse_4na[16] = {0, 8, 4, 0, 2, 0, 0, 0, 1};

int CScoreBuilder::GetBlastScore(CScope& scope,
                                 const CSeq_align& align)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CException, eUnknown,
            "CScoreBuilder::GetBlastScore(): "
            "only dense-seg alignments are supported");
    }

    if (m_ScoreBlk == 0) {
        NCBI_THROW(CException, eUnknown,
               "Blast scoring parameters have not been specified");
    }

    int computed_score = 0;
    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CAlnVec vec(ds, scope);
    CBioseq_Handle bsh1 = vec.GetBioseqHandle(0);
    CBioseq_Handle bsh2 = vec.GetBioseqHandle(1);
    CSeqVector vec1(bsh1);
    CSeqVector vec2(bsh2);
    CSeq_inst::TMol mol1 = vec1.GetSequenceType();
    CSeq_inst::TMol mol2 = vec2.GetSequenceType();

    int gap_open = m_GapOpen;
    int gap_extend = m_GapExtend;

    if (mol1 == CSeq_inst::eMol_aa && mol2 == CSeq_inst::eMol_aa) {

        Int4 **matrix = m_ScoreBlk->matrix->data;

        if (m_BlastType != eBlastp) {
            NCBI_THROW(CException, eUnknown,
                        "Protein scoring parameters required");
        }

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME the following assumes ncbistdaa format

            for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                unsigned char c1 = vec1[start1 + pos];
                unsigned char c2 = vec2[start2 + pos];
                computed_score += matrix[c1][c2];
            }
        }
    }
    else if (CSeq_inst::IsNa(mol1) && CSeq_inst::IsNa(mol2)) {

        int match = m_ScoreBlk->reward;
        int mismatch = m_ScoreBlk->penalty; // assumed negative

        if (m_BlastType != eBlastn) {
            NCBI_THROW(CException, eUnknown,
                        "Nucleotide scoring parameters required");
        }

        bool scaled_up = false;
        if (gap_open == 0 && gap_extend == 0) { // possible with megablast
            match *= 2;
            mismatch *= 2;
            gap_extend = match / 2 - mismatch;
            scaled_up = true;
        }

        int strand1 = vec.StrandSign(0);
        int strand2 = vec.StrandSign(1);

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME encoding assumed to be ncbi4na, without
            // ambiguity charaters

            if (strand1 > strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + seg_len - 1 - pos];
                    computed_score +=  (c1 == reverse_4na[c2]) ?
                                        match : mismatch;
                }
            }
            else if (strand1 < strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + seg_len - 1 - pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (reverse_4na[c1] == c2) ?
                                        match : mismatch;
                }
            }
            else {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (c1 == c2) ? match : mismatch;
                }
            }
        }

        if (scaled_up)
            computed_score /= 2;
    }
    else {
        NCBI_THROW(CException, eUnknown,
                    "pairwise alignment contains unsupported "
                    "or mismatched molecule types");
    }

    computed_score = max(0, computed_score);
    return computed_score;
}


double CScoreBuilder::GetBlastBitScore(CScope& scope,
                                       const CSeq_align& align)
{
    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return (raw_score * kbp->Lambda - kbp->logK) / NCBIMATH_LN2;
}


double CScoreBuilder::GetBlastEValue(CScope& scope,
                                     const CSeq_align& align)
{
    if (m_EffectiveSearchSpace == 0) {
        NCBI_THROW(CException, eUnknown,
               "E-value calculation requires search space to be specified");
    }

    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return BLAST_KarlinStoE_simple(raw_score, kbp, m_EffectiveSearchSpace);
}

/////////////////////////////////////////////////////////////////////////////


void CScoreBuilder::AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                             CSeq_align::EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        try {
            AddScore(scope, **iter, score);
        }
        catch (CException& e) {
            LOG_POST(Error
                << "CScoreBuilder::AddScore(): error computing score: "
                << e);
        }
    }
}

void CScoreBuilder::AddScore(CScope& scope, CSeq_align& align,
                             CSeq_align::EScoreType score)
{
    switch (score) {
    case CSeq_align::eScore_Score:
        {{
             NCBI_THROW(CException, eUnknown,
                        "CScoreBuilder::AddScore(): "
                        "generic 'score' computation is undefined");
         }}
        break;

    case CSeq_align::eScore_Blast:
        align.SetNamedScore(CSeq_align::eScore_Score,
                            GetBlastScore(scope, align));
        break;

    case CSeq_align::eScore_BitScore:
        {{
             double d = GetBlastBitScore(scope, align);
             if (d == numeric_limits<double>::infinity()  ||
                 d == numeric_limits<double>::quiet_NaN()) {
                 d = 0;
             }
             if (d > 1e35  ||  d < -1e35) {
                 d = 0;
             }
             align.SetNamedScore(CSeq_align::eScore_BitScore, d);
         }}
        break;

    case CSeq_align::eScore_EValue:
        {{
             double d = GetBlastEValue(scope, align);
             if (d == numeric_limits<double>::infinity()  ||
                 d == numeric_limits<double>::quiet_NaN()) {
                 d = 0;
             }
             if (d > 1e35  ||  d < -1e35) {
                 d = 0;
             }
             align.SetNamedScore(CSeq_align::eScore_EValue, d);
         }}
        break;

    case CSeq_align::eScore_IdentityCount:
        align.SetNamedScore(CSeq_align::eScore_IdentityCount,
                            GetIdentityCount(scope, align));
        break;

    case CSeq_align::eScore_PositiveCount:
        {{
             NCBI_THROW(CException, eUnknown,
                        "CScoreBuilder::AddScore(): "
                        "positive count algorithm not implemented");
         }}
        break;

    case CSeq_align::eScore_NegativeCount:
        {{
             NCBI_THROW(CException, eUnknown,
                        "CScoreBuilder::AddScore(): "
                        "negative count algorithm not implemented");
         }}
        break;

    case CSeq_align::eScore_MismatchCount:
        align.SetNamedScore(CSeq_align::eScore_MismatchCount,
                            GetMismatchCount(scope, align));
        break;

    case CSeq_align::eScore_AlignLength:
        align.SetNamedScore(CSeq_align::eScore_AlignLength,
                            (int)align.GetAlignLength(true /* include gaps */));
        break;

    case CSeq_align::eScore_PercentIdentity_Gapped:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eGapped);
            align.SetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped, pct_identity);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
            align.SetNamedScore(CSeq_align::eScore_MismatchCount,   mismatches);
        }}
        break;

    case CSeq_align::eScore_PercentIdentity_Ungapped:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eUngapped);
            align.SetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, pct_identity);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
            align.SetNamedScore(CSeq_align::eScore_MismatchCount,   mismatches);
        }}
        break;

    case CSeq_align::eScore_PercentIdentity_GapOpeningOnly:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eGBDNA);
            align.SetNamedScore(CSeq_align::eScore_PercentIdentity_GapOpeningOnly, pct_identity);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
            align.SetNamedScore(CSeq_align::eScore_MismatchCount,   mismatches);
        }}
        break;

    case CSeq_align::eScore_PercentCoverage:
        {{
            double pct_coverage = 0;
            s_GetPercentCoverage(scope, align, TSeqRange::GetWhole(), &pct_coverage);
            align.SetNamedScore("pct_coverage", pct_coverage);
        }}
        break;

    case CSeq_align::eScore_HighQualityPercentCoverage:
        {{
            if(align.GetSegs().Which() == CSeq_align::TSegs::e_Std)
                /// high-quality-coverage calculatino is not possbile for standard segs
                NCBI_THROW(CException, eUnknown,
                            "High-quality percent coverage not supported "
                            "for standard seg representation");

            /// If we have annotation for a high-quality region, it is in a ftable named
            /// "NCBI_GPIPE", containing a region Seq-feat named "alignable"
            TSeqRange alignable_range = TSeqRange::GetWhole();
            CBioseq_Handle query = scope.GetBioseqHandle(align.GetSeq_id(0));
            for(CFeat_CI feat_it(query, SAnnotSelector(CSeqFeatData::e_Region)); feat_it; ++feat_it)
            {
                if(feat_it->GetData().GetRegion() == "alignable" &&
                   feat_it->GetAnnot().IsNamed() &&
                   feat_it->GetAnnot().GetName() == "NCBI_GPIPE")
                {
                    alignable_range = feat_it->GetRange();
                    break;
                }
            }
            double pct_coverage = 0;
            s_GetPercentCoverage(scope, align, alignable_range, &pct_coverage);
            align.SetNamedScore(CSeq_align::eScore_HighQualityPercentCoverage, pct_coverage);
        }}
        break;

    case CSeq_align::eScore_SumEValue:
        {{
             NCBI_THROW(CException, eUnknown,
                        "CScoreBuilder::AddScore(): "
                        "sum_e not implemented");
         }}
        break;

    case CSeq_align::eScore_CompAdjMethod:
        {{
             NCBI_THROW(CException, eUnknown,
                        "CScoreBuilder::AddScore(): "
                        "comp_adj_method not implemented");
         }}
        break;
    }
}



void CScoreBuilder::AddScore(CScope& scope, CSeq_align& align,
                             EScoreType score)
{
    switch (score) {
    case eScore_Blast:
        AddScore(scope, align, CSeq_align::eScore_Blast);
        break;

    case eScore_Blast_BitScore:
        AddScore(scope, align, CSeq_align::eScore_BitScore);
        break;

    case eScore_Blast_EValue:
        AddScore(scope, align, CSeq_align::eScore_EValue);
        break;

    case eScore_MismatchCount:
        AddScore(scope, align, CSeq_align::eScore_MismatchCount);
        break;

    case eScore_IdentityCount:
        AddScore(scope, align, CSeq_align::eScore_IdentityCount);
        break;

    case eScore_PercentIdentity:
        AddScore(scope, align, CSeq_align::eScore_PercentIdentity);
        break;
    case eScore_PercentCoverage:
        AddScore(scope, align, CSeq_align::eScore_PercentCoverage);
        break;
    }
}

void CScoreBuilder::AddScore(CScope& scope,
                             list< CRef<CSeq_align> >& aligns, EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        try {
            AddScore(scope, **iter, score);
        }
        catch (CException& e) {
            LOG_POST(Error
                << "CScoreBuilder::AddScore(): error computing score: "
                << e);
        }
    }
}





END_NCBI_SCOPE
