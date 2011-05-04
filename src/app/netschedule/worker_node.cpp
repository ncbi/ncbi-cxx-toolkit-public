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
// SAffinityInfo

const TNSBitVector& 
SAffinityInfo::GetBlacklistedJobs(time_t t)
{
    if (! blacklisted_expirations.empty()  &&  t >= min_expire_time) {
        TExpirations new_blacklisted_expirations;
        bool expired = false;
        bool inited = false;
        ITERATE(TExpirations, it, blacklisted_expirations) {
            time_t exp_time = it->first;
            _ASSERT(exp_time);
            if (! inited  ||  exp_time < min_expire_time) {
                min_expire_time = exp_time;
                inited = true;
            }
            if (exp_time <= t) {
                expired = true;
                blacklisted_jobs.set(it->second, false);
            } else {
                new_blacklisted_expirations.push_back(*it);
            }
        }
        if (expired)
            blacklisted_expirations = new_blacklisted_expirations;
    }
    return blacklisted_jobs;
}


void SAffinityInfo::BlacklistJob(unsigned job_id, time_t exp_time)
{
    bool was_empty = !blacklisted_jobs.any();
    blacklisted_jobs.set(job_id);
    if (exp_time) {
        blacklisted_expirations.push_back(
            pair<time_t, unsigned>(exp_time, job_id));
        if (was_empty || exp_time < min_expire_time)
            min_expire_time = exp_time;
    }
}


///////////////////////////////////////////////////////////////////////
// CWorkerNode

void CWorkerNode::SetAuth(const string& auth)
{
    m_Auth = auth;
}


void CWorkerNode::SetNotificationTimeout(unsigned timeout)
{
    m_LastVisit = time(0);
    m_NotifyTime = timeout > 0 ? m_LastVisit + timeout : 0;
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
    ITERATE(TJobInfoById, it, m_JobInfoById) {
        node_run_timeout = std::max(node_run_timeout, (*it)->exp_time);
    }
    m_JobValidityTime = node_run_timeout;
}


std::string CWorkerNode::AsString(time_t curr, EWNodeFormat fmt) const
{
    CTime lv_time(m_LastVisit);

    string s(m_Auth + " @ ");
    s += CSocketAPI::gethostbyaddr(m_Host);
    s += " UDP:";
    s += NStr::UIntToString(m_Port);
    s += " ";
    s += lv_time.ToLocalTime().AsString();
    if (fmt == eWNF_WithNodeId) {
        s += " (";
        s += NStr::Int8ToString(ValidityTime() - curr);
        s += ") ";
        if (!m_Id.empty()) {
            s += "node_id=" + m_Id + " ";
        }
        s += "jobs=";
        ITERATE(TJobInfoById, it, m_JobInfoById) {
            s += " ";
            s += NStr::UIntToString((*it)->job_id);
        }
    }
    return s;
}

CWorkerNode_Real::CWorkerNode_Real(unsigned host) : CWorkerNode(host, 0)
{
    m_WorkerNodeList = NULL;
    m_LastVisit = time(0);
    m_VisitTimeout = 60;
    m_JobValidityTime = 0;
    m_NotifyTime = 0;
    m_Registered = false;
    m_NewStyle = false;
}

CWorkerNode_Real::~CWorkerNode_Real()
{
    if (m_WorkerNodeList != NULL)
        m_WorkerNodeList->UnregisterNode(this);
}


///////////////////////////////////////////////////////////////////////
// CWorkerNodeAffinityGuard

CWorkerNodeAffinityGuard::CWorkerNodeAffinityGuard(CWorkerNode& wn)
    : m_WorkerNode(wn)
{
    // FIXME: It is very unfortunate that there can be a CWorkerNode
    // not bound to CWorkerNodeList. May be it is easier to remove
    // node from the list rather than insert it only upon identification.
    if (!m_WorkerNode.m_WorkerNodeList)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Attempt to lock unidentified node");
    m_WorkerNode.m_WorkerNodeList->m_Lock.ReadLock();
    m_WorkerNode.m_Lock.Lock();
}


CWorkerNodeAffinityGuard::~CWorkerNodeAffinityGuard()
{
    m_WorkerNode.m_WorkerNodeList->m_Lock.Unlock();
    m_WorkerNode.m_Lock.Unlock();
}


bool CWorkerNodeAffinityGuard::HasCandidates()
{
    return m_WorkerNode.m_AffinityInfo.candidate_jobs.any();
}


void CWorkerNodeAffinityGuard::CleanCandidates(const TNSBitVector& aff_ids)
{
    SAffinityInfo& ai = m_WorkerNode.m_AffinityInfo;

    // Filter out affinities not in requested list, and if
    // something was actually filtered drop cached candidate_jobs
    unsigned old_count = ai.aff_ids.count();
    ai.aff_ids &= aff_ids;
    if (old_count != ai.aff_ids.count())
        ai.candidate_jobs.clear();
}


const TNSBitVector& CWorkerNodeAffinityGuard::GetAffinities(time_t t)
{
    return m_WorkerNode.m_AffinityInfo.aff_ids;
}


const TNSBitVector& CWorkerNodeAffinityGuard::GetBlacklistedJobs(time_t t)
{
    return m_WorkerNode.m_AffinityInfo.GetBlacklistedJobs(t);
}


void CWorkerNodeAffinityGuard::AddAffinity(unsigned aff_id, time_t exp_time)
{
    m_WorkerNode.m_AffinityInfo.aff_ids.set(aff_id);
}

unsigned
CWorkerNodeAffinityGuard::GetJobWithAffinities(const TNSBitVector* aff_ids,
                                               CJobStatusTracker& status_tracker,
                                               IAffinityResolver& affinity_resolver)
{
    unsigned job_id = 0;
    SAffinityInfo& ai = m_WorkerNode.m_AffinityInfo;
    if (aff_ids == 0)
        aff_ids = &ai.aff_ids;
    if (!ai.candidate_jobs.any() && aff_ids->any()) {
        // NB: locks m_AffinityIdxLock, thereby creating
        // m_WorkerNode.m_WorkerNodeList->m_Lock < m_AffinityIdxLock locking order
        affinity_resolver.GetJobsWithAffinities(*aff_ids, &ai.candidate_jobs);
        if (!ai.candidate_jobs.any()) // no candidates
            return 0;
        status_tracker.PendingIntersect(&ai.candidate_jobs);
        ai.candidate_jobs -= ai.blacklisted_jobs;
        ai.candidate_jobs.count(); // speed up any()
        if (!ai.candidate_jobs.any())
            return 0;
        ai.candidate_jobs.optimize(0, TNSBitVector::opt_free_0);
    }
    if (!ai.candidate_jobs.any())
        return 0;
    bool pending_jobs_avail =
        status_tracker.GetPendingJobFromSet(
        &ai.candidate_jobs, &job_id);

    if (!job_id && !pending_jobs_avail)
        return unsigned(-1); // signal outer code that there no more jobs at all
    return job_id;
}



void CWorkerNodeAffinityGuard::BlacklistJob(unsigned job_id, time_t exp_time)
{
    m_WorkerNode.m_AffinityInfo.BlacklistJob(job_id, exp_time);
}


///////////////////////////////////////////////////////////////////////
// CQueueWorkerNodeList

CQueueWorkerNodeList::CQueueWorkerNodeList(const string& queue_name)
  : m_QueueName(queue_name), m_LastNotifyTime(0)
{
    m_HostName = CSocketAPI::gethostname();
    m_StartTime = time(0);
    m_GeneratedIdCounter.Set(0);
}

void CQueueWorkerNodeList::RegisterNode(CWorkerNode* worker_node)
{
    CWriteLockGuard guard(m_Lock);

    worker_node->m_WorkerNodeList = this;
    m_WorkerNodeRegister.insert(TWorkerNodeRef(worker_node));
    worker_node->m_Registered = true;
}

void CQueueWorkerNodeList::UnregisterNode(CWorkerNode* worker_node)
{
    CWriteLockGuard guard(m_Lock);

    m_WorkerNodeByAddress.erase(worker_node);
    m_WorkerNodeById.erase(worker_node);

    ITERATE(TJobInfoById, job_info_it, worker_node->m_JobInfoById) {
        m_JobInfoById.erase(*job_info_it);
        delete *job_info_it;
    }

    worker_node->m_JobInfoById.clear();
}

void CQueueWorkerNodeList::SetId(
    CWorkerNode* worker_node, const string& node_id)
{
    CWriteLockGuard guard(m_Lock);

    if (!worker_node->m_Id.empty())
        m_WorkerNodeById.erase(worker_node);

    worker_node->m_Id = node_id;

    if (!node_id.empty()) {
        m_WorkerNodeById.insert(worker_node);
        worker_node->m_NewStyle = true;
    }
}

void CQueueWorkerNodeList::ClearNode(CWorkerNode* worker_node, TJobList& jobs)
{
    CWriteLockGuard guard(m_Lock);

    DisplaceWorkerNodeJobs(worker_node, jobs);
}

TWorkerNodeRef CQueueWorkerNodeList::ClearNode(
    const string& node_id, TJobList& jobs)
{
    CWriteLockGuard guard(m_Lock);

    CWorkerNode* worker_node = FindWorkerNodeById(node_id);

    if (worker_node != NULL)
        DisplaceWorkerNodeJobs(worker_node, jobs);

    return TWorkerNodeRef(worker_node);
}

void CQueueWorkerNodeList::AddJob(CWorkerNode* worker_node,
                                  const CJob& job,
                                  time_t exp_time,
                                  CRequestContextFactory* req_ctx_f,
                                  bool is_log)
{
    CWriteLockGuard guard(m_Lock);
    CRequestContext* req_ctx = 0;
    if (is_log && req_ctx_f) {
        req_ctx = req_ctx_f->Get();
        if (!job.GetClientIP().empty())
            req_ctx->SetClientIP(job.GetClientIP());
        req_ctx->SetSessionID(job.GetClientSID());
    }

    std::auto_ptr<SJobInfo> job_info(new SJobInfo(job.GetId(),
        exp_time, worker_node, req_ctx, req_ctx_f));
    worker_node->SetNotificationTimeout(0);
    if (is_log) {
        CDiagContext::SetRequestContext(req_ctx);
        GetDiagContext().PrintRequestStart()
            .Print("node", worker_node->GetId())
            .Print("queue", m_QueueName)
            .Print("job_id", NStr::IntToString(job.GetId()));
        CDiagContext::SetRequestContext(0);
    }

    m_JobInfoById.insert(job_info.get());
    try {
        worker_node->m_JobInfoById.insert(job_info.get());
    }
    catch (...) {
        m_JobInfoById.erase(job_info.get());
        throw;
    }
    job_info.release();
    worker_node->UpdateValidityTime();
}


void CQueueWorkerNodeList::UpdateJob(TNSJobId job_id,
                                     time_t exp_time)
{
    CWriteLockGuard guard(m_Lock);
    SJobInfo* job_info = FindJobById(job_id);
    if (job_info != NULL) {
        job_info->exp_time = exp_time;
        CWorkerNode* worker_node = job_info->assigned_node;
        worker_node->UpdateValidityTime();
        worker_node->SetNotificationTimeout(0);
    } else {
        ERR_POST("Could not find job by id " << job_id <<
            " to update expiration time");
    }
}


void CQueueWorkerNodeList::RemoveJob(const CJob&   job,
                                     ENSCompletion reason,
                                     bool          is_log)
{
    CWriteLockGuard guard(m_Lock);
    SJobInfo* job_info = FindJobById(job.GetId());
    if (job_info == NULL) {
        ERR_POST("Could not find job " << job.GetId() << " in WN list");
        return;
    }
    CWorkerNode* worker_node = job_info->assigned_node;
    //
    CRequestContext* req_ctx = job_info->req_ctx;
    if (reason != eNSCTimeout)
        worker_node->SetNotificationTimeout(0);
    if (is_log && req_ctx) {
        CDiagContext::SetRequestContext(req_ctx);
        CDiagContext::GetRequestContext().SetRequestStatus(int(reason));
        if (reason == eNSCDone || reason == eNSCFailed) {
            CDiagContext_Extra extra = GetDiagContext().Extra();
            extra.Print("output", job.GetOutput());
            const CJobRun* run = job.GetLastRun();
            if (run) {
                extra.Print("ret_code", NStr::IntToString(run->GetRetCode()))
                     .Print("err_msg", run->GetErrorMsg());
            }
        }
        GetDiagContext().PrintRequestStop();
    }
    if (req_ctx) {
        CDiagContext::SetRequestContext(0);
        _ASSERT(job_info->factory.NotEmpty());
        if (job_info->factory.NotEmpty())
            job_info->factory->Return(req_ctx);
    }
    worker_node->m_JobInfoById.erase(job_info);
    worker_node->UpdateValidityTime();
    // TODO Can be optimized by reusing the iterator
    // returned by find() in FindJobById().
    m_JobInfoById.erase(job_info);
    delete job_info;
}

SJobInfo* CQueueWorkerNodeList::FindJobById(TNSJobId job_id)
{
    SJobInfo search_image(job_id);
    TJobInfoById::iterator it = m_JobInfoById.find(&search_image);
    return it != m_JobInfoById.end() ? *it : NULL;
}

CWorkerNode* CQueueWorkerNodeList::FindWorkerNodeByAddress(
    unsigned host, unsigned short port)
{
    CWorkerNode search_image(host, port);
    TWorkerNodeByAddress::iterator it =
        m_WorkerNodeByAddress.find(&search_image);
    return it != m_WorkerNodeByAddress.end() ? *it : NULL;
}

CWorkerNode* CQueueWorkerNodeList::FindWorkerNodeById(const string& node_id)
{
    CWorkerNode search_image(node_id);
    TWorkerNodeById::iterator it = m_WorkerNodeById.find(&search_image);
    return it != m_WorkerNodeById.end() ? *it : NULL;
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
    ITERATE(TWorkerNodeRegister, it, m_WorkerNodeRegister) {
        const CWorkerNode* node = *it;
        if (!node->m_Port) continue;
        if (!unconditional && !node->ShouldNotify(curr))
            continue;
        has_notify = true;
        notify_list.push_back(TWorkerNodeHostPort(node->m_Host,
            node->m_Port));
    }
    if (!has_notify) return false;
    m_LastNotifyTime = curr;
    return true;
}


void CQueueWorkerNodeList::GetNodes(time_t t, list<TWorkerNodeRef>& nodes) const
{
    CReadLockGuard guard(m_Lock);
    ITERATE(TWorkerNodeRegister, it, m_WorkerNodeRegister) {
        CWorkerNode* node = (*it).GetNCPointer();
        if (node->ValidityTime() > t)
            nodes.push_back(TWorkerNodeRef(node));
    }
}


void
CQueueWorkerNodeList::RegisterNotificationListener(
    CWorkerNode*           worker_node,
    unsigned               timeout)
{
    CWriteLockGuard guard(m_Lock);
    worker_node->SetNotificationTimeout(timeout);
}

void CQueueWorkerNodeList::SetPort(
    CWorkerNode* worker_node, unsigned short port)
{
    CWriteLockGuard guard(m_Lock);

    unsigned short old_port = worker_node->m_Port;

    if (port == 0 || old_port == port)
        return;

    worker_node->m_Port = port;

    std::pair<TWorkerNodeByAddress::iterator, bool> insertion_result =
        m_WorkerNodeByAddress.insert(worker_node);

    if (insertion_result.second) {
        if (old_port > 0) {
            CWorkerNode search_image(worker_node->GetHost(), old_port);

            m_WorkerNodeByAddress.erase(&search_image);

            LOG_POST(Warning << "Node '" << worker_node->m_Id <<
                "' (" << CSocketAPI::gethostbyaddr(worker_node->m_Host) <<
                ":" << old_port << ") changed its control "
                "port to " << port);
        }
    } else {
        worker_node->m_Port = old_port;

        string host(CSocketAPI::gethostbyaddr(worker_node->m_Host));

        ERR_POST("Refused to replace registered WN " <<
            (*insertion_result.first)->m_Id << " [" <<
            host << ":" << port << "] with WN " << worker_node->m_Id <<
            " [" << host << ":" << old_port << "] by port assignment");
    }
}


bool
CQueueWorkerNodeList::UnRegisterNotificationListener(CWorkerNode* worker_node)
{
    CWriteLockGuard guard(m_Lock);

    // If the node is old style, destroy node information including affinity
    // association. New style node will explicitly call ClearNode
    return !worker_node->m_NewStyle && worker_node->IsIdentified();
}


void CQueueWorkerNodeList::IdentifyWorkerNodeByAddress(
    TWorkerNodeRef& temporary_wn, unsigned short port)
{
    CWriteLockGuard guard(m_Lock);

    CWorkerNode* identified_wn = FindWorkerNodeByAddress(
        temporary_wn->GetHost(), port);

    if (identified_wn != NULL)
        MergeWorkerNodes(temporary_wn, identified_wn);
    else {
        // Use the current WN and make it an "identified" node
        // by assigning a port number.
        temporary_wn->m_Port = port;
        m_WorkerNodeByAddress.insert(temporary_wn);
    }
}

void CQueueWorkerNodeList::IdentifyWorkerNodeByJobId(
    TWorkerNodeRef& temporary_wn, TNSJobId job_id)
{
    CWriteLockGuard guard(m_Lock);

    SJobInfo* job_info = FindJobById(job_id);

    // Check that the job is not assigned to a temporary worker node.
    if (job_info != NULL && job_info->assigned_node->IsIdentified() &&
            temporary_wn->m_Host == job_info->assigned_node->m_Host)
        MergeWorkerNodes(temporary_wn, job_info->assigned_node);
}

CQueueWorkerNodeList::~CQueueWorkerNodeList()
{
    CWriteLockGuard guard(m_Lock);

    NON_CONST_ITERATE(TWorkerNodeRegister, it, m_WorkerNodeRegister) {
        it->GetNCPointer()->m_WorkerNodeList = NULL;
    }
}

void CQueueWorkerNodeList::DisplaceWorkerNodeJobs(
    CWorkerNode* worker_node, TJobList& jobs)
{
    ITERATE(TJobInfoById, job_info_it, worker_node->m_JobInfoById) {
        jobs.push_back((*job_info_it)->job_id);
        m_JobInfoById.erase(*job_info_it);
        delete *job_info_it;
    }

    worker_node->m_JobInfoById.clear();
}

void CQueueWorkerNodeList::MergeWorkerNodes(
    TWorkerNodeRef& temporary, CWorkerNode* identified)
{
    identified->m_JobInfoById.insert(temporary->m_JobInfoById.begin(),
        temporary->m_JobInfoById.end());

    ITERATE(TJobInfoById, job_info_it, temporary->m_JobInfoById) {
        (*job_info_it)->assigned_node = identified;
    }

    temporary->m_JobInfoById.clear();

    // FIXME Merge affinities as well

    if (identified->m_LastVisit < temporary->m_LastVisit)
        identified->m_LastVisit = temporary->m_LastVisit;

    if (identified->m_VisitTimeout < temporary->m_VisitTimeout)
        identified->m_VisitTimeout = temporary->m_VisitTimeout;

    if (identified->m_JobValidityTime < temporary->m_JobValidityTime)
        identified->m_JobValidityTime = temporary->m_JobValidityTime;

    if (identified->m_NotifyTime < temporary->m_NotifyTime)
        identified->m_NotifyTime = temporary->m_NotifyTime;

    m_WorkerNodeRegister.erase(temporary);
    temporary = identified;
}


///////////////////////////////////////////////////////////////////////
// CQueueWorkerNodeListGuard

CQueueWorkerNodeListGuard::CQueueWorkerNodeListGuard(CQueueWorkerNodeList& wnl)
    : m_WorkerNodeList(wnl)
{
    m_WorkerNodeList.m_Lock.WriteLock();
}


CQueueWorkerNodeListGuard::~CQueueWorkerNodeListGuard()
{
    m_WorkerNodeList.m_Lock.Unlock();
}


void CQueueWorkerNodeListGuard::GetNodes(time_t t,
                                         list<TWorkerNodeRef>& nodes) const
{
    ITERATE(TWorkerNodeRegister, it, m_WorkerNodeList.m_WorkerNodeRegister) {
        CWorkerNode* node = (*it).GetNCPointer();
        if (node->ValidityTime() > t)
            nodes.push_back(TWorkerNodeRef(node));
    }
}


const TNSBitVector&
CQueueWorkerNodeListGuard::GetNodeAffinities(time_t t, CWorkerNode* wn)
{
    return wn->m_AffinityInfo.aff_ids;
}


void CQueueWorkerNodeListGuard::GetAffinities(time_t t, TNSBitVector& aff_ids)
{
    ITERATE(TWorkerNodeRegister, it, m_WorkerNodeList.m_WorkerNodeRegister) {
        CWorkerNode* node = (*it).GetNCPointer();
        if (node->ValidityTime() > t)
            aff_ids |= node->m_AffinityInfo.aff_ids;
    }
}


/*
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
*/


END_NCBI_SCOPE
