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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of NetSchedule client integrated with NCBI load balancer.
 *
 */

#include <ncbi_pch.hpp>

#include "job_time_line.hpp"

BEGIN_NCBI_SCOPE


CJobTimeLine::CJobTimeLine(unsigned discr_factor, time_t tm)
: m_DiscrFactor(discr_factor)
{
    ReInit(tm);
}

CJobTimeLine::~CJobTimeLine()
{
    NON_CONST_ITERATE(TTimeLine, it, m_TimeLine) {
        TObjVector* bv = *it;
        delete bv;
    }
}

void CJobTimeLine::ReInit(time_t tm)
{
    NON_CONST_ITERATE(TTimeLine, it, m_TimeLine) {
        TObjVector* bv = *it;
        delete bv;
    }
    if (tm == 0) {
        tm = time(0);
    }
    m_TimeLine.resize(0);
    m_TimeLineHead = (tm / m_DiscrFactor) * m_DiscrFactor;
    m_TimeLine.push_back(0);
}

void CJobTimeLine::AddObject(time_t object_time, unsigned object_id)
{
    if (object_time < m_TimeLineHead) {
        object_time = m_TimeLineHead;
    }

    unsigned slot = TimeLineSlot(object_time);
    AddObjectToSlot(slot, object_id);
}

void CJobTimeLine::AddObjectToSlot(unsigned slot, unsigned object_id)
{
    while (slot >= m_TimeLine.size()) {
        m_TimeLine.push_back(0); 
    }
    TObjVector* bv = m_TimeLine[slot];
    if (bv == 0) {
        bv = new TObjVector(bm::BM_GAP);
        m_TimeLine[slot] = bv;
    }
    bv->set(object_id);
}

bool CJobTimeLine::RemoveObject(time_t object_time, unsigned object_id)
{
    if (object_time < m_TimeLineHead) {
        return false;
    }
    unsigned slot = TimeLineSlot(object_time);
    if (slot < m_TimeLine.size()) {
        TObjVector* bv = m_TimeLine[slot];
        if (!bv) {
            return false;
        }
        bool v = bv->test(object_id);
        if (v) {
            bv->set(object_id, false);
            return true;
        }
    }
    return false;
}

void CJobTimeLine::RemoveObject(unsigned object_id)
{
    NON_CONST_ITERATE(TTimeLine, it, m_TimeLine) {
        TObjVector* bv = *it;
        if (bv) {
            bv->set(object_id, false);
        }
    }
}

void CJobTimeLine::MoveObject(time_t old_time, 
                              time_t new_time, 
                              unsigned object_id)
{
    bool removed = RemoveObject(old_time, object_id);
    if (!removed) {
        RemoveObject(object_id);
    }
    AddObject(new_time, object_id);
}


unsigned CJobTimeLine::TimeLineSlot(time_t tm) const
{
    _ASSERT(tm >= m_TimeLineHead);

    unsigned interval_head = (tm / m_DiscrFactor) * m_DiscrFactor;
    unsigned diff = interval_head - m_TimeLineHead;
    return diff / m_DiscrFactor;
}

void CJobTimeLine::EnumerateObjects(TObjVector* objects, unsigned slot) const
{
    _ASSERT(objects);

    for (unsigned i = 0; i <= slot && i < m_TimeLine.size(); ++i) {
        const TObjVector* bv = m_TimeLine[i];
        if (bv) {
            *objects |= *bv;
        }
    }
}

void CJobTimeLine::HeadTruncate(unsigned slot)
{
    time_t new_head = m_TimeLineHead + (slot+1) * m_DiscrFactor;

    if (slot + 1 >= m_TimeLine.size()) {
        ReInit(0);
        return;
    }
    for (unsigned i = 0; i <= slot; ++i) {
        if (m_TimeLine.size() == 0) {
            ReInit(0);
            return;
        }
        TObjVector* bv = m_TimeLine[0];
        delete bv;
        m_TimeLine.pop_front();
    }
    m_TimeLineHead = new_head;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/10 14:18:46  kuznets
 * +MoveObject()
 *
 * Revision 1.1  2005/03/09 17:37:16  kuznets
 * Added node notification thread and execution control timeline
 *
 *
 * ===========================================================================
 */
