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

File name: hit.cpp

Author: Jason Papadopoulos

Contents: implementation of CHit class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/hit.hpp>

#include <algorithm>

/// @file hit.cpp
/// Implementation of CHit class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)
USING_SCOPE(objects);

CHit::CHit(int seq1_index, int seq2_index, int score, 
           const CDense_seg& denseg)
    : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index),
      m_Score(score), m_EditScript(denseg)
{
    _ASSERT(denseg.GetDim() == 2);

    CDense_seg::TNumseg num_seg = denseg.GetNumseg();
    const CDense_seg::TStarts& starts = denseg.GetStarts();
    const CDense_seg::TLens& lens = denseg.GetLens();
    int len1 = 0, len2 = 0;
                                
    for (CDense_seg::TNumseg i = 0; i < num_seg; i++) {
        if (starts[2*i] >= 0)
            len1 += lens[i];
        if (starts[2*i+1] >= 0)
            len2 += lens[i];
    }   
    m_SeqRange1 = TRange(starts[0], starts[0] + len1 - 1);
    m_SeqRange2 = TRange(starts[1], starts[1] + len2 - 1);
    VerifyHit();
}   


void CHit::AddUpSubHits()
{
    _ASSERT(HasSubHits());

    // Make the ranges of the hit into the bounding box
    // on the ranges of all the subhits. Make its score
    // the sum of the scores of all the subhits

    m_SeqRange1 = m_SubHit[0]->m_SeqRange1;
    m_SeqRange2 = m_SubHit[0]->m_SeqRange2;
    m_Score = m_SubHit[0]->m_Score;

    for (int i = 1; i < (int)m_SubHit.size(); i++) {
        CHit *hit = m_SubHit[i];
        m_SeqRange1.CombineWith(hit->m_SeqRange1);
        m_SeqRange2.CombineWith(hit->m_SeqRange2);
        m_Score += hit->m_Score;
    }
}


void
CHit::GetRangeFromSeq2(TRange seq_range2,
                       TRange& seq_range1,
                       TRange& new_seq_range2,
                       TRange& tback_range)
{
    _ASSERT(m_SeqRange2.Contains(seq_range2));

    TOffsetPair start_off(m_SeqRange1.GetFrom(), 
                          m_SeqRange2.GetFrom());
    TOffsetPair new_off;
    TOffset new_tback;
    TOffset target_off;

    // Find the left endpoint of the range

    target_off = seq_range2.GetFrom();
    m_EditScript.FindOffsetFromSeq2(start_off, new_off, target_off,
                                    new_tback, true);

    seq_range1.SetFrom(new_off.first);
    new_seq_range2.SetFrom(new_off.second);
    tback_range.SetFrom(new_tback);

    // Find the right endpoint of the range

    target_off = seq_range2.GetTo();
    m_EditScript.FindOffsetFromSeq2(start_off, new_off, target_off,
                                    new_tback, false);

    seq_range1.SetTo(new_off.first);
    new_seq_range2.SetTo(new_off.second);
    tback_range.SetTo(new_tback);
}


void
CHit::GetRangeFromSeq1(TRange seq_range1,
                       TRange& new_seq_range1,
                       TRange& seq_range2,
                       TRange& tback_range)
{
    _ASSERT(m_SeqRange1.Contains(seq_range1));

    TOffsetPair start_off(m_SeqRange1.GetFrom(), 
                          m_SeqRange2.GetFrom());
    TOffsetPair new_off;
    TOffset new_tback;
    TOffset target_off;

    // Find the left endpoint of the range

    target_off = seq_range1.GetFrom();
    m_EditScript.FindOffsetFromSeq1(start_off, new_off, target_off,
                                    new_tback, true);

    new_seq_range1.SetFrom(new_off.first);
    seq_range2.SetFrom(new_off.second);
    tback_range.SetFrom(new_tback);

    // Find the right endpoint of the range

    target_off = seq_range1.GetTo();
    m_EditScript.FindOffsetFromSeq1(start_off, new_off, target_off,
                                    new_tback, false);

    new_seq_range1.SetTo(new_off.first);
    seq_range2.SetTo(new_off.second);
    tback_range.SetTo(new_tback);
}

void
CHit::VerifyHit()
{
    // verify query and subject ranges are nonempty

    _ASSERT(!m_SeqRange1.Empty());
    _ASSERT(!m_SeqRange2.Empty());

    // verify the traceback matches up to the sequence ranges

    m_EditScript.VerifyScript(m_SeqRange1, m_SeqRange2);
}


/// callback used for sorting HSP lists
struct compare_hit_seq1_start {

    /// functor that implements hit comparison
    /// @param a First hit
    /// @param b Second hit
    /// @return true if a and b are sorted in order of
    ///         increasing sequence 1 start offset, with 
    ///         sequence1 end offset used as a tiebreaker
    ///
    bool operator()(CHit * const& a, CHit * const& b) const {
        if (a->m_SeqRange1.GetFrom() < b->m_SeqRange1.GetFrom())
            return true;
        if (a->m_SeqRange1.GetFrom() > b->m_SeqRange1.GetFrom())
            return false;

        return (a->m_SeqRange1.GetTo() < b->m_SeqRange1.GetTo());
    }
};


/// Delete any hits from a list that are contained within
/// higher-scoring hits. Only overlap on sequence 1 is considered.
/// In practice, the hits refer to block alignments derived from
/// RPS blast results, and sequence 2 is an RPS database sequence.
/// It is sequence 1 that matters for later processing
/// @param subhits The list of hits to process [in/modified]
///
static void
x_RemoveEnvelopedSubHits(CHit::TSubHit& subhits)
{
    // order the hits by start offset of sequence 1
    sort(subhits.begin(), subhits.end(), compare_hit_seq1_start());
    int num_subhits = subhits.size();

    for (int i = 0; i < num_subhits - 1; i++) {

        // skip hits that have already been deleted

        CHit *hit1 = subhits[i];
        if (hit1 == 0)
            continue;

        // for all hits past hit i

        for (int j = i + 1; j < num_subhits; j++) {

            // skip hits that have already been deleted

            CHit *hit2 = subhits[j];
            if (hit2 == 0)
                continue;

            // stop looking when hits that do not overlap at all
            // on sequence 1 are encountered

            TRange& range1(hit1->m_SeqRange1);
            TRange& range2(hit2->m_SeqRange1);
            if (range1.StrictlyBelow(range2) ||
                range2.StrictlyBelow(range1)) {
                break;
            }

            // if the sequence 1 ranges overlap, delete the
            // lower-scoring hit

            if (range1.Contains(range2) || range2.Contains(range1)) {
                if (hit1->m_Score > hit2->m_Score) {
                    delete subhits[j]; subhits[j] = 0;
                    continue;
                }
                else {
                    delete subhits[i]; subhits[i] = 0;
                    break;
                }
            }
        }
    }

    // compress the list of hits to remove null pointers

    int j = 0;
    for (int i = 0; i < num_subhits; i++) {
        if (subhits[i] != 0)
            subhits[j++] = subhits[i];
    }
    if (j < num_subhits)
        subhits.resize(j);
}


void
CHit::ResolveSubHitConflicts(CSequence& seq1,
                       int **seq2_pssm,
                       CNWAligner::TScore gap_open,
                       CNWAligner::TScore gap_extend)
{
    if (m_SubHit.size() < 2)
        return;

    // first remove subhits that are completely contained

    x_RemoveEnvelopedSubHits(m_SubHit);

    int num_subhits = m_SubHit.size();

    // if there are still any conflicts, they are only between
    // adjacent pairs of hits. For all such pairs...

    for (int i = 0; i < num_subhits - 1; i++) {
        CHit *hit1 = m_SubHit[i];
        CHit *hit2 = m_SubHit[i + 1];

        TRange& seq1range1(hit1->m_SeqRange1);
        TRange& seq1range2(hit1->m_SeqRange2);
        TRange& seq2range1(hit2->m_SeqRange1);
        TRange& seq2range2(hit2->m_SeqRange2);

        // ignore pairs of hits that will never overlap
        // on sequence 1

        if (seq1range1.StrictlyBelow(seq2range1))
            continue;

        // there's an overlap. First assume the the first hit is
        // shortened, and calculate the score of the resulting alignment

        TRange tback_range1;
        TRange new_q_range1(seq1range1.GetFrom(), seq2range1.GetFrom() - 1);
        TRange new_s_range1;
        hit1->GetRangeFromSeq1(new_q_range1, new_q_range1,
                               new_s_range1, tback_range1);

        int score1 = hit1->GetEditScript().GetScore(tback_range1, 
                                    TOffsetPair(seq1range1.GetFrom(),
                                                seq1range2.GetFrom()),
                                    seq1, seq2_pssm, gap_open, gap_extend);

        // repeat the process assuming the second hit is shortened

        TRange tback_range2;
        TRange new_q_range2(seq1range1.GetTo() + 1, seq2range1.GetTo());
        TRange new_s_range2;
        hit2->GetRangeFromSeq1(new_q_range2, new_q_range2,
                               new_s_range2, tback_range2);

        int score2 = hit2->GetEditScript().GetScore(tback_range2, 
                                    TOffsetPair(seq2range1.GetFrom(), 
                                                seq2range2.GetFrom()),
                                    seq1, seq2_pssm, gap_open, gap_extend);

#if 0
        //------------------------------------------
        printf("fixup query %d db %d ", hit1->m_SeqIndex1, hit1->m_SeqIndex2);
        printf("hit1: qoff %d-%d dboff %d-%d score %d ",
               hit1->m_SeqRange1.GetFrom(), hit1->m_SeqRange1.GetTo(), 
               hit1->m_SeqRange2.GetFrom(), hit1->m_SeqRange2.GetTo(), 
               hit1->m_Score);
        printf("    vs     hsp2: %d-%d %d-%d score %d ",
               hit2->m_SeqRange1.GetFrom(), hit2->m_SeqRange1.GetTo(), 
               hit2->m_SeqRange2.GetFrom(), hit2->m_SeqRange2.GetTo(), 
               hit2->m_Score);
        printf("\n");
        //------------------------------------------
#endif

        // keep the combination that scores the highest

        if (score1 + hit2->m_Score > hit1->m_Score + score2) {
            m_SubHit[i+1] = new CHit(hit2->m_SeqIndex1, hit2->m_SeqIndex2,
                                     new_q_range2, new_s_range2, score2,
                        hit2->GetEditScript().MakeEditScript(tback_range2));
            delete hit2;
        }
        else {
            m_SubHit[i] = new CHit(hit1->m_SeqIndex1, hit1->m_SeqIndex2,
                                   new_q_range1, new_s_range1, score1,
                        hit1->GetEditScript().MakeEditScript(tback_range1));
            delete hit1;
        }
    }
}


CHit *
CHit::Clone()
{
    CHit *new_hit = new CHit(m_SeqIndex1, m_SeqIndex2,
                             m_SeqRange1, m_SeqRange2,
                             m_Score, m_EditScript);
    if (HasSubHits()) {
        NON_CONST_ITERATE(TSubHit, itr, GetSubHit()) {
            new_hit->InsertSubHit((*itr)->Clone());
        }
    }
    return new_hit;
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/* ========================================================================
 * $Log$
 * Revision 1.8  2006/09/20 19:41:25  papadopo
 * add member to initialize a hit from an ASN.1 denseg
 *
 * Revision 1.7  2006/03/22 19:23:17  dicuccio
 * Cosmetic changes: adjusted include guards; formatted CVS logs; added export
 * specifiers
 *
 * Revision 1.6  2006/03/15 02:26:32  ucko
 * +<algorithm> (once indirectly included?) for sort().
 *
 * Revision 1.5  2005/11/18 20:19:02  papadopo
 * add documentation
 *
 * Revision 1.4  2005/11/17 22:28:45  papadopo
 * rename Copy() to Clone()
 *
 * Revision 1.3  2005/11/08 18:42:16  papadopo
 * assert -> _ASSERT
 *
 * Revision 1.2  2005/11/08 17:52:59  papadopo
 * ASSERT -> assert
 *
 * Revision 1.1  2005/11/07 18:14:00  papadopo
 * Initial revision
 *
 * ======================================================================
 */
