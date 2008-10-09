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
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <util/md5.hpp>

#include "worker_node.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////
// CWorkerNode

CWorkerNode::CWorkerNode(const SWorkerNodeInfo& node_info, time_t curr,
                         bool new_style)
  : m_Id(node_info.node_id), m_Auth(node_info.auth), m_NewStyle(new_style),
    m_Host(node_info.host), m_Port(node_info.port),
    m_LastVisit(curr), m_VisitTimeout(60),
    m_JobValidityTime(0), m_NotifyTime(0)
{
}


void CWorkerNode::SetClientInfo(const string& auth, unsigned host)
{
    if (m_Host) return;
    m_Auth = auth;
    m_Host = host;
}


void CWorkerNode::SetNotificationTimeout(time_t curr, unsigned timeout)
{
    m_NotifyTime = timeout > 0 ? curr + timeout : 0;
    m_LastVisit = curr;
}


bool CWorkerNode::ShouldNotify(time_t curr) const
{
    return m_Port && m_NotifyTime > curr;
}


time_t CWorkerNode::ValidityTime() const
{
    const time_t visit_time = m_LastVisit + m_VisitTimeout;
    return std::max(visit_time, std::max(m_NotifyTime, m_JobValidityTime));
}


void CWorkerNode::UpdateValidityTime()
{
    time_t node_run_timeout = 0;
    ITERATE(CWorkerNode::TWNJobInfoMap, it, m_Jobs) {
        node_run_timeout = std::max(node_run_timeout, it->second.exp_time);
    }
    m_JobValidityTime = node_run_timeout;
}


std::string CWorkerNode::AsString(time_t curr) const
{
    CTime lv_time(m_LastVisit);

    string s(m_Auth);
    s += " @ ";
    s += CSocketAPI::gethostbyaddr(m_Host);
    s += " UDP:";
    s += NStr::UIntToString(m_Port);
    s += " ";
    if (!m_Id.empty()) {
        s += m_Id;
        s += " ";
    }
    s += lv_time.ToLocalTime().AsString();
    s += " (";
    s += NStr::IntToString(ValidityTime() - curr);
    s += ") jobs:";
    ITERATE(TWNJobInfoMap, it, m_Jobs) {
        s += " ";
        s += NStr::UIntToString(it->first);
    }
    return s;
}


///////////////////////////////////////////////////////////////////////
// CQueueWorkerNodeList

CQueueWorkerNodeList::CQueueWorkerNodeList()
  : m_LastNotifyTime(0)
{
    m_HostName = CSocketAPI::gethostname();
    m_StartTime = time(0);
    m_GeneratedIdCounter.Set(0);
}


void CQueueWorkerNodeList::RegisterNode(const SWorkerNodeInfo& node_info,
                                        TJobList&              jobs)
{
    time_t curr = time(0);
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it1 = m_WorkerNodeById.find(node_info.node_id);
    CWorkerNode* node;
    if (it1 == m_WorkerNodeById.end()) {
    	node = new CWorkerNode(node_info, curr, true);
    	m_WorkerNodeById[node_info.node_id] = CRef<CWorkerNode>(node);
        x_CheckOldWorkerNode(node_info, jobs);
    } else {
        // Re-registers the new style node, probably from completely different
        // host:port and with different auth.
        node = it1->second;
        m_HostPortIdx.erase(TWorkerNodeHostPort(node->m_Host, node->m_Port));
        node->m_Auth = node_info.auth;
        node->m_Host = node_info.host;
        node->m_Port = node_info.port;
    }
    m_HostPortIdx[TWorkerNodeHostPort(node_info.host, node_info.port)] =
        node_info.node_id;
    node->SetNotificationTimeout(curr, 0);
}


void CQueueWorkerNodeList::ClearNode(const string& node_id, TJobList& jobs)
{
    CWriteLockGuard guard(m_Lock);
    x_ClearNode(node_id, jobs);
}


void CQueueWorkerNodeList::AddJob(const string& node_id,
                                  TNSJobId job_id,
                                  time_t exp_time,
                                  CRequestContextFactory* req_ctx_f,
                                  bool log_job_state)
{
    time_t curr = time(0);
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_id);
    if (it == m_WorkerNodeById.end()) return;
    CWorkerNode* node = it->second;
    CRequestContext* req_ctx = 0;
    if (log_job_state && req_ctx_f)
        req_ctx = req_ctx_f->Get();
    node->m_Jobs[job_id] = SJobInfo(exp_time, req_ctx, req_ctx_f);
    node->UpdateValidityTime();
    node->SetNotificationTimeout(curr, 0);
    if (log_job_state) {
        CDiagContext::SetRequestContext(req_ctx);
        GetDiagContext().PrintRequestStart(string("Node ") +
            node_id + " job " + NStr::IntToString(job_id));
    }
}


void CQueueWorkerNodeList::UpdateJob(const string& node_id,
                                     TNSJobId job_id,
                                     time_t exp_time)
{
    time_t curr = time(0);
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_id);
    if (it == m_WorkerNodeById.end()) return;
    CWorkerNode* node = it->second;
    CWorkerNode::TWNJobInfoMap::iterator it1 = node->m_Jobs.find(job_id);
    if (it1 == node->m_Jobs.end()) return;
    node->m_Jobs[job_id].exp_time = exp_time;
    node->UpdateValidityTime();
    node->SetNotificationTimeout(curr, 0);
}


void CQueueWorkerNodeList::RemoveJob(const string& node_id,
                                     TNSJobId job_id,
                                     ENSCompletion reason,
                                     bool log_job_state)
{
    time_t curr = time(0);
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_id);
    if (it == m_WorkerNodeById.end()) return;
    CWorkerNode* node = it->second;
    CWorkerNode::TWNJobInfoMap::iterator it1 = node->m_Jobs.find(job_id);
    if (it1 == node->m_Jobs.end()) return;
    //
    SJobInfo& ji = it1->second;
    CRequestContext* req_ctx = ji.req_ctx;
    node->UpdateValidityTime();
    if (reason != eNSCTimeout)
        node->SetNotificationTimeout(curr, 0);
    if (log_job_state && req_ctx) {
        CDiagContext::SetRequestContext(req_ctx);
        CDiagContext::GetRequestContext().SetRequestStatus(int(reason));
        GetDiagContext().PrintRequestStop();
    }
    if (req_ctx) {
        if (ji.factory) ji.factory->Return(req_ctx);
        // else DISASTER
    }
    node->m_Jobs.erase(it1);
}


bool
CQueueWorkerNodeList::GetNotifyList(bool unconditional, time_t curr,
                                    int  notify_timeout,
                                    list<TWorkerNodeHostPort>& notify_list)
{                                     
    CWriteLockGuard guard(m_Lock);
    if (!unconditional &&
        (notify_timeout == 0 ||
         m_LastNotifyTime + notify_timeout > curr))
        return false;

    // Get worker node list to notify
    bool has_notify = false;
    ITERATE(TWorkerNodeById, it, m_WorkerNodeById) {
        const CWorkerNode& node = *(it->second);
        if (!node.m_Port) continue;
        if (!unconditional && !node.ShouldNotify(curr))
            continue;
        has_notify = true;
        notify_list.push_back(TWorkerNodeHostPort(node.m_Host, node.m_Port));
    }
    if (!has_notify) return false;
    m_LastNotifyTime = curr;
    return true;
}


void CQueueWorkerNodeList::GetNodesInfo(time_t curr,
                                        list<string>& nodes_info) const
{
    CReadLockGuard guard(m_Lock);
    ITERATE(TWorkerNodeById, it, m_WorkerNodeById) {
        const CWorkerNode& node = *it->second;
        if (node.ValidityTime() > curr) {
            nodes_info.push_back(node.AsString(curr));
        } else {
            // DEBUG
            nodes_info.push_back(node.AsString(curr)+" invalid");
        }
    }
}


void
CQueueWorkerNodeList::RegisterNotificationListener(
    const SWorkerNodeInfo& node_info,
    unsigned short         port,
    unsigned               timeout,
    TJobList&              jobs)
{
    CWriteLockGuard guard(m_Lock);
    time_t curr = time(0);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_info.node_id);
    if (it == m_WorkerNodeById.end()) return;
    CWorkerNode* node = it->second;
    if (!node->m_Port) {
    	// This is the first worker node command from old style node
    	// which mentions the port (one of GET, WGET, or REGC)
    	node->m_Port = port;
        x_CheckOldWorkerNode(node_info, jobs);
    } else {
    	if (!port) {
    		// GET with 0 port - no notification
    		timeout = 0;
    	} else if (node->m_Port != port) {
    		LOG_POST(Warning << "Node "
    			<< node->m_Id << "(" << node->m_Host << ":" << node->m_Port << ")"
    			<< " changed port to " << port);
    		// Remap to new host:port
            m_HostPortIdx.erase(TWorkerNodeHostPort(node->m_Host, node->m_Port));
            node->m_Port = port;
            m_HostPortIdx[TWorkerNodeHostPort(node->m_Host, node->m_Port)] =
                node->m_Id;
    	}
    }
    node->SetNotificationTimeout(curr, timeout);
}


bool
CQueueWorkerNodeList::UnRegisterNotificationListener(const string& node_id,
    											     TJobList& jobs)
{
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_id);
    if (it == m_WorkerNodeById.end()) return false;
    CWorkerNode* node = it->second;
    if (node->m_NewStyle) return false;
    // If the node is old style, destroy node information including affinity
    // association. New style node will explicitly call ClearNode
    x_ClearNode(it, jobs);
    return true;
}


bool CQueueWorkerNodeList::RegisterNodeVisit(SWorkerNodeInfo& node_info,
                                             TJobList& jobs)
{
    time_t curr = time(0);
    bool first_visit = false;
    CWriteLockGuard guard(m_Lock);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_info.node_id);
    CWorkerNode* node;
    if (it == m_WorkerNodeById.end()) {
    	// This call to worker node-specific function is the first one, so the
    	// node did not call INIT before and is an old style one.
        x_GenerateNodeId(node_info.node_id);
        node = new CWorkerNode(node_info, curr, false);
        m_WorkerNodeById[node_info.node_id] = CRef<CWorkerNode>(node);
        x_CheckOldWorkerNode(node_info, jobs);
        first_visit = true;
    } else {
        node = it->second;
        node->SetClientInfo(node_info.auth, node_info.host);
    }
    node->SetNotificationTimeout(curr, 0);
    return first_visit;
}


void
CQueueWorkerNodeList::x_CheckOldWorkerNode(const SWorkerNodeInfo& node_info,
                                           TJobList&              jobs)
{
    THostPortIdx::iterator it =
        m_HostPortIdx.find(TWorkerNodeHostPort(node_info.host, node_info.port));
    if (it != m_HostPortIdx.end()) {
        if (it->second != node_info.node_id)
            x_ClearNode(it->second, jobs);
    }
    m_HostPortIdx[TWorkerNodeHostPort(node_info.host, node_info.port)] =
        node_info.node_id;
}


void CQueueWorkerNodeList::x_ClearNode(const string& node_id, TJobList& jobs)
{
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(node_id);
    if (it == m_WorkerNodeById.end()) return;
    x_ClearNode(it, jobs);
}

void CQueueWorkerNodeList::x_ClearNode(TWorkerNodeById::iterator& it,
                                       TJobList& jobs)
{
    CWorkerNode* node = it->second;
    ITERATE(CWorkerNode::TWNJobInfoMap, jobinfo_it, node->m_Jobs) {
        jobs.push_back(jobinfo_it->first);
    }
    m_WorkerNodeById.erase(it);
    NON_CONST_ITERATE(THostPortIdx, hostposrt_it, m_HostPortIdx) {
        if (hostposrt_it->second == node->m_Id) {
            m_HostPortIdx.erase(hostposrt_it);
            return;
        }
    }
}


void CQueueWorkerNodeList::x_GenerateNodeId(string& node_id)
{
    // Generate synthetic id for node
    // FIXME: this code is called from under m_Lock guard, do we need it
    // really to use an atomic counter?
    CMD5 md5;
    md5.Update(m_HostName.data(), m_HostName.size());
    time_t t = time(0);
    md5.Update((const char*)&t, sizeof(t));
    unsigned ordinal = (unsigned) m_GeneratedIdCounter.Add(1);
    md5.Update((const char*)&ordinal, sizeof(ordinal));
    md5.Update((const char*)&m_StartTime, sizeof(m_StartTime));
    md5.Update((const char*)&m_LastNotifyTime, sizeof(m_LastNotifyTime));
    node_id = md5.GetHexSum();
}


END_NCBI_SCOPE
