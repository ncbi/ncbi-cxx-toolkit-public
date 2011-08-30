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
#include <connect/services/netcache_api.hpp>

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
#include "mirroring.hpp"



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
static const char* kNCReg_PassPolicy          = "blob_password_policy";
static const char* kNCReg_AppSetupPrefix      = "app_setup_";
static const char* kNCReg_AppSetupValue       = "setup";


CNetCacheServer* g_NetcacheServer = NULL;
CNCBlobStorage*  g_NCStorage      = NULL;
static map<Uint8, CNetCacheAPI> s_NCPeers;
static CFastMutex s_CachingLock;
static CStdPoolOfThreads* s_TaskPool;


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
        params->quorum = reg.GetInt(section, kNCReg_Quorum, 1);
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
    main_params->quorum          = 1;
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
            NON_CONST_REVERSE_ITERATE(TSpecKeysList, prty_it, m_SpecPriority) {
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
                    if (cur_set->entries.size() == 0) {
                        cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, static_cast<SSpecParamsSet*>(prev_set->entries[0].value.GetPointer())->entries[0].value));
                    }
                    prev_set = cur_set;
                    cur_set = static_cast<SSpecParamsSet*>(cur_set->entries[0].value.GetPointer());
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
        s_TaskPool = new CStdPoolOfThreads(serv_params.max_threads, serv_params.max_connections);

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

    CFastMutexGuard guard(s_CachingLock);

    _ASSERT(g_NetcacheServer == NULL);
    g_NetcacheServer = this;

    g_NCStorage = new CNCBlobStorage();
    if (!g_NCStorage->Initialize(do_reinit))
        return false;
    CNCFileSystem::SetDiskInitialized();

    Uint8 max_rec_no = g_NCStorage->GetMaxSyncLogRecNo();
    CNCSyncLog::Initialize(g_NCStorage->IsCleanStart(), max_rec_no);

    typedef map<Uint8, string> TPeerAddrs;
    const TPeerAddrs& peer_addrs = CNCDistributionConf::GetPeers();
    ITERATE(TPeerAddrs, it_peer, peer_addrs) {
        try {
            s_NCPeers[it_peer->first] = CNetCacheAPI(it_peer->second, "nc_peer");
        }
        catch (CNetCacheException& ex) {
            ERR_POST(Critical << "Cannot create NetCacheAPI: " << ex);
            return false;
        }
    }

    m_StartTime = GetFastLocalTime();

    if (!CNCMirroring::Initialize())
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
    CNCMirroring::Finalize();
    if (s_TaskPool)
        s_TaskPool->KillAllThreads(true);
    if (GetThreadPool())
        GetThreadPool()->KillAllThreads(true);
    if (g_NCStorage)
        g_NCStorage->Finalize();
    //delete g_NCStorage;
    //delete s_TaskPool;
    CNCSyncLog::Finalize();
    CNCDistributionConf::Finalize();

    g_NetcacheServer = NULL;
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
          << "Copy queue - " << CNCMirroring::GetQueueSize() << endl
          << "Sync log queue - " << CNCSyncLog::GetLogSize() << endl;
    CNCStat::Print(proxy);
}

int
CNetCacheServer::GetMaxConnections(void)
{
    SServer_Parameters params;
    GetParameters(&params);
    return params.max_connections;
}

CNetServer
CNetCacheServer::GetPeerServer(Uint8 server_id)
{
    if (s_NCPeers.find(server_id) == s_NCPeers.end())
        abort();
    return s_NCPeers[server_id].GetService().Iterate().GetServer();
}

static bool
s_ReadBlobsList(IReader* reader, TNCBlobSumList& blobs_list)
{
    char buf[4096];
    size_t n_read = 0;
    size_t buf_pos = 0;
    for (;;) {
        size_t bytes_read = 0;
        ERW_Result read_res = eRW_Error;
        try {
            read_res = reader->Read(buf + n_read, 4096 - n_read, &bytes_read);
        }
        catch (CNetServiceException& ex) {
            ERR_POST(Warning << "Cannot read from peer: " << ex);
        }
        if (read_res == eRW_Eof) {
            if (n_read == 0)
                return true;
            else
                goto error_blobs;
        }
        else if (read_res == eRW_Error)
            goto error_blobs;

        n_read += bytes_read;
        while (n_read - buf_pos > 2) {
            SNCCacheData* blob_sum = new SNCCacheData();
            Uint2 key_size = *(Uint2*)(buf + buf_pos);
            Uint2 rec_size = key_size + sizeof(key_size)
                             + sizeof(blob_sum->create_time)
                             + sizeof(blob_sum->create_server)
                             + sizeof(blob_sum->create_id)
                             + sizeof(blob_sum->dead_time)
                             + sizeof(blob_sum->expire)
                             + sizeof(blob_sum->ver_expire);
            if (n_read - buf_pos < rec_size)
                break;
            char* data = buf + buf_pos + sizeof(key_size);
            string key(key_size, '\0');
            memcpy(&key[0], data, key_size);
            data += key_size;
            blobs_list[key] = blob_sum;
            memcpy(&blob_sum->create_time, data, sizeof(blob_sum->create_time));
            data += sizeof(blob_sum->create_time);
            memcpy(&blob_sum->create_server, data, sizeof(blob_sum->create_server));
            data += sizeof(blob_sum->create_server);
            memcpy(&blob_sum->create_id, data, sizeof(blob_sum->create_id));
            data += sizeof(blob_sum->create_id);
            memcpy(&blob_sum->dead_time, data, sizeof(blob_sum->dead_time));
            data += sizeof(blob_sum->dead_time);
            memcpy(&blob_sum->expire, data, sizeof(blob_sum->expire));
            data += sizeof(blob_sum->expire);
            memcpy(&blob_sum->ver_expire, data, sizeof(blob_sum->ver_expire));
            buf_pos += rec_size;
        }
        memmove(buf, buf + buf_pos, n_read - buf_pos);
        n_read -= buf_pos;
        buf_pos = 0;
    }

error_blobs:
    ITERATE(TNCBlobSumList, it, blobs_list) {
        delete it->second;
    }
    blobs_list.clear();
    return false;
}

ESyncInitiateResult
CNetCacheServer::StartSyncWithPeer(Uint8                server_id,
                                   Uint2                slot,
                                   Uint8&               local_rec_no,
                                   Uint8&               remote_rec_no,
                                   TReducedSyncEvents&  events_list,
                                   TNCBlobSumList&      blobs_list)
{
    string api_cmd("SYNC_START ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(local_rec_no);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(remote_rec_no);

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    auto_ptr<IReader> reader;
    string response;
    try {
        reader.reset(srv_api.ExecRead(api_cmd, &response));
    }
    catch (CNetCacheException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        return eNetworkError;
    }
    catch (CNetSrvConnException& ex) {
        if (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
            &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle)
        {
            ERR_POST(Warning << "Cannot connect to peer: " << ex);
        }
        return eNetworkError;
    }
    if (NStr::FindNoCase(response, "CROSS_SYNC") != NPOS) {
        return eCrossSynced;
    }
    else if (NStr::FindNoCase(response, "IN_PROGRESS") != NPOS) {
        return eServerBusy;
    }
    else if (NStr::FindNoCase(response, "ALL_BLOBS") == NPOS) {
        list<CTempString> tokens;
        NStr::Split(response, " ", tokens);
        list<CTempString>::const_iterator it_tok = tokens.begin();
        try {
            ++it_tok;
            remote_rec_no = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            local_rec_no = NStr::StringToUInt8(*it_tok);
        }
        catch (CStringException& ex) {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return eNetworkError;
        }

        char buf[4096];
        size_t n_read = 0;
        size_t buf_pos = 0;
        for (;;) {
            size_t bytes_read = 0;
            ERW_Result read_res = eRW_Error;
            try {
                read_res = reader->Read(buf + n_read, 4096 - n_read, &bytes_read);
            }
            catch (CNetServiceException& ex) {
                ERR_POST(Warning << "Cannot read from peer: " << ex);
            }
            if (read_res == eRW_Eof) {
                if (n_read == 0)
                    return eProceedWithEvents;
                else
                    goto error_events;
            }
            else if (read_res == eRW_Error)
                goto error_events;

            n_read += bytes_read;
            while (n_read - buf_pos > 2) {
                SNCSyncEvent* evt;
                Uint2 key_size = *(Uint2*)(buf + buf_pos);
                Uint2 rec_size = key_size + sizeof(key_size) + 1
                                 + sizeof(evt->rec_no) + sizeof(evt->local_time)
                                 + sizeof(evt->orig_rec_no)
                                 + sizeof(evt->orig_server)
                                 + sizeof(evt->orig_time);
                if (n_read - buf_pos < rec_size)
                    break;
                evt = new SNCSyncEvent;
                char* data = buf + buf_pos + sizeof(key_size);
                evt->key.resize(key_size);
                memcpy(&evt->key[0], data, key_size);
                data += key_size;
                evt->event_type = ENCSyncEvent(*data);
                ++data;
                memcpy(&evt->rec_no, data, sizeof(evt->rec_no));
                data += sizeof(evt->rec_no);
                memcpy(&evt->local_time, data, sizeof(evt->local_time));
                data += sizeof(evt->local_time);
                memcpy(&evt->orig_rec_no, data, sizeof(evt->orig_rec_no));
                data += sizeof(evt->orig_rec_no);
                memcpy(&evt->orig_server, data, sizeof(evt->orig_server));
                data += sizeof(evt->orig_server);
                memcpy(&evt->orig_time, data, sizeof(evt->orig_time));
                buf_pos += rec_size;

                if (evt->event_type == eSyncProlong)
                    events_list[evt->key].prolong_event = evt;
                else
                    events_list[evt->key].wr_or_rm_event = evt;
            }
            memmove(buf, buf + buf_pos, n_read - buf_pos);
            n_read -= buf_pos;
            buf_pos = 0;
        }
    }
    else {
        CTempString str1, str2;
        NStr::SplitInTwo(response, " ", str1, str2);
        try {
            remote_rec_no = NStr::StringToUInt8(str2);
        }
        catch (CStringException& ex) {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return eNetworkError;
        }
        if (s_ReadBlobsList(reader.get(), blobs_list))
            return eProceedWithBlobs;
        else
            return eNetworkError;
    }

error_events:
    ITERATE(TReducedSyncEvents, it, events_list) {
        delete it->second.wr_or_rm_event;
        delete it->second.prolong_event;
    }
    events_list.clear();
    return eNetworkError;
}

ENCPeerFailure
CNetCacheServer::GetBlobsListFromPeer(Uint8           server_id,
                                      Uint2           slot,
                                      TNCBlobSumList& blobs_list,
                                      Uint8&          remote_rec_no)
{
    string api_cmd("SYNC_BLIST ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    auto_ptr<IReader> reader;
    string response;
    try {
        reader.reset(srv_api.ExecRead(api_cmd, &response));
    }
    catch (CNetCacheException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        return ePeerBadNetwork;
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
        return ePeerBadNetwork;
    }
    if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS)
        return ePeerNeedAbort;

    CTempString str1, str2;
    NStr::SplitInTwo(response, " ", str1, str2);
    remote_rec_no = NStr::StringToUInt8(str2);
    if (s_ReadBlobsList(reader.get(), blobs_list))
        return ePeerActionOK;
    else
        return ePeerBadNetwork;
}

ENCPeerFailure
CNetCacheServer::x_WriteBlobToPeer(Uint8 server_id,
                                   Uint2 slot,
                                   const string & raw_key,
                                   Uint8 orig_rec_no,
                                   bool  is_sync,
                                   bool  add_client_ip)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    ENCPeerFailure result = ePeerBadNetwork;
    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    auto_ptr<IEmbeddedStreamWriter> writer;
    string response;
    string api_cmd;

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCReadData, raw_key, "", slot);
    while (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    if (accessor->HasError())
        goto cleanup_and_exit;
    if (!accessor->IsBlobExists()) {
        result = ePeerActionOK;
        goto cleanup_and_exit;
    }

    if (is_sync) {
        api_cmd += "SYNC_PUT ";
        api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
        api_cmd.append(1, ' ');
        api_cmd += NStr::UIntToString(slot);
    }
    else {
        api_cmd += "COPY_PUT";
    }
    api_cmd += " \"";
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\" ";
    api_cmd += NStr::IntToString(accessor->GetCurBlobVersion());
    api_cmd += " \"";
    api_cmd += accessor->GetCurPassword();
    api_cmd += "\" ";
    api_cmd += NStr::UInt8ToString(accessor->GetCurBlobCreateTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(Uint4(accessor->GetCurBlobTTL()));
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(accessor->GetCurBlobDeadTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(accessor->GetCurBlobExpire());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCurBlobSize());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(Uint4(accessor->GetCurVersionTTL()));
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(accessor->GetCurVerExpire());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCurCreateServer());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCurCreateId());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(orig_rec_no);
    if (add_client_ip) {
        api_cmd += " \"";
        api_cmd += GetDiagContext().GetRequestContext().GetClientIP();
        api_cmd += "\" \"";
        api_cmd += GetDiagContext().GetRequestContext().GetSessionID();
        api_cmd.append(1, '"');
    }

    try {
        writer.reset(srv_api.ExecWrite(api_cmd, &response));
    }
    catch (CNetCacheException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        goto cleanup_and_exit;
    }
    catch (CNetSrvConnException& ex) {
        if (IsDebugMode()
            ||  (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
                 &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle))
        {
            ERR_POST(Warning << "Cannot connect to peer: " << ex);
        }
        goto cleanup_and_exit;
    }
    if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS) {
        result = ePeerNeedAbort;
    }
    else if (NStr::FindNoCase(response, "HAVE_NEWER") == NPOS) {
        accessor->SetPosition(0);
        while (accessor->ObtainFirstData(op_listener) == eNCWouldBlock)
            sem.Wait();
        if (accessor->HasError())
            goto cleanup_and_exit;

        char buf[65536];
        Uint8 total_read = 0;
        for (;;) {
            size_t n_read = accessor->ReadData(buf, 65536);
            if (accessor->HasError())
                goto cleanup_and_exit;
            if (n_read == 0)
                break;
            total_read += n_read;
            size_t buf_pos = 0;
            size_t bytes_written;
            ERW_Result write_res = eRW_Success;
            while (write_res == eRW_Success  &&  buf_pos < n_read) {
                bytes_written = 0;
                try {
                    write_res = writer->Write(buf + buf_pos, n_read - buf_pos, &bytes_written);
                }
                catch (CNetServiceException& ex) {
                    ERR_POST(Warning << "Cannot write to peer: " << ex);
                    goto cleanup_and_exit;
                }
                buf_pos += bytes_written;
            }
            if (write_res != eRW_Success)
                goto cleanup_and_exit;
        }
        try {
            writer->Close();
        }
        catch (CNetServiceException& ex) {
            ERR_POST(Warning << "Cannot write to peer: " << ex);
            goto cleanup_and_exit;
        }
        catch (CNetSrvConnException& ex) {
            ERR_POST(Warning << "Cannot write to peer: " << ex);
            goto cleanup_and_exit;
        }
        CNCDistributionConf::PrintBlobCopyStat(accessor->GetCurBlobCreateTime(),
                                               accessor->GetCurCreateServer(),
                                               server_id);
        result = ePeerActionOK;
    }
    else {
        result = ePeerActionOK;
    }

cleanup_and_exit:
    accessor->Release();
    delete op_listener;
    return result;
}

ENCPeerFailure
CNetCacheServer::SendBlobToPeer(Uint8 server_id,
                                Uint2 slot,
                                const string& key,
                                Uint8 orig_rec_no,
                                bool  add_client_ip)
{
    return x_WriteBlobToPeer(server_id, slot,
                             key, orig_rec_no, false, add_client_ip);
}

ENCPeerFailure
CNetCacheServer::x_ProlongBlobOnPeer(Uint8 server_id,
                                     Uint2 slot,
                                     const string& raw_key,
                                     const SNCBlobSummary& blob_sum,
                                     Uint8 orig_server,
                                     Uint8 orig_rec_no,
                                     Uint8 orig_time,
                                     bool  is_sync)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    string api_cmd;
    if (is_sync) {
        api_cmd += "SYNC_PROLONG ";
        api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
        api_cmd.append(1, ' ');
        api_cmd += NStr::UIntToString(slot);
    }
    else {
        api_cmd += "COPY_PROLONG";
    }
    api_cmd += " \"";
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\" ";
    api_cmd += NStr::UInt8ToString(blob_sum.create_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(blob_sum.create_server);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(blob_sum.create_id);
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(blob_sum.dead_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(blob_sum.expire);
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(blob_sum.ver_expire);
    if (orig_rec_no != 0) {
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(orig_time);
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(orig_server);
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(orig_rec_no);
    }

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        CNetServer::SExecResult exec_res = srv_api.ExecWithRetry(api_cmd);
        if (NStr::FindNoCase(exec_res.response, "NEED_ABORT") != NPOS)
            return ePeerNeedAbort;
        else
            return ePeerActionOK;
    }
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound)
            return x_WriteBlobToPeer(server_id, slot, raw_key, 0, is_sync, false);
        else {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return ePeerBadNetwork;
        }
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
        return ePeerBadNetwork;
    }
}

ENCPeerFailure
CNetCacheServer::x_ProlongBlobOnPeer(Uint8 server_id,
                                     Uint2 slot,
                                     const string& raw_key,
                                     Uint8 orig_server,
                                     Uint8 orig_rec_no,
                                     Uint8 orig_time,
                                     bool is_sync)
{
    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCRead, raw_key, "", slot);
    while (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    delete op_listener;
    if (accessor->HasError()) {
        accessor->Release();
        return ePeerBadNetwork;
    }
    if (!accessor->IsBlobExists()  ||  accessor->IsCurBlobExpired()) {
        accessor->Release();
        return ePeerActionOK;
    }

    SNCBlobSummary blob_sum;
    blob_sum.create_time = accessor->GetCurBlobCreateTime();
    blob_sum.create_server = accessor->GetCurCreateServer();
    blob_sum.create_id = accessor->GetCurCreateId();
    blob_sum.dead_time = accessor->GetCurBlobDeadTime();
    blob_sum.expire = accessor->GetCurBlobExpire();
    blob_sum.ver_expire = accessor->GetCurVerExpire();
    accessor->Release();

    return x_ProlongBlobOnPeer(server_id, slot, raw_key, blob_sum,
                               orig_server, orig_rec_no, orig_time, is_sync);
}

ENCPeerFailure
CNetCacheServer::ProlongBlobOnPeer(Uint8 server_id,
                                   Uint2 slot,
                                   const string& key,
                                   Uint8 orig_rec_no,
                                   Uint8 orig_time)
{
    return x_ProlongBlobOnPeer(server_id, slot, key,
                               CNCDistributionConf::GetSelfID(),
                               orig_rec_no, orig_time, false);
}

ENCPeerFailure
CNetCacheServer::x_SyncGetBlobFromPeer(Uint8 server_id,
                                       Uint2 slot,
                                       const string& raw_key,
                                       Uint8 create_time,
                                       Uint8 orig_rec_no)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCCopyCreate, raw_key, "", slot);
    while (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    delete op_listener;
    if (accessor->HasError()) {
        accessor->Release();
        return ePeerBadNetwork;
    }
    if (accessor->IsBlobExists()
        &&  accessor->GetCurBlobCreateTime() > create_time)
    {
        accessor->Release();
        return ePeerActionOK;
    }

    string api_cmd("SYNC_GET ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);
    api_cmd += " \"";
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\" ";
    api_cmd += NStr::UInt8ToString(create_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCurBlobCreateTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCurCreateServer());
    api_cmd.append(1, ' ');
    api_cmd += NStr::Int8ToString(accessor->GetCurCreateId());

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    auto_ptr<IReader> reader;
    string response;
    try {
        reader.reset(srv_api.ExecRead(api_cmd, &response));
    }
    catch (CNetCacheException& ex) {
        accessor->Release();
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound)
            return ePeerActionOK;
        else {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return ePeerBadNetwork;
        }
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
        accessor->Release();
        return ePeerBadNetwork;
    }
    if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS) {
        accessor->Release();
        return ePeerNeedAbort;
    }
    else if (NStr::FindNoCase(response, "HAVE_NEWER") != NPOS)
    {
        accessor->Release();
        return ePeerActionOK;
    }

    list<CTempString> params;
    NStr::Split(response, " ", params);
    if (params.size() < 11) {
        accessor->Release();
        return ePeerBadNetwork;
    }
    list<CTempString>::const_iterator param_it = params.begin();
    Uint8 create_server;
    try {
        ++param_it;
        accessor->SetBlobVersion(NStr::StringToInt(*param_it));
        ++param_it;
        accessor->SetPassword(param_it->substr(1, param_it->size() - 2));
        ++param_it;
        create_time = NStr::StringToUInt8(*param_it);
        accessor->SetBlobCreateTime(create_time);
        ++param_it;
        accessor->SetBlobTTL(int(NStr::StringToUInt(*param_it)));
        ++param_it;
        int dead_time = NStr::StringToInt(*param_it);
        ++param_it;
        int expire = NStr::StringToInt(*param_it);
        accessor->SetNewBlobExpire(expire, dead_time);
        ++param_it;
        accessor->SetVersionTTL(int(NStr::StringToUInt(*param_it)));
        ++param_it;
        accessor->SetNewVerExpire(NStr::StringToInt(*param_it));
        ++param_it;
        create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        TNCBlobId create_id = NStr::StringToUInt(*param_it);
        accessor->SetCreateServer(create_server, create_id, slot);
    }
    catch (CStringException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        accessor->Release();
        return ePeerBadNetwork;
    }

    char buf[65536];
    for (;;) {
        size_t bytes_read;
        ERW_Result read_res = eRW_Error;
        try {
            read_res = reader->Read(buf, 65536, &bytes_read);
        }
        catch (CNetServiceException& ex) {
            ERR_POST(Warning << "Cannot read from peer: " << ex);
        }
        if (read_res == eRW_Error) {
            accessor->Release();
            return ePeerBadNetwork;
        }
        accessor->WriteData(buf, bytes_read);
        if (accessor->HasError()) {
            accessor->Release();
            return ePeerBadNetwork;
        }
        if (read_res == eRW_Eof)
            break;
    }
    accessor->Finalize();
    if (accessor->HasError()) {
        accessor->Release();
        return ePeerBadNetwork;
    }
    if (orig_rec_no != 0) {
        SNCSyncEvent* event = new SNCSyncEvent();
        event->event_type = eSyncWrite;
        event->key = raw_key;
        event->orig_server = create_server;
        event->orig_time = create_time;
        event->orig_rec_no = orig_rec_no;
        CNCSyncLog::AddEvent(slot, event);
    }
    accessor->Release();
    CNCDistributionConf::PrintBlobCopyStat(create_time, create_server, CNCDistributionConf::GetSelfID());
    return ePeerActionOK;
}

ENCPeerFailure
CNetCacheServer::ReadBlobMetaData(Uint8 server_id,
                                  const string& raw_key,
                                  bool& blob_exist,
                                  SNCBlobSummary& blob_sum)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    string api_cmd("PROXY_META \"");
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\" \"";
    api_cmd += GetDiagContext().GetRequestContext().GetClientIP();
    api_cmd += "\" \"";
    api_cmd += GetDiagContext().GetRequestContext().GetSessionID();
    api_cmd.append(1, '"');

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    CNetServer::SExecResult exec_res;
    try {
        exec_res = srv_api.ExecWithRetry(api_cmd);
    }
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
            blob_exist = false;
            return ePeerActionOK;
        }
        else {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return ePeerBadNetwork;
        }
    }
    catch (CNetSrvConnException& ex) {
        if (IsDebugMode()
            ||  (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
                 &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle))
        {
            ERR_POST(Warning << "Cannot connect to peer: " << ex);
        }
        return ePeerBadNetwork;
    }
    list<CTempString> params;
    NStr::Split(exec_res.response, " ", params);
    if (params.size() < 7)
        return ePeerBadNetwork;

    list<CTempString>::const_iterator param_it = params.begin();
    try {
        ++param_it;
        blob_sum.create_time = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_id = NStr::StringToUInt(*param_it);
        ++param_it;
        blob_sum.dead_time = NStr::StringToInt(*param_it);
        ++param_it;
        blob_sum.expire = NStr::StringToInt(*param_it);
        ++param_it;
        blob_sum.ver_expire = NStr::StringToInt(*param_it);
    }
    catch (CStringException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        return ePeerBadNetwork;
    }

    blob_exist = true;
    return ePeerActionOK;
}

ENCPeerFailure
CNetCacheServer::SyncProlongOurBlob(Uint8 server_id,
                                    Uint2 slot,
                                    const string& raw_key,
                                    const SNCBlobSummary& blob_sum,
                                    bool* need_event /* = NULL */)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCRead, raw_key, "", slot);
    while (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    delete op_listener;
    if (accessor->HasError()) {
        accessor->Release();
        return ePeerBadNetwork;
    }
    if (!accessor->IsBlobExists()) {
        accessor->Release();
        return x_SyncGetBlobFromPeer(server_id, slot, raw_key, blob_sum.create_time, 0);
    }

    if (accessor->GetCurBlobCreateTime() == blob_sum.create_time
        &&  accessor->GetCurCreateServer() == blob_sum.create_server
        &&  accessor->GetCurCreateId() == blob_sum.create_id)
    {
        if (need_event)
            *need_event = false;
        if (accessor->GetCurBlobExpire() < blob_sum.expire) {
            accessor->SetCurBlobExpire(blob_sum.expire, blob_sum.dead_time);
            if (need_event)
                *need_event = true;
        }
        if (accessor->GetCurVerExpire() < blob_sum.ver_expire) {
            accessor->SetCurVerExpire(blob_sum.ver_expire);
            if (need_event)
                *need_event = true;
        }
    }
    accessor->Release();
    return ePeerActionOK;
}

ENCPeerFailure
CNetCacheServer::SyncProlongOurBlob(Uint8          server_id,
                                    Uint2          slot,
                                    SNCSyncEvent*  evt)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(evt->key, cache_name, key, subkey);

    string api_cmd("SYNC_PROINFO ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);
    api_cmd += " \"";
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\"";

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    CNetServer::SExecResult exec_res;
    try {
        exec_res = srv_api.ExecWithRetry(api_cmd);
    }
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
            return ePeerActionOK;
        }
        else {
            ERR_POST(Warning << "Cannot execute command on peer: " << ex);
            return ePeerBadNetwork;
        }
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
        return ePeerBadNetwork;
    }

    if (NStr::FindNoCase(exec_res.response, "NEED_ABORT") != NPOS)
        return ePeerNeedAbort;

    list<CTempString> params;
    NStr::Split(exec_res.response, " ", params);
    if (params.size() < 7)
        return ePeerBadNetwork;
    SNCBlobSummary blob_sum;
    list<CTempString>::const_iterator param_it = params.begin();
    try {
        ++param_it;
        blob_sum.create_time = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_id = NStr::StringToUInt(*param_it);
        ++param_it;
        blob_sum.dead_time = NStr::StringToInt(*param_it);
        ++param_it;
        blob_sum.expire = NStr::StringToInt(*param_it);
        ++param_it;
        blob_sum.ver_expire = NStr::StringToInt(*param_it);
    }
    catch (CStringException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
        return ePeerBadNetwork;
    }

    bool need_event = false;
    ENCPeerFailure op_res = SyncProlongOurBlob(server_id, slot, evt->key, blob_sum, &need_event);
    if (need_event  &&  op_res == ePeerActionOK) {
        SNCSyncEvent* event = new SNCSyncEvent();
        event->event_type = eSyncProlong;
        event->key = evt->key;
        event->orig_server = evt->orig_server;
        event->orig_time = evt->orig_time;
        event->orig_rec_no = evt->orig_rec_no;
        CNCSyncLog::AddEvent(slot, event);
    }
    return op_res;
}

bool
CNetCacheServer::SyncCommitOnPeer(Uint8 server_id,
                                  Uint2 slot,
                                  Uint8 local_rec_no,
                                  Uint8 remote_rec_no)
{
    string api_cmd("SYNC_COMMIT ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(local_rec_no);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(remote_rec_no);

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        srv_api.ExecWithRetry(api_cmd);
        return true;
    }
    catch (CNetCacheException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
    }
    return false;
}

void
CNetCacheServer::SyncCancelOnPeer(Uint8 server_id,
                                  Uint2 slot)
{
    string api_cmd("SYNC_CANCEL ");
    api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(slot);

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        srv_api.ExecWithRetry(api_cmd);
    }
    catch (CNetCacheException& ex) {
        ERR_POST(Warning << "Cannot execute command on peer: " << ex);
    }
    catch (CNetSrvConnException& ex) {
        ERR_POST(Warning << "Cannot connect to peer: " << ex);
    }
}

bool
CNetCacheServer::IsInitiallySynced(void)
{
    return g_NetcacheServer  &&  g_NetcacheServer->m_InitiallySynced;
}

void
CNetCacheServer::InitialSyncComplete(void)
{
    g_NetcacheServer->m_InitiallySynced = true;
}

void
CNetCacheServer::UpdateLastRecNo(void)
{
    Uint8 last_rec_no = CNCSyncLog::GetLastRecNo();
    g_NCStorage->SetMaxSyncLogRecNo(last_rec_no);
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

bool
CNetCacheServer::AddDeferredTask(CStdRequest* task)
{
    try {
        s_TaskPool->AcceptRequest(CRef<CStdRequest>(task));
        return true;
    }
    catch (CBlockingQueueException& ex) {
        ERR_POST(Critical << "Queue for deferred tasks is full: " << ex);
        return false;
    }
}


CNCSyncBlockedOpListener::CNCSyncBlockedOpListener(CSemaphore& sem)
    : m_Sem(sem)
{}

CNCSyncBlockedOpListener::~CNCSyncBlockedOpListener(void)
{}

void
CNCSyncBlockedOpListener::OnBlockedOpFinish(void)
{
    m_Sem.Post();
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

    if (args["version-full"]) {
        printf(NETCACHED_FULL_VERSION "\n");
        return 0;
    }
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
    if (!CNCMemManager::InitializeApp())
        goto fin_mem;
    CSQLITE_Global::Initialize();
    if (!CNCFileSystem::Initialize())
        goto fin_fs;
    server = new CNetCacheServer();
    if (server->Initialize(args["reinit"])) {
        server->Run();
        if (server->GetSignalCode()) {
            INFO_POST("Server got " << server->GetSignalCode() << " signal.");
        }
    }
    server->Finalize();
    //delete server;
fin_fs:
    CNCFileSystem::Finalize();
    CSQLITE_Global::Finalize();
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
