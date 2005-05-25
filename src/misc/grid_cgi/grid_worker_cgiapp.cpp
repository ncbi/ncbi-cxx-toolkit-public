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
 * Author:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/services/grid_default_factories.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/grid_default_factories.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/grid_control_thread.hpp>

#include <misc/grid_cgi/grid_worker_cgiapp.hpp>


#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>

/// @internal
extern "C" 
void CgiGridWorker_SignalHandler( int )
{
    try {
        ncbi::CGridWorkerCgiApp* app = 
            dynamic_cast<ncbi::CGridWorkerCgiApp*>(ncbi::CNcbiApplication::Instance());
        if (app) {
            app->RequestShutdown();
        }
    }
    catch (...) {}   // Make sure we don't throw an exception through the "C" layer
}
#endif


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//

class CCgiWorkerNodeJob : public IWorkerNodeJob
{
public:
    CCgiWorkerNodeJob(CGridWorkerCgiApp& app) : m_App(app) {}
    virtual ~CCgiWorkerNodeJob() {} 

    int Do(CWorkerNodeJobContext& context)
    {        
        CNcbiIstream& is = context.GetIStream();
        CNcbiOstream& os = context.GetOStream();

        int ret = m_App.RunJob(is, os);

        context.CommitJob();
        return ret;;
    }

private:
    CGridWorkerCgiApp& m_App;
};

class CCgiWorkerNodeJobFactory : public IWorkerNodeJobFactory
{
public:
    CCgiWorkerNodeJobFactory(CGridWorkerCgiApp& app): m_App(app) {}
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new  CCgiWorkerNodeJob(m_App);
    }
    virtual string GetJobVersion() const 
    { 
        return m_App.GetJobVersion(); 
    } 

private:
    CGridWorkerCgiApp& m_App;
};

/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNodeThread
/// @internal
class CGridWorkerNodeThread : public CThread
{
public:
    CGridWorkerNodeThread(CGridWorkerNode& worker_node,
                          CWorkerNodeControlThread& control_thread)
        : m_WorkerNode(worker_node), m_ControlThread(control_thread) {}

    ~CGridWorkerNodeThread() {}

    void RequestShutdown(CNetScheduleClient::EShutdownLevel level) 
    { 
        m_WorkerNode.RequestShutdown(level);
    }

protected:

    virtual void* Main(void)
    {
        m_WorkerNode.Start();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        m_ControlThread.RequestShutdown();
        LOG_POST("Worker Node Thread exited.");
    }

private:

    CGridWorkerNode& m_WorkerNode;
    CWorkerNodeControlThread& m_ControlThread;
};

/////////////////////////////////////////////////////////////////////////////
//
CGridWorkerCgiApp::CGridWorkerCgiApp( 
                   INetScheduleStorageFactory* storage_factory,
                   INetScheduleClientFactory* client_factory)
: m_StorageFactory(storage_factory), m_ClientFactory(client_factory)
{
    m_JobFactory.reset(new CCgiWorkerNodeJobFactory(*this));

#if defined(NCBI_OS_UNIX)
    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  CgiGridWorker_SignalHandler);
    signal(SIGTERM, CgiGridWorker_SignalHandler);    
#endif
}

void CGridWorkerCgiApp::Init(void)
{
    CCgiApplication::Init();

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Worker Node");
    SetupArgDescriptions(arg_desc.release());
       
    IRWRegistry& reg = GetConfig();
    reg.Set(kNetScheduleDriverName, "discover_low_priority_servers", "true");

    bool cache_input =
        reg.GetBool("netcache_client", "cache_input", false, 
                    0, CNcbiRegistry::eReturn);
    bool cache_output =
        reg.GetBool("netcache_client", "cache_output", false, 
                    0, CNcbiRegistry::eReturn);

    if (!m_StorageFactory.get()) 
        m_StorageFactory.reset(
            new CNetScheduleStorageFactory_NetCache(reg, 
                                                    cache_input,
                                                    cache_output)
                              );
    if (!m_ClientFactory.get()) 
        m_ClientFactory.reset(
            new CNetScheduleClientFactory(reg)
                              );
}

void CGridWorkerCgiApp::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddOptionalKey("control_port", 
                             "control_port",
                             "A TCP port number",
                             CArgDescriptions::eInteger);

    CCgiApplication::SetupArgDescriptions(arg_desc);
}

int CGridWorkerCgiApp::Run(void)
{
    LOG_POST( GetJobFactory().GetJobVersion() << WN_BUILD_DATE); 
    const IRegistry& reg = GetConfig();

    unsigned int udp_port =
        reg.GetInt("server", "notify_udp_port", 0/*9111*/,0,IRegistry::eReturn);
    unsigned int ns_timeout = 
        reg.GetInt("server","job_wait_timeout",30,0,IRegistry::eReturn);
    unsigned int threads_pool_timeout = 
        reg.GetInt("server","thread_pool_timeout",30,0,IRegistry::eReturn);
    unsigned int control_port = 
        reg.GetInt("server","control_port",9300,0,IRegistry::eReturn);
    const CArgs& args = GetArgs();
    if (args["control_port"]) {
        control_port = args["control_port"].AsInteger();
    }

    bool server_log = 
        reg.GetBool("server","log",false,0,IRegistry::eReturn);
    unsigned int log_size = 
        reg.GetInt("server","log_file_size",1024*1024,0,IRegistry::eReturn);
    string log_file_name = GetProgramDisplayName() +"_err.log." 
        + NStr::UIntToString(control_port);

    unsigned int max_total_jobs = 
        reg.GetInt("server","max_total_jobs",0,0,IRegistry::eReturn);

    bool is_daemon =
        reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);

    string masters = 
            reg.GetString("server", "master_nodes", "");
    string admin_hosts = 
            reg.GetString("server", "admin_hosts", "");

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
            reg.GetString("gw_debug", "run_name", GetProgramDisplayName());
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
        log_file_name = debug_context.GetLogFileName();
        is_daemon = false;
    }

#if defined(NCBI_OS_UNIX)
    if (is_daemon) {
        LOG_POST("Entering UNIX daemon mode...");
        bool daemon = Daemonize(0, fDaemon_DontChroot);
        if (!daemon) {
            return 0;
        }
    }
#endif

    m_ErrLog.reset(new CRotatingLogStream(log_file_name, log_size));
    // All errors redirected to rotated log
    // from this moment on the server is silent...
    SetDiagStream(m_ErrLog.get());


    m_WorkerNode.reset( new CGridWorkerNode(GetJobFactory(), 
                                            GetStorageFactory(), 
                                            GetClientFactory())
                       );
    if (udp_port == 0)
        udp_port = control_port;
    m_WorkerNode->SetListeningPort(udp_port);
    m_WorkerNode->SetMaxThreads(1);
    m_WorkerNode->SetInitThreads(1);
    m_WorkerNode->SetNSTimeout(ns_timeout);
    m_WorkerNode->SetThreadsPoolTimeout(threads_pool_timeout);
    m_WorkerNode->SetMaxTotalJobs(max_total_jobs);
    m_WorkerNode->SetMasterWorkerNodes(masters);
    m_WorkerNode->SetAdminHosts(admin_hosts);
    m_WorkerNode->ActivateServerLog(server_log);

    {{
    CWorkerNodeControlThread control_server(control_port, *m_WorkerNode) ;
    CRef<CGridWorkerNodeThread> worker_thread(
                                new CGridWorkerNodeThread(*m_WorkerNode,
                                                          control_server));
    worker_thread->Run();
    // give sometime the thread to run
    SleepMilliSec(500);
    if (m_WorkerNode->GetShutdownLevel() == CNetScheduleClient::eNoShutdown) {
        LOG_POST("\n=================== NEW RUN : " 
                 << m_WorkerNode->GetStartTime().AsString()
                 << " ===================\n"
                 << GetJobFactory().GetJobVersion() << WN_BUILD_DATE << " is started.\n"
                 << "Waiting for control commands on TCP port " << control_port << "\n"
                 << "Queue name: " << m_WorkerNode->GetQueueName() );

        try {
            control_server.Run();
        }
        catch (exception& ex) {
            ERR_POST( "Couldn't run a Threaded server: " << ex.what()  << "\n");
            RequestShutdown();
        }
    }
    worker_thread->Join();
    }}
    m_WorkerNode.reset(0);    
    // give sometime the thread to shutdown
    SleepMilliSec(500);
    return 0;
}

CCgiContext* CGridWorkerCgiApp::CreateCgiContext(CNcbiIstream& is, CNcbiOstream& os)
{
    return  new CCgiContext(*this, &is, &os);   
}

void CGridWorkerCgiApp::RequestShutdown()
{
    if (m_WorkerNode.get())
        m_WorkerNode->RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
}

string CGridWorkerCgiApp::GetJobVersion() const
{
    return GetProgramDisplayName() + " 1.0.0"; 
}

int CGridWorkerCgiApp::RunJob(CNcbiIstream& is, CNcbiOstream& os)
{
    auto_ptr<CCgiContext> cgi_context( CreateCgiContext(is ,os) );
    
    CDiagRestorer diag_restorer;

    int ret;
    try {
        ConfigureDiagnostics(*cgi_context);
        ret = ProcessRequest(*cgi_context);
        OnEvent(ret == 0 ? eSuccess : eError, ret);
        OnEvent(eExit, ret);
    } catch (exception& ex) {
        ret = OnException(ex, os);
        OnEvent(eException, ret);
    }
    OnEvent(eEndRequest, 120);
    OnEvent(eExit, ret);
    return ret;
}


/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/25 14:13:40  didenko
 * Added new Application class from easy transfer existing cgis to worker nodes
 *
 * ===========================================================================
 */
