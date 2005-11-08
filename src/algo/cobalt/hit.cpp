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

/// @file hit.cpp
/// Implementation of CHit class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

void CHit::AddUpSubHits()
{
    assert(HasSubHits());

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
    assert(m_SeqRange2.Contains(seq_range2));

    TOffsetPair start_off(m_SeqRange1.GetFrom(), 
                          m_SeqRange2.GetFrom());
    TOffsetPair new_off;
    TOffset new_tback;
    TOffset target_off;

    target_off = seq_range2.GetFrom();
    m_EditScript.FindOffsetFromSeq2(start_off, new_off, target_off,
                                    new_tback, true);

    seq_range1.SetFrom(new_off.first);
    new_seq_range2.SetFrom(new_off.second);
    tback_range.SetFrom(new_tback);

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
    assert(m_SeqRange1.Contains(seq_range1));

    TOffsetPair start_off(m_SeqRange1.GetFrom(), 
                          m_SeqRange2.GetFrom());
    TOffsetPair new_off;
    TOffset new_tback;
    TOffset target_off;

    target_off = seq_range1.GetFrom();
    m_EditScript.FindOffsetFromSeq1(start_off, new_off, target_off,
                                    new_tback, true);

    new_seq_range1.SetFrom(new_off.first);
    seq_range2.SetFrom(new_off.second);
    tback_range.SetFrom(new_tback);

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

    assert(!m_SeqRange1.Empty());
    assert(!m_SeqRange2.Empty());

    // verify the traceback matches up to the sequence ranges

    m_EditScript.VerifyScript(m_SeqRange1, m_SeqRange2);
}


/// callback used for sorting HSP lists
struct compare_hit_seq1_start {
    bool operator()(CHit * const& a, CHit * const& b) const {
        if (a->m_SeqRange1.GetFrom() < b->m_SeqRange1.GetFrom())
            return true;
        if (a->m_SeqRange1.GetFrom() > b->m_SeqRange1.GetFrom())
            return false;

        return (a->m_SeqRange1.GetTo() < b->m_SeqRange1.GetTo());
    }
};

static void
x_RemoveEnvelopedSubHits(CHit::TSubHit& subhits)
{
    sort(subhits.begin(), subhits.end(), compare_hit_seq1_start());
    int num_subhits = subhits.size();

    for (int i = 0; i < num_subhits - 1; i++) {

        CHit *hit1 = subhits[i];
        if (hit1 == 0)
            continue;

        for (int j = i + 1; j < num_subhits; j++) {

            CHit *hit2 = subhits[j];
            if (hit2 == 0)
                continue;

            TRange& range1(hit1->m_SeqRange1);
            TRange& range2(hit2->m_SeqRange1);
            if (range1.StrictlyBelow(range2) ||
                range2.StrictlyBelow(range1)) {
                break;
            }

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

    x_RemoveEnvelopedSubHits(m_SubHit);

    int num_subhits = m_SubHit.size();

    for (int i = 0; i < num_subhits - 1; i++) {
        CHit *hit1 = m_SubHit[i];
        CHit *hit2 = m_SubHit[i + 1];

        TRange& seq1range1(hit1->m_SeqRange1);
        TRange& seq1range2(hit1->m_SeqRange2);
        TRange& seq2range1(hit2->m_SeqRange1);
        TRange& seq2range2(hit2->m_SeqRange2);

        if (seq1range1.StrictlyBelow(seq2range1))
            continue;

        TRange tback_range1;
        TRange new_q_range1(seq1range1.GetFrom(), seq2range1.GetFrom() - 1);
        TRange new_s_range1;
        hit1->GetRangeFromSeq1(new_q_range1, new_q_range1,
                               new_s_range1, tback_range1);

        int score1 = hit1->GetEditScript().GetScore(tback_range1, 
                                    TOffsetPair(seq1range1.GetFrom(),
                                                seq1range2.GetFrom()),
                                    seq1, seq2_pssm, gap_open, gap_extend);

        TRange tback_range2;
        TRange new_q_range2(seq1range1.GetTo() + 1, seq2range1.GetTo());
        TRange new_s_range2;
        hit2->GetRangeFromSeq1(new_q_range2, new_q_range2,
                               new_s_range2, tback_range2);

        int score2 = hit2->GetEditScript().GetScore(tback_range2, 
                                    TOffsetPair(seq2range1.GetFrom(), 
                                                seq2range2.GetFrom()),
                                    seq1, seq2_pssm, gap_open, gap_extend);

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
CHit::Copy()
{
    CHit *new_hit = new CHit(m_SeqIndex1, m_SeqIndex2,
                             m_SeqRange1, m_SeqRange2,
                             m_Score, m_EditScript);
    if (HasSubHits()) {
        NON_CONST_ITERATE(TSubHit, itr, GetSubHit()) {
            new_hit->InsertSubHit((*itr)->Copy());
        }
    }
    return new_hit;
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/*------------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/08 17:52:59  papadopo
  ASSERT -> assert

  Revision 1.1  2005/11/07 18:14:00  papadopo
  Initial revision

  ----------------------------------------------------------------------*/
