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

#ifdef NCBI_OS_LINUX
# include <signal.h>
# include <sys/time.h>
# include <sys/resource.h>
#endif

#include "message_handler.hpp"
#include "netcached.hpp"
#include "netcache_version.hpp"
#include "nc_memory.hpp"
#include "nc_stat.hpp"
#include "distribution_conf.hpp"
#include "sync_log.hpp"
#include "peer_control.hpp"
#include "nc_storage.hpp"
#include "active_handler.hpp"



BEGIN_NCBI_SCOPE


static const char* kNCReg_ServerSection       = "server";
static const char* kNCReg_Ports               = "ports";
static const char* kNCReg_CtrlPort            = "control_port";
static const char* kNCReg_MaxConnections      = "max_connections";
static const char* kNCReg_MaxThreads          = "max_threads";
static const char* kNCReg_LogCmds             = "log_requests";
static const char* kNCReg_AdminClient         = "admin_client_name";
static const char* kNCReg_DefAdminClient      = "netcache_control";
//static const char* kNCReg_MemLimit            = "db_cache_limit";
static const char* kNCReg_MemLimit            = "memory_limit";
static const char* kNCReg_MemAlert            = "memory_alert";
static const char* kNCReg_ForceUsePoll        = "force_use_poll";
static const char* kNCReg_SpecPriority        = "app_setup_priority";
static const char* kNCReg_NetworkTimeout      = "network_timeout";
static const char* kNCReg_DisableClient       = "disable_client";
static const char* kNCReg_CommandTimeout      = "cmd_timeout";
static const char* kNCReg_BlobTTL             = "blob_ttl";
static const char* kNCReg_VerTTL              = "ver_ttl";
static const char* kNCReg_TTLUnit             = "ttl_unit";
static const char* kNCReg_ProlongOnRead       = "prolong_on_read";
static const char* kNCReg_SearchOnRead        = "search_on_read";
static const char* kNCReg_Quorum              = "quorum";
static const char* kNCReg_FastOnMain          = "fast_quorum_on_main";
static const char* kNCReg_PassPolicy          = "blob_password_policy";
static const char* kNCReg_AppSetupPrefix      = "app_setup_";
static const char* kNCReg_AppSetupValue       = "setup";


CNetCacheServer* g_NetcacheServer = NULL;
CNCBlobStorage*  g_NCStorage      = NULL;
static CFastMutex s_CachingLock;


extern "C" void
s_NCSignalHandler(int sig)
{
    if (g_NetcacheServer) {
        g_NetcacheServer->RequestShutdown(sig);
    }
}

static inline int
s_CompareStrings(const string& left, const string& right)
{
    if (left.size() != right.size())
        return int(left.size()) - int(right.size());
    else
        return left.compare(right);
}



SNCSpecificParams::~SNCSpecificParams(void)
{}


inline
CNetCacheServer::SSpecParamsEntry::SSpecParamsEntry(const string& key,
                                                    CObject*      value)
    : key(key),
      value(value)
{}


CNetCacheServer::SSpecParamsSet::~SSpecParamsSet(void)
{}


void
CNetCacheServer::x_ReadSpecificParams(const IRegistry&   reg,
                                      const string&      section,
                                      SNCSpecificParams* params)
{
    if (reg.HasEntry(section, kNCReg_DisableClient, IRegistry::fCountCleared)) {
        params->disable = reg.GetBool(section, kNCReg_DisableClient, false);
    }
    if (reg.HasEntry(section, kNCReg_CommandTimeout, IRegistry::fCountCleared)) {
        params->cmd_timeout = reg.GetInt(section, kNCReg_CommandTimeout, 600);
    }
    if (reg.HasEntry(section, kNCReg_NetworkTimeout, IRegistry::fCountCleared)) {
        params->conn_timeout = reg.GetInt(section, kNCReg_NetworkTimeout, 10);
        if (params->conn_timeout == 0) {
            INFO_POST("INI file sets network timeout to 0. Assuming 10 seconds.");
            params->conn_timeout =  10;
        }
    }
    if (reg.HasEntry(section, kNCReg_BlobTTL, IRegistry::fCountCleared)) {
        params->blob_ttl = reg.GetInt(section, kNCReg_BlobTTL, 3600);
    }
    if (reg.HasEntry(section, kNCReg_VerTTL, IRegistry::fCountCleared)) {
        params->ver_ttl = reg.GetInt(section, kNCReg_VerTTL, 3600);
    }
    if (reg.HasEntry(section, kNCReg_TTLUnit, IRegistry::fCountCleared)) {
        params->ttl_unit = reg.GetInt(section, kNCReg_TTLUnit, 300);
    }
    if (reg.HasEntry(section, kNCReg_ProlongOnRead, IRegistry::fCountCleared)) {
        params->prolong_on_read = reg.GetBool(section, kNCReg_ProlongOnRead, true);
    }
    if (reg.HasEntry(section, kNCReg_SearchOnRead, IRegistry::fCountCleared)) {
        params->srch_on_read = reg.GetBool(section, kNCReg_SearchOnRead, true);
    }
    if (reg.HasEntry(section, kNCReg_Quorum, IRegistry::fCountCleared)) {
        params->quorum = reg.GetInt(section, kNCReg_Quorum, 2);
    }
    if (reg.HasEntry(section, kNCReg_FastOnMain, IRegistry::fCountCleared)) {
        params->fast_on_main = reg.GetBool(section, kNCReg_FastOnMain, true);
    }
    if (reg.HasEntry(section, kNCReg_PassPolicy, IRegistry::fCountCleared)) {
        string pass_policy = reg.GetString(section, kNCReg_PassPolicy, "any");
        if (pass_policy == "no_password") {
            params->pass_policy = eNCOnlyWithoutPass;
        }
        else if (pass_policy == "with_password") {
            params->pass_policy = eNCOnlyWithPass;
        }
        else {
            if (pass_policy != "any") {
                ERR_POST("Incorrect value of '" << kNCReg_PassPolicy
                         << "' parameter: '" << pass_policy
                         << "', assuming 'any'");
            }
            params->pass_policy = eNCBlobPassAny;
        }
    }
}

inline void
CNetCacheServer::x_PutNewParams(SSpecParamsSet*         params_set,
                                unsigned int            best_index,
                                const SSpecParamsEntry& entry)
{
    params_set->entries.insert(params_set->entries.begin() + best_index, entry);
}

CNetCacheServer::SSpecParamsSet*
CNetCacheServer::x_FindNextParamsSet(const SSpecParamsSet*  cur_set,
                                     const string&          key,
                                     unsigned int&          best_index)
{
    unsigned int low = 1;
    unsigned int high = Uint4(cur_set->entries.size());
    while (low < high) {
        unsigned int mid = (low + high) / 2;
        const SSpecParamsEntry& entry = cur_set->entries[mid];
        int comp_res = s_CompareStrings(key, entry.key);
        if (comp_res == 0) {
            best_index = mid;
            return static_cast<SSpecParamsSet*>(entry.value.GetNCPointer());
        }
        else if (comp_res > 0)
            low = mid + 1;
        else
            high = mid;
    }
    best_index = high;
    return NULL;
}

void
CNetCacheServer::x_CheckDefClientConfig(SSpecParamsSet* cur_set,
                                        SSpecParamsSet* prev_set,
                                        Uint1 depth,
                                        SSpecParamsSet* deflt)
{
    if (depth == 0) {
        if (cur_set->entries.size() == 0) {
            cur_set->entries.push_back(deflt->entries[0]);
        }
    }
    else {
        SSpecParamsSet* this_deflt = NULL;
        if (deflt)
            this_deflt = (SSpecParamsSet*)deflt->entries[0].value.GetPointer();
        for (size_t i = 0; i < cur_set->entries.size(); ++i) {
            x_CheckDefClientConfig((SSpecParamsSet*)cur_set->entries[i].value.GetPointer(),
                                   cur_set, depth - 1, this_deflt);
            this_deflt = (SSpecParamsSet*)cur_set->entries[0].value.GetPointer();
        }
    }
}

void
CNetCacheServer::x_ReadPerClientConfig(const CNcbiRegistry& reg)
{
    m_OldSpecParams = m_SpecParams;

    string spec_prty = reg.Get(kNCReg_ServerSection, kNCReg_SpecPriority);
    NStr::Tokenize(spec_prty, ", \t\r\n", m_SpecPriority, NStr::eMergeDelims);

    SNCSpecificParams* main_params = new SNCSpecificParams;
    main_params->disable         = false;
    main_params->cmd_timeout     = 600;
    main_params->conn_timeout    = 10;
    main_params->blob_ttl        = 3600;
    main_params->ver_ttl         = 3600;
    main_params->ttl_unit        = 300;
    main_params->prolong_on_read = true;
    main_params->srch_on_read    = false;
    main_params->pass_policy     = eNCBlobPassAny;
    main_params->quorum          = 2;
    main_params->fast_on_main    = true;
    x_ReadSpecificParams(reg, kNCReg_ServerSection, main_params);
    m_DefConnTimeout = main_params->conn_timeout;
    m_DefBlobTTL = main_params->blob_ttl;
    SSpecParamsSet* params_set = new SSpecParamsSet();
    params_set->entries.push_back(SSpecParamsEntry(kEmptyStr, main_params));
    for (unsigned int i = 0; i < m_SpecPriority.size(); ++i) {
        SSpecParamsSet* next_set = new SSpecParamsSet();
        next_set->entries.push_back(SSpecParamsEntry(kEmptyStr, params_set));
        params_set = next_set;
    }
    m_SpecParams = params_set;

    if (m_SpecPriority.size() != 0) {
        list<string> conf_sections;
        reg.EnumerateSections(&conf_sections);
        ITERATE(list<string>, sec_it, conf_sections) {
            const string& section = *sec_it;
            if (!NStr::StartsWith(section, kNCReg_AppSetupPrefix, NStr::eNocase))
                continue;
            SSpecParamsSet* cur_set  = m_SpecParams;
            NON_CONST_REVERSE_ITERATE(TSpecKeysList, prty_it, m_SpecPriority) {
                const string& key_name = *prty_it;
                if (reg.HasEntry(section, key_name, IRegistry::fCountCleared)) {
                    const string& key_value = reg.Get(section, key_name);
                    unsigned int next_ind = 0;
                    SSpecParamsSet* next_set
                                = x_FindNextParamsSet(cur_set, key_value, next_ind);
                    if (!next_set) {
                        if (cur_set->entries.size() == 0) {
                            cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, new SSpecParamsSet()));
                            ++next_ind;
                        }
                        next_set = new SSpecParamsSet();
                        x_PutNewParams(cur_set, next_ind,
                                       SSpecParamsEntry(key_value, next_set));
                    }
                    cur_set = next_set;
                }
                else {
                    if (cur_set->entries.size() == 0) {
                        cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, new SSpecParamsSet()));
                    }
                    cur_set = (SSpecParamsSet*)cur_set->entries[0].value.GetPointer();
                }
            }
            if (cur_set->entries.size() != 0) {
                ERR_POST("Section '" << section << "' in configuration file is "
                         "a duplicate of another section - ignoring it.");
                continue;
            }
            SNCSpecificParams* params = new SNCSpecificParams(*main_params);
            if (reg.HasEntry(section, kNCReg_AppSetupValue)) {
                x_ReadSpecificParams(reg, reg.Get(section, kNCReg_AppSetupValue),
                                     params);
            }
            cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, params));
        }

        x_CheckDefClientConfig(m_SpecParams, NULL, Uint1(m_SpecPriority.size()), NULL);
    }
}

bool
CNetCacheServer::x_ReadServerParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    string ports_str = reg.Get(kNCReg_ServerSection, kNCReg_Ports);
    if (ports_str != m_PortsConfStr) {
        list<string> split_ports;
        NStr::Split(ports_str, ", \t\r\n", split_ports);
        TPortsList ports_list;
        ITERATE(list<string>, it, split_ports) {
            try {
                ports_list.insert(NStr::StringToInt(*it));
            }
            catch (CStringException& ex) {
                ERR_POST(Critical << "Error reading port number from '"
                                  << *it << "': " << ex);
                return false;
            }
        }
        ITERATE(TPortsList, it, ports_list) {
            unsigned int port = *it;
            if (m_Ports.find(port) == m_Ports.end())
                m_Ports.insert(port);
        }
        if (ports_list.size() != m_Ports.size()) {
            ERR_POST("After reconfiguration some ports should not be "
                     "listened anymore. It requires restarting of the server.");
        }
        if (m_Ports.size() == 0) {
            ERR_POST(Critical << "No listening client ports were configured.");
            return false;
        }
        m_PortsConfStr = ports_str;
    }

    try {
        m_CtrlPort = reg.GetInt(kNCReg_ServerSection, kNCReg_CtrlPort, 0);

        m_LogCmds     = reg.GetBool  (kNCReg_ServerSection, kNCReg_LogCmds, true);
        m_AdminClient = reg.GetString(kNCReg_ServerSection, kNCReg_AdminClient,
                                      kNCReg_DefAdminClient);
        bool force_poll = reg.GetBool(kNCReg_ServerSection, kNCReg_ForceUsePoll, true);
        SOCK_SetIOWaitSysAPI(force_poll? eSOCK_IOWaitSysAPIPoll: eSOCK_IOWaitSysAPIAuto);

        SServer_Parameters serv_params;
        serv_params.max_connections = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxConnections, 100);
        serv_params.max_threads     = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxThreads, 20);
        if (serv_params.max_threads < 1) {
            serv_params.max_threads = 1;
        }
        serv_params.init_threads = 1;
        serv_params.accept_timeout = &m_ServerAcceptTimeout;
        try {
            SetParameters(serv_params);
        }
        catch (CServer_Exception& ex) {
            ERR_POST(Critical << "Cannot set server parameters: " << ex);
            return false;
        }

        m_DebugMode = reg.GetBool(kNCReg_ServerSection, "debug_mode", false);

        string str_val   = reg.GetString(kNCReg_ServerSection, kNCReg_MemLimit, "1Gb");
        size_t mem_limit = size_t(NStr::StringToUInt8_DataSize(str_val));
        str_val          = reg.GetString(kNCReg_ServerSection, kNCReg_MemAlert, "4Gb");
        size_t mem_alert = size_t(NStr::StringToUInt8_DataSize(str_val));
        CNCMemManager::SetLimits(mem_limit, mem_alert);
        //CNCMemManager::SetLimits(mem_limit, mem_limit);

        x_ReadPerClientConfig(reg);
    }
    catch (CStringException& ex) {
        ERR_POST(Critical << "Error in configuration: " << ex);
        return false;
    }

    return true;
}

CNetCacheServer::CNetCacheServer(void)
    : m_SpecParams(NULL),
      m_OldSpecParams(NULL),
      m_Shutdown(false),
      m_Signal(0),
      m_InitiallySynced(false),
      m_CachingComplete(false)
{}

CNetCacheServer::~CNetCacheServer()
{}

void
CNetCacheServer::Init(void)
{
    INCBlockedOpListener::BindToThreadPool(GetThreadPool());
}

bool
CNetCacheServer::Initialize(bool do_reinit)
{
    m_ServerAcceptTimeout.sec = 1;
    m_ServerAcceptTimeout.usec = 0;

    if (!x_ReadServerParams())
        return false;

    if (!CNCDistributionConf::Initialize(m_CtrlPort))
        return false;
    CNCActiveHandler::Initialize();

    CFastMutexGuard guard(s_CachingLock);

    _ASSERT(g_NetcacheServer == NULL);
    g_NetcacheServer = this;

    g_NCStorage = new CNCBlobStorage();
    if (!g_NCStorage->Initialize(do_reinit))
        return false;

    Uint8 max_rec_no = g_NCStorage->GetMaxSyncLogRecNo();
    CNCSyncLog::Initialize(g_NCStorage->IsCleanStart(), max_rec_no);

    m_StartTime = GetFastLocalTime();

    if (!CNCPeerControl::Initialize())
        return false;
    CNCPeriodicSync::PreInitialize();

    if (m_Ports.find(m_CtrlPort) == m_Ports.end()) {
        Uint4 port = m_CtrlPort;
        if (m_DebugMode)
            port += 10;
        INFO_POST("Opening control port: " << port);
        AddListener(new CNCMsgHndlFactory_Proxy(), port);
    }
    string ports_str;
    ITERATE(TPortsList, it, m_Ports) {
        unsigned int port = *it;
        if (IsDebugMode())
            port += 10;
        AddListener(new CNCMsgHndlFactory_Proxy(), port);
        ports_str.append(NStr::IntToString(port));
        ports_str.append(", ", 2);
    }
    ports_str.resize(ports_str.size() - 2);
    INFO_POST("Opening client ports: " << ports_str);
    try {
        StartListening();
    }
    catch (CServer_Exception& ex) {
        ERR_POST(Critical << "Cannot listen to control port: " << ex);
        return false;
    }

    return true;
}

void
CNetCacheServer::Finalize(void)
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintLog);
    INFO_POST("NetCache server is destroying. Usage statistics:");
    x_PrintServerStats(proxy);

    if (g_NCStorage)
        UpdateLastRecNo();
    CNCPeriodicSync::Finalize();
    CNCPeerControl::Finalize();
    if (GetThreadPool())
        GetThreadPool()->KillAllThreads(true);
    if (g_NCStorage)
        g_NCStorage->Finalize();
    //delete g_NCStorage;
    //delete s_TaskPool;
    CNCSyncLog::Finalize();
    CNCDistributionConf::Finalize();

    //g_NetcacheServer = NULL;
}

const SNCSpecificParams*
CNetCacheServer::GetAppSetup(const TStringMap& client_params)
{
    const SSpecParamsSet* cur_set = m_SpecParams;
    NON_CONST_REVERSE_ITERATE(TSpecKeysList, key_it, m_SpecPriority) {
        TStringMap::const_iterator it = client_params.find(*key_it);
        const SSpecParamsSet* next_set = NULL;
        if (it != client_params.end()) {
            const string& value = it->second;
            unsigned int best_index = 0;
            next_set = x_FindNextParamsSet(cur_set, value, best_index);
        }
        if (!next_set)
            next_set = static_cast<const SSpecParamsSet*>(cur_set->entries[0].value.GetPointer());
        cur_set = next_set;
    }
    return static_cast<const SNCSpecificParams*>(cur_set->entries[0].value.GetPointer());
}
/*
void
CNetCacheServer::Reconfigure(void)
{
    CNcbiApplication::Instance()->ReloadConfig();
    x_ReadServerParams();
    g_NCStorage->Reconfigure();
}
*/
void
CNetCacheServer::x_PrintServerStats(CPrintTextProxy& proxy)
{
    SServer_Parameters params;
    GetParameters(&params);

    proxy << "Time - " << CTime(CTime::eCurrent)
                       << ", started at " << m_StartTime << endl
          << "Env  - " << CThread::GetThreadsCount() << " (cur thr), "
                       << params.max_threads << " (max thr), "
                       << params.max_connections << " (max conns)" << endl
          << endl;
    CNCMemManager::PrintStats(proxy);
    proxy << endl;
    if (g_NCStorage)
        g_NCStorage->PrintStat(proxy);
    proxy << endl
          << "Copy queue - " << CNCPeerControl::GetMirrorQueueSize() << endl
          << "Sync log queue - " << g_ToSmartStr(CNCSyncLog::GetLogSize()) << endl;
    CNCStat::Print(proxy);
}

int
CNetCacheServer::GetMaxConnections(void)
{
    SServer_Parameters params;
    GetParameters(&params);
    return params.max_connections;
}

bool
CNetCacheServer::IsInitiallySynced(void)
{
    return g_NetcacheServer  &&  g_NetcacheServer->m_InitiallySynced;
}

void
CNetCacheServer::InitialSyncComplete(void)
{
    INFO_POST("Initial synchronization complete");
    g_NetcacheServer->m_InitiallySynced = true;
}

void
CNetCacheServer::UpdateLastRecNo(void)
{
    g_NCStorage->SaveMaxSyncLogRecNo();
}

void
CNetCacheServer::CachingCompleted(void)
{
    // Avoid race with constructor
    CFastMutexGuard guard(s_CachingLock);

    if (g_NetcacheServer == NULL)
        return;

    g_NetcacheServer->m_CachingComplete = true;
    if (!CNCPeriodicSync::Initialize()) {
        g_NetcacheServer->RequestShutdown();
    }
}

bool
CNetCacheServer::IsCachingComplete(void)
{
    return g_NetcacheServer  &&  g_NetcacheServer->m_CachingComplete;
}

bool
CNetCacheServer::IsDebugMode(void)
{
    return g_NetcacheServer  &&  g_NetcacheServer->m_DebugMode;
}


CNetCacheDApp::CNetCacheDApp(void)
{
    CVersionInfo version(NCBI_PACKAGE_VERSION_MAJOR,
                         NCBI_PACKAGE_VERSION_MINOR,
                         NCBI_PACKAGE_VERSION_PATCH);
    CRef<CVersion> full_version(new CVersion(version));

    full_version->AddComponentVersion("Storage",
                                      NETCACHED_STORAGE_VERSION_MAJOR,
                                      NETCACHED_STORAGE_VERSION_MINOR,
                                      NETCACHED_STORAGE_VERSION_PATCH);
    full_version->AddComponentVersion("Protocol",
                                      NETCACHED_PROTOCOL_VERSION_MAJOR,
                                      NETCACHED_PROTOCOL_VERSION_MINOR,
                                      NETCACHED_PROTOCOL_VERSION_PATCH);

    SetVersion(version);
    SetFullVersion(full_version);
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

    arg_desc->AddFlag("reinit", "Recreate the storage database.");
#ifdef NCBI_OS_LINUX
    arg_desc->AddFlag("nodaemon",
                      "Turn off daemonization of NetCache at the start.");
#endif
    arg_desc->AddFlag("version-full", "Version");

    SetupArgDescriptions(arg_desc.release());
}

int
CNetCacheDApp::Run(void)
{
    CArgs args = GetArgs();

    INFO_POST(NETCACHED_FULL_VERSION);

#ifdef NCBI_OS_LINUX
    if (!args["nodaemon"]) {
        INFO_POST("Entering UNIX daemon mode...");
        // Here's workaround for SQLite3 bug: if stdin is closed in forked
        // process then 0 file descriptor is returned to SQLite after open().
        // But there's assert there which prevents fd to be equal to 0. So
        // we keep descriptors 0, 1 and 2 in child process open. Latter two -
        // just in case somebody will try to write to them.
        bool is_good = CProcess::Daemonize(kEmptyCStr,
                               CProcess::fDontChroot | CProcess::fKeepStdin
                                                     | CProcess::fKeepStdout);
        if (!is_good) {
            ERR_POST(Critical << "Error during daemonization");
            return 200;
        }
    }

    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  s_NCSignalHandler);
    signal(SIGTERM, s_NCSignalHandler);
#endif

    CNetCacheServer* server = NULL;
    CAsyncDiagHandler diag_handler;
    if (!CNCMemManager::InitializeApp())
        goto fin_mem;
    CSQLITE_Global::Initialize();
    server = new CNetCacheServer();
    if (server->Initialize(args["reinit"])) {
        try {
            diag_handler.InstallToDiag();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
            goto fin_server;
        }
        server->Run();
        if (server->GetSignalCode()) {
            INFO_POST("Server got " << server->GetSignalCode() << " signal.");
        }
    }
fin_server:
    server->Finalize();
    //delete server;
    CSQLITE_Global::Finalize();
    diag_handler.RemoveFromDiag();
fin_mem:
    CNCMemManager::FinalizeApp();

    return 0;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    // Main thread request context already created, so is not affected
    // by just set default, so set it manually.
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    SetDiagPostLevel(eDiag_Warning);

    // Defaults that should be always set for NetCache
    CNcbiEnvironment env;
    env.Set("NCBI_ABORT_ON_NULL", "true");
    env.Set("DIAG_SILENT_ABORT", "0");
    env.Set("DEBUG_CATCH_UNHANDLED_EXCEPTIONS", "false");
    env.Set("THREAD_CATCH_UNHANDLED_EXCEPTIONS", "false");
    env.Set("THREADPOOL_CATCH_UNHANDLED_EXCEPTIONS", "false");
    env.Set("CSERVER_CATCH_UNHANDLED_EXCEPTIONS", "false");
#ifdef NCBI_OS_LINUX
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        rlim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlim);
    }
#endif

    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_ToStdlog);
}
