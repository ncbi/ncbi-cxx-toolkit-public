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
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <algo/align/util/genomic_compart.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqalign/Dense_seg.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//#define _VERBOSE_DEBUG


bool IsIntersectingQuery(const pair<TSeqRange, TSeqRange>& r1,
                         const pair<TSeqRange, TSeqRange>& r2)
{
    bool is_intersecting =
        r1.first.IntersectingWith(r2.first);

#ifdef _VERBOSE_DEBUG
    cerr << r1.first
        << " x "
        << r2.first
        << ": is_intersecting = "
        << (is_intersecting ? "true" : "false")
        << endl;
#endif

    return is_intersecting;
}

bool IsIntersectingSubject(const pair<TSeqRange, TSeqRange>& r1,
                           const pair<TSeqRange, TSeqRange>& r2)
{
    bool is_intersecting =
        r1.second.IntersectingWith(r2.second);

#ifdef _VERBOSE_DEBUG
    cerr << r1.second
        << " x "
        << r2.second
        << ": is_intersecting = "
        << (is_intersecting ? "true" : "false")
        << endl;
#endif

    return is_intersecting;
}

bool IsConsistent(const pair<TSeqRange, TSeqRange>& r1,
                  const pair<TSeqRange, TSeqRange>& r2,
                  ENa_strand s1, ENa_strand s2)
{
    bool is_consistent = false;
    if (s1 == s2) {
        is_consistent = (r1.first <= r2.first  &&  r1.second <= r2.second)  ||
                        (r2.first <= r1.first  &&  r2.second <= r1.second);
    }
    else {
        is_consistent = (r1.first <= r2.first  &&  r2.second <= r1.second)  ||
                        (r2.first <= r1.first  &&  r1.second <= r2.second);
    }

#ifdef _VERBOSE_DEBUG
    cerr << "("
        << r1.first << ", "
        << r1.second
        << ", " << (s1 == eNa_strand_minus ? '-' : '+') << ")"
        << " x ("
        << r2.first << ", "
        << r2.second
        << ", " << (s2 == eNa_strand_minus ? '-' : '+') << ")"
        << ": is_consistent = "
        << (is_consistent ? "true" : "false")
        << endl;
#endif

    return is_consistent;
}


TSeqPos Difference(const pair<TSeqRange, TSeqRange>& r1,
                   const pair<TSeqRange, TSeqRange>& r2,
                   ENa_strand s1, ENa_strand s2)
{
    TSeqPos diff = 0;

    if (r1.first.GetTo() < r2.first.GetFrom()) {
        diff += r2.first.GetFrom() - r1.first.GetTo();
    }
    else if (r2.first.GetTo() < r1.first.GetFrom()) {
        diff += r1.first.GetFrom() - r2.first.GetTo();
    }

    if (r1.second.GetTo() < r2.second.GetFrom()) {
        diff += r2.second.GetFrom() - r1.second.GetTo();
    }
    else if (r2.second.GetTo() < r1.second.GetFrom()) {
        diff += r1.second.GetFrom() - r2.second.GetTo();
    }

    /**
    if (s1 == eNa_strand_minus) {
        if (r2.first.GetTo() < r1.first.GetFrom()) {
            diff += r1.first.GetFrom() - r2.first.GetTo();
        }
    } else {
        if (r1.first.GetTo() < r2.first.GetFrom()) {
            diff += r2.first.GetFrom() - r1.first.GetTo();
        }
    }
    if (s2 == eNa_strand_minus) {
        if (r2.second.GetTo() < r1.second.GetFrom()) {
            diff += r1.second.GetFrom() - r2.second.GetTo();
        }
    } else {
        if (r1.second.GetTo() < r2.second.GetFrom()) {
            diff += r2.second.GetFrom() - r1.second.GetTo();
        }
    }
    **/

#ifdef _VERBOSE_DEBUG
    cerr << "("
        << r1.first << ", "
        << r1.second << ")"
        << " x ("
        << r2.first << ", "
        << r2.second << ")"
        << ": diff = " << diff
        << endl;
#endif

    return diff;
}


typedef pair<TSeqRange, TSeqRange> TRange;
typedef pair<TRange, CRef<CSeq_align> > TAlignRange;

struct SRangesByStart
{
    bool operator() (const TAlignRange& r1,
                     const TAlignRange& r2) const
    {
        // First the simple sorts, by query start, 
        //  and subject start
        if(r1.first.first != r2.first.first) {
            return (r1.first.first < r2.first.first);
        }
        if(r1.first.second != r2.first.second) {
            return (r1.first.second < r2.first.second);
        }
        
        // after these what else to sort by? The goal is simply to stable, 
        //  and never sort by alignment pointer.
        // Return false ought to always be maintain original order, 
        //  so as long as the input is stable, this sort won't break it.
        return false;        
    }
};
typedef multiset<TAlignRange, SRangesByStart> TAlignRangeMultiSet;

struct SRangesBySize
{
    bool operator() (const TAlignRange& r1,
                     const TAlignRange& r2) const
    {
        TSeqPos len1 = max(r1.first.first.GetLength(),
                           r1.first.second.GetLength());
        TSeqPos len2 = max(r2.first.first.GetLength(),
                           r2.first.second.GetLength());
        if (len1 > len2) {
            return true;
        }
        if (len2 > len1) {
            return false;
        }

        TSeqRange r1_0 = r1.second->GetSeqRange(0);
        TSeqRange r2_0 = r2.second->GetSeqRange(0);
        if (r1_0 < r2_0) {
            return true;
        }
        if (r2_0 < r1_0) {
            return false;
        }

        return r1.second->GetSeqRange(1) < r2.second->GetSeqRange(1);
    }
};

struct SRangesByScore
{
    bool operator() (const TAlignRange& r1,
                     const TAlignRange& r2) const
    {
		int scores[2] = {0, 0};
		r1.second->GetNamedScore(CSeq_align::eScore_Score, scores[0]);
		r2.second->GetNamedScore(CSeq_align::eScore_Score, scores[1]);

        if (scores[0] > scores[1]) {
            return true;
        }
        if (scores[1] > scores[0]) {
            return false;
        }

        TSeqRange r1_0 = r1.second->GetSeqRange(0);
        TSeqRange r2_0 = r2.second->GetSeqRange(0);
        if (r1_0 < r2_0) {
            return true;
        }
        if (r2_0 < r1_0) {
            return false;
        }

        return r1.second->GetSeqRange(1) < r2.second->GetSeqRange(1);
    }
};

struct SRangesByPctIdent
{
    bool operator() (const TAlignRange& r1,
                     const TAlignRange& r2) const
    {
    	double scores[2] = {0.0, 0.0};
		r1.second->GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, scores[0]);
		r2.second->GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, scores[1]);
        if(scores[0] == scores[1]) {
		    TSeqPos len1 = max(r1.first.first.GetLength(),
        	                   r1.first.second.GetLength());
       		TSeqPos len2 = max(r2.first.first.GetLength(),
        	                   r2.first.second.GetLength());
        	return len1 > len2;
   		}

        if (scores[0] > scores[1]) {
            return true;
        }
        if (scores[1] > scores[0]) {
            return false;
        }

        TSeqRange r1_0 = r1.second->GetSeqRange(0);
        TSeqRange r2_0 = r2.second->GetSeqRange(0);
        if (r1_0 < r2_0) {
            return true;
        }
        if (r2_0 < r1_0) {
            return false;
        }

        return r1.second->GetSeqRange(1) < r2.second->GetSeqRange(1);
	}
};

struct SSeqAlignsBySize
{
    bool operator()(const CRef<CSeq_align>& al_ref1,
                    const CRef<CSeq_align>& al_ref2) const
    {
        TSeqPos al1_len = al_ref1->GetAlignLength();
        TSeqPos al2_len = al_ref2->GetAlignLength();
        if (al1_len > al2_len) {
            return true;
        }
        if (al2_len > al1_len) {
            return false;
        }

        TSeqRange r1_0 = al_ref1->GetSeqRange(0);
        TSeqRange r2_0 = al_ref2->GetSeqRange(0);
        if (r1_0 < r2_0) {
            return true;
        }
        if (r2_0 < r1_0) {
            return false;
        }

        return al_ref1->GetSeqRange(1) < al_ref2->GetSeqRange(1);
    };
};

struct SSeqAlignsByScore
{
    bool operator()(const CRef<CSeq_align>& al_ref1,
                    const CRef<CSeq_align>& al_ref2) const
    {
		int scores[2] = {0, 0};
		al_ref1->GetNamedScore(CSeq_align::eScore_Score, scores[0]);
		al_ref2->GetNamedScore(CSeq_align::eScore_Score, scores[1]);
        return scores[0] > scores[1];
    };
};

struct SSeqAlignsByPctIdent
{
    bool operator()(const CRef<CSeq_align>& al_ref1,
                    const CRef<CSeq_align>& al_ref2) const
    {
		double scores[2] = {0.0, 0.0};
		al_ref1->GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, scores[0]);
		al_ref2->GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, scores[1]);
        if(scores[0] == scores[1]) {
        	return al_ref1->GetAlignLength() > al_ref2->GetAlignLength();
		}
		return scores[0] > scores[1];
    };
};

struct SRangeIteratorsByAddress
{
    bool operator()(TAlignRangeMultiSet::const_iterator it1,
                    TAlignRangeMultiSet::const_iterator it2) const
    {
		return &*it1 < &*it2;
    };
};


struct SCompartScore {
    TSeqPos total_size;
    TRange total_range;

    SCompartScore() : total_size(0) {}

    bool operator<(const SCompartScore &o) const
    {
        if (total_size > o.total_size) {
            return true;
        }
        if (total_size < o.total_size) {
            return false;
        }
        return total_range < o.total_range;
    }

    void Reset()
    {
        total_size = 0;
        total_range = TRange();
    }
};


void FindCompartments(const list< CRef<CSeq_align> >& aligns,
                      list< CRef<CSeq_align_set> >& align_sets,
                      TCompartOptions options,
                      float diff_len_filter)
{
    //
    // sort by sequence pair + strand
    //
    typedef pair<CSeq_id_Handle, ENa_strand> TIdStrand;
    typedef pair<TIdStrand, TIdStrand> TIdPair;
    typedef map<TIdPair, vector< CRef<CSeq_align> > > TAlignments;
    TAlignments alignments;
    ITERATE (list< CRef<CSeq_align> >, it, aligns) {
        /**
        CRef<CSeq_align> align = *it;
        if (align->GetSegs().IsDenseg()) {
            if (align->GetSeqStrand(0) == eNa_strand_minus) {
                 cerr << "  before flip: ("
                     << align->GetSeqRange(0) << ", "
                     << (align->GetSeqStrand(0) == eNa_strand_minus ? '-' : '+')
                     << ") - ("
                     << align->GetSeqRange(1) << ", "
                     << (align->GetSeqStrand(1) == eNa_strand_minus ? '-' : '+')
                     << ")"
                     << endl;

                align->SetSegs().SetDenseg().Reverse();

                 cerr << "  after flip: ("
                     << align->GetSeqRange(0) << ", "
                     << (align->GetSeqStrand(0) == eNa_strand_minus ? '-' : '+')
                     << ") - ("
                     << align->GetSeqRange(1) << ", "
                     << (align->GetSeqStrand(1) == eNa_strand_minus ? '-' : '+')
                     << ")"
                     << endl;

            }
        }
        **/


        CSeq_id_Handle qid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(0));
        ENa_strand q_strand = (*it)->GetSeqStrand(0);
        CSeq_id_Handle sid = CSeq_id_Handle::GetHandle((*it)->GetSeq_id(1));
        ENa_strand s_strand = (*it)->GetSeqStrand(1);

        // strand normalization
        if (q_strand != s_strand  &&  q_strand == eNa_strand_minus) {
            std::swap(q_strand, s_strand);
        }
        else if (q_strand == eNa_strand_minus) {
            q_strand = eNa_strand_plus;
            s_strand = eNa_strand_plus;
        }


        TIdPair p(TIdStrand(qid, q_strand), TIdStrand(sid, s_strand));
        alignments[p].push_back(*it);
    }

    typedef pair<SCompartScore, CRef<CSeq_align_set> > TCompartScore;
    vector<TCompartScore> scored_compartments;


    // we only compartmentalize within each sequence id / strand pair
    NON_CONST_ITERATE (TAlignments, align_it, alignments) {
        const TIdPair& id_pair = align_it->first;
        ENa_strand q_strand = id_pair.first.second;
        ENa_strand s_strand = id_pair.second.second;

        vector< CRef<CSeq_align> >& aligns = align_it->second;
		if(options & fCompart_SortByScore) {
			std::stable_sort(aligns.begin(), aligns.end(), SSeqAlignsByScore());
        }
        else if(options & fCompart_SortByPctIdent) {
			std::sort(aligns.begin(), aligns.end(), SSeqAlignsByPctIdent());
        }
        else {
			std::sort(aligns.begin(), aligns.end(), SSeqAlignsBySize());
        }

#ifdef DEBUG_VERBOSE_OUTPUT
        {{
             cerr << "ids: " << id_pair.first.first << " x "
                 << id_pair.second.first << endl;
             if(options & fCompart_SortByScore) {
                 cerr << "  sort by score" << endl;
             }
             else if(options & fCompart_SortByPctIdent) {
                 cerr << "  sort by percent identity" << endl;
             }
             else {
                 cerr << "  sort by size" << endl;
             }

             ITERATE (vector< CRef<CSeq_align> >, it, aligns) {
                 cerr << "  ("
                     << (*it)->GetSeqRange(0) << ", "
                     << ((*it)->GetSeqStrand(0) == eNa_strand_minus ? '-' : '+')
                     << ") - ("
                     << (*it)->GetSeqRange(1) << ", "
                     << ((*it)->GetSeqStrand(1) == eNa_strand_minus ? '-' : '+')
                     << ")"
                     << endl;
             }
         }}
#endif

        //
        // reduce the list to a set of overall ranges
        //

        vector<TAlignRange> align_ranges;

        ITERATE (vector< CRef<CSeq_align> >, iter, aligns) {
            TSeqRange q_range = (*iter)->GetSeqRange(0);
            TSeqRange s_range = (*iter)->GetSeqRange(1);
            TRange r(q_range, s_range);
            align_ranges.push_back(TAlignRange(r, *iter));
        }

#ifdef _VERBOSE_DEBUG
        {{
             cerr << "ranges: "
                 << id_pair.first.first << "/" << q_strand
                 << " x "
                 << id_pair.second.first << "/" << s_strand
                 << ":"<< endl;
             vector<TAlignRange>::const_iterator prev_it = align_ranges.end();
             ITERATE (vector<TAlignRange>, it, align_ranges) {
                 cerr << "  ("
                     << it->first.first << ", "
                     << it->first.second << ")"
                     << " [" << it->first.first.GetLength()
                     << ", " << it->first.second.GetLength() << "]";
                 if (prev_it != align_ranges.end()) {
                     cerr << "  consistent="
                         << (IsConsistent(prev_it->first, it->first,
                                          q_strand, s_strand) ? "true" : "false");
                     cerr << "  diff="
                         << Difference(prev_it->first, it->first,
                                       q_strand, s_strand);
                 }

                 cerr << endl;
                 prev_it = it;
             }
         }}
#endif

        //
        // sort by descending hit size
        // fit each new hit into its best compartment compartment
        //
		if (options & fCompart_SortByScore) {
			std::sort(align_ranges.begin(), align_ranges.end(),
                      SRangesByScore());
        }
        else if (options & fCompart_SortByPctIdent) {
			std::sort(align_ranges.begin(), align_ranges.end(),
                      SRangesByPctIdent());
        }
        else {
			std::sort(align_ranges.begin(), align_ranges.end(),
                      SRangesBySize());
        }

        list< TAlignRangeMultiSet > compartments;

        // iteration through this list now gives us a natural assortment by
        // largest alignment.  we iterate through this list and inspect the
        // nascent compartments.  we must evaluate for possible fit within each
        // compartment
        NON_CONST_ITERATE (vector<TAlignRange>, it, align_ranges) {

            bool found = false;
            list<TAlignRangeMultiSet>::iterator best_compart =
                compartments.end();
            TSeqPos best_diff = kMax_Int;
            size_t comp_id = 0;
            NON_CONST_ITERATE (list<TAlignRangeMultiSet>,
                               compart_it, compartments) {
                ++comp_id;
                TAlignRangeMultiSet::iterator place =
                    compart_it->lower_bound(*it);

                TSeqPos diff = 0;
                bool is_consistent = false;
                bool is_intersecting_query = false;
                bool is_intersecting_subject = false;
                if (place == compart_it->end()) {
                    // best place is the end; we therefore evaluate whether we
                    // can be appended to this compartment
                    --place;
                    is_intersecting_query =
                        IsIntersectingQuery(place->first, it->first);
                    is_intersecting_subject =
                        IsIntersectingSubject(place->first, it->first);
                    is_consistent = IsConsistent(place->first,
                                                 it->first,
                                                 q_strand, s_strand);
                    diff = Difference(place->first,
                                      it->first,
                                      q_strand, s_strand);
                }
                else {
                    if (place == compart_it->begin()) {
                        // best place is the beginning; we therefore evaluate
                        // whether we can be prepended to this compartment
                        is_intersecting_query =
                            IsIntersectingQuery(it->first, place->first);
                        is_intersecting_subject =
                            IsIntersectingSubject(it->first, place->first);
                        is_consistent = IsConsistent(it->first,
                                                     place->first,
                                                     q_strand, s_strand);
                        diff = Difference(it->first,
                                          place->first,
                                          q_strand, s_strand);
                    }
                    else {
                        // best place is in the middle; we must evaluate two
                        // positions
                        is_intersecting_query =
                            IsIntersectingQuery(it->first, place->first);
                        is_intersecting_subject =
                            IsIntersectingSubject(it->first, place->first);
                        is_consistent =
                            IsConsistent(it->first,
                                         place->first,
                                         q_strand, s_strand);
                        diff = Difference(it->first,
                                          place->first,
                                          q_strand, s_strand);

                        --place;
                        is_intersecting_query |=
                            IsIntersectingQuery(place->first, it->first);
                        is_intersecting_subject |=
                            IsIntersectingSubject(place->first, it->first);
                        is_consistent &=
                            IsConsistent(place->first,
                                         it->first,
                                         q_strand, s_strand);
                        diff = min(diff,
                                   Difference(place->first,
                                              it->first,
                                              q_strand, s_strand));
                    }
                }

#ifdef _VERBOSE_DEBUG
                float diff_len_ratio = double(diff) / it->second->GetAlignLength(false);
                cerr << "  comp_id=" << comp_id
                    << "  is_consistent=" << (is_consistent ? "true" : "false")
                    << "  is_intersecting_query=" << (is_intersecting_query ? "true" : "false")
                    << "  is_intersecting_subject=" << (is_intersecting_subject ? "true" : "false")
                    << "  allow intersect_query="
                    << ((options & fCompart_AllowIntersectionsQuery) ? "true" : "false")
                    << "  allow intersect_subject="
                    << ((options & fCompart_AllowIntersectionsSubject) ? "true" : "false")
                    << "  allow intersect_both="
                    << ((options & fCompart_AllowIntersectionsBoth) ? "true" : "false")
                    << "  diff=" << diff
                    << "  best_diff=" << best_diff
                    << "  align_len=" << it->second->GetAlignLength(false)
                    << "  diff_len_ratio=" << diff_len_ratio
                    << "  filter=" << (diff_len_ratio <= diff_len_filter ? "pass" : "fail" )
                    << endl;
#endif

                if ( ((is_consistent  &&  !is_intersecting_query && !is_intersecting_subject)  ||
                      (((options & fCompart_AllowInconsistentIntersection) || is_consistent) &&
                      (((options & fCompart_AllowIntersectionsQuery)  &&
                        is_intersecting_query ) ||
                      ( (options & fCompart_AllowIntersectionsSubject)  &&
                        is_intersecting_subject ) ||
                      ( (options & fCompart_AllowIntersectionsBoth)  &&
                        is_intersecting_query && is_intersecting_subject ))))  &&
                    diff < best_diff) {
                    best_compart = compart_it;
                    best_diff = diff;
                    found = true;
                }
            }

            if ( !found  ||  best_compart == compartments.end() ) {
                compartments.push_back(TAlignRangeMultiSet());
                compartments.back().insert(*it);
            }
            else {
                best_compart->insert(*it);
			}

#ifdef _VERBOSE_DEBUG
            {{
                 cerr << "compartments: found " << compartments.size() << endl;
                 size_t count = 0;
                 ITERATE (list<TAlignRangeMultiSet>, it, compartments) {
                     ++count;
                     cerr << "  compartment " << count << endl;
                     ITERATE (TAlignRangeMultiSet, i, *it) {
                         cerr << "    ("
                             << i->first.first << ", "
                             << i->first.second << ")"
                             << " [" << i->first.first.GetLength()
                             << ", " << i->first.second.GetLength() << "]"
                             << " (0x" << std::hex << ((intptr_t)i->second.GetPointer()) << std::dec << ")"
                             << endl;
                     }
                 }
             }}
#endif
        }

#ifdef DEBUG_VERBOSE_OUTPUT
        {{
             cerr << "found " << compartments.size() << endl;
             size_t count = 0;
             ITERATE (list<TAlignRangeMultiSet>, it, compartments) {
                 ++count;
                 cerr << "  compartment " << count << endl;
                 ITERATE (TAlignRangeMultiSet, i, *it) {
                     cerr << "    ("
                         << i->first.first << ", "
                         << i->first.second << ")"
                         << " [" << i->first.first.GetLength()
                         << ", " << i->first.second.GetLength() << "]"
                         << endl;
                 }
             }
         }}
#endif

        // pack into seq-align-sets
        size_t compart_count = 0;
        ITERATE (list<TAlignRangeMultiSet>, it, compartments) {
            ++compart_count;
            CRef<CSeq_align_set> sas(new CSeq_align_set);
            SCompartScore score;
            set<TAlignRangeMultiSet::const_iterator, SRangeIteratorsByAddress> break_positions;
            if ((options & fCompart_FilterByDiffLen) && it->size() > 1) {
                /// Find large gaps that require breaking the compartments.
                /// Go over the compartment forward and then backwards, and mark
                /// positions at which the gap/alignment-lengt ratio is over the
                /// threahols; break positions are those that we fing in both
                /// passes.  We need to make the check here, not
                /// when forming the compartments, because you can have two
                /// alignments that are far away later joined through other
                /// alignments in the middle
                TAlignRangeMultiSet::const_iterator second_subject_range
                    = it->begin();
                for (; second_subject_range != it->end()
                    && second_subject_range->first.second
                        == it->begin()->first.second; ++second_subject_range);
                bool reverse_subject = second_subject_range != it->end() &&
                                       second_subject_range->first.second
                                           < it->begin()->first.second;
                TSeqPos subject_end = (reverse_subject
                                    ? it->begin()->first
                                    : it->rbegin()->first) . second.GetTo();
                set<TAlignRangeMultiSet::const_iterator, SRangeIteratorsByAddress>
                    forward_breaks, backward_breaks;
                TSignedSeqPos current_end_point = 0;
                TSignedSeqPos current_potential_break = 0;
                TSeqPos count = 1;
                TSeqPos last_break = 0;
                ITERATE (TAlignRangeMultiSet, i, *it) {
#ifdef _VERBOSE_DEBUG
                     cerr << "    ("
                         << i->first.first << ", "
                         << i->first.second << ")"
                         << " [" << i->first.first.GetLength()
                         << ", " << i->first.second.GetLength() << "]"
                         << endl;
#endif
                    TSignedSeqPos start_point = i->first.first.GetFrom() +
                      (reverse_subject ? subject_end - i->first.second.GetTo()
                                       : i->first.second.GetFrom());
                    TSignedSeqPos end_point = i->first.first.GetTo() +
                      (reverse_subject ? subject_end - i->first.second.GetFrom()
                                       : i->first.second.GetTo());
#ifdef _VERBOSE_DEBUG
                    if (i != it->begin()) {
                        cerr << "At " << ++count << " Gap " << int(start_point - current_end_point) << " on length " << (current_potential_break - current_end_point) << endl;
                    }
#endif
                    if (i != it->begin() &&
                        start_point > current_potential_break)
                    {
                        /// gap since last alignment is over threshold; mark
                        /// this is a breakpoint
                        forward_breaks.insert(i);
#ifdef _VERBOSE_DEBUG
			cerr << "Break! " << (count - last_break) << " members start_point " << start_point << " current break " << current_potential_break << endl;
			last_break = count;
#endif
                    }
                    current_end_point = max(current_potential_break, end_point);
                    current_potential_break = current_end_point +
                           diff_len_filter * i->second->GetAlignLength(false);
                }
                current_potential_break = current_end_point = INT_MAX;
#ifdef _VERBOSE_DEBUG
                count = 1;
                last_break = 0;
#endif
                TSeqPos last_double_break = 0;
                REVERSE_ITERATE (TAlignRangeMultiSet, i, *it) {
#ifdef _VERBOSE_DEBUG
                     cerr << "    ("
                         << i->first.first << ", "
                         << i->first.second << ")"
                         << " [" << i->first.first.GetLength()
                         << ", " << i->first.second.GetLength() << "]"
                         << endl;
#endif
                    TSignedSeqPos start_point = i->first.first.GetFrom() +
                      (reverse_subject ? subject_end - i->first.second.GetTo()
                                       : i->first.second.GetFrom());
                    TSignedSeqPos end_point = i->first.first.GetTo() +
                      (reverse_subject ? subject_end - i->first.second.GetFrom()
                                       : i->first.second.GetTo());
#ifdef _VERBOSE_DEBUG
                    if (i != it->rbegin()) {
                        cerr << "At " << ++count << " Gap " << int(current_end_point - end_point) << " on length " << (current_end_point - current_potential_break) << endl;
                    }
#endif
                    if (i != it->rbegin() &&
                        end_point < current_potential_break)
                    {
                        /// gap since last alignment is over threshold; mark
                        /// this is a breakpoint
                        TAlignRangeMultiSet::const_iterator breakpoint = i.base();
                        backward_breaks.insert(breakpoint);
#ifdef _VERBOSE_DEBUG
                        if (forward_breaks.count(breakpoint)) {
                            cerr << "Double after " << (count - last_double_break) << ' ';
                            last_double_break = count;
                        }
			cerr << "Break! " << (count - last_break) << " members end_point " << end_point << " current break " << current_potential_break << endl;
			last_break = count;
#endif
                    }
                    current_end_point = min(current_potential_break, start_point);
                    current_potential_break = current_end_point -
                           diff_len_filter * i->second->GetAlignLength(false);
                }
                set_intersection(forward_breaks.begin(), forward_breaks.end(),
                             backward_breaks.begin(), backward_breaks.end(),
                             inserter(break_positions, break_positions.end()),
                             SRangeIteratorsByAddress());
             }
             ITERATE (TAlignRangeMultiSet, i, *it) {
                if (break_positions.count(i)) {
#ifdef DEBUG_VERBOSE_OUTPUT
                        cerr << "compartment " << compart_count << " break at " << i->first.first.GetFrom() << ".." << i->first.first.GetTo() << endl;
#endif
                    TCompartScore sc(score, sas);
                    scored_compartments.push_back(sc);
                    score.Reset();
                    sas.Reset(new CSeq_align_set);
                }
                sas->Set().push_back(i->second);
                score.total_size += i->second->GetAlignLength();
                score.total_range.first += i->first.first;
                score.total_range.second += i->first.second;
            }

            TCompartScore sc(score, sas);
            scored_compartments.push_back(sc);
        }
    }

    //
    // sort our compartments by size descending
    //
    std::sort(scored_compartments.begin(), scored_compartments.end());
    ITERATE (vector<TCompartScore>, it, scored_compartments) {
        align_sets.push_back(it->second);
    }
}


void JoinCompartment(const CRef<CSeq_align_set>& compartment,
                     float gap_ratio,
                     list< CRef<CSeq_align> >& aligns)
{
    CRef<CSeq_align_set> disc_align_set;
    typedef const list <CRef<CSeq_align> > TConstSeqAlignList;
    TConstSeqAlignList& alignments = compartment->Get();
    TSeqPos len = 0;
    ITERATE(TConstSeqAlignList, al_it, alignments) {
        len += (*al_it)->GetAlignLength(false);
    }
    TSeqPos max_gap_len = len * gap_ratio;
    ITERATE(TConstSeqAlignList, al_it, alignments) {
        TConstSeqAlignList::const_iterator al_it_next = al_it;
        al_it_next++;
        if (!disc_align_set) disc_align_set.Reset(new CSeq_align_set);
        disc_align_set->Set().push_back(*al_it);
        if (al_it_next == alignments.end() ||
            (*al_it)->GetSeqStop(0) + max_gap_len < (*al_it_next)->GetSeqStart(0) ||
            (*al_it)->GetSeqStop(1) + max_gap_len < (*al_it_next)->GetSeqStart(1))
        {
            // Pack and ship
            CRef<CSeq_align> comp_align(new CSeq_align);
            comp_align->SetType(CSeq_align::eType_disc);
            comp_align->SetSegs().SetDisc(*disc_align_set);
            aligns.push_back(comp_align);
            disc_align_set.Reset(0);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

