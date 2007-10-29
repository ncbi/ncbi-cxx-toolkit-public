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

    CGridGlobals(const CGridGlobals&);
    CGridGlobals& operator=(const CGridGlobals&);
    
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES___GRID_GLOBALS__HPP
