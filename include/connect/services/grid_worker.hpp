#ifndef CONNECT_SERVICES__GRID_WORKER_HPP
#define CONNECT_SERVICES__GRID_WORKER_HPP


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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Interfaces.
 *
 */

/// @file grid_worker.hpp
/// Grid Framework specs.
///

#include "netschedule_api.hpp"
#include "netcache_api.hpp"
#include "error_codes.hpp"
#include "grid_app_version_info.hpp"
#include "timeline.hpp"

#include <connect/connect_export.h>

#include <util/thread_pool.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/request_control.hpp>
#include <corelib/request_ctx.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CArgs;
class IRegistry;
class CNcbiEnvironment;

class IWorkerNodeCleanupEventListener {
public:
    /// Event notifying of a safe clean-up point. It is generated for a
    /// job after the job is finished (or the worker node is shutting
    /// down). It is also generated for the whole worker node -- when
    /// it is shutting down.
    enum EWorkerNodeCleanupEvent {
        /// For jobs -- run from the same thread after Do() is done;
        /// for the whole WN -- run from a separate (clean-up) thread
        /// after all jobs are done and cleaned up, and the whole worker
        /// node is shutting down.
        eRegularCleanup,

        /// Called on emergency shutdown, always from a different (clean-up)
        /// thread, even for the jobs.
        eOnHardExit
    };

    virtual void HandleEvent(EWorkerNodeCleanupEvent cleanup_event) = 0;
    virtual ~IWorkerNodeCleanupEventListener() {}
};

/// Clean-up event source for the worker node. This interface provides
/// for subscribing for EWorkerNodeCleanupEvent. It is used by both
/// IWorkerNodeInitContext (for the global worker node clean-up) and
/// IWorkerNodeJob (for per-job clean-up).
class NCBI_XCONNECT_EXPORT IWorkerNodeCleanupEventSource : public CObject
{
public:
    virtual void AddListener(IWorkerNodeCleanupEventListener* listener) = 0;
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener) = 0;

    virtual void CallEventHandlers() = 0;
};

/// Worker Node initialize context
///
/// An instance of a class which implements this interface
/// is passed to a constructor of a worker node job class
/// The worker node job class can use this class to get
/// configuration parameters.
///
class NCBI_XCONNECT_EXPORT IWorkerNodeInitContext
{
public:
    virtual ~IWorkerNodeInitContext() {}

    /// Get a config file registry
    ///
    virtual const IRegistry&        GetConfig() const = 0;

    /// Get command line arguments
    ///
    virtual const CArgs&            GetArgs() const = 0;

    /// Get environment variables
    ///
    virtual const CNcbiEnvironment& GetEnvironment() const = 0;

    /// Get interface for registering clean-up event listeners
    ///
    virtual IWorkerNodeCleanupEventSource* GetCleanupEventSource() const = 0;

    /// Get the shared NetScheduleAPI object used by the worker node framework.
    ///
    virtual CNetScheduleAPI GetNetScheduleAPI() const = 0;

    /// Get the shared NetCacheAPI object used by the worker node framework.
    ///
    virtual CNetCacheAPI GetNetCacheAPI() const = 0;
};

class CWorkerNodeJobContext;
class CWorkerNodeControlServer;
/// Worker Node Job interface.
///
/// This interface is a worker node workhorse.
/// Job is executed by method Do of this interface.
/// Worker node application may be configured to execute several jobs at once
/// using threads, so implementation must be thread safe.
///
/// @sa CWorkerNodeJobContext, IWorkerNodeInitContext
///
class IWorkerNodeJob : public CObject
{
public:
    virtual ~IWorkerNodeJob() {}
    /// Execute the job.
    ///
    /// Job is considered successfully done if the Do method calls
    /// CommitJob (see CWorkerNodeJobContext).
    /// If method does not call CommitJob job is considered unresolved
    /// and returned back to the queue.
    /// Method can throw an exception (derived from std::exception),
    /// in this case job is considered failed (error message will be
    /// redirected to the NetSchedule queue)
    ///
    /// @param context
    ///   Context where a job can get all required information
    ///   like input/output steams, the job key etc.
    ///
    /// @return
    ///   Job exit code
    ///
    virtual int Do(CWorkerNodeJobContext& context) = 0;
};

class CGridWorkerNode;
class CWorkerNodeRequest;

/// Worker Node job context
///
/// Context in which a job is running, gives access to input and output
/// storage and some job control parameters.
///
/// @sa IWorkerNodeJob
///
class NCBI_XCONNECT_EXPORT CWorkerNodeJobContext :
        public CWorkerNodeTimelineEntry
{
public:
    /// Get the associated job structure to access all of its fields.
    const CNetScheduleJob& GetJob() const {return m_Job;}
    CNetScheduleJob& GetJob() {return m_Job;}

    /// Get a job key
    const string& GetJobKey() const        { return m_Job.job_id; }

    /// Get a job input string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an input data for a job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with
    ///    an input data for the job
    ///
    const string& GetJobInput() const      { return m_Job.input; }

    /// Set a job's output. This string will be sent to
    /// the queue when job is done.
    ///
    /// This method can be used to send a short data back to the client.
    /// To send a large result use GetOStream method. Don't call this
    /// method after GetOStream method is called.
    ///
    void          SetJobOutput(const string& output) { m_Job.output = output; }

    /// Set the return code of the job.
    ///
    void          SetJobRetCode(int ret_code) { m_Job.ret_code = ret_code; }

    /// Get a stream with input data for a job. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined
    /// using GetInputBlobSize.
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    ///
    size_t        GetInputBlobSize() const { return m_InputBlobSize; }

    /// Put progress message
    ///
    void          PutProgressMessage(const string& msg,
                                     bool send_immediately = false);

    /// Get a stream where a job can write its result
    ///
    CNcbiOstream& GetOStream();

    void CloseStreams();

    /// Confirm that a job is done and result is ready to be sent
    /// back to the client.
    ///
    /// This method should be called at the end of the
    /// IWorkerNodeJob::Do() method.
    ///
    void CommitJob();

    /// Confirm that a job is finished, but an error has happened
    /// during its execution.
    ///
    /// This method should be called at the end of the
    /// IWorkerNodeJob::Do() method.
    ///
    void CommitJobWithFailure(const string& err_msg);

    /// Schedule the job for return.
    ///
    /// This method should be called before exiting IWorkerNodeJob::Do()
    /// if the job is not needed anymore (canceled, expired or already
    /// executed elsewhere), or if the worker node is shutting down and
    /// the execution of this job should be gracefully (yet urgently)
    /// aborted and the job returned back to the network queue for
    /// execution by other worker node instances. Use the
    /// GetShutdownLevel() method to detect whether the job is not
    /// needed anymore.
    /// @see GetShutdownLevel()
    ///
    void ReturnJob();

    /// Check if job processing must be aborted.
    ///
    /// This method must be called periodically from within the Do()
    /// method to check whether it needs to stop processing the current
    /// job. If GetShutdownLevel() returns eShutdownImmediate or eDie,
    /// the Do() method must immediately return its job to the server
    /// by calling context.ReturnJob() and return a non-zero integer
    /// to the caller.
    ///
    /// If GetShutdownLevel() returns eNormalShutdown, the Do() method
    /// is free to decide whether the job processing must be aborted
    /// or completed.
    ///
    /// Aside from the natural reason for GetShutdownLevel() to return
    /// eNormalShutdown, eShutdownImmediate, or eDie, (that is, when
    /// a worker node shutdown has been requested), this method can
    /// also return eShutdownImmediate when the NetSchedule server
    /// is not expecting the current job to complete. This can happen
    /// due to multiple reasons:
    /// 1. The job has been explicitly cancelled by the submitter or
    ///    administrator.
    /// 2. The job was rescheduled to another worker node and has been
    ///    successfully finished.
    /// 3. The job has expired.
    ///
    CNetScheduleAdmin::EShutdownLevel GetShutdownLevel();

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Increment job execution timeout
    ///
    /// When node picks up the job for execution it may periodically
    /// communicate to the server that job is still alive and
    /// prolong job execution timeout, so job server does not try to
    /// reschedule.
    ///
    ///
    /// @param runtime_inc
    ///    Estimated time in seconds(from the current moment) to
    ///    finish the job.
    void JobDelayExpiration(unsigned runtime_inc);

    /// Check if logging was requested in config file
    ///
    bool IsLogRequested() const;

    /// Instruct the system that this job requires all system's resources
    /// If this method is call, the node will not accept any other jobs
    /// until this one is done. In the event if the mode has already been
    /// requested by another job this job will be returned back to the queue.
    void RequestExclusiveMode();

    const string& GetJobOutput() const { return m_Job.output; }

    CNetScheduleAPI::TJobMask GetJobMask() const { return m_Job.mask; }

    size_t GetMaxServerOutputSize();

    unsigned int GetJobNumber() const  { return m_JobNumber; }

    enum ECommitStatus {
        eDone,
        eFailure,
        eReturn,
        eNotCommitted,
        eCanceled
    };

    bool IsJobCommitted() const    { return m_JobCommitted != eNotCommitted; }
    ECommitStatus GetCommitStatus() const    { return m_JobCommitted; }
    static const char* GetCommitStatusDescription(ECommitStatus commit_status);

    bool IsCanceled() const { return m_JobCommitted == eCanceled; }

    IWorkerNodeCleanupEventSource* GetCleanupEventSource();

    CGridWorkerNode& GetWorkerNode() const {return m_WorkerNode;}

    CWorkerNodeJobContext(CGridWorkerNode& worker_node);

private:
    string& SetJobOutput()             { return m_Job.output; }
    size_t& SetJobInputBlobSize()      { return m_InputBlobSize; }
    bool IsJobExclusive() const        { return m_ExclusiveJob; }
    const string& GetErrMsg() const    { return m_Job.error_msg; }

    friend class CJobCommitterThread;
    friend class CWorkerNodeRequest;
    /// Only a CGridWorkerNode can create an instance of this class
    friend class CGridWorkerNode;

    void x_SetCanceled() { m_JobCommitted = eCanceled; }
    void CheckIfCanceled();

    void x_PrintRequestStop();
    void x_RunJob();
    void x_SendJobResults();

    void Reset();

    CGridWorkerNode&     m_WorkerNode;
    CNetScheduleJob      m_Job;
    ECommitStatus        m_JobCommitted;
    size_t               m_InputBlobSize;
    unsigned int         m_JobNumber;
    bool                 m_ExclusiveJob;

    CRef<IWorkerNodeCleanupEventSource> m_CleanupEventSource;

    CRef<CRequestContext> m_RequestContext;
    CRequestRateControl m_StatusThrottler;
    CRequestRateControl m_ProgressMsgThrottler;
    CNetScheduleExecutor m_NetScheduleExecutor;
    CNetCacheAPI m_NetCacheAPI;
    auto_ptr<CNcbiIstream> m_RStream;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream> m_WStream;

    CDeadline m_CommitExpiration;
    bool      m_FirstCommitAttempt;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CWorkerNodeJobContext(const CWorkerNodeJobContext&);
    CWorkerNodeJobContext& operator=(const CWorkerNodeJobContext&);
};

class CWorkerNodeIdleThread;
/// Worker Node Idle Task Context
///
class NCBI_XCONNECT_EXPORT CWorkerNodeIdleTaskContext
{
public:

    void RequestShutdown();
    bool IsShutdownRequested() const;

    void SetRunAgain() { m_RunAgain = true; }
    bool NeedRunAgain() const { return m_RunAgain; }

    void Reset();

private:
    friend class CWorkerNodeIdleThread;
    CWorkerNodeIdleTaskContext(CWorkerNodeIdleThread& thread);

    CWorkerNodeIdleThread& m_Thread;
    bool m_RunAgain;

private:
    CWorkerNodeIdleTaskContext(const CWorkerNodeIdleTaskContext&);
    CWorkerNodeIdleTaskContext& operator=(const CWorkerNodeIdleTaskContext&);
};

/// Worker Node Idle Task Interface
///
///  @sa IWorkerNodeJobFactory, CWorkerNodeIdleTaskContext
///
class IWorkerNodeIdleTask
{
public:
    virtual ~IWorkerNodeIdleTask() {};

    /// Do the Idle task here.
    /// It should not take a lot time, because while it is running
    /// no real jobs will be processed.
    virtual void Run(CWorkerNodeIdleTaskContext&) = 0;
};

/// Worker Node Job Factory interface
///
/// @sa IWorkerNodeJob
///
class IWorkerNodeJobFactory
{
public:
    virtual ~IWorkerNodeJobFactory() {}
    /// Create a job
    ///
    virtual IWorkerNodeJob* CreateInstance(void) = 0;

    /// Initialize a worker node factory
    ///
    virtual void Init(const IWorkerNodeInitContext& context) {}

    /// Get the job version
    ///
    virtual string GetJobVersion() const = 0;

    virtual string GetAppName() const {return GetJobVersion();}

    virtual string GetAppVersion() const {return GetJobVersion();}

    virtual string GetAppBuildDate() const {return __DATE__;}

    /// Get the Idle task
    ///
    virtual IWorkerNodeIdleTask* GetIdleTask() { return NULL; }
};

template <typename TWorkerNodeJob>
class CSimpleJobFactory : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_WorkerNodeInitContext = &context;
    }
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new TWorkerNodeJob(*m_WorkerNodeInitContext);
    }

private:
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
};

#define NCBI_DECLARE_WORKERNODE_FACTORY(TWorkerNodeJob, Version)         \
class TWorkerNodeJob##Factory : public CSimpleJobFactory<TWorkerNodeJob> \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return #TWorkerNodeJob " version " NCBI_AS_STRING(Version);      \
    }                                                                    \
    virtual string GetAppName() const                                    \
    {                                                                    \
        return #TWorkerNodeJob;                                          \
    }                                                                    \
    virtual string GetAppVersion() const                                 \
    {                                                                    \
        return NCBI_AS_STRING(Version);                                  \
    }                                                                    \
}

template <typename TWorkerNodeJob, typename TWorkerNodeIdleTask>
class CSimpleJobFactoryEx : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_WorkerNodeInitContext = &context;
        try {
            m_IdleTask.reset(new TWorkerNodeIdleTask(*m_WorkerNodeInitContext));
        } catch (exception& ex) {
            LOG_POST_XX(ConnServ_WorkerNode, 16,
                        "Error during Idle task construction: " << ex.what());
            throw;
        }
    }
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new TWorkerNodeJob(*m_WorkerNodeInitContext);
    }
    virtual IWorkerNodeIdleTask* GetIdleTask() { return m_IdleTask.get(); }

private:
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
    auto_ptr<TWorkerNodeIdleTask> m_IdleTask;
};

#define NCBI_DECLARE_WORKERNODE_FACTORY_EX(                              \
        TWorkerNodeJob,TWorkerNodeIdleTask, Version)                     \
class TWorkerNodeJob##FactoryEx                                          \
    : public CSimpleJobFactoryEx<TWorkerNodeJob, TWorkerNodeIdleTask>    \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return #TWorkerNodeJob " version " NCBI_AS_STRING(Version);      \
    }                                                                    \
    virtual string GetAppName() const                                    \
    {                                                                    \
        return #TWorkerNodeJob;                                          \
    }                                                                    \
    virtual string GetAppVersion() const                                 \
    {                                                                    \
        return NCBI_AS_STRING(Version);                                  \
    }                                                                    \
}

#define NCBI_DECLARE_WORKERNODE_FACTORY_PKG_VER_EX(                      \
        TWorkerNodeJob, TWorkerNodeIdleTask)                             \
class TWorkerNodeJob##FactoryEx                                          \
    : public CSimpleJobFactoryEx<TWorkerNodeJob, TWorkerNodeIdleTask>    \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return GRID_APP_VERSION_INFO;                                    \
    }                                                                    \
    virtual string GetAppName() const                                    \
    {                                                                    \
        return GRID_APP_NAME;                                            \
    }                                                                    \
    virtual string GetAppVersion() const                                 \
    {                                                                    \
        return GRID_APP_VERSION;                                         \
    }                                                                    \
}


/// Jobs watcher interface
class NCBI_XCONNECT_EXPORT IWorkerNodeJobWatcher
{
public:
    enum EEvent {
        eJobStarted,
        eJobStopped,
        eJobFailed,
        eJobSucceeded,
        eJobReturned,
        eJobCanceled,
        eJobLost
    };

    virtual ~IWorkerNodeJobWatcher();

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event) = 0;
};

class CWorkerNodeIdleThread;
class CJobCommitterThread;
class IGridWorkerNodeApp_Listener;
class CWNJobWatcher;
/// Grid Worker Node
///
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads (thread pool is used).
///
/// @note
/// Worker node application is parameterized using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:
    /// Construct a worker node using class factories
    ///
    CGridWorkerNode(CNcbiApplication& app,
        IWorkerNodeJobFactory* job_factory);

    virtual ~CGridWorkerNode();

    void Init();

    /// Start job execution loop.
    ///
    int Run(
#ifdef NCBI_OS_UNIX
            ESwitch daemonize = eDefault,
#endif
            string procinfo_file_name = kEmptyStr);

    void RequestShutdown();

    void ForceSingleThread() { m_SingleThreadForced = true; }

    void AddJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                          EOwnership owner = eNoOwnership);

    void SetListener(IGridWorkerNodeApp_Listener* listener);

    IWorkerNodeJobFactory& GetJobFactory() { return *m_JobProcessorFactory; }

    /// Get the maximum threads running simultaneously
    ///
    unsigned int GetMaxThreads() const { return m_MaxThreads; }

    /// Get total memory limit (automatic restart if node grows more than that)
    ///
    Uint8 GetTotalMemoryLimit() const { return m_TotalMemoryLimit; }

    /// Get total time limit (automatic restart after that)
    ///
    int GetTotalTimeLimit() const { return m_TotalTimeLimit; }
    time_t GetStartupTime() const { return m_StartupTime; }
    unsigned GetQueueTimeout() const {return m_QueueTimeout;}

    bool IsHostInAdminHostsList(const string& host) const;

    unsigned GetCommitJobInterval() const {return m_CommitJobInterval;}
    unsigned GetCheckStatusPeriod() const {return m_CheckStatusPeriod;}
    size_t GetServerOutputSize();

    const string& GetQueueName() const;

    const string& GetClientName() const;

    string GetAppName() const
    {
        CFastMutexGuard guard(m_JobProcessorMutex);
        return m_JobProcessorFactory->GetAppName();
    }
    string GetAppVersion() const
    {
        CFastMutexGuard guard(m_JobProcessorMutex);
        return m_JobProcessorFactory->GetAppVersion();
    }
    string GetBuildDate() const
    {
        CFastMutexGuard guard(m_JobProcessorMutex);
        return m_JobProcessorFactory->GetAppBuildDate();
    }

    const string& GetServiceName() const;

    CNetCacheAPI GetNetCacheAPI() const { return m_NetCacheAPI; }
    CNetScheduleAPI GetNetScheduleAPI() const { return m_NetScheduleAPI; }
    CNetScheduleExecutor GetNSExecutor() const { return m_NSExecutor; }

    bool EnterExclusiveMode();
    void LeaveExclusiveMode();
    bool IsExclusiveMode() const;
    bool WaitForExclusiveJobToFinish();

    /// Disable the automatic logging of request-start and
    /// request-stop events by the framework itself.
    enum EDisabledRequestEvents {
        eEnableStartStop,
        eDisableStartStop,
        eDisableStartOnly
    };
    static void DisableDefaultRequestEventLogging(
        EDisabledRequestEvents disabled_events = eDisableStartStop);

    IWorkerNodeCleanupEventSource* GetCleanupEventSource();

private:
    auto_ptr<IWorkerNodeJobFactory>      m_JobProcessorFactory;

    CNetCacheAPI m_NetCacheAPI;
    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleExecutor m_NSExecutor;

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

    CRef<IWorkerNodeCleanupEventSource> m_CleanupEventSource;

    IWorkerNodeJob* GetJobProcessor();

    friend class CWorkerNodeJobContext;

    void x_NotifyJobWatchers(const CWorkerNodeJobContext& job,
                            IWorkerNodeJobWatcher::EEvent event);

    set<SServerAddress> m_Masters;
    set<unsigned int> m_AdminHosts;

private:
    struct SNotificationTimelineEntry : public CWorkerNodeTimelineEntry
    {
        SServerAddress m_ServerAddress;

        unsigned m_DiscoveryIteration;

        SNotificationTimelineEntry(const SServerAddress& server_address,
                unsigned discovery_iteration) :
            m_ServerAddress(server_address),
            m_DiscoveryIteration(discovery_iteration)
        {
        }

        // Special constructor for m_TimelineSearchPattern (see below).
        SNotificationTimelineEntry() :
            m_ServerAddress(0, 0),
            m_DiscoveryIteration(0)
        {
        }

        bool IsDiscoveryAction() const {return m_DiscoveryIteration == 0;}

        struct SLess {
            bool operator ()(const SNotificationTimelineEntry* left,
                    const SNotificationTimelineEntry* right) const
            {
                return left->m_ServerAddress < right->m_ServerAddress;
            }
        };
    };

    unsigned m_DiscoveryIteration;

    CWorkerNodeTimeline<SNotificationTimelineEntry>
            m_ImmediateActions, m_Timeline;

    typedef set<SNotificationTimelineEntry*,
            SNotificationTimelineEntry::SLess> TTimelineEntries;

    TTimelineEntries m_TimelineEntryByAddress;

    SNotificationTimelineEntry m_TimelineSearchPattern;

    bool x_GetJobWithAffinityList(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job,
            CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
            const string& affinity_list);
    bool x_GetJobWithAffinityLadder(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job);
    bool x_PerformTimelineAction(SNotificationTimelineEntry* action,
            CNetScheduleJob& job);
    void x_ProcessRequestJobNotification();
    bool x_WaitForNewJob(CNetScheduleJob& job);
    bool x_GetNextJob(CNetScheduleJob& job);

    friend class CWNJobWatcher;
    friend class CWorkerNodeRequest;
    void x_ReturnJob(const CNetScheduleJob& job);
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
};

inline const string& CGridWorkerNode::GetQueueName() const
{
    return GetNetScheduleAPI().GetQueueName();
}

inline const string& CGridWorkerNode::GetClientName() const
{
    return GetNetScheduleAPI().GetService().GetServerPool().GetClientName();
}

inline const string& CGridWorkerNode::GetServiceName() const
{
    return GetNetScheduleAPI().GetService().GetServiceName();
}

inline bool CGridWorkerNode::IsExclusiveMode() const
{
    return m_IsProcessingExclusiveJob;
}


class NCBI_XCONNECT_EXPORT CGridWorkerNodeException : public CException
{
public:
    enum EErrCode {
        ePortBusy,
        eJobIsCanceled,
        eJobFactoryIsNotSet,
        eExclusiveModeIsAlreadySet
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case ePortBusy:                  return "ePortBusy";
        case eJobIsCanceled:             return "eJobIsCanceled";
        case eJobFactoryIsNotSet:        return "eJobFactoryIsNotSet";
        case eExclusiveModeIsAlreadySet: return "eExclusiveModeIsAlreadySet";
        default:                         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridWorkerNodeException, CException);
};


/* @} */


END_NCBI_SCOPE

#define WN_BUILD_DATE __DATE__ " " __TIME__

#endif //CONNECT_SERVICES__GRID_WOKER_HPP
