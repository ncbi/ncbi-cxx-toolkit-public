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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "grid_worker_impl.hpp"
#include "grid_control_thread.hpp"
#include "netcache_api_impl.hpp"

#include <connect/services/grid_globals.hpp>

#ifdef NCBI_OS_UNIX
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

#define DEFAULT_NS_TIMEOUT 30

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
IWorkerNodeJobWatcher::~IWorkerNodeJobWatcher()
{
}

/////////////////////////////////////////////////////////////////////////////
//
static CGridWorkerNode::EDisabledRequestEvents s_ReqEventsDisabled =
    CGridWorkerNode::eEnableStartStop;

void CWorkerNodeRequest::Process()
{
    // There are two implementations of x_RunJob(): one is for
    // the real worker node, and the other is for the offline run.
    // Do not add any implementation-specific code here.
    m_JobContext->x_RunJob();
}

/////////////////////////////////////////////////////////////////////////////
//
//     CGridControlThread
/// @internal
class CGridControlThread : public CThread
{
public:
    CGridControlThread(SGridWorkerNodeImpl* worker_node,
        unsigned short start_port, unsigned short end_port) : m_Control(
            worker_node, start_port, end_port),
          m_ThreadName(worker_node->GetAppName() + "_ct")
    {
    }

    void Prepare()                        {        m_Control.StartListening();  }
    unsigned short GetControlPort() const { return m_Control.GetControlPort();  }
    void Stop()                           {        m_Control.RequestShutdown(); }

protected:
    virtual void* Main(void)
    {
        SetCurrentThreadName(m_ThreadName);
        m_Control.Run();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
        LOG_POST_X(46, Info << "Control Thread has been stopped.");
    }

private:
    CWorkerNodeControlServer m_Control;
    const string m_ThreadName;
};

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleThread      --
class CWorkerNodeIdleThread : public CThread
{
public:
    CWorkerNodeIdleThread(IWorkerNodeIdleTask*,
                          SGridWorkerNodeImpl* worker_node,
                          unsigned run_delay,
                          unsigned int auto_shutdown);

    void RequestShutdown()
    {
        m_ShutdownFlag = true;
        m_Wait1.Post();
        m_Wait2.Post();
    }
    void Schedule()
    {
        CFastMutexGuard guard(m_Mutex);
        m_AutoShutdownSW.Restart();
        if (m_StopFlag) {
            m_StopFlag = false;
            m_Wait1.Post();
        }
    }
    void Suspend()
    {
        CFastMutexGuard guard(m_Mutex);
        m_AutoShutdownSW.Restart();
        m_AutoShutdownSW.Stop();
        if (!m_StopFlag) {
            m_StopFlag = true;
            m_Wait2.Post();
        }
    }

    bool IsShutdownRequested() const { return m_ShutdownFlag; }


protected:
    virtual void* Main(void);
    virtual void OnExit(void);

    CWorkerNodeIdleTaskContext& GetContext();

private:

    unsigned int x_GetInterval() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_AutoShutdown > 0 ? min(m_AutoShutdown -
                (unsigned) m_AutoShutdownSW.Elapsed(),
                        m_RunInterval) : m_RunInterval;
    }
    bool x_GetStopFlag() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_StopFlag;
    }
    unsigned x_GetIdleTimeIfShutdown() const
    {
        CFastMutexGuard guard(m_Mutex);
        auto elapsed = static_cast<unsigned>(m_AutoShutdownSW.Elapsed());
        return (m_AutoShutdown && elapsed >= m_AutoShutdown) ? elapsed : 0;
    }

    IWorkerNodeIdleTask* m_Task;
    SGridWorkerNodeImpl* m_WorkerNode;
    CWorkerNodeIdleTaskContext m_TaskContext;
    mutable CSemaphore  m_Wait1;
    mutable CSemaphore  m_Wait2;
    volatile bool       m_StopFlag;
    volatile bool       m_ShutdownFlag;
    unsigned int        m_RunInterval;
    unsigned int        m_AutoShutdown;
    CStopWatch          m_AutoShutdownSW;
    mutable CFastMutex  m_Mutex;
    const string m_ThreadName;

    CWorkerNodeIdleThread(const CWorkerNodeIdleThread&);
    CWorkerNodeIdleThread& operator=(const CWorkerNodeIdleThread&);
};

CWorkerNodeIdleThread::CWorkerNodeIdleThread(IWorkerNodeIdleTask* task,
                                             SGridWorkerNodeImpl* worker_node,
                                             unsigned run_delay,
                                             unsigned int auto_shutdown)
    : m_Task(task), m_WorkerNode(worker_node),
      m_TaskContext(*this),
      m_Wait1(0,100000), m_Wait2(0,1000000),
      m_StopFlag(false), m_ShutdownFlag(false),
      m_RunInterval(run_delay),
      m_AutoShutdown(auto_shutdown), m_AutoShutdownSW(CStopWatch::eStart),
      m_ThreadName(worker_node->GetAppName() + "_id")
{
}
void* CWorkerNodeIdleThread::Main()
{
    SetCurrentThreadName(m_ThreadName);

    while (!m_ShutdownFlag) {
        if (auto idle = x_GetIdleTimeIfShutdown()) {
            LOG_POST_X(47, "Has been idle (no jobs to process) over " << idle << " seconds. Exiting.");
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            break;
        }
        unsigned interval = m_AutoShutdown > 0 ? min (m_RunInterval,
                m_AutoShutdown) : m_RunInterval;
        if (m_Wait1.TryWait(interval, 0)) {
            if (m_ShutdownFlag)
                continue;
            interval = x_GetInterval();
            if (m_Wait2.TryWait(interval, 0)) {
                continue;
            }
        }
        if (m_Task && !x_GetStopFlag()) {
            try {
                do {
                    if (auto idle = x_GetIdleTimeIfShutdown()) {
                        LOG_POST_X(47, "Has been idle (no jobs to process) over " << idle << " seconds. Exiting.");
                        CGridGlobals::GetInstance().RequestShutdown(
                            CNetScheduleAdmin::eShutdownImmediate);
                        m_ShutdownFlag = true;
                        break;
                    }
                    GetContext().Reset();
                    m_Task->Run(GetContext());
                } while (GetContext().NeedRunAgain() && !m_ShutdownFlag);
            } NCBI_CATCH_ALL_X(58,
                "CWorkerNodeIdleThread::Main: Idle Task failed");
        }
    }
    return 0;
}

void CWorkerNodeIdleThread::OnExit(void)
{
    LOG_POST_X(49, Info << "Idle Thread has been stopped.");
}

CWorkerNodeIdleTaskContext& CWorkerNodeIdleThread::GetContext()
{
    return m_TaskContext;
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleTaskContext      --
CWorkerNodeIdleTaskContext::CWorkerNodeIdleTaskContext(
        CWorkerNodeIdleThread& thread) :
    m_Thread(thread), m_RunAgain(false)
{
}
bool CWorkerNodeIdleTaskContext::IsShutdownRequested() const
{
    return m_Thread.IsShutdownRequested();
}
void CWorkerNodeIdleTaskContext::Reset()
{
    m_RunAgain = false;
}

void CWorkerNodeIdleTaskContext::RequestShutdown()
{
    m_Thread.RequestShutdown();
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}

/////////////////////////////////////////////////////////////////////////////
//
//    CIdleWatcher
/// @internal
class CIdleWatcher : public IWorkerNodeJobWatcher
{
public:
    CIdleWatcher(CWorkerNodeIdleThread& idle) : m_Idle(idle) {}

    virtual ~CIdleWatcher() {};

    virtual void Notify(const CWorkerNodeJobContext&, EEvent event)
    {
        if (event == eJobStarted) {
            m_RunningJobs.Add(1);
            m_Idle.Suspend();
        } else if (event == eJobStopped) {
            if (m_RunningJobs.Add(-1) == 0)
                m_Idle.Schedule();
        }
    }

private:
    CWorkerNodeIdleThread& m_Idle;
    CAtomicCounter_WithAutoInit m_RunningJobs;
};


SSuspendResume::EState SSuspendResume::CheckState() volatile
{
    switch (m_Event.exchange(eNoEvent)) {
        case eNoEvent:
            break;
        case eSuspend:
            return m_IsSuspended.exchange(true) ? eSuspended : eSuspending;
        case eResume:
            m_IsSuspended.store(false);
            return eRunning;
    }

    return m_IsSuspended.load() ? eSuspended : eRunning;
}


/////////////////////////////////////////////////////////////////////////////
//
const IRegistry& SGridWorkerNodeImpl::GetConfig() const
{
    _ASSERT(m_Registry);

    return *m_Registry;
}

const CArgs& SGridWorkerNodeImpl::GetArgs() const
{
    return m_App.GetArgs();
}

const CNcbiEnvironment& SGridWorkerNodeImpl::GetEnvironment() const
{
    return m_App.GetEnvironment();
}

IWorkerNodeCleanupEventSource* SGridWorkerNodeImpl::GetCleanupEventSource() const
{
    // It should be safe to do the cast here
    // as SGridWorkerNodeImpl is only used as const IWorkerNodeInitContext in
    // its own constructor where 'this' is not const
    return const_cast<SGridWorkerNodeImpl*>(this)->m_CleanupEventSource;
}

CNetScheduleAPI SGridWorkerNodeImpl::GetNetScheduleAPI() const
{
    return m_NetScheduleAPI;
}

CNetCacheAPI SGridWorkerNodeImpl::GetNetCacheAPI() const
{
    return m_NetCacheAPI;
}

CGridWorkerNode::CGridWorkerNode(CNcbiApplicationAPI& app,
        IWorkerNodeJobFactory* job_factory) :
    m_Impl(new SGridWorkerNodeImpl(app, job_factory))
{
}

SGridWorkerNodeImpl::SGridWorkerNodeImpl(CNcbiApplicationAPI& app,
        IWorkerNodeJobFactory* job_factory) :
    m_JobProcessorFactory(job_factory),
    m_ThreadPool(NULL),
    m_MaxThreads(1),
    m_NSTimeout(DEFAULT_NS_TIMEOUT),
    m_CommitJobInterval(2),
    m_CheckStatusPeriod(2),
    m_ExclusiveJobSemaphore(1, 1),
    m_IsProcessingExclusiveJob(false),
    m_TotalMemoryLimit(0),
    m_TotalTimeLimit(0),
    m_StartupTime(0),
    m_CleanupEventSource(new CWorkerNodeCleanup()),
    m_Listener(new CGridWorkerNodeApp_Listener()),
    m_App(app),
    m_SingleThreadForced(false)
{
    if (!m_JobProcessorFactory.get())
        NCBI_THROW(CGridWorkerNodeException,
                 eJobFactoryIsNotSet, "The JobFactory is not set.");
}

void SGridWorkerNodeImpl::Init()
{
    CSynRegistryBuilder registry_builder(m_App);
    m_SynRegistry = registry_builder.Get();
    m_Registry.Reset(new CSynRegistryToIRegistry(m_SynRegistry));

    m_Listener->OnInit(this);

    // This parameter is deprecated, "[Diag]Merge_Lines" should be used instead
    if (m_SynRegistry->Get("log", "merge_lines", false)) {
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
    }

    m_NetScheduleAPI = new SNetScheduleAPIImpl(registry_builder, kEmptyStr);
    m_NetCacheAPI = new SNetCacheAPIImpl(registry_builder, kEmptyStr, kEmptyStr, kEmptyStr, m_NetScheduleAPI);
    m_JobProcessorFactory->Init(*this);
}

void CGridWorkerNode::Init()
{
    m_Impl->Init();
}

void CGridWorkerNode::Suspend(bool pullback, unsigned timeout)
{
    m_Impl->m_SuspendResume.GetLock()->Suspend(pullback, timeout);
}

void SSuspendResume::Suspend(bool pullback, unsigned timeout)
{
    if (pullback)
        SetJobPullbackTimer(timeout);
    if (m_Event.exchange(eSuspend) == eNoEvent)
        CGridGlobals::GetInstance().InterruptUDPPortListening();
}

void CGridWorkerNode::Resume()
{
    m_Impl->m_SuspendResume->Resume();
}

void SSuspendResume::Resume() volatile
{
    if (m_Event.exchange(eResume) == eNoEvent)
        CGridGlobals::GetInstance().InterruptUDPPortListening();
}

void SSuspendResume::SetJobPullbackTimer(unsigned seconds)
{
    m_JobPullbackTime = CDeadline(seconds);
    ++m_CurrentJobGeneration;
}

bool SSuspendResume::IsJobPullbackTimerExpired()
{
    return m_JobPullbackTime.IsExpired();
}

void SGridWorkerNodeImpl::x_WNCoreInit()
{
    _ASSERT(m_SynRegistry);

    if (!m_SingleThreadForced) {
        string max_threads = m_SynRegistry->Get("server", "max_threads", "8");
        if (NStr::CompareNocase(max_threads, "auto") == 0)
            m_MaxThreads = CSystemInfo::GetCpuCount();
        else {
            try {
                m_MaxThreads = NStr::StringToUInt(max_threads);
            }
            catch (exception&) {
                m_MaxThreads = CSystemInfo::GetCpuCount();
                ERR_POST_X(51, "Could not convert [server"
                    "] max_threads parameter to number.\n"
                    "Using \'auto\' option (" << m_MaxThreads << ").");
            }
        }
    }
    m_NSTimeout = m_SynRegistry->Get("server", "job_wait_timeout", DEFAULT_NS_TIMEOUT);

    {{
        string memlimitstr = m_SynRegistry->Get("server", "total_memory_limit", kEmptyStr);

        if (!memlimitstr.empty())
            m_TotalMemoryLimit = NStr::StringToUInt8_DataSize(memlimitstr);
    }}

    m_TotalTimeLimit = m_SynRegistry->Get("server", "total_time_limit", 0u);

    m_StartupTime = time(0);

    CGridGlobals::GetInstance().SetReuseJobObject(m_SynRegistry->Get("server", "reuse_job_object", false));
    CGridGlobals::GetInstance().SetWorker(this);

    m_LogRequested =         m_SynRegistry->Get("server", "log", false);
    m_ProgressLogRequested = m_SynRegistry->Get("server", "log_progress", false);
    m_ThreadPoolTimeout =    m_SynRegistry->Get("server", "thread_pool_timeout", 30);
}

void SGridWorkerNodeImpl::x_StartWorkerThreads()
{
    _ASSERT(m_MaxThreads > 0);
    _ASSERT(m_SynRegistry);

    m_ThreadPool = new CStdPoolOfThreads(m_MaxThreads, 0, 1, kMax_UInt,
            GetAppName() + "_wr");

    try {
        unsigned init_threads = m_SynRegistry->Get("server", "init_threads", 1);

        m_ThreadPool->Spawn(init_threads <= m_MaxThreads ?
                init_threads : m_MaxThreads);
    }
    catch (exception& ex) {
        ERR_POST_X(26, ex.what());
        CGridGlobals::GetInstance().RequestShutdown(
            CNetScheduleAdmin::eShutdownImmediate);
        throw;
    }
}

bool CRunningJobLimit::CountJob(const string& job_group,
        CJobRunRegistration* job_run_registration)
{
    if (m_MaxNumber == 0)
        return true;

    CFastMutexGuard guard(m_Mutex);

    pair<TJobCounter::iterator, bool> insertion(
            m_Counter.insert(TJobCounter::value_type(job_group, 1)));

    if (!insertion.second) {
        if (insertion.first->second == m_MaxNumber)
            return false;

        ++insertion.first->second;
    }

    job_run_registration->RegisterRun(this, insertion.first);

    return true;
}

int CGridWorkerNode::Run(
#ifdef NCBI_OS_UNIX
    ESwitch daemonize,
#endif
    string procinfo_file_name)
{
    return m_Impl->Run(
#ifdef NCBI_OS_UNIX
            daemonize,
#endif
            procinfo_file_name);
}

int SGridWorkerNodeImpl::Run(
#ifdef NCBI_OS_UNIX
    ESwitch daemonize,
#endif
    string procinfo_file_name)
{
    _ASSERT(m_SynRegistry);

    x_WNCoreInit();

    const SBuildInfo& build_info(m_App.GetFullVersion().GetBuildInfo());

    LOG_POST_X(50, Info << m_JobProcessorFactory->GetJobVersion() <<
            " build " << build_info.date << " tag " << build_info.tag);

    const CArgs& args = m_App.GetArgs();

    unsigned short start_port, end_port;

    {{
        string control_port_arg(args["control_port"] ?
                args["control_port"].AsString() :
                m_SynRegistry->Get("server", "control_port", "9300"));

        CTempString from_port, to_port;

        NStr::SplitInTwo(control_port_arg, "- ",
                from_port, to_port,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

        start_port = NStr::StringToNumeric<unsigned short>(from_port);
        end_port = to_port.empty() ? start_port : NStr::StringToNumeric<unsigned short>(to_port);
    }}

#ifdef NCBI_OS_UNIX
    bool is_daemon = daemonize != eDefault ? daemonize == eOn :
            m_SynRegistry->Get("server", "daemon", false);
#endif

    vector<string> vhosts;

    NStr::Split(m_SynRegistry->Get("server", "master_nodes", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        if (auto address = SSocketAddress::Parse(NStr::TruncateSpaces(*it))) {
            m_Masters.insert(std::move(address));
        }
    }

    if (vhosts.size() && m_Masters.empty()) {
        LOG_POST_X(41, Warning << "All hosts from master_nodes were ignored");
    }

    vhosts.clear();

    NStr::Split(m_SynRegistry->Get("server", "admin_hosts", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        unsigned int ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_AdminHosts.insert(ha);
    }

    if (vhosts.size() && m_AdminHosts.empty()) {
        LOG_POST_X(42, Warning << "All hosts from admin_hosts were ignored");
    }

    auto commit_job_interval = m_SynRegistry->Get("server", "commit_job_interval", m_CommitJobInterval);
    m_CommitJobInterval = static_cast<unsigned>(max(1, commit_job_interval));

    m_CheckStatusPeriod = m_SynRegistry->Get("server", "check_status_period", 2);
    if (m_CheckStatusPeriod == 0)
        m_CheckStatusPeriod = 1;

    auto default_timeout = m_SynRegistry->Get("server", "default_pullback_timeout", 0);
    m_SuspendResume.GetLock()->SetDefaultPullbackTimeout(default_timeout);

    if (m_SynRegistry->Has("server", "wait_server_timeout")) {
        ERR_POST_X(52, "[server"
            "] \"wait_server_timeout\" is not used anymore.\n"
            "Use [" << kNetScheduleAPIDriverName <<
            "] \"communication_timeout\" parameter instead.");
    }

    if (!procinfo_file_name.empty()) {
        // Make sure the process info file is writable.
        CFile proc_info_file(procinfo_file_name);
        if (proc_info_file.Exists()) {
            // Already exists
            if (!proc_info_file.CheckAccess(CFile::fWrite)) {
                fprintf(stderr, "'%s' is not writable.\n",
                        procinfo_file_name.c_str());
                return 21;
            }
        } else {
            // The process info file does not exist yet.
            // Create a temporary file in the designated directory
            // to make sure the location is writable.
            string test_file_name = procinfo_file_name + ".TEST";
            FILE* f = fopen(test_file_name.c_str(), "w");
            if (f == NULL) {
                perror(("Cannot create " + test_file_name).c_str());
                return 22;
            }
            fclose(f);
            // The worker node may fail (e.g. if the control port is busy)
            // before the process info file can be written; remove the
            // empty file to avoid confusion.
            remove(test_file_name.c_str());
        }
    }

#ifdef NCBI_OS_UNIX
    if (is_daemon) {
        LOG_POST_X(53, "Entering UNIX daemon mode...");
        CCurrentProcess::Daemonize("/dev/null",
                CProcess::fDF_KeepCWD |
                CProcess::fDF_KeepStdin |
                CProcess::fDF_KeepStdout |
                CProcess::fDF_AllowExceptions);
    }
#endif

    AddJobWatcher(CGridGlobals::GetInstance().GetJobWatcher());

    m_JobCommitterThread = new CJobCommitterThread(this);

    CRef<CGridControlThread> control_thread(
        new CGridControlThread(this, start_port, end_port));

    try {
        control_thread->Prepare();
    }
    catch (CServer_Exception& e) {
        if (e.GetErrCode() == CServer_Exception::eCouldntListen) {
            ERR_POST("Couldn't start a listener on a port from the "
                    "specified control port range; last port tried: " <<
                    control_thread->GetControlPort() << ". Another "
                    "process (probably another instance of this worker "
                    "node) is occupying the port(s).");
        } else {
            ERR_POST(e);
        }
        return 3;
    }

    const string& client(GetClientName());
    const string& host(CSocketAPI::gethostname());
    const string& port(NStr::NumericToString(control_thread->GetControlPort()));
    LOG_POST_X(60, "Control port: " << port);
    m_NetScheduleAPI->SetAuthParam("control_port", port);
    m_NetScheduleAPI->SetAuthParam("client_host", host);

    // This overrides default client node format (omits user name),
    // so deployed worker nodes could be determined by GRID Dashboard.
    m_NetScheduleAPI.SetClientNode(client + "::" + host + ':' + port);

    m_NetScheduleAPI.SetProgramVersion(m_JobProcessorFactory->GetJobVersion());

    CRequestContext& request_context = CDiagContext::GetRequestContext();

    request_context.SetSessionID(m_NetScheduleAPI->m_ClientSession);

#ifdef NCBI_OS_UNIX
    bool reliable_cleanup = m_SynRegistry->Get("server", "reliable_cleanup", false);

    if (reliable_cleanup) {
        TPid child_pid = CCurrentProcess::Fork();
        if (child_pid != 0) {
            CProcess child_process(child_pid);
            CProcess::CExitInfo exit_info;
            child_process.Wait(kInfiniteTimeoutMs, &exit_info);

            x_ClearNode();
            remove(procinfo_file_name.c_str());

            int retcode = 0;
            if (exit_info.IsPresent()) {
                if (exit_info.IsSignaled()) {
                    int signum = exit_info.GetSignal();
                    ERR_POST(Critical << "Child process " << child_pid <<
                            " was terminated by signal " << signum);
                    kill(CCurrentProcess::GetPid(), signum);
                } else if (exit_info.IsExited()) {
                    retcode = exit_info.GetExitCode();
                    if (retcode == 0) {
                        LOG_POST("Worker node process " << child_pid <<
                                " terminated normally (exit code 0)");
                    } else {
                        ERR_POST(Error << "Worker node process " << child_pid <<
                                " terminated with exit code " << retcode);
                    }
                }
            }
            // Exit the parent process with the same return code.
            return retcode;
        }
    }
#endif

    // Now that most of parameters have been checked, create the
    // "procinfo" file.
    if (!procinfo_file_name.empty()) {
        FILE* procinfo_file;
        if ((procinfo_file = fopen(procinfo_file_name.c_str(), "wt")) == NULL) {
            perror(procinfo_file_name.c_str());
            return 23;
        }
        fprintf(procinfo_file, "pid: %lu\nport: %s\n"
                "client_node: %s\nclient_session: %s\n",
                (unsigned long) CDiagContext::GetPID(),
                port.c_str(),
                m_NetScheduleAPI->m_ClientNode.c_str(),
                m_NetScheduleAPI->m_ClientSession.c_str());
        fclose(procinfo_file);
    }

    m_NSExecutor = m_NetScheduleAPI.GetExecutor();
    m_NSExecutor->retry_on_exception = eOff;

    CDeadline max_wait_for_servers(TWorkerNode_MaxWaitForServers::GetDefault());

    for (;;) {
        try {
            CNetScheduleAdmin::TQueueInfo queue_info;

            m_NetScheduleAPI.GetAdmin().GetQueueInfo(queue_info);

            m_QueueTimeout = NStr::StringToUInt(queue_info["timeout"]);

            m_QueueEmbeddedOutputSize = m_NetScheduleAPI->m_UseEmbeddedStorage ?
                    m_NetScheduleAPI.GetServerParams().max_output_size : 0;
            break;
        }
        catch (CException& e) {
            int s = (int) m_NSTimeout;
            do {
                ERR_POST(e);
                if (CGridGlobals::GetInstance().IsShuttingDown() ||
                        max_wait_for_servers.GetRemainingTime().IsZero())
                    return 1;
                SleepSec(1);
            } while (--s > 0);
        }
    }

    m_JobsPerClientIP.ResetJobCounter( (unsigned) m_SynRegistry->Get("server", "max_jobs_per_client_ip", 0));
    m_JobsPerSessionID.ResetJobCounter((unsigned) m_SynRegistry->Get("server", "max_jobs_per_session_id", 0));

    CWNJobWatcher& watcher(CGridGlobals::GetInstance().GetJobWatcher());
    watcher.SetMaxJobsAllowed(    m_SynRegistry->Get("server", "max_total_jobs", 0));
    watcher.SetMaxFailuresAllowed(m_SynRegistry->Get("server", "max_failed_jobs", 0));
    watcher.SetInfiniteLoopTime(  m_SynRegistry->Get("server", "infinite_loop_time", 0));
    CGridGlobals::GetInstance().SetUDPPort(
            m_NSExecutor->m_NotificationHandler.GetPort());

    IWorkerNodeIdleTask* task = NULL;

    unsigned idle_run_delay = m_SynRegistry->Get("server", "idle_run_delay", 30);
    unsigned auto_shutdown  = m_SynRegistry->Get("server", "auto_shutdown_if_idle", 0);

    if (idle_run_delay > 0)
        task = m_JobProcessorFactory->GetIdleTask();
    if (task || auto_shutdown > 0) {
        m_IdleThread.Reset(new CWorkerNodeIdleThread(task, this,
                task ? idle_run_delay : auto_shutdown, auto_shutdown));
        m_IdleThread->Run();
        AddJobWatcher(*(new CIdleWatcher(*m_IdleThread)), eTakeOwnership);
    }

    m_JobCommitterThread->Run();
    control_thread->Run();

    LOG_POST_X(54, Info << "\n=================== NEW RUN : " <<
        CGridGlobals::GetInstance().GetStartTime().AsString() <<
            " ===================\n" <<
        m_JobProcessorFactory->GetJobVersion() << " build " <<
        build_info.date << " tag " << build_info.tag <<
        " is started.\n"
        "Waiting for control commands on " << CSocketAPI::gethostname() <<
            ":" << control_thread->GetControlPort() << "\n"
        "Queue name: " << GetQueueName() << "\n"
        "Maximum job threads: " << m_MaxThreads << "\n");

    m_Listener->OnGridWorkerStart();

    x_StartWorkerThreads();

    {
        CRef<CMainLoopThread> main_loop_thread(new CMainLoopThread(this));

        main_loop_thread->Run();
        main_loop_thread->Join();
    }

    LOG_POST_X(31, Info << "Shutting down...");

    bool force_exit = m_SynRegistry->Get("server", "force_exit", false);
    if (force_exit) {
        ERR_POST_X(45, "Force exit (worker threads will not be waited for)");
    } else
        x_StopWorkerThreads();

    LOG_POST_X(55, Info << "Stopping the job committer thread...");
    m_JobCommitterThread->Stop();

    CNcbiOstrstream os;
    CGridGlobals::GetInstance().GetJobWatcher().Print(os);
    LOG_POST_X(56, Info << string(CNcbiOstrstreamToString(os)));

    if (m_IdleThread) {
        if (!m_IdleThread->IsShutdownRequested()) {
            LOG_POST_X(57, "Stopping Idle thread...");
            m_IdleThread->RequestShutdown();
        }
        m_IdleThread->Join();
    }

    m_JobCommitterThread->Join();

#ifdef NCBI_OS_UNIX
    // Clear the node only if reliable CLRN mode is not enabled.
    // Otherwise, the node will be cleared in the parent process.
    if (!reliable_cleanup)
#endif
    {
        x_ClearNode();
        remove(procinfo_file_name.c_str());
    }

    int exit_code = x_WNCleanUp();

    LOG_POST(Info << "Stopping the socket server thread...");
    control_thread->Stop();
    control_thread->Join();

    LOG_POST_X(38, Info << "Worker Node has been stopped.");

    if (force_exit) {
        SleepMilliSec(200);
        _exit(exit_code);
    }

    return exit_code;
}

void SGridWorkerNodeImpl::x_StopWorkerThreads()
{
    if (m_ThreadPool != NULL) {
        try {
            LOG_POST_X(32, Info << "Stopping worker threads...");
            m_ThreadPool->KillAllThreads(true);
            delete m_ThreadPool;
            m_ThreadPool = NULL;
        }
        catch (exception& ex) {
            ERR_POST_X(33, "Could not stop worker threads: " << ex.what());
        }
    }
}

void SGridWorkerNodeImpl::x_ClearNode()
{
    try {
        m_NetScheduleAPI->x_ClearNode();
    }
    catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError
            || NStr::Find(ex.what(), "Server error:Unknown request") == NPOS) {
            ERR_POST_X(35, "Could not unregister from NetSchedule services: "
                       << ex);
        }
    }
    catch (exception& ex) {
        ERR_POST_X(36, "Could not unregister from NetSchedule services: " <<
                ex.what());
    }
}

void CGridWorkerNode::RequestShutdown()
{
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}


void SGridWorkerNodeImpl::AddJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                                           EOwnership owner)
{
    if (m_Watchers.find(&job_watcher) == m_Watchers.end())
        m_Watchers[&job_watcher] = owner == eTakeOwnership ?
                AutoPtr<IWorkerNodeJobWatcher>(&job_watcher) :
                AutoPtr<IWorkerNodeJobWatcher>();
}

void CGridWorkerNode::SetListener(IGridWorkerNodeApp_Listener* listener)
{
    m_Impl->m_Listener.reset(
            listener ? listener : new CGridWorkerNodeApp_Listener());
}

void SGridWorkerNodeImpl::x_NotifyJobWatchers(
        const CWorkerNodeJobContext& job_context,
        IWorkerNodeJobWatcher::EEvent event)
{
    CFastMutexGuard guard(m_JobWatcherMutex);
    NON_CONST_ITERATE(TJobWatchers, it, m_Watchers) {
        try {
            const_cast<IWorkerNodeJobWatcher*>(it->first)->Notify(job_context,
                    event);
        }
        NCBI_CATCH_ALL_X(66, "Error while notifying a job watcher");
    }
}

bool CGridWorkerNode::IsHostInAdminHostsList(const string& host) const
{
    if (m_Impl->m_AdminHosts.empty())
        return true;
    unsigned int ha = CSocketAPI::gethostbyname(host);
    if (m_Impl->m_AdminHosts.find(ha) != m_Impl->m_AdminHosts.end())
        return true;
    unsigned int ha_lh = CSocketAPI::gethostbyname("");
    if (ha == ha_lh) {
        ha = CSocketAPI::gethostbyname("localhost");
        if (m_Impl->m_AdminHosts.find(ha) != m_Impl->m_AdminHosts.end())
            return true;
    }
    return false;
}

bool SGridWorkerNodeImpl::x_AreMastersBusy() const
{
    ITERATE(set<SSocketAddress>, it, m_Masters) {
        STimeout tmo = {0, 500};
        CSocket socket(it->host, it->port, &tmo, eOff);
        if (socket.GetStatus(eIO_Open) != eIO_Success)
            continue;

        CNcbiOstrstream os;
        os << GetClientName() << endl <<
                GetQueueName()  << ";" <<
                GetServiceName() << endl <<
                "GETLOAD" << endl << ends;

        string msg = CNcbiOstrstreamToString(os);
        if (socket.Write(msg.data(), msg.size()) != eIO_Success)
            continue;
        string reply;
        if (socket.ReadLine(reply) != eIO_Success)
            continue;
        if (NStr::StartsWith(reply, "ERR:")) {
            NStr::Replace(reply, "ERR:", "", msg);
            ERR_POST_X(43, "Worker Node at " << it->AsString() <<
                " returned error: " << msg);
        } else if (NStr::StartsWith(reply, "OK:")) {
            NStr::Replace(reply, "OK:", "", msg);
            try {
                int load = NStr::StringToInt(msg);
                if (load > 0)
                    return false;
            } catch (exception&) {}
        } else {
            ERR_POST_X(44, "Worker Node at " << it->AsString() <<
                " returned unknown reply: " << reply);
        }
    }
    return true;
}

bool g_IsRequestStartEventEnabled()
{
    return s_ReqEventsDisabled == CGridWorkerNode::eEnableStartStop;
}

bool g_IsRequestStopEventEnabled()
{
    return s_ReqEventsDisabled == CGridWorkerNode::eEnableStartStop ||
            s_ReqEventsDisabled == CGridWorkerNode::eDisableStartOnly;
}

bool SGridWorkerNodeImpl::EnterExclusiveMode()
{
    if (m_ExclusiveJobSemaphore.TryWait()) {
        _ASSERT(!m_IsProcessingExclusiveJob);

        m_IsProcessingExclusiveJob = true;
        return true;
    }
    return false;
}

void SGridWorkerNodeImpl::LeaveExclusiveMode()
{
    _ASSERT(m_IsProcessingExclusiveJob);

    m_IsProcessingExclusiveJob = false;
    m_ExclusiveJobSemaphore.Post();
}

bool SGridWorkerNodeImpl::WaitForExclusiveJobToFinish()
{
    if (m_ExclusiveJobSemaphore.TryWait(m_NSTimeout)) {
        m_ExclusiveJobSemaphore.Post();
        return true;
    }
    return false;
}

void CGridWorkerNode::DisableDefaultRequestEventLogging(
        CGridWorkerNode::EDisabledRequestEvents disabled_events)
{
    s_ReqEventsDisabled = disabled_events;
}

void CGridWorkerNode::ForceSingleThread()
{
    m_Impl->m_SingleThreadForced = true;
}

IWorkerNodeJobFactory& CGridWorkerNode::GetJobFactory()
{
    return *m_Impl->m_JobProcessorFactory;
}

unsigned CGridWorkerNode::GetMaxThreads() const
{
    return m_Impl->m_MaxThreads;
}

Uint8 CGridWorkerNode::GetTotalMemoryLimit() const
{
    return m_Impl->m_TotalMemoryLimit;
}

unsigned CGridWorkerNode::GetTotalTimeLimit() const
{
    return m_Impl->m_TotalTimeLimit;
}

time_t CGridWorkerNode::GetStartupTime() const
{
    return m_Impl->m_StartupTime;
}

unsigned CGridWorkerNode::GetQueueTimeout() const
{
    return m_Impl->m_QueueTimeout;
}

unsigned CGridWorkerNode::GetCommitJobInterval() const
{
    return m_Impl->m_CommitJobInterval;
}

unsigned CGridWorkerNode::GetCheckStatusPeriod() const
{
    return m_Impl->m_CheckStatusPeriod;
}

string CGridWorkerNode::GetAppName() const
{
    CFastMutexGuard guard(m_Impl->m_JobProcessorMutex);
    return m_Impl->m_JobProcessorFactory->GetAppName();
}

CGridWorkerNode::TVersion CGridWorkerNode::GetAppVersion() const
{
    const CVersionAPI& version(m_Impl->m_App.GetFullVersion());
    const CVersionInfo& version_info(version.GetVersionInfo());
    const SBuildInfo& build_info(version.GetBuildInfo());

    const auto& job_factory(m_Impl->m_JobProcessorFactory);
    _ASSERT(job_factory.get());
    const string job_version(job_factory->GetAppVersion());

    return make_pair(job_version.empty() ? version_info.Print() : job_version, build_info);
}

CNetCacheAPI CGridWorkerNode::GetNetCacheAPI() const
{
    return m_Impl->m_NetCacheAPI;
}

CNetScheduleAPI CGridWorkerNode::GetNetScheduleAPI() const
{
    return m_Impl->m_NetScheduleAPI;
}

CNetScheduleExecutor CGridWorkerNode::GetNSExecutor() const
{
    return m_Impl->m_NSExecutor;
}

IWorkerNodeCleanupEventSource* CGridWorkerNode::GetCleanupEventSource()
{
    return m_Impl->m_CleanupEventSource;
}

bool CGridWorkerNode::IsSuspended() const
{
    return m_Impl->m_SuspendResume->IsSuspended();
}

const string& CGridWorkerNode::GetQueueName() const
{
    return m_Impl->GetQueueName();
}

const string& CGridWorkerNode::GetClientName() const
{
    return m_Impl->GetClientName();
}

const string& CGridWorkerNode::GetServiceName() const
{
    return m_Impl->GetServiceName();
}

END_NCBI_SCOPE
