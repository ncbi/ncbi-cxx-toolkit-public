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

#include "nc_pch.hpp"

#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <db/sqlite/sqlitewrapp.hpp>

#include "task_server.hpp"

#include "netcached.hpp"
#include "message_handler.hpp"
#include "netcache_version.hpp"
#include "nc_stat.hpp"
#include "distribution_conf.hpp"
#include "sync_log.hpp"
#include "peer_control.hpp"
#include "nc_storage.hpp"
#include "active_handler.hpp"
#include "periodic_sync.hpp"
#include "nc_storage_blob.hpp"

#include "logging.hpp"
#include "server_core.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/resource.h>
#endif

// this must be 0 in release builds
#define MAKE_TEST_BUILD   0

#if MAKE_TEST_BUILD
#include <util/random_gen.hpp>
void s_InitTests(void);
#endif

BEGIN_NCBI_SCOPE


static const char* kNCReg_ServerSection       = "netcache";
static const char* kNCReg_Ports               = "ports";
static const char* kNCReg_CtrlPort            = "control_port";
static const char* kNCReg_AdminClient         = "admin_client_name";
static const char* kNCReg_DefAdminClient      = "netcache_control";
static const char* kNCReg_SpecPriority        = "app_setup_priority";
//static const char* kNCReg_NetworkTimeout      = "network_timeout";
static const char* kNCReg_DisableClient       = "disable_client";
//static const char* kNCReg_CommandTimeout      = "cmd_timeout";
static const char* kNCReg_LifespanTTL         = "lifespan_ttl";
static const char* kNCReg_MaxTTL              = "max_ttl";
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


typedef set<unsigned int>   TPortsList;
typedef vector<string>      TSpecKeysList;
struct SSpecParamsEntry {
    string        key;
    CSrvRef<CObject> value;

    SSpecParamsEntry(const string& key, CObject* value);
};
struct SSpecParamsSet : public CObject {
    vector<SSpecParamsEntry>  entries;

    virtual ~SSpecParamsSet(void);
};


/// Port where server runs
static TPortsList s_Ports;
static Uint4 s_CtrlPort;
/// Name of client that should be used for administrative commands
static string s_AdminClient;
static TSpecKeysList s_SpecPriority;
static CSrvRef<SSpecParamsSet> s_SpecParams;
//static unsigned int s_DefConnTimeout;
static int s_DefBlobTTL;
static bool s_DebugMode = false;
static bool s_InitiallySynced = false;
static bool s_CachingComplete = false;
static CNCMsgHandler_Factory s_MsgHandlerFactory;
static CNCHeartBeat* s_HeartBeat;
static string s_PidFile;


CNCBlobKeyLight& CNCBlobKeyLight::operator=(const CTempString& packed_key)
{
    m_PackedKey.assign(packed_key.data(), packed_key.size());
    UnpackBlobKey();
    m_KeyVersion = 0;
    return *this;
}

unsigned int CNCBlobKeyLight::KeyVersion(void) const
{
    if (m_KeyVersion == 0) {
        CNetCacheKey tmp;
        if (CNetCacheKey::ParseBlobKey(m_RawKey.data(), m_RawKey.size(), &tmp)) {
            return tmp.GetVersion();
        }
    }
    return m_KeyVersion;
}

CNCBlobKeyLight& CNCBlobKeyLight::Copy(const CNCBlobKeyLight& another)
{
    if (this != &another) {
        m_PackedKey = another.m_PackedKey;
        UnpackBlobKey();
        m_KeyVersion   = another.m_KeyVersion;
    }
    return *this;
}
void CNCBlobKeyLight::Clear(void)
{
    m_PackedKey.clear();
    m_Cachename.clear();
    m_RawKey.clear();
    m_SubKey.clear();
    m_KeyVersion = 0;
}

void CNCBlobKeyLight::PackBlobKey(const CTempString& cache_name,
    const CTempString& blob_key, const CTempString& blob_subkey)
{
    m_PackedKey.reserve(cache_name.size() + blob_key.size()
                        + blob_subkey.size() + 2);
    m_PackedKey.assign(cache_name.data(), cache_name.size());
    m_PackedKey.append(1, '\1');
    m_PackedKey.append(blob_key.data(), blob_key.size());
    m_PackedKey.append(1, '\1');
    m_PackedKey.append(blob_subkey.data(), blob_subkey.size());
}

void CNCBlobKeyLight::UnpackBlobKey()
{
    // cache
    const char* cache_str = m_PackedKey.data();
    size_t cache_size = m_PackedKey.find('\1', 0);
    m_Cachename.assign(cache_str, cache_size);
    // key
    const char* key_str = cache_str + cache_size + 1;
    size_t key_size = m_PackedKey.find('\1', cache_size + 1)
                      - cache_size - 1;
    m_RawKey.assign(key_str, key_size);
    // subkey
    const char* subkey_str = key_str + key_size + 1;
    size_t subkey_size = m_PackedKey.size() - cache_size - key_size - 2;
    m_SubKey.assign(subkey_str, subkey_size);
}

string CNCBlobKeyLight::KeyForLogs(void) const
{
    string result;
    result.append("'");
    result.append(m_RawKey.data(), m_RawKey.size());
    result.append("'");
    if (!m_Cachename.empty()  ||  !m_SubKey.empty()) {
        result.append(", '");
        result.append(m_SubKey.data(), m_SubKey.size());
        result.append("' from cache '");
        result.append(m_Cachename.data(), m_Cachename.size());
        result.append("'");
    }
    return result;
}

void CNCBlobKey::Assign( const CTempString& cache_name,
                         const CTempString& blob_key,
                         const CTempString& blob_subkey)
{
    if (blob_key.empty()) {
        Clear();
        return;
    }
    string rawKey = blob_key;
    if (CNetCacheKey::ParseBlobKey(blob_key.data(), blob_key.size(), this)) {
        // If key is given and it's NetCache key (not ICache key) then we need
        // to strip service name from it. It's necessary for the case when new
        // CNetCacheAPI with enabled_mirroring=true passes key to an old CNetCacheAPI
        // which doesn't even know about mirroring.
        if (cache_name.empty() && HasExtensions()) {
            rawKey = StripKeyExtensions();
        }
    } else if (cache_name.empty()) {
        SRV_LOG(Critical, "CNetCacheKey failed to parse blob key: " << blob_key);
        Clear();
        return;
    }
    PackBlobKey(cache_name, rawKey, blob_subkey);
    UnpackBlobKey();
    SetKeyVersion(max((unsigned int)1,GetVersion()));
}


CNCHeartBeat::CNCHeartBeat(void)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CNCHeartBeat";
#endif
}

CNCHeartBeat::~CNCHeartBeat(void)
{}

void CNCHeartBeat::CheckConfFile(void)
{
    static time_t modified = 0;
    CFile cfg(GetConfName());
    if (modified == 0) {
        if (cfg.Exists()) {
            cfg.GetTimeT(&modified);
        }
        return;
    }
    if (!cfg.Exists()) {
        if (modified != 1) {
            CNCAlerts::Register(CNCAlerts::eStartupConfigChanged, GetConfName() + ": file not found");
        }
        modified = 1;
        return;
    }
    time_t mod = 0;
    cfg.GetTimeT(&mod);
    if (mod == modified) {
        return;
    }
    modified = mod;
    string msg(GetConfName() + ": modified on ");
    CTime tm;
    if (cfg.GetTime(&tm)) {
        msg += tm.AsString();
    } else {
        msg += " GetTime() failed";
    }
    CNCAlerts::Register(CNCAlerts::eStartupConfigChanged, msg);
}

void
CNCHeartBeat::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
    if (CTaskServer::IsInShutdown())
        return;

// save server state into statistics
    CheckConfFile();
    CNCBlobStorage::CheckDiskSpace();
    SNCStateStat state;
    CNCServer::ReadCurState(state);
    CNCStat::SaveCurStateStat(state);

    RunAfter(1);
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
SSpecParamsEntry::SSpecParamsEntry(const string& key,
                                   CObject*      value)
    : key(key),
      value(value)
{}


SSpecParamsSet::~SSpecParamsSet(void)
{}


static void
s_ReadSpecificParams(const IRegistry& reg,
                     const string& section,
                     SNCSpecificParams* params)
{
    if (reg.HasEntry(section, kNCReg_DisableClient, IRegistry::fCountCleared)) {
        params->disable = reg.GetBool(section, kNCReg_DisableClient, false);
    }
    if (reg.HasEntry(section, kNCReg_BlobTTL, IRegistry::fCountCleared)) {
        params->blob_ttl = reg.GetInt(section, kNCReg_BlobTTL, 3600);
    }
    if (reg.HasEntry(section, kNCReg_LifespanTTL, IRegistry::fCountCleared)) {
        params->lifespan_ttl = reg.GetInt(section, kNCReg_LifespanTTL, 0);
    }
    if (reg.HasEntry(section, kNCReg_MaxTTL, IRegistry::fCountCleared)) {
        Uint4 one_month = 2592000; //30 days: 3600 x 24 x 30
        params->max_ttl = reg.GetInt(section, kNCReg_MaxTTL, max( 10 * params->blob_ttl, one_month));
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
                SRV_LOG(Error, "Incorrect value of '" << kNCReg_PassPolicy
                               << "' parameter: '" << pass_policy
                               << "', assuming 'any'");
            }
            params->pass_policy = eNCBlobPassAny;
        }
    }
}

static inline void
s_PutNewParams(SSpecParamsSet* params_set,
               unsigned int best_index,
               const SSpecParamsEntry& entry)
{
    params_set->entries.insert(params_set->entries.begin() + best_index, entry);
}

static SSpecParamsSet*
s_FindNextParamsSet(const SSpecParamsSet* cur_set,
                    const string& key,
                    unsigned int& best_index)
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

static void
s_CheckDefClientConfig(SSpecParamsSet* cur_set,
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
            s_CheckDefClientConfig((SSpecParamsSet*)cur_set->entries[i].value.GetPointer(),
                                   cur_set, depth - 1, this_deflt);
            this_deflt = (SSpecParamsSet*)cur_set->entries[0].value.GetPointer();
        }
    }
}

static void
s_ReadPerClientConfig(const CNcbiRegistry& reg)
{
    string spec_prty = reg.Get(kNCReg_ServerSection, kNCReg_SpecPriority);
    NStr::Tokenize(spec_prty, ", \t\r\n", s_SpecPriority, NStr::eMergeDelims);

    SNCSpecificParams* main_params = new SNCSpecificParams;
    main_params->disable         = false;
    //main_params->cmd_timeout     = 600;
    //main_params->conn_timeout    = 10;
    main_params->lifespan_ttl    = 0;
    main_params->max_ttl         = 2592000;
    main_params->blob_ttl        = 3600;
    main_params->ver_ttl         = 3600;
    main_params->ttl_unit        = 300;
    main_params->prolong_on_read = true;
    main_params->srch_on_read    = true;
    main_params->pass_policy     = eNCBlobPassAny;
    main_params->quorum          = 2;
    main_params->fast_on_main    = true;
    s_ReadSpecificParams(reg, kNCReg_ServerSection, main_params);
    //s_DefConnTimeout = main_params->conn_timeout;
    s_DefBlobTTL = main_params->blob_ttl;
    SSpecParamsSet* params_set = new SSpecParamsSet();
    params_set->entries.push_back(SSpecParamsEntry(kEmptyStr, main_params));
    for (unsigned int i = 0; i < s_SpecPriority.size(); ++i) {
        SSpecParamsSet* next_set = new SSpecParamsSet();
        next_set->entries.push_back(SSpecParamsEntry(kEmptyStr, params_set));
        params_set = next_set;
    }
    s_SpecParams = params_set;

    if (s_SpecPriority.size() != 0) {
        list<string> conf_sections;
        reg.EnumerateSections(&conf_sections);
        ITERATE(list<string>, sec_it, conf_sections) {
            const string& section = *sec_it;
            if (!NStr::StartsWith(section, kNCReg_AppSetupPrefix, NStr::eNocase))
                continue;
            SSpecParamsSet* cur_set  = s_SpecParams;
            NON_CONST_REVERSE_ITERATE(TSpecKeysList, prty_it, s_SpecPriority) {
                const string& key_name = *prty_it;
                if (reg.HasEntry(section, key_name, IRegistry::fCountCleared)) {
                    const string& key_value = reg.Get(section, key_name);
                    unsigned int next_ind = 0;
                    SSpecParamsSet* next_set
                                = s_FindNextParamsSet(cur_set, key_value, next_ind);
                    if (!next_set) {
                        if (cur_set->entries.size() == 0) {
                            cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, new SSpecParamsSet()));
                            ++next_ind;
                        }
                        next_set = new SSpecParamsSet();
                        s_PutNewParams(cur_set, next_ind,
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
                SRV_LOG(Error, "Section '" << section << "' in configuration file is "
                               "a duplicate of another section - ignoring it.");
                continue;
            }
            SNCSpecificParams* params = new SNCSpecificParams(*main_params);
            if (reg.HasEntry(section, kNCReg_AppSetupValue)) {
                s_ReadSpecificParams(reg, reg.Get(section, kNCReg_AppSetupValue),
                                     params);
            }
            cur_set->entries.push_back(SSpecParamsEntry(kEmptyStr, params));
        }

        s_CheckDefClientConfig(s_SpecParams, NULL, Uint1(s_SpecPriority.size()), NULL);
    }
}

static bool
s_ReadServerParams(void)
{
    const CNcbiRegistry& reg = CTaskServer::GetConfRegistry();

    string ports_str = reg.Get(kNCReg_ServerSection, kNCReg_Ports);
    list<string> split_ports;
    NStr::Split(ports_str, ", \t\r\n", split_ports);
    ITERATE(list<string>, it, split_ports) {
        try {
            s_Ports.insert(NStr::StringToInt(*it));
        }
        catch (CStringException& ex) {
            SRV_LOG(Critical, "Error reading port number from '"
                              << *it << "': " << ex);
            return false;
        }
    }
    if (s_Ports.size() == 0) {
        SRV_LOG(Critical, "No listening client ports were configured.");
        return false;
    }

    try {
        s_CtrlPort = reg.GetInt(kNCReg_ServerSection, kNCReg_CtrlPort, 0);
        s_AdminClient = reg.GetString(kNCReg_ServerSection, kNCReg_AdminClient,
                                      kNCReg_DefAdminClient);
        s_DebugMode = reg.GetBool(kNCReg_ServerSection, "debug_mode", false);

        s_ReadPerClientConfig(reg);
    }
    catch (CStringException& ex) {
        SRV_LOG(Critical, "Error in configuration: " << ex);
        return false;
    }

    return true;
}

static bool
s_Initialize(bool do_reinit)
{
    CSQLITE_Global::Initialize();
    CWriteBackControl::Initialize();
    InitClientMessages();

    if (!CNCDistributionConf::Initialize(s_CtrlPort))
        return false;
    CNCActiveHandler::Initialize();

    if (!CNCBlobStorage::Initialize(do_reinit))
        return false;

    if (CNCDistributionConf::GetMaxBlobSizeSync() > CNCBlobStorage::GetMaxBlobSizeStore()) {
        SRV_LOG(Critical, "Bad configuration: max_blob_size_sync ("
                            << CNCDistributionConf::GetMaxBlobSizeSync()
                            << ") is greater than max_blob_size_store ("
                            << CNCBlobStorage::GetMaxBlobSizeStore() << ").");
        return false;
    }

    CNCStat::Initialize();
    s_HeartBeat = new CNCHeartBeat();

    Uint8 max_rec_no = CNCBlobStorage::GetMaxSyncLogRecNo();
    string forget = CNCBlobStorage::GetPurgeData();
    INFO("Initial Purge data: " << forget);
    CNCBlobAccessor::UpdatePurgeData( forget, ';' );
    CNCSyncLog::Initialize(CNCBlobStorage::IsCleanStart(), max_rec_no);

    if (!CNCPeerControl::Initialize())
        return false;

    if (s_Ports.find(s_CtrlPort) == s_Ports.end()) {
        Uint4 port = s_CtrlPort;
        if (s_DebugMode)
            port += 10;
        INFO("Opening control port: " << port);
        if (!CTaskServer::AddListeningPort(port, &s_MsgHandlerFactory))
            return false;
    }
    string ports_str;
    ITERATE(TPortsList, it, s_Ports) {
        unsigned int port = *it;
        if (s_DebugMode)
            port += 10;
        if (!CTaskServer::AddListeningPort(port, &s_MsgHandlerFactory))
            return false;
        ports_str.append(NStr::IntToString(port));
        ports_str.append(", ", 2);
    }
    ports_str.resize(ports_str.size() - 2);
    INFO("Opening client ports: " << ports_str);


#if MAKE_TEST_BUILD
    s_InitTests();
#endif

    return true;
}

static void
s_Finalize(void)
{
    INFO("NetCache server is finalizing.");
    CNCStat::DumpAllStats();

    CNCBlobStorage::SaveMaxSyncLogRecNo();
    CNCPeriodicSync::Finalize();
    CNCPeerControl::Finalize();
    CNCBlobStorage::Finalize();
    CNCSyncLog::Finalize();
    CNCDistributionConf::Finalize();
    CSQLITE_Global::Finalize();
}

const SNCSpecificParams*
CNCServer::GetAppSetup(const TStringMap& client_params)
{
    const SSpecParamsSet* cur_set = s_SpecParams;
    NON_CONST_REVERSE_ITERATE(TSpecKeysList, key_it, s_SpecPriority) {
        TStringMap::const_iterator it = client_params.find(*key_it);
        const SSpecParamsSet* next_set = NULL;
        if (it != client_params.end()) {
            const string& value = it->second;
            unsigned int best_index = 0;
            next_set = s_FindNextParamsSet(cur_set, value, best_index);
        }
        if (!next_set)
            next_set = static_cast<const SSpecParamsSet*>(cur_set->entries[0].value.GetPointer());
        cur_set = next_set;
    }
    return static_cast<const SNCSpecificParams*>(cur_set->entries[0].value.GetPointer());
}

void CNCServer::WriteAppSetup(CSrvSocketTask& task, const SNCSpecificParams* params)
{
    string is("\": "), eol(",\n\"");
    task.WriteText(eol).WriteText(kNCReg_DisableClient).WriteText(is).WriteText(NStr::BoolToString(params->disable));
    task.WriteText(eol).WriteText(kNCReg_ProlongOnRead).WriteText(is).WriteText(NStr::BoolToString(params->prolong_on_read));
    task.WriteText(eol).WriteText(kNCReg_SearchOnRead ).WriteText(is).WriteText(NStr::BoolToString(params->srch_on_read));
    task.WriteText(eol).WriteText(kNCReg_FastOnMain   ).WriteText(is).WriteText(NStr::BoolToString(params->fast_on_main));
    task.WriteText(eol).WriteText(kNCReg_PassPolicy   ).WriteText(is);
    task.WriteText("\"");
    switch (params->pass_policy) {
    case eNCOnlyWithoutPass:  task.WriteText("no_password");   break;
    case eNCOnlyWithPass:     task.WriteText("with_password"); break;
    case eNCBlobPassAny:      task.WriteText("any");           break;
    }
    task.WriteText("\"");
    task.WriteText(eol).WriteText(kNCReg_LifespanTTL).WriteText(is).WriteNumber(params->lifespan_ttl);
    task.WriteText(eol).WriteText(kNCReg_MaxTTL     ).WriteText(is).WriteNumber(params->max_ttl);
    task.WriteText(eol).WriteText(kNCReg_BlobTTL    ).WriteText(is).WriteNumber(params->blob_ttl);
    task.WriteText(eol).WriteText(kNCReg_VerTTL     ).WriteText(is).WriteNumber(params->ver_ttl);
    task.WriteText(eol).WriteText(kNCReg_TTLUnit    ).WriteText(is).WriteNumber(params->ttl_unit);
    task.WriteText(eol).WriteText(kNCReg_Quorum     ).WriteText(is).WriteNumber(params->quorum);
}

void CNCServer::WriteEnvInfo(CSrvSocketTask& task)
{
    string is("\": "),iss("\": \""), eol(",\n\""), eos("\"");
    task.WriteText(eol).WriteText("hostname"    ).WriteText(iss).WriteText(   CTaskServer::GetHostName()).WriteText(eos);
    task.WriteText(eol).WriteText("workdir"     ).WriteText(iss).WriteText(   CDir::GetCwd()).WriteText(eos);
    task.WriteText(eol).WriteText("pid"         ).WriteText(is ).WriteNumber( CProcess::GetCurrentPid());
    task.WriteText(eol).WriteText("pidfile"     ).WriteText(iss).WriteText(   s_PidFile).WriteText(eos);
    task.WriteText(eol).WriteText("conffile"    ).WriteText(iss).WriteText(   GetConfName()).WriteText(eos);
    task.WriteText(eol).WriteText("logfile"     ).WriteText(iss).WriteText(   GetLogFileName()).WriteText(eos);
    task.WriteText(eol).WriteText("logfile_size").WriteText(iss).WriteText(
        NStr::UInt8ToString_DataSize(CFile(GetLogFileName()).GetLength())).WriteText(eos);
}

bool
CNCServer::IsInitiallySynced(void)
{
    return s_InitiallySynced;
}

void
CNCServer::InitialSyncComplete(void)
{
    INFO("Initial synchronization complete");
    s_InitiallySynced = true;
    SetWBInitialSyncComplete();
}

void
CNCServer::InitialSyncRequired(void)
{
    INFO("Initial synchronization required");
    s_InitiallySynced = false;
}

void
CNCServer::CachingCompleted(void)
{
    s_CachingComplete = true;
    s_HeartBeat->SetRunnable();
    if (!CNCPeriodicSync::Initialize())
        CTaskServer::RequestShutdown(eSrvFastShutdown);
}

bool
CNCServer::IsCachingComplete(void)
{
    return s_CachingComplete;
}

bool
CNCServer::IsDebugMode(void)
{
    return s_DebugMode;
}
/*
unsigned int
CNCServer::GetDefConnTimeout(void)
{
    return s_DefConnTimeout;
}
*/
int
CNCServer::GetDefBlobTTL(void)
{
    return s_DefBlobTTL;
}

const string&
CNCServer::GetAdminClient(void)
{
    return s_AdminClient;
}

int
CNCServer::GetUpTime(void)
{
    CSrvTime cur_time = CSrvTime::Current();
    cur_time -=  CTaskServer::GetStartTime();
    return int(cur_time.Sec());
}

void
CNCServer::ReadCurState(SNCStateStat& state)
{
    CNCBlobStorage::MeasureDB(state);
    CNCPeerControl::ReadCurState(state);
    state.sync_log_size = CNCSyncLog::GetLogSize();
    CWriteBackControl::ReadState(state);
}

bool s_ReportPid(const string& pid_file)
{
    s_PidFile = pid_file;
    CNcbiOfstream ofs(pid_file.c_str(), IOS_BASE::out | IOS_BASE::trunc );
    if (!ofs.is_open()) {
        return false;
    }
    ofs << CProcess::GetCurrentPid() << endl;
    return true;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        string param(argv[i]);
        if (param == "-version") {
            cout << NETCACHED_VERSION << endl;
            return 0;
        } else if (param == "-help") {
            cout << "Network data storage server" << endl;
            cout << "Arguments:" << endl;
            cout << "-conffile name  - configuration file, default = netcached.ini" << endl;
            cout << "-logfile  name  - log file, default = netcached.log" << endl;
            cout << "-pidfile  name  - report process ID into this file" << endl;
            cout << "-reinit         - reinitialize database, cleaning all data from it" << endl;
            cout << "-nodaemon       - do not enter UNIX daemon mode" << endl;
            return 0;
        }
    }

    // Defaults that should be always set for NetCache
#ifdef NCBI_OS_LINUX
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        rlim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlim);
    }
#endif

    if (!CTaskServer::Initialize(argc, argv)  ||  !s_ReadServerParams()) {
        cerr << "Failed to initialize: conffile: " << GetConfName() << ", logfile: " << GetLogFileName() << endl;
        CTaskServer::Finalize();
        return 100;
    }

    INFO(NETCACHED_FULL_VERSION);

    bool is_daemon = true;
    bool is_reinit = false;
    string pid_file;

    for (int i = 1; i < argc; ++i) {
        string param(argv[i]);
        if (param == "-nodaemon") {
            is_daemon = false;
        } else if (param == "-reinit") {
            is_reinit = true;
        } else if (param == "-pidfile") {
            bool ok=false;
            if (i + 1 < argc) {
                pid_file = argv[++i];
                ok = !pid_file.empty() && pid_file[0] != '-' && s_ReportPid(pid_file);
                if (!ok) {
                    CNCAlerts::Register(CNCAlerts::ePidFileFailed, pid_file + ": cannot write into");
                    cerr << "Cannot write into pidfile: " << pid_file << endl;
                    ERR_POST(Critical << "Cannot write into pidfile: " << pid_file);
                }
            }
            else {
                cerr << "Parameter -pidfile misses file name" << endl;
                ERR_POST(Critical << "Parameter -pidfile misses file name");
            }
            if (!ok) {
                CTaskServer::Finalize();
                return 122;
            }
        } else {
            cerr << "Unknown parameter: " << param << endl;
            ERR_POST(Critical << "Unknown parameter: " << param);
            CTaskServer::Finalize();
            return 150;
        }
    }

#ifdef NCBI_OS_LINUX
    if (is_daemon) {
        cout << "Entering UNIX daemon mode..." << endl;
        INFO("Entering UNIX daemon mode...");
        // Here's workaround for SQLite3 bug: if stdin is closed in forked
        // process then 0 file descriptor is returned to SQLite after open().
        // But there's assert there which prevents fd to be equal to 0. So
        // we keep descriptors 0, 1 and 2 in child process open. Latter two -
        // just in case somebody will try to write to them.
        bool is_good = CProcess::Daemonize(kEmptyCStr,
                               CProcess::fDontChroot | CProcess::fKeepStdin
                                                     | CProcess::fKeepStdout) != 0;
        if (!is_good) {
            cerr << "Error during daemonization" << endl;
            SRV_LOG(Critical, "Error during daemonization");
            return 200;
        }
    }
#endif

    if (!pid_file.empty()) {
        s_ReportPid(pid_file);
    }
    if (s_Initialize(is_reinit))
        CTaskServer::Run();
    s_Finalize();
    CTaskServer::Finalize();

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// tests

// Task generates and puts a lot of blobs into storage
// Sort of, controlled environment...
// no sockets, no communications, just a flood of data

#if MAKE_TEST_BUILD

static Uint4 MAKE_TEST_TASKS = 100;
static Uint8 MAKE_TEST_MAXSIZE = 1000000;

static Uint4 MAKE_TEST_DELAY = 1;
static Uint4 MAKE_TEST_INTERVAL = 10;

class CTest_BlobStorage : public CSrvStatesTask<CTest_BlobStorage>
{
public:
    CTest_BlobStorage(Uint4 id);
    virtual ~CTest_BlobStorage(void);

    State x_Start(void);
    State x_WaitForBlobAccess(void);
    State x_PutBegin(void);
    State x_PutBlobChunk(void);
    State x_PutEnd(void);
    State x_Remove(void);
    State x_Finalize(void);
    State x_Next(void);
    State x_Stop(void);

private:
    void Reset(void);
    size_t PutData(void* buf, size_t size);

    bool IsTaskPaused(void);
    void PrintStat(void);

    bool               m_Paused;
    Uint4              m_Id;
    Uint4              m_Counter;
    vector<string>     m_Keys;
    Uint2                     m_LocalPort;
    Uint2                     m_BlobSlot;
    Uint2                     m_TimeBucket;

    Uint8                     m_DataSize;
    Uint8                     m_BlobSize;
    Uint4                     m_ChunkLen;

    CNCBlobAccessor*          m_BlobAccess;
    string                    m_RawKey;
    CNCBlobKey                m_BlobKey;
    size_t                    m_Command;
    CSrvTime                  m_CmdStartTime;

    const SNCSpecificParams*  m_AppSetup;

    static CMiniMutex ms_RndLock;
    static CRandom ms_Rnd;
    static const char* ms_Commands[];

    friend void s_InitTests(void);
};

CMiniMutex CTest_BlobStorage::ms_RndLock;
CRandom CTest_BlobStorage::ms_Rnd;
const char* CTest_BlobStorage::ms_Commands[] = {"UNKNOWN", "PUT3", "REMO"};
static const size_t s_Noop   = 0;
static const size_t s_Put3   = 1;
static const size_t s_Remove = 2;

static Uint4  s_CounterPut = 0;
static Uint4  s_CounterRem = 0;
static Uint8  s_SizePut = 0;
static Uint8  s_SizeRem = 0;
static CFutex s_ftxStop;
static CFutex s_ftxCounter;

/////////////////////////////////////////////////////////////////////////////

void s_InitTests(void)
{
    CRandom::TValue seed = CRandom::TValue(time(NULL));
    CTest_BlobStorage::ms_Rnd.SetSeed(seed);
    INFO("s_InitTests, seed: " << seed);

    for (Uint4 i=0; i < MAKE_TEST_TASKS; ++i) {
        new CTest_BlobStorage(i);
    }
}

/////////////////////////////////////////////////////////////////////////////

CTest_BlobStorage::CTest_BlobStorage(Uint4 id)
{
    m_Paused = false;
    m_Id = id;
    m_Counter = 0;
    m_AppSetup = CNCServer::GetAppSetup(TStringMap());
    Reset();
    SetState(&Me::x_Start);
    SetRunnable();
}
CTest_BlobStorage::~CTest_BlobStorage(void)
{
}

void CTest_BlobStorage::Reset(void)
{
    ++m_Counter;

    m_LocalPort = 9999;
    m_BlobSlot = 0;
    m_TimeBucket = 0;
    
    m_BlobAccess = nullptr;
    m_RawKey.clear();
    m_BlobKey.Clear();
    m_Command = 0;

    ms_RndLock.Lock();
    Uint4 rand1 =  ms_Rnd.GetRand(0, MAKE_TEST_MAXSIZE);
    Uint4 rand2 =  max( ms_Rnd.GetRand(1, (rand1+3)/4 + 1), rand1/100);
#if 1
    if (m_Keys.size() > 10) {
        if (ms_Rnd.GetRand(1, 2) == 2) {
            random_shuffle(m_Keys.begin(), m_Keys.end());
            m_BlobKey.Assign( *m_Keys.begin());
            m_Keys.erase(m_Keys.begin());
        }
    }
#else
    if (m_Counter == 2) {
        m_BlobKey = *m_Keys.begin();
        m_Keys.erase(m_Keys.begin());
    }
#endif
    ms_RndLock.Unlock();

    m_DataSize = rand1;
    m_ChunkLen = rand2;
    m_BlobSize = 0; 

    if (!m_BlobKey.IsValid()) {
        m_Command = s_Put3;
    } else {
        m_Command = s_Remove;
    }

#if 0
    if (m_Counter > 2) {
        m_Command = s_Noop;
    }
#endif
}

size_t CTest_BlobStorage::PutData(void* buf, size_t size)
{
    memset(buf, 1, size);
    return size;
}

bool CTest_BlobStorage::IsTaskPaused(void)
{
    if (m_Id == 0) {
        // "primary" task
        //      stops all other test task
        //      waits for them to stop
        //      prints statistics
        //      lets other tasks go
        if ((m_Counter % MAKE_TEST_INTERVAL) == 0) {

            // raise Stop sign
            if (!s_ftxStop.ChangeValue(0,1)) {
                abort();
            }
            int count=0;
            s_ftxCounter.AddValue(1);
            CSrvTime to;
#ifdef NCBI_OS_LINUX
            to.tv_sec = 10;
#endif
            // wait for other tasks
            while (s_ftxCounter.WaitValueChange(count, to) != CFutex::eTimedOut) {
                count = s_ftxCounter.GetValue();
                if (count == MAKE_TEST_TASKS) {
                    break;
                }
            }
/*
            if (count != MAKE_TEST_TASKS) {
cout << "timeout" << endl;
                return false;
            }
*/

            PrintStat();       

            // let'em go
            s_ftxCounter.ChangeValue(MAKE_TEST_TASKS, 0);
            s_ftxStop.ChangeValue(1,0);
//            s_ftxStop.WakeUpWaiters(MAKE_TEST_TASKS);
        }
        return false;
    }
    // "secondary" tasks
    //  check Stop sign
    if (s_ftxStop.WaitValueChange(0, CSrvTime()) == CFutex::eTimedOut) {
        if (m_Paused) {
            m_Paused = false;
        }
        return m_Paused;
    }
    // Stop sign is there, report and stop
    if (!m_Paused) {
        m_Paused = true;
        s_ftxCounter.AddValue(1);
        s_ftxCounter.WakeUpWaiters(1);
    }
    return m_Paused;
}

void CTest_BlobStorage::PrintStat(void)
{
    SNCStateStat state;
    CNCBlobStorage::MeasureDB(state);
    CWriteBackControl::ReadState(state);

cout << endl;
cout << "===============" << endl;
cout << "s_CounterPut  = " << s_CounterPut << endl;
cout << "s_CounterRem  = " << s_CounterRem << endl;
cout << "blob expected = " << (s_CounterPut - s_CounterRem) << endl;
cout << "s_SizePut     = " << s_SizePut << endl;
cout << "s_SizeRem     = " << s_SizeRem << endl;
cout << "size expected = " << (s_SizePut - s_SizeRem) << endl;
cout << "===============" << endl;
cout << "db_files  = " << state.db_files << endl;
cout << "cnt_blobs = " << state.cnt_blobs << endl;
cout << "cnt_keys  = " << state.cnt_keys << endl;

cout << "state_time     = " << CSrvTime::CurSecs() << endl;
cout << "min_dead_time  = " << state.min_dead_time << endl;

cout << "db_size   = " << state.db_size << endl;
cout << "db_garb   = " << state.db_garb << endl;
cout << "inuse     = " << state.db_size - state.db_garb << endl;
cout << "wb_size   = " << state.wb_size << endl;
cout << "wb_releasable   = " << state.wb_releasable << endl;
cout << "wb_releasing   = " << state.wb_releasing << endl;
cout << "===============" << endl;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_Start(void)
{
    m_CmdStartTime = CSrvTime::Current();
    CNCStat::CmdStarted(ms_Commands[m_Command]);

    if (m_Command == s_Put3) {
        CNCDistributionConf::GenerateBlobKey(m_LocalPort, m_RawKey, m_BlobSlot, m_TimeBucket);
        m_BlobKey.Assign(m_RawKey);
        m_Keys.push_back(m_RawKey);
    } else {
        CNCDistributionConf::GetSlotByRnd(m_BlobKey.GetRandomPart(), m_BlobSlot, m_TimeBucket);
    }

    m_BlobAccess = CNCBlobStorage::GetBlobAccess( eNCCreate, m_BlobKey.PackedKey(), "", m_TimeBucket);
    m_BlobAccess->RequestMetaInfo(this);
    return &CTest_BlobStorage::x_WaitForBlobAccess;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_WaitForBlobAccess(void)
{
    if (!m_BlobAccess->IsMetaInfoReady()) {
        return NULL;
    }
    if (m_Command == s_Put3) {
        return &CTest_BlobStorage::x_PutBegin;
    }
    return &CTest_BlobStorage::x_Remove;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_PutBegin(void)
{
    m_BlobAccess->SetBlobTTL(m_AppSetup->blob_ttl);
    m_BlobAccess->SetVersionTTL(0);
    m_BlobAccess->SetBlobVersion(0);
    return &CTest_BlobStorage::x_PutBlobChunk;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_PutBlobChunk(void)
{
    Uint4 read_len = Uint4(m_BlobAccess->GetWriteMemSize());
    if (m_BlobAccess->HasError()) {
        abort();
    }
    if (read_len > m_ChunkLen) {
        read_len = m_ChunkLen;
    }
    if (read_len > m_DataSize) {
        read_len = (Uint4)m_DataSize;
    }

    Uint4 n_read = Uint4(PutData(m_BlobAccess->GetWriteMemPtr(), read_len));
    m_BlobAccess->MoveWritePos(n_read);
    m_DataSize -= n_read;
    m_BlobSize += n_read;
    CNCStat::ClientDataWrite(n_read);

    if (m_DataSize != 0) {
        SetRunnable();
        return NULL;
    }
    return &CTest_BlobStorage::x_PutEnd;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_PutEnd(void)
{
    CSrvTime cur_srv_time = CSrvTime::Current();
    Uint8 cur_time = cur_srv_time.AsUSec();
    int cur_secs = int(cur_srv_time.Sec());
    m_BlobAccess->SetBlobCreateTime(cur_time);
    if (m_BlobAccess->GetNewBlobExpire() == 0)
        m_BlobAccess->SetNewBlobExpire(cur_secs + m_BlobAccess->GetNewBlobTTL());
    m_BlobAccess->SetNewVerExpire(cur_secs + m_BlobAccess->GetNewVersionTTL());
    m_BlobAccess->SetCreateServer(
        CNCDistributionConf::GetSelfID(), CNCBlobStorage::GetNewBlobId());
    if (m_BlobAccess->HasError()) {
        abort();
    }
    return &CTest_BlobStorage::x_Finalize;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_Remove(void)
{
    if (m_BlobAccess->IsBlobExists()  &&  !m_BlobAccess->IsCurBlobExpired())
    {
        m_BlobSize = m_BlobAccess->GetCurBlobSize();

// from
// CNCMessageHandler::x_DoCmd_Remove
#if 0
        m_BlobAccess->SetBlobTTL(m_AppSetup->blob_ttl);
#else
        bool is_mirrored = CNCDistributionConf::CountServersForSlot(m_BlobSlot) != 0;
        bool is_good = CNCServer::IsInitiallySynced() && !CNCPeerControl::HasPeerInThrottle();
        unsigned int mirrored_ttl = is_good ? min(Uint4(300), m_AppSetup->blob_ttl) : m_AppSetup->blob_ttl;
        unsigned int local_ttl = 5;
        m_BlobAccess->SetBlobTTL( is_mirrored ? mirrored_ttl : local_ttl);
#endif

        m_BlobAccess->SetBlobVersion(0);
        int expire = CSrvTime::CurSecs() - 1;
        unsigned int ttl = m_BlobAccess->GetNewBlobTTL();
#if 0
        if (m_BlobAccess->IsBlobExists()  &&  m_BlobAccess->GetCurBlobTTL() > ttl)
            ttl = m_BlobAccess->GetCurBlobTTL();
#endif
        m_BlobAccess->SetNewBlobExpire(expire, expire + ttl + 1);

// from
// CNCMessageHandler::x_FinishReadingBlob
        CSrvTime cur_srv_time = CSrvTime::Current();
        Uint8 cur_time = cur_srv_time.AsUSec();
        int cur_secs = int(cur_srv_time.Sec());
        m_BlobAccess->SetBlobCreateTime(cur_time);
        if (m_BlobAccess->GetNewBlobExpire() == 0)
            m_BlobAccess->SetNewBlobExpire(cur_secs + m_BlobAccess->GetNewBlobTTL());
        m_BlobAccess->SetNewVerExpire(cur_secs + m_BlobAccess->GetNewVersionTTL());
        m_BlobAccess->SetCreateServer(CNCDistributionConf::GetSelfID(),
                                      CNCBlobStorage::GetNewBlobId());
    }
    return &CTest_BlobStorage::x_Finalize;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_Finalize(void)
{
    if (m_Command == s_Put3) {
        AtomicAdd(s_CounterPut, 1);
        AtomicAdd(s_SizePut, m_BlobSize);
    } else {
        AtomicAdd(s_CounterRem, 1);
        AtomicAdd(s_SizeRem, m_DataSize);
    }

    m_BlobAccess->Finalize();
    m_BlobAccess->Release();
    m_BlobAccess = NULL;

    CSrvTime cmd_len = CSrvTime::Current();
    cmd_len -= m_CmdStartTime;
    Uint8 len_usec = cmd_len.AsUSec();
    CNCStat::CmdFinished(ms_Commands[m_Command], len_usec, eStatus_OK);
    CNCStat::ClientBlobWrite(m_BlobSize, len_usec);

    return &CTest_BlobStorage::x_Next;
}

CTest_BlobStorage::State 
CTest_BlobStorage::x_Next(void)
{
    if (CTaskServer::IsInShutdown()) {
        return &CTest_BlobStorage::x_Stop;
    }
    if (IsTaskPaused()) {
        SetRunnable();
        return NULL;
    }
    Reset();
    if (m_Command != s_Noop) {
        SetState(&Me::x_Start);
        if (MAKE_TEST_DELAY != 0) {
            RunAfter(MAKE_TEST_DELAY);
        } else {
            SetRunnable();
        }
    }
    return NULL;
}

CTest_BlobStorage::State
CTest_BlobStorage::x_Stop(void)
{
    Terminate();
    return NULL;
}

#endif //MAKE_TEST_BUILD

