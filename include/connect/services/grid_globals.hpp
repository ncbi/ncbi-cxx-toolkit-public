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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 */

#include <connect/services/netschedule_api.hpp>
#include <connect/services/grid_worker.hpp>

#include <corelib/ncbimisc.hpp>


BEGIN_NCBI_SCOPE


/// Grid worker global varialbles
///
/// @sa CNetScheduleAPI
///


/// @internal
class CGridGlobals;
class NCBI_XCONNECT_EXPORT CWNJobWatcher : public IWorkerNodeJobWatcher
{
public:
    CWNJobWatcher();
    virtual ~CWNJobWatcher();

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event);

    void Print(CNcbiOstream& os) const;
    unsigned GetJobsRunningNumber() const
    { return (unsigned) m_ActiveJobs.size(); }

    void SetMaxJobsAllowed(unsigned int max_jobs_allowed)
    { m_MaxJobsAllowed = max_jobs_allowed; }
    void SetMaxFailuresAllowed(unsigned int max_failures_allowed)
    { m_MaxFailuresAllowed = max_failures_allowed; }
    void SetInfiniteLoopTime(unsigned int infinite_loop_time)
    { m_InfiniteLoopTime = infinite_loop_time; }


    void CheckForInfiniteLoop();

private:
    unsigned int m_JobsStarted;
    unsigned int m_JobsSucceeded;
    unsigned int m_JobsFailed;
    unsigned int m_JobsReturned;
    unsigned int m_JobsRescheduled;
    unsigned int m_JobsCanceled;
    unsigned int m_JobsLost;
    unsigned int m_MaxJobsAllowed;
    unsigned int m_MaxFailuresAllowed;
    unsigned int m_InfiniteLoopTime;
    struct SJobActivity {
        CStopWatch elasped_time;
        bool is_stuck;
        SJobActivity(CStopWatch et, bool is) : elasped_time(et), is_stuck(is) {}
        SJobActivity() : elasped_time(CStopWatch(CStopWatch::eStart)), is_stuck(false) {}
    };

    typedef map<CWorkerNodeJobContext*, SJobActivity> TActiveJobs;
    TActiveJobs    m_ActiveJobs;
    mutable CMutex m_ActiveJobsMutex;

    friend class CGridGlobals;
    void x_KillNode(CGridWorkerNode);

private:
    CWNJobWatcher(const CWNJobWatcher&);
    CWNJobWatcher& operator=(const CWNJobWatcher&);
};


/// @internal
class NCBI_XCONNECT_EXPORT CGridGlobals
{
public:
    CGridGlobals();
    ~CGridGlobals();

    static CGridGlobals& GetInstance();

    unsigned int GetNewJobNumber();

    bool ReuseJobObject() const {return m_ReuseJobObject;}
    void SetReuseJobObject(bool value) {m_ReuseJobObject = value;}
    void SetWorker(SGridWorkerNodeImpl* worker) {m_Worker = worker;}
    void SetUDPPort(unsigned short udp_port) {m_UDPPort = udp_port;}

    /// Request node shutdown
    void RequestShutdown(CNetScheduleAdmin::EShutdownLevel level)
    {
        m_ShutdownLevel = level;
        InterruptUDPPortListening();
    }
    void RequestShutdown(CNetScheduleAdmin::EShutdownLevel level,
            int exit_code)
    {
        m_ShutdownLevel = level;
        m_ExitCode = exit_code;
        InterruptUDPPortListening();
    }
    bool IsShuttingDown();

    /// Check if shutdown was requested.
    ///
    CNetScheduleAdmin::EShutdownLevel GetShutdownLevel(void)
                      { return m_ShutdownLevel; }

    void SetExitCode(int exit_code) { m_ExitCode = exit_code; }
    int GetExitCode() const { return m_ExitCode; }

    CWNJobWatcher& GetJobWatcher();

    const CTime& GetStartTime() const { return m_StartTime; }

    void KillNode();

    void InterruptUDPPortListening();

private:
    CAtomicCounter_WithAutoInit m_JobsStarted;
    bool m_ReuseJobObject;

    volatile CNetScheduleAdmin::EShutdownLevel m_ShutdownLevel;
    volatile int                               m_ExitCode;
    unique_ptr<CWNJobWatcher> m_JobWatcher;
    const CTime  m_StartTime;
    SGridWorkerNodeImpl* m_Worker;
    unsigned short m_UDPPort;

    CGridGlobals(const CGridGlobals&);
    CGridGlobals& operator=(const CGridGlobals&);
};

inline bool CGridGlobals::IsShuttingDown()
{
    return m_ShutdownLevel != CNetScheduleAdmin::eNoShutdown;
}

END_NCBI_SCOPE

#endif // CONNECT_SERVICES___GRID_GLOBALS__HPP
