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
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>

#include <algo/align/util/align_compare.hpp>

#include <cmath>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// Retrieve a list of interval-by-interval accounting within an alignment
//
static void s_GetAlignmentSpans_Interval(const CSeq_align& align,
                                         CAlignCompare::TAlignmentSpans& spans)
{
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Disc:
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, it,
                 align.GetSegs().GetDisc().Get()) {
            s_GetAlignmentSpans_Interval(**it, spans);
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
                 spans[TSeqRange(r.GetSecondFrom(), r.GetSecondTo())] =
                     TSeqRange(r.GetFirstFrom(), r.GetFirstTo());
             }
         }}
        break;
    }
}

// Retrieve a list of exon-by-exon accounting within an alignment
//
static void s_GetAlignmentSpans_Exon(const CSeq_align& align,
                                     CAlignCompare::TAlignmentSpans& spans)
{
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        spans[align.GetSeqRange(1)] = align.GetSeqRange(0);
        break;

    case CSeq_align::TSegs::e_Disc:
        /* UNTESTED */
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, it,
                 align.GetSegs().GetDisc().Get()) {
            spans[(*it)->GetSeqRange(1)] = (*it)->GetSeqRange(0);
        }
        break;

    case CSeq_align::TSegs::e_Std:
        /* UNTESTED */
        ITERATE (CSeq_align::TSegs::TStd, it, align.GetSegs().GetStd()) {
            const CStd_seg& seg = **it;
            spans[seg.GetLoc()[1]->GetTotalRange()] =
                seg.GetLoc()[0]->GetTotalRange();
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
            spans[genomic] = product;
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
                                       CAlignCompare::TAlignmentSpans& spans)
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
            spans[genomic] = product;
        }
        last_exon = exon;
    }
}


// Retrieve a list of total range spans for an alignment
//
static void s_GetAlignmentSpans_Span(const CSeq_align& align,
                                     CAlignCompare::TAlignmentSpans& spans)
{
    spans[align.GetSeqRange(1)] = align.GetSeqRange(0);
}

struct SComparison
{
    SComparison(const CAlignCompare::TAlignmentSpans& first,
                const CAlignCompare::TAlignmentSpans& second);

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
SComparison::SComparison(const CAlignCompare::TAlignmentSpans& first,
                         const CAlignCompare::TAlignmentSpans& second)
{
    spans_in_common = 0;
    spans_overlap = 0;
    spans_unique_first = 0;
    spans_unique_second = 0;

    float dot = 0;
    float sum_a = 0;
    float sum_b = 0;

    CAlignCompare::TAlignmentSpans::const_iterator first_it = first.begin();
    CAlignCompare::TAlignmentSpans::const_iterator second_it = second.begin();
    for ( ;  first_it != first.end()  &&  second_it != second.end();  ) {
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
    is_equivalent = spans_in_common == first.size() &&
                    spans_in_common == second.size();
    for ( ;  first_it != first.end();  ++first_it, ++spans_unique_first) {
        sum_a += first_it->first.GetLength() * first_it->first.GetLength();
    }
    for ( ;  second_it != second.end();  ++second_it, ++spans_unique_second) {
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

    SComp_Less()
        : strict_compare(false)
    {
    }

    SComp_Less(bool strict)
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

        if (c1.first.first->subject_range < c2.first.first->subject_range) {
            return false;
        }
        if (c2.first.first->subject_range < c1.first.first->subject_range) {
            return true;
        }
        return c1.first.second->query_range < c2.first.second->query_range;
    }
};

CAlignCompare::SAlignment::
SAlignment(int s, const CRef<CSeq_align> &al, EMode mode)
: source_set(s)
, query(CSeq_id_Handle::GetHandle(al->GetSeq_id(0)))
, subject(CSeq_id_Handle::GetHandle(al->GetSeq_id(1)))
, query_strand(al->GetSeqStrand(0))
, subject_strand(al->GetSeqStrand(1))
, query_range(al->GetSeqRange(0))
, subject_range(al->GetSeqRange(1))
, align(al)
, match_level(CAlignCompare::e_NoMatch)
{
    try {
        switch (mode) {
        case e_Interval:
            s_GetAlignmentSpans_Interval(*align, spans);
            break;

        case e_Exon:
            s_GetAlignmentSpans_Exon(*align, spans);
            break;

        case e_Span:
            s_GetAlignmentSpans_Span(*align, spans);
            break;

        case e_Intron:
            s_GetAlignmentSpans_Intron(*align, spans);
            break;
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "alignment not processed: " << e);
        spans.clear();
    }
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
    if (m_NextSet1Group.front()->query != m_NextSet2Group.front()->query) {
        return m_NextSet1Group.front()->query.GetSeqId()->AsFastaString()
             < m_NextSet2Group.front()->query.GetSeqId()->AsFastaString()
             ? 1 : 2;
    } else if (m_NextSet1Group.front()->subject !=
               m_NextSet2Group.front()->subject)
    {
        return m_NextSet1Group.front()->subject.GetSeqId()->AsFastaString()
             < m_NextSet2Group.front()->subject.GetSeqId()->AsFastaString()
             ? 1 : 2;
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
        if (current_group.empty() ||
            (align->query == current_group.front()->query &&
             align->subject == current_group.front()->subject))
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
                                            SComparison((*set1_it)->spans,
                                                        (*set2_it)->spans)));
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

        /// Add remaining alignments, for which no match was found, in order
        /// of their appearance in alignment6 comparisons
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
