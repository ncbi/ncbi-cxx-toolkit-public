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

class NCBI_XCONNECT_EXPORT CWorkerNodeTimelineEntry : public CObject
{
public:
    friend class CWorkerNodeTimeline_Base;

    CWorkerNodeTimelineEntry() : m_Timeline(NULL), m_Deadline(0, 0) {}

    bool IsInTimeline() const {return m_Timeline != NULL;}

    void MoveTo(CWorkerNodeTimeline_Base* timeline);

    const CDeadline GetTimeout() const {return m_Deadline;}

    void ResetTimeout(unsigned seconds)
    {
        m_Deadline = CDeadline(seconds, 0);
    }

    bool TimeHasCome() const
    {
        return m_Deadline.GetRemainingTime().IsZero();
    }

    CWorkerNodeTimelineEntry* GetNext() const
    {
        return m_Next;
    }

private:
    CWorkerNodeTimeline_Base* m_Timeline;
    CWorkerNodeTimelineEntry* m_Prev;
    CWorkerNodeTimelineEntry* m_Next;
    CDeadline                 m_Deadline;
};

class NCBI_XCONNECT_EXPORT CWorkerNodeTimeline_Base
{
protected:
    friend class CWorkerNodeTimelineEntry;

    CWorkerNodeTimeline_Base() : m_Head(NULL), m_Tail(NULL)
    {
    }

public:
    bool IsEmpty() const {return m_Head == NULL;}

    void Push(CWorkerNodeTimelineEntry* new_entry)
    {
        _ASSERT(new_entry->m_Timeline == NULL);
        AttachTail(new_entry);
        new_entry->AddReference();
    }

protected:
    void AttachTail(CWorkerNodeTimelineEntry* new_tail)
    {
        new_tail->m_Timeline = this;
        new_tail->m_Next = NULL;
        if ((new_tail->m_Prev = m_Tail) != NULL)
            m_Tail->m_Next = new_tail;
        else
            m_Head = new_tail;
        m_Tail = new_tail;
    }

    CWorkerNodeTimelineEntry* DetachHead()
    {
        _ASSERT(m_Head != NULL);
        CWorkerNodeTimelineEntry* detached_head = m_Head;
        if ((m_Head = detached_head->m_Next) != NULL)
            m_Head->m_Prev = NULL;
        else
            m_Tail = NULL;
        detached_head->m_Timeline = NULL;
        return detached_head;
    }

public:
    void MoveHeadTo(CWorkerNodeTimeline_Base* timeline)
    {
        timeline->AttachTail(DetachHead());
    }

    void Clear()
    {
        CWorkerNodeTimelineEntry* entry = m_Head;
        if (entry != NULL) {
            m_Tail = m_Head = NULL;
            do {
                entry->m_Timeline = NULL;
                CWorkerNodeTimelineEntry* next_entry = entry->m_Next;
                entry->RemoveReference();
                entry = next_entry;
            } while (entry != NULL);
        }
    }

protected:
    ~CWorkerNodeTimeline_Base()
    {
        Clear();
    }

    CWorkerNodeTimelineEntry* m_Head;
    CWorkerNodeTimelineEntry* m_Tail;
};

inline void CWorkerNodeTimelineEntry::MoveTo(CWorkerNodeTimeline_Base* timeline)
{
    if (m_Timeline != timeline) {
        if (m_Timeline == NULL)
            AddReference();
        else {
            if (m_Prev == NULL)
                if ((m_Timeline->m_Head = m_Next) != NULL)
                    m_Next->m_Prev = NULL;
                else
                    m_Timeline->m_Tail = NULL;
            else if (m_Next == NULL)
                (m_Timeline->m_Tail = m_Prev)->m_Next = NULL;
            else
                (m_Prev->m_Next = m_Next)->m_Prev = m_Prev;
        }
        timeline->AttachTail(this);
    }
}

template <typename TTimelineEntry, typename TRefType>
class CWorkerNodeTimeline : public CWorkerNodeTimeline_Base
{
public:
    TRefType GetHead() const
    {
        return TRefType(static_cast<TTimelineEntry*>(m_Head));
    }
    TRefType GetNext(TRefType ref) const
    {
        _ASSERT(ref);
        return TRefType(static_cast<TTimelineEntry*>(ref->GetNext()));
    }
    void Shift(TRefType& ref)
    {
        CWorkerNodeTimelineEntry* head = DetachHead();
        ref = static_cast<TTimelineEntry*>(head);
        head->RemoveReference();
    }
};

END_NCBI_SCOPE

#endif // !defined(CONNECT_SERVICES__TIMELINE_HPP)
