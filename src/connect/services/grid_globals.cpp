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
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/grid_globals.hpp>
#include <connect/services/error_codes.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_safe_static.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

#define RESOURCE_OVERUSE_EXIT_CODE 100

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeStatictics
/// @internal
CWNJobWatcher::CWNJobWatcher()
    : m_JobsStarted(0), m_JobsSucceeded(0), m_JobsFailed(0), m_JobsReturned(0),
      m_JobsCanceled(0), m_JobsLost(0),
      m_MaxJobsAllowed(0), m_MaxFailuresAllowed(0),
      m_InfiniteLoopTime(0)
{
}
CWNJobWatcher::~CWNJobWatcher()
{
}

void CWNJobWatcher::Notify(const CWorkerNodeJobContext& job_context,
                            EEvent event)
{
    switch (event) {
    case eJobStarted:
        {
            CMutexGuard guard(m_ActiveJobsMutex);
            m_ActiveJobs[const_cast<CWorkerNodeJobContext*>(&job_context)] =
                    SJobActivity();
            ++m_JobsStarted;
            if (m_MaxJobsAllowed > 0 && m_JobsStarted > m_MaxJobsAllowed - 1) {
                LOG_POST_X(1, "The maximum number of allowed jobs (" <<
                              m_MaxJobsAllowed << ") has been reached. "
                              "Sending the shutdown request." );
                CGridGlobals::GetInstance().
                    RequestShutdown(CNetScheduleAdmin::eNormalShutdown);
            }
        }
        break;
    case eJobStopped:
        {
            CMutexGuard guard(m_ActiveJobsMutex);
            m_ActiveJobs.erase(
                    const_cast<CWorkerNodeJobContext*>(&job_context));
        }
        break;
    case eJobFailed:
        ++m_JobsFailed;
        if (m_MaxFailuresAllowed > 0 &&
                m_JobsFailed > m_MaxFailuresAllowed - 1) {
            LOG_POST_X(2, "The maximum number of failed jobs (" <<
                          m_MaxFailuresAllowed << ") has been reached. "
                          "Shutting down..." );
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        }
        break;
    case eJobSucceeded:
        ++m_JobsSucceeded;
        break;
    case eJobReturned:
        ++m_JobsReturned;
        break;
    case eJobCanceled:
        ++m_JobsCanceled;
        break;
    case eJobLost:
        ++m_JobsLost;
        break;
    }

    if (event != eJobStarted) {
        CGridWorkerNode& worker_node = job_context.GetWorkerNode();
        Uint8 total_memory_limit = worker_node.GetTotalMemoryLimit();
        if (total_memory_limit > 0) {  // memory check requested
            size_t memory_usage;
            if (!GetMemoryUsage(&memory_usage, 0, 0)) {
                ERR_POST("Could not check self memory usage" );
            } else if (memory_usage > total_memory_limit) {
                ERR_POST(Warning << "Memory usage (" << memory_usage <<
                    ") is above the configured limit (" <<
                    total_memory_limit << ")");

                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eNormalShutdown,
                        RESOURCE_OVERUSE_EXIT_CODE);
            }
        }

        int total_time_limit = worker_node.GetTotalTimeLimit();
        if (total_time_limit > 0 &&  // time check requested
                time(0) > worker_node.GetStartupTime() + total_time_limit)
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eNormalShutdown, RESOURCE_OVERUSE_EXIT_CODE);
    }
}

void CWNJobWatcher::Print(CNcbiOstream& os) const
{
    os << "Started: " <<
                    CGridGlobals::GetInstance().GetStartTime().AsString() <<
            "\nJobs Succeeded: " << m_JobsSucceeded <<
            "\nJobs Failed: " << m_JobsFailed  <<
            "\nJobs Returned: " << m_JobsReturned <<
            "\nJobs Canceled: " << m_JobsCanceled <<
            "\nJobs Lost: " << m_JobsLost << "\n";

    CMutexGuard guard(m_ActiveJobsMutex);
    os << "Jobs Running: " << m_ActiveJobs.size() << "\n";
    ITERATE(TActiveJobs, it, m_ActiveJobs) {
        os << it->first->GetJobKey() << " " << it->first->GetJobInput()
           << " -- running for " << (int)it->second.elasped_time.Elapsed()
           << " seconds.";
        if (it->second.flag)
            os << "!!! LONG RUNNING JOB !!!";
        os << "\n";
    }
}

void CWNJobWatcher::CheckForInfiniteLoop()
{
    if (m_InfiniteLoopTime > 0) {
        size_t count = 0;
        CMutexGuard guard(m_ActiveJobsMutex);
        NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
            if (!it->second.flag) {
                if ( it->second.elasped_time.Elapsed() > m_InfiniteLoopTime) {
                    ERR_POST_X(3, "An infinite loop is detected in job "
                                  << it->first->GetJobKey());
                    it->second.flag = true;
                    CGridGlobals::GetInstance().
                        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
                }
            } else
                ++count;
        }
        if( count > 0 && count == m_ActiveJobs.size()) {
            ERR_POST_X(4, "All jobs are in infinite loops. "
                          "Server is shutting down.");
            CGridGlobals::GetInstance().KillNode();
        }
    }
}

void CWNJobWatcher::x_KillNode(CGridWorkerNode& worker)
{
    CMutexGuard guard(m_ActiveJobsMutex);
    NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
        CNetScheduleJob& job = it->first->GetJob();
        if (!it->second.flag) {
            worker.x_ReturnJob(job);
        } else {
            job.error_msg = "Job execution time exceeded " +
                    NStr::NumericToString(
                            unsigned(it->second.elasped_time.Elapsed()));
            job.error_msg += " seconds.";
            worker.GetNSExecutor().PutFailure(job);
        }
    }
    TPid cpid = CProcess::GetCurrentPid();
    CProcess(cpid).Kill();
}


/////////////////////////////////////////////////////////////////////////////
//
CGridGlobals::CGridGlobals() :
    m_ReuseJobObject(false),
    m_ShutdownLevel(CNetScheduleAdmin::eNoShutdown),
    m_ExitCode(0),
    m_StartTime(GetFastLocalTime()),
    m_Worker(NULL),
    m_UDPPort(0)
{
}

CGridGlobals::~CGridGlobals()
{
}

/* static */
CGridGlobals& CGridGlobals::GetInstance()
{
    static CSafeStatic<CGridGlobals> global_instance;

    return global_instance.Get();
}


unsigned int CGridGlobals::GetNewJobNumber()
{
    return (unsigned) m_JobsStarted.Add(1);
}

CWNJobWatcher& CGridGlobals::GetJobWatcher()
{
    if (!m_JobWatcher.get())
        m_JobWatcher.reset(new CWNJobWatcher);
    return *m_JobWatcher;
}

void CGridGlobals::KillNode()
{
    _ASSERT(m_Worker);
    if (m_Worker)
        GetJobWatcher().x_KillNode(*m_Worker);
}

void CGridGlobals::x_InterruptUDPPortListening()
{
    if (m_UDPPort != 0)
        CDatagramSocket().Send("SHUTDOWN", sizeof("SHUTDOWN"),
                "127.0.0.1", m_UDPPort);
}

END_NCBI_SCOPE
