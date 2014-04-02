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

CWorkerNodeJobContext* CJobCommitterThread::AllocJobContext()
{
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    if (m_JobContextPool.IsEmpty())
        return new CWorkerNodeJobContext(*m_WorkerNode);

    CWorkerNodeJobContext* job_context = m_JobContextPool.Shift();

    job_context->m_Job.Reset();

    return job_context;
}

void CJobCommitterThread::PutJobContextBackAndCommitJob(
        CWorkerNodeJobContext* job_context)
{
    job_context->m_FirstCommitAttempt = true;

    TFastMutexGuard mutex_lock(m_TimelineMutex);

    // Must be called prior to adding the job context to
    // m_ImmediateActions: when empty, m_ImmediateActions
    // indicates that the committer thread is waiting and
    // the semaphore must be incremented.
    WakeUp();

    m_ImmediateActions.Push(job_context);
}

void CJobCommitterThread::Stop()
{
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    WakeUp();
}

void* CJobCommitterThread::Main()
{
    TFastMutexGuard mutex_lock(m_TimelineMutex);

    do {
        if (m_Timeline.IsEmpty()) {
            TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

            m_Semaphore.Wait();
        } else {
            unsigned sec, nanosec;

            m_Timeline.GetHead()->GetTimeout().GetRemainingTime().GetNano(&sec,
                    &nanosec);

            if (sec == 0 && nanosec == 0)
                m_ImmediateActions.Push(m_Timeline.Shift());
            else {
                bool wait_interrupted;
                {
                    TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

                    wait_interrupted = m_Semaphore.TryWait(sec, nanosec);
                }
                if (!wait_interrupted)
                    m_ImmediateActions.Push(m_Timeline.Shift());
            }
        }

        while (!m_ImmediateActions.IsEmpty()) {
            // Do not remove the job context from m_ImmediateActions
            // prior to calling x_CommitJob() to avoid race conditions
            // (otherwise, the semaphore can be Post()'ed multiple times
            // by the worker threads while this thread is in x_CommitJob()).
            CWorkerNodeJobContext* job_context = m_ImmediateActions.GetHead();
            if (x_CommitJob(job_context))
                m_JobContextPool.Push(m_ImmediateActions.Shift());
            else
                m_Timeline.Push(m_ImmediateActions.Shift());
        }
    } while (!CGridGlobals::GetInstance().IsShuttingDown());

    while (!m_Timeline.IsEmpty())
        delete m_Timeline.Shift();

    while (!m_JobContextPool.IsEmpty())
        delete m_JobContextPool.Shift();

    return NULL;
}

bool CJobCommitterThread::x_CommitJob(CWorkerNodeJobContext* job_context)
{
    TFastMutexUnlockGuard mutext_unlock(m_TimelineMutex);

    CRequestContextSwitcher request_state_guard(job_context->m_RequestContext);

    bool recycle_job_context = false;

    try {
        switch (job_context->GetCommitStatus()) {
        case CWorkerNodeJobContext::eDone:
            m_WorkerNode->GetNSExecutor().PutResult(job_context->GetJob());
            break;

        case CWorkerNodeJobContext::eFailure:
            m_WorkerNode->GetNSExecutor().PutFailure(job_context->GetJob());
            break;

        case CWorkerNodeJobContext::eReturn:
            m_WorkerNode->x_ReturnJob(job_context->GetJob());
            break;

        default: // Always eCanceled; eNotCommitted is overridden in x_RunJob().
            ERR_POST("Job " << job_context->GetJob().job_id <<
                    " has been canceled");
        }

        recycle_job_context = true;
    }
    catch (CNetScheduleException& e) {
        ERR_POST_X(65, "Could not commit " <<
                job_context->m_Job.job_id << ": " << e.what());
        recycle_job_context = true;
    }
    catch (exception& e) {
        unsigned commit_interval = m_WorkerNode->GetCommitJobInterval();
        job_context->ResetTimeout(commit_interval);
        if (job_context->m_FirstCommitAttempt) {
            job_context->m_FirstCommitAttempt = false;
            job_context->m_CommitExpiration =
                    CDeadline(m_WorkerNode->GetQueueTimeout(), 0);
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

    if (recycle_job_context)
        job_context->x_PrintRequestStop();

    return recycle_job_context;
}

/// @internal
IWorkerNodeJob* CGridWorkerNode::GetJobProcessor()
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
