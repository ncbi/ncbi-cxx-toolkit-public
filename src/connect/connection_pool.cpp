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
* Authors:  Aaron Ucko, Victor Joukov
*
* File Description:
*   threaded server connection pool
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "connection_pool.hpp"


BEGIN_NCBI_SCOPE

void CServer_ConnectionPool::SPerConnInfo::UpdateExpiration(
    const TConnBase* conn)
{
    const STimeout* timeout = kDefaultTimeout;
    const CSocket*  socket  = dynamic_cast<const CSocket*>(conn);

    if (socket) {
        timeout = socket->GetTimeout(eIO_ReadWrite);
    }
    if (type == eInactiveSocket  &&  timeout != kDefaultTimeout
        &&  timeout != kInfiniteTimeout) {
        expiration.SetCurrent();
        expiration.AddSecond(timeout->sec);
        expiration.AddNanoSecond(timeout->usec * 1000);
    } else {
        expiration.Clear();
    }
}


bool CServer_ConnectionPool::Add(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);

    if (data.size() >= m_MaxConnections) {
        // XXX - try to prune old idle connections before giving up?
        return false;
    }

    SPerConnInfo& info = data[conn];
    info.type = type;
    info.UpdateExpiration(conn);

    return true;
}


void CServer_ConnectionPool::SetConnType(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    TData::iterator it = data.find(conn);
    if (it == data.end()) {
        _TRACE("SetConnType called on unknown connection.");
    } else {
        it->second.type = type;
        it->second.UpdateExpiration(conn);
    }
}


void CServer_ConnectionPool::Clean(void)
{
    CTime now(CTime::eCurrent, CTime::eGmt);
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    if (data.empty()) {
        return;
    }
    for (TData::iterator next = data.begin(), it = next++;
         next != data.end();
         it = next, ++next) {
        if (it->second.type != eInactiveSocket) {
            continue;
        }
        if (!it->first->IsOpen() || it->second.expiration <= now) {
            it->first->OnTimeout();
            delete it->first;
            data.erase(it);
        }
    }
}


void CServer_ConnectionPool::GetPollVec(vector<CSocketAPI::SPoll>& polls) const
{
    polls.clear();
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    polls.reserve(data.size());
    ITERATE (TData, it, data) {
        CPollable* pollable = dynamic_cast<CPollable*>(it->first);
        _ASSERT(pollable);
        if (it->second.type != eActiveSocket) {
            if (!it->first->IsOpen() ) {
                continue;
            }
            polls.push_back
                (CSocketAPI::SPoll(pollable, it->first->GetEventsToPollFor()));
        }
    }
}


void CServer_ConnectionPool::StopListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Passivate();
    }
}


void CServer_ConnectionPool::ResumeListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Activate();
    }
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 6.1  2006/09/13 18:32:21  joukovv
* Added (non-functional yet) framework for thread-per-request thread pooling model,
* netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
*
*
* ===========================================================================
*/
