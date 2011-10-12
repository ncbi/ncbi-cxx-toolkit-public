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


#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <util/checksum.hpp>
#include <util/random_gen.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/netcache_key.hpp>

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
static Uint4    s_SlotRndShare  = numeric_limits<Uint4>::max();
static Uint8    s_SelfID        = 0;
static string   s_SelfGroup;
static CRandom  s_KeyRnd(CRandom::TValue(time(NULL)));
static string   s_SelfHostIP;
static CAtomicCounter s_BlobId;
static TNCPeerList s_Peers;
static string   s_MirroringSizeFile;
static string   s_PeriodicLogFile;
static FILE*    s_CopyDelayLog = NULL;
static Uint1    s_CntActiveSyncs = 4;
static Uint1    s_MaxSyncsOneServer = 2;
static Uint1    s_CntSyncWorkers = 30;
static Uint1    s_MaxWorkerTimePct = 50;
static Uint1    s_CntMirroringThreads = 0;
static Uint1    s_MirrorSmallExclusive = 2;
static Uint1    s_MirrorSmallPreferred = 2;
static Uint8    s_SmallBlobBoundary = 65535;
static string   s_SyncLogFileName;
static Uint4    s_MaxSlotLogEvents;
static Uint4    s_CleanLogReserve;
static Uint4    s_MaxCleanLogBatch;
static Uint8    s_MinForcedCleanPeriod;
static Uint4    s_CleanAttemptInterval;
static Uint8    s_PeriodicSyncInterval;
static Uint8    s_PeriodicSyncHeadTime;
static Uint8    s_PeriodicSyncTailTime;
static Uint8    s_PeriodicSyncTimeout;
static Uint8    s_FailedSyncRetryDelay;
static Uint8    s_NetworkErrorTimeout;

static const char*  kNCReg_NCPoolSection       = "mirror";
static string       kNCReg_NCServerPrefix      = "server_";
static string       kNCReg_NCServerSlotsPrefix = "srv_slots_";


bool
CNCDistributionConf::Initialize(Uint2 control_port)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    Uint4 self_host = SOCK_gethostbyname(0);
    s_SelfID = (Uint8(self_host) << 32) + control_port;
    char ipaddr[32];
    SOCK_ntoa(self_host, ipaddr, sizeof(ipaddr)-1);
    s_SelfHostIP = ipaddr;
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
            ERR_POST(Critical << "Incorrect peer server specification: '"
                              << reg_value << "'");
            return false;
        }
        list<CTempString>::const_iterator it_fields = srv_fields.begin();
        string grp_name = *it_fields;
        ++it_fields;
        string host_str = *it_fields;
        Uint4 host = CSocketAPI::gethostbyname(host_str);
        ++it_fields;
        string port_str = *it_fields;
        Uint2 port = NStr::StringToUInt(port_str, NStr::fConvErr_NoThrow);
        if (host == 0  ||  port == 0) {
            ERR_POST(Critical << "Bad configuration: host does not exist ("
                              << reg_value << ")");
            return false;
        }
        Uint8 srv_id = (Uint8(host) << 32) + port;
        if (srv_id == s_SelfID) {
            found_self = true;
            s_SelfGroup = grp_name;
        }
        else
            s_Peers[srv_id] = host_str + ":" + port_str;

        // There must be corresponding description of slots
        value_name = kNCReg_NCServerSlotsPrefix + NStr::IntToString(srv_idx);
        reg_value = reg.Get(kNCReg_NCPoolSection, value_name.c_str());
        if (reg_value.empty()) {
            ERR_POST(Critical << "Bad configuration: no slots for a server "
                              << srv_idx);
            return false;
        }

        list<string> values;
        NStr::Split(reg_value, ",", values);
        ITERATE(list<string>, it, values) {
            Uint2 slot = NStr::StringToUInt(*it, NStr::fConvErr_NoThrow);
            if (slot == 0) {
                ERR_POST(Critical << "Bad slot number: " << *it);
                return false;
            }
            TServersList& srvs = s_RawSlot2Servers[slot];
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end()) {
                ERR_POST(Critical << "Bad configuration: slot " << slot
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
            ERR_POST(Critical <<  "Bad configuration - no description found for "
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
        s_CntActiveSyncs = reg.GetInt(kNCReg_NCPoolSection, "max_active_syncs", 4);
        s_MaxSyncsOneServer = reg.GetInt(kNCReg_NCPoolSection, "max_syncs_one_server", 2);
        s_MaxWorkerTimePct = reg.GetInt(kNCReg_NCPoolSection, "max_deferred_time_pct", 10);
        s_CntSyncWorkers = reg.GetInt(kNCReg_NCPoolSection, "threads_deferred", 30);
        if (s_CntSyncWorkers < 1)
            s_CntSyncWorkers = 1;
        s_CntMirroringThreads = reg.GetInt(kNCReg_NCPoolSection, "threads_instant", 6);
        s_SmallBlobBoundary = reg.GetInt(kNCReg_NCPoolSection, "small_blob_max_size", 100);
        s_SmallBlobBoundary *= 1024;
        s_MirrorSmallPreferred = reg.GetInt(kNCReg_NCPoolSection, "small_blob_preferred_threads_pct", 33);
        if (s_MirrorSmallPreferred > 100)
            s_MirrorSmallPreferred = 100;
        s_MirrorSmallPreferred = s_MirrorSmallPreferred * s_CntMirroringThreads / 100;
        if (s_MirrorSmallPreferred == 0)
            s_MirrorSmallPreferred = 1;
        s_MirrorSmallExclusive = reg.GetInt(kNCReg_NCPoolSection, "small_blob_exclusive_threads_pct", 33);
        if (s_MirrorSmallExclusive > 100)
            s_MirrorSmallExclusive = 100;
        s_MirrorSmallExclusive = s_MirrorSmallExclusive * s_CntMirroringThreads / 100;
        if (s_MirrorSmallExclusive == 0)
            s_MirrorSmallExclusive = 1;
        if (s_MirrorSmallExclusive > s_CntMirroringThreads - s_MirrorSmallPreferred)
            s_MirrorSmallExclusive = s_CntMirroringThreads - s_MirrorSmallPreferred;

        s_SyncLogFileName = reg.GetString(kNCReg_NCPoolSection, "sync_log_file", "sync_events.log");
        s_MaxSlotLogEvents = reg.GetInt(kNCReg_NCPoolSection, "max_slot_log_records", 100000);
        if (s_MaxSlotLogEvents < 10)
            s_MaxSlotLogEvents = 10;
        s_CleanLogReserve = reg.GetInt(kNCReg_NCPoolSection, "clean_slot_log_reserve", 1000);
        if (s_CleanLogReserve >= s_MaxSlotLogEvents)
            s_CleanLogReserve = s_MaxSlotLogEvents - 1;
        s_MaxCleanLogBatch = reg.GetInt(kNCReg_NCPoolSection, "max_clean_log_batch", 10000);
        s_MinForcedCleanPeriod = reg.GetInt(kNCReg_NCPoolSection, "min_forced_clean_log_period", 10);
        s_MinForcedCleanPeriod *= kNCTimeTicksInSec;
        s_CleanAttemptInterval = reg.GetInt(kNCReg_NCPoolSection, "clean_log_attempt_interval", 1);
        s_PeriodicSyncInterval = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_interval", 10);
        s_PeriodicSyncInterval *= kNCTimeTicksInSec;
        s_PeriodicSyncHeadTime = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_head_time", 1);
        s_PeriodicSyncHeadTime *= kNCTimeTicksInSec;
        s_PeriodicSyncTailTime = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_tail_time", 10);
        s_PeriodicSyncTailTime *= kNCTimeTicksInSec;
        s_PeriodicSyncTimeout = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_timeout", 10);
        s_PeriodicSyncTimeout *= kNCTimeTicksInSec;
        s_FailedSyncRetryDelay = reg.GetInt(kNCReg_NCPoolSection, "failed_sync_retry_delay", 1);
        s_FailedSyncRetryDelay *= kNCTimeTicksInSec;
        s_NetworkErrorTimeout = reg.GetInt(kNCReg_NCPoolSection, "network_error_timeout", 300);
        s_NetworkErrorTimeout *= kNCTimeTicksInSec;
    }
    catch (CStringException& ex) {
        ERR_POST(Critical << "Bad configuration: " << ex);
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

Uint2
CNCDistributionConf::GetSlotByKey(const string& key)
{
    Uint4 key_rnd;
    if (key[0] == '\1') {
        // NetCache-generated key
        size_t ind = key.find('_', 0);  // version
        ind = key.find('_', ind + 1);   // id
        ind = key.find('_', ind + 1);   // host
        ind = key.find('_', ind + 1);   // port
        ind = key.find('_', ind + 1);   // time
        ind = key.find('_', ind + 1);   // random
        ++ind;
        key_rnd = NStr::StringToUInt(CTempString(&key[ind], key.size() - ind),
                                     NStr::fAllowTrailingSymbols);
    }
    else {
        // ICache key provided by client
        CChecksum crc32(CChecksum::eCRC32);
        crc32.AddChars(key.data(), key.size());
        key_rnd = crc32.GetChecksum();
    }
    // Slot numbers are 1-based
    return Uint2(key_rnd / s_SlotRndShare) + 1;
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
CNCDistributionConf::GenerateBlobKey(Uint2 local_port)
{
    Uint4 rnd_num = s_KeyRnd.GetRand();

    Uint2 cnt_pieces = Uint2(s_SelfSlots.size());
    Uint4 piece_share = (CRandom::GetMax() + 1) / cnt_pieces + 1;
    Uint2 index = rnd_num / piece_share;
    rnd_num -= index * piece_share;
    Uint2 slot = s_SelfSlots[index];
    Uint4 key_rnd = (slot - 1) * s_SlotRndShare + rnd_num % s_SlotRndShare;
    string key;
    CNetCacheKey::GenerateBlobKey(&key,
                                  static_cast<Uint4>(s_BlobId.Add(1)),
                                  s_SelfHostIP, local_port, 1, key_rnd);
    return key;
}

Uint8
CNCDistributionConf::GetMainSrvId(const string& key)
{
    try {
        CNetCacheKey nc_key(key);
        Uint4 host;
        Uint2 port;
        CSocketAPI::StringToHostPort(nc_key.GetHost(), &host, &port);
        return (Uint8(host) << 32) + nc_key.GetPort();
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
CNCDistributionConf::GetCntSyncWorkers(void)
{
    return s_CntSyncWorkers;
}

Uint1
CNCDistributionConf::GetMaxWorkerTimePct(void)
{
    return s_MaxWorkerTimePct;
}

Uint1
CNCDistributionConf::GetCntMirroringThreads(void)
{
    return s_CntMirroringThreads;
}

Uint1
CNCDistributionConf::GetMirrorSmallPrefered(void)
{
    return s_MirrorSmallPreferred;
}

Uint1
CNCDistributionConf::GetMirrorSmallExclusive(void)
{
    return s_MirrorSmallExclusive;
}

Uint8
CNCDistributionConf::GetSmallBlobBoundary(void)
{
    return s_SmallBlobBoundary;
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
    return s_PeriodicSyncHeadTime;
}

Uint8
CNCDistributionConf::GetPeriodicSyncTailTime(void)
{
    return s_PeriodicSyncTailTime;
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
        Uint8 cur_time = CNetCacheServer::GetPreciseTime();
        fprintf(s_CopyDelayLog,
                NCBI_BIGCOUNT_FORMAT_SPEC "," NCBI_BIGCOUNT_FORMAT_SPEC
                "," NCBI_BIGCOUNT_FORMAT_SPEC "," NCBI_BIGCOUNT_FORMAT_SPEC "\n",
                TNCBI_BigCount(cur_time), TNCBI_BigCount(create_server),
                TNCBI_BigCount(write_server), TNCBI_BigCount(cur_time - create_time));
    }
}


END_NCBI_SCOPE

