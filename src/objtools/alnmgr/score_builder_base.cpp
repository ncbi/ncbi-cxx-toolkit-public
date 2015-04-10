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
#include <objtools/alnmgr/score_builder_base.hpp>
#include <objtools/alnmgr/alntext.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>

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

#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Default constructor
CScoreBuilderBase::CScoreBuilderBase()
: m_ErrorMode(eError_Throw)
, m_SubstMatrixName("BLOSUM62")
{
}

/// Destructor
CScoreBuilderBase::~CScoreBuilderBase()
{
}

/// Get length of intersection between a range and a range collection
static inline
TSeqPos s_IntersectionLength(const CRangeCollection<TSeqPos> &ranges,
                             const TSeqRange &range)
{
    TSeqPos length = 0;
    ITERATE (CRangeCollection<TSeqPos>, it, ranges) {
        length += it->IntersectionWith(range).GetLength();
    }
    return length;
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
                                            const CRangeCollection<TSeqPos> &ranges,
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
        const CSeq_id &failed_id = align.GetSeq_id(genomic_bsh ? 0 : 1);
        NCBI_THROW(CSeqalignException, eInvalidSeqId,
                   "Can't get sequence data for " + failed_id.AsFastaString() +
                   " in order to count identities/mismatches");
    }

    CSeqVector prod(prod_bsh, CBioseq_Handle::eCoding_Iupac);

    switch (align.GetSegs().GetSpliced().GetProduct_type()) {
    case CSpliced_seg::eProduct_type_transcript:
        {{
             CSeqVector gen(genomic_bsh, CBioseq_Handle::eCoding_Iupac);
             ITERATE (CPairwiseAln, it, *pairwise) {
                 const CPairwiseAln::TAlnRng& range = *it;
                 TSeqRange r1(range.GetFirstFrom(), range.GetFirstTo());
                 TSeqRange r2(range.GetSecondFrom(), range.GetSecondTo());
                 string prod_data;
                 prod.GetSeqData(r1.GetFrom(), r1.GetTo() + 1, prod_data);
                 string gen_data;
                 gen.GetSeqData(r2.GetFrom(), r2.GetTo() + 1, gen_data);
                 if (range.IsReversed()) {
                     CSeqManip::ReverseComplement(gen_data,
                                                  CSeqUtil::e_Iupacna,
                                                  0, gen_data.size());
                 }

                 CRangeCollection<TSeqPos> seg_ranges = ranges;
                 seg_ranges.IntersectWith(r1);
                 ITERATE (CRangeCollection<TSeqPos>, range_it, seg_ranges) {
                     TSeqPos start_offset = range_it->GetFrom() - r1.GetFrom(),
                             end_offset = range_it->GetToOpen() - r1.GetFrom();
                     string::const_iterator pit = prod_data.begin()
                                                + start_offset;
                     string::const_iterator pit_end = prod_data.begin()
                                                + end_offset;
                     string::const_iterator git = gen_data.begin()
                                                + start_offset;
                     string::const_iterator git_end = gen_data.begin()
                                                + end_offset;
    
                     for ( ;  pit != pit_end  &&  git != git_end;  ++pit, ++git)
                     {
                         bool match = (*pit == *git);
                         *identities +=  match;
                         *mismatches += !match;
                     }
                 }
             }
         }}
        break;

    case CSpliced_seg::eProduct_type_protein:
        {{
             int gcode = 1;
             try {
                 const COrg_ref& org_ref = sequence::GetOrg_ref(genomic_bsh);
                 gcode = org_ref.GetOrgname().GetGcode();
             }
             catch (CException&) {
             }
             const CTrans_table& tbl = CGen_code_table::GetTransTable(gcode);

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
                         int state = tbl.SetCodonState(codon[0], codon[1], codon[2]);
                         char residue = (prod_pos == 2
                                         ? tbl.GetStartResidue(state)
                                         : tbl.GetCodonResidue(state));

                         /// NOTE:
                         /// we increment identities/mismatches by 3 here,
                         /// counting identities in nucleotide space!!
                         if (residue == prod[prod_pos / 3]  &&
                             residue != 'X'  &&  residue != '-') {
                             *identities += 3;
                         } else {
                             *mismatches += 3;
                         }
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
                                       int* identities, int* mismatches,
                                       const CRangeCollection<TSeqPos> &ranges =
                                            CRangeCollection<TSeqPos>(TSeqRange::GetWhole()))
{
    _ASSERT(identities  &&  mismatches);
    if (ranges.empty()) {
        return;
    }

    {{
         ///
         /// shortcut: if 'num_ident' is present, we trust it
         ///
         int num_ident = 0;
         if (ranges.begin()->IsWhole() &&
             align.GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident))
         {
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
            vector<string> data;
            CAlnVec vec(ds, scope);
            data.resize(vec.GetNumRows());
            for (int seg = 0;  seg < vec.GetNumSegs();  ++seg) {
                bool has_gap = false;
                for (int i = 0;  !has_gap && i < vec.GetNumRows();  ++i) {
                    if (vec.GetStart(i, seg) == -1) {
                        has_gap = true;
                    }
                }
                if (has_gap) {
                    /// we compute ungapped identities
                    /// gap on at least one row, so we skip this segment
                    continue;
                }

                TSeqPos seg_start = vec.GetStart(0, seg),
                        seg_stop = vec.GetStop(0, seg);
                CRangeCollection<TSeqPos> seg_ranges = ranges;
                seg_ranges.IntersectWith(TSeqRange(seg_start, seg_stop));
                for (int i = 0;  i < vec.GetNumRows();  ++i) {
                    TSeqPos offset = vec.GetStart(i, seg) - seg_start;
                    ITERATE (CRangeCollection<TSeqPos>, range_it, seg_ranges) {
                        string seq_string; 
                        vec.GetSeqString(seq_string, i,
                                         range_it->GetFrom()+offset,
                                         range_it->GetTo()+offset);
                        data[i] += seq_string;
                    }
                }
            }
            s_GetNucIdentityMismatch(data, identities, mismatches);
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter,
                     align.GetSegs().GetDisc().Get()) {
                s_GetCountIdentityMismatch(scope, **iter,
                                           identities, mismatches, ranges);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        NCBI_THROW(CSeqalignException, eNotImplemented,
                   "identity + mismatch function not implemented for std-seg");
        break;

    case CSeq_align::TSegs::e_Spliced:
        {{
             int aln_identities = 0;
             int aln_mismatches = 0;
             bool has_non_standard = false;
             ITERATE (CSpliced_seg::TExons, iter,
                      align.GetSegs().GetSpliced().GetExons()) {
                 const CSpliced_exon& exon = **iter;
                 TSeqRange product_span;
                 product_span.Set(exon.GetProduct_start().AsSeqPos(),
                                  exon.GetProduct_end().AsSeqPos());
                 if (exon.IsSetParts()) {
                     TSeqPos part_start = product_span.GetFrom();
                     ITERATE (CSpliced_exon::TParts, it, exon.GetParts()) {
                         const CSpliced_exon_chunk& chunk = **it;
                         int part_len = 0;
                         switch (chunk.Which()) {
                         case CSpliced_exon_chunk::e_Match:
                             part_len = chunk.GetMatch();
                             aln_identities += s_IntersectionLength(ranges,
                                                TSeqRange(part_start,
                                                          part_start+part_len-1));
                             break;
     
                         case CSpliced_exon_chunk::e_Mismatch:
                             part_len = chunk.GetMismatch();
                             aln_mismatches += s_IntersectionLength(ranges,
                                                TSeqRange(part_start,
                                                          part_start+part_len-1));
                             break;
     
                         case CSpliced_exon_chunk::e_Diag:
                             part_len = chunk.GetDiag();
                             if (s_IntersectionLength(ranges,
                                     TSeqRange(part_start,
                                               part_start+part_len-1)))
                             {
                                 has_non_standard = true;
                             }
                             break;
     
                         case CSpliced_exon_chunk::e_Product_ins:
                             part_len = chunk.GetProduct_ins();
                             break;
     
                         default:
                             break;
                         }
                         part_start += part_len;
                     }
                 } else {
                     aln_identities += s_IntersectionLength(ranges, product_span);
                 }
             }
             if ( !has_non_standard ) {
                 *identities += aln_identities;
                 *mismatches += aln_mismatches;
                 break;
             }

             /// we must compute match and mismatch based on first
             /// prinicples. Sometimes loader will fail in getting
             /// all components of the genomic sequence; in that case
             /// throw an exception, but make it somewhat more informative
             try {
                 s_GetSplicedSegIdentityMismatch(scope, align, ranges,
                                                 identities, mismatches);
             } catch (CLoaderException &e) {
                 NCBI_RETHROW_SAME(e,
                                   "Can't calculate identities/mismatches for "
                                   "alignment with genomic sequence " +
                                   align.GetSeq_id(1).AsFastaString() +
                                   "; Loader can't load all required "
                                   "components of sequence");
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
                                 CScoreBuilderBase::EPercentIdentityType type,
                                 const CRangeCollection<TSeqPos> &ranges =
                                      CRangeCollection<TSeqPos>(TSeqRange::GetWhole()))
{
    size_t count_aligned = 0;
    switch (type) {
    case CScoreBuilderBase::eGapped:
        count_aligned = align.GetAlignLengthWithinRanges(ranges, true /* include gaps */);
        break;

    case CScoreBuilderBase::eUngapped:
        count_aligned = align.GetAlignLengthWithinRanges(ranges, false /* omit gaps */);
        break;

    case CScoreBuilderBase::eGBDNA:
        count_aligned  = align.GetAlignLengthWithinRanges(ranges, false /* omit gaps */);
        count_aligned += align.GetNumGapOpeningsWithinRanges(ranges);
        break;
    }

    s_GetCountIdentityMismatch(scope, align, identities, mismatches, ranges);
    if (count_aligned) {
        *pct_identity = 100.0f * double(*identities) / count_aligned;
    } else {
        *pct_identity = 0;
    }
}


///
/// calculate the percent coverage
///
static bool s_SequenceIsProtein(CScope& scope,
                                const CSeq_id& id)
{
    CSeq_inst::TMol mol = scope.GetSequenceType(id);
    if (mol == CSeq_inst::eMol_not_set) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(id);
        if ( !bsh ) {
            NCBI_THROW(CException, eUnknown,
                       "failed to retrieve sequence: " + id.AsFastaString());
        }
        return bsh.IsAa();
    }

    return (mol == CSeq_inst::eMol_aa);
}


static bool s_IsProteinToGenomic(CScope& scope,
                                 const CSeq_align& align)
{
    if (align.GetSegs().IsSpliced()) {
        return align.GetSegs().GetSpliced()
            .GetProduct_type() == CSpliced_seg::eProduct_type_protein;
    }

    if (align.GetSegs().IsDenseg()) {
        const CDense_seg& seg = align.GetSegs().GetDenseg();
        if (seg.IsSetWidths()) {
            // FIXME: I can't remember what the rule for widths is
            // 
        }
        else {
            // we must be protein-to-protein or nuc-to-nuc
            return false;
        }
    }

    // our short-cuts are exhausted
    // fall back to a check of sequence type
    const CSeq_id& id0 = align.GetSeq_id(0);
    if ( !s_SequenceIsProtein(scope, id0) ) {
        return false;
    }
    const CSeq_id& id1 = align.GetSeq_id(1);
    return s_SequenceIsProtein(scope, id1);
}


static void s_GetPercentCoverage(CScope& scope, const CSeq_align& align,
                                 const CRangeCollection<TSeqPos>& ranges,
                                 double* pct_coverage)
{
    if (!ranges.empty() && ranges.begin()->IsWhole() &&
        align.GetNamedScore(CSeq_align::eScore_PercentCoverage,
                            *pct_coverage)) {
        return;
    }

    size_t covered_bases = align.GetAlignLengthWithinRanges
                               (ranges, false /* don't include gaps */);
    size_t seq_len = 0;
    if(ranges.empty() || !ranges.begin()->IsWhole()){
        seq_len = ranges.GetCoveredLength();
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
            seq_len = scope.GetSequenceLength(align.GetSeq_id(0));

            //
            // determine if the alignment is protein-to-genomic
            //
            bool is_protein_to_genomic = s_IsProteinToGenomic(scope, align);
            if (is_protein_to_genomic) {
                /// alignment is protein-to-genomic alignment
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
void CScoreBuilderBase::x_GetMatrixCounts(CScope& scope,
                       const CSeq_align& align,
                       int* positives, int* negatives)
{
    if (!align.GetSegs().IsSpliced() ||
         align.GetSegs().GetSpliced().GetProduct_type() !=
                CSpliced_seg::eProduct_type_protein)
    {
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "num_positives and num_negatives scores only defined "
                   "for protein alignment");
    }
    CProteinAlignText pro_text(scope, align, m_SubstMatrixName);
    const string& prot = pro_text.GetProtein();
    const string& dna = pro_text.GetDNA();
    const string& match = pro_text.GetMatch();
    for(string::size_type i=0;i<match.size(); ++i) {
        if( isalpha(prot[i]) && (dna[i] != '-')) {
            int increment = isupper(prot[i]) ? 3 : 1;
            switch(match[i]) {
            case '|':
            case '+':
                *positives += increment;
                break;
            case 'X': /// skip introns and bad parts
                break;
            default://mismatch
                *negatives += increment;
                break;
            }
        }
    }
}




void CScoreBuilderBase::SetSubstMatrix(const string &name)
{
    m_SubstMatrixName = name;
}

double CScoreBuilderBase::GetPercentIdentity(CScope& scope,
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


double CScoreBuilderBase::GetPercentIdentity(CScope& scope,
                                         const CSeq_align& align,
                                         const TSeqRange &range,
                                         EPercentIdentityType type)
{
    int identities = 0;
    int mismatches = 0;
    double pct_identity = 0;
    s_GetPercentIdentity(scope, align,
                         &identities, &mismatches, &pct_identity, type,
                          CRangeCollection<TSeqPos>(range));
    return pct_identity;
}


double CScoreBuilderBase::GetPercentIdentity(CScope& scope,
                                         const CSeq_align& align,
                                         const CRangeCollection<TSeqPos> &ranges,
                                         EPercentIdentityType type)
{
    int identities = 0;
    int mismatches = 0;
    double pct_identity = 0;
    s_GetPercentIdentity(scope, align,
                         &identities, &mismatches, &pct_identity, type, ranges);
    return pct_identity;
}


double CScoreBuilderBase::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align,
                         CRangeCollection<TSeqPos>(TSeqRange::GetWhole()),
                         &pct_coverage);
    return pct_coverage;
}

double CScoreBuilderBase::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align,
                                         const TSeqRange& range)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align, CRangeCollection<TSeqPos>(range), &pct_coverage);
    return pct_coverage;
}

double CScoreBuilderBase::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align,
                                         const CRangeCollection<TSeqPos>& ranges)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align, ranges, &pct_coverage);
    return pct_coverage;
}

int CScoreBuilderBase::GetIdentityCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
    return identities;
}


int CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches);
    return mismatches;
}


void CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                     int& identities, int& mismatches)
{
    identities = 0;
    mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
}


int CScoreBuilderBase::GetIdentityCount(CScope& scope, const CSeq_align& align,
                                        const TSeqRange& range)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches,
                               CRangeCollection<TSeqPos>(range));
    return identities;
}


int CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                        const TSeqRange& range)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches,
                               CRangeCollection<TSeqPos>(range));
    return mismatches;
}


void CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                        const TSeqRange& range,
                                     int& identities, int& mismatches)
{
    identities = 0;
    mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches,
                               CRangeCollection<TSeqPos>(range));
}


int CScoreBuilderBase::GetIdentityCount(CScope& scope, const CSeq_align& align,
                                        const CRangeCollection<TSeqPos> &ranges)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches, ranges);
    return identities;
}


int CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                        const CRangeCollection<TSeqPos> &ranges)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches, ranges);
    return mismatches;
}


void CScoreBuilderBase::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                        const CRangeCollection<TSeqPos> &ranges,
                                     int& identities, int& mismatches)
{
    identities = 0;
    mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches, ranges);
}


int CScoreBuilderBase::GetPositiveCount(CScope& scope, const CSeq_align& align)
{
    int positives = 0;
    int negatives = 0;
    x_GetMatrixCounts(scope, align, &positives, &negatives);
    return positives;
}


int CScoreBuilderBase::GetNegativeCount(CScope& scope, const CSeq_align& align)
{
    int positives = 0;
    int negatives = 0;
    x_GetMatrixCounts(scope, align, &positives, &negatives);
    return negatives;
}


void CScoreBuilderBase::GetMatrixCounts(CScope& scope, const CSeq_align& align,
                                     int& positives, int& negatives)
{
    positives = 0;
    negatives = 0;
    x_GetMatrixCounts(scope, align, &positives, &negatives);
}


int CScoreBuilderBase::GetGapBaseCount(const CSeq_align& align)
{
    return align.GetTotalGapCount();
}


int CScoreBuilderBase::GetGapCount(const CSeq_align& align)
{
    return align.GetNumGapOpenings();
}


TSeqPos CScoreBuilderBase::GetAlignLength(const CSeq_align& align, bool ungapped)
{
    return align.GetAlignLength( !ungapped /* true = include gaps = !ungapped */);
}


int CScoreBuilderBase::GetGapBaseCount(const CSeq_align& align,
                                       const TSeqRange &range)
{
    return align.GetTotalGapCountWithinRange(range);
}


int CScoreBuilderBase::GetGapCount(const CSeq_align& align,
                                   const TSeqRange &range)
{
    return align.GetNumGapOpeningsWithinRange(range);
}


TSeqPos CScoreBuilderBase::GetAlignLength(const CSeq_align& align,
                                          const TSeqRange &range,
                                          bool ungapped)
{
    return align.GetAlignLengthWithinRange(range, !ungapped
          /* true = include gaps = !ungapped */);
}


int CScoreBuilderBase::GetGapBaseCount(const CSeq_align& align,
                                       const CRangeCollection<TSeqPos> &ranges)
{
    return align.GetTotalGapCountWithinRanges(ranges);
}


int CScoreBuilderBase::GetGapCount(const CSeq_align& align,
                                   const CRangeCollection<TSeqPos> &ranges)
{
    return align.GetNumGapOpeningsWithinRanges(ranges);
}


TSeqPos CScoreBuilderBase::GetAlignLength(const CSeq_align& align,
                                   const CRangeCollection<TSeqPos> &ranges,
                                   bool ungapped)
{
    return align.GetAlignLengthWithinRanges(ranges, !ungapped
          /* true = include gaps = !ungapped */);
}


/////////////////////////////////////////////////////////////////////////////

void CScoreBuilderBase::AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                             CSeq_align::EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        AddScore(scope, **iter, score);
    }
}

void CScoreBuilderBase::AddScore(CScope& scope, CSeq_align& align,
                                 CSeq_align::EScoreType score)
{
    try {
        switch (score) {
        /// Special cases for the three precent-identity scores, to add
        /// the num_ident and num_mismatch scores as well
        case CSeq_align::eScore_PercentIdentity_Gapped:
        case CSeq_align::eScore_PercentIdentity_Ungapped:
        case CSeq_align::eScore_PercentIdentity_GapOpeningOnly:
            {{
                int identities      = 0;
                int mismatches      = 0;
                double pct_identity = 0;
                s_GetPercentIdentity(scope, align, &identities, &mismatches,
                    &pct_identity,
                    static_cast<EPercentIdentityType>(
                        score - CSeq_align::eScore_PercentIdentity_Gapped));
                align.SetNamedScore(score, pct_identity);
                align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
                align.SetNamedScore(CSeq_align::eScore_MismatchCount,   mismatches);
            }}
            break;

        default:
            {{
                double score_value = ComputeScore(scope, align, score);
                if (CSeq_align::IsIntegerScore(score)) {
                    align.SetNamedScore(score, (int)score_value);
                } else {
                    if (score_value == numeric_limits<double>::infinity()) {
                        score_value = numeric_limits<double>::max() / 10.0;
                    }
                    align.SetNamedScore(score, score_value);
                }
            }}
        }
    } catch (CSeqalignException& e) {
        // Unimplemented (code missing) or unsupported (score cannot be defined)
        // is handled according to the error handling mode. All other
        // errors always throw.
        switch (e.GetErrCode()) {
        case CSeqalignException::eUnsupported:
        case CSeqalignException::eNotImplemented:
            break;
        default:
            throw;
        }

        switch (GetErrorMode()) {
        case eError_Throw:
            throw;
        case eError_Report:
            LOG_POST(Error
                << "CScoreBuilderBase::AddScore(): error computing score: "
                << e);
        default:
            break;
        }
    }
}

string GetDonor(const objects::CSpliced_exon& exon) {
    if( exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases() ) {
        return exon.GetDonor_after_exon().GetBases();
    }
    return string();
}    

string GetAcceptor(const objects::CSpliced_exon& exon) {
    if( exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases()  ) {
        return exon.GetAcceptor_before_exon().GetBases();
    }
    return string();
}

//returns true for GT/AG, GC/AG AND AT/AC        
bool IsConsSplice(const string& donor, const string acc) {
    if(donor.length()<2 || acc.length()<2) return false;
    if(toupper(Uchar(acc.c_str()[0])) != 'A') return false;
    switch(toupper(Uchar(acc.c_str()[1]))) {
    case 'C':
        if( toupper(Uchar(donor.c_str()[0])) == 'A' && toupper(Uchar(donor.c_str()[1])) == 'T' ) return true;
        else return false;
        break;
    case 'G':
        if( toupper(Uchar(donor.c_str()[0])) == 'G' ) {
            char don2 = toupper(Uchar(donor.c_str()[1]));
            if(don2 == 'T' || don2 == 'C') return true;
        }
        return false;
        break;
    default:
        return false;
        break;
    }
    return false;
}
           
    
void CScoreBuilderBase::AddSplignScores(const CSeq_align& align,
                                        CSeq_align::TScore &scores)
{
    typedef CSeq_align::TSegs::TSpliced TSpliced;
    const TSpliced & spliced (align.GetSegs().GetSpliced());
    if(spliced.GetProduct_type() != CSpliced_seg::eProduct_type_transcript) {
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CScoreBuilderBase::AddSplignScores(): Unsupported product type");
    }

    const bool qstrand (spliced.GetProduct_strand() != eNa_strand_minus);

    typedef TSpliced::TExons TExons;
    const TExons & exons (spliced.GetExons());

    size_t matches (0),
        aligned_query_bases (0),  // matches, mismatches and indels
        aln_length_exons (0),
        aln_length_gaps (0),
        splices_total (0),        // twice the number of introns
        splices_consensus (0);

    const TSeqPos  qlen (spliced.GetProduct_length());
    const TSeqPos polya (spliced.CanGetPoly_a()?
                         spliced.GetPoly_a(): (qstrand? qlen: TSeqPos(-1)));
    const TSeqPos prod_length_no_polya (qstrand? polya: qlen - 1 - polya);
        
    typedef CSpliced_exon TExon;
    TSeqPos qprev (qstrand? TSeqPos(-1): qlen);
    string donor;
    ITERATE(TExons, ii2, exons) {

        const TExon & exon (**ii2);
        const TSeqPos qmin (exon.GetProduct_start().GetNucpos()),
            qmax (exon.GetProduct_end().GetNucpos());

        const TSeqPos qgap (qstrand? qmin - qprev - 1: qprev - qmax - 1);

        if(qgap > 0) {
            aln_length_gaps += qgap;
            donor.clear();
        }
        else if (ii2 != exons.begin()) {
            splices_total += 2;
            if(IsConsSplice(donor, GetAcceptor(exon))) { splices_consensus += 2; }
        }

        typedef TExon::TParts TParts;
        const TParts & parts (exon.GetParts());
        string errmsg;
        ITERATE(TParts, ii3, parts) {
            const CSpliced_exon_chunk & part (**ii3);
            const CSpliced_exon_chunk::E_Choice choice (part.Which());
            TSeqPos len (0);
            switch(choice) {
            case CSpliced_exon_chunk::e_Match:
                len = part.GetMatch();
                matches += len;
                aligned_query_bases += len;
                break;
            case CSpliced_exon_chunk::e_Mismatch:
                len = part.GetMismatch();
                aligned_query_bases += len;
                break;
            case CSpliced_exon_chunk::e_Product_ins:
                len = part.GetProduct_ins();
                aligned_query_bases += len;
                break;
            case CSpliced_exon_chunk::e_Genomic_ins:
                len = part.GetGenomic_ins();
                break;
            default:
                errmsg = "Unexpected spliced exon chunk part: "
                    + part.SelectionName(choice);
                NCBI_THROW(CSeqalignException, eUnsupported, errmsg);
            }
            aln_length_exons += len;
        }

        donor = GetDonor(exon);
        qprev = qstrand? qmax: qmin;
    } // TExons

    const TSeqPos qgap (qstrand? polya - qprev - 1: qprev - polya - 1);
    aln_length_gaps += qgap;

    for (CSeq_align::TScore::iterator it = scores.begin(); it != scores.end(); )
    {
        CSeq_align::EScoreType score_type = CSeq_align::eScore_Score;
        if ((*it)->GetId().IsStr()) {
            CSeq_align::TScoreNameMap::const_iterator score = 
                    CSeq_align::ScoreNameMap()
                             . find((*it)->GetId().GetStr());
            if (score != CSeq_align::ScoreNameMap().end()) {
                score_type = score->second;
            }
        }
        if (score_type >= CSeq_align::eScore_Matches &&
            score_type <= CSeq_align::eScore_ExonIdentity)
        {
            it = scores.erase(it);
        } else {
            ++it;
        }
    }

        {
            CRef<CScore> score_matches (new CScore());
            score_matches->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_Matches));
            score_matches->SetValue().SetInt(matches);
            scores.push_back(score_matches);
        }
        {
            CRef<CScore> score_overall_identity (new CScore());
            score_overall_identity->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_OverallIdentity));
            score_overall_identity->SetValue().
                SetReal(double(matches)/(aln_length_exons + aln_length_gaps));
            scores.push_back(score_overall_identity);
        }
        {
            CRef<CScore> score_splices (new CScore());
            score_splices->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_Splices));
            score_splices->SetValue().SetInt(splices_total);
            scores.push_back(score_splices);
        }
        {
            CRef<CScore> score_splices_consensus (new CScore());
            score_splices_consensus->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_ConsensusSplices));
            score_splices_consensus->SetValue().SetInt(splices_consensus);
            scores.push_back(score_splices_consensus);
        }
        {
            CRef<CScore> score_coverage (new CScore());
            score_coverage->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_ProductCoverage));
            score_coverage->SetValue().
                SetReal(double(aligned_query_bases) / prod_length_no_polya);
            scores.push_back(score_coverage);
        }
        {
            CRef<CScore> score_exon_identity (new CScore());
            score_exon_identity->SetId().SetStr(
                CSeq_align::ScoreName(CSeq_align::eScore_ExonIdentity));
            score_exon_identity->SetValue().
                SetReal(double(matches) / aln_length_exons);
            scores.push_back(score_exon_identity);
        }

}

double CScoreBuilderBase::ComputeScore(CScope& scope, const CSeq_align& align,
                                       CSeq_align::EScoreType score)
{
    return ComputeScore(scope, align,
                      CRangeCollection<TSeqPos>(TSeqRange::GetWhole()), score);
}

double CScoreBuilderBase::ComputeScore(CScope& scope, const CSeq_align& align,
                                       const TSeqRange &range,
                                       CSeq_align::EScoreType score)
{
    return ComputeScore(scope, align, CRangeCollection<TSeqPos>(range), score);
}

double CScoreBuilderBase::ComputeScore(CScope& scope, const CSeq_align& align,
                                       const CRangeCollection<TSeqPos> &ranges,
                                       CSeq_align::EScoreType score)
{
    switch (score) {
    case CSeq_align::eScore_Score:
        {{
             NCBI_THROW(CSeqalignException, eUnsupported,
                        "CScoreBuilderBase::ComputeScore(): "
                        "generic 'score' computation is undefined");
         }}
        break;

    case CSeq_align::eScore_Blast:
    case CSeq_align::eScore_BitScore:
    case CSeq_align::eScore_EValue:
    case CSeq_align::eScore_SumEValue:
    case CSeq_align::eScore_CompAdjMethod:
        NCBI_THROW(CSeqalignException, eNotImplemented,
                   "CScoreBuilderBase::ComputeScore(): "
                   "BLAST scores are available in CScoreBuilder, "
                   "not CScoreBuilderBase");
        break;

    case CSeq_align::eScore_IdentityCount:
        return GetIdentityCount(scope, align, ranges);

    case CSeq_align::eScore_PositiveCount:
        if (ranges.empty() || !ranges.begin()->IsWhole()) {
            NCBI_THROW(CSeqalignException, eNotImplemented,
                       "positive-count score not supported within a range");
        }
        return GetPositiveCount(scope, align);

    case CSeq_align::eScore_NegativeCount:
        if (ranges.empty() || !ranges.begin()->IsWhole()) {
            NCBI_THROW(CSeqalignException, eNotImplemented,
                       "positive-count score not supported within a range");
        }
        return GetNegativeCount(scope, align);

    case CSeq_align::eScore_MismatchCount:
        return GetMismatchCount(scope, align, ranges);

    case CSeq_align::eScore_GapCount:
        return GetGapCount(align, ranges);

    case CSeq_align::eScore_AlignLength:
        return align.GetAlignLengthWithinRanges(ranges, true /* include gaps */);

    case CSeq_align::eScore_PercentIdentity_Gapped:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eGapped, ranges);
            return pct_identity;
        }}
        break;

    case CSeq_align::eScore_PercentIdentity_Ungapped:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eUngapped, ranges);
            return pct_identity;
        }}
        break;

    case CSeq_align::eScore_PercentIdentity_GapOpeningOnly:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity,
                                 eGBDNA, ranges);
            return pct_identity;
        }}
        break;

    case CSeq_align::eScore_PercentCoverage:
        {{
            double pct_coverage = 0;
            s_GetPercentCoverage(scope, align, ranges, &pct_coverage);
            return pct_coverage;
        }}
        break;

    case CSeq_align::eScore_HighQualityPercentCoverage:
        {{
            if(align.GetSegs().Which() == CSeq_align::TSegs::e_Std)
                /// high-quality-coverage calculatino is not possbile for standard segs
                NCBI_THROW(CSeqalignException, eUnsupported,
                            "High-quality percent coverage not supported "
                            "for standard seg representation");

            if (ranges.empty() || !ranges.begin()->IsWhole()) {
                NCBI_THROW(CSeqalignException, eNotImplemented,
                           "High-quality percent coverage not supported "
                           "within a range");
            }
            /// If we have annotation for a high-quality region, it is in a ftable named
            /// "NCBI_GPIPE", containing a region Seq-feat named "alignable"
            TSeqRange alignable_range = TSeqRange::GetWhole();
            CBioseq_Handle query = scope.GetBioseqHandle(align.GetSeq_id(0));
            for(CFeat_CI feat_it(query,
                                 SAnnotSelector(CSeqFeatData::e_Region).
                                 SetExcludeExternal());
                    feat_it; ++feat_it)
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
            s_GetPercentCoverage(scope, align,
                                 CRangeCollection<TSeqPos>(alignable_range),
                                 &pct_coverage);
            return pct_coverage;
        }}
        break;

    case CSeq_align::eScore_Matches:
    case CSeq_align::eScore_OverallIdentity:
    case CSeq_align::eScore_Splices:
    case CSeq_align::eScore_ConsensusSplices:
    case CSeq_align::eScore_ProductCoverage:
    case CSeq_align::eScore_ExonIdentity:
        {{
             if (ranges.empty() || !ranges.begin()->IsWhole()) {
                 NCBI_THROW(CSeqalignException, eNotImplemented,
                            "splign scores not supported within a range");
             }
             CSeq_align::TScore scores;
             AddSplignScores(align, scores);
             ITERATE (CSeq_align::TScore, it, scores) {
                 if ((*it)->GetId().GetStr() == CSeq_align::ScoreName(score))
                 {
                     if ((*it)->GetValue().IsInt()) {
                         return (*it)->GetValue().GetInt();
                     } else {
                         return (*it)->GetValue().GetReal();
                     }
                 }
             }
             NCBI_ASSERT(false, "Should never reach this point");
        }}

    default:
        {{
            NCBI_THROW(CSeqalignException, eNotImplemented,
                       "Unknown score");
            return 0;
        }}
    }
}





END_NCBI_SCOPE

