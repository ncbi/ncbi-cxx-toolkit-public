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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "wn_commit_thread.hpp"
#include "wn_cleanup.hpp"
#include "grid_worker_impl.hpp"
#include "netschedule_api_impl.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/ns_job_serializer.hpp>

#include <corelib/rwstream.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     --

CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node) :
    m_WorkerNode(worker_node),
    m_CleanupEventSource(new CWorkerNodeJobCleanup(
        static_cast<CWorkerNodeCleanup*>(worker_node.GetCleanupEventSource()))),
    m_RequestContext(new CRequestContext),
    m_StatusThrottler(1, CTimeSpan(worker_node.GetCheckStatusPeriod(), 0)),
    m_ProgressMsgThrottler(1),
    m_NetScheduleExecutor(worker_node.GetNSExecutor()),
    m_NetCacheAPI(worker_node.GetNetCacheAPI()),
    m_JobGeneration(worker_node.m_CurrentJobGeneration),
    m_CommitExpiration(0, 0)
{
}

const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_WorkerNode.GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName() const
{
    return m_WorkerNode.GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream()
{
    IReader* reader = new CStringOrBlobStorageReader(GetJobInput(),
            m_NetCacheAPI, &m_InputBlobSize);
    m_RStream.reset(new CRStream(reader, 0, 0,
            CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));
    m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_RStream;
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    m_Writer.reset(new CStringOrBlobStorageWriter(
            m_WorkerNode.GetServerOutputSize(),
                    m_NetCacheAPI, GetJob().output));
    m_WStream.reset(new CWStream(m_Writer.get(), 0, 0,
            CRWStreambuf::fLeakExceptions));
    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_WStream;
}

void CWorkerNodeJobContext::CloseStreams()
{
    try {
        m_ProgressMsgThrottler.Reset(1);
        m_StatusThrottler.Reset(1,
                CTimeSpan(m_WorkerNode.GetCheckStatusPeriod(), 0));

        m_RStream.reset();
        m_WStream.reset();

        if (m_Writer.get() != NULL) {
            m_Writer->Close();
            m_Writer.reset();
        }
    }
    NCBI_CATCH_ALL_X(61, "Could not close IO streams");
}

void CWorkerNodeJobContext::CommitJob()
{
    CheckIfCanceled();
    m_JobCommitted = eDone;
}

void CWorkerNodeJobContext::CommitJobWithFailure(const string& err_msg)
{
    CheckIfCanceled();
    m_JobCommitted = eFailure;
    m_Job.error_msg = err_msg;
}

void CWorkerNodeJobContext::ReturnJob()
{
    CheckIfCanceled();
    m_JobCommitted = eReturn;
}

void CWorkerNodeJobContext::RescheduleJob(
        const string& affinity, const string& group)
{
    CheckIfCanceled();
    m_JobCommitted = eRescheduled;
    m_Job.affinity = affinity;
    m_Job.group = group;
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    CheckIfCanceled();
    if (!send_immediately &&
            !m_ProgressMsgThrottler.Approve(CRequestRateControl::eErrCode)) {
        LOG_POST(Warning << "Progress message \"" <<
                msg << "\" has been suppressed.");
        return;
    }

    if (m_WorkerNode.m_ProgressLogRequested) {
        LOG_POST(GetJobKey() << " progress: " <<
                NStr::TruncateSpaces(msg, NStr::eTrunc_End));
    }

    try {
        if (m_Job.progress_msg.empty() ) {
            m_NetScheduleExecutor.GetProgressMsg(m_Job);
        }
        if (!m_Job.progress_msg.empty())
            m_NetCacheAPI.PutData(m_Job.progress_msg, msg.data(), msg.length());
        else {
            m_Job.progress_msg =
                    m_NetCacheAPI.PutData(msg.data(), msg.length());

            m_NetScheduleExecutor.PutProgressMsg(m_Job);
        }
    }
    catch (exception& ex) {
        ERR_POST_X(6, "Couldn't send a progress message: " << ex.what());
    }
}

void CWorkerNodeJobContext::JobDelayExpiration(unsigned runtime_inc)
{
    try {
        m_NetScheduleExecutor.JobDelayExpiration(GetJobKey(), runtime_inc);
    }
    catch (exception& ex) {
        ERR_POST_X(8, "CWorkerNodeJobContext::JobDelayExpiration: " <<
                ex.what());
    }
}

bool CWorkerNodeJobContext::IsLogRequested() const
{
    return m_WorkerNode.m_LogRequested;
}

CNetScheduleAdmin::EShutdownLevel CWorkerNodeJobContext::GetShutdownLevel()
{
    if (m_StatusThrottler.Approve(CRequestRateControl::eErrCode))
        try {
            ENetScheduleQueuePauseMode pause_mode;
            switch (m_NetScheduleExecutor.GetJobStatus(GetJobKey(),
                    NULL, &pause_mode)) {
            case CNetScheduleAPI::eRunning:
                if (pause_mode == eNSQ_WithPullback) {
                    m_WorkerNode.SetJobPullbackTimer(
                            m_WorkerNode.m_DefaultPullbackTimeout);
                    LOG_POST("Pullback request from the server, "
                            "(default) pullback timeout=" <<
                            m_WorkerNode.m_DefaultPullbackTimeout);
                }
                break;

            case CNetScheduleAPI::eCanceled:
                x_SetCanceled();
                /* FALL THROUGH */

            default:
                return CNetScheduleAdmin::eShutdownImmediate;
            }
        }
        catch(exception& ex) {
            ERR_POST("Cannot retrieve job status for " << GetJobKey() <<
                    ": " << ex.what());
        }

    if (m_WorkerNode.CheckForPullback(m_JobGeneration)) {
        LOG_POST("Pullback timeout for " << m_Job.job_id);
        return CNetScheduleAdmin::eShutdownImmediate;
    }

    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void CWorkerNodeJobContext::CheckIfCanceled()
{
    if (IsCanceled()) {
        NCBI_THROW_FMT(CGridWorkerNodeException, eJobIsCanceled,
            "Job " << m_Job.job_id << " has been canceled");
    }
}

void CWorkerNodeJobContext::Reset()
{
    m_JobNumber = CGridGlobals::GetInstance().GetNewJobNumber();

    m_JobCommitted = eNotCommitted;
    m_InputBlobSize = 0;
    m_ExclusiveJob = (m_Job.mask & CNetScheduleAPI::eExclusiveJob) != 0;

    m_RequestContext->Reset();
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    if (!m_ExclusiveJob) {
        if (!m_WorkerNode.EnterExclusiveMode()) {
            NCBI_THROW(CGridWorkerNodeException,
                eExclusiveModeIsAlreadySet, "");
        }
        m_ExclusiveJob = true;
    }
}

const char* CWorkerNodeJobContext::GetCommitStatusDescription(
        CWorkerNodeJobContext::ECommitStatus commit_status)
{
    switch (commit_status) {
    case eDone:
        return "done";
    case eFailure:
        return "failed";
    case eReturn:
        return "returned";
    case eRescheduled:
        return "rescheduled";
    case eCanceled:
        return "canceled";
    default:
        return "not committed";
    }
}

void CWorkerNodeJobContext::x_PrintRequestStop()
{
    m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    if (!m_RequestContext->IsSetRequestStatus())
        m_RequestContext->SetRequestStatus(
            GetCommitStatus() == CWorkerNodeJobContext::eDone &&
                m_Job.ret_code == 0 ? 200 : 500);

    if (m_RequestContext->GetAppState() == eDiagAppState_Request)
        m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    if (g_IsRequestStopEventEnabled())
        GetDiagContext().PrintRequestStop();
}

void CWorkerNodeJobContext::x_RunJob()
{
    m_RequestContext->SetRequestID((int) GetJobNumber());

    if (!m_Job.client_ip.empty())
        m_RequestContext->SetClientIP(m_Job.client_ip);

    if (!m_Job.session_id.empty())
        m_RequestContext->SetSessionID(m_Job.session_id);

    m_RequestContext->SetAppState(eDiagAppState_RequestBegin);

    CRequestContextSwitcher request_state_guard(m_RequestContext);

    if (g_IsRequestStartEventEnabled())
        GetDiagContext().PrintRequestStart().Print("jid", m_Job.job_id);

    GetWorkerNode().x_NotifyJobWatchers(*this,
            IWorkerNodeJobWatcher::eJobStarted);

    m_RequestContext->SetAppState(eDiagAppState_Request);

    try {
        SetJobRetCode(m_WorkerNode.GetJobProcessor()->Do(*this));
    }
    catch (CGridWorkerNodeException& ex) {
        switch (ex.GetErrCode()) {
        case CGridWorkerNodeException::eJobIsCanceled:
            x_SetCanceled();
            break;

        case CGridWorkerNodeException::eExclusiveModeIsAlreadySet:
            if (IsLogRequested()) {
                LOG_POST_X(21, "Job " << GetJobKey() <<
                    " will be returned back to the queue "
                    "because it requested exclusive mode while "
                    "another job already has the exclusive status.");
            }
            if (!IsJobCommitted())
                ReturnJob();
            break;

        default:
            ERR_POST_X(62, ex);
            if (!IsJobCommitted())
                ReturnJob();
        }
    }
    catch (CNetScheduleException& e) {
        ERR_POST_X(20, "job" << GetJobKey() << " failed: " << e);
        if (e.GetErrCode() == CNetScheduleException::eJobNotFound)
            x_SetCanceled();
        else if (!IsJobCommitted())
            CommitJobWithFailure(e.what());
    }
    catch (exception& e) {
        ERR_POST_X(18, "job" << GetJobKey() << " failed: " << e.what());
        if (!IsJobCommitted())
            CommitJobWithFailure(e.what());
    }

    CloseStreams();

    if (m_WorkerNode.IsExclusiveMode() && m_ExclusiveJob)
        m_WorkerNode.LeaveExclusiveMode();

    switch (GetCommitStatus()) {
    case eDone:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobSucceeded);
        break;

    case eNotCommitted:
        if (TWorkerNode_AllowImplicitJobReturn::GetDefault() ||
                GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown) {
            ReturnJob();
            m_WorkerNode.x_NotifyJobWatchers(*this,
                    IWorkerNodeJobWatcher::eJobReturned);
            break;
        }

        CommitJobWithFailure("Job was not explicitly committed");
        /* FALL THROUGH */

    case eFailure:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobFailed);
        break;

    case eReturn:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobReturned);
        break;

    case eRescheduled:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobRescheduled);
        break;

    default: // eCanceled - will be processed in x_SendJobResults().
        break;
    }

    GetWorkerNode().x_NotifyJobWatchers(*this,
            IWorkerNodeJobWatcher::eJobStopped);

    if (!CGridGlobals::GetInstance().IsShuttingDown())
        static_cast<CWorkerNodeJobCleanup*>(
                GetCleanupEventSource())->CallEventHandlers();

    m_WorkerNode.m_JobCommitterThread->RecycleJobContextAndCommitJob(this);
}

void* CMainLoopThread::Main()
{
    CDeadline max_wait_for_servers(
            TWorkerNode_MaxWaitForServers::GetDefault());

    auto_ptr<CWorkerNodeJobContext> job_context(
            m_WorkerNode->m_JobCommitterThread->AllocJobContext());

    unsigned try_count = 0;
    while (!CGridGlobals::GetInstance().IsShuttingDown()) {
        try {
            try {
                m_WorkerNode->m_ThreadPool->WaitForRoom(
                        m_WorkerNode->m_ThreadPoolTimeout);
            }
            catch (CBlockingQueueException&) {
                // threaded pool is busy
                continue;
            }

            if (m_WorkerNode->x_GetNextJob(job_context->GetJob())) {
                job_context->Reset();

                try {
                    m_WorkerNode->m_ThreadPool->AcceptRequest(CRef<CStdRequest>(
                            new CWorkerNodeRequest(job_context.get())));
                }
                catch (CBlockingQueueException& ex) {
                    ERR_POST_X(28, ex);
                    // that must not happen after CBlockingQueue is fixed
                    _ASSERT(0);
                    job_context->ReturnJob();
                    m_WorkerNode->m_JobCommitterThread->
                            RecycleJobContextAndCommitJob(job_context.get());
                }
                job_context.release();
                job_context.reset(
                        m_WorkerNode->m_JobCommitterThread->AllocJobContext());
            }
            max_wait_for_servers =
                CDeadline(TWorkerNode_MaxWaitForServers::GetDefault());
        }
        catch (CNetSrvConnException& e) {
            SleepMilliSec(s_GetRetryDelay());
            if (e.GetErrCode() == CNetSrvConnException::eConnectionFailure &&
                    !max_wait_for_servers.GetRemainingTime().IsZero())
                continue;
            ERR_POST(Critical << "Could not connect to the "
                    "configured servers, exiting...");
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
        }
        catch (CNetServiceException& ex) {
            ERR_POST_X(40, ex);
            if (++try_count >= TServConn_ConnMaxRetries::GetDefault()) {
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            } else {
                SleepMilliSec(s_GetRetryDelay());
                continue;
            }
        }
        catch (exception& ex) {
            ERR_POST_X(29, ex.what());
            if (TWorkerNode_StopOnJobErrors::GetDefault()) {
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            }
        }
        try_count = 0;
    }

    return NULL;
}

bool CGridWorkerNode::x_GetJobWithAffinityList(SNetServerImpl* server,
        const CDeadline* timeout, CNetScheduleJob& job,
        CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
        const string& affinity_list)
{
    return m_NSExecutor->ExecGET(server, m_NSExecutor->
            m_NotificationHandler.CmdAppendTimeoutAndClientInfo(
                    CNetScheduleNotificationHandler::MkBaseGETCmd(
                            affinity_preference, affinity_list),
                    timeout), job);
}

bool CGridWorkerNode::x_GetJobWithAffinityLadder(SNetServerImpl* server,
        const CDeadline* timeout, CNetScheduleJob& job)
{
    if (m_NSExecutor->m_API->m_AffinityLadder.empty())
        return x_GetJobWithAffinityList(server, timeout, job,
                m_NSExecutor->m_AffinityPreference, kEmptyStr);

    list<string>::const_iterator it =
            m_NSExecutor->m_API->m_AffinityLadder.begin();

    for (;;) {
        string affinity_list = *it;
        if (++it == m_NSExecutor->m_API->m_AffinityLadder.end())
            return x_GetJobWithAffinityList(server, timeout, job,
                    m_NSExecutor->m_AffinityPreference, affinity_list);
        else if (x_GetJobWithAffinityList(server, NULL, job,
                CNetScheduleExecutor::ePreferredAffinities, affinity_list))
            return true;
    }
}

CGridWorkerNode::SNotificationTimelineEntry*
    CGridWorkerNode::x_GetTimelineEntry(SNetServerImpl* server_impl)
{
    m_TimelineSearchPattern.m_ServerAddress =
            server_impl->m_ServerInPool->m_Address;

    TTimelineEntries::iterator it(
            m_TimelineEntryByAddress.find(&m_TimelineSearchPattern));

    if (it != m_TimelineEntryByAddress.end())
        return *it;

    SNotificationTimelineEntry* new_entry = new SNotificationTimelineEntry(
            m_TimelineSearchPattern.m_ServerAddress, m_DiscoveryIteration);

    m_TimelineEntryByAddress.insert(new_entry);

    return new_entry;
}

// Action on a *detached* timeline entry.
bool CGridWorkerNode::x_PerformTimelineAction(
        CGridWorkerNode::SNotificationTimelineEntry* timeline_entry,
        CNetScheduleJob& job)
{
    if (timeline_entry->IsDiscoveryAction()) {
        if (!x_EnterSuspendedState()) {
            ++m_DiscoveryIteration;
            for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
                    CNetService::eIncludePenalized); it; ++it) {
                SNotificationTimelineEntry* srv_entry = x_GetTimelineEntry(*it);
                srv_entry->m_DiscoveryIteration = m_DiscoveryIteration;
                if (!srv_entry->IsInTimeline())
                    m_ImmediateActions.Push(srv_entry);
            }
        }

        timeline_entry->ResetTimeout(m_NSTimeout);
        m_Timeline.Push(timeline_entry);
        return false;
    }

    if (x_EnterSuspendedState() ||
            // Skip servers that disappeared from LBSM.
            timeline_entry->m_DiscoveryIteration != m_DiscoveryIteration)
        return false;

    CNetServer server(m_NetScheduleAPI->m_Service.GetServer(
            timeline_entry->m_ServerAddress));

    timeline_entry->ResetTimeout(m_NSTimeout);

    try {
        if (x_GetJobWithAffinityLadder(server,
                &timeline_entry->GetTimeout(), job)) {
            // A job has been returned; add the server to
            // m_ImmediateActions because there can be more
            // jobs in the queue.
            m_ImmediateActions.Push(timeline_entry);
            return true;
        } else {
            // No job has been returned by this server;
            // query the server later.
            m_Timeline.Push(timeline_entry);
            return false;
        }
    }
    catch (CNetSrvConnException& e) {
        // Because a connection error has occurred, do not
        // put this server back to the timeline.
        LOG_POST(Warning << e.GetMsg());
        return false;
    }
}

bool CGridWorkerNode::x_EnterSuspendedState()
{
    if (CGridGlobals::GetInstance().IsShuttingDown())
        return true;

    void* event;

    while ((event = SwapPointers(&m_SuspendResumeEvent,
            NO_EVENT)) != NO_EVENT) {
        if (event == SUSPEND_EVENT) {
            if (!m_TimelineIsSuspended) {
                // Stop the timeline.
                m_TimelineIsSuspended = true;
                m_ImmediateActions.Clear();
                m_Timeline.Clear();
                m_TimelineSearchPattern.ResetTimeout(m_NSTimeout);
                m_Timeline.Push(&m_TimelineSearchPattern);
            }
        } else { /* event == RESUME_EVENT */
            if (m_TimelineIsSuspended) {
                // Resume the timeline.
                m_TimelineIsSuspended = false;
                m_TimelineSearchPattern.MoveTo(&m_ImmediateActions);
            }
        }
    }

    return m_TimelineIsSuspended;
}

void CGridWorkerNode::x_ProcessRequestJobNotification()
{
    if (!x_EnterSuspendedState()) {
        CNetServer server;

        if (m_NSExecutor->m_NotificationHandler.CheckRequestJobNotification(
                m_NSExecutor, &server))
            x_GetTimelineEntry(server)->MoveTo(&m_ImmediateActions);
    }
}

bool CGridWorkerNode::x_WaitForNewJob(CNetScheduleJob& job)
{
    for (;;) {
        while (!m_ImmediateActions.IsEmpty()) {
            if (x_PerformTimelineAction(m_ImmediateActions.Shift(), job))
                return true;

            while (!m_Timeline.IsEmpty() && m_Timeline.GetHead()->TimeHasCome())
                m_ImmediateActions.Push(m_Timeline.Shift());

            // Check if there's a notification in the UDP socket.
            while (m_NSExecutor->m_NotificationHandler.ReceiveNotification())
                x_ProcessRequestJobNotification();
        }

        if (CGridGlobals::GetInstance().IsShuttingDown())
            return false;

        if (!m_Timeline.IsEmpty()) {
            if (m_NSExecutor->m_NotificationHandler.WaitForNotification(
                    m_Timeline.GetHead()->GetTimeout()))
                x_ProcessRequestJobNotification();
            else if (x_PerformTimelineAction(m_Timeline.Shift(), job))
                return true;
        }
    }
}

bool CGridWorkerNode::x_GetNextJob(CNetScheduleJob& job)
{
    if (!x_AreMastersBusy()) {
        SleepSec(m_NSTimeout);
        return false;
    }

    if (!WaitForExclusiveJobToFinish())
        return false;

    bool job_exists = x_WaitForNewJob(job);

    if (job_exists && job.mask & CNetScheduleAPI::eExclusiveJob) {
        if (EnterExclusiveMode())
            job_exists = true;
        else {
            x_ReturnJob(job);
            job_exists = false;
        }
    }
    if (job_exists && x_EnterSuspendedState()) {
        x_ReturnJob(job);
        return false;
    }
    return job_exists;
}

void CGridWorkerNode::x_ReturnJob(const CNetScheduleJob& job)
{
    GetNSExecutor().ReturnJob(job.job_id, job.auth_token);
}

size_t CGridWorkerNode::GetServerOutputSize()
{
    return m_QueueEmbeddedOutputSize;
}

bool CGridWorkerNode::EnterExclusiveMode()
{
    if (m_ExclusiveJobSemaphore.TryWait()) {
        _ASSERT(!m_IsProcessingExclusiveJob);

        m_IsProcessingExclusiveJob = true;
        return true;
    }
    return false;
}

void CGridWorkerNode::LeaveExclusiveMode()
{
    _ASSERT(m_IsProcessingExclusiveJob);

    m_IsProcessingExclusiveJob = false;
    m_ExclusiveJobSemaphore.Post();
}

bool CGridWorkerNode::WaitForExclusiveJobToFinish()
{
    if (m_ExclusiveJobSemaphore.TryWait(m_NSTimeout)) {
        m_ExclusiveJobSemaphore.Post();
        return true;
    }
    return false;
}

END_NCBI_SCOPE
