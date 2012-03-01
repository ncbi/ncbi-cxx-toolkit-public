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

#include <util/random_gen.hpp>

#include "periodic_sync.hpp"
#include "distribution_conf.hpp"
#include "netcached.hpp"
#include "sync_log.hpp"
#include "message_handler.hpp"
#include "peer_control.hpp"
#include "active_handler.hpp"
#include "nc_storage.hpp"


BEGIN_NCBI_SCOPE


static TSyncSlotsMap    s_SlotsList;
static TSyncSlotsMap    s_SlotsMap;
static CFastMutex       s_MainLock;
static CConditionVariable s_CleanerCond;
static CConditionVariable s_MainsCond;
static bool             s_NeedFinish = false;
static CMiniMutex       s_RndLock;
static CRandom          s_Rnd(CRandom::TValue(time(NULL)));

typedef vector< CNCRef<CNCActiveSyncControl> > TSyncControls;
static TSyncControls s_SyncControls;
static CNCRef<CThread> s_LogCleaner;

static CNCThrottler_Getter s_TimeThrottler;

static FILE* s_LogFile = NULL;



static void
s_FindServerSlot(Uint8 server_id,
                 Uint2 slot,
                 SSyncSlotData*& slot_data,
                 SSyncSlotSrv*&  slot_srv)
{
    slot_data = NULL;
    slot_srv = NULL;
    TSyncSlotsMap::const_iterator it_slot = s_SlotsMap.find(slot);
    if (it_slot == s_SlotsMap.end())
        return;
    slot_data = it_slot->second;
    slot_data->lock.Lock();
    TSlotSrvsList srvs = slot_data->srvs;
    slot_data->lock.Unlock();
    ITERATE(TSlotSrvsList, it_srv, srvs) {
        SSyncSlotSrv* this_srv = it_srv->second;
        if (this_srv->peer->GetSrvId() == server_id) {
            slot_srv = this_srv;
            return;
        }
    }
}

static ESyncInitiateResult
s_StartSync(SSyncSlotData* slot_data, SSyncSlotSrv* slot_srv, bool is_passive)
{
    CFastMutexGuard g_slot(slot_data->lock);
    if (slot_data->cleaning  ||  slot_data->clean_required)
        return eServerBusy;

    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started) {
        if (!is_passive  ||  !slot_srv->is_passive  ||  slot_srv->started_cmds != 0)
            return eCrossSynced;
        slot_srv->sync_started = false;
        --slot_data->cnt_sync_started;
    }

    if (!is_passive  &&  !slot_srv->peer->StartActiveSync())
        return eServerBusy;
    slot_srv->sync_started = true;
    slot_srv->is_passive = is_passive;
    ++slot_srv->cur_sync_id;
    ++slot_data->cnt_sync_started;
    return eProceedWithEvents;
}

static void
s_StopSync(SSyncSlotData* slot_data, SSyncSlotSrv* slot_srv, Uint8 next_delay)
{
    slot_srv->peer->RegisterSyncStop(slot_srv->is_passive,
                                     slot_srv->next_sync_time,
                                     next_delay);
    slot_srv->sync_started = false;
    if (slot_data->cnt_sync_started == 0)
        abort();
    if (--slot_data->cnt_sync_started == 0  &&  slot_data->clean_required)
        s_CleanerCond.SignalAll();
}

static void
s_CommitSync(SSyncSlotData* slot_data, SSyncSlotSrv* slot_srv)
{
    if (slot_srv->is_by_blobs)
        slot_srv->was_blobs_sync = true;
    if (!slot_srv->made_initial_sync  &&  !CNetCacheServer::IsInitiallySynced())
    {
        slot_srv->made_initial_sync = true;
        slot_srv->peer->AddInitiallySyncedSlot();
    }
    s_StopSync(slot_data, slot_srv, CNCDistributionConf::GetPeriodicSyncInterval());
}

static void
s_DoCleanLog(Uint2 slot)
{
    CNCRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    GetDiagContext().SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "clean")
                        .Print("slot", slot);
    }
    ctx->SetRequestStatus(CNCMessageHandler::eStatus_OK);
    Uint8 cleaned = CNCSyncLog::Clean(slot);
    ctx->SetBytesWr(Int8(cleaned));
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStop();
    }
    ctx.Reset();
    GetDiagContext().SetRequestContext(NULL);
}

static void
s_LogCleanerMain(void)
{
    Uint8 min_period = CNCDistributionConf::GetMinForcedCleanPeriod();
    Uint4 interval = CNCDistributionConf::GetCleanAttemptInterval();
    map<Uint2, Uint8> last_force_time;
    while (!s_NeedFinish) {
        ITERATE(TSyncSlotsMap, it_slot, s_SlotsList) {
            SSyncSlotData* slot_data = it_slot->second;
            Uint2 slot = slot_data->slot;
            slot_data->lock.Lock();
            if (slot_data->cnt_sync_started == 0) {
                slot_data->cleaning = true;
                slot_data->lock.Unlock();
                s_DoCleanLog(slot);
                slot_data->lock.Lock();
                slot_data->cleaning = false;
                if (slot_data->clean_required) {
                    slot_data->clean_required = false;
                    last_force_time[slot] = CNetCacheServer::GetPreciseTime();
                }
            }
            else if (!slot_data->clean_required
                     &&  !CNCPeerControl::HasServersForInitSync()
                     &&  CNCSyncLog::IsOverLimit(slot)
                     &&  CNetCacheServer::GetPreciseTime()
                                          - last_force_time[slot] >= min_period)
            {
                slot_data->clean_required = true;
            }
            slot_data->lock.Unlock();
        }
        s_MainLock.Lock();
        s_CleanerCond.WaitForSignal(s_MainLock, CTimeout(interval, 0));
        s_MainLock.Unlock();
    }
}



SSyncSlotData::SSyncSlotData(Uint2 slot_)
    : slot(slot_),
      cnt_sync_started(0),
      cleaning(false),
      clean_required(false)
{}

SSyncSlotSrv::SSyncSlotSrv(CNCPeerControl* peer_)
    : peer(peer_),
      sync_started(false),
      was_blobs_sync(false),
      made_initial_sync(false),
      started_cmds(0),
      next_sync_time(0),
      cur_sync_id(0)
{}



void
CNCPeriodicSync::PreInitialize(void)
{
    s_TimeThrottler.Initialize();
}

bool
CNCPeriodicSync::Initialize(void)
{
    s_LogFile = fopen(CNCDistributionConf::GetPeriodicLogFile().c_str(), "a");

    const vector<Uint2>& slots = CNCDistributionConf::GetSelfSlots();
    for (Uint2 i = 0; i < slots.size(); ++i) {
        SSyncSlotData* data = new SSyncSlotData(slots[i]);
        Uint2 sort_seed;
        do {
            sort_seed = s_Rnd.GetRand(0, numeric_limits<Uint2>::max());
        }
        while (s_SlotsList.find(sort_seed) != s_SlotsList.end());
        s_SlotsList[sort_seed] = data;
        s_SlotsMap[data->slot] = data;
    }

    Uint4 cnt_to_sync = 0;
    const TNCPeerList& peers = CNCDistributionConf::GetPeers();
    ITERATE(TNCPeerList, it_peer, peers) {
        CNCPeerControl* peer = CNCPeerControl::Peer(it_peer->first);
        const vector<Uint2>& commonSlots =
                        CNCDistributionConf::GetCommonSlots(it_peer->first);
        ITERATE(vector<Uint2>, it_slot, commonSlots) {
            SSyncSlotData* slot_data = s_SlotsMap[*it_slot];
            SSyncSlotSrv* slot_srv = new SSyncSlotSrv(peer);
            Uint2 sort_seed;
            do {
                sort_seed = s_Rnd.GetRand(0, numeric_limits<Uint2>::max());
            }
            while (slot_data->srvs.find(sort_seed) != slot_data->srvs.end());
            slot_data->srvs[sort_seed] = slot_srv;
        }
        if (!commonSlots.empty()) {
            peer->SetSlotsForInitSync(Uint2(commonSlots.size()));
            ++cnt_to_sync;
        }
    }
    CNCPeerControl::SetServersForInitSync(cnt_to_sync);

    Uint1 cnt_syncs = CNCDistributionConf::GetCntActiveSyncs();
    for (Uint1 i = 0; i < cnt_syncs; ++i) {
        s_SyncControls.push_back(NCRef(new CNCActiveSyncControl()));
    }

    s_LogCleaner = NewBGThread(&s_LogCleanerMain);
    try {
        s_LogCleaner->Run();
        for (Uint1 i = 0; i < cnt_syncs; ++i) {
            s_SyncControls[i]->Run();
        }
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
        return false;
    }

    if (cnt_to_sync == 0)
        CNetCacheServer::InitialSyncComplete();

    return true;
}

void
CNCPeriodicSync::Finalize(void)
{
    s_NeedFinish = true;
    s_CleanerCond.SignalAll();
    try {
        if (s_LogCleaner.NotNull())
            s_LogCleaner->Join();
        NON_CONST_ITERATE(TSyncControls, it, s_SyncControls) {
            if (*it)
                (*it)->WakeUp();
        }
        s_MainsCond.SignalAll();
        NON_CONST_ITERATE(TSyncControls, it, s_SyncControls) {
            if (*it)
                (*it)->Join();
        }
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
    }
    if (s_LogFile)
        fclose(s_LogFile);
    s_TimeThrottler.Finalize();
}

ESyncInitiateResult
CNCPeriodicSync::Initiate(Uint8  server_id,
                          Uint2  slot,
                          Uint8* local_start_rec_no,
                          Uint8* remote_start_rec_no,
                          TReducedSyncEvents* events,
                          Uint8* sync_id)
{
    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);
    if (slot_srv) {
        slot_srv->peer->RegisterConnSuccess();
    }
    if (slot_srv == NULL
        ||  (CNCPeerControl::HasServersForInitSync()
             &&  (slot_srv->made_initial_sync
                  ||  slot_data->cnt_sync_started != 0)))
    {
        return eServerBusy;
    }

    ESyncInitiateResult init_res = s_StartSync(slot_data, slot_srv, true);
    if (init_res != eProceedWithEvents)
        return init_res;

    slot_srv->started_cmds = 1;
    *sync_id = slot_srv->cur_sync_id;
    bool records_available = CNCSyncLog::GetEventsList(server_id,
                                                       slot,
                                                       local_start_rec_no,
                                                       remote_start_rec_no,
                                                       events);
    if (records_available
        ||  (CNCSyncLog::GetLogSize(slot) == 0  &&  slot_srv->was_blobs_sync))
    {
        slot_srv->is_by_blobs = false;
        return eProceedWithEvents;
    }
    else {
        slot_srv->is_by_blobs = true;
        return eProceedWithBlobs;
    }
}

ESyncInitiateResult
CNCPeriodicSync::CanStartSyncCommand(Uint8  server_id,
                                     Uint2  slot,
                                     bool   can_abort,
                                     Uint8& sync_id)
{
    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);
    if (slot_srv == NULL)
        return eNetworkError;
    
    CFastMutexGuard g_slot(slot_data->lock);
    if (slot_data->clean_required  &&  can_abort)
        return eServerBusy;

    CFastMutexGuard g_srv(slot_srv->lock);
    if (!slot_srv->sync_started  ||  !slot_srv->is_passive)
        return eNetworkError;

    ++slot_srv->started_cmds;
    sync_id = slot_srv->cur_sync_id;
    return eProceedWithEvents;
}

void
CNCPeriodicSync::MarkCurSyncByBlobs(Uint8 server_id, Uint2 slot, Uint8 sync_id)
{
    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);

    CFastMutexGuard g_slot(slot_data->lock);
    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started  &&  slot_srv->is_passive
        &&  slot_srv->cur_sync_id == sync_id)
    {
        slot_srv->is_by_blobs = true;
    }
}

void
CNCPeriodicSync::SyncCommandFinished(Uint8 server_id, Uint2 slot, Uint8 sync_id)
{
    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);

    CFastMutexGuard g_slot(slot_data->lock);
    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started  &&  slot_srv->is_passive
        &&  slot_srv->cur_sync_id == sync_id)
    {
        if (slot_srv->started_cmds == 0)
            abort();
        if (--slot_srv->started_cmds == 0)
            slot_srv->last_active_time = CNetCacheServer::GetPreciseTime();
    }
}

void
CNCPeriodicSync::Commit(Uint8 server_id,
                        Uint2 slot,
                        Uint8 sync_id,
                        Uint8 local_synced_rec_no,
                        Uint8 remote_synced_rec_no)
{
    CNCSyncLog::SetLastSyncRecNo(server_id, slot,
                                 local_synced_rec_no,
                                 remote_synced_rec_no);
    CNetCacheServer::UpdateLastRecNo();

    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);

    CFastMutexGuard g_slot(slot_data->lock);
    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started  &&  slot_srv->is_passive
        &&  slot_srv->cur_sync_id == sync_id)
    {
        s_CommitSync(slot_data, slot_srv);
    }
}

void
CNCPeriodicSync::Cancel(Uint8 server_id, Uint2 slot, Uint8 sync_id)
{
    SSyncSlotData* slot_data;
    SSyncSlotSrv* slot_srv;
    s_FindServerSlot(server_id, slot, slot_data, slot_srv);

    CFastMutexGuard g_slot(slot_data->lock);
    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started  &&  slot_srv->is_passive
        &&  slot_srv->cur_sync_id == sync_id)
    {
        s_StopSync(slot_data, slot_srv, 0);
    }
}


CNCActiveSyncControl::CNCActiveSyncControl(void)
{}

CNCActiveSyncControl::~CNCActiveSyncControl(void)
{}

void*
CNCActiveSyncControl::Main(void)
{
    Uint8 sync_interval = CNCDistributionConf::GetPeriodicSyncInterval();
    bool force_init_sync = false;
    bool need_rehash = false;
    while (!s_NeedFinish) {
        Uint8 min_next_time = numeric_limits<Uint8>::max();
        bool did_sync = false;
        Uint8 now = CNetCacheServer::GetPreciseTime();
        Uint8 loop_start = now;
        ITERATE(TSyncSlotsMap, it_slot, s_SlotsList) {
            SSyncSlotData* slot_data = it_slot->second;
            slot_data->lock.Lock();
            if (slot_data->cnt_sync_started == 0  ||  force_init_sync) {
                if (need_rehash) {
                    TSlotSrvsList::iterator it = slot_data->srvs.begin();
                    SSyncSlotSrv* slot_srv = it->second;
                    slot_data->srvs.erase(it);
                    Uint2 rnd;
                    do {
                        s_RndLock.Lock();
                        rnd = s_Rnd.GetRand(0, numeric_limits<Uint2>::max());
                        s_RndLock.Unlock();
                    }
                    while (slot_data->srvs.find(rnd) != slot_data->srvs.end());
                    slot_data->srvs[rnd] = slot_srv;
                }
                TSlotSrvsList srvs = slot_data->srvs;
                slot_data->lock.Unlock();
                ITERATE(TSlotSrvsList, it_srv, srvs) {
                    SSyncSlotSrv* slot_srv = it_srv->second;
                    Uint8 next_time = max(slot_srv->next_sync_time,
                                          slot_srv->peer->GetNextSyncTime());
                    if (next_time <= now
                        &&  (!CNCPeerControl::HasServersForInitSync()
                             ||  !slot_srv->made_initial_sync)
                        &&  x_DoPeriodicSync(slot_data, slot_srv))
                    {
                        did_sync = true;
                        break;
                    }
                }
                slot_data->lock.Lock();
            }
            ITERATE(TSlotSrvsList, it_srv, slot_data->srvs) {
                SSyncSlotSrv* slot_srv = it_srv->second;
                slot_srv->lock.Lock();
                if (slot_srv->sync_started) {
                    if (slot_srv->is_passive
                        &&  slot_srv->started_cmds == 0
                        &&  now - slot_srv->last_active_time
                            >= CNCDistributionConf::GetPeriodicSyncTimeout())
                    {
                        s_StopSync(slot_data, slot_srv, 0);
                    }
                }
                else {
                    Uint8 next_time = max(slot_srv->next_sync_time,
                                          slot_srv->peer->GetNextSyncTime());
                    min_next_time = min(min_next_time, next_time);
                }
                slot_srv->lock.Unlock();
            }
            slot_data->lock.Unlock();
            if (s_NeedFinish)
                break;
            now = CNetCacheServer::GetPreciseTime();
        }
        force_init_sync = CNCPeerControl::HasServersForInitSync()  &&  !did_sync;
        need_rehash = now - loop_start >= sync_interval;

        if (!s_NeedFinish) {
            now = CNetCacheServer::GetPreciseTime();
            Uint8 wait_time;
            if (min_next_time > now) {
                wait_time = min_next_time - now;
                if (wait_time > sync_interval)
                    wait_time = sync_interval;
            }
            else {
                s_RndLock.Lock();
                wait_time = s_Rnd.GetRand(0, 10000);
                s_RndLock.Unlock();
            }

            Uint4 timeout_sec  = Uint4(wait_time / kNCTimeTicksInSec);
            Uint4 timeout_usec = Uint4(wait_time % kNCTimeTicksInSec);
            s_MainLock.Lock();
            if (!s_NeedFinish) {
                s_MainsCond.WaitForSignal(s_MainLock,
                                          CTimeout(timeout_sec, timeout_usec));
            }
            s_MainLock.Unlock();
        }
    }

    return NULL;
}

bool
CNCActiveSyncControl::x_DoPeriodicSync(SSyncSlotData* slot_data,
                                     SSyncSlotSrv*  slot_srv)
{
    ESyncInitiateResult init_res = s_StartSync(slot_data, slot_srv, false);
    if (init_res != eProceedWithEvents)
        return false;

    m_SlotData = slot_data;
    m_SlotSrv = slot_srv;
    m_SrvId = slot_srv->peer->GetSrvId();
    m_Slot = slot_data->slot;
    m_Result = eSynOK;
    m_SlotSrv->is_by_blobs = false;
    m_StartedCmds = 0;
    m_NextTask = eSynNoTask;
    Uint8 start_time = CNetCacheServer::GetPreciseTime();

    m_ReadOK = m_ReadERR = 0;
    m_WriteOK = m_WriteERR = 0;
    m_ProlongOK = m_ProlongERR = 0;
    m_DelOK = m_DelERR = 0;

    m_DiagCtx = new CRequestContext();
    m_DiagCtx->SetRequestID();
    GetDiagContext().SetRequestContext(m_DiagCtx);
    if (g_NetcacheServer->IsLogCmds()) {
        CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
        extra.Print("_type", "sync");
        extra.Print("srv_id", m_SrvId);
        extra.Print("slot", m_Slot);
        extra.Print("self_id", CNCDistributionConf::GetSelfID());
        extra.Flush();
    }
    m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_OK);

    CNCSyncLog::GetLastSyncedRecNo(m_SrvId, m_Slot,
                                   &m_LocalStartRecNo, &m_RemoteStartRecNo);

    CNCActiveHandler* conn = slot_srv->peer->GetBGConn();
    if (conn) {
        m_Lock.Lock();
        m_StartedCmds = 1;
        conn->SyncStart(this, m_LocalStartRecNo, m_RemoteStartRecNo);
        while (m_StartedCmds != 0  &&  !s_NeedFinish)
            m_WaitCond.WaitForSignal(m_Lock);
        if (s_NeedFinish)
            m_Result = eSynAborted;
        m_Lock.Unlock();
    }
    else {
        m_Result = eSynNetworkError;
    }

    m_LocalSyncedRecNo = 0;
    m_RemoteSyncedRecNo = 0;
    if (m_Result == eSynOK) {
        if (m_SlotSrv->is_by_blobs)
            x_PrepareSyncByBlobs();
        else
            x_PrepareSyncByEvents();
        if (m_Result == eSynOK) {
            m_Lock.Lock();
            x_CalcNextTask();
            if (m_NextTask != eSynNeedFinalize) {
                if (m_SlotSrv->peer->AddSyncControl(this)) {
                    while ((m_NextTask != eSynNoTask  ||  m_StartedCmds != 0)
                           &&  !s_NeedFinish)
                    {
                        m_WaitCond.WaitForSignal(m_Lock);
                    }
                }
                else {
                    m_NextTask = eSynNoTask;
                    m_Result = eSynNetworkError;
                }
            }
            else if (m_SlotSrv->peer->FinishSync(this)) {
                while ((m_NextTask != eSynNoTask  ||  m_StartedCmds != 0)
                       &&  !s_NeedFinish)
                {
                    m_WaitCond.WaitForSignal(m_Lock);
                }
            }
            else {
                m_StartedCmds = 0;
                m_NextTask = eSynNoTask;
                m_Result = eSynNetworkError;
            }
            if (s_NeedFinish)
                m_Result = eSynAborted;
            m_Lock.Unlock();
        }
    }
    x_CleanRemoteObjects();

    switch (m_Result) {
    case eSynOK:
        CNetCacheServer::UpdateLastRecNo();
        break;
    case eSynAborted:
        m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_SyncAborted);
        break;
    case eSynCrossSynced:
        m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_CrossSync);
        break;
    case eSynServerBusy:
        m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_SyncBusy);
        break;
    case eSynNetworkError:
        m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_BadCmd);
        break;
    }

    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().Extra()
            .Print("sync", (m_SlotSrv->is_by_blobs? "blobs": "events"))
            .Print("r_ok", m_ReadOK)
            .Print("r_err", m_ReadERR)
            .Print("w_ok", m_WriteOK)
            .Print("w_err", m_WriteERR)
            .Print("p_ok", m_ProlongOK)
            .Print("p_err", m_ProlongERR)
            .Print("d_ok", m_DelOK)
            .Print("d_err", m_DelERR);
        GetDiagContext().PrintRequestStop();
    }
    m_DiagCtx.Reset();
    GetDiagContext().SetRequestContext(NULL);

    if (s_LogFile) {
        Uint8 end_time = CNetCacheServer::GetPreciseTime();
        Uint8 log_size = CNCSyncLog::GetLogSize();
        fprintf(s_LogFile,
                "%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%u,%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC
                ",%d,%d,%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC ",%u,%u\n",
                CNCDistributionConf::GetSelfID(), m_SrvId, m_Slot,
                start_time, end_time, end_time - start_time,
                int(m_SlotSrv->is_by_blobs), m_Result, log_size,
                m_ReadOK, m_ReadERR, m_WriteOK, m_WriteERR,
                m_ProlongOK, m_ProlongERR,
                Uint4(CNCPeerControl::sm_TotalCopyRequests.Get()),
                Uint4(CNCPeerControl::sm_CopyReqsRejected.Get()));
        fflush(s_LogFile);
    }

    CFastMutexGuard g_slot(m_SlotData->lock);
    CFastMutexGuard g_srv(m_SlotSrv->lock);
    if (m_Result == eSynOK) {
        s_CommitSync(m_SlotData, m_SlotSrv);
    }
    else {
        s_StopSync(m_SlotData, m_SlotSrv, CNCDistributionConf::GetFailedSyncRetryDelay());
    }
    return m_Result == eSynOK;
}


void
CNCActiveSyncControl::x_PrepareSyncByEvents(void)
{
    m_Events2Get.clear();
    m_Events2Send.clear();
    if (CNCSyncLog::GetSyncOperations(m_SrvId, m_Slot,
                                      m_LocalStartRecNo,
                                      m_RemoteStartRecNo,
                                      m_RemoteEvents,
                                      &m_Events2Get,
                                      &m_Events2Send,
                                      &m_LocalSyncedRecNo,
                                      &m_RemoteSyncedRecNo)
        ||  (CNCSyncLog::GetLogSize(m_Slot) == 0  &&  m_SlotSrv->was_blobs_sync))
    {
        m_CurGetEvent = m_Events2Get.begin();
        m_CurSendEvent = m_Events2Send.begin();
    }
    else {
        m_SlotSrv->is_by_blobs = true;
        CNCActiveHandler* conn = m_SlotSrv->peer->GetBGConn();
        if (conn) {
            m_Lock.Lock();
            m_StartedCmds = 1;
            conn->SyncBlobsList(this);
            while (m_StartedCmds != 0  &&  !s_NeedFinish)
                m_WaitCond.WaitForSignal(m_Lock);
            if (s_NeedFinish)
                m_Result = eSynAborted;
            else if (m_Result == eSynOK)
                x_PrepareSyncByBlobs();
            m_Lock.Unlock();
        }
        else {
            m_Result = eSynNetworkError;
        }
    }
}

void
CNCActiveSyncControl::x_PrepareSyncByBlobs(void)
{
    m_LocalSyncedRecNo = CNCSyncLog::GetCurrentRecNo(m_Slot);
    m_RemoteSyncedRecNo = m_RemoteStartRecNo;

    ITERATE(TNCBlobSumList, it, m_LocalBlobs) {
        delete it->second;
    }
    m_LocalBlobs.clear();
    g_NCStorage->GetFullBlobsList(m_Slot, m_LocalBlobs);

    m_CurLocalBlob = m_LocalBlobs.begin();
    m_CurRemoteBlob = m_RemoteBlobs.begin();
}

void
CNCActiveSyncControl::x_CleanRemoteObjects(void)
{
    ITERATE(TNCBlobSumList, it, m_RemoteBlobs) {
        delete it->second;
    }
    m_RemoteBlobs.clear();
    ITERATE(TReducedSyncEvents, it, m_RemoteEvents) {
        delete it->second.wr_or_rm_event;
        delete it->second.prolong_event;
    }
    m_RemoteEvents.clear();
}

void
CNCActiveSyncControl::x_CalcNextTask(void)
{
    switch (m_NextTask) {
    case eSynEventSend:
        ++m_CurSendEvent;
        break;
    case eSynEventGet:
        ++m_CurGetEvent;
        break;
    case eSynBlobUpdateOur:
    case eSynBlobUpdatePeer:
        ++m_CurLocalBlob;
        ++m_CurRemoteBlob;
        break;
    case eSynBlobSend:
        ++m_CurLocalBlob;
        break;
    case eSynBlobGet:
        ++m_CurRemoteBlob;
        break;
    case eSynNoTask:
        break;
    case eSynNeedFinalize:
        m_NextTask = eSynNoTask;
        return;
    }

    if (m_SlotData->clean_required  &&  m_Result != eSynNetworkError)
        m_Result = eSynAborted;

    if (m_Result == eSynNetworkError  ||  m_Result == eSynAborted)
        m_NextTask = eSynNeedFinalize;
    else if (!m_SlotSrv->is_by_blobs) {
        if (m_CurSendEvent != m_Events2Send.end())
            m_NextTask = eSynEventSend;
        else if (m_CurGetEvent != m_Events2Get.end())
            m_NextTask = eSynEventGet;
        else
            m_NextTask = eSynNeedFinalize;
    }
    else {
sync_next_key:
        if (m_CurLocalBlob != m_LocalBlobs.end()
            &&  m_CurRemoteBlob != m_RemoteBlobs.end())
        {
            if (m_CurLocalBlob->first == m_CurRemoteBlob->first) {
                if (m_CurLocalBlob->second->isEqual(*m_CurRemoteBlob->second)) {
                    // Equivalent blobs, skip them.
                    ++m_CurLocalBlob;
                    ++m_CurRemoteBlob;
                    goto sync_next_key;
                }

                // The same blob key. Test which one is newer.
                if (m_CurLocalBlob->second->isOlder(*m_CurRemoteBlob->second))
                    m_NextTask = eSynBlobUpdateOur;
                else
                    m_NextTask = eSynBlobUpdatePeer;
            }
            else if (m_CurLocalBlob->first < m_CurRemoteBlob->first)
                m_NextTask = eSynBlobSend;
            else
                m_NextTask = eSynBlobGet;
        }
        // Process the tails of the lists
        else if (m_CurLocalBlob != m_LocalBlobs.end())
            m_NextTask = eSynBlobSend;
        else if (m_CurRemoteBlob != m_RemoteBlobs.end())
            m_NextTask = eSynBlobGet;
        else
            m_NextTask = eSynNeedFinalize;
    }
}

void
CNCActiveSyncControl::x_DoEventSend(const SSyncTaskInfo& task_info,
                                    CNCActiveHandler* conn)
{
    SNCSyncEvent* event = *task_info.send_evt;
    switch (event->event_type) {
    case eSyncWrite:
        conn->SyncSend(this, event);
        break;
    case eSyncProlong:
        conn->SyncProlongPeer(this, event);
        break;
    }
}

void
CNCActiveSyncControl::x_DoEventGet(const SSyncTaskInfo& task_info,
                                   CNCActiveHandler* conn)
{
    SNCSyncEvent* event = *task_info.get_evt;
    switch (event->event_type) {
    case eSyncWrite:
        conn->SyncRead(this, event);
        break;
    case eSyncProlong:
        conn->SyncProlongOur(this, event);
        break;
    }
}

void
CNCActiveSyncControl::x_DoBlobUpdateOur(const SSyncTaskInfo& task_info,
                                        CNCActiveHandler* conn)
{
    string key(task_info.remote_blob->first);
    SNCCacheData* local_blob = task_info.local_blob->second;
    SNCCacheData* remote_blob = task_info.remote_blob->second;
    if (local_blob->isSameData(*remote_blob))
        conn->SyncProlongOur(this, key, *remote_blob);
    else
        conn->SyncRead(this, key, remote_blob->create_time);
}

void
CNCActiveSyncControl::x_DoBlobUpdatePeer(const SSyncTaskInfo& task_info,
                                         CNCActiveHandler* conn)
{
    string key(task_info.remote_blob->first);
    SNCCacheData* local_blob = task_info.local_blob->second;
    SNCCacheData* remote_blob = task_info.remote_blob->second;
    if (local_blob->isSameData(*remote_blob))
        conn->SyncProlongPeer(this, key, *local_blob);
    else
        conn->SyncSend(this, key);
}

void
CNCActiveSyncControl::x_DoBlobSend(const SSyncTaskInfo& task_info,
                                   CNCActiveHandler* conn)
{
    string key(task_info.local_blob->first);
    conn->SyncSend(this, key);
}

void
CNCActiveSyncControl::x_DoBlobGet(const SSyncTaskInfo& task_info,
                                  CNCActiveHandler* conn)
{
    string key(task_info.remote_blob->first);
    Uint8 create_time = task_info.remote_blob->second->create_time;
    conn->SyncRead(this, key, create_time);
}

void
CNCActiveSyncControl::x_DoFinalize(CNCActiveHandler* conn)
{
    if (m_Result == eSynOK) {
        CNCSyncLog::SetLastSyncRecNo(m_SrvId, m_Slot,
                                     m_LocalSyncedRecNo, m_RemoteSyncedRecNo);
        conn->SyncCommit(this, m_LocalSyncedRecNo, m_RemoteSyncedRecNo);
    }
    else if (m_Result == eSynAborted) {
        conn->SyncCancel(this);
    }
    else
        abort();
}

bool
CNCActiveSyncControl::GetNextTask(SSyncTaskInfo& task_info)
{
    m_Lock.Lock();
    if (m_NextTask == eSynNoTask)
        abort();
    task_info.task_type = m_NextTask;
    task_info.get_evt = m_CurGetEvent;
    task_info.send_evt = m_CurSendEvent;
    task_info.local_blob = m_CurLocalBlob;
    task_info.remote_blob = m_CurRemoteBlob;
    ++m_StartedCmds;
    if (m_StartedCmds == 0)
        abort();
    x_CalcNextTask();
    bool has_more = m_NextTask != eSynNeedFinalize  &&  m_NextTask != eSynNoTask;
    m_Lock.Unlock();

    return has_more;
}

void
CNCActiveSyncControl::ExecuteSyncTask(const SSyncTaskInfo& task_info,
                                      CNCActiveHandler* conn)
{
    switch (task_info.task_type) {
    case eSynEventSend:
        x_DoEventSend(task_info, conn);
        break;
    case eSynEventGet:
        x_DoEventGet(task_info, conn);
        break;
    case eSynBlobUpdateOur:
        x_DoBlobUpdateOur(task_info, conn);
        break;
    case eSynBlobUpdatePeer:
        x_DoBlobUpdatePeer(task_info, conn);
        break;
    case eSynBlobSend:
        x_DoBlobSend(task_info, conn);
        break;
    case eSynBlobGet:
        x_DoBlobGet(task_info, conn);
        break;
    case eSynNeedFinalize:
        x_DoFinalize(conn);
        break;
    default:
        abort();
    }
}

void
CNCActiveSyncControl::CmdFinished(ESyncResult res, ESynActionType action, CNCActiveHandler* conn)
{
    m_Lock.Lock();
    if (res == eSynOK) {
        switch (action) {
        case eSynActionRead:
            ++m_ReadOK;
            break;
        case eSynActionWrite:
            ++m_WriteOK;
            break;
        case eSynActionProlong:
            ++m_ProlongOK;
            break;
        case eSynActionRemove:
            ++m_DelOK;
            break;
        case eSynActionNone:
            break;
        }
    }
    else {
        switch (action) {
        case eSynActionRead:
            ++m_ReadERR;
            break;
        case eSynActionWrite:
            ++m_WriteERR;
            break;
        case eSynActionProlong:
            ++m_ProlongERR;
            break;
        case eSynActionRemove:
            ++m_DelERR;
            break;
        case eSynActionNone:
            break;
        }
    }

    if (res == eSynAborted  &&  m_Result != eSynNetworkError)
        m_Result = eSynAborted;
    else if (res != eSynOK)
        m_Result = res;

    if (m_StartedCmds == 0)
        abort();
    if (--m_StartedCmds == 0) {
        if (m_NextTask == eSynNeedFinalize) {
            m_StartedCmds = 1;
            if (m_Result == eSynNetworkError
                ||  !m_SlotSrv->peer->FinishSync(this))
            {
                m_StartedCmds = 0;
                m_NextTask = eSynNoTask;
                m_Result = eSynNetworkError;
                m_WaitCond.SignalAll();
            }
            else {
                if (--m_StartedCmds == 0)
                    m_WaitCond.SignalAll();
            }
        }
        else if (m_NextTask == eSynNoTask) {
            m_WaitCond.SignalAll();
        }
    }
    m_Lock.Unlock();
}

void
CNCActiveSyncControl::WakeUp(void)
{
    m_Lock.Lock();
    m_StartedCmds = 0;
    m_NextTask = eSynNoTask;
    m_WaitCond.SignalAll();
    m_Lock.Unlock();
}


CNCTimeThrottler*
CNCThrottler_Getter::CreateTlsObject(void)
{
    return new CNCTimeThrottler();
}

void
CNCThrottler_Getter::DeleteTlsObject(void* obj)
{
    delete (CNCTimeThrottler*)obj;
}



CNCTimeThrottler::CNCTimeThrottler(void)
    : m_PeriodStart(0),
      m_TotalTime(0),
      m_CurStart(0),
      m_Mode(eTotal)
{}

Uint4
CNCTimeThrottler::BeginTimeEvent(Uint8 server_id)
{
    Uint8 now = CNetCacheServer::GetPreciseTime();
    Uint4 to_wait = 0;
    if (m_PeriodStart == 0) {
        m_PeriodStart = now;
    }
    else {
        Uint8 diff = now - m_PeriodStart;
        Uint8 allowed = diff * CNCDistributionConf::GetMaxWorkerTimePct() / 100;
        Uint8 spent_srv = m_SrvTime[server_id];
        Uint8 per_srv_allowed = allowed / m_SrvTime.size();
        /*if (allowed < m_TotalTime) {
            to_wait = Uint4(m_TotalTime - allowed);
            if (spent_srv < per_srv_allowed)
                m_Mode = eByServer;
        }
        else if (m_Mode == eByServer) {
            if (spent_srv > per_srv_allowed) {
                to_wait = Uint4(m_SrvTime.size() * (spent_srv - per_srv_allowed));
            }
            else {
                Uint8 max_srv_time = 0;
                ITERATE(TTimeMap, it, m_SrvTime) {
                    max_srv_time = max(max_srv_time, it->second);
                }
                if (max_srv_time * m_SrvTime.size() <= allowed)
                    goto reset_period_start;
            }
        }*/
        if (per_srv_allowed < spent_srv) {
            to_wait = Uint4(m_SrvTime.size() * (spent_srv - per_srv_allowed));
        }
        else {
            Uint8 max_srv_time = 0;
            ITERATE(TTimeMap, it, m_SrvTime) {
                max_srv_time = max(max_srv_time, it->second);
            }
            if (max_srv_time * m_SrvTime.size() <= allowed  &&  diff > 30000000)
            {
//reset_period_start:
                m_PeriodStart = now;
                m_Mode = eTotal;
                m_TotalTime = 0;
                NON_CONST_ITERATE(TTimeMap, it, m_SrvTime) {
                    it->second = 0;
                }
            }
        }
    }
    m_CurStart = now;
    if (to_wait > 2 * kNCTimeTicksInSec)
        to_wait = 2 * kNCTimeTicksInSec;
    return to_wait;
}

void
CNCTimeThrottler::EndTimeEvent(Uint8 server_id, Uint8 adj_time)
{
    if (m_CurStart == 0)
        return;

    Uint8 now = CNetCacheServer::GetPreciseTime();
    Uint8 evt_time = now - m_CurStart;
    evt_time -= adj_time;
    m_TotalTime += evt_time;
    m_SrvTime[server_id] += evt_time;
    m_CurStart = 0;
}


Uint4
CNCPeriodicSync::BeginTimeEvent(Uint8 server_id)
{
    return s_TimeThrottler.GetObjPtr()->BeginTimeEvent(server_id);
}

void
CNCPeriodicSync::EndTimeEvent(Uint8 server_id, Uint8 adj_time)
{
    return s_TimeThrottler.GetObjPtr()->EndTimeEvent(server_id, adj_time);
}

END_NCBI_SCOPE
