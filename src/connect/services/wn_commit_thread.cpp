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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "wn_commit_thread.hpp"
#include "grid_worker_impl.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/error_codes.hpp>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
///@internal
static void s_TlsCleanup(IWorkerNodeJob* p_value, void* /* data */ )
{
    if (p_value != NULL)
        p_value->RemoveReference();
}
/// @internal
static CStaticTls<IWorkerNodeJob> s_tls;

CJobCommitterThread::CJobCommitterThread(SGridWorkerNodeImpl* worker_node) :
    m_WorkerNode(worker_node),
    m_Semaphore(0, 1),
    m_ThreadName(worker_node->GetAppName() + "_cm")
{
}

CWorkerNodeJobContext CJobCommitterThread::AllocJobContext()
{
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    if (m_JobContextPool.empty())
        return new SWorkerNodeJobContextImpl(m_WorkerNode);

    CWorkerNodeJobContext job_context(m_JobContextPool.front());
    m_JobContextPool.pop_front();

    job_context->m_Job.Reset();
    job_context->m_JobGeneration = m_WorkerNode->m_CurrentJobGeneration;

    return job_context;
}

void CJobCommitterThread::RecycleJobContextAndCommitJob(
        SWorkerNodeJobContextImpl* job_context,
        CRequestContextSwitcher& rctx_switcher)
{
    job_context->m_FirstCommitAttempt = true;

    TFastMutexGuard mutex_lock(m_TimelineMutex);

    // Must be called prior to adding the job context to
    // m_ImmediateActions: when empty, m_ImmediateActions
    // indicates that the committer thread is waiting and
    // the semaphore must be incremented.
    WakeUp();

    m_ImmediateActions.push_back(TEntry(job_context));

    // We must do it here, before m_TimelineMutex is unlocked
    rctx_switcher.Release();
}

void CJobCommitterThread::Stop()
{
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    WakeUp();
}

void* CJobCommitterThread::Main()
{
    SetCurrentThreadName(m_ThreadName);
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    do {
        if (m_Timeline.empty()) {
            TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

            m_Semaphore.Wait();
        } else {
            unsigned sec, nanosec;

            m_Timeline.front()->GetTimeout().GetRemainingTime().GetNano(&sec,
                    &nanosec);

            if (sec == 0 && nanosec == 0) {
                m_ImmediateActions.push_back(m_Timeline.front());
                m_Timeline.pop_front();
            } else {
                bool wait_interrupted;
                {
                    TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

                    wait_interrupted = m_Semaphore.TryWait(sec, nanosec);
                }
                if (!wait_interrupted) {
                    m_ImmediateActions.push_back(m_Timeline.front());
                    m_Timeline.pop_front();
                }
            }
        }

        while (!m_ImmediateActions.empty()) {
            TEntry& entry = m_ImmediateActions.front();

            // Do not remove the job context from m_ImmediateActions
            // prior to calling x_CommitJob() to avoid race conditions
            // (otherwise, the semaphore can be Post()'ed multiple times
            // by the worker threads while this thread is in x_CommitJob()).
            if (x_CommitJob(entry)) {
                m_JobContextPool.push_back(entry);
            } else {
                m_Timeline.push_back(entry);
            }

            m_ImmediateActions.pop_front();
        }
    } while (!CGridGlobals::GetInstance().IsShuttingDown());

    return NULL;
}

bool CJobCommitterThread::x_CommitJob(SWorkerNodeJobContextImpl* job_context)
{
    TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

    CRequestContextSwitcher request_state_guard(job_context->m_RequestContext);

    bool recycle_job_context = false;

    try {
        switch (job_context->m_JobCommitStatus) {
        case CWorkerNodeJobContext::eCS_Done:
            m_WorkerNode->m_NSExecutor.PutResult(job_context->m_Job);
            break;

        case CWorkerNodeJobContext::eCS_Failure:
            m_WorkerNode->m_NSExecutor.PutFailure(job_context->m_Job,
                    job_context->m_DisableRetries);
            break;

        default: /* eCS_NotCommitted */
            // In the unlikely event of eCS_NotCommitted, return the job.
            /* FALL THROUGH */

        case CWorkerNodeJobContext::eCS_Return:
            m_WorkerNode->m_NSExecutor.ReturnJob(job_context->m_Job);
            break;

        case CWorkerNodeJobContext::eCS_Reschedule:
            m_WorkerNode->m_NSExecutor.Reschedule(job_context->m_Job);
            break;

        case CWorkerNodeJobContext::eCS_JobIsLost:
            // Job is cancelled or otherwise taken away from the worker
            // node. Whatever the cause is, it has been reported already.
            break;
        }

        recycle_job_context = true;
    }
    catch (CNetScheduleException& e) {
        ERR_POST_X(65, "Could not commit " <<
                job_context->m_Job.job_id << ": " << e.what());
        recycle_job_context = true;
    }
    catch (exception& e) {
        unsigned commit_interval = m_WorkerNode->m_CommitJobInterval;
        job_context->ResetTimeout(commit_interval);
        if (job_context->m_FirstCommitAttempt) {
            job_context->m_FirstCommitAttempt = false;
            job_context->m_CommitExpiration =
                    CDeadline(m_WorkerNode->m_QueueTimeout, 0);
        } else if (job_context->m_CommitExpiration <
                job_context->GetTimeout()) {
            ERR_POST_X(64, "Could not commit " <<
                    job_context->m_Job.job_id << ": " << e.what());
            recycle_job_context = true;
        }
        if (!recycle_job_context) {
            ERR_POST_X(63, "Error while committing " <<
                    job_context->m_Job.job_id << ": " << e.what() <<
                    "; will retry in " << commit_interval << " seconds.");
        }
    }

    m_WorkerNode->m_JobsInProgress.Remove(job_context->m_Job.job_id);

    if (recycle_job_context)
        job_context->x_PrintRequestStop();

    return recycle_job_context;
}

/// @internal
IWorkerNodeJob* SGridWorkerNodeImpl::GetJobProcessor()
{
    IWorkerNodeJob* ret = s_tls.GetValue();
    if (ret == NULL) {
        try {
            CFastMutexGuard guard(m_JobProcessorMutex);
            ret = m_JobProcessorFactory->CreateInstance();
        }
        catch (exception& e) {
            ERR_POST_X(9, "Could not create an instance of the "
                    "job processor class." << e.what());
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            throw;
        }
        if (ret == NULL) {
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            NCBI_THROW(CException, eUnknown,
                    "Could not create an instance of the job processor class.");
        }
        if (CGridGlobals::GetInstance().ReuseJobObject()) {
            s_tls.SetValue(ret, s_TlsCleanup);
            ret->AddReference();
        }
    }
    return ret;
}

END_NCBI_SCOPE
