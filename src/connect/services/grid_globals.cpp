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

#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeStatictics
/// @internal
CWNJobsWatcher::CWNJobsWatcher()
    : m_JobsStarted(0), m_JobsSucceed(0), m_JobsFailed(0), m_JobsReturned(0),
      m_JobsCanceled(0), m_JobsLost(0),
      m_MaxJobsAllowed(0), m_MaxFailuresAllowed(0),
      m_InfinitLoopTime(0)
{
}
CWNJobsWatcher::~CWNJobsWatcher()
{
}

void CWNJobsWatcher::Notify(const CWorkerNodeJobContext& job,
                            EEvent event)
{
    switch(event) {
    case eJobStarted :
        {
            CMutexGuard guard(m_ActiveJobsMutex);
            m_ActiveJobs[&job] = SJobActivity();
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
    case eJobStopped :
        {
            CMutexGuard guard(m_ActiveJobsMutex);
            m_ActiveJobs.erase(&job);
        }
        break;
    case eJobFailed :
        ++m_JobsFailed;
        if (m_MaxFailuresAllowed > 0 && m_JobsFailed > m_MaxFailuresAllowed - 1) {
                LOG_POST_X(2, "The maximum number of failed jobs (" <<
                              m_MaxFailuresAllowed << ") has been reached. "
                              "Sending the shutdown request." );
                CGridGlobals::GetInstance().
                    RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            }
        break;
    case eJobSucceed :
        ++m_JobsSucceed;
        break;
    case eJobReturned :
        ++m_JobsReturned;
        break;
    case eJobCanceled :
        ++m_JobsCanceled;
        break;
    case eJobLost:
        ++m_JobsLost;
        break;
    }
}

void CWNJobsWatcher::Print(CNcbiOstream& os) const
{
    os << "Started: " << CGridGlobals::GetInstance()
                              .GetStartTime().AsString() << endl;
    os << "Jobs Succeeded: " << m_JobsSucceed << endl
       << "Jobs Failed: "  << m_JobsFailed  << endl
       << "Jobs Returned: "<< m_JobsReturned << endl
       << "Jobs Canceled: "<< m_JobsCanceled << endl
       << "Jobs Lost: "    << m_JobsLost << endl;

    CMutexGuard guard(m_ActiveJobsMutex);
    os << "Jobs Running: " << m_ActiveJobs.size() << endl;
    ITERATE(TActiveJobs, it, m_ActiveJobs) {
        os << it->first->GetJobKey() << " " << it->first->GetJobInput()
           << " -- running for " << (int)it->second.elasped_time.Elapsed()
           << " seconds.";
        if (it->second.flag)
            os << "!!! INFINITE LOOP !!!";
        os << endl;
    }
}

void CWNJobsWatcher::CheckInfinitLoop()
{
    if (m_InfinitLoopTime > 0) {
        size_t count = 0;
        CMutexGuard guard(m_ActiveJobsMutex);
        NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
            if (!it->second.flag) {
                if ( it->second.elasped_time.Elapsed() > m_InfinitLoopTime) {
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

void CWNJobsWatcher::x_KillNode(CGridWorkerNode& worker)
{
    CMutexGuard guard(m_ActiveJobsMutex);
    NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
        const CNetScheduleJob& job = it->first->GetJob();
        if (!it->second.flag) {
            worker.x_ReturnJob(job.job_id, job.auth_token);
        } else {
            worker.x_FailJob(job,
                "An long running job has been detected after " +
                NStr::IntToString((int)it->second.elasped_time.Elapsed()) +
                " seconds of execution.");
        }
    }
    TPid cpid = CProcess::GetCurrentPid();
    CProcess(cpid).Kill();
}


/////////////////////////////////////////////////////////////////////////////
//
auto_ptr<CGridGlobals> CGridGlobals::sm_Instance;

CGridGlobals::CGridGlobals() :
      m_ReuseJobObject(false),
      m_ShutdownLevel(CNetScheduleAdmin::eNoShutdown),
      m_StartTime(GetFastLocalTime()),
      m_Worker(NULL)
{
}

CGridGlobals::~CGridGlobals()
{
}

/* static */
CGridGlobals& CGridGlobals::GetInstance()
{
    if ( !sm_Instance.get() )
        sm_Instance.reset(new CGridGlobals);
    return *sm_Instance;
}


unsigned int CGridGlobals::GetNewJobNumber()
{
    return (unsigned int)m_JobsStarted.Add(1);
}

CWNJobsWatcher& CGridGlobals::GetJobsWatcher()
{
    if (!m_JobsWatcher.get())
        m_JobsWatcher.reset(new CWNJobsWatcher);
    return *m_JobsWatcher;
}

void CGridGlobals::KillNode()
{
    _ASSERT(m_Worker);
    if( m_Worker )
        GetJobsWatcher().x_KillNode(*m_Worker);
}

END_NCBI_SCOPE
