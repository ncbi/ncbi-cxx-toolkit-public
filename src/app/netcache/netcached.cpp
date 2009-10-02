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



BEGIN_NCBI_SCOPE;

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
    : m_MaxConnSpan(0),
      m_ClosedConns(0),
      m_OpenedConns(0),
      m_OverflowConns(0),
      m_ConnsSpansSum(0),
      m_MaxCmdSpan(0),
      m_CntCmds(0),
      m_CmdSpans(0),
      m_TimedOutCmds(0)
{}

void
CNCServerStat::AddFinishedCmd(const char*           cmd,
                              double                cmd_span,
                              const vector<double>& state_spans)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);

    ++stat->m_CntCmds;
    ++stat->m_CntCmdsByCmd[cmd];
    stat->m_CmdSpans           += cmd_span;
    stat->m_CmdSpansByCmd[cmd] += cmd_span;
    stat->m_MaxCmdSpan          = max(stat->m_MaxCmdSpan, cmd_span);
    double& max_by_cmd          = stat->m_MaxCmdSpanByCmd[cmd];
    max_by_cmd                  = max(max_by_cmd, cmd_span);

    if (stat->m_StatesSpansSums.size() < state_spans.size()) {
        stat->m_StatesSpansSums.resize(state_spans.size(), 0);
    }
    for (size_t i = 0; i < state_spans.size(); ++i) {
        stat->m_StatesSpansSums[i] += state_spans[i];
    }
}

inline void
CNCServerStat::x_CollectTo(CNCServerStat* other)
{
    CFastMutexGuard guard(m_ObjLock);

    other->m_MaxConnSpan = max(other->m_MaxConnSpan, m_MaxConnSpan);
    other->m_ClosedConns += m_ClosedConns;
    other->m_OpenedConns += m_OpenedConns;
    other->m_OverflowConns += m_OverflowConns;
    other->m_ConnsSpansSum += m_ConnsSpansSum;
    other->m_MaxCmdSpan = max(other->m_MaxCmdSpan, m_MaxCmdSpan);
    ITERATE(TCmdsSpansMap, it, m_MaxCmdSpanByCmd) {
        double& other_val = other->m_MaxCmdSpanByCmd[it->first];
        other_val = max(other_val, it->second);
    }
    other->m_CntCmds += m_CntCmds;
    ITERATE(TCmdsCountsMap, it, m_CntCmdsByCmd) {
        other->m_CntCmdsByCmd[it->first] += it->second;
    }
    other->m_CmdSpans += m_CmdSpans;
    ITERATE(TCmdsSpansMap, it, m_CmdSpansByCmd) {
        other->m_CmdSpansByCmd[it->first] += it->second;
    }
    if (other->m_StatesSpansSums.size() < m_StatesSpansSums.size()) {
        other->m_StatesSpansSums.resize(m_StatesSpansSums.size(), 0);
    }
    for (size_t i = 0; i < m_StatesSpansSums.size(); ++i) {
        other->m_StatesSpansSums[i] += m_StatesSpansSums[i];
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

    proxy << "Number of connections opened      - " << stat.m_OpenedConns << endl
          << "Number of connections closed      - " << stat.m_ClosedConns << endl
          << "Number of connections overflowed  - " << stat.m_OverflowConns << endl
          << "Maximum time connection was alive - " << stat.m_MaxConnSpan << endl
          << "Average time connection was alive - "
                          << g_SafeDiv(stat.m_ConnsSpansSum, stat.m_ClosedConns) << endl
          << endl
          << "Number of commands received   - " << stat.m_CntCmds << endl
          << "Number of commands timed out  - " << stat.m_TimedOutCmds << endl
          << "Maximum time command executed - " << stat.m_MaxCmdSpan << endl
          << "Average time command executed - "
                          << g_SafeDiv(stat.m_CmdSpans, stat.m_CntCmds) << endl
          << "Commands by type:" << endl;
    TCmdsCountsMap::iterator it_cnt  = stat.m_CntCmdsByCmd   .begin();
    TCmdsSpansMap::iterator  it_span = stat.m_CmdSpansByCmd  .begin();
    TCmdsSpansMap::iterator  it_max  = stat.m_MaxCmdSpanByCmd.begin();
    for (; it_cnt != stat.m_CntCmdsByCmd.end(); ++it_cnt, ++it_span, ++it_max) {
        _ASSERT(!SConstCharCompare()(it_cnt->first, it_span->first)
                &&  !SConstCharCompare()(it_span->first, it_cnt->first));
        _ASSERT(!SConstCharCompare()(it_cnt->first, it_max->first)
                &&  !SConstCharCompare()(it_max->first, it_cnt->first));

        proxy << it_cnt->first << " - " << it_cnt->second << " (cnt), "
              << g_SafeDiv(it_span->second, it_cnt->second) << " (avg time), "
              << it_max->second << " (max time)" << endl;
    }

    proxy << endl
          << "Average times of handlers' states:" << endl;
    for (size_t i = 0; i < stat.m_StatesSpansSums.size(); ++i) {
        proxy << CNCMessageHandler::GetStateName(int(i)) << " - "
              << g_SafeDiv(stat.m_StatesSpansSums[i], stat.m_CntCmds) << " ("
              << int(g_SafeDiv(stat.m_StatesSpansSums[i], stat.m_CmdSpans) * 100)
                                                  << "% of total)" << endl;
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

        CNCBlobStorage* storage = new CNCBlobStorage(reg, section_name,
                                                     reinit);
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
        unsigned int hostaddr = SOCK_HostToNetLong(SOCK_gethostbyname(0));
        char ipaddr[32];
        sprintf(ipaddr, "%u.%u.%u.%u", (hostaddr >> 24) & 0xff,
                                       (hostaddr >> 16) & 0xff,
                                       (hostaddr >> 8)  & 0xff,
                                        hostaddr        & 0xff);
        m_Host = ipaddr;
    }

    SServer_Parameters params;
    params.max_connections = x_RegReadInt(reg, kNCReg_MaxConnections, 100);
    params.max_threads     = x_RegReadInt(reg, kNCReg_MaxThreads,     20);
    // A bit of hard coding but it's really not necessary to create more than
    // 25 threads in server's thread pool.
    if (params.max_threads >= 25) {
        ERR_POST(Warning << "NetCache configuration is not optimal. "
                            "Maximum optimal number of threads is 25, "
                            "maximum number given - " << params.max_threads
                         << ".");
    }
    params.init_threads    = x_RegReadInt(reg, kNCReg_InitThreads,    10);
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
        CNCMemManager::SetLimit(size_t(NStr::StringToUInt8_DataSize(
                              x_RegReadString(reg, kNCReg_MemLimit, "1Gb"))));
    }
    catch (CStringException& ex) {
        ERR_POST_X(14, "Error in " << kNCReg_MemLimit << " parameter: " << ex);
    }

    CSQLITE_Global::Initialize();
    CSQLITE_Global::EnableSharedCache();
    x_CreateStorages(reg, do_reinit);

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

    proxy << "Server started at " << m_StartTime << endl
          << "Current # of threads      - "
                        << CThread::GetThreadsCount() << endl
          << "Initial # of threads      - "
                        << params.init_threads << endl
          << "Maximum # of threads      - "
                        << params.max_threads << endl
          << "Maximum # of connections  - "
                        << params.max_connections << endl
          << "Command execution timeout - " << m_CmdTimeout << endl
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
    CSQLITE_Global::Finalize();

    return 0;
}

END_NCBI_SCOPE;


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
