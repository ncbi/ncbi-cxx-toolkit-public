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
#include "nc_stat.hpp"
#include "error_codes.hpp"



BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Main


static const char* kNCReg_ServerSection       = "server";
static const char* kNCReg_Ports               = "ports";
static const char* kNCReg_MaxConnections      = "max_connections";
static const char* kNCReg_MaxThreads          = "max_threads";
static const char* kNCReg_LogCmds             = "log_requests";
static const char* kNCReg_AdminClient         = "admin_client_name";
static const char* kNCReg_DefAdminClient      = "netcache_control";
static const char* kNCReg_MemLimit            = "memory_limit";
static const char* kNCReg_MemAlert            = "memory_alert";
static const char* kNCReg_SpecPriority        = "app_setup_priority";
static const char* kNCReg_NetworkTimeout      = "network_timeout";
static const char* kNCReg_DisableClient       = "disable_client";
static const char* kNCReg_CommandTimeout      = "cmd_timeout";
static const char* kNCReg_BlobTTL             = "blob_ttl";
static const char* kNCReg_ProlongOnRead       = "prolong_on_read";
static const char* kNCReg_PassPolicy          = "blob_password_policy";
static const char* kNCReg_AppSetupPrefix      = "app_setup_";
static const char* kNCReg_AppSetupValue       = "setup";


CNetCacheServer* g_NetcacheServer = NULL;
CNCBlobStorage*  g_NCStorage      = NULL;


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
        params->disable = reg.GetBool(section, kNCReg_DisableClient,
                                      false, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_CommandTimeout, IRegistry::fCountCleared)) {
        params->cmd_timeout = reg.GetInt(section, kNCReg_CommandTimeout,
                                         600, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_NetworkTimeout, IRegistry::fCountCleared)) {
        params->conn_timeout = reg.GetInt(section, kNCReg_NetworkTimeout,
                                          10, 0, IRegistry::eErrPost);
        if (params->conn_timeout == 0) {
            ERR_POST_X(2, "INI file sets network timeout to 0. Assuming 10 seconds.");
            params->conn_timeout =  10;
        }
    }
    if (reg.HasEntry(section, kNCReg_BlobTTL, IRegistry::fCountCleared)) {
        params->blob_ttl = reg.GetInt(section, kNCReg_BlobTTL,
                                      3600, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_ProlongOnRead, IRegistry::fCountCleared)) {
        params->prolong_on_read = reg.GetBool(section, kNCReg_ProlongOnRead,
                                              true, 0, IRegistry::eErrPost);
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
                ERR_POST_X(16, "Incorrect value of '" << kNCReg_PassPolicy
                               << "' parameter: '" << pass_policy << "'");
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
    unsigned int high = cur_set->entries.size();
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
            catch (CException& ex) {
                ERR_POST_X(17, "Error reading port number from '"
                               << *it << "': " << ex);
            }
        }
        ITERATE(TPortsList, it, ports_list) {
            unsigned int port = *it;
            if (m_Ports.find(port) == m_Ports.end()) {
                AddListener(new CNCMsgHndlFactory_Proxy(), port);
                m_Ports.insert(port);
            }
        }
        StartListening();
        if (ports_list.size() != m_Ports.size()) {
            ERR_POST_X(18, "After reconfiguration some ports should not be "
                           "listened anymore. It requires restarting of the server.");
        }
        if (m_Ports.size() == 0) {
            NCBI_THROW(CUtilException, eWrongData,
                       "No listening ports were configured.");
        }
        m_PortsConfStr = ports_str;

        ports_str.clear();
        ITERATE(TPortsList, it, m_Ports) {
            ports_str.append(NStr::IntToString(*it));
            ports_str.append(", ", 2);
        }
        ports_str.resize(ports_str.size() - 2);
        INFO_POST("Server is listening to these ports: " << ports_str);
    }

    m_LogCmds     = reg.GetBool  (kNCReg_ServerSection, kNCReg_LogCmds, true,
                                  IRegistry::eErrPost);
    m_AdminClient = reg.GetString(kNCReg_ServerSection, kNCReg_AdminClient,
                                  kNCReg_DefAdminClient);

    SServer_Parameters params;
    params.max_connections = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxConnections,
                                        100, IRegistry::eErrPost);
    params.max_threads     = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxThreads,
                                        20,  IRegistry::eErrPost);
    // A bit of hard coding but it's really not necessary to create more than
    // 20 threads in server's thread pool.
    if (params.max_threads > 20) {
        ERR_POST(Warning << "NetCache configuration is not optimal. "
                            "Maximum optimal number of threads is 20, "
                            "maximum number given - " << params.max_threads
                         << ".");
    }
    if (params.max_threads < 1) {
        params.max_threads = 1;
    }
    params.init_threads = 1;
    params.accept_timeout = &m_ServerAcceptTimeout;
    SetParameters(params);

    try {
        string str_val   = reg.GetString(kNCReg_ServerSection, kNCReg_MemLimit, "1Gb");
        size_t mem_limit = size_t(NStr::StringToUInt8_DataSize(str_val));
        str_val          = reg.GetString(kNCReg_ServerSection, kNCReg_MemAlert, "4Gb");
        size_t mem_alert = size_t(NStr::StringToUInt8_DataSize(str_val));
        CNCMemManager::SetLimits(mem_limit, mem_alert);
    }
    catch (CStringException& ex) {
        ERR_POST_X(14, "Error in " << kNCReg_MemLimit
                         << " or " << kNCReg_MemAlert << " parameter: " << ex);
    }

    m_OldSpecParams = m_SpecParams;

    string spec_prty = reg.Get(kNCReg_ServerSection, kNCReg_SpecPriority);
    list<string> prty_list;
    NStr::Split(spec_prty, " \t\r\n", prty_list);
    m_SpecPriority.assign(prty_list.begin(), prty_list.end());

    SNCSpecificParams* main_params = new SNCSpecificParams;
    main_params->disable         = false;
    main_params->cmd_timeout     = 600;
    main_params->conn_timeout    = 10;
    main_params->blob_ttl        = 3600;
    main_params->prolong_on_read = true;
    main_params->pass_policy     = eNCBlobPassAny;
    x_ReadSpecificParams(reg, kNCReg_ServerSection, main_params);
    m_DefConnTimeout = main_params->conn_timeout;
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
            SSpecParamsSet* prev_set = NULL;
            ITERATE(TSpecKeysList, prty_it, m_SpecPriority) {
                const string& key_name = *prty_it;
                if (reg.HasEntry(section, key_name, IRegistry::fCountCleared)) {
                    const string& key_value = reg.Get(section, key_name);
                    unsigned int next_ind = 0;
                    SSpecParamsSet* next_set
                                = x_FindNextParamsSet(cur_set, key_value, next_ind);
                    if (!next_set) {
                        if (cur_set->entries.size() == 0) {
                            cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, static_cast<SSpecParamsSet*>(prev_set->entries[0].value.GetPointer())->entries[0].value));
                        }
                        next_set = new SSpecParamsSet();
                        x_PutNewParams(cur_set, next_ind,
                                       SSpecParamsEntry(key_value, next_set));
                    }
                    prev_set = cur_set;
                    cur_set = next_set;
                }
                else {
                    cur_set = static_cast<SSpecParamsSet*>(cur_set->entries[0].value.GetPointer());
                }
            }
            if (cur_set->entries.size() != 0) {
                ERR_POST_X(19, "Section '" << section << "' in configuration file is "
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
    }
}

CNetCacheServer::CNetCacheServer(bool do_reinit)
    : m_SpecParams(NULL),
      m_OldSpecParams(NULL),
      m_Shutdown(false),
      m_Signal(0)
{
    m_ServerAcceptTimeout.sec = 1;
    m_ServerAcceptTimeout.usec = 0;

    unsigned int host = SOCK_gethostbyname(0);
    char ipaddr[32];
    SOCK_ntoa(host, ipaddr, sizeof(ipaddr)-1);
    m_HostIP.assign(ipaddr);

    x_ReadServerParams();

    g_NCStorage = new CNCBlobStorage(do_reinit);
    CNCFileSystem::SetDiskInitialized();

    m_BlobIdCounter.Set(0);
    m_StartTime = GetFastTime();

    _ASSERT(g_NetcacheServer == NULL);
    g_NetcacheServer = this;
}

void
CNetCacheServer::Init(void)
{
    INCBlockedOpListener::BindToThreadPool(GetThreadPool());
}

CNetCacheServer::~CNetCacheServer()
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintLog);
    INFO_POST("NetCache server is destroying. Usage statistics:");
    x_PrintServerStats(proxy);

    delete g_NCStorage;
    g_NetcacheServer = NULL;
}

const SNCSpecificParams*
CNetCacheServer::GetAppSetup(const TStringMap& client_params)
{
    const SSpecParamsSet* cur_set = m_SpecParams;
    ITERATE(TSpecKeysList, key_it, m_SpecPriority) {
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

void
CNetCacheServer::Reconfigure(void)
{
    CNcbiApplication::Instance()->ReloadConfig();
    x_ReadServerParams();
    g_NCStorage->Reconfigure();
}

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
    g_NCStorage->PrintStat(proxy);
    proxy << endl;
    CNCStat::Print(proxy);
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
#ifdef NCBI_OS_UNIX
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

    if (args["version-full"]) {
        printf(NETCACHED_FULL_VERSION "\n");
        return 0;
    }
    INFO_POST(NETCACHED_FULL_VERSION);

#ifdef NCBI_OS_UNIX
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
            NCBI_THROW(CCoreException, eCore, "Error during daemonization");
        }
    }

    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  s_NCSignalHandler);
    signal(SIGTERM, s_NCSignalHandler);
#endif

    CNCMemManager::InitializeApp();
    CSQLITE_Global::Initialize();
    CSQLITE_Global::EnableSharedCache();
    CNCFileSystem ::Initialize();
    try {
        AutoPtr<CNetCacheServer> server(new CNetCacheServer(args["reinit"]));
        server->Run();
        if (server->GetSignalCode()) {
            INFO_POST("Server got " << server->GetSignalCode() << " signal.");
        }
    }
    catch (...) {
        CNCFileSystem ::Finalize();
        CSQLITE_Global::Finalize();
        CNCMemManager ::FinalizeApp();
        throw;
    }
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
