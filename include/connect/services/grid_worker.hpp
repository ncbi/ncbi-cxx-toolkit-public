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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Interfaces.
 *
 */

/// @file grid_worker.hpp
/// NetSchedule Framework specs.
///

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/blob_storage.hpp>
#include <connect/connect_export.h>
#include <connect/services/ns_client_factory.hpp>
//#nclude <connect/services/ns_client_wrappers.hpp>
#include <connect/services/error_codes.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CArgs;
class IRegistry;
class CNcbiEnvironment;

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

    /// Get a config file regestry
    ///
    virtual const IRegistry&        GetConfig() const;

    /// Get command line arguments
    ///
    virtual const CArgs&            GetArgs() const;

    /// Get environment variables
    ///
    virtual const CNcbiEnvironment& GetEnvironment() const;
};

class CWorkerNodeJobContext;
class CWorkerNodeControlThread;
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
    /// Job is considered successfull if method calls
    /// CommitJob (see CWorkerNodeJobContext).
    /// If method does not call CommitJob job is considered unresolved
    /// and returned back to the queue.
    /// Method can throw an exception (derived from std::exception),
    /// in this case job is considered failed (error message will be
    /// redirected to the netschedule queue)
    ///
    /// @param context
    ///   Context where a job can get all requered information
    ///   like input/output steams, the job key etc.
    ///
    /// @return
    ///   Job exit code
    ///
    virtual int Do(CWorkerNodeJobContext& context) = 0;
};

class CGridWorkerNode;
class CGridThreadContext;
class CWorkerNodeRequest;

/// Worker Node job context
///
/// Context in which a job is runnig, gives access to input and output
/// storage and some job control parameters.
///
/// @sa IWorkerNodeJob
///
class NCBI_XCONNECT_EXPORT CWorkerNodeJobContext
{
public:

    ~CWorkerNodeJobContext();

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

    /// Get a stream with input data for a job. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined
    /// using GetInputBlobSize. Will throw a CNetCacheStorageException
    /// exception with code eBlocked if the mode parameter is set to
    /// IBlobStorage::eLockNoWait and a blob is blocked by
    /// another process.
    ///
    CNcbiIstream& GetIStream(IBlobStorage::ELockMode mode =
                             IBlobStorage::eLockWait);

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

    /// Confirm that a job is done and result is ready to be sent
    /// back to the client.
    ///
    /// This method should be called at the end of IWorkerNodeJob::Do
    /// method. If this method or CommitJobWithFailure are not called
    /// the job's result will NOT be sent
    /// to the queue (job is returned back to the NetSchedule queue
    /// and re-executed in a while)
    ///
    void CommitJob() { m_JobCommitted = eDone; }

    /// Confirm that a job is finished, but an error has happend during its
    /// execution.
    ///
    /// This method should be called at the end of IWorkerNodeJob::Do
    /// method. If this method or CommitJob are not called
    /// the job's result will NOT be sent
    /// to the queue (job is returned back to the NetSchedule queue
    /// and re-executed in a while)
    ///
    void CommitJobWithFailure(const string& err_msg)
    { m_JobCommitted = eFailure; m_Job.error_msg = err_msg; }

    /// Check if node application shutdown was requested.
    ///
    /// If job takes a long time it should periodically call
    /// this method during the execution and check if node shutdown was
    /// requested and gracefully finish its work.
    ///
    /// Shutdown level eNormal means job can finish the job and exit,
    /// eImmidiate means node should exit immediately
    /// (unfinished jobs are returned back to the queue)
    ///
    CNetScheduleAdmin::EShutdownLevel GetShutdownLevel(void) const;

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Set job execution timeout
    ///
    /// When node picks up the job for execution it may evaluate what time it
    /// takes for computation and report it to the queue. If job does not
    /// finish in the specified timeframe (because of a failure)
    /// it is going to be rescheduled
    ///
    /// Default value for the run timeout specified in the queue settings on
    /// the server side.
    ///
    /// @param time_to_run
    ///    Time in seconds to finish the job. 0 means "queue default value".
    ///
    void SetJobRunTimeout(unsigned time_to_run);

    /// Increment job execution timeout
    ///
    /// When node picks up the job for execution it may peridically
    /// communicate to the server that job is still alive and
    /// prolong job execution timeout, so job server does not try to
    /// reschedule.
    ///
    ///
    /// @param runtime_inc
    ///    Estimated time in seconds(from the current moment) to
    ///    finish the job.
    ///
    /// @sa SetRunTimeout
    void JobDelayExpiration(unsigned runtime_inc);

    /// Check if logging was requested in config file
    ///
    bool IsLogRequested(void) const { return m_LogRequested; }

    /// Instruct the system that this job requires all system's resoures
    /// If this method is call, the node will not accept any other jobs
    /// until this one is done. In the event if the mode has already been
    /// requested by another job this job will be returnd back to the queue.
    void RequestExclusiveMode();

    const string& GetJobOutput() const { return m_Job.output; }

    CNetScheduleAPI::TJobMask GetJobMask() const { return m_Job.mask; }

    size_t GetMaxServerOutputSize() const;

    unsigned int GetJobNumber() const  { return m_JobNumber; }

private:
    enum ECommitStatus {
        eDone,
        eFailure,
        eNotCommitted
    };

    friend class CGridThreadContext;
    void SetThreadContext(CGridThreadContext*);
    string& SetJobOutput()             { return m_Job.output; }
    size_t& SetJobInputBlobSize()      { return m_InputBlobSize; }
    bool IsJobExclusive() const        { return m_ExclusiveJob; }
    const string& GetErrMsg() const    { return m_Job.error_msg; }

    friend class CWorkerNodeRequest;
    CGridWorkerNode& GetWorkerNode()   { return m_WorkerNode; }
    bool IsJobCommitted() const    { return m_JobCommitted != eNotCommitted; }
    ECommitStatus GetCommitStatus() const    { return m_JobCommitted; }

    /// Only a CGridWorkerNode can create an instance of this class
    friend class CGridWorkerNode;
    CWorkerNodeJobContext(CGridWorkerNode&   worker_node,
                          const CNetScheduleJob& job,
                          unsigned int       job_nubmer,
                          bool               log_requested);

    void Reset(const CNetScheduleJob& job, unsigned int job_number);

    CGridWorkerNode&     m_WorkerNode;
    CNetScheduleJob      m_Job;
    ECommitStatus        m_JobCommitted;
    size_t               m_InputBlobSize;
    bool                 m_LogRequested;
    unsigned int         m_JobNumber;
    CGridThreadContext*  m_ThreadContext;
    bool                 m_ExclusiveJob;


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

/// Worker Node Idle Task Interaface
///
///  @sa IWorkerNodeJobFactory, CWorkerNodeIdleTaskContext
///
class IWorkerNodeIdleTask
{
public:
    virtual ~IWorkerNodeIdleTask() {};

    /// Do an Idle Taks here.
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
    virtual string GetJobVersion(void) const = 0;

    /// Get an Idle task
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
        return #TWorkerNodeJob " version " #Version;                     \
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
                        "Idle tast is not created: " << ex.what());
        } catch (...) {
            LOG_POST_XX(ConnServ_WorkerNode, 17,
                        "Idle tast is not created: Unknown error");
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

#define NCBI_DECLARE_WORKERNODE_FACTORY_EX(TWorkerNodeJob,TWorkerNodeIdleTask, Version) \
class TWorkerNodeJob##FactoryEx                                          \
    : public CSimpleJobFactoryEx<TWorkerNodeJob, TWorkerNodeIdleTask>    \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return #TWorkerNodeJob " version " #Version;                     \
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
        eJobSucceed,
        eJobReturned,
        eJobCanceled,
        eJobLost
    };

    virtual ~IWorkerNodeJobWatcher();

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event) = 0;

};

class CWNJobsWatcher;
/// Grid Worker Node
///
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads (thread pool is used).
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:

    /// Construct a worker node using class factories
    ///
    CGridWorkerNode(IWorkerNodeJobFactory&      job_creator,
                    IBlobStorageFactory&        ns_storage_creator,
                    INetScheduleClientFactory&  ns_client_creator,
                    IWorkerNodeJobWatcher*      job_wather);

    virtual ~CGridWorkerNode();

    /// Set a UDP port to listen to input jobs
    ///
    void SetListeningPort(unsigned int udp_port) { m_UdpPort = udp_port; }
    unsigned int GetListeningPort() const { return m_UdpPort; }

    /// Set the maximum threads running simultaneously
    ///
    void SetMaxThreads(unsigned int max_threads)
                      { m_MaxThreads = max_threads; }

    /// Get the maximum threads running simultaneously
    //
    unsigned int GetMaxThreads() const { return m_MaxThreads; }

    void SetInitThreads(unsigned int init_threads)
                      { m_InitThreads = init_threads; }

    void SetNSTimeout(unsigned int timeout) { m_NSTimeout = timeout; }
    void SetThreadsPoolTimeout(unsigned int timeout)
                      { m_ThreadsPoolTimeout = timeout; }

    void ActivateServerLog(bool on_off)
                      { m_LogRequested = on_off; }

    void SetMasterWorkerNodes(const string& hosts);
    void SetAdminHosts(const string& hosts);
    bool IsHostInAdminHostsList(const string& host) const;

    void SetUseEmbeddedStorage(bool on_off) { m_UseEmbeddedStorage = on_off; }
    bool IsEmeddedStorageUsed() const { return m_UseEmbeddedStorage; }
    unsigned int GetCheckStatusPeriod() const { return m_CheckStatusPeriod; }
    void SetCheckStatusPeriod(unsigned int sec) { m_CheckStatusPeriod = sec; }
    size_t GetServerOutputSize() const;

    /// Start jobs execution.
    ///
    void Run();

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Get a job version
    ///
    string GetJobVersion() const
    {
        CFastMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.GetJobVersion();
    }

    /// Get a Connection Info
    ///
    const string& GetServiceName() const;


    CNetScheduleAPI& GetNSClient() const;
    CNetScheduleExecuter GetNSExecuter() const;

    bool IsExclusiveMode();

private:
    IWorkerNodeJobFactory&       m_JobFactory;
    IBlobStorageFactory&         m_NSStorageFactory;
    IWorkerNodeJobWatcher*       m_JobWatcher;

    mutable auto_ptr<CNetScheduleAPI>    m_SharedNSClient;

    auto_ptr<CStdPoolOfThreads>  m_ThreadsPool;
    unsigned int                 m_UdpPort;
    unsigned int                 m_MaxThreads;
    unsigned int                 m_InitThreads;
    unsigned int                 m_NSTimeout;
    unsigned int                 m_ThreadsPoolTimeout;
    mutable CFastMutex           m_JobFactoryMutex;
    CFastMutex                   m_StorageFactoryMutex;
    CFastMutex                   m_JobWatcherMutex;
    bool                         m_LogRequested;
    bool                         m_UseEmbeddedStorage;
    unsigned int                 m_CheckStatusPeriod;
    CSemaphore                   m_JobGetterSemaphore;

    friend class CGridThreadContext;
    IWorkerNodeJob* CreateJob()
    {
        CFastMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.CreateInstance();
    }
    IBlobStorage* CreateStorage()
    {
        CFastMutexGuard guard(m_StorageFactoryMutex);
        return  m_NSStorageFactory.CreateInstance();
    }
    friend class CWorkerNodeJobContext;
    CSemaphore& x_GetJobGetterSemaphore() { return m_JobGetterSemaphore; }

    void x_NotifyJobWatcher(const CWorkerNodeJobContext& job,
                            IWorkerNodeJobWatcher::EEvent event)
    {
        if (m_JobWatcher) {
            CFastMutexGuard guard(m_JobWatcherMutex);
            m_JobWatcher->Notify(job, event);
        }
    }

    struct SHost {
        SHost(string h, unsigned int p) : host(h), port(p) {}
        bool operator== (const SHost& h) const
        { return host == h.host && port == h.port; }
        bool operator< (const SHost& h) const
        { return (host == h.host ? (port < h.port) : (host < h.host)); }

        string host;
        unsigned int port;
    };
    set<SHost> m_Masters;
    set<unsigned int> m_AdminHosts;

    bool x_GetNextJob(CNetScheduleJob& job);

    friend class CWNJobsWatcher;
    friend class CWorkerNodeRequest;
    void x_ReturnJob(const string& job_key);
    void x_FailJob(const string& job_key, const string& reason);
    bool x_CreateNSReadClient();
    bool x_AreMastersBusy() const;

    CGridWorkerNode(const CGridWorkerNode&);
    CGridWorkerNode& operator=(const CGridWorkerNode&);

};

inline const string& CGridWorkerNode::GetQueueName() const
{
    return GetNSClient().GetQueueName();
}

inline const string& CGridWorkerNode::GetClientName() const
{
    return GetNSClient().GetClientName();
}

inline const string& CGridWorkerNode::GetServiceName() const
{
    return GetNSClient().GetServiceName();
}

inline CNetScheduleAPI& CGridWorkerNode::GetNSClient() const
{
    return *m_SharedNSClient;
}

inline CNetScheduleExecuter CGridWorkerNode::GetNSExecuter() const
{
    return GetNSClient().GetExecuter();
}


class CGridWorkerNodeException : public CException
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

    NCBI_EXCEPTION_DEFAULT(CGridWorkerNodeException, CException);
};


/* @} */


END_NCBI_SCOPE

#define WN_BUILD_DATE " build " __DATE__ " " __TIME__ " (Framework version: 3.0.0)"

#endif //CONNECT_SERVICES__GRID_WOKER_HPP
