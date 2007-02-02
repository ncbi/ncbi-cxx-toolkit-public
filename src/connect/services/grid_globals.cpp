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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbidiag.hpp>
#include <connect/services/grid_globals.hpp>

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
            m_ActiveJobs[&job] = make_pair(CStopWatch(CStopWatch::eStart), false);
            ++m_JobsStarted;
            if (m_MaxJobsAllowed > 0 && m_JobsStarted > m_MaxJobsAllowed - 1) {
                LOG_POST(CTime(CTime::eCurrent).AsString() 
                         << " The maximum number of allowed jobs (" 
                         << m_MaxJobsAllowed << ") has been reached.\n" 
                         << "Sending the shutdown request." );
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
                LOG_POST(CTime(CTime::eCurrent).AsString() 
                         << " The maximum number of failed jobs (" 
                         << m_MaxFailuresAllowed << ") has been reached.\n" 
                         << "Sending the shutdown request." );
                CGridGlobals::GetInstance().
                    RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
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
    os << "Jobs Succeed: " << m_JobsSucceed << endl
       << "Jobs Failed: "  << m_JobsFailed  << endl
       << "Jobs Returned: "<< m_JobsReturned << endl
       << "Jobs Canceled: "<< m_JobsCanceled << endl
       << "Jobs Lost: "    << m_JobsLost << endl;
    
    CMutexGuard guard(m_ActiveJobsMutex);
    os << "Jobs Running: " << m_ActiveJobs.size() << endl;
    ITERATE(TActiveJobs, it, m_ActiveJobs) {
        os << it->first->GetJobKey() << " " << it->first->GetJobInput()
           << " -- running for " << (int)it->second.first.Elapsed()
           << " seconds.";
        if (it->second.second)
            os << "!!! INFINIT LOOP !!!";
        os << endl;
    }
}

void CWNJobsWatcher::CheckInfinitLoop()
{
    if (m_InfinitLoopTime > 0) {
        size_t count = 0;
        CMutexGuard guard(m_ActiveJobsMutex);
        NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
            if (!it->second.second) {
                if ( it->second.first.Elapsed() > m_InfinitLoopTime) {
                    ERR_POST(CTime(CTime::eCurrent).AsString() 
                             << " An infinit loop is detected in job " 
                             << it->first->GetJobKey());
                    it->second.second = true;      
                    CGridGlobals::GetInstance().
                        RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
                }
            } else
                ++count;
        }
        if( count > 0 && count == m_ActiveJobs.size()) {
            ERR_POST(CTime(CTime::eCurrent).AsString()
                     << " All jobs are in the infinit loops." 
                     << " SERVER IS COMMITTING SUICIDE!!");
            CGridGlobals::GetInstance().KillNode();
        }
    }
}

void CWNJobsWatcher::x_KillNode(CGridWorkerNode& worker)
{
    CMutexGuard guard(m_ActiveJobsMutex);
    NON_CONST_ITERATE(TActiveJobs, it, m_ActiveJobs) {
        if (!it->second.second) {
            worker.x_ReturnJob(it->first->GetJobKey());
        } else {
            worker.x_FailJob(it->first->GetJobKey(),
                             "An infinit loop has been detected after "
                             + NStr::IntToString((int)it->second.first.Elapsed())
                             + " seconds of execution.");
        }
    }
    TPid cpid = CProcess::GetCurrentPid();
    CProcess(cpid).Kill();
}


/////////////////////////////////////////////////////////////////////////////
//
auto_ptr<CGridGlobals> CGridGlobals::sm_Instance;

CGridGlobals::CGridGlobals()
    : m_ReuseJobObject(false),
      m_ShutdownLevel(CNetScheduleAdmin::eNoShutdown),
      m_StartTime(CTime(CTime::eCurrent)),
      m_Worker(NULL),
      m_ExclusiveMode(false)
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

void CGridGlobals::SetExclusiveMode(bool on_off) 
{ 
    CFastMutexGuard guard(m_ExclModeGuard);
    if (m_ExclusiveMode && on_off)
        NCBI_THROW(CGridGlobalsException, eExclusiveModeIsAlreadySet, "");
    m_ExclusiveMode = on_off; 
}
bool CGridGlobals::IsExclusiveMode() const 
{ 
    CFastMutexGuard guard(m_ExclModeGuard);
    return m_ExclusiveMode; 
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.7  2006/08/03 19:33:10  didenko
 * Added auto_shutdown_if_idle config file paramter
 * Added current date to messages in the log file.
 *
 * Revision 6.6  2006/06/22 13:52:36  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 6.5  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 6.4  2006/05/12 15:13:37  didenko
 * Added infinit loop detection mechanism in job executions
 *
 * Revision 6.3  2006/04/04 19:15:02  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 6.2  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 6.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */
 
