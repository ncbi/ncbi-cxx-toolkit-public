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

File name: traceback.cpp

Author: Jason Papadopoulos

Contents: implementation of CEditScript class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/traceback.hpp>

/// @file traceback.cpp
/// Implementation of CEditScript class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)
USING_SCOPE(objects);

void 
CEditScript::AddOps(EGapAlignOpType op_type, int num_ops)
{
    // add num_ops traceback operations, either as a 
    // separate edit script operation or merged into
    // the previous operation

    if (m_Script.empty()) {
        m_Script.push_back(STracebackOp(op_type, num_ops));
    }
    else {
        STracebackOp& last_op(m_Script.back());
        if (last_op.op_type == op_type) {
            last_op.num_ops += num_ops;
        }
        else {
            m_Script.push_back(STracebackOp(op_type, num_ops));
        }
    }
}


CEditScript::CEditScript(GapEditScript *blast_tback) 
{
    _ASSERT(blast_tback);

    for (Int4 i = 0;  i < blast_tback->size;  ++i) {
        AddOps(blast_tback->op_type[i], blast_tback->num[i]);
    }
}


CEditScript::CEditScript(const CDense_seg& denseg)
{
    _ASSERT(denseg.GetDim() == 2);

    CDense_seg::TNumseg num_seg = denseg.GetNumseg();
    const CDense_seg::TStarts& starts = denseg.GetStarts();
    const CDense_seg::TLens& lens = denseg.GetLens();

    for (CDense_seg::TNumseg i = 0; i < num_seg; i++) {

        TSignedSeqPos start1 = starts[2*i];
        TSignedSeqPos start2 = starts[2*i+1];

        if (start1 < 0)
            AddOps(eGapAlignDel, (int)lens[i]);
        else if (start2 < 0)
            AddOps(eGapAlignIns, (int)lens[i]);
        else
            AddOps(eGapAlignSub, (int)lens[i]);
    }
}


CEditScript::CEditScript(const CDense_diag& dendiag)
{
    _ASSERT(dendiag.GetDim() == 2);

    const CDense_diag::TLen& len = dendiag.GetLen();
    AddOps(eGapAlignSub, (int)len);
}


CEditScript
CEditScript::MakeEditScript(const CNWAligner::TTranscript& tback, 
                           TRange tback_range)
{
    _ASSERT(!tback_range.Empty());
    _ASSERT(tback_range.GetTo() < (int)tback.size());

    // Note that the traceback conventions between blast and CNWAligner
    // are opposites: blast wants deletions to represent
    // gaps in the first sequence and insertions for gaps
    // in the second, while CNWAligner wants the reverse.
    // However, an HSP wants the query first and the subject
    // second, while the aligner has the subject first and
    // the query second (if the subject is a profile, which
    // happens to be the case in this application). Hence 
    // the two opposites cancel, and deletions and insertions 
    // can be assumed to mean the same for both algorithms.

    CEditScript new_script;
    for (int i = tback_range.GetFrom(); i <= tback_range.GetTo(); i++) {
        switch (tback[i]) {
        case CNWAligner::eTS_Delete:
            new_script.AddOps(eGapAlignDel, 1);
            break;

        case CNWAligner::eTS_Insert:
            new_script.AddOps(eGapAlignIns, 1);
            break;

        case CNWAligner::eTS_Match:
        case CNWAligner::eTS_Replace:
            new_script.AddOps(eGapAlignSub, 1);
            break;

        default:
            break;
        }
    }
    return new_script;
}


CEditScript
CEditScript::MakeEditScript(TRange tback_range)
{
    _ASSERT(!tback_range.Empty());

    // locate the first link that contains tback_range

    int link_start = 0;
    TScriptOps::iterator itr(m_Script.begin());
    while (itr != m_Script.end()) {
        if (link_start + itr->num_ops > tback_range.GetFrom())
            break;

        link_start += itr->num_ops;
        itr++;
    }
    _ASSERT(itr != m_Script.end());

    CEditScript new_script;

    // mark off traceback operations until tback_range is exhausted
    int curr_tback = tback_range.GetFrom();
    while (itr != m_Script.end() && 
           curr_tback <= tback_range.GetTo()) {

        // The following handles a complete or partial range,
        // at the beginning, middle, or end of tback_range.
        // It also handles ranges with only a single link. 

        int num_ops = min(tback_range.GetTo() - curr_tback + 1,
                          link_start + itr->num_ops - curr_tback);
        _ASSERT(num_ops);
        new_script.AddOps(itr->op_type, num_ops);
        curr_tback += num_ops;
        link_start += itr->num_ops;
        itr++;
    }

    // make sure traceback didn't run out prematurely
    _ASSERT(curr_tback > tback_range.GetTo());
    return new_script;
}

void 
CEditScript::FindOffsetFromSeq2(TOffsetPair start_offsets,
                                TOffsetPair& new_offsets,
                                TOffset seq2_target, TOffset& new_tback,
                                bool go_past_seq1_gap)
{
    TOffset curr_tback = 0;
    TOffset q = start_offsets.first;
    TOffset s = start_offsets.second;
    TScriptOps::iterator itr(m_Script.begin());

    // for each link in the edit script
    
    while (itr != m_Script.end()) {

        if (itr->op_type == eGapAlignDel) {

            // gap in query; if seq2_target occurs in
            // this link then either point beyond the
            // entire gap or exclude the entire gap

            if (s + itr->num_ops > seq2_target) {
                if (go_past_seq1_gap == false) {
                    q--; s--; curr_tback--;
                }
                else {
                    s += itr->num_ops;
                    curr_tback += itr->num_ops;
                }
                break;
            }

            // otherwise move to start offset of next link 
            s += itr->num_ops;
            curr_tback += itr->num_ops;
        }
        else if (itr->op_type == eGapAlignSub) {

            // substitutions; if seq2_target does not occur in 
            // this link, advance to the start of the next link

            if (s + itr->num_ops > seq2_target) {
                curr_tback += seq2_target - s;
                q += seq2_target - s;
                s = seq2_target;
                break;
            }
            q += itr->num_ops;
            s += itr->num_ops;
            curr_tback += itr->num_ops;
        }
        else {

            // gap in subject; just skip to the start of
            // the next link, since this link will never 
            // get to seq2_target

            q += itr->num_ops;
            curr_tback += itr->num_ops;
        }

        itr++;
    }

    // traceback must not run out before the desired
    // query offset is found
    _ASSERT(itr != m_Script.end());

    new_offsets.first = q;
    new_offsets.second = s;
    new_tback = curr_tback;
}


void 
CEditScript::FindOffsetFromSeq1(TOffsetPair start_offsets,
                                TOffsetPair& new_offsets,
                                TOffset seq1_target, TOffset& new_tback,
                                bool go_past_seq2_gap)
{
    TOffset curr_tback = 0;
    TOffset q = start_offsets.first;
    TOffset s = start_offsets.second;
    TScriptOps::iterator itr(m_Script.begin());

    // for each link in the edit script
    
    while (itr != m_Script.end()) {

        if (itr->op_type == eGapAlignDel) {

            // gap in query; just skip to the start of
            // the next link, since this link will never 
            // get to seq1_target

            s += itr->num_ops;
            curr_tback += itr->num_ops;
        }
        else if (itr->op_type == eGapAlignSub) {

            // substitutions; if seq1_target does not occur in 
            // this link, advance to the start of the next link

            if (q + itr->num_ops > seq1_target) {
                curr_tback += seq1_target - q;
                s += seq1_target - q;
                q = seq1_target;
                break;
            }
            q += itr->num_ops;
            s += itr->num_ops;
            curr_tback += itr->num_ops;
        }
        else {

            // gap in subject; if seq1_target occurs in
            // this link then either point beyond the
            // entire gap or exclude the entire gap

            if (q + itr->num_ops > seq1_target) {
                if (go_past_seq2_gap == false) {
                    q--; s--; curr_tback--;
                }
                else {
                    q += itr->num_ops;
                    curr_tback += itr->num_ops;
                }
                break;
            }

            // otherwise move to start offset of next link 
            q += itr->num_ops;
            curr_tback += itr->num_ops;
        }

        itr++;
    }

    // traceback must not run out before the desired
    // subject offset is found
    _ASSERT(itr != m_Script.end());

    new_offsets.first = q;
    new_offsets.second = s;
    new_tback = curr_tback;
}

int 
CEditScript::GetScore(TRange tback_range, 
                      TOffsetPair start_offsets,
                      CSequence& seq1, 
                      int **seq2_pssm, int gap_open, int gap_extend)
{
    TOffset q = start_offsets.first;
    TOffset s = start_offsets.second;
    int score = 0;

    _ASSERT(!tback_range.Empty());

    // find the first link that contains tback_range
    int link_start = 0;
    TScriptOps::iterator itr(m_Script.begin());
    while (itr != m_Script.end()) {
        if (link_start + itr->num_ops > tback_range.GetFrom())
            break;

        switch (itr->op_type) {
        case eGapAlignDel: 
            s += itr->num_ops; 
            break;
        case eGapAlignSub: 
            s += itr->num_ops; 
            q += itr->num_ops; 
            break;
        case eGapAlignIns: 
            q += itr->num_ops; 
            break;
        default: 
            break;
        }

        link_start += itr->num_ops;
        itr++;
    }
    _ASSERT(itr != m_Script.end());

    // assign the first query and subject offset in
    // tback_range to q and s, respectively
    switch (itr->op_type) {
    case eGapAlignDel: 
        s += tback_range.GetFrom() - link_start;
        break;
    case eGapAlignSub: 
        s += tback_range.GetFrom() - link_start;
        q += tback_range.GetFrom() - link_start;
        break;
    case eGapAlignIns: 
        q += tback_range.GetFrom() - link_start;
        break;
    default: 
        break;
    }

    // proceed link by link until tback_range is exhausted
    TOffset curr_tback = tback_range.GetFrom();
    while (itr != m_Script.end() && 
           curr_tback <= tback_range.GetTo()) {

        // The following handles a complete or partial range,
        // at the beginning, middle, or end of tback_range.
        // It also handles ranges with only a single link. 

        int num_ops = min(tback_range.GetTo() - curr_tback + 1,
                          link_start + itr->num_ops - curr_tback);
        _ASSERT(num_ops);

        switch (itr->op_type) {
        case eGapAlignDel: 
            score += gap_open + num_ops * gap_extend;
            s += num_ops;
            break;
        case eGapAlignSub: 
            for (int i = 0; i < num_ops; i++) {
                unsigned char c = seq1.GetLetter(q + i);
                score += seq2_pssm[s + i][c];
            }
            s += num_ops;
            q += num_ops;
            break;
        case eGapAlignIns: 
            score += gap_open + num_ops * gap_extend;
            q += num_ops;
            break;
        default: 
            break;
        }

        curr_tback += num_ops;
        link_start += itr->num_ops;
        itr++;
    }

    // make sure traceback did not run out prematurely
    _ASSERT(curr_tback > tback_range.GetTo());

    return score;
}

vector<TOffsetPair>
CEditScript::ListMatchRegions(TOffsetPair start_offsets)
{
    vector<TOffsetPair> region_list;
    int q = start_offsets.first;
    int s = start_offsets.second;

    TScriptOps::iterator itr(m_Script.begin());
    while (itr != m_Script.end()) {
        switch (itr->op_type) {
        case eGapAlignDel:
            s += itr->num_ops;
            break;
        case eGapAlignSub:
            region_list.push_back(TOffsetPair(q, s));
            q += itr->num_ops;
            s += itr->num_ops;
            region_list.push_back(TOffsetPair(q, s));
            break;
        case eGapAlignIns:
            q += itr->num_ops;
            break;
        default:
            break;
        }

        itr++;
    }
    return region_list;
}

void
CEditScript::VerifyScript(TRange seq1_range, TRange seq2_range)
{
    int length1 = 0;
    int length2 = 0;

    if (m_Script.empty())
        return;

    for (int i = 0; i < (int)m_Script.size(); i++) {
        switch (m_Script[i].op_type) {
        case eGapAlignDel:
            length2 += m_Script[i].num_ops;
            break;
        case eGapAlignSub:
            length1 += m_Script[i].num_ops;
            length2 += m_Script[i].num_ops;
            break;
        case eGapAlignIns:
            length1 += m_Script[i].num_ops;
            break;
        default:
            break;
        }
    }
    _ASSERT(length1 == seq1_range.GetLength());
    _ASSERT(length2 == seq2_range.GetLength());
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
