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

const CNetScheduleJob& CWorkerNodeJobContext::GetJob() const
{
    return m_Impl->m_Job;
}

CNetScheduleJob& CWorkerNodeJobContext::GetJob()
{
    return m_Impl->m_Job;
}

const string& CWorkerNodeJobContext::GetJobKey() const
{
    return m_Impl->m_Job.job_id;
}

const string& CWorkerNodeJobContext::GetJobInput() const
{
    return m_Impl->m_Job.input;
}

void CWorkerNodeJobContext::SetJobOutput(const string& output)
{
    m_Impl->m_Job.output = output;
}

void CWorkerNodeJobContext::SetJobRetCode(int ret_code)
{
    m_Impl->m_Job.ret_code = ret_code;
}

size_t CWorkerNodeJobContext::GetInputBlobSize() const
{
    return m_Impl->m_InputBlobSize;
}

const string& CWorkerNodeJobContext::GetJobOutput() const
{
    return m_Impl->m_Job.output;
}

CNetScheduleAPI::TJobMask CWorkerNodeJobContext::GetJobMask() const
{
    return m_Impl->m_Job.mask;
}

unsigned int CWorkerNodeJobContext::GetJobNumber() const
{
    return m_Impl->m_JobNumber;
}

bool CWorkerNodeJobContext::IsJobCommitted() const
{
    return m_Impl->m_JobCommitStatus != eCS_NotCommitted;
}

CWorkerNodeJobContext::ECommitStatus
        CWorkerNodeJobContext::GetCommitStatus() const
{
    return m_Impl->m_JobCommitStatus;
}

bool CWorkerNodeJobContext::IsJobLost() const
{
    return m_Impl->m_JobCommitStatus == eCS_JobIsLost;
}

IWorkerNodeCleanupEventSource* CWorkerNodeJobContext::GetCleanupEventSource()
{
    return m_Impl->m_CleanupEventSource;
}

CGridWorkerNode CWorkerNodeJobContext::GetWorkerNode() const
{
    return m_Impl->m_WorkerNode;
}

SWorkerNodeJobContextImpl::SWorkerNodeJobContextImpl(
        SGridWorkerNodeImpl* worker_node) :
    m_WorkerNode(worker_node),
    m_CleanupEventSource(
            new CWorkerNodeJobCleanup(worker_node->m_CleanupEventSource)),
    m_RequestContext(new CRequestContext),
    m_StatusThrottler(1, CTimeSpan(worker_node->m_CheckStatusPeriod, 0)),
    m_ProgressMsgThrottler(1),
    m_NetScheduleExecutor(worker_node->m_NSExecutor),
    m_NetCacheAPI(worker_node->m_NetCacheAPI),
    m_JobGeneration(worker_node->m_CurrentJobGeneration),
    m_CommitExpiration(0, 0),
    m_Deadline(0, 0)
{
}

const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_Impl->m_WorkerNode->GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName() const
{
    return m_Impl->m_WorkerNode->GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream()
{
    IReader* reader = new CStringOrBlobStorageReader(GetJobInput(),
            m_Impl->m_NetCacheAPI, &m_Impl->m_InputBlobSize);
    m_Impl->m_RStream.reset(new CRStream(reader, 0, 0,
            CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));
    m_Impl->m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_Impl->m_RStream;
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    m_Impl->m_Writer.reset(new CStringOrBlobStorageWriter(
            m_Impl->m_WorkerNode->m_QueueEmbeddedOutputSize,
                    m_Impl->m_NetCacheAPI, GetJob().output));
    m_Impl->m_WStream.reset(new CWStream(m_Impl->m_Writer.get(), 0, 0,
            CRWStreambuf::fLeakExceptions));
    m_Impl->m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_Impl->m_WStream;
}

void CWorkerNodeJobContext::CloseStreams()
{
    try {
        m_Impl->m_ProgressMsgThrottler.Reset(1);
        m_Impl->m_StatusThrottler.Reset(1,
                CTimeSpan(m_Impl->m_WorkerNode->m_CheckStatusPeriod, 0));

        m_Impl->m_RStream.reset();
        m_Impl->m_WStream.reset();

        if (m_Impl->m_Writer.get() != NULL) {
            m_Impl->m_Writer->Close();
            m_Impl->m_Writer.reset();
        }
    }
    NCBI_CATCH_ALL_X(61, "Could not close IO streams");
}

void CWorkerNodeJobContext::CommitJob()
{
    m_Impl->CheckIfJobIsLost();
    m_Impl->m_JobCommitStatus = eCS_Done;
}

void CWorkerNodeJobContext::CommitJobWithFailure(const string& err_msg,
        bool no_retries)
{
    m_Impl->CheckIfJobIsLost();
    m_Impl->m_JobCommitStatus = eCS_Failure;
    m_Impl->m_DisableRetries = no_retries;
    m_Impl->m_Job.error_msg = err_msg;
}

void CWorkerNodeJobContext::ReturnJob()
{
    m_Impl->CheckIfJobIsLost();
    m_Impl->m_JobCommitStatus = eCS_Return;
}

void CWorkerNodeJobContext::RescheduleJob(
        const string& affinity, const string& group)
{
    m_Impl->CheckIfJobIsLost();
    m_Impl->m_JobCommitStatus = eCS_Reschedule;
    m_Impl->m_Job.affinity = affinity;
    m_Impl->m_Job.group = group;
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    m_Impl->PutProgressMessage(msg, send_immediately);
}

void SWorkerNodeJobContextImpl::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    CheckIfJobIsLost();
    if (!send_immediately &&
            !m_ProgressMsgThrottler.Approve(CRequestRateControl::eErrCode)) {
        LOG_POST(Warning << "Progress message \"" <<
                msg << "\" has been suppressed.");
        return;
    }

    if (m_WorkerNode->m_ProgressLogRequested) {
        LOG_POST(m_Job.job_id << " progress: " <<
                NStr::TruncateSpaces(msg, NStr::eTrunc_End));
    }

    try {
        if (m_Job.progress_msg.empty()) {
            m_NetScheduleExecutor.GetProgressMsg(m_Job);

            if (m_Job.progress_msg.empty()) {
                m_Job.progress_msg =
                        m_NetCacheAPI.PutData(msg.data(), msg.length());

                m_NetScheduleExecutor.PutProgressMsg(m_Job);

                return;
            }
        }

        m_NetCacheAPI.PutData(m_Job.progress_msg, msg.data(), msg.length());
    }
    catch (exception& ex) {
        ERR_POST_X(6, "Couldn't send a progress message: " << ex.what());
    }
}

void CWorkerNodeJobContext::JobDelayExpiration(unsigned runtime_inc)
{
    m_Impl->CheckIfJobIsLost();
    m_Impl->JobDelayExpiration(runtime_inc);
}

void SWorkerNodeJobContextImpl::JobDelayExpiration(unsigned runtime_inc)
{
    try {
        m_NetScheduleExecutor.JobDelayExpiration(m_Job, runtime_inc);
    }
    catch (exception& ex) {
        ERR_POST_X(8, "CWorkerNodeJobContext::JobDelayExpiration: " <<
                ex.what());
    }
}

bool CWorkerNodeJobContext::IsLogRequested() const
{
    return m_Impl->m_WorkerNode->m_LogRequested;
}

CNetScheduleAdmin::EShutdownLevel CWorkerNodeJobContext::GetShutdownLevel()
{
    return m_Impl->GetShutdownLevel();
}

CNetScheduleAdmin::EShutdownLevel SWorkerNodeJobContextImpl::GetShutdownLevel()
{
    if (m_StatusThrottler.Approve(CRequestRateControl::eErrCode))
        try {
            ENetScheduleQueuePauseMode pause_mode;
            CNetScheduleAPI::EJobStatus job_status =
                m_NetScheduleExecutor.GetJobStatus(m_Job, NULL, &pause_mode);
            switch (job_status) {
            case CNetScheduleAPI::eRunning:
                if (pause_mode == eNSQ_WithPullback) {
                    m_WorkerNode->SetJobPullbackTimer(
                            m_WorkerNode->m_DefaultPullbackTimeout);
                    LOG_POST("Pullback request from the server, "
                            "(default) pullback timeout=" <<
                            m_WorkerNode->m_DefaultPullbackTimeout);
                }
                /* FALL THROUGH */

            case CNetScheduleAPI::ePending:
                // NetSchedule will still allow to commit this job.
                break;

            default:
                // The worker node does not "own" the job any longer.
                ERR_POST("Cannot proceed with job processing: job '" <<
                        m_Job.job_id << "' changed status to '" <<
                        CNetScheduleAPI::StatusToString(job_status) << "'.");
                MarkJobAsLost();
                return CNetScheduleAdmin::eShutdownImmediate;
            }
        }
        catch(exception& ex) {
            ERR_POST("Cannot retrieve job status for " << m_Job.job_id <<
                    ": " << ex.what());
        }

    if (m_WorkerNode->CheckForPullback(m_JobGeneration)) {
        LOG_POST("Pullback timeout for " << m_Job.job_id);
        return CNetScheduleAdmin::eShutdownImmediate;
    }

    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void SWorkerNodeJobContextImpl::CheckIfJobIsLost()
{
    if (m_JobCommitStatus == CWorkerNodeJobContext::eCS_JobIsLost) {
        NCBI_THROW_FMT(CGridWorkerNodeException, eJobIsLost,
            "Job " << m_Job.job_id << " has been canceled");
    }
}

void SWorkerNodeJobContextImpl::ResetJobContext()
{
    m_JobNumber = CGridGlobals::GetInstance().GetNewJobNumber();

    m_JobCommitStatus = CWorkerNodeJobContext::eCS_NotCommitted;
    m_DisableRetries = false;
    m_InputBlobSize = 0;
    m_ExclusiveJob =
            (m_Job.mask & CNetScheduleAPI::eExclusiveJob) != 0;

    m_RequestContext->Reset();
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    if (!m_Impl->m_ExclusiveJob) {
        if (!m_Impl->m_WorkerNode->EnterExclusiveMode()) {
            NCBI_THROW(CGridWorkerNodeException,
                eExclusiveModeIsAlreadySet, "");
        }
        m_Impl->m_ExclusiveJob = true;
    }
}

const char* CWorkerNodeJobContext::GetCommitStatusDescription(
        CWorkerNodeJobContext::ECommitStatus commit_status)
{
    switch (commit_status) {
    case eCS_Done:
        return "done";
    case eCS_Failure:
        return "failed";
    case eCS_Return:
        return "returned";
    case eCS_Reschedule:
        return "rescheduled";
    case eCS_JobIsLost:
        return "lost";
    default:
        return "not committed";
    }
}

void SWorkerNodeJobContextImpl::x_PrintRequestStop()
{
    m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    if (!m_RequestContext->IsSetRequestStatus())
        m_RequestContext->SetRequestStatus(
            m_JobCommitStatus == CWorkerNodeJobContext::eCS_Done &&
                m_Job.ret_code == 0 ? 200 : 500);

    if (m_RequestContext->GetAppState() == eDiagAppState_Request)
        m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    if (g_IsRequestStopEventEnabled())
        GetDiagContext().PrintRequestStop();
}

void SWorkerNodeJobContextImpl::x_RunJob()
{
    CWorkerNodeJobContext this_job_context(this);

    m_RequestContext->SetRequestID((int) this_job_context.GetJobNumber());

    if (!m_Job.client_ip.empty())
        m_RequestContext->SetClientIP(m_Job.client_ip);

    if (!m_Job.session_id.empty())
        m_RequestContext->SetSessionID(m_Job.session_id);

    if (!m_Job.page_hit_id.empty())
        m_RequestContext->SetHitID(m_Job.page_hit_id);

    m_RequestContext->SetAppState(eDiagAppState_RequestBegin);

    CRequestContextSwitcher request_state_guard(m_RequestContext);

    if (g_IsRequestStartEventEnabled())
        GetDiagContext().PrintRequestStart().Print("jid", m_Job.job_id);

    m_RequestContext->SetAppState(eDiagAppState_Request);

    CJobRunRegistration client_ip_registration, session_id_registration;

    if (!m_Job.client_ip.empty() &&
            !m_WorkerNode->m_JobsPerClientIP.CountJob(m_Job.client_ip,
                    &client_ip_registration)) {
        ERR_POST("Too many jobs with client IP \"" <<
                 m_Job.client_ip << "\"; job " <<
                 m_Job.job_id << " will be returned.");
        m_JobCommitStatus = CWorkerNodeJobContext::eCS_Return;
    } else if (!m_Job.session_id.empty() &&
            !m_WorkerNode->m_JobsPerSessionID.CountJob(m_Job.session_id,
                    &session_id_registration)) {
        ERR_POST("Too many jobs with session ID \"" <<
                 m_Job.session_id << "\"; job " <<
                 m_Job.job_id << " will be returned.");
        m_JobCommitStatus = CWorkerNodeJobContext::eCS_Return;
    } else {
        m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                IWorkerNodeJobWatcher::eJobStarted);

        try {
            m_Job.ret_code =
                    m_WorkerNode->GetJobProcessor()->Do(this_job_context);
        }
        catch (CGridWorkerNodeException& ex) {
            switch (ex.GetErrCode()) {
            case CGridWorkerNodeException::eJobIsLost:
                break;

            case CGridWorkerNodeException::eExclusiveModeIsAlreadySet:
                if (m_WorkerNode->m_LogRequested) {
                    LOG_POST_X(21, "Job " << m_Job.job_id <<
                        " will be returned back to the queue "
                        "because it requested exclusive mode while "
                        "another exclusive job is already running.");
                }
                if (m_JobCommitStatus ==
                        CWorkerNodeJobContext::eCS_NotCommitted)
                    m_JobCommitStatus = CWorkerNodeJobContext::eCS_Return;
                break;

            default:
                ERR_POST_X(62, ex);
                if (m_JobCommitStatus ==
                        CWorkerNodeJobContext::eCS_NotCommitted)
                    m_JobCommitStatus = CWorkerNodeJobContext::eCS_Return;
            }
        }
        catch (CNetScheduleException& e) {
            ERR_POST_X(20, "job " << m_Job.job_id << " failed: " << e);
            if (e.GetErrCode() == CNetScheduleException::eJobNotFound) {
                ERR_POST("Cannot proceed with job processing: job '" <<
                        m_Job.job_id << "' has expired.");
                MarkJobAsLost();
            } else if (m_JobCommitStatus ==
                    CWorkerNodeJobContext::eCS_NotCommitted) {
                m_JobCommitStatus = CWorkerNodeJobContext::eCS_Failure;
                m_Job.error_msg = e.what();
            }
        }
        catch (exception& e) {
            ERR_POST_X(18, "job " << m_Job.job_id << " failed: " << e.what());
            if (m_JobCommitStatus == CWorkerNodeJobContext::eCS_NotCommitted) {
                m_JobCommitStatus = CWorkerNodeJobContext::eCS_Failure;
                m_Job.error_msg = e.what();
            }
        }

        this_job_context.CloseStreams();

        switch (m_JobCommitStatus) {
        case CWorkerNodeJobContext::eCS_Done:
            m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                    IWorkerNodeJobWatcher::eJobSucceeded);
            break;

        case CWorkerNodeJobContext::eCS_NotCommitted:
            if (TWorkerNode_AllowImplicitJobReturn::GetDefault() ||
                    this_job_context.GetShutdownLevel() !=
                            CNetScheduleAdmin::eNoShutdown) {
                m_JobCommitStatus = CWorkerNodeJobContext::eCS_Return;
                m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                        IWorkerNodeJobWatcher::eJobReturned);
                break;
            }

            m_JobCommitStatus = CWorkerNodeJobContext::eCS_Failure;
            m_Job.error_msg = "Job was not explicitly committed";
            /* FALL THROUGH */

        case CWorkerNodeJobContext::eCS_Failure:
            m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                    IWorkerNodeJobWatcher::eJobFailed);
            break;

        case CWorkerNodeJobContext::eCS_Return:
            m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                    IWorkerNodeJobWatcher::eJobReturned);
            break;

        case CWorkerNodeJobContext::eCS_Reschedule:
            m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                    IWorkerNodeJobWatcher::eJobRescheduled);
            break;

        default: // eCanceled - no action needed.
            // This object will be recycled in x_CommitJob().
            break;
        }

        m_WorkerNode->x_NotifyJobWatchers(this_job_context,
                IWorkerNodeJobWatcher::eJobStopped);
    }

    if (m_WorkerNode->IsExclusiveMode() && m_ExclusiveJob)
        m_WorkerNode->LeaveExclusiveMode();

    if (!CGridGlobals::GetInstance().IsShuttingDown())
        m_CleanupEventSource->CallEventHandlers();

    m_WorkerNode->m_JobCommitterThread->RecycleJobContextAndCommitJob(this,
            request_state_guard);
}

void* CMainLoopThread::Main()
{
    SetCurrentThreadName(m_ThreadName);
    m_Impl.Main();
    return NULL;
}

void CMainLoopThread::CImpl::Main()
{
    CDeadline max_wait_for_servers(
            TWorkerNode_MaxWaitForServers::GetDefault());

    CWorkerNodeJobContext job_context(
            m_WorkerNode->m_JobCommitterThread->AllocJobContext());

    CRequestContextSwitcher no_op;
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

            if (x_GetNextJob(job_context->m_Job)) {
                job_context->ResetJobContext();

                try {
                    m_WorkerNode->m_ThreadPool->AcceptRequest(CRef<CStdRequest>(
                            new CWorkerNodeRequest(job_context)));
                }
                catch (CBlockingQueueException& ex) {
                    ERR_POST_X(28, ex);
                    // that must not happen after CBlockingQueue is fixed
                    _ASSERT(0);
                    job_context->m_JobCommitStatus =
                            CWorkerNodeJobContext::eCS_Return;
                    m_WorkerNode->m_JobCommitterThread->
                            RecycleJobContextAndCommitJob(job_context, no_op);
                }
                job_context =
                        m_WorkerNode->m_JobCommitterThread->AllocJobContext();
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
}


CMainLoopThread::CImpl::EState CMainLoopThread::CImpl::CheckState()
{
    if (CGridGlobals::GetInstance().IsShuttingDown())
        return eStopped;

    EState ret = eWorking;

    do {
        void* event;

        while ((event = SwapPointers(&m_WorkerNode->m_SuspendResumeEvent,
                NO_EVENT)) != NO_EVENT) {
            if (event == SUSPEND_EVENT) {
                if (!m_WorkerNode->m_TimelineIsSuspended) {
                    // Stop the timeline.
                    m_WorkerNode->m_TimelineIsSuspended = true;
                    ret = eRestarted;
                }
            } else { /* event == RESUME_EVENT */
                if (m_WorkerNode->m_TimelineIsSuspended) {
                    // Resume the timeline.
                    m_WorkerNode->m_TimelineIsSuspended = false;
                }
            }
        }

        m_WorkerNode->m_NSExecutor->
            m_NotificationHandler.WaitForNotification(m_Timeout);
    } while (m_WorkerNode->m_TimelineIsSuspended);

    return ret;
}

CNetServer CMainLoopThread::CImpl::ReadNotifications()
{
    if (m_WorkerNode->m_NSExecutor->
            m_NotificationHandler.ReceiveNotification())
        return x_ProcessRequestJobNotification();

    return CNetServer();
}

CNetServer CMainLoopThread::CImpl::WaitForNotifications(const CDeadline& deadline)
{
     if (m_WorkerNode->m_NSExecutor->
             m_NotificationHandler.WaitForNotification(deadline)) {
        return x_ProcessRequestJobNotification();
     }

     return CNetServer();
}

CNetServer CMainLoopThread::CImpl::x_ProcessRequestJobNotification()
{
    CNetServer server;

    // No need to check state here, it will be checked before entry processing
    m_WorkerNode->m_NSExecutor->
        m_NotificationHandler.CheckRequestJobNotification(
                m_WorkerNode->m_NSExecutor, &server);

    return server;
}

bool MoreJobs()
{
    return true;
}

bool CMainLoopThread::CImpl::CheckEntry(
        CNetScheduleTimeline::SEntry& entry,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* /*job_status*/)
{
    CNetServer server(m_API.GetService().GetServer(entry.server_address));
    return m_WorkerNode->m_NSExecutor->x_GetJobWithAffinityLadder(server,
            m_Timeout, job);
}

CMainLoopThread::CImpl::EResult CMainLoopThread::CImpl::GetJob(
        const CDeadline& deadline,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status)
{
    for (;;) {
        for (;;) {
            EState state = CheckState();

            if (state == eStopped) {
                return eInterrupt;
            }
            
            if (state == eRestarted) {
                Restart();
            }

            if (!HasImmediateActions()) {
                break;
            }

            CNetScheduleTimeline::SEntry timeline_entry(PullImmediateAction());

            if (IsDiscoveryAction(timeline_entry)) {
                NextDiscoveryIteration(m_API);
                PushScheduledAction(timeline_entry, m_Timeout);
            } else {
                try {
                    if (CheckEntry(timeline_entry, job, job_status)) {
                        // A job has been returned; add the server to
                        // immediate actions because there can be more
                        // jobs in the queue.
                        PushImmediateAction(timeline_entry);
                        return eJob;
                    } else {
                        // No job has been returned by this server;
                        // query the server later.
                        PushScheduledAction(timeline_entry, m_Timeout);
                    }
                }
                catch (CNetSrvConnException& e) {
                    // Because a connection error has occurred, do not
                    // put this server back to the timeline.
                    LOG_POST(Warning << e.GetMsg());
                }
            }

            CheckScheduledActions();

            // Check if there's a notification in the UDP socket.
            while (CNetServer server = ReadNotifications()) {
                MoveToImmediateActions(server);
            }
        }

        if (!MoreJobs())
            return eNoJobs;

        if (deadline.IsExpired())
            return eAgain;

        // At least, the discovery action must be there
        _ASSERT(HasScheduledActions());

        // There's still time. Wait for notifications and query the servers.
        CDeadline next_event_time = GetNextTimeout();
        bool last_wait = deadline < next_event_time;
        if (last_wait) next_event_time = deadline;

        if (CNetServer server = WaitForNotifications(next_event_time)) {
            do {
                MoveToImmediateActions(server);
            } while (server = ReadNotifications());
        } else if (last_wait) {
            return eAgain;
        } else {
            PushImmediateAction(PullScheduledAction());
        }
    }
}

bool CMainLoopThread::CImpl::x_GetNextJob(CNetScheduleJob& job)
{
    if (!m_WorkerNode->x_AreMastersBusy()) {
        SleepSec(m_WorkerNode->m_NSTimeout);
        return false;
    }

    if (!m_WorkerNode->WaitForExclusiveJobToFinish())
        return false;

    if (GetJob(CTimeout::eInfinite, job, NULL) != eJob) {
        return false;
    }

    // Already executing this job, so do nothing
    // (and rely on that execution to report its result later)
    if (!m_WorkerNode->m_JobsInProgress.Add(job.job_id)) {
        return false;
    }

    if (job.mask & CNetScheduleAPI::eExclusiveJob) {
        if (!m_WorkerNode->EnterExclusiveMode()) {
            m_WorkerNode->m_NSExecutor.ReturnJob(job);
            return false;
        }
    }

    // No need to check for idleness here, running jobs won't be stopped anyway
    if (CGridGlobals::GetInstance().IsShuttingDown()) {
        m_WorkerNode->m_NSExecutor.ReturnJob(job);
        return false;
    }

    return true;
}

size_t CGridWorkerNode::GetServerOutputSize()
{
    return m_Impl->m_QueueEmbeddedOutputSize;
}

END_NCBI_SCOPE
