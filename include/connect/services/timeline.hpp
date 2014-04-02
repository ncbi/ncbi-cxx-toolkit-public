#ifndef CONNECT_SERVICES__TIMELINE_HPP
#define CONNECT_SERVICES__TIMELINE_HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Grid Worker Node Timeline - declarations.
 *
 */

#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE

class CWorkerNodeTimeline_Base;

class NCBI_XCONNECT_EXPORT CWorkerNodeTimelineEntry
{
public:
    friend class CWorkerNodeTimeline_Base;

    CWorkerNodeTimelineEntry() : m_Timeline(NULL), m_Deadline(0, 0) {}

    bool IsInTimeline() const {return m_Timeline != NULL;}

    bool IsInTimeline(CWorkerNodeTimeline_Base* timeline) const
    {
        return m_Timeline == timeline;
    }

    void Cut();

    const CDeadline& GetTimeout() const {return m_Deadline;}

    void ResetTimeout(unsigned seconds)
    {
        m_Deadline = CDeadline(seconds, 0);
    }

    bool TimeHasCome() const
    {
        return m_Deadline.GetRemainingTime().IsZero();
    }

private:
    CWorkerNodeTimeline_Base* m_Timeline;
    CWorkerNodeTimelineEntry* m_Prev;
    CWorkerNodeTimelineEntry* m_Next;
    CDeadline                 m_Deadline;
};

class NCBI_XCONNECT_EXPORT CWorkerNodeTimeline_Base
{
public:
    friend class CWorkerNodeTimelineEntry;

    CWorkerNodeTimeline_Base() : m_Head(NULL), m_Tail(NULL)
    {
    }

    CWorkerNodeTimelineEntry* GetHead() const {return m_Head;}

    bool IsEmpty() const {return m_Head == NULL;}

    void Push(CWorkerNodeTimelineEntry* new_entry)
    {
        _ASSERT(new_entry->m_Timeline == NULL);
        if (m_Tail != NULL)
            (m_Tail->m_Next = new_entry)->m_Prev = m_Tail;
        else
            (m_Head = new_entry)->m_Prev = NULL;
        new_entry->m_Next = NULL;
        (m_Tail = new_entry)->m_Timeline = this;
    }

    CWorkerNodeTimelineEntry* Shift()
    {
        _ASSERT(m_Head != NULL);
        CWorkerNodeTimelineEntry* old_head = m_Head;
        if ((m_Head = old_head->m_Next) != NULL)
            m_Head->m_Prev = NULL;
        else
            m_Tail = NULL;
        old_head->m_Timeline = NULL;
        return old_head;
    }

private:
    CWorkerNodeTimelineEntry* m_Head;
    CWorkerNodeTimelineEntry* m_Tail;
};

inline void CWorkerNodeTimelineEntry::Cut()
{
    if (m_Timeline != NULL) {
        if (m_Prev == NULL)
            if ((m_Timeline->m_Head = m_Next) == NULL)
                m_Timeline->m_Tail = NULL;
            else
                m_Next->m_Prev = NULL;
        else if (m_Next == NULL)
            (m_Timeline->m_Tail = m_Prev)->m_Next = NULL;
        else
            (m_Prev->m_Next = m_Next)->m_Prev = m_Prev;
        m_Timeline = NULL;
    }
}

template <typename TTimelineEntry>
class CWorkerNodeTimeline : public CWorkerNodeTimeline_Base
{
public:
    TTimelineEntry* GetHead() const
    {
        return static_cast<TTimelineEntry*>(
                CWorkerNodeTimeline_Base::GetHead());
    }
    TTimelineEntry* Shift()
    {
        return static_cast<TTimelineEntry*>(CWorkerNodeTimeline_Base::Shift());
    }
};

END_NCBI_SCOPE

#endif // !defined(CONNECT_SERVICES__TIMELINE_HPP)
