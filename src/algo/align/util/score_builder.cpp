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
#include <algo/blast/core/blast_encoding.h>

#include <objtools/alnmgr/alnvec.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>

#include <util/sequtil/sequtil_manip.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_generators.hpp>

#include <util/checksum.hpp>
#include <sstream>


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

int CScoreBuilder::GetBlastScore(CScope& scope,
                                 const CSeq_align& align)
{
    if (align.CheckNumRows() != 2) {
    NCBI_THROW(CSeqalignException, eUnsupported,
               "CScoreBuilder::GetBlastScore(): "
               "only two-row alignments are supported");
    }
    if (align.GetSegs().IsDenseg() ) {
        return GetBlastScoreDenseg(scope, align);
    }
    if (align.GetSegs().IsStd() ) {
        return GetBlastScoreStd(scope, align);
    }
    if (align.GetSegs().IsSpliced() ) {
        return GetBlastScoreSpliced(scope, align);
    }
    NCBI_THROW(CSeqalignException, eUnsupported,
               "CScoreBuilder::GetBlastScore(): " +
               align.GetSegs().SelectionName(align.GetSegs().Which())
               +" is not supported");
    return 0;
}

static const unsigned char reverse_4na[16] = {0, 8, 4, 0, 2, 0, 0, 0, 1};

int CScoreBuilder::GetBlastScoreDenseg(CScope& scope,
                                 const CSeq_align& align)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CScoreBuilder::GetBlastScore(): "
                   "only dense-seg alignments are supported");
    }

    if (m_ScoreBlk == 0) {
        NCBI_THROW(CSeqalignException, eInvalidInputData,
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
            NCBI_THROW(CSeqalignException, eUnsupported,
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
            NCBI_THROW(CSeqalignException, eUnsupported,
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
        const CSeq_align* align_ptr = &align;

        auto_ptr<CSeq_align> swapped_align_ptr;
        if (mol2 == CSeq_inst::eMol_aa) {
            swapped_align_ptr.reset(new CSeq_align);
            swapped_align_ptr->Assign(align);
            swapped_align_ptr->SwapRows(0,1);
            align_ptr = swapped_align_ptr.get();
        }
        list<CRef<CPairwiseAln> > pairs;

        TAlnSeqIdIRef id1(new CAlnSeqId(align_ptr->GetSeq_id(0)));
        id1->SetBaseWidth(3);
        TAlnSeqIdIRef id2(new CAlnSeqId(align_ptr->GetSeq_id(1)));
        CRef<CPairwiseAln> pairwise(new CPairwiseAln(id1, id2));
        TAlnSeqIdVec ids;
        ids.push_back(id1); ids.push_back(id2);
        ConvertSeqAlignToPairwiseAln(*pairwise, *align_ptr, 0, 1, CAlnUserOptions::eBothDirections, &ids);

        pairs.push_back(pairwise);

        return GetBlastScoreProtToNucl(scope, *align_ptr, pairs);
    }

    computed_score = max(0, computed_score);
    return computed_score;
}

int CScoreBuilder::GetBlastScoreStd(CScope& scope,
                                 const CSeq_align& align)
{
    CSeq_id_Handle bsh1 = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
    CSeq_id_Handle bsh2 = CSeq_id_Handle::GetHandle(align.GetSeq_id(1));

    CSeq_inst::TMol mol1 = scope.GetBioseqHandle(bsh1).GetSequenceType();
    CSeq_inst::TMol mol2 = scope.GetBioseqHandle(bsh2).GetSequenceType();

    if (mol1 == mol2) {
        CRef<CSeq_align> new_align =
            ConvertSeq_align(align,
                 CSeq_align::TSegs::e_Denseg,
                 -1,
                 &scope);
        return GetBlastScoreDenseg(scope, *new_align);
    }

    const CSeq_align* align_ptr = &align;

    auto_ptr<CSeq_align> swapped_align_ptr;
    if (CSeq_inst::IsNa(mol1)) {
        swapped_align_ptr.reset(new CSeq_align);
        swapped_align_ptr->Assign(align);
        swapped_align_ptr->SwapRows(0,1);
        align_ptr = swapped_align_ptr.get();
    }

    list<CRef<CPairwiseAln> > pairs;
    CRef<CPairwiseAln> aln = CreatePairwiseAlnFromSeqAlign(*align_ptr);
    pairs.push_back(aln);

    return GetBlastScoreProtToNucl(scope, *align_ptr, pairs);
}

int CScoreBuilder::GetBlastScoreSpliced(CScope& scope,
                                 const CSeq_align& align)
{
        // check assumptions:
        //
        if ( align.GetSegs().GetSpliced().GetProduct_type() !=
             CSpliced_seg::eProduct_type_protein) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "CScore_TblastnScore: "
                       "valid only for protein spliced-seg alignments");
        }

        list<CRef<CPairwiseAln> > pairs;
        CSeq_align sub_align;
        sub_align.Assign(align);

        ITERATE (CSpliced_seg::TExons, it,
                 align.GetSegs().GetSpliced().GetExons()) {
            CRef<CSpliced_exon> exon = *it;
            sub_align.SetSegs().SetSpliced().SetExons().clear();
            sub_align.SetSegs().SetSpliced().SetExons().push_back(exon);

            CRef<CPairwiseAln> aln = CreatePairwiseAlnFromSeqAlign(sub_align);

            if (exon->IsSetAcceptor_before_exon() || pairs.empty()) {
                pairs.push_back(aln);
            } else {
                ITERATE(CPairwiseAln, r, *aln) {
                    pairs.back()->push_back(*r);
                }
            }
        }
        return GetBlastScoreProtToNucl(scope, align, pairs);
}

int CScoreBuilder::GetBlastScoreProtToNucl(CScope& scope,
                                           const CSeq_align& align,
                                           list<CRef<CPairwiseAln> >& pairs)
{
    CSeq_id_Handle prot_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
    CSeq_id_Handle genomic_idh =
        CSeq_id_Handle::GetHandle(align.GetSeq_id(1));

    ENa_strand strand = align.GetSeqStrand(1);
    CBioseq_Handle prot_bsh = scope.GetBioseqHandle(prot_idh);
    CBioseq_Handle genomic_bsh = scope.GetBioseqHandle(genomic_idh);
    CSeqVector prot_vec   (prot_bsh);

    int gcode = 1;
    try {
        gcode = sequence::GetOrg_ref(genomic_bsh).GetGcode();
    }
    catch (CException&) {
        // use the default genetic code
    }

    const CTrans_table& tbl = CGen_code_table::GetTransTable(gcode);

    int state = 0;
    int offs = 0;
    int score = 1;

    if (m_ScoreBlk == NULL) {
        CRef<CBlastOptionsHandle>
            options(CBlastOptionsFactory::Create(blast::eTblastn));
        x_Initialize(*options);
    }
    Int4 **matrix = m_ScoreBlk->matrix->data;

//         int num_positives = 0;
//         int num_negatives = 0;
//         int num_match = 0;
//         int num_mismatch = 0;
    ITERATE (list<CRef<CPairwiseAln> >, it, pairs) {
        CRef<CPairwiseAln> aln = *it;

        int this_pair_score = -1;
        list<int> gaps;
        CPairwiseAln::const_iterator prev = aln->end();
        ITERATE (CPairwiseAln, range_it, *aln) {

            // handle gaps
            if (prev != aln->end()) {
                int q_gap = range_it->GetFirstFrom() - prev->GetFirstTo() - 1;
                int s_gap =
                    (strand == eNa_strand_minus ?
                     prev->GetSecondFrom() - range_it->GetSecondTo() - 1 :
                     range_it->GetSecondFrom() - prev->GetSecondTo() - 1);

                // check if this range is in the list of known introns

                int gap = abs(q_gap - s_gap);
                gaps.push_back(gap);
            }
            prev = range_it;

            CRange<int> q_range = range_it->GetFirstRange();
            CRange<int> s_range = range_it->GetSecondRange();

            int s_start = s_range.GetFrom();
            int s_end   = s_range.GetTo();
            int q_pos   = q_range.GetFrom();

            int new_offs = q_pos % 3;
            for ( ;  offs != new_offs;  offs = (offs + 1) % 3) {
                state = tbl.NextCodonState(state, 'N');
            }

            // first range is in nucleotide coordinates...

            CRef<CSeq_loc> loc =
                genomic_bsh.GetRangeSeq_loc(s_start, s_end, strand);
            CSeqVector genomic_vec(*loc, scope, CBioseq_Handle::eCoding_Iupac);
            CSeqVector_CI vec_it(genomic_vec);

            for ( ;  s_start <= s_end;  ++s_start, ++q_pos, ++vec_it) {
                state = tbl.NextCodonState(state, *vec_it);
                if (offs % 3 == 2) {
                    Uint1 prot = prot_vec[(int)(q_pos / 3)];
                    Uint1 xlate = AMINOACID_TO_NCBISTDAA[(unsigned)tbl.GetCodonResidue(state)];

                    int this_score = matrix[prot][xlate];

//                         num_match += (prot == xlate);
//                         num_mismatch += (prot != xlate);
//                         num_positives += (this_score > 0);
//                         num_negatives += (this_score <= 0);
                    this_pair_score += this_score;
                }

                offs = (offs + 1) % 3;
            }
        }

        // adjust score for gaps
        // HACK: this isn't exactly correct; it overestimates scores because we
        // don't have full accounting of composition based statistics, etc.
        // It's close enough, though
        gaps.sort();

        ITERATE(list<int>, gap_bases, gaps) {
            int new_score = this_pair_score - m_GapOpen - (*gap_bases/3) * m_GapExtend;
            // do not score huge gaps - they are between hits gaps
            if (new_score > 0 ) {
                this_pair_score = new_score;
            } else {
                this_pair_score -= 1;
            }
        }

        score += this_pair_score;
    }

    return score;
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
        NCBI_THROW(CSeqalignException, eInvalidInputData,
                   "E-value calculation requires search space "
                   "to be specified");
    }

    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return BLAST_KarlinStoE_simple(raw_score, kbp, m_EffectiveSearchSpace);
}


double CScoreBuilder::ComputeScore(CScope& scope, const CSeq_align& align,
                                   const CRangeCollection<TSeqPos> &ranges,
                                   CSeq_align::EScoreType score)
{
    // Override certain score computations in this subclass.
    switch (score) {
    case CSeq_align::eScore_Blast:
        return GetBlastScore(scope, align);

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
             return d;
         }}

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
             return d;
         }}

    case CSeq_align::eScore_SumEValue:
    case CSeq_align::eScore_CompAdjMethod:
        // FIXME TODO: Not implemented.

    // Fallback to superclass implementation of score computation.
    default:
        return CScoreBuilderBase::ComputeScore(scope, align, ranges, score);
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
        CSeq_align& align = **iter;
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
}

static inline void s_RecordMatch(size_t match, string &BTOP, string &flipped_BTOP)
{
    if (match) {
        string match_str = NStr::NumericToString(match);
        BTOP += match_str;
        flipped_BTOP.insert(0, match_str);
    }
}

static pair<string,string> s_ComputeTraceback(CScope& scope,
                                              const CSeq_align& align)
{
    if (!align.GetSegs().IsDenseg() || align.CheckNumRows() != 2) {
        NCBI_THROW(CException, eUnknown,
                   "Traceback strings can only be calculated for pairwise "
                   "Dense-seg alignments");
    }

    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CAlnVec vec(ds, scope);
    string BTOP, flipped_BTOP;
    for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
        string query, subject;
        vec.GetSegSeqString(query, 0, i);
        vec.GetSegSeqString(subject, 1, i);
        if (query.empty()) {
            for (size_t idx = 0; idx < subject.size(); ++idx) {
                string complement;
                CSeqManip::Complement(subject, CSeqUtil::e_Iupacna,
                                      idx, 1, complement);
                BTOP += '-';
                BTOP += subject[idx];
                flipped_BTOP.insert(flipped_BTOP.begin(), complement[0]);
                flipped_BTOP.insert(flipped_BTOP.begin(), '-');
            }
        } else if (subject.empty()) {
            for (size_t idx = 0; idx < query.size(); ++idx) {
                string complement;
                CSeqManip::Complement(query, CSeqUtil::e_Iupacna,
                                      idx, 1, complement);
                BTOP += query[idx];
                BTOP += '-';
                flipped_BTOP.insert(flipped_BTOP.begin(), '-');
                flipped_BTOP.insert(flipped_BTOP.begin(), complement[0]);
            }
        } else {
            size_t match = 0;
            for (size_t idx = 0; idx < query.size(); ++idx) {
                NCBI_ASSERT(query.size() == subject.size(),
                            "inconsistent aligned segment length");
                if (query[idx] == subject[idx]) {
                    ++match;
                } else {
                    s_RecordMatch(match, BTOP, flipped_BTOP);
                    match = 0;
                    BTOP += query[idx];
                    BTOP += subject[idx];
                    string query_complement;
                    CSeqManip::Complement(query, CSeqUtil::e_Iupacna,
                                          idx, 1, query_complement);
                    string subject_complement;
                    CSeqManip::Complement(subject, CSeqUtil::e_Iupacna,
                                          idx, 1, subject_complement);
                    flipped_BTOP.insert(flipped_BTOP.begin(),
                                        subject_complement[0]);
                    flipped_BTOP.insert(flipped_BTOP.begin(),
                                        query_complement[0]);
                }
            }
            s_RecordMatch(match, BTOP, flipped_BTOP);
        }
    }
    return pair<string,string>(
        BTOP, align.GetSeqStrand(0) == align.GetSeqStrand(1)
                  ? BTOP : flipped_BTOP);
}

void CScoreBuilder::AddTracebacks(CScope& scope, CSeq_align& align)
{
    CRef<CUser_object> tracebacks;
    ITERATE (CSeq_align::TExt, ext_it, align.SetExt()) {
        if ((*ext_it)->GetType().IsStr() &&
            (*ext_it)->GetType().GetStr() == "Tracebacks")
        {
            /// Tracebacks object already exists
            tracebacks = *ext_it;
            break;
        }
    }

    if (!tracebacks) {
        tracebacks.Reset(new CUser_object);
        tracebacks->SetType().SetStr("Tracebacks");
        align.SetExt().push_back(tracebacks);
    } else if (tracebacks->HasField("Query") && tracebacks->HasField("Subject"))
    {
        return;
    }
    pair<string,string> traceback_strings = s_ComputeTraceback(scope, align);
    tracebacks->SetField("Query").SetData().SetStr(traceback_strings.first);
    tracebacks->SetField("Subject").SetData().SetStr(traceback_strings.second);
}

void CScoreBuilder::AddTracebacks(CScope& scope,
                                  list< CRef<CSeq_align> >& aligns)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        AddTracebacks(scope, **iter);
    }
}

string CScoreBuilder::GetTraceback(const CSeq_align& align,
                                   CSeq_align::TDim row)
{
    if (align.IsSetExt()) {
        ITERATE (CSeq_align::TExt, ext_it, align.GetExt()) {
            if ((*ext_it)->GetType().IsStr() &&
                (*ext_it)->GetType().GetStr() == "Tracebacks")
            {
                string field = row == 0 ? "Query" : "Subject";
                if ((*ext_it)->HasField(field)) {
                    return (*ext_it)->GetField(field).GetData().GetStr();
                }
                break;
            }
        }
    }
    return "";
}

string CScoreBuilder::GetTraceback(CScope& scope, const CSeq_align& align,
                                   CSeq_align::TDim row)
{
    string stored_traceback = GetTraceback(align, row);
    if (!stored_traceback.empty()) {
        return stored_traceback;
    }

    /// Tracebacks user object not found; need to calculate on the fly
    pair<string,string> traceback_strings = s_ComputeTraceback(scope, align);
    return row == 0 ? traceback_strings.first : traceback_strings.second;
}



// clear out all of the parts of a seq-align
//  that might mess with the CRC but don't effect 
//  the important parts of the alignment
void s_CleanSeqAlign(CSeq_align& align)
{

    // These seg specific parts are probably incomplete
    // but denseg, disc, and spliced ought to handle 99.9%
    if (align.CanGetSegs()) { 
        if (align.GetSegs().IsDenseg()) {
            align.SetSegs().SetDenseg().SetScores().clear();
            // ...
        }
        else if (align.GetSegs().IsDisc()) {
            NON_CONST_ITERATE(CSeq_align_set::Tdata, align_iter, 
                align.SetSegs().SetDisc().Set()) {
                s_CleanSeqAlign(**align_iter);
            }
        }
        else if (align.GetSegs().IsSpliced()) {
            NON_CONST_ITERATE(CSpliced_seg::TExons, exon_iter, 
                align.SetSegs().SetSpliced().SetExons()) {
                (*exon_iter)->SetScores().Set().clear();
            }
        }
        else if (align.GetSegs().IsSparse()) {
            align.SetSegs().SetSparse().SetRow_scores().clear();
            // ...
        }  
        else if (align.GetSegs().IsStd()) {
            NON_CONST_ITERATE(CSeq_align::C_Segs::TStd, std_iter, 
                align.SetSegs().SetStd()) {
                (*std_iter)->SetScores().clear();
            }
        }
    }


    align.SetScore().clear();
    align.SetId().clear();
    align.SetBounds().clear();
    align.SetExt().clear();
}

int CScoreBuilder::ComputeTieBreaker(const CSeq_align& align) 
{
    CChecksum checksum;
    
    CSeq_align clean;
    clean.Assign(align);
    s_CleanSeqAlign(clean);

    stringstream cleanStr;
    cleanStr << MSerial_AsnText << clean;

    checksum.AddLine(cleanStr.str());

    int result = 0;
    Uint4* result_ptr = (Uint4*)&result;
    *result_ptr = checksum.GetChecksum(); 

    return result;
}

void CScoreBuilder::AddTieBreaker(CSeq_align& align) 
{
    int tiebreaker = ComputeTieBreaker(align);
    align.SetNamedScore("tiebreaker", tiebreaker);
}


END_NCBI_SCOPE
