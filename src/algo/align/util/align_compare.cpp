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

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>

#include <algo/align/util/align_compare.hpp>
#include <algo/align/util/score_lookup.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>

#include <cmath>
#include <ctype.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// Retrieve a list of interval-by-interval accounting within an alignment
//

static void s_UpdateSpans(const TSeqRange &query_range,
                          const TSeqRange &subject_range,
                          CAlignCompare::SAlignment& align_info,
                          CAlignCompare::ERowComparison row)
{
     align_info.spans[row == CAlignCompare::e_Query ? query_range : subject_range] =
         row == CAlignCompare::e_Subject ? subject_range : query_range;
}

static void s_GetAlignmentSpans_Interval(const CSeq_align& align,
                                         CAlignCompare::SAlignment& align_info,
                                         CAlignCompare::ERowComparison row)
{
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Disc:
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, it,
                 align.GetSegs().GetDisc().Get()) {
            s_GetAlignmentSpans_Interval(**it, align_info, row);
        }
        break;

    default:
        {{
             CAlnSeqId id0(align.GetSeq_id(0));
             CAlnSeqId id1(align.GetSeq_id(1));

             TAlnSeqIdIRef r0(&id0);
             TAlnSeqIdIRef r1(&id1);
             CPairwiseAln pw(r0, r1);
             ConvertSeqAlignToPairwiseAln(pw, align, 0, 1);
             ITERATE (CPairwiseAln, it, pw) {
                 const CPairwiseAln::TAlignRange& r = *it;
                 s_UpdateSpans(TSeqRange(r.GetFirstFrom(), r.GetFirstTo()),
                               TSeqRange(r.GetSecondFrom(), r.GetSecondTo()),
                               align_info, row);
             }
         }}
        break;
    }
}

static void s_GetAlignmentMismatches(const CSeq_align& align,
                                     CAlignCompare::SAlignment& align_info,
                                     CAlignCompare::ERowComparison row)
{
    if (!align.GetSegs().IsSpliced()) {
        CScoreLookup lookup;
        string traceback = lookup.GetTraceback(align, 0);
        if (traceback.empty()) {
            NCBI_THROW(CException, eUnknown,
                       "Comparing mismatches for dense-seg alignments requires "
                       "traceback information");
        }
        int product_dir = align.GetSeqStrand(0) == eNa_strand_minus
                           ? -1 : 1;
        TSeqPos product_pos = product_dir == 1 ? align.GetSeqStart(0)
                                                : align.GetSeqStop(0);
        int genomic_dir = align.GetSeqStrand(1) == eNa_strand_minus
                           ? -1 : 1;
        TSeqPos genomic_pos = product_dir == 1 ? align.GetSeqStart(1)
                                                : align.GetSeqStop(1);
        unsigned match = 0;
        ITERATE (string, it, traceback) {
            if (isdigit(*it)) {
                match = match * 10 + (*it - '0');
                continue;
            }
            product_pos += match * product_dir;
            genomic_pos += match * genomic_dir;
            match = 0;
            bool genomic_ins = *it == '-';
            bool product_ins = *++it == '-';
            if (!genomic_ins && !product_ins) {
                /// mismatch
                if (row != CAlignCompare::e_Subject) {
                    align_info.query_mismatches += TSeqRange(
                        product_pos, product_pos);

                }
                if (row != CAlignCompare::e_Query) {
                    align_info.subject_mismatches += TSeqRange(
                        genomic_pos, genomic_pos);
                }
            }
            if (!genomic_ins) {
                product_pos += product_dir;
            }
            if (!product_ins) {
                genomic_pos += genomic_dir;
            }
        }
        if (product_pos != (product_dir == 1 ? align.GetSeqStop(0)+1
                                              : align.GetSeqStart(0)-1)
         || genomic_pos != (genomic_dir == 1 ? align.GetSeqStop(1)+1
                                              : align.GetSeqStart(1)-1))
        {
           NCBI_THROW(CException, eUnknown,
                      "Inconsistent length of traceback string " + traceback);
        }
        return;
    }

    bool is_product_minus = align.GetSegs().GetSpliced().IsSetProduct_strand() &&
                            align.GetSegs().GetSpliced().GetProduct_strand() == eNa_strand_minus;
    bool is_genomic_minus = align.GetSegs().GetSpliced().IsSetGenomic_strand() &&
                            align.GetSegs().GetSpliced().GetGenomic_strand() == eNa_strand_minus;
    ITERATE (CSpliced_seg::TExons, it, align.GetSegs().GetSpliced().GetExons())
    {
        const CSpliced_exon& exon = **it;
        if (!exon.IsSetParts()) {
            continue;
        }
        int product_dir = is_product_minus ||
                          (exon.IsSetProduct_strand() &&
                           exon.GetProduct_strand() == eNa_strand_minus)
                           ? -1 : 1;
        TSeqPos product_pos = (product_dir == 1 ? exon.GetProduct_start()
                                                 : exon.GetProduct_end())
                              . AsSeqPos();
        int genomic_dir = is_genomic_minus ||
                          (exon.IsSetGenomic_strand() &&
                           exon.GetGenomic_strand() == eNa_strand_minus)
                           ? -1 : 1;
        TSeqPos genomic_pos = genomic_dir == 1 ? exon.GetGenomic_start()
                                                : exon.GetGenomic_end();
        ITERATE (CSpliced_exon::TParts, part_it, exon.GetParts()) {
            switch ((*part_it)->Which()) {
                case CSpliced_exon_chunk::e_Mismatch:
                    if (row != CAlignCompare::e_Subject) {
                        TSeqPos product_mismatch_end = product_pos +
                              product_dir * ((*part_it)->GetMismatch()-1);
                        align_info.query_mismatches += TSeqRange(
                            min(product_pos,product_mismatch_end),
                            max(product_pos,product_mismatch_end));

                    }
                    if (row != CAlignCompare::e_Query) {
                        TSeqPos genomic_mismatch_end = genomic_pos +
                              genomic_dir * ((*part_it)->GetMismatch()-1);
                        align_info.subject_mismatches += TSeqRange(
                            min(genomic_pos,genomic_mismatch_end),
                            max(genomic_pos,genomic_mismatch_end));
                    }
                    product_pos += product_dir * (*part_it)->GetMismatch();
                    genomic_pos += genomic_dir * (*part_it)->GetMismatch();
                    break;

                case CSpliced_exon_chunk::e_Match:
                    product_pos += product_dir * (*part_it)->GetMatch();
                    genomic_pos += genomic_dir * (*part_it)->GetMatch();
                    break;

                case CSpliced_exon_chunk::e_Product_ins:
                    product_pos += product_dir * (*part_it)->GetProduct_ins();
                    break;

                case CSpliced_exon_chunk::e_Genomic_ins:
                    genomic_pos += genomic_dir * (*part_it)->GetGenomic_ins();
                    break;

                default:
                    NCBI_THROW(CException, eUnknown,
                               "Unsupported exon part");
            }
        }
    }
}

// Retrieve a list of exon-by-exon accounting within an alignment
//
static void s_GetAlignmentSpans_Exon(const CSeq_align& align,
                                     CAlignCompare::SAlignment &align_info,
                                     CAlignCompare::ERowComparison row)
{
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        s_UpdateSpans(align.GetSeqRange(0), align.GetSeqRange(1), align_info, row);
        break;

    case CSeq_align::TSegs::e_Disc:
        /* UNTESTED */
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, it,
                 align.GetSegs().GetDisc().Get()) {
            s_UpdateSpans((*it)->GetSeqRange(0), (*it)->GetSeqRange(1), align_info, row);
        }
        break;

    case CSeq_align::TSegs::e_Std:
        /* UNTESTED */
        ITERATE (CSeq_align::TSegs::TStd, it, align.GetSegs().GetStd()) {
            const CStd_seg& seg = **it;
            s_UpdateSpans(seg.GetLoc()[0]->GetTotalRange(),
                          seg.GetLoc()[1]->GetTotalRange(), align_info, row);
        }
        break;

    case CSeq_align::TSegs::e_Spliced:
        ITERATE (CSpliced_seg::TExons, it,
                 align.GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **it;
            TSeqRange genomic(exon.GetGenomic_start(), exon.GetGenomic_end());
            TSeqRange product;
            product.SetFrom(exon.GetProduct_start().AsSeqPos());
            product.SetTo(exon.GetProduct_end().AsSeqPos());
            s_UpdateSpans(product, genomic, align_info, row);
        }
        break;

    default:
        NCBI_THROW(CException, eUnknown,
                   "unhandled alignment type");
    }
}


// Retrieve a list of intron-by-intron accounting within an alignment;
// meaningful only for Spliced-seg alignments
//
static void s_GetAlignmentSpans_Intron(const CSeq_align& align,
                                       CAlignCompare::SAlignment &align_info,
                                       CAlignCompare::ERowComparison row)
{
    if (!align.GetSegs().IsSpliced() ||
        !align.GetSegs().GetSpliced().CanGetProduct_strand() ||
        !align.GetSegs().GetSpliced().CanGetGenomic_strand())
    {
        NCBI_THROW(CException, eUnknown,
                   "intron mode only meaningful for Spliced-seg alignments");
    }

    bool is_reverse = align.GetSegs().GetSpliced().GetProduct_strand() !=
                          align.GetSegs().GetSpliced().GetGenomic_strand();

    CRef<CSpliced_exon> last_exon;
    ITERATE (CSpliced_seg::TExons, it,
             align.GetSegs().GetSpliced().GetExons()) {
        CRef<CSpliced_exon> exon = *it;
        if (last_exon) {
            CRef<CSpliced_exon> first_exon = is_reverse ? exon : last_exon;
            CRef<CSpliced_exon> second_exon = is_reverse ? last_exon : exon;
            TSeqRange genomic(first_exon->GetGenomic_end(),
                              second_exon->GetGenomic_start());
            TSeqRange product;
            product.SetFrom(last_exon->GetProduct_end().AsSeqPos());
            product.SetTo(exon->GetProduct_start().AsSeqPos());
            s_UpdateSpans(product, genomic, align_info, row);
        }
        last_exon = exon;
    }
}


// Retrieve a list of total range spans for an alignment
//
static void s_GetAlignmentSpans_Span(const CSeq_align& align,
                                     CAlignCompare::SAlignment &align_info,
                                     CAlignCompare::ERowComparison row)
{
    s_UpdateSpans(align.GetSeqRange(0), align.GetSeqRange(1), align_info, row);
}

template<typename T>
void s_PopulateScores(const CSeq_align &align, 
                      const vector<string> &score_list, 
                      vector<T> &scores,
                      bool required = true)
{
    CScoreLookup lookup;
    ITERATE (vector<string>, it, score_list) {
        T value = 0;
        try {
            value = lookup.GetScore(align, *it);
        } catch(CAlgoAlignUtilException &e) {
            /// If scores are not required, use value of 0 for scores that were not found
            if (required || e.GetErrCode() != CAlgoAlignUtilException::eScoreNotFound) {
                throw;
            }
        }
        scores.push_back(value);
    }
}

static void s_PopulateScoreSet(const CSeq_align &align,
                               const set<string> &score_set,
                               bool score_set_as_blacklist,
                               CAlignCompare::TIntegerScoreSet &integer_scores,
                               CAlignCompare::TRealScoreSet &real_scores)
{
    if (score_set.empty() && !score_set_as_blacklist) {
        /// Empty set of scores; nothing to do
        return;
    }
    ITERATE (CSeq_align::TScore, score_it, align.GetScore()) {
        if ((*score_it)->GetId().IsStr() &&
                (score_set_as_blacklist ? 0 : 1) ==
                score_set.count((*score_it)->GetId().GetStr()))
        {
            if ((*score_it)->GetValue().IsInt()) {
                integer_scores[(*score_it)->GetId().GetStr()] =
                    (*score_it)->GetValue().GetInt();
            } else {
                real_scores[(*score_it)->GetId().GetStr()] =
                    (*score_it)->GetValue().GetReal();
            }
        }
    }
}

static bool s_EquivalentScores(const CAlignCompare::TRealScoreSet &scores1,
                               const CAlignCompare::TRealScoreSet &scores2,
                               double real_score_tolerance)
{
    for (CAlignCompare::TRealScoreSet::const_iterator it1 = scores1.begin(),
                                                      it2 = scores2.begin();
         it1 != scores1.end() || it2 != scores2.end(); ++it1, ++it2)
    {
        if (it1 == scores1.end() || it2 == scores2.end()
                                 || it1->first != it2->first)
        {
            /// The two don't have the same set of real-value scores
            return false;
        }
        double allowed_diff = max(abs(it1->second), abs(it2->second))
                            * real_score_tolerance;
        if (abs(it1->second - it2->second) > allowed_diff) {
            return false;
        }
    }
    return true;
}

struct SComparison
{
    SComparison(const CAlignCompare::SAlignment& first,
                const CAlignCompare::SAlignment& second,
                double real_score_tolerance);

    size_t spans_in_common;
    size_t spans_overlap;
    size_t spans_unique_first;
    size_t spans_unique_second;
    bool is_equivalent;
    float overlap;
};

//////////////////////////////////////////////////////////////////////////////
//
// SComparison constructor does the hard work of verifying that two sets of alignment
// span ranges actually overlap, and determines by how much
//
SComparison::SComparison(const CAlignCompare::SAlignment& first,
                         const CAlignCompare::SAlignment& second,
                         double real_score_tolerance)
: spans_in_common(0)
, spans_overlap(0)
, spans_unique_first(0)
, spans_unique_second(0)
, is_equivalent(false)
, overlap(0)
{
    if (first.CompareGroup(second, false) != 0) {
        /// Alignments have different disambiguiting score values, can't be compared
        return;
    }

    float dot = 0;
    float sum_a = 0;
    float sum_b = 0;

    CAlignCompare::TAlignmentSpans::const_iterator first_it = first.spans.begin();
    CAlignCompare::TAlignmentSpans::const_iterator second_it = second.spans.begin();
    for ( ;  first_it != first.spans.end()  &&  second_it != second.spans.end();  ) {
        if (*first_it == *second_it) {
            TSeqPos intersecting_len = first_it->first.GetLength();
            dot += float(intersecting_len) * float(intersecting_len);
            sum_a += first_it->first.GetLength() * first_it->first.GetLength();
            sum_b += second_it->first.GetLength() * second_it->first.GetLength();

            ++spans_in_common;
            ++first_it;
            ++second_it;
        } else {
            bool overlap =
                first_it->first.IntersectingWith(second_it->first)  &&
                first_it->second.IntersectingWith(second_it->second);
            if (overlap) {
                TSeqRange r = first_it->first;
                r.IntersectWith(second_it->first);

                TSeqPos intersecting_len = r.GetLength();
                dot += float(intersecting_len) * float(intersecting_len);
                sum_a += first_it->first.GetLength() * first_it->first.GetLength();
                sum_b += second_it->first.GetLength() * second_it->first.GetLength();

                ++spans_overlap;
            }
            if (*first_it < *second_it) {
                sum_a += first_it->first.GetLength() * first_it->first.GetLength();
                if ( !overlap ) {
                    ++spans_unique_first;
                }
                ++first_it;
            } else {
                sum_b += second_it->first.GetLength() * second_it->first.GetLength();
                if ( !overlap ) {
                    ++spans_unique_second;
                }
                ++second_it;
            }
        }
    }
    is_equivalent = spans_in_common == first.spans.size() &&
                    spans_in_common == second.spans.size() &&
                    first.query_mismatches == second.query_mismatches &&
                    first.subject_mismatches == second.subject_mismatches &&
                    first.integer_scores == second.integer_scores &&
                    s_EquivalentScores(first.real_scores, second.real_scores,
                                       real_score_tolerance);
    for ( ;  first_it != first.spans.end();  ++first_it, ++spans_unique_first) {
        sum_a += first_it->first.GetLength() * first_it->first.GetLength();
    }
    for ( ;  second_it != second.spans.end();  ++second_it, ++spans_unique_second) {
        sum_b += second_it->first.GetLength() * second_it->first.GetLength();
    }

    overlap = dot == 0 ? 0 : dot / ::sqrt(sum_a * sum_b);
}


struct SAlignment_PtrLess
{
    bool operator()(const CAlignCompare::SAlignment *ptr1,
                    const CAlignCompare::SAlignment *ptr2) const
    {
        const CAlignCompare::SAlignment& k1 = *ptr1;
        const CAlignCompare::SAlignment& k2 = *ptr2;

        if (k1.query < k2.query)                    { return true; }
        if (k2.query < k1.query)                    { return false; }
        if (k1.subject < k2.subject)                { return true; }
        if (k2.subject < k1.subject)                { return false; }

        if (k1.scores < k2.scores)                  { return true; }
        if (k2.scores < k1.scores)                  { return false; }

        if (k1.query_strand < k2.query_strand)      { return true; }
        if (k2.query_strand < k1.query_strand)      { return false; }
        if (k1.subject_strand < k2.subject_strand)  { return true; }
        if (k2.subject_strand < k1.subject_strand)  { return false; }

        if (k1.subject_range < k2.subject_range)    { return true; }
        if (k2.subject_range < k1.subject_range)    { return false; }
        if (k1.query_range < k2.query_range)        { return true; }
        if (k2.query_range < k1.query_range)        { return false; }

        return ptr1 < ptr2;
    }
};

typedef set<CAlignCompare::SAlignment *, SAlignment_PtrLess> TAlignPtrSet;
typedef pair<CAlignCompare::SAlignment *, CAlignCompare::SAlignment *> TPtrPair;
typedef pair<TPtrPair, SComparison> TComp;

struct SComp_Less
{
    bool strict_compare;

    SComp_Less(bool strict = false)
        : strict_compare(strict)
    {
    }

    bool operator()(const TComp& c1, const TComp& c2) const
    {
        // strict comparison amounts to placing all identical pairs either before
        // or after non-identical ones
        // putting identical pairs first means that we evaluate the best examples first, and
        // can establish equality without polluting the comparison with weaker
        // alignments; non-strict means we combine weaker overlapping
        // alignments together into equivalence groups with alignments that are
        // identical
        if (strict_compare) {
            if (c1.second.is_equivalent && !c2.second.is_equivalent) {
                return true;
            }
            if (c2.second.is_equivalent && !c1.second.is_equivalent) {
                return false;
            }
        }
        else {
            if (c1.second.is_equivalent && !c2.second.is_equivalent) {
                return false;
            }
            if (c2.second.is_equivalent && !c1.second.is_equivalent) {
                return true;
            }
        }

        if (c1.first.first->subject_range < c2.first.first->subject_range)
        {
            return false;
        }
        if (c2.first.first->subject_range < c1.first.first->subject_range)
        {
            return true;
        }
        return c1.first.second->query_range < c2.first.second->query_range;
    }
};

CAlignCompare::SAlignment::
SAlignment(int s, const CRef<CSeq_align> &al, EMode mode, ERowComparison row,
                  const TDisambiguatingScoreList &score_list,
                  const vector<string> &quality_score_list,
                  const set<string> &score_set,
                  bool score_set_as_blacklist)
: source_set(s)
, query_strand(eNa_strand_unknown)
, subject_strand(eNa_strand_unknown)
, align(al)
, match_level(CAlignCompare::e_NoMatch)
{
    if (row != e_Subject) {
        query = CSeq_id_Handle::GetHandle(align->GetSeq_id(0));
        query_strand = align->GetSeqStrand(0);
        query_range = align->GetSeqRange(0);
    }
    if (row != e_Query) {
        subject = CSeq_id_Handle::GetHandle(align->GetSeq_id(1));
        subject_strand = align->GetSeqStrand(1);
        subject_range = align->GetSeqRange(1);
    }
    try {
        s_PopulateScores(*align, score_list.first, scores.first);
        s_PopulateScores(*align, score_list.second, scores.second, false);
        s_PopulateScores(*align, quality_score_list, quality_scores);
        s_PopulateScoreSet(*align, score_set, score_set_as_blacklist,
                           integer_scores, real_scores);
        switch (mode) {
        case e_Full:
            s_GetAlignmentMismatches(*align, *this, row);
            // fall through

        case e_Interval:
            s_GetAlignmentSpans_Interval(*align, *this, row);
            break;

        case e_Exon:
            s_GetAlignmentSpans_Exon(*align, *this, row);
            break;

        case e_Span:
            s_GetAlignmentSpans_Span(*align, *this, row);
            break;

        case e_Intron:
            s_GetAlignmentSpans_Intron(*align, *this, row);
            break;
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "alignment not processed: " << e);
        spans.clear();
    }
}

int CAlignCompare::SAlignment::
CompareGroup(const SAlignment &o, bool strict_only) const
{
    if (query.AsString() < o.query.AsString()) { return -1; }
    if (o.query.AsString() < query.AsString()) { return 1; }

    if (subject.AsString() < o.subject.AsString()) { return -1; }
    if (o.subject.AsString() < subject.AsString()) { return 1; }

    if (scores.first < o.scores.first) { return -1; }
    if (o.scores.first < scores.first) { return 1; }

    if (strict_only) {
        return 0;
    }

    for (unsigned score_index = 0; score_index < scores.second.size(); ++score_index) {
        if (scores.second[score_index] && o.scores.second[score_index]) {
            if (scores.second[score_index] < o.scores.second[score_index]) { return -1; }
            if (o.scores.second[score_index] < scores.second[score_index]) { return 1; }
        }
    }

    return 0;
}

int CAlignCompare::x_DetermineNextGroupSet()
{
    if (m_NextSet1Group.empty()) {
        if (m_Set1.EndOfData()) {
            return 2;
        } else {
            m_NextSet1Group.push_back(x_NextAlignment(1));
        }
    }
    if (m_NextSet2Group.empty()) {
        if (m_Set2.EndOfData()) {
            return 1;
        } else {
            m_NextSet2Group.push_back(x_NextAlignment(2));
        }
    }
    int compare_group = m_NextSet1Group.front()
                      ->CompareGroup(*m_NextSet2Group.front(), true);
    if (compare_group < 0) {
        return 1;
    } else if (compare_group > 0) {
        return 2;
    } else {
        return 3;
    }
}

void CAlignCompare::x_GetCurrentGroup(int set)
{
    IAlignSource &source = set == 1 ? m_Set1 : m_Set2;
    list< AutoPtr<SAlignment> > &current_group =
        set == 1 ? m_CurrentSet1Group : m_CurrentSet2Group;
    list< AutoPtr<SAlignment> > &next_group =
        set == 1 ? m_NextSet1Group : m_NextSet2Group;
    current_group.clear();
    current_group.splice(current_group.end(), next_group);
    while (!source.EndOfData() && next_group.empty()) {
        AutoPtr<SAlignment> align = x_NextAlignment(set);
        if (current_group.empty() || align->CompareGroup(*current_group.front(), true) == 0)
        {
            current_group.push_back(align);
        } else {
            next_group.push_back(align);
        }
    }
}

vector<const CAlignCompare::SAlignment *> CAlignCompare::NextGroup()
{
    int next_group_set = x_DetermineNextGroupSet();
    if (next_group_set & 1) {
        x_GetCurrentGroup(1);
    }
    if (next_group_set & 2) {
        x_GetCurrentGroup(2);
    }

    vector<const SAlignment *> group;
    switch (next_group_set) {
    case 1:
        if (!m_IgnoreNotPresent) {
            m_CountOnlySet1 += m_CurrentSet1Group.size();
            ITERATE (list< AutoPtr<SAlignment> >, it, m_CurrentSet1Group) {
                group.push_back(&**it);
            }
        }
        break;

    case 2:
        if (!m_IgnoreNotPresent) {
            m_CountOnlySet2 += m_CurrentSet2Group.size();
            ITERATE (list< AutoPtr<SAlignment> >, it, m_CurrentSet2Group) {
                group.push_back(&**it);
            }
        }
        break;

    default:
    {{
        TAlignPtrSet set1_aligns;
        NON_CONST_ITERATE (list< AutoPtr<SAlignment> >, it, m_CurrentSet1Group)
        {
            set1_aligns.insert(&**it);
        }
        TAlignPtrSet set2_aligns;
        NON_CONST_ITERATE (list< AutoPtr<SAlignment> >, it, m_CurrentSet2Group)
        {
            set2_aligns.insert(&**it);
        }

        vector<TComp> comparisons;
        ITERATE (TAlignPtrSet, set1_it, set1_aligns) {
            ITERATE (TAlignPtrSet, set2_it, set2_aligns) {
                comparisons.push_back(TComp(TPtrPair(*set1_it, *set2_it),
                                            SComparison(**set1_it, **set2_it,
                                                        m_RealScoreTolerance)));
            }
        }
        std::sort(comparisons.begin(), comparisons.end(), SComp_Less(m_Strict));

        typedef pair<TAlignPtrSet, EMatchLevel> TAlignGroup;

        list<TAlignGroup> groups;
        map<const SAlignment *, list<TAlignGroup>::iterator> group_map;

        ITERATE (vector<TComp>, it, comparisons) {
            bool is_equivalent = it->second.is_equivalent;
            /// This comparison counts if the two alignments are equivalent, or
            /// they overlap and we haven't yet seen an equivalence for either
            if (is_equivalent ||
                    (it->second.overlap > 0 &&
                     it->first.first->match_level != e_Equiv &&
                     it->first.second->match_level != e_Equiv))
            {
                list<TAlignGroup>::iterator align1_group = groups.end(),
                                            align2_group = groups.end();
                if (set1_aligns.erase(it->first.first)) {
                    it->first.first->match_level =
                        is_equivalent ? e_Equiv : e_Overlap;
                    group.push_back(it->first.first);
                    ++(is_equivalent ? m_CountEquivSet1 : m_CountOverlapSet1);
                } else {
                    align1_group = group_map[it->first.first];
                }
                if (set2_aligns.erase(it->first.second)) {
                    it->first.second->match_level =
                        is_equivalent ? e_Equiv : e_Overlap;
                    group.push_back(it->first.second);
                    ++(is_equivalent ? m_CountEquivSet2 : m_CountOverlapSet2);
                } else {
                    align2_group = group_map[it->first.second];
                }
                if (align1_group == groups.end() &&
                    align2_group == groups.end())
                {
                    /// Neither alignemnts was encountered before, so create
                    /// new group
                    list<TAlignGroup>::iterator new_group =
                        groups.insert(groups.end(), TAlignGroup());
                    new_group->first.insert(it->first.first);
                    new_group->first.insert(it->first.second);
                    new_group->second = it->first.first->match_level;
                    group_map[it->first.first] = new_group;
                    group_map[it->first.second] = new_group;
                    ++(is_equivalent ? m_CountEquivGroups
                                     : m_CountOverlapGroups);
                } else if(align1_group == groups.end()) {
                    /// alignment 1 is new, add it to existing group
                    align2_group->first.insert(it->first.first);
                    group_map[it->first.first] = align2_group;
                } else if(align2_group == groups.end()) {
                    /// alignment 2 is new, add it to existing group
                    align1_group->first.insert(it->first.second);
                    group_map[it->first.second] = align1_group;
                } else if (align1_group != align2_group) {
                    /// The alignments are in two separate groups; merge them
                    ITERATE (TAlignPtrSet, group2_it, align2_group->first) {
                        align1_group->first.insert(*group2_it);
                        group_map[*group2_it] = align1_group;
                    }
                    if (align2_group->second == e_Overlap) {
                        --m_CountOverlapGroups;
                        if (align1_group->second == e_Equiv) {
                            /// Change the group from equivalence to overlap
                            align1_group->second = e_Overlap;
                            ++m_CountOverlapGroups;
                            --m_CountEquivGroups;
                        }
                    } else {
                        --m_CountEquivGroups;
                    }
                    groups.erase(align2_group);
                }
            }
        }

        ITERATE (list<TAlignGroup>, group_it, groups) {
            if (group_it->second == e_NoMatch) {
                continue;
            }
            if (group_it->second == e_Overlap && !m_QualityScores.empty())
            {
                /// Find which side is better
                vector<SAlignment *> best(3, static_cast<SAlignment *>(NULL));
                ITERATE (TAlignPtrSet, align_it, group_it->first) {
                    SAlignment *&side_best = best[(*align_it)->source_set];
                    if (!side_best || (*align_it)->quality_scores >
                                      side_best->quality_scores)
                    {
                        side_best = *align_it;
                    }
                }
                if (best[1]->quality_scores != best[2]->quality_scores) {
                    int better_side =
                        best[1]->quality_scores > best[2]->quality_scores
                                  ? 1 : 2;
                    ITERATE (TAlignPtrSet, align_it, group_it->first) {
                        (*align_it)->match_level =
                             (*align_it)->source_set == better_side 
                                 ? e_OverlapBetter : e_OverlapWorse;
                    }
                }
            }
            ITERATE (TAlignPtrSet, align1_it, group_it->first) {
                if ((*align1_it)->source_set != 1) {
                    continue;
                }
                ITERATE (TAlignPtrSet, align2_it, group_it->first) {
                    if ((*align2_it)->source_set != 2) {
                        continue;
                    }
                    (*align1_it)->matched_alignments.push_back(*align2_it);
                    (*align2_it)->matched_alignments.push_back(*align1_it);
                }
            }
        }

        /// Add remaining alignments, for which no match was found, in order
        /// of their appearance in alignment comparisons
        m_CountOnlySet1 += set1_aligns.size();
        m_CountOnlySet2 += set2_aligns.size();
        ITERATE (vector<TComp>, comp_it, comparisons) {
            if (set1_aligns.empty() && set2_aligns.empty()) {
                /// Found best comparison for all alignments
                break;
            }
            if (comp_it->second.overlap == 0) {
                if (set1_aligns.erase(comp_it->first.first)) {
                    group.push_back(comp_it->first.first);
                }
                if (set2_aligns.erase(comp_it->first.second)) {
                    group.push_back(comp_it->first.second);
                }
            }
        }
    }}
    }

    return group;
}

END_NCBI_SCOPE
