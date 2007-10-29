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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>

#include <connect/services/netcache_client.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/grid_worker_app_impl.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/grid_control_thread.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/error_codes.hpp>


#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobWatchers
/// @internal

class CWorkerNodeJobWatchers : public IWorkerNodeJobWatcher
{
public:
    virtual ~CWorkerNodeJobWatchers() {};
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        NON_CONST_ITERATE(TCont, it, m_Watchers) {
            IWorkerNodeJobWatcher* watcher = 
                const_cast<IWorkerNodeJobWatcher*>(it->first);
            watcher->Notify(job, event);
        }        
    }

    void AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher, EOwnership owner)
    {
        TCont::const_iterator it = m_Watchers.find(&job_watcher);
        if (it == m_Watchers.end()) {
            if (owner == eTakeOwnership) 
                m_Watchers[&job_watcher] = AutoPtr<IWorkerNodeJobWatcher>(&job_watcher);
            else
                m_Watchers[&job_watcher] = AutoPtr<IWorkerNodeJobWatcher>();
        }
    } 

private:
    typedef map<IWorkerNodeJobWatcher*, AutoPtr<IWorkerNodeJobWatcher> > TCont;
    TCont m_Watchers;
};

/////////////////////////////////////////////////////////////////////////////
//
//    CLogWatcher
/// @internal
/*
class CLogWatcher : public IWorkerNodeJobWatcher
{
public:
    CLogWatcher(CNcbiOstream* os) : m_Os(os) {}
    virtual ~CLogWatcher() {};
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        if (event == eJobStopped) {
            if (m_Os) {
                if (!IsDiagStream(m_Os)) {
                    *m_Os << "The Diag Stream was hijacked (probably by job : " << 
                        job.GetJobKey() << ")" << endl;
                    m_Os->flush();
                    m_Os = NULL;
                }
            }
        }
    }

private:
    CNcbiOstream* m_Os;
};
*/
/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNodeThread
/// @internal
/*
class CGridWorkerNodeThread : public CThread
{
public:
    CGridWorkerNodeThread(CGridWorkerNode& worker_node) 
        : m_WorkerNode(worker_node) {}

    ~CGridWorkerNodeThread() {}
protected:

    virtual void* Main(void)
    {
        m_WorkerNode.Start();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        LOG_POST_X(45, CTime(CTime::eCurrent).AsString() << " Worker Node Thread exited.");
    }

private:
    CGridWorkerNode& m_WorkerNode;
};
*/

/////////////////////////////////////////////////////////////////////////////
//
//     CGridControleThread
/// @internal
class CGridControlThread : public CThread
{
public:
    CGridControlThread(unsigned int start_port, unsigned int end_port, CGridWorkerNode& wnode) 
        : m_Control(new CWorkerNodeControlThread(start_port, end_port, wnode)) {}

    ~CGridControlThread() {}

    void Prepare() { m_Control->StartListening(); }

    unsigned int GetControlPort() { return m_Control->GetControlPort(); }
    void Stop() { if (m_Control.get()) m_Control->RequestShutdown(); }
protected:

    virtual void* Main(void)
    {
        m_Control->Run();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        LOG_POST_X(46, CTime(CTime::eCurrent).AsString() << " Control Thread has been stopped.");
    }

private:
    auto_ptr<CWorkerNodeControlThread> m_Control;
};

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleThread      -- 
class CWorkerNodeIdleThread : public CThread
{
public:
    CWorkerNodeIdleThread(IWorkerNodeIdleTask*, 
                          CGridWorkerNode& worker_node,
                          unsigned run_delay,
                          //bool exclusive_mode,
                          unsigned int auto_shutdown);

    void RequestShutdown() 
    { 
        m_ShutdownFlag = true; 
        m_Wait1.Post(); 
        m_Wait2.Post(); 
    }
    void Schedule() 
    { 
        CFastMutexGuard guard(m_Mutext);
        m_AutoShutdownSW.Restart();
        if (m_StopFlag) {
            m_StopFlag = false; 
            m_Wait1.Post(); 
        }
    }
    void Suspend() 
    { 
        CFastMutexGuard guard(m_Mutext);
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
        CFastMutexGuard guard(m_Mutext);
        return  m_AutoShutdown > 0 ?
                min( m_AutoShutdown - (unsigned int)m_AutoShutdownSW.Elapsed(), m_RunInterval ) 
                : m_RunInterval;
    }
    bool x_GetStopFlag() const 
    { 
        CFastMutexGuard guard(m_Mutext);
        return m_StopFlag; 
    }
    bool x_IsAutoShutdownTime() const 
    {
        CFastMutexGuard guard(m_Mutext);
        return m_AutoShutdown > 0 ? m_AutoShutdownSW.Elapsed() > m_AutoShutdown : false;
    }

    IWorkerNodeIdleTask* m_Task;
    CGridWorkerNode& m_WorkerNode;
    auto_ptr<CWorkerNodeIdleTaskContext> m_TaskContext;
    mutable CSemaphore  m_Wait1; 
    mutable CSemaphore  m_Wait2; 
    volatile bool       m_StopFlag;
    volatile bool       m_ShutdownFlag;
    unsigned int        m_RunInterval;
    //bool                m_ExclusiveMode;
    unsigned int        m_AutoShutdown;
    CStopWatch          m_AutoShutdownSW;
    mutable CFastMutex  m_Mutext;

    CWorkerNodeIdleThread(const CWorkerNodeIdleThread&);
    CWorkerNodeIdleThread& operator=(const CWorkerNodeIdleThread&);
};

CWorkerNodeIdleThread::CWorkerNodeIdleThread(IWorkerNodeIdleTask* task,
                                             CGridWorkerNode& worker_node,
                                             unsigned run_delay,
                                             //bool exclusive_mode,
                                             unsigned int auto_shutdown)
    : m_Task(task), m_WorkerNode(worker_node),
      m_Wait1(0,100000), m_Wait2(0,1000000),
      m_StopFlag(false), m_ShutdownFlag(false),
      m_RunInterval(run_delay), //m_ExclusiveMode(exclusive_mode),
      m_AutoShutdown(auto_shutdown), m_AutoShutdownSW(CStopWatch::eStart)
{
}
void* CWorkerNodeIdleThread::Main()
{
    while (!m_ShutdownFlag) {
        if ( x_IsAutoShutdownTime() ) {
            LOG_POST_X(47, CTime(CTime::eCurrent).AsString() 
                       << " There are no more jobs to be done. Exiting.");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            break;
        }            
        unsigned int interval = m_AutoShutdown > 0 ? min (m_RunInterval,m_AutoShutdown) : m_RunInterval;
        if (m_Wait1.TryWait(interval, 0)) {
            if (m_ShutdownFlag)
                continue;
            interval = x_GetInterval();
            if (m_Wait2.TryWait(interval, 0)) {
                continue; 
            }
        } 
        if (m_Task && !x_GetStopFlag()) {
            //if (m_ExclusiveMode)
            //    m_WorkerNode.PutOnHold(true);
            try {
                do {
                    if ( x_IsAutoShutdownTime() ) {
                        LOG_POST_X(48, CTime(CTime::eCurrent).AsString() 
                                   << " There are no more jobs to be done. Exiting.");
                        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
                        m_ShutdownFlag = true;
                        break;
                    }            
                    GetContext().Reset();
                    m_Task->Run(GetContext());
                } while( GetContext().NeedRunAgain() && !m_ShutdownFlag);
            } NCBI_CATCH_ALL_X(58, "CWorkerNodeIdleThread::Main: Idle Task failed");
            //if (m_ExclusiveMode)
            //    m_WorkerNode.PutOnHold(false);
        }
    }
    return 0;
}

void CWorkerNodeIdleThread::OnExit(void)
{
    LOG_POST_X(49, CTime(CTime::eCurrent).AsString() << " Idle Thread has been stopped.");
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
CWorkerNodeIdleTaskContext::
CWorkerNodeIdleTaskContext(CWorkerNodeIdleThread& thread)
    : m_Thread(thread), m_RunAgain(false)
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
    CIdleWatcher(CWorkerNodeIdleThread& idle) 
        : m_Idle(idle), m_RunningJobs(0) {}
    virtual ~CIdleWatcher() {};
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        if (event == eJobStarted) {
            ++m_RunningJobs;
            m_Idle.Suspend();
        } else if (event == eJobStopped) {
            --m_RunningJobs;
            if (m_RunningJobs == 0)
                m_Idle.Schedule();
        }
    }

private:
    CWorkerNodeIdleThread& m_Idle;
    volatile int m_RunningJobs;
};


/////////////////////////////////////////////////////////////////////////////
//

CGridWorkerApp_Impl::CGridWorkerApp_Impl(
                               CNcbiApplication&          app,
                               IWorkerNodeJobFactory*     job_factory, 
                               IBlobStorageFactory*       storage_factory,
                               INetScheduleClientFactory* client_factory)
: m_JobFactory(job_factory), m_StorageFactory(storage_factory),
  m_ClientFactory(client_factory),
  m_App(app), m_SingleThreadForced(false)
{
    if (!m_JobFactory.get())
        NCBI_THROW(CGridWorkerAppException, 
                 eJobFactoryIsNotSet, "The JobFactory is not set.");

}

CGridWorkerApp_Impl::~CGridWorkerApp_Impl()
{
}

void CGridWorkerApp_Impl::Init()
{
    //    SetDiagPostLevel(eDiag_Info);
    //    SetDiagPostFlag(eDPF_DateTime);
  
    IRWRegistry& reg = m_App.GetConfig();
    reg.Set(kNetScheduleAPIDriverName, "discover_low_priority_servers", "true");

    if (!m_StorageFactory.get()) 
        m_StorageFactory.reset(new CBlobStorageFactory(reg));
    if (!m_ClientFactory.get()) 
        m_ClientFactory.reset(new CNetScheduleClientFactory(reg));
}

const string kServerSec = "server";

static void s_ParseControlPorts(const string& sports, unsigned int& start_port, unsigned int& end_port)
{
    string sport1, sport2;
    NStr::SplitInTwo(sports, "-", sport1, sport2);
    if (sport2.empty()) sport2 = sport1;
    start_port = NStr::StringToUInt(sport1);
    end_port = NStr::StringToUInt(sport2);
}

int CGridWorkerApp_Impl::Run()
{
    LOG_POST_X(50, GetJobFactory().GetJobVersion() << WN_BUILD_DATE); 

    const IRegistry& reg = m_App.GetConfig();
    //unsigned int udp_port =
    //    reg.GetInt(kServerSec, "notify_udp_port", 0/*9111*/,0,IRegistry::eReturn);
    unsigned int max_threads = 1;
    unsigned int init_threads = 1;
    if (!m_SingleThreadForced) {
        string s_max_threads = 
            reg.GetString(kServerSec,"max_threads","auto");
        if (NStr::CompareNocase(s_max_threads, "auto") == 0 )
            max_threads = GetCpuCount();
        else {
            try {
                max_threads = NStr::StringToUInt(s_max_threads);
            } catch (...) {
                max_threads = GetCpuCount();
                ERR_POST_X(51, "Could not convert [" << kServerSec 
                           << "] max_threads parameter to number.\n" 
                           << "Using \'auto\' option (" << max_threads << ").");
            }
        }
        init_threads = 
            reg.GetInt(kServerSec,"init_threads",1,0,IRegistry::eReturn);
    }
    if (init_threads > max_threads) 
        init_threads = max_threads;

    unsigned int ns_timeout = 
        reg.GetInt(kServerSec,"job_wait_timeout",30,0,IRegistry::eReturn);
    unsigned int threads_pool_timeout = 
        reg.GetInt(kServerSec,"thread_pool_timeout",30,0,IRegistry::eReturn);
    string scontrol_port = 
        reg.GetString(kServerSec,"control_port","9300");

    unsigned int start_port, end_port;
    s_ParseControlPorts(scontrol_port, start_port, end_port);

    const CArgs& args = m_App.GetArgs();
    if (args["control_port"]) {
        scontrol_port = args["control_port"].AsString();
        s_ParseControlPorts(scontrol_port, start_port, end_port);
    }

    bool server_log = 
        reg.GetBool(kServerSec,"log",false,0,IRegistry::eReturn);

    unsigned int idle_run_delay = 
        reg.GetInt(kServerSec,"idle_run_delay",30,0,IRegistry::eReturn);

    unsigned int auto_shutdown = 
        reg.GetInt(kServerSec,"auto_shutdown_if_idle",0,0,IRegistry::eReturn);

    unsigned int infinit_loop_time = 0;
    if (reg.HasEntry(kServerSec,"infinite_loop_time") )
        infinit_loop_time = reg.GetInt(kServerSec,"infinite_loop_time",0,0,IRegistry::eReturn);
    else
        infinit_loop_time = reg.GetInt(kServerSec,"infinit_loop_time",0,0,IRegistry::eReturn);
                             
    //bool idle_exclusive =
    //    reg.GetBool(kServerSec, "idle_exclusive", true, 0, 
    //                CNcbiRegistry::eReturn);


    bool reuse_job_object =
        reg.GetBool(kServerSec, "reuse_job_object", false, 0, 
                    CNcbiRegistry::eReturn);

    unsigned int max_total_jobs = 
        reg.GetInt(kServerSec,"max_total_jobs",0,0,IRegistry::eReturn);

    unsigned int max_failed_jobs = 
        reg.GetInt(kServerSec,"max_failed_jobs",0,0,IRegistry::eReturn);

    bool is_daemon =
        reg.GetBool(kServerSec, "daemon", false, 0, CNcbiRegistry::eReturn);

    string masters = 
            reg.GetString(kServerSec, "master_nodes", kEmptyStr);
    string admin_hosts = 
            reg.GetString(kServerSec, "admin_hosts", kEmptyStr);

    unsigned int check_status_period = 
        reg.GetInt(kServerSec,"check_status_period",2,0,IRegistry::eReturn);

    if (reg.HasEntry(kServerSec,"wait_server_timeout")) {
       ERR_POST_X(52, "[" + kServerSec + "] \"wait_server_timeout\" is not used anymore.\n"
	      "Use [" + kNetScheduleAPIDriverName + "] \"communication_timeout\" paramter instead.");
    }

    bool use_embedded_input = false;
    if (reg.HasEntry(kNetScheduleAPIDriverName, "use_embedded_storage"))
        use_embedded_input = reg.
            GetBool(kNetScheduleAPIDriverName, "use_embedded_storage", false, 0, 
                    CNcbiRegistry::eReturn);
    else
        use_embedded_input = reg.
            GetBool(kNetScheduleAPIDriverName, "use_embedded_input", false, 0, 
                    CNcbiRegistry::eReturn);

    CGridDebugContext::eMode debug_mode = CGridDebugContext::eGDC_NoDebug;
    string dbg_mode = reg.GetString("gw_debug", "mode", kEmptyStr);
    if (NStr::CompareNocase(dbg_mode, "gather")==0) {
        debug_mode = CGridDebugContext::eGDC_Gather;
    } else if (NStr::CompareNocase(dbg_mode, "execute")==0) {
        debug_mode = CGridDebugContext::eGDC_Execute;
    }
    if (debug_mode != CGridDebugContext::eGDC_NoDebug) {
        CGridDebugContext& debug_context = 
            CGridDebugContext::Create(debug_mode,GetStorageFactory());
        string run_name = 
            reg.GetString("gw_debug", "run_name", m_App.GetProgramDisplayName());
        debug_context.SetRunName(run_name);
        if (debug_mode == CGridDebugContext::eGDC_Gather) {
            max_total_jobs =
                reg.GetInt("gw_debug","gather_nrequests",1,0,IRegistry::eReturn);
        } else if (debug_mode == CGridDebugContext::eGDC_Execute) {
            string files = 
                reg.GetString("gw_debug", "execute_requests", kEmptyStr);
            max_total_jobs = 0;
            debug_context.SetExecuteList(files);
        }
        is_daemon = false;
    }

#if defined(NCBI_OS_UNIX)
    if (is_daemon) {
        LOG_POST_X(53, "Entering UNIX daemon mode...");
        bool daemon = Daemonize("/dev/null", fDaemon_DontChroot | fDaemon_KeepStdin |
                                fDaemon_KeepStdout);
        if (!daemon) {
            return 0;
        }
    }
#endif

    AttachJobWatcher(CGridGlobals::GetInstance().GetJobsWatcher());
    m_WorkerNode.reset(new CGridWorkerNode(GetJobFactory(), 
                                           GetStorageFactory(), 
                                           GetClientFactory(),
                                           m_JobWatchers.get())
                       );
    m_WorkerNode->SetMaxThreads(max_threads);
    m_WorkerNode->SetInitThreads(init_threads);
    m_WorkerNode->SetNSTimeout(ns_timeout);
    m_WorkerNode->SetUseEmbeddedStorage(use_embedded_input);
    m_WorkerNode->SetThreadsPoolTimeout(threads_pool_timeout);
    m_WorkerNode->SetMasterWorkerNodes(masters);
    m_WorkerNode->SetAdminHosts(admin_hosts);
    m_WorkerNode->ActivateServerLog(server_log);
    m_WorkerNode->SetCheckStatusPeriod(check_status_period);

    CGridGlobals::GetInstance().SetReuseJobObject(reuse_job_object);
    CGridGlobals::GetInstance().GetJobsWatcher().SetMaxJobsAllowed(max_total_jobs);
    CGridGlobals::GetInstance().GetJobsWatcher().SetMaxFailuresAllowed(max_failed_jobs);
    CGridGlobals::GetInstance().GetJobsWatcher().SetInfinitLoopTime(infinit_loop_time);
    CGridGlobals::GetInstance().SetWorker(*m_WorkerNode);

    IWorkerNodeIdleTask* task = NULL;
    if (idle_run_delay > 0)
        task = GetJobFactory().GetIdleTask();
    if (task || auto_shutdown > 0 ) {
        m_IdleThread.Reset(new CWorkerNodeIdleThread(task, *m_WorkerNode, 
                                                     task ? idle_run_delay : auto_shutdown,
                                                     //idle_exclusive,
                                                     auto_shutdown));
        m_IdleThread->Run();
        AttachJobWatcher(*(new CIdleWatcher(*m_IdleThread)), eTakeOwnership);
    }

    {{
    CRef<CGridControlThread> control_thread(new CGridControlThread(start_port, end_port, *m_WorkerNode));

    //CRef<CGridWorkerNodeThread> worker_thread(
    //                            new CGridWorkerNodeThread(*m_WorkerNode));

    control_thread->Prepare();
    m_WorkerNode->SetListeningPort(control_thread->GetControlPort());

    control_thread->Run();
    //worker_thread->Run();
    // give sometime the thread to run
    SleepMilliSec(500);
    if (CGridGlobals::GetInstance().
        GetShutdownLevel() == CNetScheduleAdmin::eNoShutdown) {
        LOG_POST_X(54, "\n=================== NEW RUN : " 
                   << CGridGlobals::GetInstance().GetStartTime().AsString()
                   << " ===================\n"
                   << GetJobFactory().GetJobVersion() << WN_BUILD_DATE << " is started.\n"
                   << "Waiting for control commands on " 
                   << CSocketAPI::gethostname() << ":" << control_thread->GetControlPort() << "\n"
                   << "Queue name: " << m_WorkerNode->GetQueueName() << "\n"
                   << "Maximum job threads: " << max_threads << "\n");

        m_WorkerNode->Run();

        LOG_POST_X(55, CTime(CTime::eCurrent).AsString() << " Stopping Control thread...");
        control_thread->Stop();
        CNcbiOstrstream os;
        CGridGlobals::GetInstance().GetJobsWatcher().Print(os);
        LOG_POST_X(56, string(CNcbiOstrstreamToString(os)));
    }
    control_thread->Join();
    //    worker_thread->Join();
    if (m_IdleThread) {
        if (!m_IdleThread->IsShutdownRequested()) {
            LOG_POST_X(57, CTime(CTime::eCurrent).AsString() << " Stopping Idle thread...");
            m_IdleThread->RequestShutdown();
        }
        m_IdleThread->Join();
    }
    }}

    m_WorkerNode.reset(0);    
    // give sometime the thread to shutdown
    //SleepMilliSec(500);
    return 0;
}

void CGridWorkerApp_Impl::RequestShutdown()
{
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}


void CGridWorkerApp_Impl::AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher, 
                                           EOwnership owner)
{
    if (!m_JobWatchers.get())
        m_JobWatchers.reset(new CWorkerNodeJobWatchers);
    m_JobWatchers->AttachJobWatcher(job_watcher, owner);
};

END_NCBI_SCOPE
