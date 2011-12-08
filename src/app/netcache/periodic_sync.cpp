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
#include "mirroring.hpp"


BEGIN_NCBI_SCOPE


static TSyncSlotsMap    s_SlotsList;
static TSyncSlotsMap    s_SlotsMap;
static CSemaphore       s_WorkersSem(0, 1000000000);
static CSemaphore       s_CleanerSem(0, 1000000000);
static CSemaphore       s_MainsSem(0, 1000000000);
static CAtomicCounter   s_SyncOnInit;
static CAtomicCounter   s_WaitToOpenToClients;
static bool             s_NeedFinish = false;
static CRandom          s_Rnd(CRandom::TValue(time(NULL)));

typedef vector<CActiveSyncControl*> TSyncControls;
static TSyncControls    s_SyncControls;
typedef vector< CRef<CThread> >  TThreadsList;
static TThreadsList  s_SyncMains;
static TThreadsList  s_Workers;
static CRef<CThread> s_LogCleaner;

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
        if (this_srv->srv_data->srv_id == server_id) {
            slot_srv = this_srv;
            return;
        }
    }
}

static inline void
s_SetNextTime(Uint8& next_time, Uint8 value, bool add_random)
{
    if (add_random)
        value += s_Rnd.GetRand(0, kNCTimeTicksInSec);
    if (next_time < value)
        next_time = value;
}

static ESyncInitiateResult
s_StartSync(SSyncSlotData* slot_data, SSyncSlotSrv* slot_srv, bool is_passive)
{
    CFastMutexGuard g_slot(slot_data->lock);
    if (slot_data->cleaning  ||  slot_data->clean_required)
        return eServerBusy;

    CFastMutexGuard g_srv(slot_srv->lock);
    if (slot_srv->sync_started) {
        if (!is_passive  ||  !slot_srv->is_passive)
            return eCrossSynced;
        slot_srv->sync_started = false;
        --slot_data->cnt_sync_started;
    }

    SSyncSrvData* srv_data = slot_srv->srv_data;
    CFastMutexGuard g_srv_data(srv_data->lock);
    if (!is_passive
        &&  srv_data->cnt_active_syncs >= CNCDistributionConf::GetMaxSyncsOneServer())
    {
        return eServerBusy;
    }

    slot_srv->sync_started = true;
    slot_srv->is_passive = is_passive;
    if (!is_passive)
        ++srv_data->cnt_active_syncs;
    ++slot_srv->cur_sync_id;
    ++slot_data->cnt_sync_started;
    return eProceedWithEvents;
}

static void
s_SrvInitiallySynced(SSyncSrvData* srv_data)
{
    if (!srv_data->initially_synced) {
        INFO_POST("Initial sync for " << srv_data->srv_id << " completed");
        srv_data->initially_synced = true;
        s_SyncOnInit.Add(-1);
    }
}

static void
s_SlotsInitiallySynced(SSyncSrvData* srv_data, Uint2 cnt_slots)
{
    if (cnt_slots != 0  &&  srv_data->slots_to_init != 0) {
        if (cnt_slots != 1) {
            LOG_POST("Server " << srv_data->srv_id << " is out of reach");
        }
        srv_data->slots_to_init -= cnt_slots;
        if (srv_data->slots_to_init == 0) {
            s_SrvInitiallySynced(srv_data);
            if (s_WaitToOpenToClients.Add(-1) == 0)
                CNetCacheServer::InitialSyncComplete();
        }
    }
}

static void
s_StopSync(SSyncSlotData* slot_data,
           SSyncSlotSrv* slot_srv,
           Uint8 next_delay,
           bool  error_from_start)
{
    SSyncSrvData* srv_data = slot_srv->srv_data;
    CFastMutexGuard guard(srv_data->lock);
    Uint8 now = CNetCacheServer::GetPreciseTime();
    Uint8 next_time = now + next_delay;
    s_SetNextTime(slot_srv->next_sync_time, next_time, true);
    if (srv_data->first_nw_err_time == 0)
        s_SetNextTime(srv_data->next_sync_time, now, false);
    else {
        s_SetNextTime(srv_data->next_sync_time, next_time, true);
        if (error_from_start)
            s_SrvInitiallySynced(srv_data);
        if (now - srv_data->first_nw_err_time
            >= CNCDistributionConf::GetNetworkErrorTimeout())
        {
            s_SlotsInitiallySynced(srv_data, srv_data->slots_to_init);
        }
    }

    slot_srv->sync_started = false;
    if (!slot_srv->is_passive)
        --srv_data->cnt_active_syncs;
    if (slot_data->cnt_sync_started == 0)
        abort();
    if (--slot_data->cnt_sync_started == 0  &&  slot_data->clean_required)
        s_CleanerSem.Post();
}

static void
s_CommitSync(SSyncSlotData* slot_data, SSyncSlotSrv* slot_srv)
{
    if (slot_srv->is_by_blobs)
        slot_srv->was_blobs_sync = true;
    if (!slot_srv->made_initial_sync  &&  !CNetCacheServer::IsInitiallySynced())
    {
        slot_srv->made_initial_sync = true;
        SSyncSrvData* srv_data = slot_srv->srv_data;
        CFastMutexGuard guard(srv_data->lock);
        s_SlotsInitiallySynced(srv_data, 1);
    }
    s_StopSync(slot_data, slot_srv,
               CNCDistributionConf::GetPeriodicSyncInterval(), false);
}

static void
s_DoCleanLog(Uint2 slot)
{
    CRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    GetDiagContext().SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "clean")
                        .Print("slot", NStr::UIntToString(slot));
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
                     &&  s_SyncOnInit.Get() == 0
                     &&  CNCSyncLog::IsOverLimit(slot)
                     &&  CNetCacheServer::GetPreciseTime()
                                          - last_force_time[slot] >= min_period)
            {
                slot_data->clean_required = true;
            }
            slot_data->lock.Unlock();
        }
        s_CleanerSem.TryWait(interval, 0);
    }
}

static void
s_SyncWorkerMain(void)
{
    while (!s_NeedFinish) {
        bool had_tasks = false;
        for (Uint1 i = 0; i < s_SyncControls.size(); ++i) {
            CActiveSyncControl* ctrl = s_SyncControls[i];
            ESynTaskType task_type = ctrl->GetSynTaskType();
            if (task_type == eSynNoTask)
                continue;
            else if (task_type == eSynNeedFinalize)
                ctrl->FinalizeSync();
            else {
                ctrl->ExecuteSynTask(task_type);
                had_tasks = true;
            }
        }
        if (!had_tasks)
            s_WorkersSem.TryWait(1, 0);
    }
}

static void
s_ActiveSyncsMain(void)
{
    static CAtomicCounter_WithAutoInit ctrl_counter;
    Uint1 ctrl_idx = Uint1(ctrl_counter.Add(1)) - 1;
    CActiveSyncControl* ctrl = s_SyncControls[ctrl_idx];

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
                        rnd = s_Rnd.GetRand(0, numeric_limits<Uint2>::max());
                    }
                    while (slot_data->srvs.find(rnd) != slot_data->srvs.end());
                    slot_data->srvs[rnd] = slot_srv;
                }
                TSlotSrvsList srvs = slot_data->srvs;
                slot_data->lock.Unlock();
                ITERATE(TSlotSrvsList, it_srv, srvs) {
                    SSyncSlotSrv* slot_srv = it_srv->second;
                    SSyncSrvData* srv_data = slot_srv->srv_data;
                    Uint8 next_time = max(slot_srv->next_sync_time,
                                          srv_data->next_sync_time);
                    if (next_time <= now
                        &&  (s_SyncOnInit.Get() == 0
                             ||  !slot_srv->made_initial_sync)
                        &&  ctrl->DoPeriodicSync(slot_data, slot_srv))
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
                        s_StopSync(slot_data, slot_srv, 0, false);
                    }
                }
                else {
                    Uint8 next_time = max(slot_srv->next_sync_time,
                                          slot_srv->srv_data->next_sync_time);
                    min_next_time = min(min_next_time, next_time);
                }
                slot_srv->lock.Unlock();
            }
            slot_data->lock.Unlock();
            if (s_NeedFinish)
                break;
            now = CNetCacheServer::GetPreciseTime();
        }
        force_init_sync = s_SyncOnInit.Get() != 0  &&  !did_sync;
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
                wait_time = s_Rnd.GetRand(0, 10000);
            }

            Uint4 timeout_sec  = Uint4(wait_time / kNCTimeTicksInSec);
            Uint4 timeout_usec = Uint4(wait_time % kNCTimeTicksInSec);
            s_MainsSem.TryWait(timeout_sec, timeout_usec * 1000);
        }
    }
}



SSyncSrvData::SSyncSrvData(Uint8 srv_id_)
    : srv_id(srv_id_),
      next_sync_time(0),
      first_nw_err_time(0),
      slots_to_init(0),
      cnt_active_syncs(0),
      initially_synced(false)
{}

SSyncSlotData::SSyncSlotData(Uint2 slot_)
    : slot(slot_),
      cnt_sync_started(0),
      cleaning(false),
      clean_required(false)
{}

SSyncSlotSrv::SSyncSlotSrv(SSyncSrvData* srv_data_)
    : srv_data(srv_data_),
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

    Uint1 cnt_workers = CNCDistributionConf::GetCntSyncWorkers();
    for (Uint1 i = 0; i < cnt_workers; ++i) {
        s_Workers.push_back(CRef<CThread>(NewBGThread(&s_SyncWorkerMain)));
    }

    Uint4 cnt_to_sync = 0;
    const TNCPeerList& peers = CNCDistributionConf::GetPeers();
    ITERATE(TNCPeerList, it_peer, peers) {
        SSyncSrvData* srv_data = new SSyncSrvData(it_peer->first);
        const vector<Uint2>& commonSlots =
                        CNCDistributionConf::GetCommonSlots(it_peer->first);
        ITERATE(vector<Uint2>, it_slot, commonSlots) {
            SSyncSlotData* slot_data = s_SlotsMap[*it_slot];
            SSyncSlotSrv* slot_srv = new SSyncSlotSrv(srv_data);
            Uint2 sort_seed;
            do {
                sort_seed = s_Rnd.GetRand(0, numeric_limits<Uint2>::max());
            }
            while (slot_data->srvs.find(sort_seed) != slot_data->srvs.end());
            slot_data->srvs[sort_seed] = slot_srv;
        }
        if (commonSlots.size() == 0) {
            delete srv_data;
        }
        else {
            srv_data->slots_to_init = Uint2(commonSlots.size());
            ++cnt_to_sync;
        }
    }
    s_SyncOnInit.Set(cnt_to_sync);
    s_WaitToOpenToClients.Set(cnt_to_sync);

    Uint1 cnt_syncs = CNCDistributionConf::GetCntActiveSyncs();
    for (Uint1 i = 0; i < cnt_syncs; ++i) {
        s_SyncMains.push_back(CRef<CThread>(NewBGThread(&s_ActiveSyncsMain)));
        s_SyncControls.push_back(new CActiveSyncControl());
    }

    s_LogCleaner = NewBGThread(&s_LogCleanerMain);
    try {
        s_LogCleaner->Run();
        for (Uint1 i = 0; i < cnt_workers; ++i) {
            s_Workers[i]->Run();
        }
        for (Uint1 i = 0; i < cnt_syncs; ++i) {
            s_SyncMains[i]->Run();
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
    if (s_LogCleaner.IsNull()) {
        // Didn't have a chance to initialize
        return;
    }

    s_NeedFinish = true;
    s_CleanerSem.Post();
    try {
        s_LogCleaner->Join();
        s_WorkersSem.Post(CNCDistributionConf::GetCntSyncWorkers());
        NON_CONST_ITERATE(TThreadsList, it, s_Workers) {
            (*it)->Join();
        }
        NON_CONST_ITERATE(TSyncControls, it, s_SyncControls) {
            (*it)->WakeUp();
        }
        s_MainsSem.Post(CNCDistributionConf::GetCntActiveSyncs());
        NON_CONST_ITERATE(TThreadsList, it, s_SyncMains) {
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
    if (slot_srv == NULL
        ||  (s_SyncOnInit.Get() != 0
             &&  (slot_srv->made_initial_sync
                  ||  slot_data->cnt_sync_started != 0)))
    {
        return eServerBusy;
    }

    ESyncInitiateResult init_res = s_StartSync(slot_data, slot_srv, true);
    if (init_res != eProceedWithEvents)
        return init_res;

    slot_srv->started_cmds = 1;
    slot_srv->srv_data->first_nw_err_time = 0;
    *sync_id = slot_srv->cur_sync_id;
    bool records_available = CNCSyncLog::GetEventsList(server_id,
                                                       slot,
                                                       local_start_rec_no,
                                                       remote_start_rec_no,
                                                       events);
    if (records_available
        ||  (CNCSyncLog::GetLogSize() == 0  &&  slot_srv->was_blobs_sync))
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
        s_StopSync(slot_data, slot_srv, 0, false);
    }
}


CActiveSyncControl::CActiveSyncControl(void)
    : m_WaitSem(0, 1000000),
      m_HasTasks(false)
{}

bool
CActiveSyncControl::DoPeriodicSync(SSyncSlotData* slot_data,
                                   SSyncSlotSrv*  slot_srv)
{
    ESyncInitiateResult init_res = s_StartSync(slot_data, slot_srv, false);
    if (init_res != eProceedWithEvents)
        return false;

    m_SlotData = slot_data;
    m_SlotSrv = slot_srv;
    m_SrvId = slot_srv->srv_data->srv_id;
    m_Slot = slot_data->slot;
    m_Result = eSynOK;
    m_SlotSrv->is_by_blobs = false;
    Uint8 start_time = CNetCacheServer::GetPreciseTime();
    bool error_from_start = false;

    m_ReadOK = m_ReadERR = 0;
    m_WriteOK = m_WriteERR = 0;
    m_ProlongOK = m_ProlongERR = 0;
    m_DelOK = m_DelERR = 0;
    m_StartedCmds = 0;

    m_DiagCtx = new CRequestContext();
    m_DiagCtx->SetRequestID();
    GetDiagContext().SetRequestContext(m_DiagCtx);
    if (g_NetcacheServer->IsLogCmds()) {
        CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
        extra.Print("_type", "sync");
        extra.Print("srv_id", NStr::UInt8ToString(m_SrvId));
        extra.Print("slot", NStr::UIntToString(m_Slot));
        extra.Flush();
    }
    m_DiagCtx->SetRequestStatus(CNCMessageHandler::eStatus_OK);

    Uint8 local_start_rec_no;
    Uint8 remote_start_rec_no;
    CNCSyncLog::GetLastSyncedRecNo(m_SrvId, m_Slot,
                                   &local_start_rec_no,
                                   &remote_start_rec_no);

    TReducedSyncEvents remote_events;
    ITERATE(TNCBlobSumList, it, m_RemoteBlobs) {
        delete it->second;
    }
    m_RemoteBlobs.clear();
    init_res = CNetCacheServer::StartSyncWithPeer(m_SrvId, m_Slot,
                                                  local_start_rec_no,
                                                  remote_start_rec_no,
                                                  remote_events, m_RemoteBlobs);
    if (init_res == eNetworkError)
        error_from_start = true;
    else
        m_SlotSrv->srv_data->first_nw_err_time = 0;
    if (init_res != eProceedWithEvents  &&  init_res != eProceedWithBlobs) {
        GetDiagContext().Extra().Print("init_res", NStr::UIntToString(init_res));
    }

    Uint8 local_synced_rec_no = 0;
    Uint8 remote_synced_rec_no = 0;
    switch (init_res) {
    case eProceedWithEvents:
        x_PrepareSyncByEvents(local_start_rec_no,
                              remote_start_rec_no,
                              remote_events,
                              &local_synced_rec_no,
                              &remote_synced_rec_no);
        break;
    case eProceedWithBlobs:
        remote_synced_rec_no = remote_start_rec_no;
        x_PrepareSyncByBlobs(&local_synced_rec_no);
        break;
    case eNetworkError:
        m_Result = eSynNetworkError;
        break;
    case eCrossSynced:
        m_Result = eSynCrossSynced;
        break;
    case eServerBusy:
        m_Result = eSynServerBusy;
        break;
    }
    if (m_Result == eSynOK) {
        m_Lock.Lock();
        m_HasTasks = true;
        try {
            s_WorkersSem.Post(Uint2(s_Workers.size()));
        }
        catch (CCoreException&) {
            // ignore
        }
        while (m_HasTasks  &&  !s_NeedFinish) {
            m_Lock.Unlock();
            m_WaitSem.Wait();
            m_Lock.Lock();
        }
        if (m_HasTasks) {
            m_HasTasks = false;
            m_Result = eSynAborted;
        }
        m_Lock.Unlock();
    }
    ITERATE(TReducedSyncEvents, it, remote_events) {
        delete it->second.wr_or_rm_event;
        delete it->second.prolong_event;
    }
    switch (m_Result) {
    case eSynOK:
        CNCSyncLog::SetLastSyncRecNo(m_SrvId, m_Slot,
                                     local_synced_rec_no,
                                     remote_synced_rec_no);
        CNetCacheServer::SyncCommitOnPeer(m_SrvId, m_Slot,
                                          local_synced_rec_no,
                                          remote_synced_rec_no);
        CNetCacheServer::UpdateLastRecNo();
        break;
    case eSynAborted:
        CNetCacheServer::SyncCancelOnPeer(m_SrvId, m_Slot);
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
        if (m_SlotSrv->srv_data->first_nw_err_time == 0)
            m_SlotSrv->srv_data->first_nw_err_time = CNetCacheServer::GetPreciseTime();
        break;
    }

    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().Extra()
            .Print("sync", (m_SlotSrv->is_by_blobs? "blobs": "events"))
            .Print("r_ok", NStr::UInt8ToString(m_ReadOK))
            .Print("r_err", NStr::UInt8ToString(m_ReadERR))
            .Print("w_ok", NStr::UInt8ToString(m_WriteOK))
            .Print("w_err", NStr::UInt8ToString(m_WriteERR))
            .Print("p_ok", NStr::UInt8ToString(m_ProlongOK))
            .Print("p_err", NStr::UInt8ToString(m_ProlongERR))
            .Print("d_ok", NStr::UInt8ToString(m_DelOK))
            .Print("d_err", NStr::UInt8ToString(m_DelERR));
        GetDiagContext().PrintRequestStop();
    }
    m_DiagCtx.Reset();
    GetDiagContext().SetRequestContext(NULL);

    Uint8 end_time = CNetCacheServer::GetPreciseTime();
    Uint8 log_size = CNCSyncLog::GetLogSize();
    if (s_LogFile) {
        fprintf(s_LogFile,
                "%" NCBI_BIGCOUNT_FORMAT_SPEC ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%u,%" NCBI_BIGCOUNT_FORMAT_SPEC ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%d,%d,%" NCBI_BIGCOUNT_FORMAT_SPEC ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%" NCBI_BIGCOUNT_FORMAT_SPEC ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%" NCBI_BIGCOUNT_FORMAT_SPEC ",%" NCBI_BIGCOUNT_FORMAT_SPEC
                ",%" NCBI_BIGCOUNT_FORMAT_SPEC ",%u,%u\n",
                TNCBI_BigCount(CNCDistributionConf::GetSelfID()),
                TNCBI_BigCount(m_SrvId), m_Slot,
                TNCBI_BigCount(start_time), TNCBI_BigCount(end_time),
                TNCBI_BigCount(end_time - start_time),
                int(m_SlotSrv->is_by_blobs), m_Result, TNCBI_BigCount(log_size),
                TNCBI_BigCount(m_ReadOK), TNCBI_BigCount(m_ReadERR),
                TNCBI_BigCount(m_WriteOK), TNCBI_BigCount(m_WriteERR),
                TNCBI_BigCount(m_ProlongOK), TNCBI_BigCount(m_ProlongERR),
                Uint4(CNCMirroring::sm_TotalCopyRequests.Get()),
                Uint4(CNCMirroring::sm_CopyReqsRejected.Get()));
        fflush(s_LogFile);
    }

    CFastMutexGuard g_slot(m_SlotData->lock);
    CFastMutexGuard g_srv(m_SlotSrv->lock);
    if (m_Result == eSynOK) {
        s_CommitSync(m_SlotData, m_SlotSrv);
    }
    else {
        s_StopSync(m_SlotData, m_SlotSrv,
                   CNCDistributionConf::GetFailedSyncRetryDelay(),
                   error_from_start);
    }
    return m_Result == eSynOK;
}


void
CActiveSyncControl::x_PrepareSyncByEvents(Uint8 local_start_rec_no,
                                          Uint8 remote_start_rec_no,
                                          const TReducedSyncEvents& remote_events,
                                          Uint8* local_synced_rec_no,
                                          Uint8* remote_synced_rec_no)
{
    m_Events2Get.clear();
    m_Events2Send.clear();
    m_SlotSrv->is_by_blobs = false;
    if (CNCSyncLog::GetSyncOperations(m_SrvId, m_Slot,
                                      local_start_rec_no,
                                      remote_start_rec_no,
                                      remote_events,
                                      &m_Events2Get,
                                      &m_Events2Send,
                                      local_synced_rec_no,
                                      remote_synced_rec_no)
        ||  (CNCSyncLog::GetLogSize() == 0  &&  m_SlotSrv->was_blobs_sync))
    {
        m_CurGetEvent = m_Events2Get.begin();
        m_CurSendEvent = m_Events2Send.begin();
    }
    else {
        ENCPeerFailure res = CNetCacheServer::GetBlobsListFromPeer(
                                                m_SrvId, m_Slot, m_RemoteBlobs,
                                                *remote_synced_rec_no);
        if (res == ePeerBadNetwork)
            m_Result = eSynNetworkError;
        else if (res == ePeerNeedAbort)
            m_Result = eSynAborted;
        else
            x_PrepareSyncByBlobs(local_synced_rec_no);
    }
}

void
CActiveSyncControl::x_PrepareSyncByBlobs(Uint8* local_synced_rec_no)
{
    *local_synced_rec_no = CNCSyncLog::GetCurrentRecNo(m_Slot);

    ITERATE(TNCBlobSumList, it, m_LocalBlobs) {
        delete it->second;
    }
    m_LocalBlobs.clear();
    m_SlotSrv->is_by_blobs = true;
    g_NCStorage->GetFullBlobsList(m_Slot, m_LocalBlobs);

    m_CurLocalBlob = m_LocalBlobs.begin();
    m_CurRemoteBlob = m_RemoteBlobs.begin();
}

ESynTaskType
CActiveSyncControl::GetSynTaskType(void)
{
    m_Lock.Lock();
    if (!m_HasTasks) {
        m_Lock.Unlock();
        return eSynNoTask;
    }

    if (m_SlotData->clean_required  &&  m_Result != eSynNetworkError)
        m_Result = eSynAborted;
    if (m_Result == eSynNetworkError  ||  m_Result == eSynAborted)
        return eSynNeedFinalize;

    if (!m_SlotSrv->is_by_blobs) {
        if (m_CurSendEvent != m_Events2Send.end())
            return eSynEventSend;
        else if (m_CurGetEvent != m_Events2Get.end())
            return eSynEventGet;
        else
            return eSynNeedFinalize;
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
                    return eSynBlobUpdateOur;
                else
                    return eSynBlobUpdatePeer;
            }
            else if (m_CurLocalBlob->first < m_CurRemoteBlob->first)
                return eSynBlobSend;
            else
                return eSynBlobGet;
        }
        // Process the tails of the lists
        else if (m_CurLocalBlob != m_LocalBlobs.end())
            return eSynBlobSend;
        else if (m_CurRemoteBlob != m_RemoteBlobs.end())
            return eSynBlobGet;
        else
            return eSynNeedFinalize;
    }
}

void
CActiveSyncControl::x_DoEventSend(ENCPeerFailure& task_res,
                                  ESynActionType& action)
{
    SNCSyncEvent* event = *m_CurSendEvent;
    ++m_CurSendEvent;
    m_Lock.Unlock();

    switch (event->event_type) {
    case eSyncWrite:
        task_res = CNetCacheServer::SyncWriteBlobToPeer(m_SrvId, m_Slot, event);
        action = eSynActionWrite;
        break;
    case eSyncProlong:
        task_res = CNetCacheServer::SyncProlongBlobOnPeer(m_SrvId, m_Slot, event);
        action = eSynActionProlong;
        break;
    }
}

void
CActiveSyncControl::x_DoEventGet(ENCPeerFailure& task_res,
                                 ESynActionType& action)
{
    SNCSyncEvent* event = *m_CurGetEvent;
    ++m_CurGetEvent;
    m_Lock.Unlock();

    switch (event->event_type) {
    case eSyncWrite:
        task_res = CNetCacheServer::SyncGetBlobFromPeer(m_SrvId, m_Slot, event);
        action = eSynActionRead;
        break;
    case eSyncProlong:
        task_res = CNetCacheServer::SyncProlongOurBlob(m_SrvId, m_Slot, event);
        action = eSynActionProlong;
        break;
    }
}

void
CActiveSyncControl::x_DoBlobUpdateOur(ENCPeerFailure& task_res,
                                      ESynActionType& action)
{
    string key(m_CurRemoteBlob->first);
    if (m_CurLocalBlob->second->isSameData(*m_CurRemoteBlob->second)) {
        const SNCBlobSummary* blob_sum = m_CurRemoteBlob->second;
        ++m_CurLocalBlob;
        ++m_CurRemoteBlob;
        m_Lock.Unlock();

        task_res = CNetCacheServer::SyncProlongOurBlob(m_SrvId, m_Slot,
                                                       key, *blob_sum);
        action = eSynActionProlong;
    }
    else {
        Uint8 create_time = m_CurRemoteBlob->second->create_time;
        ++m_CurLocalBlob;
        ++m_CurRemoteBlob;
        m_Lock.Unlock();

        task_res = CNetCacheServer::SyncGetBlobFromPeer(m_SrvId, m_Slot,
                                                        key, create_time);
        action = eSynActionRead;
    }
}

void
CActiveSyncControl::x_DoBlobUpdatePeer(ENCPeerFailure& task_res,
                                       ESynActionType& action)
{
    string key(m_CurRemoteBlob->first);
    if (m_CurLocalBlob->second->isSameData(*m_CurRemoteBlob->second)) {
        const SNCBlobSummary* blob_sum = m_CurLocalBlob->second;
        ++m_CurLocalBlob;
        ++m_CurRemoteBlob;
        m_Lock.Unlock();

        task_res = CNetCacheServer::SyncProlongBlobOnPeer(m_SrvId, m_Slot,
                                                          key, *blob_sum);
        action = eSynActionProlong;
    }
    else {
        ++m_CurLocalBlob;
        ++m_CurRemoteBlob;
        m_Lock.Unlock();

        task_res = CNetCacheServer::SyncWriteBlobToPeer(m_SrvId, m_Slot, key);
        action = eSynActionWrite;
    }
}

void
CActiveSyncControl::x_DoBlobSend(ENCPeerFailure& task_res,
                                 ESynActionType& action)
{
    string key(m_CurLocalBlob->first);
    ++m_CurLocalBlob;
    m_Lock.Unlock();

    task_res = CNetCacheServer::SyncWriteBlobToPeer(m_SrvId, m_Slot, key);
    action = eSynActionWrite;
}

void
CActiveSyncControl::x_DoBlobGet(ENCPeerFailure& task_res,
                                ESynActionType& action)
{
    string key(m_CurRemoteBlob->first);
    Uint8 create_time = m_CurRemoteBlob->second->create_time;
    ++m_CurRemoteBlob;
    m_Lock.Unlock();

    task_res = CNetCacheServer::SyncGetBlobFromPeer(m_SrvId, m_Slot,
                                                    key, create_time);
    action = eSynActionRead;
}

void
CActiveSyncControl::ExecuteSynTask(ESynTaskType task_type)
{
    ++m_StartedCmds;
    GetDiagContext().SetRequestContext(m_DiagCtx);

    ENCPeerFailure task_res = ePeerBadNetwork;
    ESynActionType action = eSynActionRead;
    switch (task_type) {
    case eSynEventSend:
        x_DoEventSend(task_res, action);
        break;
    case eSynEventGet:
        x_DoEventGet(task_res, action);
        break;
    case eSynBlobUpdateOur:
        x_DoBlobUpdateOur(task_res, action);
        break;
    case eSynBlobUpdatePeer:
        x_DoBlobUpdatePeer(task_res, action);
        break;
    case eSynBlobSend:
        x_DoBlobSend(task_res, action);
        break;
    case eSynBlobGet:
        x_DoBlobGet(task_res, action);
        break;
    case eSynNoTask:
    case eSynNeedFinalize:
        abort();
    }

    m_Lock.Lock();
    --m_StartedCmds;
    if (task_res == ePeerActionOK) {
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
        }
    }

    if (task_res == ePeerBadNetwork)
        m_Result = eSynNetworkError;
    else if (task_res == ePeerNeedAbort  &&  m_Result != eSynNetworkError)
        m_Result = eSynAborted;

    m_Lock.Unlock();
}

void
CActiveSyncControl::FinalizeSync(void)
{
    if (m_StartedCmds == 0) {
        m_HasTasks = false;
        m_WaitSem.Post();
    }
    m_Lock.Unlock();
}

void
CActiveSyncControl::WakeUp(void)
{
    m_WaitSem.Post();
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
