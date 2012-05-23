#ifndef NETSCHEDULE_QUEUE_COLL__HPP
#define NETSCHEDULE_QUEUE_COLL__HPP


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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   NetSchedule queue collection and database.
 *
 */


/// @file queue_coll.hpp
/// NetSchedule queue collection and database.
///
/// @internal


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <connect/services/netschedule_api.hpp>
#include "ns_util.hpp"
#include "queue_clean_thread.hpp"
#include "ns_queue.hpp"
#include "queue_vc.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;


/// Queue database manager
///
/// @internal
///
class CQueueIterator;
class CQueueConstIterator;


class CQueueCollection
{
public:
    typedef map<string, CRef<CQueue> >      TQueueMap;
    typedef CQueueIterator                  iterator;
    typedef CQueueConstIterator             const_iterator;

public:
    CQueueCollection(const CQueueDataBase& db);
    ~CQueueCollection();

    void Close();

    CRef<CQueue> GetQueue(const string& qname) const;
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    CQueue& AddQueue(const string& name, CQueue* queue);
    // Remove queue from collection, notifying every interested party
    // through week reference mechanism.
    bool RemoveQueue(const string& name);

    // Iteration over queue map
    CQueueIterator       begin();
    CQueueIterator       end();
    CQueueConstIterator  begin() const;
    CQueueConstIterator  end()   const;

    size_t GetSize() const { return m_QMap.size(); }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);

private:
    friend class CQueueIterator;
    friend class CQueueConstIterator;

    const CQueueDataBase&   m_QueueDataBase;
    TQueueMap               m_QMap;
    mutable CRWLock         m_Lock;
};



class CQueueIterator
{
public:
    CQueueIterator(CQueueCollection::TQueueMap::iterator  iter,
                   CRWLock *                              lock)
        : m_Iter(iter), m_Lock(lock)
    {
        if (m_Lock)
            m_Lock->ReadLock();
    }

    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueIterator(const CQueueIterator& rhs)
        : m_Iter(rhs.m_Iter), m_Lock(rhs.m_Lock)
    {
        // Linear on lock
        if (m_Lock)
            rhs.m_Lock = 0;
    }

    ~CQueueIterator()
    {
        if (m_Lock)
            m_Lock->Unlock();
    }

    CQueue& operator*()
    {
        return *m_Iter->second;
    }

    const string & GetName() const
    {
        return m_Iter->first;
    }

    void operator++()
    {
        ++m_Iter;
    }

    bool operator==(const CQueueIterator& rhs)
    {
        return m_Iter == rhs.m_Iter;
    }

    bool operator!=(const CQueueIterator& rhs)
    {
        return m_Iter != rhs.m_Iter;
    }

private:
    CQueueCollection::TQueueMap::iterator m_Iter;
    mutable CRWLock*                      m_Lock;
};


class CQueueConstIterator
{
public:
    CQueueConstIterator(CQueueCollection::TQueueMap::const_iterator  iter,
                        CRWLock *                                    lock)
        : m_Iter(iter), m_Lock(lock)
    {
        if (m_Lock)
            m_Lock->ReadLock();
    }

    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueConstIterator(const CQueueConstIterator& rhs)
        : m_Iter(rhs.m_Iter), m_Lock(rhs.m_Lock)
    {
        // Linear on lock
        if (m_Lock)
            rhs.m_Lock = 0;
    }

    ~CQueueConstIterator()
    {
        if (m_Lock) m_Lock->Unlock();
    }

    const CQueue& operator*() const
    {
        return *m_Iter->second;
    }

    const string & GetName() const
    {
        return m_Iter->first;
    }

    void operator++()
    {
        ++m_Iter;
    }

    bool operator==(const CQueueConstIterator& rhs)
    {
        return m_Iter == rhs.m_Iter;
    }

    bool operator!=(const CQueueConstIterator& rhs)
    {
        return m_Iter != rhs.m_Iter;
    }

private:
    CQueueCollection::TQueueMap::const_iterator m_Iter;
    mutable CRWLock *                           m_Lock;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_QUEUE_COLL__HPP */

