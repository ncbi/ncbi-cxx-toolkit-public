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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule worker node
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <connect/ncbi_socket.hpp>

#include "worker_node.hpp"

BEGIN_NCBI_SCOPE


//

CQueueWorkerNodes::CQueueWorkerNodes()
  : m_LastNotifyTime(0)
{
}


bool CQueueWorkerNodes::GetNotifyList(bool unconditional, time_t curr,
                                      int  notify_timeout, list<TWorkerNodeHostPort>& notify_list)
{                                     
    CWriteLockGuard guard(m_WNodeLock);
    if (!unconditional &&
        (notify_timeout == 0 ||
         m_LastNotifyTime + notify_timeout > curr))
        return false;

    // Get worker node list to notify
    bool has_notify = false;
    ITERATE(TWorkerNodes, it, m_WorkerNodes) {
        if (!unconditional && it->second.notify_time < curr)
            continue;
        // Check that notification port is OK
        if (!it->first.second)
            continue;
        has_notify = true;
        notify_list.push_back(it->first);
    }
    if (!has_notify) return false;
    m_LastNotifyTime = curr;
    return true;
}


void CQueueWorkerNodes::GetNodesInfo(time_t curr, int run_timeout,
                                     list<string>& nodes_info) const
{
    CReadLockGuard guard(m_WNodeLock);
    ITERATE(TWorkerNodes, it, m_WorkerNodes) {
        unsigned host = it->first.first;
        unsigned short port = it->first.second;
        const SWorkerNodeInfo& wn_info = it->second;
        int visit_timeout = (wn_info.visit_timeout ?
            wn_info.visit_timeout : run_timeout) + 20 ;
        if (wn_info.last_visit + visit_timeout < curr)
            continue;

        CTime lv_time(wn_info.last_visit);

        string info(wn_info.auth);
        info += " @ ";
        info += CSocketAPI::gethostbyaddr(host);
        info += "  UDP:";
        info += NStr::UIntToString(port);
        info += "  ";
        info += lv_time.ToLocalTime().AsString();
        nodes_info.push_back(info);
    }
}


void CQueueWorkerNodes::RegisterNotificationListener(unsigned        host,
                                                    unsigned short  port,
                                                    unsigned        timeout,
                                                    const string&   auth)
{
    CWriteLockGuard guard(m_WNodeLock);
    TWorkerNodes::iterator it =
        m_WorkerNodes.find(TWorkerNodeHostPort(host, port));
    time_t curr = time(0);
    if (it != m_WorkerNodes.end()) {  // update registration timestamp
        it->second.Set(curr, timeout, auth);
    } else {
        m_WorkerNodes[TWorkerNodeHostPort(host, port)] =
            SWorkerNodeInfo(curr, timeout, auth);
    }
}


void CQueueWorkerNodes::UnRegisterNotificationListener(unsigned       host,
                                                      unsigned short port)
{
    CWriteLockGuard guard(m_WNodeLock);
    m_WorkerNodes.erase(TWorkerNodeHostPort(host, port));
}


void CQueueWorkerNodes::RegisterWorkerNodeVisit(unsigned       host,
                                               unsigned short port,
                                               unsigned       timeout)
{
    CWriteLockGuard guard(m_WNodeLock);
    TWorkerNodes::iterator it =
        m_WorkerNodes.find(TWorkerNodeHostPort(host, port));
    if (it == m_WorkerNodes.end()) return;
    it->second.last_visit = time(0);
    it->second.visit_timeout = timeout;
}


END_NCBI_SCOPE
