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

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

#include "message_handler.hpp"
#include "netcached.hpp"
#include "netcache_version.hpp"
#include "nc_memory.hpp"
#include "nc_stat.hpp"
#include "error_codes.hpp"
#include "distribution_conf.hpp"
#include "sync_log.hpp"
#include "mirroring.hpp"



BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Main


static const char* kNCReg_ServerSection       = "server";
static const char* kNCReg_Ports               = "ports";
static const char* kNCReg_CtrlPort            = "control_port";
static const char* kNCReg_MaxConnections      = "max_connections";
static const char* kNCReg_MaxThreads          = "max_threads";
static const char* kNCReg_LogCmds             = "log_requests";
static const char* kNCReg_AdminClient         = "admin_client_name";
static const char* kNCReg_DefAdminClient      = "netcache_control";
static const char* kNCReg_MemLimit            = "db_cache_limit";
//static const char* kNCReg_MemAlert            = "memory_alert";
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
static CThreadPool* s_TaskPool;


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
            ERR_POST("INI file sets network timeout to 0. Assuming 10 seconds.");
            params->conn_timeout =  10;
        }
    }
    if (reg.HasEntry(section, kNCReg_BlobTTL, IRegistry::fCountCleared)) {
        params->blob_ttl = reg.GetInt(section, kNCReg_BlobTTL,
                                      3600, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_VerTTL, IRegistry::fCountCleared)) {
        params->ver_ttl = reg.GetInt(section, kNCReg_VerTTL,
                                     3600, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_TTLUnit, IRegistry::fCountCleared)) {
        params->ttl_unit = reg.GetInt(section, kNCReg_TTLUnit,
                                      300, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_ProlongOnRead, IRegistry::fCountCleared)) {
        params->prolong_on_read = reg.GetBool(section, kNCReg_ProlongOnRead,
                                              true, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_SearchOnRead, IRegistry::fCountCleared)) {
        params->srch_on_read = reg.GetBool(section, kNCReg_SearchOnRead,
                                           true, 0, IRegistry::eErrPost);
    }
    if (reg.HasEntry(section, kNCReg_Quorum, IRegistry::fCountCleared)) {
        params->quorum = reg.GetInt(section, kNCReg_Quorum,
                                    1, 0, IRegistry::eErrPost);
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
            if (m_Ports.find(port) == m_Ports.end())
                m_Ports.insert(port);
        }
        if (ports_list.size() != m_Ports.size()) {
            ERR_POST_X(18, "After reconfiguration some ports should not be "
                           "listened anymore. It requires restarting of the server.");
        }
        if (m_Ports.size() == 0) {
            NCBI_THROW(CUtilException, eWrongData,
                       "No listening ports were configured.");
        }
        m_PortsConfStr = ports_str;
    }
    m_CtrlPort = reg.GetInt(kNCReg_ServerSection, kNCReg_CtrlPort, 0, 0,
                            IRegistry::eErrPost);

    m_LogCmds     = reg.GetBool  (kNCReg_ServerSection, kNCReg_LogCmds, true, 0,
                                  IRegistry::eErrPost);
    m_AdminClient = reg.GetString(kNCReg_ServerSection, kNCReg_AdminClient,
                                  kNCReg_DefAdminClient);
    bool force_poll = reg.GetBool(kNCReg_ServerSection, kNCReg_ForceUsePoll,
                                  true, 0, IRegistry::eErrPost);
    SOCK_SetIOWaitSysAPI(force_poll? eSOCK_IOWaitSysAPIPoll: eSOCK_IOWaitSysAPIAuto);

    SServer_Parameters serv_params;
    serv_params.max_connections = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxConnections,
                                             100, 0, IRegistry::eErrPost);
    serv_params.max_threads     = reg.GetInt(kNCReg_ServerSection, kNCReg_MaxThreads,
                                             20,  0, IRegistry::eErrPost);
    if (serv_params.max_threads < 1) {
        serv_params.max_threads = 1;
    }
    serv_params.init_threads = 1;
    serv_params.accept_timeout = &m_ServerAcceptTimeout;
    SetParameters(serv_params);
    s_TaskPool = new CThreadPool(1000000000, serv_params.max_threads, 1);

    m_DebugMode = reg.GetBool(kNCReg_ServerSection, "debug_mode", false, 0, IRegistry::eErrPost);

    try {
        string str_val   = reg.GetString(kNCReg_ServerSection, kNCReg_MemLimit, "1Gb");
        size_t mem_limit = size_t(NStr::StringToUInt8_DataSize(str_val));
        /*str_val          = reg.GetString(kNCReg_ServerSection, kNCReg_MemAlert, "4Gb");
        size_t mem_alert = size_t(NStr::StringToUInt8_DataSize(str_val));*/
        CNCMemManager::SetLimits(mem_limit, mem_limit);
    }
    catch (CStringException& ex) {
        /*ERR_POST_X(14, "Error in " << kNCReg_MemLimit
                         << " or " << kNCReg_MemAlert << " parameter: " << ex);*/
        ERR_POST("Error in " << kNCReg_MemLimit << " parameter: " << ex);
    }

    m_OldSpecParams = m_SpecParams;

    string spec_prty = reg.Get(kNCReg_ServerSection, kNCReg_SpecPriority);
    NStr::Tokenize(spec_prty, ", \t\r\n", m_SpecPriority, NStr::eMergeDelims);

    SNCSpecificParams* main_params = new SNCSpecificParams;
    main_params->disable         = false;
    main_params->cmd_timeout     = 600;
    main_params->conn_timeout    = 10;
    main_params->blob_ttl        = 3600;
    main_params->ver_ttl         = 3600;
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
      m_Signal(0),
      m_OpenToClients(false)
{
    m_ServerAcceptTimeout.sec = 1;
    m_ServerAcceptTimeout.usec = 0;

    x_ReadServerParams();
    INCBlockedOpListener::BindToThreadPool(s_TaskPool);

    CNCDistributionConf::Initialize(m_CtrlPort);

    CFastMutexGuard guard(s_CachingLock);

    g_NCStorage = new CNCBlobStorage(do_reinit);
    CNCFileSystem::SetDiskInitialized();

    Uint8 max_rec_no = g_NCStorage->GetMaxSyncLogRecNo();
    CNCSyncLog::Initialize(g_NCStorage->IsCleanStart(), max_rec_no);

    typedef map<Uint8, string> TPeerAddrs;
    const TPeerAddrs& peer_addrs = CNCDistributionConf::GetPeers();
    ITERATE(TPeerAddrs, it_peer, peer_addrs) {
        s_NCPeers[it_peer->first] = CNetCacheAPI(it_peer->second, "nc_peer");
    }

    m_StartTime = GetFastLocalTime();

    _ASSERT(g_NetcacheServer == NULL);
    g_NetcacheServer = this;

    CNCMirroring::Initialize();
    CNCPeriodicSync::PreInitialize();

    if (m_Ports.find(m_CtrlPort) == m_Ports.end()) {
        Uint4 port = m_CtrlPort;
        if (m_DebugMode)
            port += 10;
        LOG_POST("Opening control port " << port);
        AddListener(new CNCMsgHndlFactory_Proxy(), port);
        StartListening();
    }
}

CNetCacheServer::~CNetCacheServer()
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintLog);
    INFO_POST("NetCache server is destroying. Usage statistics:");
    x_PrintServerStats(proxy);

    delete s_TaskPool;
    UpdateLastRecNo();
    CNCPeriodicSync::Finalize();
    CNCMirroring::Finalize();
    CNCSyncLog::Finalize();
    CNCDistributionConf::Finalize();

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

Uint8
g_SubtractTime(Uint8 lhs, Uint8 rhs)
{
    if (Uint4(lhs) >= Uint4(rhs))
        return lhs - rhs;

    lhs -= Uint8(1) << 32;
    lhs += 1000000;
    return lhs - rhs;
}

Uint8
g_AddTime(Uint8 lhs, Uint8 rhs)
{
    Uint8 result = lhs + rhs;
    if (Uint4(result) >= 1000000) {
        result -= 1000000;
        result += Uint8(1) << 32;
    }
    return result;
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
    try {
        char buf[4096];
        size_t n_read = 0;
        size_t buf_pos = 0;
        for (;;) {
            size_t bytes_read = 0;
            ERW_Result read_res = reader->Read(buf + n_read, 4096 - n_read, &bytes_read);
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
                memcpy(&blob_sum->ver_expire, data, sizeof(blob_sum->ver_expire));
                buf_pos += rec_size;
            }
            memmove(buf, buf + buf_pos, n_read - buf_pos);
            n_read -= buf_pos;
            buf_pos = 0;
        }
    }
    catch (CException& ex) {
        ERR_POST(ex);
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
    try {
        string response;
        auto_ptr<IReader> reader(srv_api.ExecRead(api_cmd, &response));
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
            ++it_tok;
            remote_rec_no = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            local_rec_no = NStr::StringToUInt8(*it_tok);

            char buf[4096];
            size_t n_read = 0;
            size_t buf_pos = 0;
            SBlobEvent* last_blob_evt = NULL;
            for (;;) {
                size_t bytes_read = 0;
                ERW_Result read_res = reader->Read(buf + n_read, 4096 - n_read, &bytes_read);
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

                    if (last_blob_evt  &&  evt->event_type == eSyncProlong) {
                        last_blob_evt->prolong_event = evt;
                        last_blob_evt = NULL;
                    }
                    else {
                        last_blob_evt = &events_list[evt->key];
                        last_blob_evt->wr_or_rm_event = evt;
                    }
                }
                memmove(buf, buf + buf_pos, n_read - buf_pos);
                n_read -= buf_pos;
                buf_pos = 0;
            }
        }
        else {
            CTempString str1, str2;
            NStr::SplitInTwo(response, " ", str1, str2);
            remote_rec_no = NStr::StringToUInt8(str2);
            if (s_ReadBlobsList(reader.get(), blobs_list))
                return eProceedWithBlobs;
            else
                return eNetworkError;
        }
    }
    catch (CNetSrvConnException& ex) {
        if (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
            &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle)
        {
            ERR_POST(ex);
        }
    }
    catch (CException& ex) {
        ERR_POST(ex);
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
    try {
        string response;
        auto_ptr<IReader> reader(srv_api.ExecRead(api_cmd, &response));
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
    catch (CException& ex) {
        ERR_POST(ex);
        return ePeerBadNetwork;
    }
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

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCReadData, raw_key, "", slot);
    while (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    if (!accessor->IsBlobExists()  ||  accessor->IsBlobExpired()) {
        accessor->Release();
        return ePeerActionOK;
    }

    string api_cmd;
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
    api_cmd += NStr::IntToString(accessor->GetBlobVersion());
    api_cmd += " \"";
    api_cmd += accessor->GetPassword();
    api_cmd += "\" ";
    api_cmd += NStr::UInt8ToString(accessor->GetBlobCreateTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(Uint4(accessor->GetBlobTTL()));
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(accessor->GetBlobDeadTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetBlobSize());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UIntToString(Uint4(accessor->GetVersionTTL()));
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(accessor->GetVerDeadTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCreateServer());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCreateId());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(orig_rec_no);
    if (add_client_ip) {
        api_cmd += " \"";
        api_cmd += GetDiagContext().GetRequestContext().GetClientIP();
        api_cmd += "\" \"";
        api_cmd += GetDiagContext().GetRequestContext().GetSessionID();
        api_cmd.append(1, '"');
    }

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        string response;
        ENCPeerFailure result = ePeerActionOK;
        auto_ptr<IEmbeddedStreamWriter> writer(srv_api.ExecWrite(api_cmd, &response));
        if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS) {
            result = ePeerNeedAbort;
            writer->Flush();
            writer->Close();
        }
        else if (NStr::FindNoCase(response, "HAVE_NEWER") == NPOS) {
            accessor->SetPosition(0);
            while (accessor->ObtainFirstData(op_listener) == eNCWouldBlock)
                sem.Wait();

            char buf[65536];
            Uint8 total_read = 0;
            for (;;) {
                size_t n_read = accessor->ReadData(buf, 65536);
                if (n_read == 0)
                    break;
                total_read += n_read;
                size_t buf_pos = 0;
                size_t bytes_written;
                ERW_Result write_res = eRW_Success;
                while (write_res == eRW_Success  &&  buf_pos < n_read) {
                    bytes_written = 0;
                    write_res = writer->Write(buf + buf_pos, n_read - buf_pos, &bytes_written);
                    buf_pos += bytes_written;
                }
                if (write_res != eRW_Success) {
                    //ERR_POST(Critical << "Cannot write blob to peer");
                    writer->Close();
                    accessor->Release();
                    return ePeerBadNetwork;
                }
            }
            if (total_read != accessor->GetBlobSize())
                abort();
            writer->Flush();
            writer->Close();
            CNCDistributionConf::PrintBlobCopyStat(accessor->GetBlobCreateTime(),
                                                   accessor->GetCreateServer(),
                                                   server_id);
        }
        else {
            writer->Flush();
            writer->Close();
        }
        accessor->Release();
        return result;
    }
    catch (CNetSrvConnException& ex) {
        if (IsDebugMode()
            ||  (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
                 &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle))
        {
            ERR_POST(ex);
        }
        accessor->Release();
        return ePeerBadNetwork;
    }
    catch (CException& ex) {
        ERR_POST(ex);
        accessor->Release();
        return ePeerBadNetwork;
    }
}

ENCPeerFailure
CNetCacheServer::SendBlobToPeer(Uint8 server_id,
                                const string& key,
                                Uint8 orig_rec_no,
                                bool  add_client_ip)
{
    return x_WriteBlobToPeer(server_id, CNCDistributionConf::GetSlotByKey(key),
                             key, orig_rec_no, false, add_client_ip);
}

ENCPeerFailure
CNetCacheServer::x_DelBlobFromPeer(Uint8 server_id,
                                   Uint2 slot,
                                   SNCSyncEvent* evt,
                                   bool  is_sync,
                                   bool  add_client_ip)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(evt->key, cache_name, key, subkey);

    string api_cmd;
    if (is_sync) {
        api_cmd += "SYNC_REMOVE ";
        api_cmd += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
        api_cmd.append(1, ' ');
        api_cmd += NStr::UIntToString(slot);
    }
    else {
        api_cmd += "COPY_REMOVE";
    }
    api_cmd += " \"";
    api_cmd += cache_name;
    api_cmd += "\" \"";
    api_cmd += key;
    api_cmd += "\" \"";
    api_cmd += subkey;
    api_cmd += "\" ";
    api_cmd += NStr::UInt8ToString(evt->orig_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(evt->orig_server);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(evt->orig_rec_no);
    if (add_client_ip) {
        api_cmd += " \"";
        api_cmd += GetDiagContext().GetRequestContext().GetClientIP();
        api_cmd += "\" \"";
        api_cmd += GetDiagContext().GetRequestContext().GetSessionID();
        api_cmd.append(1, '"');
    }

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        string response;
        IReader* reader = srv_api.ExecRead(api_cmd, &response);
        delete reader;
        if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS)
            return ePeerNeedAbort;
        else
            return ePeerActionOK;
    }
    catch (CException& ex) {
        ERR_POST(ex);
        return ePeerBadNetwork;
    }
}

ENCPeerFailure
CNetCacheServer::SyncProlongBlobOnPeer(Uint8 server_id,
                                       Uint2 slot,
                                       const string& raw_key,
                                       const SNCBlobSummary& blob_sum,
                                       SNCSyncEvent* evt /* = NULL */)
{
    int cur_time = int(time(NULL));
    if (!IsDebugMode())
        cur_time += 30;
    if (blob_sum.dead_time <= cur_time)
        return ePeerActionOK;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    string api_cmd("SYNC_PROLONG ");
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
    api_cmd += NStr::UInt8ToString(blob_sum.create_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(blob_sum.create_server);
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(blob_sum.create_id);
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(blob_sum.dead_time);
    api_cmd.append(1, ' ');
    api_cmd += NStr::IntToString(blob_sum.ver_expire);
    if (evt) {
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(evt->orig_time);
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(evt->orig_server);
        api_cmd.append(1, ' ');
        api_cmd += NStr::UInt8ToString(evt->orig_rec_no);
    }

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        string response;
        IReader* reader = srv_api.ExecRead(api_cmd, &response);
        delete reader;
        if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS)
            return ePeerNeedAbort;
        else
            return ePeerActionOK;
    }
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound)
            return x_WriteBlobToPeer(server_id, slot, raw_key, 0,
                                     true, false);
        else {
            ERR_POST(ex);
            return ePeerBadNetwork;
        }
    }
    catch (CException& ex) {
        ERR_POST(ex);
        return ePeerBadNetwork;
    }
}

ENCPeerFailure
CNetCacheServer::SyncProlongBlobOnPeer(Uint8         server_id,
                                       Uint2         slot,
                                       SNCSyncEvent* evt)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(evt->key, cache_name, key, subkey);

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCRead, evt->key, "", slot);
    if (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    if (!accessor->IsBlobExists()  ||  accessor->IsBlobExpired()) {
        accessor->Release();
        return ePeerActionOK;
    }

    SNCBlobSummary blob_sum;
    blob_sum.create_time = accessor->GetBlobCreateTime();
    blob_sum.create_server = accessor->GetCreateServer();
    blob_sum.create_id = accessor->GetCreateId();
    blob_sum.dead_time = accessor->GetBlobDeadTime();
    blob_sum.ver_expire = accessor->GetVerDeadTime();
    accessor->Release();

    return SyncProlongBlobOnPeer(server_id, slot, evt->key, blob_sum, evt);
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
    if (accessor->IsBlobExists()
        &&  accessor->GetBlobCreateTime() > create_time)
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
    api_cmd += NStr::UInt8ToString(accessor->GetBlobCreateTime());
    api_cmd.append(1, ' ');
    api_cmd += NStr::UInt8ToString(accessor->GetCreateServer());
    api_cmd.append(1, ' ');
    api_cmd += NStr::Int8ToString(accessor->GetCreateId());

    CNetServer srv_api(CNetCacheServer::GetPeerServer(server_id));
    try {
        string response;
        auto_ptr<IReader> reader(srv_api.ExecRead(api_cmd, &response));
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
        if (params.size() < 10) {
            accessor->Release();
            return ePeerBadNetwork;
        }
        list<CTempString>::const_iterator param_it = params.begin();
        ++param_it;
        accessor->SetBlobVersion(NStr::StringToInt(*param_it));
        ++param_it;
        accessor->SetPassword(param_it->substr(1, param_it->size() - 2));
        ++param_it;
        accessor->SetBlobCreateTime(NStr::StringToUInt8(*param_it));
        ++param_it;
        accessor->SetBlobTTL(int(NStr::StringToUInt(*param_it)));
        ++param_it;
        accessor->SetBlobDeadTime(NStr::StringToInt(*param_it));
        ++param_it;
        accessor->SetVersionTTL(int(NStr::StringToUInt(*param_it)));
        ++param_it;
        accessor->SetVerDeadTime(NStr::StringToInt(*param_it));
        ++param_it;
        Uint8 create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        TNCBlobId create_id = NStr::StringToUInt(*param_it);
        accessor->SetCreateServer(create_server, create_id, slot);

        char buf[65536];
        for (;;) {
            size_t bytes_read;
            ERW_Result read_res = reader->Read(buf, 65536, &bytes_read);
            if (read_res == eRW_Error) {
                accessor->Release();
                return ePeerBadNetwork;
            }
            accessor->WriteData(buf, bytes_read);
            if (read_res == eRW_Eof)
                break;
        }
        accessor->Finalize();
        if (orig_rec_no != 0) {
            SNCSyncEvent* event = new SNCSyncEvent();
            event->event_type = eSyncWrite;
            event->key = raw_key;
            event->orig_server = accessor->GetCreateServer();
            event->orig_time = accessor->GetBlobCreateTime();
            event->orig_rec_no = orig_rec_no;
            CNCSyncLog::AddEvent(slot, event);
        }
        Uint8 create_time = accessor->GetBlobCreateTime();
        accessor->Release();
        CNCDistributionConf::PrintBlobCopyStat(create_time, create_server, CNCDistributionConf::GetSelfID());
        return ePeerActionOK;
    }
    catch (CNetCacheException& ex) {
        accessor->Release();
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound)
            return ePeerActionOK;
        else {
            ERR_POST(ex);
            return ePeerBadNetwork;
        }
    }
    catch (CException& ex) {
        ERR_POST(ex);
        accessor->Release();
        return ePeerBadNetwork;
    }
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
    try {
        string response;
        auto_ptr<IReader> reader(srv_api.ExecRead(api_cmd, &response));
        list<CTempString> params;
        NStr::Split(response, " ", params);
        if (params.size() < 5)
            return ePeerBadNetwork;

        list<CTempString>::const_iterator param_it = params.begin();
        ++param_it;
        blob_sum.create_time = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_id = NStr::StringToUInt(*param_it);
        ++param_it;
        blob_sum.dead_time = NStr::StringToInt(*param_it);

        blob_exist = true;
        return ePeerActionOK;
    }
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
            blob_exist = false;
            return ePeerActionOK;
        }
        else {
            ERR_POST(ex);
            return ePeerBadNetwork;
        }
    }
    catch (CNetSrvConnException& ex) {
        if (IsDebugMode()
            ||  (ex.GetErrCode() != CNetSrvConnException::eConnectionFailure
                 &&  ex.GetErrCode() != CNetSrvConnException::eServerThrottle))
        {
            ERR_POST(ex);
        }
        return ePeerBadNetwork;
    }
    catch (CException& ex) {
        ERR_POST(ex);
        return ePeerBadNetwork;
    }
}

ENCPeerFailure
CNetCacheServer::SyncDelOurBlob(Uint8 server_id, Uint2 slot, SNCSyncEvent* evt)
{
    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(evt->key, cache_name, key, subkey);

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCDelete, evt->key, "", slot);
    if (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    if (accessor->IsBlobExists()
        &&  accessor->GetBlobCreateTime() < evt->orig_time)
    {
        accessor->DeleteBlob();
        SNCSyncEvent* event = new SNCSyncEvent();
        event->event_type = eSyncUserRemove;
        event->key = evt->key;
        event->orig_server = evt->orig_server;
        event->orig_time = evt->orig_time;
        event->orig_rec_no = evt->orig_rec_no;
        CNCSyncLog::AddEvent(slot, event);
    }
    accessor->Release();
    return ePeerActionOK;
}

ENCPeerFailure
CNetCacheServer::SyncProlongOurBlob(Uint8 server_id,
                                    Uint2 slot,
                                    const string& raw_key,
                                    const SNCBlobSummary& blob_sum,
                                    bool* need_event /* = NULL */)
{
    int cur_time = int(time(NULL));
    if (!IsDebugMode())
        cur_time += 30;
    if (blob_sum.dead_time <= cur_time)
        return ePeerActionOK;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    CSemaphore sem(0, 1);
    CNCSyncBlockedOpListener* op_listener = new CNCSyncBlockedOpListener(sem);
    CNCBlobAccessor* accessor = g_NCStorage->GetBlobAccess(
                                             eNCRead, raw_key, "", slot);
    if (accessor->ObtainMetaInfo(op_listener) == eNCWouldBlock)
        sem.Wait();
    if (!accessor->IsBlobExists()) {
        accessor->Release();
        return x_SyncGetBlobFromPeer(server_id, slot, raw_key, blob_sum.create_time, 0);
    }

    if (accessor->GetBlobCreateTime() == blob_sum.create_time
        &&  accessor->GetCreateServer() == blob_sum.create_server
        &&  accessor->GetCreateId() == blob_sum.create_id)
    {
        if (need_event)
            *need_event = false;
        if (accessor->GetBlobDeadTime() < blob_sum.dead_time) {
            accessor->SetBlobDeadTime(blob_sum.dead_time);
            if (need_event)
                *need_event = true;
        }
        if (accessor->GetVerDeadTime() < blob_sum.ver_expire) {
            accessor->SetVerDeadTime(blob_sum.ver_expire);
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
    try {
        string response;
        auto_ptr<IReader> reader(srv_api.ExecRead(api_cmd, &response));
        if (NStr::FindNoCase(response, "NEED_ABORT") != NPOS)
            return ePeerNeedAbort;

        list<CTempString> params;
        NStr::Split(response, " ", params);
        if (params.size() < 6)
            return ePeerBadNetwork;
        SNCBlobSummary blob_sum;
        list<CTempString>::const_iterator param_it = params.begin();
        ++param_it;
        blob_sum.create_time = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_server = NStr::StringToUInt8(*param_it);
        ++param_it;
        blob_sum.create_id = NStr::StringToUInt(*param_it);
        ++param_it;
        blob_sum.dead_time = NStr::StringToInt(*param_it);
        ++param_it;
        blob_sum.ver_expire = NStr::StringToInt(*param_it);

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
    catch (CNetCacheException& ex) {
        if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
            return ePeerActionOK;
        }
        else {
            ERR_POST(ex);
            return ePeerBadNetwork;
        }
    }
    catch (CException& ex) {
        ERR_POST(ex);
        return ePeerBadNetwork;
    }
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
        IReader* reader = srv_api.ExecRead(api_cmd, NULL);
        delete reader;
        return true;
    }
    catch (CException& ex) {
        ERR_POST(ex);
        return false;
    }
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
        IReader* reader = srv_api.ExecRead(api_cmd, NULL);
        delete reader;
    }
    catch (CException& ex) {
        ERR_POST(ex);
    }
}

bool
CNetCacheServer::IsOpenToClients(void)
{
    return g_NetcacheServer  &&  g_NetcacheServer->m_OpenToClients;
}

void
CNetCacheServer::MayOpenToClients(void)
{
    try {
        string ports_str;
        ITERATE(TPortsList, it, g_NetcacheServer->m_Ports) {
            unsigned int port = *it;
            if (g_NetcacheServer->IsDebugMode())
                port += 10;
            g_NetcacheServer->AddListener(new CNCMsgHndlFactory_Proxy(), port);
            ports_str.append(NStr::IntToString(port));
            ports_str.append(", ", 2);
        }
        ports_str.resize(ports_str.size() - 2);
        LOG_POST("Opening ports " << ports_str << " to clients");
        g_NetcacheServer->StartListening();
    }
    catch (CException& ex) {
        ERR_POST("Failed to open client ports: " << ex << " Shutting down.");
        g_NetcacheServer->RequestShutdown();
    }
    g_NetcacheServer->m_OpenToClients = true;
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
    CNCPeriodicSync::Initialize();
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

void
CNetCacheServer::AddDeferredTask(CThreadPool_Task* task)
{
    s_TaskPool->AddTask(task);
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
    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    // Main thread request context already created, so is not affected
    // by just set default, so set it manually.
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_ToStdlog);
}
