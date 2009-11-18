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
 * Author: Pavel Ivanov
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

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

#include "message_handler.hpp"
#include "netcached.hpp"
#include "netcache_version.hpp"
#include "nc_memory.hpp"
#include "error_codes.hpp"

#define NETCACHED_HUMAN_VERSION \
    "NCBI NetCache server Version " NETCACHED_VERSION \
    " build " __DATE__ " " __TIME__

#define NETCACHED_FULL_VERSION \
    "NCBI NetCache Server version " NETCACHED_VERSION \
    " Storage version " NETCACHED_STORAGE_VERSION \
    " Protocol version " NETCACHED_PROTOCOL_VERSION \
    " build " __DATE__ " " __TIME__



BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Main


static const char* kNCReg_ServerSection      = "server";
static const char* kNCReg_DefCacheSection    = "bdb";
static const char* kNCReg_CacheSectionPrefix = "icache";
static const char* kNCReg_Daemon             = "daemon";
static const char* kNCReg_Port               = "port";
static const char* kNCReg_NetworkTimeout     = "network_timeout";
static const char* kNCReg_CommandTimeout     = "request_timeout";
static const char* kNCReg_UseHostname        = "use_hostname";
static const char* kNCReg_MaxConnections     = "max_connections";
static const char* kNCReg_MaxThreads         = "max_threads";
static const char* kNCReg_InitThreads        = "init_threads";
static const char* kNCReg_DropBadDB          = "drop_db";
static const char* kNCReg_ManageSessions     = "session_mng";
static const char* kNCReg_SessMngTimeout     = "session_shutdown_timeout";
static const char* kNCReg_MemLimit           = "memory_limit";
static const char* kNCReg_MemAlert           = "memory_alert";


CNCServerStat_Getter  CNCServerStat::sm_Getter;
CNCServerStat         CNCServerStat::sm_Instances[kNCMaxThreadsCnt];


static CNetCacheServer* s_NetcacheServer     = NULL;


extern "C"
void NCSignalHandler(int sig)
{
    if (s_NetcacheServer) {
        s_NetcacheServer->RequestShutdown(sig);
    }
}



CNCServerStat::CNCServerStat(void)
    : m_OpenedConns(0),
      m_OverflowConns(0),
      m_TimedOutCmds(0)
{
    m_ConnSpan.Initialize();
    m_CmdSpan .Initialize();
}

inline CNCServerStat::TCmdsSpansMap::iterator
CNCServerStat::x_GetSpanFigure(TCmdsSpansMap& cmd_span_map, const char* cmd)
{
    TCmdsSpansMap::iterator it_span = cmd_span_map.lower_bound(cmd);
    if (it_span == cmd_span_map.end()
        ||  NStr::strcmp(cmd, it_span->first) != 0)
    {
        TSpanValue value;
        value.Initialize();
        it_span = cmd_span_map.insert(TCmdsSpansMap::value_type(cmd, value)).first;
    }
    return it_span;
}

void
CNCServerStat::AddFinishedCmd(const char* cmd,
                              double      cmd_span,
                              int         cmd_status)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();

    stat->m_CmdSpan.AddValue(cmd_span);
    TCmdsSpansMap& cmd_span_map = stat->m_CmdSpanByCmd[cmd_status];
    TCmdsSpansMap::iterator it_span = x_GetSpanFigure(cmd_span_map, cmd);
    it_span->second.AddValue(cmd_span);

    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::x_CollectTo(CNCServerStat* other)
{
    CSpinGuard guard(m_ObjLock);

    other->m_OpenedConns += m_OpenedConns;
    other->m_OverflowConns += m_OverflowConns;
    other->m_ConnSpan.AddValues(m_ConnSpan);
    other->m_CmdSpan.AddValues(m_CmdSpan);
    ITERATE(TStatCmdsSpansMap, it_st, m_CmdSpanByCmd) {
        const TCmdsSpansMap& cmd_span_map = it_st->second;
        TCmdsSpansMap& other_cmd_span     = other->m_CmdSpanByCmd[it_st->first];
        ITERATE(TCmdsSpansMap, it_span, cmd_span_map) {
            TCmdsSpansMap::iterator other_span
                            = x_GetSpanFigure(other_cmd_span, it_span->first);
            other_span->second.AddValues(it_span->second);
        }
    }
    other->m_TimedOutCmds += m_TimedOutCmds;
}

void
CNCServerStat::Print(CPrintTextProxy& proxy)
{
    CNCServerStat stat;
    for (unsigned int i = 0; i < kNCMaxThreadsCnt; ++i) {
        sm_Instances[i].x_CollectTo(&stat);
    }

    proxy << "Connections - " << stat.m_OpenedConns           << " (open), "
                              << stat.m_ConnSpan.GetCount()   << " (closed), "
                              << stat.m_OverflowConns         << " (overflow), "
                              << stat.m_ConnSpan.GetAverage() << " (avg alive), "
                              << stat.m_ConnSpan.GetMaximum() << " (max alive)" << endl
          << "Commands    - " << stat.m_CmdSpan.GetCount()    << " (cnt), "
                              << stat.m_CmdSpan.GetAverage()  << " (avg time), "
                              << stat.m_CmdSpan.GetMaximum()  << " (max time), "
                              << stat.m_TimedOutCmds          << " (t/o)" << endl
          << "By status and type:" << endl;
    ITERATE(TStatCmdsSpansMap, it_span_st, stat.m_CmdSpanByCmd) {
        proxy << it_span_st->first << ":" << endl;
        ITERATE(TCmdsSpansMap, it_span, it_span_st->second) {
            proxy << it_span->first << " - "
                              << it_span->second.GetCount()   << " (cnt), "
                              << it_span->second.GetAverage() << " (avg time), "
                              << it_span->second.GetMaximum() << " (max time)" << endl;
        }
    }
}



inline int
CNetCacheServer::x_RegReadInt(const IRegistry& reg,
                              const char*      value_name,
                              int              def_value)
{
    return reg.GetInt(kNCReg_ServerSection, value_name, def_value,
                      0, IRegistry::eErrPost);
}

inline bool
CNetCacheServer::x_RegReadBool(const IRegistry& reg,
                               const char*      value_name,
                               bool             def_value)
{
    return reg.GetBool(kNCReg_ServerSection, value_name, def_value,
                       0, IRegistry::eErrPost);
}

inline string
CNetCacheServer::x_RegReadString(const IRegistry& reg,
                                 const char*      value_name,
                                 const string&    def_value)
{
    return reg.GetString(kNCReg_ServerSection, value_name, def_value);
}

inline void
CNetCacheServer::x_CreateStorages(const IRegistry& reg, bool do_reinit)
{
    CNCBlobStorage::EReinitMode reinit = CNCBlobStorage::eNoReinit;
    if (do_reinit) {
        reinit = CNCBlobStorage::eForceReinit;
    }
    else if (x_RegReadBool(reg, kNCReg_DropBadDB, false)) {
        reinit = CNCBlobStorage::eReinitBad;
    }

    CNCBlobStorage* storage = new CNCBlobStorage(reg, kNCReg_DefCacheSection,
                                                 reinit);
    storage->SetMonitor(&m_Monitor);
    m_StorageMap[""] = storage;

    list<string> sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        const string& section_name = *it;
        string tmp, cache_name;

        NStr::SplitInTwo(section_name, "_", tmp, cache_name);
        if (NStr::CompareNocase(tmp, kNCReg_CacheSectionPrefix) != 0) {
            continue;
        }
        NStr::ToLower(cache_name);

        storage = new CNCBlobStorage(reg, section_name, reinit);
        storage->SetMonitor(&m_Monitor);
        m_StorageMap[cache_name] = storage;
    }
}

inline void
CNetCacheServer::x_StartSessionManagement(unsigned int shutdown_timeout)
{
    LOG_POST_X(5, Info << "Starting session management thread. timeout="
                       << shutdown_timeout);
    m_SessionMngThread.Reset(
                new CSessionManagementThread(*this, shutdown_timeout, 10, 2));
    m_SessionMngThread->Run();
}

inline void
CNetCacheServer::x_StopSessionManagement(void)
{
    if (!m_SessionMngThread.Empty()) {
        LOG_POST_X(6, Info << "Stopping session management thread...");
        m_SessionMngThread->RequestStop();
        m_SessionMngThread->Join();
        LOG_POST_X(7, Info << "Stopped.");
    }
}

CNetCacheServer::CNetCacheServer(bool do_reinit)
    : m_Shutdown(false),
      m_Signal(0)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

#if defined(NCBI_OS_UNIX)
    bool is_daemon = x_RegReadBool(reg, kNCReg_Daemon, false);
    if (is_daemon) {
        LOG_POST_X(1, "Entering UNIX daemon mode...");
        // Here's workaround for SQLite3 bug: if stdin is closed in forked
        // process then 0 file descriptor is returned to SQLite after open().
        // But there's assert there which prevents fd to be equal to 0. So
        // we keep descriptors 0, 1 and 2 in child process open. Latter two -
        // just in case somebody will try to write to them.
        bool is_good = CProcess::Daemonize(kEmptyCStr,
                               CProcess::fDontChroot | CProcess::fKeepStdin
                                                     | CProcess::fKeepStdout);
        if (!is_good) {
            NCBI_THROW(CCoreException, eCore, "Error during daemonization");
        }
    }

    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  NCSignalHandler);
    signal(SIGTERM, NCSignalHandler);
#endif

    m_Port              = x_RegReadInt(reg, kNCReg_Port,           9000);
    m_CmdTimeout        = x_RegReadInt(reg, kNCReg_CommandTimeout, 600);
    m_InactivityTimeout = x_RegReadInt(reg, kNCReg_NetworkTimeout, 10);
    if (m_InactivityTimeout <= 0) {
        ERR_POST_X(2, Warning << "INI file sets network timeout to 0 or less."
                                 " Assuming 10 seconds.");
        m_InactivityTimeout =  10;
    } else {
        LOG_POST_X(3, "Network IO timeout " << m_InactivityTimeout);
    }

    bool use_hostname = x_RegReadBool(reg, kNCReg_UseHostname, false);
    if (use_hostname) {
        char hostname[256];
        if (SOCK_gethostname(hostname, sizeof(hostname)) == 0) {
            m_Host = hostname;
        }
    } else {
        unsigned int host = SOCK_gethostbyname(0);
        char ipaddr[32];
        SOCK_ntoa(host, ipaddr, sizeof(ipaddr)-1);
        m_Host = ipaddr;
    }

    SServer_Parameters params;
    params.max_connections = x_RegReadInt(reg, kNCReg_MaxConnections, 100);
    params.max_threads     = x_RegReadInt(reg, kNCReg_MaxThreads,     20);
    // A bit of hard coding but it's really not necessary to create more than
    // 25 threads in server's thread pool.
    if (params.max_threads >= 20) {
        ERR_POST(Warning << "NetCache configuration is not optimal. "
                            "Maximum optimal number of threads is 20, "
                            "maximum number given - " << params.max_threads
                         << ".");
    }
    params.init_threads = x_RegReadInt(reg, kNCReg_InitThreads, 1);
    if (params.init_threads > params.max_threads) {
        params.init_threads = params.max_threads;
    }
    // Accept timeout
    m_ServerAcceptTimeout.sec = 1;
    m_ServerAcceptTimeout.usec = 0;
    params.accept_timeout = &m_ServerAcceptTimeout;

    SetParameters(params);
    AddListener(new CNCMsgHndlFactory_Proxy(this), m_Port);

    CNCMemManager::InitializeApp();
    try {
        size_t mem_limit = size_t(NStr::StringToUInt8_DataSize(
                                  x_RegReadString(reg, kNCReg_MemLimit, "1Gb")));
        size_t mem_alert = size_t(NStr::StringToUInt8_DataSize(
                                  x_RegReadString(reg, kNCReg_MemAlert, "4Gb")));
        CNCMemManager::SetLimits(mem_limit, mem_alert);
    }
    catch (CStringException& ex) {
        ERR_POST_X(14, "Error in " << kNCReg_MemLimit
                         << " or " << kNCReg_MemAlert << " parameter: " << ex);
    }

    CSQLITE_Global::Initialize();
    CSQLITE_Global::EnableSharedCache();
    CNCFileSystem ::Initialize();
    x_CreateStorages(reg, do_reinit);
    CNCFileSystem::SetDiskInitialized();

    // Start session management
    bool session_mng = x_RegReadBool(reg, kNCReg_ManageSessions, false);
    if (session_mng) {
        x_StartSessionManagement(x_RegReadInt(reg, kNCReg_SessMngTimeout, 60));
    }

    m_BlobIdCounter.Set(0);
    m_StartTime = GetFastTime();

    _ASSERT(s_NetcacheServer == NULL);
    s_NetcacheServer = this;
}

CNetCacheServer::~CNetCacheServer()
{
    {{
        CPrintTextProxy proxy(CPrintTextProxy::ePrintLog);
        LOG_POST_X(13, "NetCache server is stopping. Usage statistics:");
        x_PrintServerStats(proxy);
    }}
    x_StopSessionManagement();
}

void
CNetCacheServer::x_PrintServerStats(CPrintTextProxy& proxy)
{
    SServer_Parameters params;
    GetParameters(&params);

    proxy << "Time - " << CTime(CTime::eCurrent)
                       << ", started at " << m_StartTime << endl
          << "Env  - " << CThread::GetThreadsCount() << " (cur thr), "
                       << params.init_threads << " (init thr), "
                       << params.max_threads << " (max thr), "
                       << params.max_connections << " (max conns), "
                       << m_CmdTimeout << " (cmd t/o)" << endl
          << endl;
    CNCMemManager::PrintStats(proxy);
    proxy << endl;
    CNCServerStat::Print(proxy);

    ITERATE(TStorageMap, it, m_StorageMap) {
        proxy << endl;
        it->second->PrintStat(proxy);
    }
}



void
CNetCacheDApp::Init(void)
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

int
CNetCacheDApp::Run(void)
{
    CArgs args = GetArgs();

    if (args["version-full"]) {
        printf(NETCACHED_FULL_VERSION "\n");
        return 0;
    }
    LOG_POST_X(8, NETCACHED_FULL_VERSION);

    {{
        AutoPtr<CNetCacheServer> server(new CNetCacheServer(args["reinit"]));

        LOG_POST_X(9, "Running server on port " << server->GetPort());
        server->Run();

        if (server->GetSignalCode()) {
            LOG_POST_X(10, "Server got "
                           << server->GetSignalCode() << " signal.");
        }
    }}
    CNCFileSystem ::Finalize();
    CSQLITE_Global::Finalize();
    CNCMemManager ::FinalizeApp();

    return 0;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    // Main thread request context already created, so is not affected
    // by just set default, so set it manually.
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_ToStdlog);
}
