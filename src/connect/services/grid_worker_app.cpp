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
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>
#include <connect/threaded_server.hpp>

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNodeThread
/// @internal
class NCBI_XCONNECT_EXPORT CGridWorkerNodeThread : public CThread
{
public:
    CGridWorkerNodeThread(CGridWorkerNode& worker_node);
    ~CGridWorkerNodeThread();

    void RequestShutdown(CNetScheduleClient::EShutdownLevel level) 
    { 
        m_WorkerNode.RequestShutdown(level);
    }
protected:

    virtual void* Main(void);
    virtual void OnExit(void);

private:

    CGridWorkerNode& m_WorkerNode;
};

CGridWorkerNodeThread::CGridWorkerNodeThread(CGridWorkerNode& worker_node)
    : m_WorkerNode(worker_node)
{
}
CGridWorkerNodeThread::~CGridWorkerNodeThread()
{
}

void* CGridWorkerNodeThread::Main(void)
{
    m_WorkerNode.Start();
    return NULL;
}

void CGridWorkerNodeThread::OnExit(void)
{
    CThread::OnExit();
    LOG_POST("Worker Node Thread exited.");
}

/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNodeThread
/// @internal
class CWorkerNodeThreadedServer : public CThreadedServer
{
public:
    CWorkerNodeThreadedServer(unsigned int port, 
                              CGridWorkerNode& worker_node);

    virtual ~CWorkerNodeThreadedServer();

    virtual void Process(SOCK sock);

    virtual bool ShutdownRequested(void) 
    {
        return m_WorkerNode.GetShutdownLevel() 
                       != CNetScheduleClient::eNoShutdown;
    }

private:
    CGridWorkerNode& m_WorkerNode;
    STimeout         m_ThrdSrvAcceptTimeout;

};

CWorkerNodeThreadedServer::CWorkerNodeThreadedServer(
                                    unsigned int port, 
                                    CGridWorkerNode& worker_node)
: CThreadedServer(port), m_WorkerNode(worker_node)
{
    m_InitThreads = 1;
    m_MaxThreads = 3;
    m_ThrdSrvAcceptTimeout.sec = 0;
    m_ThrdSrvAcceptTimeout.usec = 500;
    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
}

CWorkerNodeThreadedServer::~CWorkerNodeThreadedServer()
{
    LOG_POST(Info << "Control server stopped.");
}

#define JS_CHECK_IO_STATUS(x) \
        switch (x)  { \
        case eIO_Success: \
            break; \
        default: \
            return; \
        } 

const string SHUTDOWN_CMD = "SHUTDOWN";
const string VERSION_CMD = "VERSION";
void CWorkerNodeThreadedServer::Process(SOCK sock)
{
    EIO_Status io_st;
    CSocket socket;
    try {
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
        socket.DisableOSSendDelay();
        
        string auth;
        io_st = socket.ReadLine(auth);
        JS_CHECK_IO_STATUS(io_st);
        
        string queue;
        io_st = socket.ReadLine(queue);
        JS_CHECK_IO_STATUS(io_st);

        string request;
        io_st = socket.ReadLine(request);
        JS_CHECK_IO_STATUS(io_st);
        
        if( strncmp( request.c_str(), SHUTDOWN_CMD.c_str(), 
                     SHUTDOWN_CMD.length() ) == 0 ) {
            m_WorkerNode.RequestShutdown(CNetScheduleClient::eNormalShutdown);
            string ans = "OK:";
            socket.Write(ans.c_str(), ans.length() + 1 );
            LOG_POST(Info << "Shutdown request has been received.");
            return;
        }
        if( strncmp( request.c_str(), VERSION_CMD.c_str(), 
                     VERSION_CMD.length() ) == 0 ) {
            string ans = "OK:" + m_WorkerNode.GetJobVersion();
            socket.Write(ans.c_str(), ans.length() + 1 );
            return;
        } 
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "Exception in the control server : " << ex.what() );
        string err = "ERR:" + NStr::PrintableString(ex.what());
        socket.Write(err.c_str(), err.length() + 1 );     
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CNetScheduleStorageFactory_NetCache : public INetScheduleStorageFactory
{
public:
    CNetScheduleStorageFactory_NetCache(const IRegistry& reg);

    virtual INetScheduleStorage* CreateInstance(void);

private:
    typedef CPluginManager<CNetCacheClient>    TPMNetCache;
    TPMNetCache               m_PM_NetCache;
    const IRegistry& m_Registry;
};
CNetScheduleStorageFactory_NetCache::
        CNetScheduleStorageFactory_NetCache(const IRegistry& reg)
: m_Registry(reg)
{
    m_PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);
}

INetScheduleStorage* 
CNetScheduleStorageFactory_NetCache::CreateInstance(void)
{
    CConfig conf(m_Registry);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* netcache_tree = 
            param_tree->FindSubNode(kNetCacheDriverName);

    auto_ptr<CNetCacheClient> nc_client;
    if (netcache_tree) {
        nc_client.reset( 
            m_PM_NetCache.CreateInstance(
                    kNetCacheDriverName,
                    CVersionInfo(TPMNetCache::TInterfaceVersion::eMajor,
                                 TPMNetCache::TInterfaceVersion::eMinor,
                                 TPMNetCache::TInterfaceVersion::ePatchLevel), 
                                 netcache_tree)
                       );
    }

    return new CNetCacheNSStorage(nc_client);
}



/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CNetScheduleClientFactory : public INetScheduleClientFactory
{
public:
    CNetScheduleClientFactory(const IRegistry& reg);

    virtual CNetScheduleClient* CreateInstance(void);

private:
    typedef CPluginManager<CNetScheduleClient> TPMNetSchedule;
    TPMNetSchedule            m_PM_NetSchedule;
    const IRegistry& m_Registry;
};

CNetScheduleClientFactory::CNetScheduleClientFactory(const IRegistry& reg)
: m_Registry(reg)
{
    m_PM_NetSchedule.RegisterWithEntryPoint(NCBI_EntryPoint_xnetschedule);
}
CNetScheduleClient* CNetScheduleClientFactory::CreateInstance(void)
{
    auto_ptr<CNetScheduleClient> ret;

    CConfig conf(m_Registry);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* netschedule_tree = 
            param_tree->FindSubNode(kNetScheduleDriverName);

    if (netschedule_tree) {
        ret.reset( 
            m_PM_NetSchedule.CreateInstance(
                    kNetScheduleDriverName,
                    CVersionInfo(TPMNetSchedule::TInterfaceVersion::eMajor,
                                 TPMNetSchedule::TInterfaceVersion::eMinor,
                                 TPMNetSchedule::TInterfaceVersion::ePatchLevel), 
                                 netschedule_tree)
                                 );
        ret->ActivateRequestRateControl(false);
    }
    return ret.release();
}


/////////////////////////////////////////////////////////////////////////////
//
#if defined(NCBI_OS_UNIX)
/// @internal
extern "C" 
void GridWorker_SignalHandler( int )
{
    CGridWorkerApp* app = 
        dynamic_cast<CGridWorkerApp*>(CNcbiApplication::Instance());
    if (app) {
        app->RequestShutdown();
    }
}
#endif

/////////////////////////////////////////////////////////////////////////////
//
CGridWorkerApp::CGridWorkerApp(IWorkerNodeJobFactory* job_factory, 
                               INetScheduleStorageFactory* storage_factory,
                               INetScheduleClientFactory* client_factory)
: m_JobFactory(job_factory), m_StorageFactory(storage_factory),
  m_ClientFactory(client_factory), m_HandleSignals(true)
{
    if (!m_JobFactory.get())
        NCBI_THROW(CGridWorkerAppException,
                   eJobFactoryIsNotSet, "The JobFactory is not set.");
    if (!m_StorageFactory.get())
        m_StorageFactory.reset(
            new CNetScheduleStorageFactory_NetCache(GetConfig()) 
                              );
    if (!m_ClientFactory.get())
        m_ClientFactory.reset(
            new CNetScheduleClientFactory(GetConfig())
                              );

}
CGridWorkerApp::~CGridWorkerApp()
{
}

int CGridWorkerApp::Run(void)
{
    const IRegistry& reg = GetConfig();

    unsigned int udp_port =
       reg.GetInt("server", "udp_port", 9111,0,IRegistry::eReturn);
    unsigned int max_threads = 
       reg.GetInt("server","max_threads",4,0,IRegistry::eReturn);
    unsigned int init_threads = 
       reg.GetInt("server","init_threads",2,0,IRegistry::eReturn);
    unsigned int ns_timeout = 
       reg.GetInt("server","job_wait_timeout",30,0,IRegistry::eReturn);
    unsigned int threads_pool_timeout = 
       reg.GetInt("server","thread_pool_timeout",30,0,IRegistry::eReturn);
    unsigned int control_port = 
       reg.GetInt("server","control_port",9300,0,IRegistry::eReturn);
    bool server_log = 
        reg.GetBool("server","log",false,0,IRegistry::eReturn);
    if (server_log)
        SetDiagPostLevel(eDiag_Info);

    m_WorkerNode.reset( new CGridWorkerNode(GetJobFactory(), 
                                            GetStorageFactory(), 
                                            GetClientFactory())
                       );
    m_WorkerNode->SetListeningPort(udp_port);
    m_WorkerNode->SetMaxThreads(max_threads);
    m_WorkerNode->SetInitThreads(init_threads);
    m_WorkerNode->SetNSTimeout(ns_timeout);
    m_WorkerNode->SetThreadsPoolTimeout(threads_pool_timeout);


#if defined(NCBI_OS_UNIX)
    if (m_HandleSignals ) {
    // attempt to get server gracefully shutdown on signal
        signal( SIGINT, GridWorker_SignalHandler);
        signal( SIGTERM, GridWorker_SignalHandler);    
    }
#endif

    {{
    CRef<CGridWorkerNodeThread> worker_thread(
                                new CGridWorkerNodeThread(*m_WorkerNode));
    worker_thread->Run();
    LOG_POST(Info 
        << "Grid Worker Node \"" << GetJobFactory().GetJobVersion() 
        << "\" is started.\n"
        << "Waiting for jobs on UDP port " << udp_port << "\n"
        << "Waiting for control commands on TCP port " << control_port << "\n"
        << "Maximum job threads is " << max_threads << "\n");

    CWorkerNodeThreadedServer control_server(control_port, *m_WorkerNode);
    control_server.Run();
    worker_thread->Join();
    }}
    m_WorkerNode.reset(0);    
    return 0;
}

void CGridWorkerApp::RequestShutdown()
{
    if (m_WorkerNode.get())
        m_WorkerNode->RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
 
