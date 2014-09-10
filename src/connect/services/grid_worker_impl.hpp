#ifndef CONNECT_SERVICES__GRID_WORKER_IMPL__HPP
#define CONNECT_SERVICES__GRID_WORKER_IMPL__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *    Common NetSchedule Worker Node declarations
 */

#include "wn_commit_thread.hpp"
#include "wn_cleanup.hpp"
#include "netschedule_api_impl.hpp"

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//

///@internal
struct SWorkerNodeJobContextImpl : public CWorkerNodeTimelineEntry
{
    SWorkerNodeJobContextImpl(SGridWorkerNodeImpl* worker_node);

    void Reset();

    void x_SetCanceled() { m_JobCommitted = CWorkerNodeJobContext::eCanceled; }
    void CheckIfCanceled();

    void x_PrintRequestStop();
    virtual void x_RunJob();

    SGridWorkerNodeImpl* m_WorkerNode;
    CNetScheduleJob m_Job;
    CWorkerNodeJobContext::ECommitStatus m_JobCommitted;
    size_t m_InputBlobSize;
    unsigned int m_JobNumber;
    bool m_ExclusiveJob;

    CRef<CWorkerNodeCleanup> m_CleanupEventSource;

    CRef<CRequestContext> m_RequestContext;
    CRequestRateControl m_StatusThrottler;
    CRequestRateControl m_ProgressMsgThrottler;
    CNetScheduleExecutor m_NetScheduleExecutor;
    CNetCacheAPI m_NetCacheAPI;
    auto_ptr<CNcbiIstream> m_RStream;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream> m_WStream;

    // Used for the job "pullback" mechanism.
    unsigned m_JobGeneration;

    CDeadline m_CommitExpiration;
    bool      m_FirstCommitAttempt;
};

///@internal
struct SGridWorkerNodeImpl : public CObject
{
    SGridWorkerNodeImpl(CNcbiApplication& app,
            IWorkerNodeJobFactory* job_factory);

    void AddJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                          EOwnership owner = eNoOwnership);

    int Run(
#ifdef NCBI_OS_UNIX
            ESwitch daemonize,
#endif
            string procinfo_file_name);

    void x_WNCoreInit();
    void x_StartWorkerThreads();
    void x_StopWorkerThreads();
    void x_ClearNode();
    int x_WNCleanUp();

    const string& GetQueueName() const
    {
        return m_NetScheduleAPI.GetQueueName();
    }
    const string& GetClientName() const
    {
        return m_NetScheduleAPI->m_Service->m_ServerPool->m_ClientName;
    }
    const string& GetServiceName() const
    {
        return m_NetScheduleAPI->m_Service->m_ServiceName;
    }

    bool EnterExclusiveMode();
    void LeaveExclusiveMode();
    bool IsExclusiveMode() const {return m_IsProcessingExclusiveJob;}
    bool WaitForExclusiveJobToFinish();

    void SetJobPullbackTimer(unsigned seconds);
    bool CheckForPullback(unsigned job_generation);

    int OfflineRun();

    auto_ptr<IWorkerNodeJobFactory>      m_JobProcessorFactory;

    CNetCacheAPI m_NetCacheAPI;
    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleExecutor m_NSExecutor;
    CStdPoolOfThreads* m_ThreadPool;

    unsigned int                 m_MaxThreads;
    unsigned int                 m_NSTimeout;
    mutable CFastMutex           m_JobProcessorMutex;
    unsigned                     m_CommitJobInterval;
    unsigned                     m_CheckStatusPeriod;
    CSemaphore                   m_ExclusiveJobSemaphore;
    bool                         m_IsProcessingExclusiveJob;
    Uint8                        m_TotalMemoryLimit;
    unsigned                     m_TotalTimeLimit;
    time_t                       m_StartupTime;
    unsigned                     m_QueueTimeout;

    typedef map<IWorkerNodeJobWatcher*,
            AutoPtr<IWorkerNodeJobWatcher> > TJobWatchers;
    CFastMutex m_JobWatcherMutex;
    TJobWatchers m_Watchers;

    CRef<CWorkerNodeCleanup> m_CleanupEventSource;

    IWorkerNodeJob* GetJobProcessor();

    void x_NotifyJobWatchers(const CWorkerNodeJobContext& job,
                            IWorkerNodeJobWatcher::EEvent event);

    set<SServerAddress> m_Masters;
    set<unsigned int> m_AdminHosts;

    void* volatile m_SuspendResumeEvent;
    bool m_TimelineIsSuspended;
    // Support for the job "pullback" mechanism.
    CFastMutex m_JobPullbackMutex;
    unsigned m_CurrentJobGeneration;
    unsigned m_DefaultPullbackTimeout;
    CDeadline m_JobPullbackTime;

    bool x_AreMastersBusy() const;
    auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;

    CRef<CJobCommitterThread> m_JobCommitterThread;
    CRef<CWorkerNodeIdleThread>  m_IdleThread;

    auto_ptr<IGridWorkerNodeApp_Listener> m_Listener;

    CNcbiApplication& m_App;
    bool m_SingleThreadForced;
    bool m_LogRequested;
    bool m_ProgressLogRequested;
    size_t m_QueueEmbeddedOutputSize;
    unsigned m_ThreadPoolTimeout;
};


///@internal
class CWorkerNodeRequest : public CStdRequest
{
public:
    CWorkerNodeRequest(SWorkerNodeJobContextImpl* context) :
        m_JobContext(context)
    {
    }

    virtual void Process();

private:
    CWorkerNodeJobContext m_JobContext;
};


/////////////////////////////////////////////////////////////////////////////
//

#define NO_EVENT ((void*) 0)
#define SUSPEND_EVENT ((void*) 1)
#define RESUME_EVENT ((void*) 2)

/////////////////////////////////////////////////////////////////////////////
//

bool g_IsRequestStartEventEnabled();
bool g_IsRequestStopEventEnabled();

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CMainLoopThread : public CThread
{
public:
    CMainLoopThread(SGridWorkerNodeImpl* worker_node) :
        m_WorkerNode(worker_node),
        m_Semaphore(0, 1),
        m_DiscoveryIteration(1),
        m_DiscoveryAction(
                new SNotificationTimelineEntry(SServerAddress(0, 0), 0))
    {
        m_ImmediateActions.Push(m_DiscoveryAction);
    }

    virtual void* Main();

    virtual ~CMainLoopThread();

private:
    SGridWorkerNodeImpl* m_WorkerNode;
    CSemaphore m_Semaphore;
    unsigned m_DiscoveryIteration;

    typedef CWorkerNodeTimeline<SNotificationTimelineEntry,
            SNotificationTimelineEntry::TRef> TNotificationTimeline;

    TNotificationTimeline m_ImmediateActions, m_Timeline;

    typedef set<SNotificationTimelineEntry*,
            SNotificationTimelineEntry::SLess> TTimelineEntries;

    TTimelineEntries m_TimelineEntryByAddress;

    SNotificationTimelineEntry::TRef m_DiscoveryAction;

    bool x_GetJobWithAffinityList(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job,
            CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
            const string& affinity_list);
    bool x_GetJobWithAffinityLadder(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job);
    SNotificationTimelineEntry* x_GetTimelineEntry(SNetServerImpl* server_impl);
    bool x_PerformTimelineAction(TNotificationTimeline& timeline,
            CNetScheduleJob& job);
    bool x_EnterSuspendedState();
    void x_ProcessRequestJobNotification();
    bool x_WaitForNewJob(CNetScheduleJob& job);
    bool x_GetNextJob(CNetScheduleJob& job);
    void x_ReturnJob(const CNetScheduleJob& job);
};

END_NCBI_SCOPE

#endif /* CONNECT_SERVICES__GRID_WORKER_IMPL__HPP */
