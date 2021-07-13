static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: hitlist.cpp

Author: Jason Papadopoulos

Contents: Implementation of CHitList class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/hitlist.hpp>

#include <algorithm>


/// @file hitlist.cpp
/// Implementation of CHitList class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Callback used to sort hits into canonical order
struct compare_hit_redundant {


    /// Sort by seq1 index, then seq2 index, then
    /// seq1 range, then seq2 range, then score
    /// @param aa First hit [in]
    /// @param bb Second hit [in]
    /// @param true if aa and bb are in the correct order
    ///
    bool operator()(CHitList::TListEntry const& aa, 
                    CHitList::TListEntry const& bb) const {
        CHit *a = aa.second;
        CHit *b = bb.second;
 
        if (a->m_SeqIndex1 < b->m_SeqIndex1)
            return true;
        if (a->m_SeqIndex1 > b->m_SeqIndex1)
            return false;
 
        if (a->m_SeqIndex2 < b->m_SeqIndex2)
            return true;
        if (a->m_SeqIndex2 > b->m_SeqIndex2)
            return false;
 
        if (a->m_SeqRange1.GetFrom() < b->m_SeqRange1.GetFrom())
            return true;
        if (a->m_SeqRange1.GetFrom() > b->m_SeqRange1.GetFrom())
            return false;
 
        if (a->m_SeqRange1.GetTo() < b->m_SeqRange1.GetTo())
            return true;
        if (a->m_SeqRange1.GetTo() > b->m_SeqRange1.GetTo())
            return false;
 
        if (a->m_SeqRange2.GetFrom() < b->m_SeqRange2.GetFrom())
            return true;
        if (a->m_SeqRange2.GetFrom() > b->m_SeqRange2.GetFrom())
            return false;
 
        if (a->m_SeqRange2.GetTo() < b->m_SeqRange2.GetTo())
            return true;
        if (a->m_SeqRange2.GetTo() > b->m_SeqRange2.GetTo())
            return false;
 
        return (a->m_Score < b->m_Score);
    }
};

void
CHitList::MakeCanonical()
{
    if (Empty())
        return;

    // change all the hits so that the first sequence index
    // is the one that is the smallest

    for (int i = 0; i < Size(); i++) {
        CHit *hit = GetHit(i);
        if (hit->m_SeqIndex1 < hit->m_SeqIndex2)
            continue;

        _ASSERT(hit->m_SeqIndex1 != hit->m_SeqIndex2);
        swap(hit->m_SeqIndex1, hit->m_SeqIndex2);
        swap(hit->m_SeqRange1, hit->m_SeqRange2);
        hit->GetEditScript().ReverseEditScript();
        if (hit->HasSubHits()) {
            NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
                CHit *subhit = *subitr;
                swap(subhit->m_SeqIndex1, subhit->m_SeqIndex2);
                swap(subhit->m_SeqRange1, subhit->m_SeqRange2);
                subhit->GetEditScript().ReverseEditScript();
            }
        }
    }

    sort(m_List.begin(), m_List.end(), compare_hit_redundant());

    // delete duplicate hits

    int i = 0;
    int num_hits = Size();
    while(i < num_hits) {
        int j;
        for (j = i + 1; j < num_hits; j++) {
            if (GetHit(j)->m_SeqIndex1 != GetHit(i)->m_SeqIndex1  ||
                GetHit(j)->m_SeqIndex2 != GetHit(i)->m_SeqIndex2  ||
                GetHit(j)->m_SeqRange1 != GetHit(i)->m_SeqRange1  ||
                GetHit(j)->m_SeqRange2 != GetHit(i)->m_SeqRange2  ||
                GetHit(j)->m_Score != GetHit(i)->m_Score  ||
                GetHit(j)->HasSubHits() || GetHit(i)->HasSubHits()) {
                break;
            }
            SetKeepHit(j, false);
        }
        i = j;
    }

    // remove the duplicates

    PurgeUnwantedHits();
}


/// Given two hits, representing alignments of different 
/// sequences A and B to the same common sequence, produce 
/// another hit that represents the alignment directly between 
/// A and B.
/// @param hit1 The first hit [in]
/// @param hit2 The second hit [in]
/// @return The hit containing the alignment between A and B
///
static CHit *
x_MatchSubHits(CHit *hit1, CHit *hit2)
{

    // calculate the range on the common sequence where
    // the A and B sequences overlap.

    TRange& srange1 = hit1->m_SeqRange2;
    TRange& srange2 = hit2->m_SeqRange2;
    TRange s_range(srange1.IntersectionWith(srange2));

    // ignore an overlap that is too small

    if (s_range.GetLength() <= CHit::kMinHitSize)
        return 0;

    // Now calculate the A and B ranges associated with s_range

    TRange q_range1, q_range2;
    TRange tmp_s_range;
    TRange tmp_tback_range;

    hit1->GetRangeFromSeq2(s_range, q_range1, tmp_s_range, tmp_tback_range);
    hit2->GetRangeFromSeq2(s_range, q_range2, tmp_s_range, tmp_tback_range);

    // Throw away the alignment if the A and B ranges are too
    // small, or if the difference in A and B lengths is too large

    if (q_range1.GetLength() <= CHit::kMinHitSize || 
        q_range2.GetLength() <= CHit::kMinHitSize)
        return 0;

    // join the two alignment halves together into a
    // single alignment. Don't bother recalculating the
    // score; if there was any overlap at all the scores
    // should be similar. Also ignore the traceback

    return new CHit(hit1->m_SeqIndex1, hit2->m_SeqIndex1,
                    q_range1, q_range2,
                    min(hit1->m_Score, hit2->m_Score),
                    CEditScript());
}


/// Callback to sort a list of hits in order of increasing
/// sequence 2 index
struct compare_hit_seq2_idx {

    /// Sort two hits by increasing sequence 2 index
    /// @param a The first hit
    /// @param b The second hit
    /// @return true if 'a' and 'b' are already correctly sorted
    ///
    bool operator()(CHitList::TListEntry const& a, 
                    CHitList::TListEntry const& b) const {
        return (a.second->m_SeqIndex2 < b.second->m_SeqIndex2);
    }
};

void
CHitList::MatchOverlappingSubHits(CHitList& matched_list)
{
    int num_hits = Size();

    // At least two hits are needed to generate any matches
    if (num_hits < 2)
        return;

    sort(m_List.begin(), m_List.end(), compare_hit_seq2_idx());

    for (int i = 0; i < num_hits - 1; i++) {

        // for each hit

        CHit *hit1 = GetHit(i);
        _ASSERT(hit1->HasSubHits());

        // for each succeeding hit

        for (int j = i + 1; j < num_hits; j++) {

            CHit *hit2 = GetHit(j);
            _ASSERT(hit2->HasSubHits());

            // if hit1 and hit2 do not align to the
            // same common sequence, later hits will not
            // align either

            if (hit1->m_SeqIndex2 != hit2->m_SeqIndex2)
                break;

            // sequence 1 must be different for the two hits

            if (hit1->m_SeqIndex1 == hit2->m_SeqIndex1)
                continue;

            CHit *new_hit = 0;

            // for each subhit of hit1

            NON_CONST_ITERATE(CHit::TSubHit, itr1, hit1->GetSubHit()) {

                // for each subhit of hit 2

                NON_CONST_ITERATE(CHit::TSubHit, itr2, hit2->GetSubHit()) {

                    // create an alignment between the two subhits if
                    // their sequence 2 ranges overlap

                    CHit *new_subhit = x_MatchSubHits(*itr1, *itr2);
                    if (new_subhit != 0) {
                        if (new_hit == 0) {
                            // create the first subhit of a new hit

                            new_hit = new CHit(hit1->m_SeqIndex1, 
                                               hit2->m_SeqIndex1);
                            new_hit->InsertSubHit(new_subhit);
                            continue;
                        }
                        CHit::TSubHit& subhits = new_hit->GetSubHit();
                        if (new_subhit->m_SeqRange1.GetFrom() -
                              subhits.back()->m_SeqRange1.GetTo() !=
                            new_subhit->m_SeqRange2.GetFrom() -
                              subhits.back()->m_SeqRange2.GetTo()) {

                            // the current generated subhit does not
                            // start the same distance away from the
                            // previous subhit on bother sequences.
                            // Merge the new subhit into the previous one
#if 0
                            //---------------------------------------
                            printf("collapse query %d %d-%d / %d-%d "
                                   "query %d %d-%d %d-%d\n",
                                   hit1->m_SeqIndex1,
                                   subhits.back()->m_SeqRange1.GetFrom(),
                                   subhits.back()->m_SeqRange1.GetTo(),
                                   new_subhit->m_SeqRange1.GetFrom(),
                                   new_subhit->m_SeqRange1.GetTo(),
                                   hit2->m_SeqIndex1,
                                   subhits.back()->m_SeqRange2.GetFrom(),
                                   subhits.back()->m_SeqRange2.GetTo(),
                                   new_subhit->m_SeqRange2.GetFrom(),
                                   new_subhit->m_SeqRange2.GetTo());
                            //---------------------------------------
#endif
                            subhits.back()->m_SeqRange1.SetTo(
                                          new_subhit->m_SeqRange1.GetTo());
                            subhits.back()->m_SeqRange2.SetTo(
                                          new_subhit->m_SeqRange2.GetTo());
                            subhits.back()->m_Score += new_subhit->m_Score;
                            delete new_subhit;
                        }
                        else {
                            // just add the new subhit

                            new_hit->InsertSubHit(new_subhit);
                        }
                    }
                }
            }

            // if any subhits were generated, add the resulting
            // hit to the output list
            if (new_hit != 0) {
                new_hit->AddUpSubHits();
                matched_list.AddToHitList(new_hit);
            }
        }
    }
}


/// callback used to sort hits in order of decreasing score
class compare_hit_score {
public:
    /// Compare hits by score
    /// @param a First hit
    /// @param b Second hit
    /// @return true if hits are already in order of
    ///         descending score
    ///
    bool operator()(CHitList::TListEntry const& a, 
                    CHitList::TListEntry const& b) const {
        return (a.second->m_Score > b.second->m_Score);
    }
};

void
CHitList::SortByScore()
{
    sort(m_List.begin(), m_List.end(), compare_hit_score());
}

/// callback used to sort hits in order of increasing 
/// sequence offset
struct compare_hit_start {

    /// Compare hits by sequence offset
    /// @param aa First hit
    /// @param bb Second hit
    /// @return true if hits are already in order of
    ///         increasing offset on sequence 1, with the
    ///         offset on sequence 2 used as a tiebreaker
    ///
    bool operator()(CHitList::TListEntry const& aa, 
                    CHitList::TListEntry const& bb) const {
        CHit *a = aa.second;
        CHit *b = bb.second;
        if (a->m_SeqRange1.GetFrom() < b->m_SeqRange1.GetFrom())
            return true;
        if (a->m_SeqRange1.GetFrom() > b->m_SeqRange1.GetFrom())
            return false;
 
        return (a->m_SeqRange2.GetFrom() < b->m_SeqRange2.GetFrom());
    }
};

void
CHitList::SortByStartOffset()
{
    sort(m_List.begin(), m_List.end(), compare_hit_start());
}

void
CHitList::Append(CHitList& hitlist)
{
    for (int i = 0; i < hitlist.Size(); i++) {
        AddToHitList(hitlist.GetHit(i));
        m_List.back().second = m_List.back().second->Clone();
    }
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
