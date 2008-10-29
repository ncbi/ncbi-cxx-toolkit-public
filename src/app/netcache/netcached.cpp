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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/thread_nonstop.hpp>

#include <bdb/bdb_blobcache.hpp>

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

#include "message_handler.hpp"

#include "netcached.hpp"
#include "netcache_version.hpp"

#define NETCACHED_HUMAN_VERSION \
    "NCBI NetCache server Version " NETCACHED_VERSION \
    " build " __DATE__ " " __TIME__

#define NETCACHED_FULL_VERSION \
    "NCBI NetCache Server version " NETCACHED_VERSION \
    " Storage version " NETCACHED_STORAGE_VERSION \
    " Protocol version " NETCACHED_PROTOCOL_VERSION \
    " build " __DATE__ " " __TIME__



USING_NCBI_SCOPE;

//const unsigned int kNetCacheBufSize = 64 * 1024;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

/// General purpose NetCache Mutex
//DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

/// Mutex to guard vector of busy IDs 
//DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex_ID);


//////////////////////////////////////////////////////////////////////////
/// CNetCacheConnectionFactory::
class CNetCacheConnectionFactory : public IServer_ConnectionFactory
{
public:
    CNetCacheConnectionFactory(CNetCacheServer* server) :
        m_Server(server)
    {
    }
    IServer_ConnectionHandler* Create(void)
    {
        return new CNetCache_MessageHandler(m_Server);
    }
private:
    CNetCacheServer* m_Server;
};


static CNetCacheServer* s_netcache_server = 0;


/// @internal
CNetCacheServer::CNetCacheServer(unsigned int     port,
                                 bool             use_hostname,
                                 CBDB_Cache*      cache,
                                 unsigned         network_timeout,
                                 bool             is_log,
                                 const IRegistry& reg) 
:   m_Port(port),
    m_MaxId(0),
    m_Cache(cache),
    m_Shutdown(false),
    m_Signal(0),
    m_InactivityTimeout(network_timeout),
    m_EffectiveLogLevel(is_log ? kLogLevelBase : kLogLevelDisable),
    m_ConfigLogLevel(is_log ? kLogLevelBase : kLogLevelDisable),
    m_Reg(reg)
{
    if (use_hostname) {
        char hostname[256];
        if (SOCK_gethostname(hostname, sizeof(hostname)) == 0) {
            m_Host = hostname;
        }
    } else {
        unsigned int hostaddr = SOCK_HostToNetLong(SOCK_gethostbyname(0));
        char ipaddr[32];
        sprintf(ipaddr, "%u.%u.%u.%u", (hostaddr >> 24) & 0xff,
                                       (hostaddr >> 16) & 0xff,
                                       (hostaddr >> 8)  & 0xff,
                                        hostaddr        & 0xff);
        m_Host = ipaddr;
    }

    s_netcache_server = this;
    m_PendingRequests.Set(0);
}

CNetCacheServer::~CNetCacheServer()
{
    try {
        NON_CONST_ITERATE(TLocalCacheMap, it, m_LocalCacheMap) {
            ICache *ic = it->second;
            delete ic; it->second = 0;
        }
        StopSessionManagement();
    } catch (...) {
        ERR_POST("Exception in ~CNetCacheServer()");
        _ASSERT(0);
    }
}


void CNetCacheServer::SetShutdownFlag(int sig) {
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_Signal = sig;
    }
}


void CNetCacheServer::StartSessionManagement(
                                unsigned session_shutdown_timeout)
{
    LOG_POST(Info << "Starting session management thread. timeout=" 
                  << session_shutdown_timeout);
    m_SessionMngThread.Reset(
        new CSessionManagementThread(*this,
                            session_shutdown_timeout, 10, 2));
    m_SessionMngThread->Run();
}

void CNetCacheServer::StopSessionManagement()
{
# ifdef NCBI_THREADS
    if (!m_SessionMngThread.Empty()) {
        LOG_POST(Info << "Stopping session management thread...");
        m_SessionMngThread->RequestStop();
        m_SessionMngThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}


bool CNetCacheServer::IsManagingSessions()
{
    return !m_SessionMngThread.Empty();
}


bool CNetCacheServer::RegisterSession(const string& host, unsigned pid)
{
    if (m_SessionMngThread.Empty()) return false;
    return m_SessionMngThread->RegisterSession(host, pid);
}


void CNetCacheServer::UnRegisterSession(const string& host, unsigned pid)
{
    if (m_SessionMngThread.Empty()) return;
    m_SessionMngThread->UnRegisterSession(host, pid);
}


void CNetCacheServer::SetMonitorSocket(CSocket& socket)
{
    m_Monitor.SetSocket(socket);
}


bool CNetCacheServer::IsMonitoring()
{
    return m_Monitor.IsMonitorActive();
}


void CNetCacheServer::MonitorPost(const string& msg)
{
    m_Monitor.SendString(msg+'\n');
}


void CNetCacheServer::SetBufferSize(unsigned buf_size)
{
}


void CNetCacheServer::MountICache(CConfig&                conf,
                                  CPluginManager<ICache>& pm_cache)
{
    CFastMutexGuard guard(m_LocalCacheMap_Lock);

    const CConfig::TParamTree* param_tree = conf.GetTree();
    x_GetICacheNames(&m_LocalCacheMap);
    NON_CONST_ITERATE(TLocalCacheMap, it, m_LocalCacheMap) {
        const string& cache_name = it->first;
        if (it->second != 0) {
            continue;  // already created
        }
        string section_name("icache_");
        section_name.append(cache_name);

        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(section_name);
        if (!bdb_tree) {
            ERR_POST("Configuration error. Cannot find registry section " 
                     << section_name);
            continue;
        }

        ICache* ic;
        try {
            ic = pm_cache.CreateInstance(
                                kBDBCacheDriverName,
                                TCachePM::GetDefaultDrvVers(),
                                bdb_tree);

            it->second = ic;
            LOG_POST("Local cache mounted: " << cache_name);
            CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            if (bdb_cache) {
                bdb_cache->SetMonitor(&m_Monitor);
            }

        } 
        catch (exception& ex)
        {
            ERR_POST("Error mounting local cache:" << cache_name << 
                     ": " << ex.what()
                    );
            throw;
        }

    } // ITERATE
}

ICache* CNetCacheServer::GetLocalCache(const string& cache_name)
{
    CFastMutexGuard guard(m_LocalCacheMap_Lock);

    TLocalCacheMap::iterator it = m_LocalCacheMap.find(cache_name);
    if (it == m_LocalCacheMap.end()) {
        return 0;
    }
    return it->second;
}


unsigned CNetCacheServer::GetTimeout()
{
    return m_InactivityTimeout;
}


void CNetCacheServer::WriteMsg(CSocket&       sock, 
                               const string&  prefix, 
                               const string&  msg)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");
    size_t n_written;
    /* EIO_Status io_st = */
        sock.Write(err_msg.data(), err_msg.size(), &n_written);
    _TRACE("WriteMsg " << prefix << msg);
}


const IRegistry& CNetCacheServer::GetRegistry()
{
    return m_Reg;
}


CBDB_Cache* CNetCacheServer::GetCache()
{
    return m_Cache;
}


const string& CNetCacheServer::GetHost()
{
    return m_Host;
}


unsigned CNetCacheServer::GetPort()
{
    return m_Port;
}


unsigned CNetCacheServer::GetInactivityTimeout(void)
{
    return m_InactivityTimeout;
}


CFastLocalTime& CNetCacheServer::GetTimer()
{
    return m_LocalTimer;
}


void CNetCacheServer::WriteBuf(CSocket& sock,
                                char*    buf,
                                size_t   bytes)
{
    do {
        size_t n_written;
        EIO_Status io_st;
        io_st = sock.Write(buf, bytes, &n_written);
        switch (io_st) {
        case eIO_Success:
            break;
        case eIO_Timeout:
            {
            string msg = "Communication timeout error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eTimeout, msg);
            }
            break;
        case eIO_Closed:
            {
            string msg = 
                "Communication error: peer closed connection (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_Interrupt:
            {
            string msg = 
                "Communication error: interrupt signal (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_InvalidArg:
            {
            string msg = 
                "Communication error: invalid argument (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        default:
            {
            string msg = "Communication error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}


void CNetCacheServer::SetLogLevel(int level)
{
    if (level == kLogLevelBase) {
        m_EffectiveLogLevel = m_ConfigLogLevel;
    } else {
        m_EffectiveLogLevel = level;
    }
}


int CNetCacheServer::GetLogLevel() const
{
    return m_EffectiveLogLevel;
}


void CNetCacheServer::RegisterProtocolErr(
                               SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    bdb_cache->RegisterProtocolError(op, auth);
}


void CNetCacheServer::RegisterInternalErr(
                               SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    bdb_cache->RegisterInternalError(op, auth);
}


void CNetCacheServer::RegisterNoBlobErr(
                               SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    bdb_cache->RegisterNoBlobError(op, auth);
}


void CNetCacheServer::RegisterException(
                               SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&             auth,
                               const CNetServiceException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    switch (ex.GetErrCode()) {
    case CNetServiceException::eTimeout:
    case CNetServiceException::eCommunicationError:
        bdb_cache->RegisterCommError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}


void CNetCacheServer::RegisterException(
                               SBDB_CacheUnitStatistics::EErrGetPut op,
                               const string&             auth,
                               const CNetCacheException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }

    switch (ex.GetErrCode()) {
    case CNetCacheException::eAuthenticationError:
    case CNetCacheException::eKeyFormatError:
        bdb_cache->RegisterProtocolError(op, auth);
        break;
    case CNetCacheException::eServerError:
        bdb_cache->RegisterInternalError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}


/// Read the registry for icache_XXXX entries
void CNetCacheServer::x_GetICacheNames(TLocalCacheMap* cache_map)
{
    string cache_name;
    list<string> sections;
    m_Reg.EnumerateSections(&sections);

    string tmp;
    ITERATE(list<string>, it, sections) {
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, cache_name);
        if (NStr::CompareNocase(tmp, "icache") != 0) {
            continue;
        }

        NStr::ToLower(cache_name);
        (*cache_map)[cache_name] = 0;

    } // ITERATE
}


///////////////////////////////////////////////////////////////////////

/// @internal
extern "C" 
void Threaded_Server_SignalHandler(int sig)
{
    if (s_netcache_server && 
        (!s_netcache_server->ShutdownRequested()) ) {
        s_netcache_server->SetShutdownFlag(sig);
    }
}


///////////////////////////////////////////////////////////////////////


/// NetCache daemon application
///
/// @internal
///
class CNetCacheDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
protected:
    void StopSessionMngThread();

private:
    CRef<CCacheCleanerThread>  m_PurgeThread;

    // Need to keep instance because of STimeout requirements
    STimeout m_ServerAcceptTimeout;
};


void CNetCacheDApp::Init(void)
{
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");
    arg_desc->AddFlag("version-full", "Version");

    SetupArgDescriptions(arg_desc.release());
}


int CNetCacheDApp::Run(void)
{
    CBDB_Cache* bdb_cache = 0;
    CArgs args = GetArgs();

    if (args["version-full"]) {
        printf(NETCACHED_FULL_VERSION "\n");
        return 0;
    }

    LOG_POST(NETCACHED_FULL_VERSION);

    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);
        
#if defined(NCBI_OS_UNIX)
        bool is_daemon =
            reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);
        if (is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = CProcess::Daemonize(0, CProcess::fDontChroot);
            if (!daemon) {
                return 0;
            }
        }
        
        // attempt to get server gracefully shutdown on signal
        signal(SIGINT, Threaded_Server_SignalHandler);
        signal(SIGTERM, Threaded_Server_SignalHandler);
#endif
        int port = 
            reg.GetInt("server", "port", 9000, 0, CNcbiRegistry::eReturn);
        bool is_port_free = true;

        // try server port to check if it is busy
        {{
        CListeningSocket lsock(port);
        if (lsock.GetStatus() != eIO_Success) {
            is_port_free = false;
        }
        }}

        if (!is_port_free) {
            ERR_POST("Startup problem: listening port is busy. Port="
                     << NStr::IntToString(port));
            return 1;
        }

        // Mount BDB Cache

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(kBDBCacheDriverName);


        auto_ptr<ICache> cache;

        CPluginManager<ICache> pm_cache;
        pm_cache.RegisterWithEntryPoint(NCBI_EntryPoint_xcache_bdb);

        if (bdb_tree) {

            CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
            const string& db_path = 
                bdb_conf.GetString("netcached", "path", 
                                    CConfig::eErr_Throw, kEmptyStr);
            bool reinit =
            reg.GetBool("server", "reinit", false, 0, CNcbiRegistry::eReturn);

            if (args["reinit"] || reinit) {  // Drop the database directory
                LOG_POST("Removing BDB database directory " << db_path);
                CDir dir(db_path);
                dir.Remove();
            }
            

            LOG_POST("Initializing BDB cache");
            ICache* ic;

            try {
                ic = 
                    pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CNetCacheServer::TCachePM::GetDefaultDrvVers(),
                        bdb_tree);
            } 
            catch (CBDB_Exception& ex)
            {
                bool drop_db = reg.GetBool("server", "drop_db", 
                                           true, 0, CNcbiRegistry::eReturn);
                if (drop_db) {
                    ERR_POST("Error initializing BDB ICache interface: "
                             << ex.what());
                    LOG_POST("Database directory will be dropped.");

                    CDir dir(db_path);
                    dir.Remove();

                    ic = 
                      pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CNetCacheServer::TCachePM::GetDefaultDrvVers(),
                        bdb_tree);

                } else {
                    throw;
                }
            }

            bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            cache.reset(ic);


        } else {
            ERR_POST("Configuration error. Cannot init storage. Driver name:"
                     << kBDBCacheDriverName);
            return 1;
        }

        // cache storage media has been created

        if (!cache->IsOpen()) {
            ERR_POST("Configuration error. Cache not open.");
            return 1;
        }

        // start server

        unsigned network_timeout =
            reg.GetInt("server", "network_timeout", 10, 0, CNcbiRegistry::eReturn);
        if (network_timeout == 0) {
            ERR_POST(Warning << 
                "INI file sets 0 sec. network timeout. Assume 10 seconds.");
            network_timeout =  10;
        } else {
            LOG_POST("Network IO timeout " << network_timeout);
        }
        bool use_hostname =
            reg.GetBool("server", "use_hostname", false, 0, CNcbiRegistry::eReturn);
        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);
        unsigned buf_size =
            reg.GetInt("server", "buf_size", 64 * 1024, 0, CNcbiRegistry::eReturn);

        SServer_Parameters params;
        params.max_connections =
            reg.GetInt("server", "max_connections", 100, 0, CNcbiRegistry::eReturn);
        params.max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);
        params.init_threads =
            reg.GetInt("server", "init_threads", 10, 0, CNcbiRegistry::eReturn);
        if (params.init_threads > params.max_threads)
            params.init_threads = params.max_threads;

        // Accept timeout
        m_ServerAcceptTimeout.sec = 1;
        m_ServerAcceptTimeout.usec = 0;
        params.accept_timeout = &m_ServerAcceptTimeout;


        auto_ptr<CNetCacheServer> server(
            new CNetCacheServer(port,
                                use_hostname,
                                bdb_cache,
                                network_timeout,
                                is_log,
                                reg));
        server->SetParameters(params);

        server->AddListener(
            new CNetCacheConnectionFactory(&*server),
            port);

        server->SetBufferSize(buf_size);

        if (bdb_cache) {
            bdb_cache->SetMonitor(&server->GetMonitor());
        }

        // create ICache instances

        server->MountICache(conf, pm_cache);

        // Start session management

        {{
            bool session_mng = 
                reg.GetBool("server", "session_mng", 
                            false, 0, IRegistry::eReturn);
            if (session_mng) {
                unsigned session_shutdown_timeout =
                    reg.GetInt("server", "session_shutdown_timeout", 
                               60, 0, IRegistry::eReturn);

                server->StartSessionManagement(session_shutdown_timeout);
            }
        }}

        //NcbiCerr << "Running server on port " << port << NcbiEndl;
        LOG_POST("Running server on port " << port);
        server->Run();

        if (server->GetSignalCode()) {
            LOG_POST("Server got " << server->GetSignalCode() <<
                     " signal.");
        }
        LOG_POST(Info << "Server stopped. Closing storage.");
        // This place is suspicious. We already keep pointer to the cache in
        // 'cache' auto_ptr. So we call Close on this cache at least twice.
        // Close() is protected by double-call check, but insufficiently.
        bdb_cache->Close();
    }
    catch (CBDB_ErrnoException& ex)
    {
        ERR_POST("Error: DBD errno exception:" << ex);
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        ERR_POST("Error: DBD library exception:" << ex);
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    GetDiagContext().SetOldPostFormat(false);
    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_ToStdlog);
}
