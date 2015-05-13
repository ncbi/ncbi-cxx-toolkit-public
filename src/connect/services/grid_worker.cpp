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
        unsigned int start_port, unsigned int end_port) : m_Control(
            new CWorkerNodeControlServer(worker_node, start_port, end_port)),
          m_ThreadName(worker_node->GetAppName() + "_ct")
    {
    }

    void Prepare() {m_Control->StartListening();}

    unsigned short GetControlPort() {return m_Control->GetControlPort();}

    void Stop()
    {
        if (m_Control.get())
            m_Control->RequestShutdown();
    }

protected:
    virtual void* Main(void)
    {
        SetCurrentThreadName(m_ThreadName);
        m_Control->Run();
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
    auto_ptr<CWorkerNodeControlServer> m_Control;
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
    bool x_IsAutoShutdownTime() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_AutoShutdown > 0 &&
                m_AutoShutdownSW.Elapsed() > m_AutoShutdown;
    }

    IWorkerNodeIdleTask* m_Task;
    SGridWorkerNodeImpl* m_WorkerNode;
    auto_ptr<CWorkerNodeIdleTaskContext> m_TaskContext;
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
        if ( x_IsAutoShutdownTime() ) {
            LOG_POST_X(47, Info <<
                "There are no more jobs to be done. Exiting.");
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
                    if (x_IsAutoShutdownTime()) {
                        LOG_POST_X(48, Info <<
                            "There are no more jobs to be done. Exiting.");
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
    if (!m_TaskContext.get())
        m_TaskContext.reset(new CWorkerNodeIdleTaskContext(*this));
    return *m_TaskContext;
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

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
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


/////////////////////////////////////////////////////////////////////////////
//
CGridWorkerNode::CGridWorkerNode(CNcbiApplication& app,
        IWorkerNodeJobFactory* job_factory) :
    m_Impl(new SGridWorkerNodeImpl(app, job_factory))
{
}

SGridWorkerNodeImpl::SGridWorkerNodeImpl(CNcbiApplication& app,
        IWorkerNodeJobFactory* job_factory) :
    m_JobProcessorFactory(job_factory),
    m_ThreadPool(NULL),
    m_MaxThreads(1),
    m_NSTimeout(DEFAULT_NS_TIMEOUT),
    m_CommitJobInterval(COMMIT_JOB_INTERVAL_DEFAULT),
    m_CheckStatusPeriod(2),
    m_ExclusiveJobSemaphore(1, 1),
    m_IsProcessingExclusiveJob(false),
    m_TotalMemoryLimit(0),
    m_TotalTimeLimit(0),
    m_StartupTime(0),
    m_CleanupEventSource(new CWorkerNodeCleanup()),
    m_SuspendResumeEvent(NO_EVENT),
    m_TimelineIsSuspended(false),
    m_CurrentJobGeneration(0),
    m_DefaultPullbackTimeout(0),
    m_JobPullbackTime(0),
    m_Listener(new CGridWorkerNodeApp_Listener()),
    m_App(app),
    m_SingleThreadForced(false)
{
    if (!m_JobProcessorFactory.get())
        NCBI_THROW(CGridWorkerNodeException,
                 eJobFactoryIsNotSet, "The JobFactory is not set.");
}

void CGridWorkerNode::Init()
{
    m_Impl->m_Listener->OnInit(&m_Impl->m_App);

    IRWRegistry& reg = m_Impl->m_App.GetConfig();

    if (reg.GetBool("log", "merge_lines", false)) {
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
    }

    reg.Set(kNetScheduleAPIDriverName, "discover_low_priority_servers", "true");

    m_Impl->m_NetScheduleAPI = CNetScheduleAPI(reg);
    m_Impl->m_NetCacheAPI = CNetCacheAPI(reg,
            kEmptyStr, m_Impl->m_NetScheduleAPI);
}

void CGridWorkerNode::Suspend(bool pullback, unsigned timeout)
{
    if (pullback)
        m_Impl->SetJobPullbackTimer(timeout);
    if (SwapPointers(&m_Impl->m_SuspendResumeEvent, SUSPEND_EVENT) == NO_EVENT)
        CGridGlobals::GetInstance().InterruptUDPPortListening();
}

void CGridWorkerNode::Resume()
{
    if (SwapPointers(&m_Impl->m_SuspendResumeEvent, RESUME_EVENT) == NO_EVENT)
        CGridGlobals::GetInstance().InterruptUDPPortListening();
}

void SGridWorkerNodeImpl::SetJobPullbackTimer(unsigned seconds)
{
    CFastMutexGuard mutex_guard(m_JobPullbackMutex);
    m_JobPullbackTime = CDeadline(seconds);
    ++m_CurrentJobGeneration;
}

bool SGridWorkerNodeImpl::CheckForPullback(unsigned job_generation)
{
    CFastMutexGuard mutex_guard(m_JobPullbackMutex);
    return job_generation != m_CurrentJobGeneration &&
            m_JobPullbackTime.IsExpired();
}

void SGridWorkerNodeImpl::x_WNCoreInit()
{
    const string kServerSec("server");

    const IRegistry& reg = m_App.GetConfig();

    if (!m_SingleThreadForced) {
        string max_threads = reg.GetString(kServerSec, "max_threads", "auto");
        if (NStr::CompareNocase(max_threads, "auto") == 0)
            m_MaxThreads = GetCpuCount();
        else {
            try {
                m_MaxThreads = NStr::StringToUInt(max_threads);
            }
            catch (exception&) {
                m_MaxThreads = GetCpuCount();
                ERR_POST_X(51, "Could not convert [" << kServerSec <<
                    "] max_threads parameter to number.\n"
                    "Using \'auto\' option (" << m_MaxThreads << ").");
            }
        }
    }
    m_NSTimeout = reg.GetInt(kServerSec,
            "job_wait_timeout", DEFAULT_NS_TIMEOUT, 0, IRegistry::eReturn);

    {{
        string memlimitstr(reg.GetString(kServerSec,
                "total_memory_limit", kEmptyStr, IRegistry::eReturn));

        if (!memlimitstr.empty())
            m_TotalMemoryLimit = NStr::StringToUInt8_DataSize(memlimitstr);
    }}

    m_TotalTimeLimit = reg.GetInt(kServerSec,
            "total_time_limit", 0, 0, IRegistry::eReturn);

    m_StartupTime = time(0);

    CGridGlobals::GetInstance().SetReuseJobObject(reg.GetBool(kServerSec,
        "reuse_job_object", false, 0, CNcbiRegistry::eReturn));
    CGridGlobals::GetInstance().SetWorker(this);

    m_LogRequested = reg.GetBool(kServerSec,
        "log", false, 0, IRegistry::eReturn);
    m_ProgressLogRequested = reg.GetBool(kServerSec,
        "log_progress", false, 0, IRegistry::eReturn);
    m_ThreadPoolTimeout = reg.GetInt(kServerSec,
        "thread_pool_timeout", 30, 0, IRegistry::eReturn);
}

void SGridWorkerNodeImpl::x_StartWorkerThreads()
{
    _ASSERT(m_MaxThreads > 0);

    m_ThreadPool = new CStdPoolOfThreads(m_MaxThreads, 0, 1, kMax_UInt,
            GetAppName() + "_wr");

    try {
        unsigned init_threads = m_App.GetConfig().GetInt("server",
                "init_threads", 1, 0, IRegistry::eReturn);

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
    x_WNCoreInit();

    const string kServerSec("server");

    LOG_POST_X(50, Info << m_JobProcessorFactory->GetJobVersion() <<
            " build " WN_BUILD_DATE);

    const IRegistry& reg = m_App.GetConfig();

    const CArgs& args = m_App.GetArgs();

    unsigned int start_port, end_port;

    {{
        string control_port_arg(args["control_port"] ?
                args["control_port"].AsString() :
                reg.GetString(kServerSec, "control_port", "9300"));

        CTempString from_port, to_port;

        NStr::SplitInTwo(control_port_arg, "- ",
                from_port, to_port, NStr::eMergeDelims);

        start_port = NStr::StringToUInt(from_port);
        end_port = to_port.empty() ? start_port : NStr::StringToUInt(to_port);
    }}

#ifdef NCBI_OS_UNIX
    bool is_daemon = daemonize != eDefault ? daemonize == eOn :
            reg.GetBool(kServerSec, "daemon", false, 0, CNcbiRegistry::eReturn);
#endif

    vector<string> vhosts;

    NStr::Tokenize(reg.GetString(kServerSec,
        "master_nodes", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        string host, port;
        NStr::SplitInTwo(NStr::TruncateSpaces(*it), ":", host, port);
        if (!host.empty() && !port.empty())
            m_Masters.insert(SServerAddress(host,
                    (unsigned short) NStr::StringToUInt(port)));
    }

    vhosts.clear();

    NStr::Tokenize(reg.GetString(kServerSec,
        "admin_hosts", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        unsigned int ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_AdminHosts.insert(ha);
    }

    m_CommitJobInterval = reg.GetInt(kServerSec, "commit_job_interval",
            COMMIT_JOB_INTERVAL_DEFAULT, 0, IRegistry::eReturn);
    if (m_CommitJobInterval == 0)
        m_CommitJobInterval = 1;

    m_CheckStatusPeriod = reg.GetInt(kServerSec,
            "check_status_period", 2, 0, IRegistry::eReturn);
    if (m_CheckStatusPeriod == 0)
        m_CheckStatusPeriod = 1;

    m_DefaultPullbackTimeout = reg.GetInt(kServerSec,
            "default_pullback_timeout", 0, 0, IRegistry::eReturn);

    if (reg.HasEntry(kServerSec, "wait_server_timeout")) {
        ERR_POST_X(52, "[" << kServerSec <<
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
        CProcess::Daemonize("/dev/null",
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

    string control_port_str(
            NStr::NumericToString(control_thread->GetControlPort()));
    LOG_POST_X(60, "Control port: " << control_port_str);
    m_NetScheduleAPI.SetAuthParam("control_port", control_port_str);
    m_NetScheduleAPI.SetAuthParam("client_host", CSocketAPI::gethostname());

    if (m_NetScheduleAPI->m_ClientNode.empty()) {
        string client_node(m_NetScheduleAPI->
                m_Service->m_ServerPool->m_ClientName);
        client_node.append(2, ':');
        client_node.append(CSocketAPI::gethostname());
        client_node.append(1, ':');
        client_node.append(NStr::NumericToString(
            control_thread->GetControlPort()));
        m_NetScheduleAPI.SetClientNode(client_node);

        string session(NStr::NumericToString(CProcess::GetCurrentPid()) + '@');
        session += NStr::NumericToString(GetFastLocalTime().GetTimeT());
        session += ':';
        session += GetDiagContext().GetStringUID();

        m_NetScheduleAPI.SetClientSession(session);
    }

    m_NetScheduleAPI.SetProgramVersion(m_JobProcessorFactory->GetJobVersion());

    CRequestContext& request_context = CDiagContext::GetRequestContext();

    request_context.SetSessionID(m_NetScheduleAPI->m_ClientSession);

#ifdef NCBI_OS_UNIX
    bool reliable_cleanup = reg.GetBool(kServerSec,
            "reliable_cleanup", false, 0, CNcbiRegistry::eReturn);

    if (reliable_cleanup) {
        TPid child_pid = CProcess::Fork();
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
                    kill(CProcess::GetCurrentPid(), signum);
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
                control_port_str.c_str(),
                m_NetScheduleAPI->m_ClientNode.c_str(),
                m_NetScheduleAPI->m_ClientSession.c_str());
        fclose(procinfo_file);
    }

    m_NSExecutor = m_NetScheduleAPI.GetExecutor();
    m_NSExecutor->m_WorkerNodeMode = true;

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
            LOG_POST(Error << e);
            int s = (int) m_NSTimeout;
            do {
                if (CGridGlobals::GetInstance().IsShuttingDown() ||
                        max_wait_for_servers.GetRemainingTime().IsZero())
                    return 1;
                SleepSec(1);
            } while (--s > 0);
        }
    }

    m_JobsPerClientIP.ResetJobCounter((unsigned) reg.GetInt(kServerSec,
            "max_jobs_per_client_ip", 0, 0, IRegistry::eReturn));
    m_JobsPerSessionID.ResetJobCounter((unsigned) reg.GetInt(kServerSec,
            "max_jobs_per_session_id", 0, 0, IRegistry::eReturn));

    CWNJobWatcher& watcher(CGridGlobals::GetInstance().GetJobWatcher());
    watcher.SetMaxJobsAllowed(reg.GetInt(kServerSec,
            "max_total_jobs", 0, 0, IRegistry::eReturn));
    watcher.SetMaxFailuresAllowed(reg.GetInt(kServerSec,
            "max_failed_jobs", 0, 0, IRegistry::eReturn));
    watcher.SetInfiniteLoopTime(reg.GetInt(kServerSec,
            "infinite_loop_time", 0, 0, IRegistry::eReturn));
    CGridGlobals::GetInstance().SetUDPPort(
            m_NSExecutor->m_NotificationHandler.GetPort());

    IWorkerNodeIdleTask* task = NULL;

    unsigned idle_run_delay = reg.GetInt(kServerSec,
        "idle_run_delay", 30, 0, IRegistry::eReturn);

    unsigned auto_shutdown = reg.GetInt(kServerSec,
        "auto_shutdown_if_idle", 0, 0, IRegistry::eReturn);

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
        m_JobProcessorFactory->GetJobVersion() << " build " WN_BUILD_DATE <<
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

    bool force_exit = reg.GetBool(kServerSec,
            "force_exit", false, 0, CNcbiRegistry::eReturn);
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
};

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
    ITERATE(set<SServerAddress>, it, m_Masters) {
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

int CGridWorkerNode::GetTotalTimeLimit() const
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

string CGridWorkerNode::GetAppVersion() const
{
    CFastMutexGuard guard(m_Impl->m_JobProcessorMutex);
    return m_Impl->m_JobProcessorFactory->GetAppVersion();
}

string CGridWorkerNode::GetBuildDate() const
{
    CFastMutexGuard guard(m_Impl->m_JobProcessorMutex);
    return m_Impl->m_JobProcessorFactory->GetAppBuildDate();
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
    return m_Impl->m_TimelineIsSuspended;
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
