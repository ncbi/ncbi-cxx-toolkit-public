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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */


#include "nc_pch.hpp"

#include <corelib/ncbireg.hpp>
#include <util/checksum.hpp>
#include <util/random_gen.hpp>
#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include "task_server.hpp"

#include "distribution_conf.hpp"
#include "netcached.hpp"


BEGIN_NCBI_SCOPE


struct SSrvGroupInfo
{
    Uint8   srv_id;
    string  grp;

    SSrvGroupInfo(Uint8 srv, const string& group)
        : srv_id(srv), grp(group)
    {}
};

typedef vector<SSrvGroupInfo>       TSrvGroupsList;
typedef map<Uint2, TSrvGroupsList>  TSrvGroupsMap;

typedef map<Uint2, TServersList>    TSlot2SrvMap;
typedef map<Uint8, vector<Uint2> >  TSrv2SlotMap;

static TSrvGroupsMap s_Slot2Servers;
static TSlot2SrvMap s_RawSlot2Servers;
static TSrv2SlotMap s_CommonSlots;
static vector<Uint2> s_SelfSlots;
static Uint2    s_MaxSlotNumber = 0;
static Uint2    s_CntSlotBuckets = 0;
static Uint2    s_CntTimeBuckets = 0;
static Uint4    s_SlotRndShare  = numeric_limits<Uint4>::max();
static Uint4    s_TimeRndShare  = numeric_limits<Uint4>::max();
static Uint8    s_SelfID        = 0;
static string   s_SelfGroup;
static string   s_SelfName;
static CMiniMutex s_KeyRndLock;
static CRandom  s_KeyRnd(CRandom::TValue(time(NULL)));
static string   s_SelfHostIP;
static CAtomicCounter s_BlobId;
static TNCPeerList s_Peers;
static string   s_MirroringSizeFile;
static string   s_PeriodicLogFile;
static FILE*    s_CopyDelayLog = NULL;
static Uint1    s_CntActiveSyncs = 4;
static Uint1    s_MaxSyncsOneServer = 2;
static Uint1    s_SyncPriority = 10;
static Uint2    s_MaxPeerTotalConns = 100;
static Uint2    s_MaxPeerBGConns = 50;
static Uint1    s_CntErrorsToThrottle = 10;
static Uint8    s_PeerThrottlePeriod = 10 * kUSecsPerSecond;
static Uint1    s_PeerTimeout = 2;
static Uint1    s_BlobListTimeout = 10;
static Uint8    s_SmallBlobBoundary = 65535;
static Uint2    s_MaxMirrorQueueSize = 10000;
static string   s_SyncLogFileName;
static Uint4    s_MaxSlotLogEvents = 0;
static Uint4    s_CleanLogReserve = 0;
static Uint4    s_MaxCleanLogBatch = 0;
static Uint8    s_MinForcedCleanPeriod = 0;
static Uint4    s_CleanAttemptInterval = 0;
static Uint8    s_PeriodicSyncInterval = 0;
//static Uint8    s_PeriodicSyncHeadTime;
//static Uint8    s_PeriodicSyncTailTime;
static Uint8    s_PeriodicSyncTimeout = 0;
static Uint8    s_FailedSyncRetryDelay = 0;
static Uint8    s_NetworkErrorTimeout = 0;

static const char*  kNCReg_NCPoolSection       = "mirror";
static string       kNCReg_NCServerPrefix      = "server_";
static string       kNCReg_NCServerSlotsPrefix = "srv_slots_";


bool
CNCDistributionConf::Initialize(Uint2 control_port)
{
    string log_pfx("Bad configuration: ");
    const CNcbiRegistry& reg = CTaskServer::GetConfRegistry();

    Uint4 self_host = CTaskServer::GetIPByHost(CTaskServer::GetHostName());
    s_SelfID = (Uint8(self_host) << 32) + control_port;
    s_SelfHostIP = CTaskServer::IPToString(self_host);
    s_BlobId.Set(0);

    string reg_value;
    bool found_self = false;
    for (int srv_idx = 0; ; ++srv_idx) {
        string value_name = kNCReg_NCServerPrefix + NStr::IntToString(srv_idx);
        reg_value = reg.Get(kNCReg_NCPoolSection, value_name.c_str());
        if (reg_value.empty())
            break;

        list<CTempString> srv_fields;
        NStr::Split(reg_value, ":", srv_fields);
        if (srv_fields.size() != 3) {
            SRV_LOG(Critical, log_pfx << "invalid peer server specification: '"
                              << reg_value << "'");
            return false;
        }
        list<CTempString>::const_iterator it_fields = srv_fields.begin();
        string grp_name = *it_fields;
        ++it_fields;
        string host_str = *it_fields;
        Uint4 host = CTaskServer::GetIPByHost(host_str);
        ++it_fields;
        string port_str = *it_fields;
        Uint2 port = NStr::StringToUInt(port_str, NStr::fConvErr_NoThrow);
        if (host == 0  ||  port == 0) {
            SRV_LOG(Critical, log_pfx << "host does not exist ("
                              << reg_value << ")");
            return false;
        }
        Uint8 srv_id = (Uint8(host) << 32) + port;
        string peer_str = host_str + ":" + port_str;
        if (srv_id == s_SelfID) {
            if (found_self) {
                SRV_LOG(Critical, log_pfx << "self host mentioned twice");
                return false;
            }
            found_self = true;
            s_SelfGroup = grp_name;
            s_SelfName = peer_str;
        }
        else {
            if (s_Peers.find(srv_id) != s_Peers.end()) {
                SRV_LOG(Critical, log_pfx << "host " << peer_str
                                  << " mentioned twice");
                return false;
            }
            s_Peers[srv_id] = peer_str;
        }

        // There must be corresponding description of slots
        value_name = kNCReg_NCServerSlotsPrefix + NStr::IntToString(srv_idx);
        reg_value = reg.Get(kNCReg_NCPoolSection, value_name.c_str());
        if (reg_value.empty()) {
            SRV_LOG(Critical, log_pfx << "no slots for a server "
                              << srv_idx);
            return false;
        }

        list<string> values;
        NStr::Split(reg_value, ",", values);
        ITERATE(list<string>, it, values) {
            Uint2 slot = NStr::StringToUInt(*it, NStr::fConvErr_NoThrow);
            if (slot == 0) {
                SRV_LOG(Critical, log_pfx << "bad slot number " << *it);
                return false;
            }
            TServersList& srvs = s_RawSlot2Servers[slot];
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end()) {
                SRV_LOG(Critical, log_pfx << "slot " << slot
                                  << " provided twice for server " << srv_idx);
                return false;
            }
            if (srv_id == s_SelfID)
                s_SelfSlots.push_back(slot);
            else {
                srvs.push_back(srv_id);
                s_Slot2Servers[slot].push_back(SSrvGroupInfo(srv_id, grp_name));
            }
            s_MaxSlotNumber = max(slot, s_MaxSlotNumber);
        }
    }
    if (s_MaxSlotNumber <= 1) {
        s_MaxSlotNumber = 1;
        s_SlotRndShare = numeric_limits<Uint4>::max();
    }
    else {
        s_SlotRndShare = numeric_limits<Uint4>::max() / s_MaxSlotNumber + 1;
    }

    if (!found_self) {
        if (s_Peers.size() != 0) {
            SRV_LOG(Critical, log_pfx << "no description found for "
                              "itself (port " << control_port << ")");
            return false;
        }
        s_SelfSlots.push_back(1);
        s_SelfGroup = "grp1";
    }

    ITERATE(TNCPeerList, it_peer, s_Peers)  {
        Uint8 srv_id = it_peer->first;
        vector<Uint2>& common_slots = s_CommonSlots[srv_id];
        ITERATE(TSlot2SrvMap, it_slot, s_RawSlot2Servers) {
            Uint2 slot = it_slot->first;
            if (find(s_SelfSlots.begin(), s_SelfSlots.end(), slot) == s_SelfSlots.end())
                continue;
            const TServersList& srvs = it_slot->second;
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end())
                common_slots.push_back(it_slot->first);
        }
    }

    s_MirroringSizeFile = reg.Get(kNCReg_NCPoolSection, "mirroring_log_file");
    s_PeriodicLogFile   = reg.Get(kNCReg_NCPoolSection, "periodic_log_file");

    s_CopyDelayLog = fopen(reg.Get(kNCReg_NCPoolSection, "copy_delay_log_file").c_str(), "a");

    try {
        s_CntSlotBuckets = reg.GetInt(kNCReg_NCPoolSection, "cnt_slot_buckets", 10);
        if (numeric_limits<Uint2>::max() / s_CntSlotBuckets < s_MaxSlotNumber) {
            SRV_LOG(Critical, log_pfx << "too many buckets per slot ("
                              << s_CntSlotBuckets << ") with given number of slots ("
                              << s_MaxSlotNumber << ").");
            return false;
        }
        s_CntTimeBuckets = s_CntSlotBuckets * s_MaxSlotNumber;
        s_TimeRndShare = s_SlotRndShare / s_CntSlotBuckets + 1;
        s_CntActiveSyncs = reg.GetInt(kNCReg_NCPoolSection, "max_active_syncs", 4);
        s_MaxSyncsOneServer = reg.GetInt(kNCReg_NCPoolSection, "max_syncs_one_server", 2);
        s_SyncPriority = reg.GetInt(kNCReg_NCPoolSection, "deferred_priority", 10);
        s_MaxPeerTotalConns = reg.GetInt(kNCReg_NCPoolSection, "max_peer_connections", 100);
        s_MaxPeerBGConns = reg.GetInt(kNCReg_NCPoolSection, "max_peer_bg_connections", 50);
        s_CntErrorsToThrottle = reg.GetInt(kNCReg_NCPoolSection, "peer_errors_for_throttle", 10);
        s_PeerThrottlePeriod = reg.GetInt(kNCReg_NCPoolSection, "peer_throttle_period", 10);
        s_PeerThrottlePeriod *= kUSecsPerSecond;
        s_PeerTimeout = reg.GetInt(kNCReg_NCPoolSection, "peer_communication_timeout", 2);
        s_BlobListTimeout = reg.GetInt(kNCReg_NCPoolSection, "peer_blob_list_timeout", 10);
        s_SmallBlobBoundary = reg.GetInt(kNCReg_NCPoolSection, "small_blob_max_size", 100);
        s_SmallBlobBoundary *= 1024;
        s_MaxMirrorQueueSize = reg.GetInt(kNCReg_NCPoolSection, "max_instant_queue_size", 10000);

        s_SyncLogFileName = reg.GetString(kNCReg_NCPoolSection, "sync_log_file", "sync_events.log");
        s_MaxSlotLogEvents = reg.GetInt(kNCReg_NCPoolSection, "max_slot_log_records", 100000);
        if (s_MaxSlotLogEvents < 10)
            s_MaxSlotLogEvents = 10;
        s_CleanLogReserve = reg.GetInt(kNCReg_NCPoolSection, "clean_slot_log_reserve", 1000);
        if (s_CleanLogReserve >= s_MaxSlotLogEvents)
            s_CleanLogReserve = s_MaxSlotLogEvents - 1;
        s_MaxCleanLogBatch = reg.GetInt(kNCReg_NCPoolSection, "max_clean_log_batch", 10000);
        s_MinForcedCleanPeriod = reg.GetInt(kNCReg_NCPoolSection, "min_forced_clean_log_period", 10);
        s_MinForcedCleanPeriod *= kUSecsPerSecond;
        s_CleanAttemptInterval = reg.GetInt(kNCReg_NCPoolSection, "clean_log_attempt_interval", 1);
        s_PeriodicSyncInterval = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_interval", 10);
        s_PeriodicSyncInterval *= kUSecsPerSecond;
        s_PeriodicSyncTimeout = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_timeout", 10);
        s_PeriodicSyncTimeout *= kUSecsPerSecond;
        s_FailedSyncRetryDelay = reg.GetInt(kNCReg_NCPoolSection, "failed_sync_retry_delay", 1);
        s_FailedSyncRetryDelay *= kUSecsPerSecond;
        s_NetworkErrorTimeout = reg.GetInt(kNCReg_NCPoolSection, "network_error_timeout", 300);
        s_NetworkErrorTimeout *= kUSecsPerSecond;
    }
    catch (CStringException& ex) {
        SRV_LOG(Critical, log_pfx  << ex);
        return false;
    }
    return true;
}

void
CNCDistributionConf::Finalize(void)
{
    if (s_CopyDelayLog)
        fclose(s_CopyDelayLog);
}

TServersList
CNCDistributionConf::GetServersForSlot(Uint2 slot)
{
    TSrvGroupsList srvs = s_Slot2Servers[slot];
    random_shuffle(srvs.begin(), srvs.end());
    TServersList result;
    for (size_t i = 0; i < srvs.size(); ++i) {
        if (srvs[i].grp == s_SelfGroup)
            result.push_back(srvs[i].srv_id);
    }
    for (size_t i = 0; i < srvs.size(); ++i) {
        if (srvs[i].grp != s_SelfGroup)
            result.push_back(srvs[i].srv_id);
    }
    return result;
}

const TServersList&
CNCDistributionConf::GetRawServersForSlot(Uint2 slot)
{
    return s_RawSlot2Servers[slot];
}

const vector<Uint2>&
CNCDistributionConf::GetCommonSlots(Uint8 server)
{
    return s_CommonSlots[server];
}

Uint8
CNCDistributionConf::GetSelfID(void)
{
    return s_SelfID;
}

const TNCPeerList&
CNCDistributionConf::GetPeers(void)
{
    return s_Peers;
}

string
CNCDistributionConf::GetPeerName(Uint8 srv_id)
{
    string name;
    if (srv_id == s_SelfID) {
        name = s_SelfName;
    }
    else if (s_Peers.find(srv_id) != s_Peers.end()) {
        name = s_Peers[srv_id];
    }
    else {
        name = "unknown_server";
    }
    return name;
}

string
CNCDistributionConf::GetFullPeerName(Uint8 srv_id)
{
    string name( GetPeerName(srv_id));
    name += " (";
    name += NStr::UInt8ToString(srv_id);
    name += ") ";
    return name;
}


TServersList
CNCDistributionConf::GetPeerServers(void)
{
    TServersList result;
    ITERATE(TNCPeerList, it_peer, s_Peers)  {
        if (GetSelfID() != it_peer->first) {
            result.push_back(it_peer->first);
        }
    }
    return result;
}

void
CNCDistributionConf::GenerateBlobKey(Uint2 local_port,
                                     string& key, Uint2& slot, Uint2& time_bucket)
{
    s_KeyRndLock.Lock();
    Uint4 rnd_num = s_KeyRnd.GetRand();
    s_KeyRndLock.Unlock();

    Uint2 cnt_pieces = Uint2(s_SelfSlots.size());
    Uint4 piece_share = (CRandom::GetMax() + 1) / cnt_pieces + 1;
    Uint2 index = rnd_num / piece_share;
    rnd_num -= index * piece_share;
    slot = s_SelfSlots[index];
    Uint4 remain = rnd_num % s_SlotRndShare;
    Uint4 key_rnd = (slot - 1) * s_SlotRndShare + remain;
    time_bucket = Uint2((slot - 1) * s_CntSlotBuckets + remain / s_TimeRndShare) + 1;
    CNetCacheKey::GenerateBlobKey(&key,
                                  static_cast<Uint4>(s_BlobId.Add(1)),
                                  s_SelfHostIP, local_port, 1, key_rnd);
}

bool
CNCDistributionConf::GetSlotByKey(const string& key, Uint2& slot, Uint2& time_bucket)
{
    if (key[0] == '\1')
        // NetCache-generated key
        return GetSlotByNetCacheKey(key, slot, time_bucket);
    else {
        // ICache key provided by client
        GetSlotByICacheKey(key, slot, time_bucket);
        return true;
    }
}

bool
CNCDistributionConf::GetSlotByNetCacheKey(const string& key,
        Uint2& slot, Uint2& time_bucket)
{
#define SKIP_UNDERSCORE(key, ind) \
    ind = key.find('_', ind + 1); \
    if (ind == string::npos) \
        return false;

    size_t ind = 0;
    SKIP_UNDERSCORE(key, ind);      // version
    SKIP_UNDERSCORE(key, ind);      // id
    SKIP_UNDERSCORE(key, ind);      // host
    SKIP_UNDERSCORE(key, ind);      // port
    SKIP_UNDERSCORE(key, ind);      // time
    SKIP_UNDERSCORE(key, ind);      // random
    ++ind;

    unsigned key_rnd = NStr::StringToUInt(
            CTempString(&key[ind], key.size() - ind),
            NStr::fConvErr_NoThrow | NStr::fAllowTrailingSymbols);

    if (key_rnd == 0 && errno != 0)
        return false;

    GetSlotByRnd(key_rnd, slot, time_bucket);

    return true;
}

void
CNCDistributionConf::GetSlotByICacheKey(const string& key,
        Uint2& slot, Uint2& time_bucket)
{
    CChecksum crc32(CChecksum::eCRC32);

    crc32.AddChars(key.data(), key.size());

    GetSlotByRnd(crc32.GetChecksum(), slot, time_bucket);
}

void
CNCDistributionConf::GetSlotByRnd(Uint4 key_rnd,
        Uint2& slot, Uint2& time_bucket)
{
    // Slot numbers are 1-based
    slot = Uint2(key_rnd / s_SlotRndShare) + 1;
    time_bucket = Uint2((slot - 1) * s_CntSlotBuckets
                        + key_rnd % s_SlotRndShare / s_TimeRndShare) + 1;
}

Uint4
CNCDistributionConf::GetMainSrvIP(const string& key)
{
    try {
        CNetCacheKey nc_key(key);
        return CTaskServer::GetIPByHost(nc_key.GetHost());
    }
    catch (CNetCacheException&) {
        return 0;
    }
}

bool
CNCDistributionConf::IsServedLocally(Uint2 slot)
{
    return find(s_SelfSlots.begin(), s_SelfSlots.end(), slot) != s_SelfSlots.end();
}

Uint2
CNCDistributionConf::GetCntSlotBuckets(void)
{
    return s_CntSlotBuckets;
}

Uint2
CNCDistributionConf::GetCntTimeBuckets(void)
{
    return s_CntTimeBuckets;
}

const vector<Uint2>&
CNCDistributionConf::GetSelfSlots(void)
{
    return s_SelfSlots;
}

const string&
CNCDistributionConf::GetMirroringSizeFile(void)
{
    return s_MirroringSizeFile;
}

const string&
CNCDistributionConf::GetPeriodicLogFile(void)
{
    return s_PeriodicLogFile;
}

Uint1
CNCDistributionConf::GetCntActiveSyncs(void)
{
    return s_CntActiveSyncs;
}

Uint1
CNCDistributionConf::GetMaxSyncsOneServer(void)
{
    return s_MaxSyncsOneServer;
}

Uint1
CNCDistributionConf::GetSyncPriority(void)
{
    return CNCServer::IsInitiallySynced()? s_SyncPriority: 1;
}

Uint2
CNCDistributionConf::GetMaxPeerTotalConns(void)
{
    return s_MaxPeerTotalConns;
}

Uint2
CNCDistributionConf::GetMaxPeerBGConns(void)
{
    return s_MaxPeerBGConns;
}

Uint1
CNCDistributionConf::GetCntErrorsToThrottle(void)
{
    return s_CntErrorsToThrottle;
}

Uint8
CNCDistributionConf::GetPeerThrottlePeriod(void)
{
    return s_PeerThrottlePeriod;
}

Uint1
CNCDistributionConf::GetPeerTimeout(void)
{
    return s_PeerTimeout;
}

Uint1
CNCDistributionConf::GetBlobListTimeout(void)
{
    return s_BlobListTimeout;
}

Uint8
CNCDistributionConf::GetSmallBlobBoundary(void)
{
    return s_SmallBlobBoundary;
}

Uint2
CNCDistributionConf::GetMaxMirrorQueueSize(void)
{
    return s_MaxMirrorQueueSize;
}

const string&
CNCDistributionConf::GetSyncLogFileName(void)
{
    return s_SyncLogFileName;
}

Uint4
CNCDistributionConf::GetMaxSlotLogEvents(void)
{
    return s_MaxSlotLogEvents;
}

Uint4
CNCDistributionConf::GetCleanLogReserve(void)
{
    return s_CleanLogReserve;
}

Uint4
CNCDistributionConf::GetMaxCleanLogBatch(void)
{
    return s_MaxCleanLogBatch;
}

Uint8
CNCDistributionConf::GetMinForcedCleanPeriod(void)
{
    return s_MinForcedCleanPeriod;
}

Uint4
CNCDistributionConf::GetCleanAttemptInterval(void)
{
    return s_CleanAttemptInterval;
}

Uint8
CNCDistributionConf::GetPeriodicSyncInterval(void)
{
    return s_PeriodicSyncInterval;
}

Uint8
CNCDistributionConf::GetPeriodicSyncHeadTime(void)
{
    //return s_PeriodicSyncHeadTime;
    return 0;
}

Uint8
CNCDistributionConf::GetPeriodicSyncTailTime(void)
{
    //return s_PeriodicSyncTailTime;
    return 0;
}

Uint8
CNCDistributionConf::GetPeriodicSyncTimeout(void)
{
    return s_PeriodicSyncTimeout;
}

Uint8
CNCDistributionConf::GetFailedSyncRetryDelay(void)
{
    return s_FailedSyncRetryDelay;
}

Uint8
CNCDistributionConf::GetNetworkErrorTimeout(void)
{
    return s_NetworkErrorTimeout;
}

void
CNCDistributionConf::PrintBlobCopyStat(Uint8 create_time, Uint8 create_server, Uint8 write_server)
{
    if (s_CopyDelayLog) {
        Uint8 cur_time = CSrvTime::Current().AsUSec();
        fprintf(s_CopyDelayLog,
                "%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC "\n",
                cur_time, create_server, write_server, cur_time - create_time);
    }
}


END_NCBI_SCOPE
