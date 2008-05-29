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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Queue cleaning thread.
 *                   
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include "smng_thread.hpp"

BEGIN_NCBI_SCOPE

CSessionManagementThread::CSessionManagementThread(
                            CNetCacheServer& nc_server,
                            unsigned         inact_shut_delay,
                            unsigned         run_delay,
                            unsigned         stop_request_poll)
: CThreadNonStop(run_delay, stop_request_poll),
  m_NC_Server(nc_server),
  m_InactivityShutdownDelay(inact_shut_delay),
  m_LastSessionExitTime(0),
  m_ShutdownFlag(false)
{
}

CSessionManagementThread::~CSessionManagementThread()
{
}

void CSessionManagementThread::DoJob(void)
{
    try {
        CFastMutexGuard mg(m_Lock);
        if (m_LastSessionExitTime) {
            time_t curr = time(0);   
            if (unsigned(curr - m_LastSessionExitTime) > 
                                        m_InactivityShutdownDelay) {
                ERR_POST(
                  "Session inactivity timeout expired, shutting down.");
                m_NC_Server.SetShutdownFlag();
                m_ShutdownFlag = true;
                return;
            }
        }
    }
    catch(exception& ex)
    {
        RequestStop();
        ERR_POST(Error << "Error during notification: "
                        << ex.what()
                        << " session management thread has been stopped.");
    }
}


bool CSessionManagementThread::RegisterSession(const string& host, 
                                               unsigned      pid)
{
    CFastMutexGuard mg(m_Lock);
    if (m_ShutdownFlag) {
        return false;
    }
    TSessionList::iterator it = x_FindSession(host, pid);
    if (it == m_SessionList.end()) {
        m_SessionList.push_back(SSessionInfo(host, pid));
        m_LastSessionExitTime = 0;
    }
    return true;
}

void CSessionManagementThread::UnRegisterSession(const string& host, 
                                               unsigned      pid)
{
    CFastMutexGuard mg(m_Lock);
    TSessionList::iterator it = x_FindSession(host, pid);
    if (it != m_SessionList.end()) {
        m_SessionList.erase(it);
        if (m_SessionList.empty()) {
            m_LastSessionExitTime = time(0);
        }
    }
}

CSessionManagementThread::TSessionList::iterator 
CSessionManagementThread::x_FindSession(const string& host, 
                                        unsigned      pid)
{
    NON_CONST_ITERATE(TSessionList, it, m_SessionList) {
        SSessionInfo& si = *it;
        if (si.host_name == host && si.pid == pid) {
            return it;
        }
    }
    return m_SessionList.end();
}


END_NCBI_SCOPE
