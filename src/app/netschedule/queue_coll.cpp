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
 * File Description: NetSchedule queue collection and database managenement.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/ncbireg.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_util.hpp>

#include <util/time_line.hpp>

#include "queue_coll.hpp"
#include "ns_util.hpp"
#include "netschedule_version.hpp"
#include "ns_server.hpp"


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// CQueueCollection implementation
CQueueCollection::CQueueCollection(const CQueueDataBase& db) :
    m_QueueDataBase(db)
{}


CQueueCollection::~CQueueCollection()
{}


void CQueueCollection::Close()
{
    CWriteLockGuard     guard(m_Lock);

    m_QMap.clear();
}


CRef<CQueue> CQueueCollection::GetQueue(const string& name) const
{
    CReadLockGuard              guard(m_Lock);
    TQueueMap::const_iterator   it = m_QMap.find(name);
    if (it == m_QMap.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Job queue not found: " + name);
    return it->second;
}


bool CQueueCollection::QueueExists(const string& name) const
{
    CReadLockGuard      guard(m_Lock);

    return (m_QMap.find(name) != m_QMap.end());
}


CQueue& CQueueCollection::AddQueue(const string& name,
                                   CQueue*       queue)
{
    CWriteLockGuard     guard(m_Lock);

    m_QMap[name] = queue;
    return *queue;
}


bool CQueueCollection::RemoveQueue(const string& name)
{
    CWriteLockGuard         guard(m_Lock);
    TQueueMap::iterator     it = m_QMap.find(name);

    if (it == m_QMap.end())
        return false;

    m_QMap.erase(it);
    return true;
}


// FIXME: remove this arcane construct, CQueue now has all
// access methods required by ITERATE(CQueueCollection, it, m_QueueCollection)
CQueueIterator CQueueCollection::begin()
{
    return CQueueIterator(m_QMap.begin(), &m_Lock);
}


CQueueIterator CQueueCollection::end()
{
    return CQueueIterator(m_QMap.end(), NULL);
}


CQueueConstIterator CQueueCollection::begin() const
{
    return CQueueConstIterator(m_QMap.begin(), &m_Lock);
}


CQueueConstIterator CQueueCollection::end() const
{
    return CQueueConstIterator(m_QMap.end(), NULL);
}


END_NCBI_SCOPE

