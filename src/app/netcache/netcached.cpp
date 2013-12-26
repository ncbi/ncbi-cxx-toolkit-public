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
#include <db/sqlite/sqlitewrapp.hpp>

#include "task_server.hpp"

#include "message_handler.hpp"
#include "netcached.hpp"
#include "netcache_version.hpp"
#include "nc_stat.hpp"
#include "distribution_conf.hpp"
#include "sync_log.hpp"
#include "peer_control.hpp"
#include "nc_storage.hpp"
#include "active_handler.hpp"
#include "periodic_sync.hpp"
#include "nc_storage_blob.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/resource.h>
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




CNCHeartBeat::~CNCHeartBeat(void)
{}

void
CNCHeartBeat::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
    if (CTaskServer::IsInShutdown())
        return;

// save server state into statistics
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
    main_params->srch_on_read    = false;
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
    state.mirror_queue_size = CNCPeerControl::GetMirrorQueueSize();
    state.sync_log_size = CNCSyncLog::GetLogSize();
    CWriteBackControl::ReadState(state);
}

bool s_ReportPid(const string& pid_file)
{
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
    // Defaults that should be always set for NetCache
#ifdef NCBI_OS_LINUX
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        rlim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlim);
    }
#endif

    if (!CTaskServer::Initialize(argc, argv)  ||  !s_ReadServerParams()) {
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
                    ERR_POST(Critical << "Cannot write into pidfile: " << pid_file);
                }
            }
            else {
                ERR_POST(Critical << "Parameter -pidfile misses file name");
            }
            if (!ok) {
                CTaskServer::Finalize();
                return 122;
            }
        } else {
            ERR_POST(Critical << "Unknown parameter: " << param);
            CTaskServer::Finalize();
            return 150;
        }
    }

#ifdef NCBI_OS_LINUX
    if (is_daemon) {
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
