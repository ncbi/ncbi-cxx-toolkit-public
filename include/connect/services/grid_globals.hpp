#ifndef CONNECT_SERVICES___GRID_GLOBALS__HPP
#define CONNECT_SERVICES___GRID_GLOBALS__HPP

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
 * Authors:  Maxim Didenko
 *
 */

#include <corelib/ncbimisc.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/services/grid_worker.hpp>


BEGIN_NCBI_SCOPE


/// Grid worker global varialbles
///
/// @sa CNetScheduleClient
///


/// @internal
class CGridGlobals;
class NCBI_XCONNECT_EXPORT CWNJobsWatcher : public IWorkerNodeJobWatcher
{
public:
    CWNJobsWatcher();
    virtual ~CWNJobsWatcher();

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event);

    void Print(CNcbiOstream& os) const;
    unsigned int GetJobsRunningNumber() const { return m_ActiveJobs.size(); }

    void SetMaxJobsAllowed(unsigned int max_jobs_allowed)
    { m_MaxJobsAllowed = max_jobs_allowed; }
    void SetMaxFailuresAllowed(unsigned int max_failures_allowed)
    { m_MaxFailuresAllowed = max_failures_allowed; }
    void SetInfinitLoopTime(unsigned int inifinit_loop_time)
    { m_InfinitLoopTime = inifinit_loop_time; }


    void CheckInfinitLoop();

private:        
    unsigned int m_JobsStarted;
    unsigned int m_JobsSucceed;
    unsigned int m_JobsFailed;
    unsigned int m_JobsReturned;
    unsigned int m_JobsCanceled;
    unsigned int m_JobsLost;
    unsigned int m_MaxJobsAllowed;
    unsigned int m_MaxFailuresAllowed;
    unsigned int m_InfinitLoopTime;

    typedef map<const CWorkerNodeJobContext*, pair<CStopWatch, bool> > TActiveJobs;
    TActiveJobs    m_ActiveJobs;
    mutable CMutex m_ActiveJobsMutex;

    friend class CGridGlobals;
    void x_KillNode(CGridWorkerNode&);

private:
    CWNJobsWatcher(const CWNJobsWatcher&);
    CWNJobsWatcher& operator=(const CWNJobsWatcher&);
};


/// @internal
class NCBI_XCONNECT_EXPORT CGridGlobals
{
public:
    ~CGridGlobals();
    
    static CGridGlobals& GetInstance();

    unsigned int GetNewJobNumber();

    bool ReuseJobObject() const { return m_ReuseJobObject; }
    void SetReuseJobObject(bool value) { m_ReuseJobObject = value; }
    void SetWorker(CGridWorkerNode& worker) { m_Worker = &worker; }
    void SetExclusiveMode(bool on_off);
    bool IsExclusiveMode() const;


    /// Request node shutdown
    void RequestShutdown(CNetScheduleAdmin::EShutdownLevel level) 
                      { m_ShutdownLevel = level; }


    /// Check if shutdown was requested.
    ///
    CNetScheduleAdmin::EShutdownLevel GetShutdownLevel(void) 
                      { return m_ShutdownLevel; }

    CWNJobsWatcher& GetJobsWatcher();

    const CTime& GetStartTime() const { return m_StartTime; }
    
    void KillNode();
private:

    CGridGlobals();
    static auto_ptr<CGridGlobals> sm_Instance;  

    CAtomicCounter m_JobsStarted;
    bool m_ReuseJobObject;

    volatile CNetScheduleAdmin::EShutdownLevel m_ShutdownLevel;
    auto_ptr<CWNJobsWatcher> m_JobsWatcher;
    const CTime  m_StartTime;
    CGridWorkerNode* m_Worker;
    volatile bool m_ExclusiveMode;
    mutable CFastMutex m_ExclModeGuard;

    CGridGlobals(const CGridGlobals&);
    CGridGlobals& operator=(const CGridGlobals&);
    
};

class CGridGlobalsException : public CException
{
public:
    enum EErrCode {
        eExclusiveModeIsAlreadySet
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eExclusiveModeIsAlreadySet:    return "eExclusiveModeIsAlreadySet";
        default:                      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridGlobalsException, CException);
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2006/06/22 13:52:35  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 1.6  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 1.5  2006/05/12 15:13:37  didenko
 * Added infinit loop detection mechanism in job executions
 *
 * Revision 1.4  2006/04/04 19:15:01  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 1.3  2006/02/16 15:39:09  didenko
 * If an instance of a job's class could not be create then the worker node
 * should shutdown itself.
 *
 * Revision 1.2  2006/02/15 20:27:45  didenko
 * Added new optional config parameter "reuse_job_object" which allows
 * reusing IWorkerNodeJob objects in the jobs' threads instead of
 * creating a new object for each job.
 *
 * Revision 1.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES___GRID_GLOBALS__HPP
