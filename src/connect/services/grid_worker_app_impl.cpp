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


#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
#endif

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

/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNodeThread
/// @internal
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
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
        LOG_POST(CTime(CTime::eCurrent).AsString() << " Worker Node Thread exited.");
    }

private:
    CGridWorkerNode& m_WorkerNode;
};

/////////////////////////////////////////////////////////////////////////////
//
//     CGridControleThread
/// @internal
class CGridControlThread : public CThread
{
public:
    CGridControlThread(CWorkerNodeControlThread& control) 
        : m_Control(control) {}

    ~CGridControlThread() {}
protected:

    virtual void* Main(void)
    {
        m_Control.Run();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
        LOG_POST(CTime(CTime::eCurrent).AsString() << " Control Thread exited.");
    }

private:
    CWorkerNodeControlThread& m_Control;
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
                          bool exclusive_mode,
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
    bool                m_ExclusiveMode;
    unsigned int        m_AutoShutdown;
    CStopWatch          m_AutoShutdownSW;
    mutable CFastMutex  m_Mutext;

    CWorkerNodeIdleThread(const CWorkerNodeIdleThread&);
    CWorkerNodeIdleThread& operator=(const CWorkerNodeIdleThread&);
};

CWorkerNodeIdleThread::CWorkerNodeIdleThread(IWorkerNodeIdleTask* task,
                                             CGridWorkerNode& worker_node,
                                             unsigned run_delay,
                                             bool exclusive_mode,
                                             unsigned int auto_shutdown)
    : m_Task(task), m_WorkerNode(worker_node),
      m_Wait1(0,100000), m_Wait2(0,1000000),
      m_StopFlag(false), m_ShutdownFlag(false),
      m_RunInterval(run_delay), m_ExclusiveMode(exclusive_mode),
      m_AutoShutdown(auto_shutdown), m_AutoShutdownSW(CStopWatch::eStart)
{
}
void* CWorkerNodeIdleThread::Main()
{
    while (!m_ShutdownFlag) {
        if ( x_IsAutoShutdownTime() ) {
            LOG_POST(CTime(CTime::eCurrent).AsString() 
                     << " There are no more jobs to be done. Exiting.");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
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
            if (m_ExclusiveMode)
                m_WorkerNode.PutOnHold(true);
            try {
                do {
                    if ( x_IsAutoShutdownTime() ) {
                        LOG_POST(CTime(CTime::eCurrent).AsString() 
                                 << " There are no more jobs to be done. Exiting.");
                        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
                        m_ShutdownFlag = true;
                        break;
                    }            
                    GetContext().Reset();
                    m_Task->Run(GetContext());
                } while( GetContext().NeedRunAgain() && !m_ShutdownFlag);
            } NCBI_CATCH_ALL("CWorkerNodeIdleThread::Main: Idle Task failed");
            if (m_ExclusiveMode)
                m_WorkerNode.PutOnHold(false);
        }
    }
    return 0;
}

void CWorkerNodeIdleThread::OnExit(void)
{
    LOG_POST(CTime(CTime::eCurrent).AsString() << " Idle Thread stopped.");
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
        RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
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

int CGridWorkerApp_Impl::Run()
{
    LOG_POST( GetJobFactory().GetJobVersion() << WN_BUILD_DATE); 

    const IRegistry& reg = m_App.GetConfig();
    unsigned int udp_port =
        reg.GetInt(kServerSec, "notify_udp_port", 0/*9111*/,0,IRegistry::eReturn);
    unsigned int max_threads = 1;
    unsigned int init_threads = 1;
    if (!m_SingleThreadForced) {
        max_threads = 
            reg.GetInt(kServerSec,"max_threads",4,0,IRegistry::eReturn);
        init_threads = 
            reg.GetInt(kServerSec,"init_threads",2,0,IRegistry::eReturn);
    }
    if (init_threads > max_threads) 
        init_threads = max_threads;

    unsigned int ns_timeout = 
        reg.GetInt(kServerSec,"job_wait_timeout",30,0,IRegistry::eReturn);
    unsigned int threads_pool_timeout = 
        reg.GetInt(kServerSec,"thread_pool_timeout",30,0,IRegistry::eReturn);
    unsigned int control_port = 
        reg.GetInt(kServerSec,"control_port",9300,0,IRegistry::eReturn);

    const CArgs& args = m_App.GetArgs();
    if (args["control_port"]) {
        control_port = args["control_port"].AsInteger();
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
                             
    bool idle_exclusive =
        reg.GetBool(kServerSec, "idle_exclusive", true, 0, 
                    CNcbiRegistry::eReturn);


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
        LOG_POST("Entering UNIX daemon mode...");
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
    if (udp_port == 0)
        udp_port = control_port;
    m_WorkerNode->SetListeningPort(udp_port);
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
                                                     idle_exclusive,
                                                     auto_shutdown));
        m_IdleThread->Run();
        AttachJobWatcher(*(new CIdleWatcher(*m_IdleThread)), eTakeOwnership);
    }

    {{
    CWorkerNodeControlThread control_server(control_port, *m_WorkerNode);
    CRef<CGridControlThread> control_thread(new CGridControlThread(control_server));

    //CRef<CGridWorkerNodeThread> worker_thread(
    //                            new CGridWorkerNodeThread(*m_WorkerNode));

    control_thread->Run();
    //worker_thread->Run();
    // give sometime the thread to run
    SleepMilliSec(500);
    if (CGridGlobals::GetInstance().
        GetShutdownLevel() == CNetScheduleAdmin::eNoShutdown) {
        LOG_POST("\n=================== NEW RUN : " 
                 << CGridGlobals::GetInstance().GetStartTime().AsString()
                 << " ===================\n"
                 << GetJobFactory().GetJobVersion() << WN_BUILD_DATE << " is started.\n"
                 << "Waiting for control commands on TCP port " << control_port << "\n"
                 << "Queue name: " << m_WorkerNode->GetQueueName()
                 << "Maximum job threads: " << max_threads);


        try {
            m_WorkerNode->Start();
                //control_server.Run();
        } catch (exception& ex) {
            ERR_POST(CTime(CTime::eCurrent).AsString()
                     << " Couldn't run a worker node: " << ex.what()  << "\n");
            RequestShutdown();
        } catch (...) {
            ERR_POST(CTime(CTime::eCurrent).AsString()
                     << " Couldn't run a worker node: Unknown error\n");
            RequestShutdown();
        }
        control_server.RequestShutdown();
    }
    control_thread->Join();
    //    worker_thread->Join();
    if (m_IdleThread) {
        m_IdleThread->RequestShutdown();
        m_IdleThread->Join();
    }
    }}
    m_WorkerNode.reset(0);    
    // give sometime the thread to shutdown
    SleepMilliSec(500);
    return 0;
}

void CGridWorkerApp_Impl::RequestShutdown()
{
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
}


void CGridWorkerApp_Impl::AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher, 
                                           EOwnership owner)
{
    if (!m_JobWatchers.get())
        m_JobWatchers.reset(new CWorkerNodeJobWatchers);
    m_JobWatchers->AttachJobWatcher(job_watcher, owner);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.31  2006/12/11 16:17:46  vasilche
 * Fixed use of mutex guard.
 *
 * Revision 6.30  2006/12/08 15:49:56  didenko
 * Fixed a mutex guard usage
 *
 * Revision 6.29  2006/11/30 15:33:33  didenko
 * Moved to a new log system
 *
 * Revision 6.28  2006/10/31 18:41:17  grichenk
 * Redesigned diagnostics setup.
 * Moved the setup function to ncbidiag.cpp.
 *
 * Revision 6.27  2006/09/19 14:34:41  didenko
 * Code clean up
 * Catch and log all exceptions in destructors
 *
 * Revision 6.26  2006/08/28 19:36:56  didenko
 * Changed threads' order starting
 *
 * Revision 6.25  2006/08/08 18:26:20  didenko
 * Code cleanning
 *
 * Revision 6.24  2006/08/03 19:33:10  didenko
 * Added auto_shutdown_if_idle config file paramter
 * Added current date to messages in the log file.
 *
 * Revision 6.23  2006/06/22 13:52:36  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 6.22  2006/06/08 19:39:35  didenko
 * Spelling correction
 *
 * Revision 6.21  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 6.20  2006/05/12 15:13:37  didenko
 * Added infinit loop detection mechanism in job executions
 *
 * Revision 6.19  2006/04/12 19:03:49  didenko
 * Renamed parameter "use_embedded_input" to "use_embedded_storage"
 *
 * Revision 6.18  2006/04/04 19:15:02  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 6.17  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 6.16  2006/03/07 17:20:29  didenko
 * Fixing call to Daemonize function
 *
 * Revision 6.15  2006/02/27 14:50:21  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 6.14  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 6.13  2006/02/15 17:16:02  didenko
 * Added ReqeustShutdown method to worker node idle task context
 *
 * Revision 6.12  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * Revision 6.11  2006/02/07 20:59:34  didenko
 * Fixed Idle task timeout calculation
 *
 * Revision 6.10  2006/02/01 19:04:38  didenko
 * - call to CONNECT_Init function
 *
 * Revision 6.9  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 6.8  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 6.7  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 6.6  2005/12/12 15:13:16  didenko
 * Now CNetScheduleStorageFactory_NetCache class reads all init
 * parameters from the registry
 *
 * Revision 6.5  2005/08/15 19:08:43  didenko
 * Changed NetScheduler Storage parameters
 *
 * Revision 6.4  2005/07/27 14:30:52  didenko
 * Changed the logging system
 *
 * Revision 6.3  2005/07/26 15:25:01  didenko
 * Added logging type parameter
 *
 * Revision 6.2  2005/05/26 15:22:42  didenko
 * Cosmetics
 *
 * Revision 6.1  2005/05/25 18:52:37  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * Revision 1.27  2005/05/25 14:14:33  didenko
 * Cosmetics
 *
 * Revision 1.26  2005/05/23 15:51:54  didenko
 * Moved grid_control_thread.hpp grid_debug_context.hpp to
 * include/connect/service
 *
 * Revision 1.25  2005/05/19 15:15:24  didenko
 * Added admin_hosts parameter to worker nodes configurations
 *
 * Revision 1.24  2005/05/17 20:25:21  didenko
 * Added control_port command line parameter
 * Added control_port number to the name of the log file
 *
 * Revision 1.23  2005/05/16 14:20:55  didenko
 * Added master/slave dependances between worker nodes.
 *
 * Revision 1.22  2005/05/12 14:52:05  didenko
 * Added a worker node build time and start time to the statistic
 *
 * Revision 1.21  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 1.20  2005/05/10 15:42:53  didenko
 * Moved grid worker control thread to its own file
 *
 * Revision 1.19  2005/05/10 14:14:33  didenko
 * Added blob caching
 *
 * Revision 1.18  2005/05/06 14:58:41  didenko
 * Fixed Uninitialized varialble bug
 *
 * Revision 1.17  2005/05/05 15:57:35  didenko
 * Minor fixes
 *
 * Revision 1.16  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.15  2005/04/28 17:50:14  didenko
 * Fixing the type of max_total_jobs var
 *
 * Revision 1.14  2005/04/27 16:17:20  didenko
 * Fixed logging system for worker node
 *
 * Revision 1.13  2005/04/27 15:16:29  didenko
 * Added rotating log
 * Added optional deamonize
 *
 * Revision 1.12  2005/04/27 14:10:36  didenko
 * Added max_total_jobs parameter to a worker node configuration file
 *
 * Revision 1.11  2005/04/21 20:15:52  didenko
 * Added some comments
 *
 * Revision 1.10  2005/04/21 19:10:01  didenko
 * Added IWorkerNodeInitContext
 * Added some convenient macros
 *
 * Revision 1.9  2005/04/19 18:58:52  didenko
 * Added Init method to CGridWorker
 *
 * Revision 1.8  2005/04/07 13:19:17  didenko
 * Fixed a bug that could cause a core dump
 *
 * Revision 1.7  2005/04/05 15:16:20  didenko
 * Fixed a bug that in some cases can lead to a core dump
 *
 * Revision 1.6  2005/03/28 14:39:43  didenko
 * Removed signal handling
 *
 * Revision 1.5  2005/03/25 16:27:02  didenko
 * Moved defaults factories the their own header file
 *
 * Revision 1.4  2005/03/24 15:06:20  didenko
 * Got rid of warnnings about missing virtual destructors.
 *
 * Revision 1.3  2005/03/23 21:26:06  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.2  2005/03/22 20:36:22  didenko
 * Make it compile under CGG
 *
 * Revision 1.1  2005/03/22 20:18:25  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
