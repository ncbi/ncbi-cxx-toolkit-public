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
 */

#include "nc_pch.hpp"
#include <corelib/request_ctx.hpp>

#include "netcached.hpp"
#include "distribution_conf.hpp"
#include "nc_storage_blob.hpp"
#include "nc_storage.hpp"
#include "storage_types.hpp"
#include "nc_stat.hpp"
#include <set>

BEGIN_NCBI_SCOPE

struct SWriteBackData
{
    CMiniMutex lock;
    size_t cur_size;
    size_t releasable_size;
    size_t releasing_size;
    vector<SNCBlobVerData*> *to_add_list;
    vector<SNCBlobVerData*> *to_del_list;

    SWriteBackData(void);
};

extern Uint4 s_TaskPriorityWbMemRelease;
static size_t s_WBSoftSizeLimit = NCBI_CONST_UINT8(2000000000);
static size_t s_WBHardSizeLimit = NCBI_CONST_UINT8(3000000000);
static int s_WBWriteTimeout = 1000;
static int s_WBWriteTimeout2 = 1000;
static Uint2 s_WBFailedWriteDelay = 2;

static ssize_t s_WBCurSize = 0;
static ssize_t s_WBReleasableSize = 0;
static ssize_t s_WBReleasingSize = 0;
static SWriteBackData* s_WBData = NULL;
static CWriteBackControl* s_WBControl = NULL;
static TVerDataMap* s_VersMap = NULL;

static CMiniMutex s_ConsListLock;
static Uint4 s_CntConsumers = 0;
static TSrvConsList s_ConsList;
vector<SNCBlobVerData*> s_WBToAddList;
vector<SNCBlobVerData*> s_WBToDelList;
/*
static Uint8 s_CntMgrs = 0;
static Uint8 s_CntVers = 0;
*/
typedef map<string, Uint8> TForgets;
static TForgets s_Forgets;
static TForgets s_ForgetKeys;
static Uint8 s_LatestPurge = 0;
static CBulkCleaner* s_BulkCleaner = NULL;

// for PutFailed/PutSucceeded
static bool s_FailMonitor = false;
static CMiniMutex s_FailedListLock;
static int s_FailedReserve=0;
static CAtomicCounter s_FailedCounter;
static set<string> s_FailedKeys;

static Uint8 s_AnotherServerMain = 0;

static Uint8 s_BlobSync = 0;
static Uint8 s_BlobSyncTDiff = 0;
static Uint8 s_BlobSyncMaxTDiff = 0;

static Uint8 s_BlobNotify = 0;
static Uint8 s_BlobNotifyTDiff = 0;
static Uint8 s_BlobNotifyMaxTDiff = 0;


static const size_t kVerManagerSize = sizeof(CNCBlobVerManager)
                                      + sizeof(CCurVerReader);
static const size_t kDefChunkMapsSize
                = sizeof(SNCChunkMaps)
                  + (kNCMaxBlobMapsDepth + 1)
                    * (sizeof(SNCChunkMapInfo)
                       + (kNCMaxChunksInMap - 1) * sizeof(SNCDataCoord));




static bool
s_IsCurVerOlder(const SNCBlobVerData* cur_ver, const SNCBlobVerData* new_ver)
{
    bool res = true;
    if (cur_ver) {
        if (cur_ver->create_time != new_ver->create_time) {
            res = cur_ver->create_time < new_ver->create_time;
        } else if (cur_ver->create_server != new_ver->create_server) {
            res = cur_ver->create_server < new_ver->create_server;
        } else if (cur_ver->create_id != new_ver->create_id) {
            res = cur_ver->create_id < new_ver->create_id;
        } else if (cur_ver->expire != new_ver->expire) {
            res = cur_ver->expire < new_ver->expire;
        } else if (cur_ver->ver_expire != new_ver->ver_expire) {
            res = cur_ver->ver_expire < new_ver->ver_expire;
        } else {
            res = cur_ver->dead_time < new_ver->dead_time;
        }
    }
    return res;
}

Uint8 GetWBSoftSizeLimit(void) {
    return s_WBSoftSizeLimit;
}
Uint8 GetWBHardSizeLimit(void) {
    return s_WBHardSizeLimit;
}
int GetWBWriteTimeout(void) {
    return s_WBWriteTimeout;
}
int GetWBFailedWriteDelay(void) {
    return s_WBFailedWriteDelay;
}

void
SetWBSoftSizeLimit(Uint8 limit)
{
    s_WBSoftSizeLimit = size_t(limit);
}

void
SetWBHardSizeLimit(Uint8 limit)
{
    s_WBHardSizeLimit = max(size_t(limit), s_WBSoftSizeLimit + 1000000000);
}

void
SetWBWriteTimeout(int timeout1, int timeout2)
{
    s_WBWriteTimeout = timeout1;
    s_WBWriteTimeout2 = timeout2;
}
void
SetWBInitialSyncComplete(void)
{
    s_WBWriteTimeout = s_WBWriteTimeout2;
}

void
SetWBFailedWriteDelay(int delay)
{
    s_WBFailedWriteDelay = Uint2(delay);
}

static inline SWriteBackData*
s_GetWBData(void)
{
    return &s_WBData[CTaskServer::GetCurThreadNum()];
}

static inline size_t
s_CalcVerDataSize(SNCBlobVerData* ver_data)
{
   return sizeof(*ver_data) + ver_data->chunks.capacity() * sizeof(char*);
}

static size_t
s_CalcChunkMapsSize(Uint2 map_size)
{
    if (map_size == kNCMaxChunksInMap)
        return kDefChunkMapsSize;
    else {
        return sizeof(SNCChunkMaps)
               + (kNCMaxBlobMapsDepth + 1)
                  * (sizeof(SNCChunkMapInfo) + (map_size - 1) * sizeof(SNCDataCoord));
    }
}

static void
s_AddCurrentMem(size_t mem_size)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->cur_size += mem_size;
    wb_data->lock.Unlock();
}

static void
s_SubCurrentMem(size_t mem_size)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->cur_size -= mem_size;
    wb_data->lock.Unlock();
}

static void
s_AddReleasableMem(SNCBlobVerData* ver_data,
                   size_t add_releasable,
                   size_t sub_releasing)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->releasable_size += add_releasable;
    wb_data->releasing_size -= sub_releasing;
    if (ver_data) {
        wb_data->to_add_list->push_back(ver_data);
    }
    wb_data->lock.Unlock();
}

static void
s_SubReleasableMem(size_t mem_size)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->releasable_size -= mem_size;
    wb_data->lock.Unlock();
}

static void
s_AddReleasingMem(size_t add_releasing, size_t sub_releasable)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->releasing_size += add_releasing;
    wb_data->releasable_size -= sub_releasable;
    wb_data->lock.Unlock();
}

static void
s_SubReleasingMem(size_t mem_size)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->releasing_size -= mem_size;
    wb_data->lock.Unlock();
}

static void
s_ScheduleVerDelete(SNCBlobVerData* ver_data)
{
    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->to_del_list->push_back(ver_data);
    ver_data->delete_scheduled = true;
    wb_data->lock.Unlock();
}

static char*
s_AllocWriteBackMem(Uint4 mem_size, CSrvTransConsumer* consumer)
{
    char* mem = NULL;
    if (s_WBCurSize <= s_WBReleasingSize
        || (size_t)(s_WBCurSize - s_WBReleasingSize) + mem_size < s_WBHardSizeLimit
        ||  s_WBReleasableSize < (ssize_t)mem_size
        ||  CTaskServer::IsInShutdown())
    {
        mem = (char*)malloc(mem_size);
        if (mem) {
            s_AddCurrentMem(mem_size);
        }
    }
    else {
        s_ConsListLock.Lock();
        s_ConsList.push_front(*consumer);
        consumer->m_TransFinished = false;
        ++s_CntConsumers;
        s_ConsListLock.Unlock();
    }
    return mem;
}

static void
s_NotifyConsumers(void)
{
    TSrvConsList cons_list;

    s_ConsListLock.Lock();
    s_ConsList.swap(cons_list);
    s_ConsListLock.Unlock();

    while (!cons_list.empty()) {
        CSrvTransConsumer* consumer = &cons_list.front();
        cons_list.pop_front();
        consumer->m_TransFinished = true;
        consumer->SetRunnable();
    }
}

static char*
s_ReallocWriteBackMem(char* mem, Uint4 old_size, Uint4 new_size)
{
    Uint4 diff = old_size - new_size;
    if (diff != 0) {
        mem = (char*)realloc(mem, new_size);
        s_SubCurrentMem(diff);
    }
    return mem;
}

static void
s_FreeWriteBackMem(char* mem, Uint4 mem_size, Uint4 sub_releasing)
{
    free(mem);

    SWriteBackData* wb_data = s_GetWBData();
    wb_data->lock.Lock();
    wb_data->cur_size -= mem_size;
    wb_data->releasing_size -= sub_releasing;
    wb_data->lock.Unlock();
}

static void
s_ReleaseMemory(size_t soft_limit)
{
    if (s_WBCurSize - s_WBReleasingSize <= (ssize_t)soft_limit)
        return;

    size_t to_free = s_WBCurSize - s_WBReleasingSize - soft_limit;
    size_t freed = 0;
    while (freed < to_free  &&  !s_VersMap->empty()) {
        TVerDataMap::iterator it = s_VersMap->begin();
        SNCBlobVerData* ver_data = &*it;
        s_VersMap->erase(it);

        if (ver_data->releasable_mem == 0)
            continue;

        int access_time = ACCESS_ONCE(ver_data->last_access_time);
        if (access_time != ver_data->saved_access_time) {
            ver_data->saved_access_time = access_time;
            s_VersMap->insert_equal(*ver_data);
            continue;
        }
        freed += ver_data->RequestMemRelease();
    }
    s_WBReleasingSize += freed;
    s_WBReleasableSize -= freed;
}

static void
s_TransferVerList(vector<SNCBlobVerData*>& from_list,
                  vector<SNCBlobVerData*>& to_list)
{
    if (from_list.size() == 0)
        return;

    size_t prev_size = to_list.size();
    to_list.resize(prev_size + from_list.size(), NULL);
    memcpy(&to_list[prev_size], &from_list[0],
           from_list.size() * sizeof(SNCBlobVerData*));
    from_list.resize(0);
}

static void
s_CollectWBData(SWriteBackData* wb_data)
{
    vector<SNCBlobVerData*> *next_add = new vector<SNCBlobVerData*>;
    vector<SNCBlobVerData*> *next_del = new vector<SNCBlobVerData*>;
    vector<SNCBlobVerData*> *prev_add = nullptr, *prev_del = nullptr;

    wb_data->lock.Lock();
    s_WBCurSize += wb_data->cur_size;
    wb_data->cur_size = 0;
    s_WBReleasableSize += wb_data->releasable_size;
    wb_data->releasable_size = 0;
    s_WBReleasingSize += wb_data->releasing_size;
    wb_data->releasing_size = 0;

    if (next_add) {
        prev_add = wb_data->to_add_list;
        wb_data->to_add_list = next_add;
    } else {
        s_TransferVerList(*(wb_data->to_add_list), s_WBToAddList);
    }
    if (next_del) {
        prev_del = wb_data->to_del_list;
        wb_data->to_del_list = next_del;
    } else {
        s_TransferVerList(*(wb_data->to_del_list), s_WBToDelList);
    }

    wb_data->lock.Unlock();

    if (prev_add) {
        s_TransferVerList(*prev_add, s_WBToAddList);
        delete prev_add;
    }
    if (prev_del) {
        s_TransferVerList(*prev_del, s_WBToDelList);
        delete prev_del;
    }
}

static void
s_ProcessWBAddDel(Uint4 was_del_size)
{
// add new verdata into map (s_VersMap) sorted by
// last seen access time (ver_data->saved_access_time)
    for (Uint4 i = 0; i < s_WBToAddList.size(); ++i) {
        SNCBlobVerData* ver_data = s_WBToAddList[i];
        if (!ver_data->is_linked()  &&  ver_data->releasable_mem != 0
            &&  !ver_data->delete_scheduled)
        {
            ver_data->saved_access_time = ver_data->last_access_time;
            s_VersMap->insert_equal(*ver_data);
        }
    }
    s_WBToAddList.clear();
// remove from map and delete those about to be deleted
//  (SNCBlobVerData request deletion)
    for (Uint4 i = 0; i < was_del_size; ++i) {
        SNCBlobVerData* ver_data = s_WBToDelList[i];
        if (ver_data->is_linked()) {
            s_VersMap->erase(s_VersMap->iterator_to(*ver_data));
        }
        s_WBReleasingSize -= ver_data->meta_mem;
        s_WBCurSize -= ver_data->meta_mem;
        ver_data->Terminate();
    }
    Uint4 new_size = Uint4(s_WBToDelList.size() - was_del_size);
    if (was_del_size != 0  &&  new_size != 0) {
        memmove(&s_WBToDelList[0], &s_WBToDelList[was_del_size],
                new_size * sizeof(SNCBlobVerData*));
    }
    s_WBToDelList.resize(new_size, NULL);
}



SWriteBackData::SWriteBackData(void)
    : cur_size(0),
      releasable_size(0),
      releasing_size(0)
{
    to_add_list = new vector<SNCBlobVerData*>;
    to_del_list = new vector<SNCBlobVerData*>;
}


CWriteBackControl::CWriteBackControl(void)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CWriteBackControl";
#endif
}

CWriteBackControl::~CWriteBackControl(void)
{}

void
CWriteBackControl::Initialize(void)
{
    s_VersMap = new TVerDataMap();
    s_WBData = new SWriteBackData[CTaskServer::GetMaxRunningThreads()];
    s_WBControl = new CWriteBackControl();
    s_WBControl->SetRunnable();

    s_BulkCleaner = new CBulkCleaner;
    s_BulkCleaner->SetRunnable();
}

void
CWriteBackControl::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// monitor write-back cache statistics (where blobs are stored before being saved on disk)

    if (CTaskServer::IsInShutdown())
        return;

    ssize_t was_cur_size = s_WBCurSize;
    Uint4 was_del_size = Uint4(s_WBToDelList.size());

    Uint4 cnt_datas = CTaskServer::GetMaxRunningThreads();
    for (Uint4 i = 0; i < cnt_datas; ++i) {
// collect stat from all threads
        s_CollectWBData(&s_WBData[i]);
    }

// delete SNCBlobVerData
    s_ProcessWBAddDel(was_del_size);

    size_t cur_size_change = 0;
    if (s_WBCurSize > was_cur_size)
        cur_size_change = s_WBCurSize - was_cur_size;
    size_t soft_limit = s_WBSoftSizeLimit;
    if (cur_size_change < soft_limit)
        soft_limit -= cur_size_change;
    Uint4 cnt_cons = ACCESS_ONCE(s_CntConsumers);
    if (soft_limit > cnt_cons * kNCMaxBlobChunkSize)
        soft_limit -= cnt_cons * kNCMaxBlobChunkSize;

    s_ReleaseMemory(soft_limit);
    s_NotifyConsumers();

    RunAfter(1);
}

void
CWriteBackControl::ReadState(SNCStateStat& state)
{
    state.wb_size = (ssize_t(s_WBCurSize) > 0? s_WBCurSize: 0);
    state.wb_releasable = (ssize_t(s_WBReleasableSize) > 0? s_WBReleasableSize: 0);
    state.wb_releasing = (ssize_t(s_WBReleasingSize) > 0? s_WBReleasingSize: 0);

    state.cnt_another_server_main = s_AnotherServerMain;
    Uint8 prev = s_BlobSync;
    if (prev != 0) {
        state.avg_tdiff_blobcopy = s_BlobSyncTDiff / prev;
        state.max_tdiff_blobcopy = s_BlobSyncMaxTDiff;
    } else {
        state.avg_tdiff_blobcopy = 0;
        state.max_tdiff_blobcopy = 0;
    }
    prev = s_BlobNotify;
    if (prev != 0) {
        state.avg_tdiff_blobnotify = s_BlobNotifyTDiff / prev;
        state.max_tdiff_blobnotify = s_BlobNotifyMaxTDiff;
    } else {
        state.avg_tdiff_blobnotify = 0;
        state.max_tdiff_blobnotify = 0;
    }
}

void CWriteBackControl::AnotherServerMain(void)
{
    AtomicAdd( s_AnotherServerMain, 1);
}

void CWriteBackControl::StartSyncBlob(Uint8 create_time)
{
    if (!CNCServer::IsInitiallySynced()) {
        return;
    }
    Uint8 tdiff = CSrvTime::Current().AsUSec() - create_time;
    Uint8 prev = s_BlobSync;
    Uint8 prevdiff = s_BlobSyncTDiff;
    AtomicAdd(s_BlobSync, 1);
    AtomicAdd(s_BlobSyncTDiff, tdiff);
    if (prev > s_BlobSync || prevdiff > s_BlobSyncTDiff) {
        s_BlobSync = 0;
        s_BlobSyncTDiff = 0;
        s_BlobSyncMaxTDiff = 0;
    }
    if (s_BlobSyncMaxTDiff < tdiff) {
        s_BlobSyncMaxTDiff = tdiff;
    }
}

void
CWriteBackControl::RecordNotifyUpdateBlob(Uint8 update_received)
{
    if (!CNCServer::IsInitiallySynced()) {
        return;
    }
    Uint8 tdiff = CSrvTime::Current().AsUSec() - update_received;
    Uint8 prev = s_BlobNotify;
    Uint8 prevdiff = s_BlobNotifyTDiff;
    AtomicAdd(s_BlobNotify, 1);
    AtomicAdd(s_BlobNotifyTDiff, tdiff);
    if (prev > s_BlobNotify || prevdiff > s_BlobNotifyTDiff) {
        s_BlobNotify = 0;
        s_BlobNotifyTDiff = 0;
        s_BlobNotifyMaxTDiff = 0;
    }
    if (s_BlobNotifyMaxTDiff < tdiff) {
        s_BlobNotifyMaxTDiff = tdiff;
    }
}

void
CWriteBackControl::ResetStatCounters(void)
{
    s_BlobSync = 0;
    s_BlobSyncTDiff = 0;
    s_BlobSyncMaxTDiff = 0;
}


void
CNCBlobVerManager::x_DeleteCurVersion(void)
{
    m_CacheData->coord.clear();
    m_CacheData->dead_time = 0;
    CNCBlobStorage::ChangeCacheDeadTime(m_CacheData);
    m_CacheData->expire = 0;
    if (m_CurVersion) {
        m_CurVersion->SetNotCurrent();
        m_CurVersion.Reset();
    }
}

void
CNCBlobVerManager::DeleteVersion(const SNCBlobVerData* ver_data)
{
    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->Get_ver_mgr() == this);
    if (m_CurVersion == ver_data)
        x_DeleteCurVersion();
    m_CacheData->lock.Unlock();
}

void
CNCBlobVerManager::DeleteDeadVersion(int /*cut_time*/)
{
    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->Get_ver_mgr() == this);
#if 0
    CSrvRef<SNCBlobVerData> cur_ver(m_CurVersion);
    if (m_CurVersion) {
        if (m_CurVersion->dead_time <= cut_time)
            x_DeleteCurVersion();
    }
    else if (!m_CacheData->coord.empty()  &&  m_CacheData->dead_time <= cut_time) {
        x_DeleteCurVersion();
    }
#else
    x_DeleteCurVersion();
#endif
    m_CacheData->lock.Unlock();
}

CNCBlobVerManager::CNCBlobVerManager(Uint2         time_bucket,
                                     const string& key,
                                     SNCCacheData* cache_data)
    : m_TimeBucket(time_bucket),
      m_NeedReleaseMem(false),
      m_NeedAbort(false),
      m_CacheData(cache_data),
      m_CurVerReader(new CCurVerReader(this)),
      m_Key(key)
{
    CNCBlobStorage::ReferenceCacheData(m_CacheData);
//    m_CacheData->ver_mgr = this;
    if (!AtomicCAS(cache_data->ver_mgr, nullptr, this)) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheFailedMgrAttach,"CNCBlobVerManager ctor");
#endif
    }
    //AtomicAdd(s_CntMgrs, 1);
    //Uint8 cnt = AtomicAdd(s_CntMgrs, 1);
    //INFO("CNCBlobVerManager, cnt=" << cnt);
#if __NC_TASKS_MONITOR
    m_TaskName = "CNCBlobVerManager";
#endif
    SetPriority(s_TaskPriorityWbMemRelease);
}

CNCBlobVerManager::~CNCBlobVerManager(void)
{
    //AtomicSub(s_CntMgrs, 1);
    //Uint8 cnt = AtomicSub(s_CntMgrs, 1);
    //INFO("~CNCBlobVerManager, cnt=" << cnt);
}

void
CNCBlobVerManager::ObtainReference(void)
{
    AddReference();
    if (ReferencedOnlyOnce()) {
        m_NeedReleaseMem = false;
        if (m_CurVersion)
            m_CurVersion->SetNonReleasable();
    }
}

CNCBlobVerManager*
CNCBlobVerManager::Get(Uint2         time_bucket,
                       const string& key,
                       SNCCacheData* cache_data,
                       bool          for_new_version)
{
    cache_data->lock.Lock();
    CNCBlobVerManager* mgr = cache_data->Get_ver_mgr();
    if (mgr) {
        mgr->ObtainReference();
    }
    else if (for_new_version  ||  !cache_data->coord.empty()) {
        mgr = new CNCBlobVerManager(time_bucket, key, cache_data);
        mgr->AddReference();
        s_AddCurrentMem(kVerManagerSize);
    }
    cache_data->lock.Unlock();

    return mgr;
}

void
CNCBlobVerManager::Release(void)
{
    SNCCacheData* cache_data = m_CacheData;
    cache_data->lock.Lock();
    // DeleteThis below should be executed under the lock
    RemoveReference();
    cache_data->lock.Unlock();
}

void
CNCBlobVerManager::DeleteThis(void)
{
    if (m_CurVersion)
        m_CurVersion->SetReleasable();
    SetRunnable();
}

void
CNCBlobVerManager::x_ReleaseMgr(void)
{
    SNCCacheData*  cache_data = m_CacheData;
    m_CacheData = nullptr;
    if (cache_data) {
//        cache_data->ver_mgr = NULL;
        if (!AtomicCAS(cache_data->ver_mgr, this, nullptr)) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheFailedMgrDetach,"x_ReleaseMgr");
if (cache_data->ver_mgr != nullptr) {
CNCAlerts::Register(CNCAlerts::eDebugCacheWrongMgr,"x_ReleaseMgr");
}
#endif
        }
        if (m_CurVersion) {
            if (!m_NeedAbort || m_Key == cache_data->key) {
                if (!cache_data->coord.empty() && cache_data->coord != m_CurVersion->coord) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheDeleted1,"x_ReleaseMgr");
#endif
                    CExpiredCleaner::x_DeleteData(cache_data);
                }
                cache_data->coord = m_CurVersion->coord;
            }
            m_CurVersion.Reset();
        }
        else {
            s_SubCurrentMem(kVerManagerSize);
        }

        if (cache_data->dead_time == 0 && !cache_data->coord.empty()) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheDeleted2,"x_ReleaseMgr");
#endif
            CExpiredCleaner::x_DeleteData(cache_data);
        }
        cache_data->lock.Unlock();
        if (!m_NeedAbort) {
            CNCBlobStorage::ReleaseCacheData(cache_data);
        }
    } else {
        s_SubCurrentMem(kVerManagerSize);
    }
    if (!Referenced()) {
        m_CurVerReader->Terminate();
        m_CurVerReader = nullptr;
        Terminate();
    }
}

void
CNCBlobVerManager::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// CNCBlobAccessor has a reference to this one.
// for each blob key there is only one CNCBlobVerManager.

    CSrvRef<SNCBlobVerData> cur_ver;

    m_CacheData->lock.Lock();

    if (m_CacheData->Get_ver_mgr() != this) {
        m_NeedAbort = true;
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheWrong,"CNCBlobVerManager::ExecuteSlice");
#endif
        x_ReleaseMgr();
        return;
    }

    cur_ver = m_CurVersion;
    if (!cur_ver  &&  !Referenced()) {
        x_ReleaseMgr();
        return;
    }
    if (!cur_ver) {
        m_CacheData->lock.Unlock();
        return;
    }

// initially, blob is in memory
// check if it is time to be saved onto disk
    int cur_time = CSrvTime::CurSecs();
    int write_time = ACCESS_ONCE(cur_ver->need_write_time);

#if 1
    if (cur_time + 60  >= cur_ver->dead_time) {
        write_time = 0;
    }
#endif

    if (write_time != 0) {
        m_CacheData->lock.Unlock();
        if (write_time <= cur_time  ||  CTaskServer::IsInShutdown()) {
            if (!CNCBlobStorage::IsAbandoned()) {
                CNCBlobStorage::ReferenceCacheData(m_CacheData);
                cur_ver->RequestDataWrite();
            }
        } else {
            RunAfter(write_time - cur_time);
        }
        return;
    }

// if requested, remove metadata from memory
    if (m_NeedReleaseMem  &&  !Referenced()) {
        x_ReleaseMgr();
        return;
    }
    m_NeedReleaseMem = false;

    m_CacheData->lock.Unlock();
}

void CNCBlobVerManager::RevokeDataWrite()
{
    CNCBlobStorage::ReleaseCacheData(m_CacheData);
}

void CNCBlobVerManager::DataWritten(void)
{
    if (m_CurVersion) {
        m_CacheData->lock.Lock();
        if (!m_CacheData->coord.empty() && m_CacheData->coord != m_CurVersion->coord) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheDeleted3,"DataWritten");
#endif
            CExpiredCleaner::x_DeleteData(m_CacheData);
        }
        m_CacheData->coord = m_CurVersion->coord;
        if (m_CacheData->dead_time == 0 && !m_CacheData->coord.empty()) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugCacheDeleted4,"DataWritten");
#endif
            CExpiredCleaner::x_DeleteData(m_CacheData);
        }
        m_CacheData->lock.Unlock();
        CNCBlobStorage::ReleaseCacheData(m_CacheData);
    }
}

void
CNCBlobVerManager::RequestMemRelease(void)
{
    m_CacheData->lock.Lock();
    if (!Referenced()) {
        m_NeedReleaseMem = true;
        SetRunnable();
    }
    m_CacheData->lock.Unlock();
}

CSrvRef<SNCBlobVerData>
CNCBlobVerManager::GetCurVersion(void)
{
    CSrvRef<SNCBlobVerData> to_del_ver;
    CSrvRef<SNCBlobVerData> cur_ver;

    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->Get_ver_mgr() == this);
    if (m_CurVersion
        &&  m_CurVersion->dead_time <= CSrvTime::CurSecs())
    {
        to_del_ver = m_CurVersion;
        x_DeleteCurVersion();
    }
    cur_ver = m_CurVersion;
    m_CacheData->lock.Unlock();

    return cur_ver;
}

CSrvRef<SNCBlobVerData>
CNCBlobVerManager::CreateNewVersion(void)
{
    SNCBlobVerData* data = new SNCBlobVerData(this);
//    data->manager       = this;
    data->coord.clear();
    data->data_coord.clear();
    data->create_time   = 0;
    data->expire        = 0;
    data->dead_time     = 0;
    data->size          = 0;
    data->blob_ver      = 0;
    data->chunk_size    = kNCMaxBlobChunkSize;
    data->map_size      = kNCMaxChunksInMap;
    data->meta_mem      = s_CalcVerDataSize(data);
    s_AddCurrentMem(data->meta_mem);
    return SrvRef(data);
}

void
CNCBlobVerManager::FinalizeWriting(SNCBlobVerData* ver_data)
{
    CSrvRef<SNCBlobVerData> old_ver(ver_data);

    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->Get_ver_mgr() == this);
    if (ver_data->dead_time > CSrvTime::CurSecs()
        &&  s_IsCurVerOlder(m_CurVersion, ver_data))
    {
        old_ver.Swap(m_CurVersion);
        m_CacheData->coord = m_CurVersion->coord;
        m_CacheData->dead_time = m_CurVersion->dead_time;
        if (m_CacheData->saved_dead_time != m_CacheData->dead_time) {
            CNCBlobStorage::ChangeCacheDeadTime(m_CacheData);
        }
        m_CacheData->create_id = m_CurVersion->create_id;
        m_CacheData->create_server = m_CurVersion->create_server;
        m_CacheData->create_time = m_CurVersion->create_time;
        m_CacheData->expire = m_CurVersion->expire;
        m_CacheData->ver_expire = m_CurVersion->ver_expire;
        m_CacheData->size = m_CurVersion->size;
        m_CacheData->chunk_size = m_CurVersion->chunk_size;
        m_CacheData->map_size = m_CurVersion->map_size;

        m_CurVersion->meta_has_changed = true;
        m_CurVersion->last_access_time = CSrvTime::CurSecs();
        m_CurVersion->need_write_time = m_CurVersion->last_access_time
                                        + s_WBWriteTimeout;
        if (old_ver)
            old_ver->SetNotCurrent();
        m_CurVersion->SetCurrent();

        SetRunnable();
    }
    m_CacheData->lock.Unlock();

    old_ver.Reset();
}

void
CNCBlobVerManager::DeadTimeChanged(SNCBlobVerData* ver_data)
{
    m_CacheData->lock.Lock();
    if (m_CurVersion == ver_data) {
        m_CacheData->dead_time = ver_data->dead_time;
        m_CacheData->expire = ver_data->expire;
        m_CacheData->ver_expire = ver_data->ver_expire;
        m_CurVersion->last_access_time = CSrvTime::CurSecs();
        m_CurVersion->need_write_time = m_CurVersion->last_access_time
                                        + s_WBWriteTimeout;
        m_CurVersion->meta_has_changed = true;

        SetRunnable();
    }
    m_CacheData->lock.Unlock();
}


CCurVerReader::CCurVerReader(CNCBlobVerManager* mgr)
    : m_VerMgr(mgr)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CCurVerReader";
#endif
}

CCurVerReader::~CCurVerReader(void)
{}

void
CCurVerReader::ExecuteSlice(TSrvThreadNum thr_num)
{
    if (IsTransStateFinal())
        return;

// read current blob metadata from db

    SNCCacheData* cache_data = m_VerMgr->m_CacheData;
    if (cache_data->coord.empty()) {
        FinishTransition();
        return;
    }

    SNCBlobVerData* ver_data = new SNCBlobVerData(m_VerMgr);
//    ver_data->manager = m_VerMgr;
    ver_data->coord = cache_data->coord;
    ver_data->create_time = cache_data->create_time;
    ver_data->size = cache_data->size;
    ver_data->chunk_size = cache_data->chunk_size;
    ver_data->map_size = cache_data->map_size;
    ver_data->is_cur_version = true;
    ver_data->last_access_time = CSrvTime::CurSecs();
    if (ver_data->size != 0) {
        ver_data->cnt_chunks = (ver_data->size - 1) / ver_data->chunk_size + 1;
        ver_data->chunks.resize(ver_data->cnt_chunks, NULL);
    }
    ver_data->cur_chunk_num = ver_data->cnt_chunks;
    ver_data->meta_mem = s_CalcVerDataSize(ver_data);
    s_AddCurrentMem(ver_data->meta_mem);
    ver_data->meta_mem += kVerManagerSize;
    m_VerMgr->m_CurVersion = ver_data;
    if (!CNCBlobStorage::ReadBlobInfo(ver_data)) {
        SRV_LOG(Error, "Problem reading meta-information about blob "
                          << CNCBlobKeyLight(m_VerMgr->m_Key).KeyForLogs());
        CSrvRef<SNCBlobVerData> cur_ver(ver_data);
        m_VerMgr->DeleteVersion(ver_data);
    }

    FinishTransition();
}


SNCChunkMaps::SNCChunkMaps(Uint2 map_size)
{
    size_t mem_size = (char*)maps[0]->coords - (char*)maps[0]
                      + map_size * sizeof(maps[0]->coords[0]);
    for (Uint1 i = 0; i <= kNCMaxBlobMapsDepth; ++i) {
        maps[i] = (SNCChunkMapInfo*)calloc(mem_size, 1);
    }
}

SNCChunkMaps::~SNCChunkMaps(void)
{
    for (Uint1 i = 0; i <= kNCMaxBlobMapsDepth; ++i) {
        free(maps[i]);
    }
}


CWBMemDeleter::CWBMemDeleter(char* mem, Uint4 mem_size)
    : m_Mem(mem),
      m_MemSize(mem_size)
{}

CWBMemDeleter::~CWBMemDeleter(void)
{}

void
CWBMemDeleter::ExecuteRCU(void)
{
    s_FreeWriteBackMem(m_Mem, m_MemSize, m_MemSize);
    delete this;
}


SNCBlobVerData::SNCBlobVerData(CNCBlobVerManager* mgr)
    :   size(0),
        create_time(0),
        create_server(0),
        create_id(0),
        updated_on_server(0),
        updated_at_time(0),
        update_received(0),
        ttl(0),
        expire(0),
        dead_time(0),
        blob_ver(0),
        ver_ttl(0),
        ver_expire(0),
        chunk_size(0),
        map_size(0),
        map_depth(0),
        has_error(false),
        is_cur_version(false),
        meta_has_changed(false),
        move_or_rewrite(false),
        is_releasable(false),
        request_data_write(false),
        need_write_all(false),
        need_stop_write(false),
        need_mem_release(false),
        delete_scheduled(false),
        map_move_counter(0),
        last_access_time(0),
        need_write_time(0),
        cnt_chunks(0),
        cur_chunk_num(0),
        chunk_maps(NULL),
        ver_manager(mgr),
        saved_access_time(0),
        meta_mem(0),
        data_mem(0),
        releasable_mem(0),
        releasing_mem(0)
{
    //AtomicAdd(s_CntVers, 1);
    //Uint8 cnt = AtomicAdd(s_CntVers, 1);
    //INFO("SNCBlobVerData, cnt=" << cnt);
#if __NC_TASKS_MONITOR
    m_TaskName = "SNCBlobVerData";
#endif
    SetPriority(s_TaskPriorityWbMemRelease);
}

SNCBlobVerData::~SNCBlobVerData(void)
{
    if (chunk_maps) {
        SRV_FATAL("chunk_maps not released");
    }

    //AtomicSub(s_CntVers, 1);
    //Uint8 cnt = AtomicSub(s_CntVers, 1);
    //INFO("~SNCBlobVerData, cnt=" << cnt);
}

void
SNCBlobVerData::DeleteThis(void)
{
    wb_mem_lock.Lock();
    if (!is_cur_version) {
        s_AddReleasingMem(releasable_mem + meta_mem, releasable_mem);
        releasing_mem += releasable_mem + meta_mem;
        releasable_mem = 0;
        need_mem_release = true;
    }
    else if (releasable_mem != 0) {
        s_AddReleasingMem(releasable_mem, releasable_mem);
        releasing_mem += releasable_mem;
        releasable_mem = 0;
    }
    ver_manager = NULL;
    wb_mem_lock.Unlock();

    SetRunnable();
}

void
SNCBlobVerData::RequestDataWrite(void)
{
    wb_mem_lock.Lock();
    request_data_write = true;
    need_write_all = true;
    need_write_time = 0;
    size_t mem_size = releasable_mem;
    if (is_releasable)
        mem_size -= meta_mem;
    s_AddReleasingMem(mem_size, mem_size);
    releasing_mem += mem_size;
    releasable_mem -= mem_size;
    wb_mem_lock.Unlock();

    SetRunnable();
}

size_t
SNCBlobVerData::RequestMemRelease(void)
{
    wb_mem_lock.Lock();
    size_t mem_size = releasable_mem;
    releasing_mem += mem_size;
    releasable_mem = 0;
    need_write_time = 0;
    need_mem_release = true;
    wb_mem_lock.Unlock();

    SetRunnable();
    return mem_size;
}

void
SNCBlobVerData::SetNotCurrent(void)
{
    wb_mem_lock.Lock();
    if (request_data_write) {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugDeleteSNCBlobVerData,"SetNotCurrent");
#endif
        request_data_write = false;
        ver_manager->RevokeDataWrite();
    }
    is_cur_version = false;
    need_stop_write = true;
    meta_mem -= kVerManagerSize;
    wb_mem_lock.Unlock();

    SetRunnable();
}

void
SNCBlobVerData::SetCurrent(void)
{
    wb_mem_lock.Lock();
    is_cur_version = true;
    meta_mem += kVerManagerSize;
    wb_mem_lock.Unlock();
}

void
SNCBlobVerData::SetReleasable(void)
{
    last_access_time = CSrvTime::CurSecs();

    wb_mem_lock.Lock();
    is_releasable = true;
    releasable_mem += meta_mem;
    s_AddReleasableMem(this, meta_mem, 0);
    wb_mem_lock.Unlock();
}

void
SNCBlobVerData::SetNonReleasable(void)
{
    last_access_time = CSrvTime::CurSecs();

    wb_mem_lock.Lock();
    is_releasable = false;
    if (releasable_mem != 0) {
        if (releasable_mem < meta_mem) {
            SRV_FATAL("blob ver data broken");
        }
        releasable_mem -= meta_mem;
        s_SubReleasableMem(meta_mem);
    }
    else {
        if (releasing_mem < meta_mem) {
            SRV_FATAL("blob ver data broken");
        }
        releasing_mem -= meta_mem;
        need_mem_release = false;
        s_SubReleasingMem(meta_mem);
    }
    wb_mem_lock.Unlock();
}

void
SNCBlobVerData::x_FreeChunkMaps(void)
{
    if (chunk_maps) {
        s_SubCurrentMem(s_CalcChunkMapsSize(map_size));
        delete chunk_maps;
        chunk_maps = NULL;
    }
}

bool
SNCBlobVerData::x_WriteBlobInfo(void)
{
    if (!meta_has_changed)
        return true;

    CNCBlobVerManager* mgr = ACCESS_ONCE(ver_manager);
    if (!mgr) {
        need_stop_write = true;
        return true;
    }

    if (move_or_rewrite  ||  !AtomicCAS(move_or_rewrite, false, true)) {
        need_write_all = true;
        return true;
    }

    meta_has_changed = false;
    bool new_write = coord.empty();
    if (!CNCBlobStorage::WriteBlobInfo(mgr->GetKey(), this, chunk_maps, cnt_chunks, mgr->GetCacheData()))
    {
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugWriteBlobInfoFailed,"x_WriteBlobInfo");
#endif
        meta_has_changed = true;
        move_or_rewrite = false;
        need_write_all = true;
        RunAfter(s_WBFailedWriteDelay);
        return false;
    }
    if (new_write) {
        CNCStat::DiskBlobWrite(size);
    }
    x_FreeChunkMaps();

    move_or_rewrite = false;
    return true;
}

bool
SNCBlobVerData::x_WriteCurChunk(char* write_mem, Uint4 write_size)
{
    if (!chunk_maps) {
        chunk_maps = new SNCChunkMaps(map_size);
        s_AddCurrentMem(s_CalcChunkMapsSize(map_size));
    }
    CNCBlobVerManager* mgr = ACCESS_ONCE(ver_manager);
    if (!mgr) {
        need_stop_write = true;
        return true;
    }
    char* new_mem = CNCBlobStorage::WriteChunkData(
                                        this, chunk_maps, mgr->GetCacheData(),
                                        cur_chunk_num, write_mem, write_size);
    if (!new_mem) {
        RunAfter(s_WBFailedWriteDelay);
        return false;
    }
    CNCStat::DiskDataWrite(write_size);

    wb_mem_lock.Lock();
    chunks[cur_chunk_num] = new_mem;
    ++cur_chunk_num;
    if (data_mem < write_size) {
        SRV_FATAL("blob ver data broken");
    }
    data_mem -= write_size;
    if (releasing_mem != 0) {
        if (releasing_mem < write_size) {
            SRV_FATAL("blob ver data broken");
        }
        releasing_mem -= write_size;
    }
    else {
        if (releasable_mem < write_size) {
            SRV_FATAL("blob ver data broken");
        }
        releasable_mem -= write_size;
        // memory will be subtracted from global releasing after call to
        // deleter below
        s_AddReleasingMem(write_size, write_size);
    }
    wb_mem_lock.Unlock();

    CWBMemDeleter* deleter = new CWBMemDeleter(write_mem, write_size);
    deleter->CallRCU();

    return true;
}

bool
SNCBlobVerData::x_ExecuteWriteAll(void)
{
    wb_mem_lock.Lock();

    if (need_stop_write) {
        need_write_all = false;
        need_stop_write = false;
        if (!need_mem_release  &&  releasing_mem != 0) {
            s_AddReleasableMem(this, releasing_mem, releasing_mem);
            releasable_mem += releasing_mem;
            releasing_mem = 0;
        }
        wb_mem_lock.Unlock();
        return false;
    }
    else if (cur_chunk_num == cnt_chunks) {
        need_write_all = false;
        wb_mem_lock.Unlock();

        if (x_WriteBlobInfo())
            SetRunnable();
        return true;
    }
    char* write_mem = chunks[cur_chunk_num];
    Uint4 write_size = chunk_size;
    if (cur_chunk_num == cnt_chunks - 1)
        write_size = Uint4(min(size - (cnt_chunks - 1) * chunk_size, Uint8(chunk_size)));
    wb_mem_lock.Unlock();

    if (x_WriteCurChunk(write_mem, write_size))
        SetRunnable();
    return true;
}

void
SNCBlobVerData::x_DeleteVersion(void)
{
    CNCBlobStorage::DeleteBlobInfo(this, chunk_maps);
    coord.clear();
    x_FreeChunkMaps();
    if (cur_chunk_num < cnt_chunks) {
        for (Uint8 num = cur_chunk_num; num < cnt_chunks - 1; ++num) {
            s_FreeWriteBackMem(chunks[num], chunk_size, chunk_size);
            if (releasing_mem < chunk_size) {
                SRV_FATAL("blob ver data broken");
            }
            releasing_mem -= chunk_size;
        }
        Uint4 last_size = Uint4(min(size - (cnt_chunks - 1) * chunk_size, Uint8(chunk_size)));
        s_FreeWriteBackMem(chunks[cnt_chunks - 1], last_size, last_size);
        if (releasing_mem < last_size) {
            SRV_FATAL("blob ver data broken");
        }
        releasing_mem -= last_size;
        cur_chunk_num = cnt_chunks;
    }
}

void
SNCBlobVerData::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// writes blob data and metadata into db
    if (need_write_all  &&  x_ExecuteWriteAll())
        return;

// remove blob from memory
    wb_mem_lock.Lock();
    CNCBlobVerManager* mgr = ACCESS_ONCE(ver_manager);
    if (mgr) {
        if (request_data_write) {
            request_data_write = false;
            mgr->DataWritten();
        }
        if (need_mem_release) {
            // if still has something to write, request that
            if (data_mem != 0  ||  (meta_has_changed  &&  is_cur_version)) {
                need_write_all = true;
#ifdef _DEBUG
CNCAlerts::Register(CNCAlerts::eDebugExtraWrite,"SNCBlobVerData::ExecuteSlice");
#endif
                wb_mem_lock.Unlock();
                SetRunnable();
                return;
            }
            if (is_cur_version) {
                wb_mem_lock.Unlock();
                mgr->RequestMemRelease();
                return;
            }
            need_mem_release = false;
        }
        wb_mem_lock.Unlock();
    }
    else {
        wb_mem_lock.Unlock();

        if (!is_cur_version)
            x_DeleteVersion();
#if 0
        if (releasable_mem != 0  ||  releasing_mem != meta_mem) {
            SRV_FATAL("blob ver data broken");
        }
#endif
        if (!delete_scheduled)
            s_ScheduleVerDelete(this);
    }
}

void
SNCBlobVerData::AddChunkMem(char* mem, Uint4 mem_size)
{
    wb_mem_lock.Lock();
    size_t old_meta = s_CalcVerDataSize(this);
    chunks.push_back(mem);
    ++cnt_chunks;
    size_t add_meta_size = s_CalcVerDataSize(this) - old_meta;
    if (add_meta_size != 0) {
        s_AddCurrentMem(add_meta_size);
        meta_mem += add_meta_size;
    }
    data_mem += mem_size;
    releasable_mem += mem_size;
    s_AddReleasableMem(this, mem_size, 0);
    wb_mem_lock.Unlock();
}


//static Uint8 s_CntAccs = 0;

CNCBlobAccessor::CNCBlobAccessor(void)
    : m_ChunkMaps(NULL),
      m_MetaInfoReady(false),
      m_WriteMemRequested(false),
      m_Buffer(NULL)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CNCBlobAccessor";
#endif
    //Uint8 cnt = AtomicAdd(s_CntAccs, 1);
    //INFO("CNCBlobAccessor, cnt=" << cnt);
}

CNCBlobAccessor::~CNCBlobAccessor(void)
{
    if (m_ChunkMaps) {
        SRV_FATAL("blob accessor broken");
    }

    //Uint8 cnt = AtomicSub(s_CntAccs, 1);
    //INFO("~CNCBlobAccessor, cnt=" << cnt);
}

void
CNCBlobAccessor::Prepare(const string& key,
                         const string& password,
                         Uint2         time_bucket,
                         ENCAccessType access_type)
{
    m_BlobKey       = key;
    m_Password      = password;
    m_TimeBucket    = time_bucket;
    m_AccessType    = access_type;
    m_HasError      = false;
    m_VerManager    = NULL;
    m_CurChunk      = 0;
    m_ChunkPos      = 0;
    m_SizeRead      = 0;
}

void
CNCBlobAccessor::Initialize(SNCCacheData* cache_data)
{
    if (cache_data) {
        bool new_version = m_AccessType == eNCCreate
                           ||  m_AccessType == eNCCopyCreate;
        m_VerManager = CNCBlobVerManager::Get(m_TimeBucket, cache_data->key,
                                              cache_data, new_version);
    }
}

void
CNCBlobAccessor::Deinitialize(void)
{
    switch (m_AccessType) {
    case eNCReadData:
        if (m_ChunkMaps) {
            s_SubCurrentMem(s_CalcChunkMapsSize(m_CurData->map_size));
            delete m_ChunkMaps;
            m_ChunkMaps = NULL;
        }
        break;
    case eNCCreate:
    case eNCCopyCreate:
        if (m_NewData  &&  m_Buffer) {
            s_FreeWriteBackMem(m_Buffer, m_NewData->chunk_size, 0);
            m_Buffer = NULL;
        }
        break;
    default:
        break;
    }

    m_NewData.Reset();
    m_CurData.Reset();
    if (m_VerManager) {
        m_VerManager->Release();
        m_VerManager = NULL;
    }
}

void
CNCBlobAccessor::Release(void)
{
    Deinitialize();
    Terminate();
}

void
CNCBlobAccessor::RequestMetaInfo(CSrvTask* owner)
{
    m_Owner = owner;
    if (m_VerManager) {
        m_VerManager->RequestCurVersion(this);
    }
    else {
        m_MetaInfoReady = true;
    }
}

void
CNCBlobAccessor::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
    if (!IsTransFinished())
        return;

    if (!m_MetaInfoReady) {
// read blob metadata from db
        m_CurData = m_VerManager->GetCurVersion();
        m_MetaInfoReady = true;
        m_Owner->SetRunnable();
    }
    else if (m_WriteMemRequested) {
// client sends data, we need memory
// if no memory, wait a bit, then try again
        m_Buffer = s_AllocWriteBackMem(m_NewData->chunk_size, this);
        if (m_Buffer) {
            m_WriteMemRequested = false;
            m_Owner->SetRunnable();
        }
    }
}

void
CNCBlobAccessor::x_DelCorruptedVersion(void)
{
    SRV_LOG(Critical, "Database information about blob "
                      << CNCBlobKeyLight(m_BlobKey).KeyForLogs()
                      << " is corrupted. Blob will be deleted");
    m_VerManager->DeleteVersion(m_CurData);
    m_CurData->has_error = true;
    m_HasError = true;
}

Uint4
CNCBlobAccessor::GetReadMemSize(void)
{
    if (m_CurData->has_error) {
        m_HasError = true;
        return 0;
    }
    if (GetPosition() >= m_CurData->size) {
        SRV_FATAL("blob accessor broken");
    }
    if (m_Buffer) {
        if (m_ChunkPos < m_ChunkSize) {
            m_Buffer = m_CurData->chunks[m_CurChunk];
            return m_ChunkSize - m_ChunkPos;
        }
        ++m_CurChunk;
        m_ChunkPos = 0;
    }

    Uint8 need_size = m_CurData->size - GetPosition() + m_ChunkPos;
    if (need_size > m_CurData->chunk_size)
        need_size = m_CurData->chunk_size;

    m_Buffer = ACCESS_ONCE(m_CurData->chunks[m_CurChunk]);
    if (m_Buffer) {
        m_ChunkSize = Uint4(need_size);
        return m_ChunkSize - m_ChunkPos;
    }

    if (!m_ChunkMaps) {
        m_ChunkMaps = new SNCChunkMaps(m_CurData->map_size);
        s_AddCurrentMem(s_CalcChunkMapsSize(m_CurData->map_size));
    }
    if (!CNCBlobStorage::ReadChunkData(m_CurData, m_ChunkMaps, m_CurChunk,
                                       m_Buffer, m_ChunkSize))
    {
        x_DelCorruptedVersion();
        return 0;
    }
    if (m_ChunkSize != need_size) {
        x_DelCorruptedVersion();
        return 0;
    }

    ACCESS_ONCE(m_CurData->chunks[m_CurChunk]) = m_Buffer;
    return m_ChunkSize - m_ChunkPos;
}

void
CNCBlobAccessor::MoveReadPos(Uint4 move_size)
{
    m_ChunkPos += move_size;
    m_SizeRead += move_size;
    if (m_CurData->cur_chunk_num > m_CurChunk
        &&  m_Buffer == m_CurData->chunks[m_CurChunk])
    {
        CNCStat::DiskDataRead(move_size);
    }
}

void
CNCBlobAccessor::x_CreateNewData(void)
{
    if (!m_NewData) {
        m_NewData = m_VerManager->CreateNewVersion();
        m_NewData->password = m_Password;
    }
}

size_t
CNCBlobAccessor::GetWriteMemSize(void)
{
    x_CreateNewData();
    if (m_WriteMemRequested)
        return 0;
    if (m_Buffer  &&  m_ChunkPos < m_NewData->chunk_size)
        return m_NewData->chunk_size - m_ChunkPos;

    if (m_Buffer) {
        if (m_ChunkPos != m_NewData->chunk_size) {
            SRV_FATAL("blob accessor broken");
        }
        m_NewData->AddChunkMem(m_Buffer, m_NewData->chunk_size);
        m_Buffer = NULL;
        ++m_CurChunk;
        m_ChunkPos = 0;
    }
    m_WriteMemRequested = true;
    m_Buffer = s_AllocWriteBackMem(m_NewData->chunk_size, this);
    if (!m_Buffer)
        return 0;

    m_WriteMemRequested = false;
    return m_NewData->chunk_size - m_ChunkPos;
}

void
CNCBlobAccessor::Finalize(void)
{
    if (m_ChunkPos != 0) {
        m_Buffer = s_ReallocWriteBackMem(m_Buffer, m_NewData->chunk_size, m_ChunkPos);
        m_NewData->AddChunkMem(m_Buffer, m_ChunkPos);
        m_Buffer = NULL;
    }
    m_VerManager->FinalizeWriting(m_NewData);
    if (m_CurData.NotNull() && m_CurData->update_received != 0) {
        CWriteBackControl::RecordNotifyUpdateBlob(m_CurData->update_received);
    }
    m_CurData = m_NewData;
}

bool
CNCBlobAccessor::ReplaceBlobInfo(const SNCBlobVerData& new_info)
{
    if (!s_IsCurVerOlder(m_CurData, &new_info)) {
        return false;
    }

    x_CreateNewData();
    m_NewData->create_time = new_info.create_time;
    m_NewData->ttl = new_info.ttl;
    m_NewData->dead_time = new_info.dead_time;
    m_NewData->expire = new_info.expire;
    m_NewData->password = new_info.password;
    m_NewData->blob_ver = new_info.blob_ver;
    m_NewData->ver_ttl = new_info.ver_ttl;
    m_NewData->ver_expire = new_info.ver_expire;
    m_NewData->create_server = new_info.create_server;
    m_NewData->create_id = new_info.create_id;
    return true;
}

string
CNCBlobAccessor::GetCurPassword(void) const
{
    if (m_CurData->password.empty())
        return m_CurData->password;
    else
        return CNCBlobStorage::PrintablePassword(m_CurData->password);
}

Uint8 CNCBlobAccessor::GetPurgeCount()
{
    return  s_Forgets.size() + s_ForgetKeys.size();
}

bool
CNCBlobAccessor::IsPurged(const CNCBlobKeyLight& nc_key) const
{
    if (nc_key.Cache().empty()) {
        return false;
    }
    Uint8 cr_time = GetCurBlobCreateTime();
    if (cr_time == 0 || cr_time > s_LatestPurge) {
        return false;
    }
    string key(nc_key.Cache());
    bool res = false;
    s_ConsListLock.Lock();
    for (int t=0; !res && t<2; ++t) {
        key.append(1,'\1');
        TForgets::const_iterator i = s_ForgetKeys.find(key);
        if (i != s_ForgetKeys.end()) {
            res = cr_time <= i->second;
        }
        key.append(nc_key.RawKey());
    }
    s_ConsListLock.Unlock();
    return res;
}

bool
CNCBlobAccessor::Purge(const CNCBlobKeyLight& nc_key, Uint8 when)
{
    bool res=false;
    Uint8 lifespan = 9U * 24U * 3600U * (Uint8)kUSecsPerSecond; // 9 days
    Uint8 now = CSrvTime::Current().AsUSec();
    Uint8 longago = now > lifespan ? (now-lifespan) : 0;
    s_LatestPurge = max(s_LatestPurge, when);
    
    s_ConsListLock.Lock();
    ERASE_ITERATE( TForgets, f, s_Forgets) {
        if (f->second < longago) {
            s_Forgets.erase(f);
            res=true;
        }
    }
    ERASE_ITERATE( TForgets, f, s_ForgetKeys) {
        if (f->second < longago) {
            s_ForgetKeys.erase(f);
            res=true;
        }
    }
    if (when != 0) {
        string key(nc_key.Cache());
        if (nc_key.RawKey().empty()) {
            TForgets::iterator i = s_Forgets.find(key);
            if (i == s_Forgets.end()) {
                s_Forgets[key] = when;
                res=true;
            } else if (i->second < when) {
                i->second = when;
                res=true;
            }
            key.append(1,'\1');
        } else {
            key.append(1,'\1').append(nc_key.RawKey()).append(1,'\1');
        }
        TForgets::iterator i = s_ForgetKeys.find(key);
        if (i == s_ForgetKeys.end()) {
            s_ForgetKeys[key] = when;
            res=true;
        } else if (i->second < when) {
            i->second = when;
            res=true;
        }
    }
    s_ConsListLock.Unlock();
    return res;
}

string CNCBlobAccessor::GetPurgeData(char separator)
{
    string res;
    Purge(CNCBlobKeyLight(),0);
    s_ConsListLock.Lock();
    ITERATE( TForgets, f, s_Forgets) {
        res += NStr::NumericToString(f->second);
        res += ' ';
        res += f->first;
        res += separator;
    }
    s_ConsListLock.Unlock();
    return res;
}

bool CNCBlobAccessor::UpdatePurgeData(const string& data, char separator)
{
    bool res=false, error=false;;
    const char *begin = data.data();
    const char *end = begin + data.size();
    if (end != begin) {
        s_ConsListLock.Lock();
        while (end != begin) {
            Uint8 when = NStr::StringToUInt8(begin,
                NStr::fConvErr_NoThrow | NStr::fAllowTrailingSymbols);
            if (when == 0) {
                error=true;
                break;
            }
            const char *blank = strchr(begin,' ');
            if (!blank) {
                error=true;
                break;
            }
            const char *eol = strchr(blank,separator);
            if (!eol) {
                error=true;
                break;
            }
            string cache(blank+1, eol-blank-1);
            TForgets::const_iterator i = s_Forgets.find(cache);
            if (i == s_Forgets.end() || i->second < when) {
                s_Forgets[cache] = when;
                res = true;
            }
            begin = eol + 1;
        }
        s_ConsListLock.Unlock();
    }
    if (error) {
        SRV_LOG(Error, "Invalid PURGE data: " << data);
    }
    return res;
}

void CNCBlobAccessor::SetFailedWriteCount(int failed_write)
{
    s_FailedListLock.Lock();
    s_FailedReserve = failed_write - s_FailedKeys.size();
    s_FailMonitor = failed_write > 0;
    s_FailedListLock.Unlock();
}
int CNCBlobAccessor::GetFailedWriteCount(void)
{
    return s_FailedReserve;
}
void CNCBlobAccessor::PutFailed(const string& blob_key)
{
    if (s_FailMonitor) {
        s_FailedListLock.Lock();
        if (s_FailedReserve > 0) {
            s_FailedKeys.insert(blob_key);
            --s_FailedReserve;
            s_FailedCounter.Add(1);
        }
        s_FailedListLock.Unlock();
    }
}
void CNCBlobAccessor::PutSucceeded(const string& blob_key)
{
    if (s_FailMonitor && s_FailedCounter.Get() != 0) {
        s_FailedListLock.Lock();
        if (s_FailedKeys.erase(blob_key)) {
            ++s_FailedReserve;
            s_FailedCounter.Add(-1);
        }
        s_FailedListLock.Unlock();
    }
}
bool CNCBlobAccessor::HasPutSucceeded(const string& blob_key)
{
    bool b = false;
    if (s_FailMonitor && s_FailedCounter.Get() != 0) {
        s_FailedListLock.Lock();
        b = s_FailedKeys.find(blob_key) != s_FailedKeys.end();
        s_FailedListLock.Unlock();
    }
    return !b;
}

/////////////////////////////////////////////////////////////////////////////
CBulkCleaner::CBulkCleaner(void)
{
    SetState(&CBulkCleaner::x_StartSession);
}

CBulkCleaner::~CBulkCleaner(void)
{
}

CBulkCleaner::State
CBulkCleaner::x_StartSession(void)
{
    if (CTaskServer::IsInShutdown()) {
        return NULL;
    }
    m_CurBucket = 1;
    m_CrTime = 0;
    m_BlobAccess = NULL;
    s_ConsListLock.Lock();
    while (!s_ForgetKeys.empty()) {
        TForgets::const_iterator i = s_ForgetKeys.begin();
        m_Filter = i->first;
        m_CrTime = i->second;
        if (m_CrTime != 0) {
            break;
        }
        s_ForgetKeys.erase(i);
    }
    s_ConsListLock.Unlock();
    if (m_CrTime != 0) {
        CreateNewDiagCtx();
        return &CBulkCleaner::x_FindNext;
    }
    RunAfter(10);
    return NULL;
}

CBulkCleaner::State
CBulkCleaner::x_FindNext(void)
{
    if (CTaskServer::IsInShutdown()) {
        return NULL;
    }
    for (; m_CurBucket <= CNCDistributionConf::GetCntTimeBuckets(); ++m_CurBucket) {
        m_Key = CNCBlobStorage::FindBlob(m_CurBucket, m_Filter, m_CrTime);
        if (!m_Key.empty()) {
            return &CBulkCleaner::x_RequestBlobAccess;
        }
    }
    return &CBulkCleaner::x_FinishSession;
}

CBulkCleaner::State
CBulkCleaner::x_RequestBlobAccess(void)
{
    CNCBlobKeyLight nc_key(m_Key);
    Uint2 slot, bkt;
    if (nc_key.IsICacheKey()) {
        CNCDistributionConf::GetSlotByICacheKey(nc_key, slot, bkt);
    } else {
        // we should never be here
        ++m_CurBucket;
        return &CBulkCleaner::x_FindNext;
    }
    m_BlobAccess = CNCBlobStorage::GetBlobAccess( eNCCreate, nc_key.PackedKey(), "", bkt);
    m_BlobAccess->RequestMetaInfo(this);
    return &CBulkCleaner::x_RemoveBlob;
}

CBulkCleaner::State
CBulkCleaner::x_RemoveBlob(void)
{
    if (CTaskServer::IsInShutdown()) {
        return NULL;
    }
    if (!m_BlobAccess->IsMetaInfoReady()) {
        return NULL;
    }

// see
// CNCMessageHandler::x_DoCmd_Remove
    if ((!m_BlobAccess || !m_BlobAccess->IsBlobExists() || m_BlobAccess->IsCurBlobExpired()))
    {
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
        return &CBulkCleaner::x_FindNext;;
    }

    m_BlobAccess->SetBlobTTL(m_BlobAccess->GetCurBlobTTL());
    m_BlobAccess->SetBlobVersion(0);
    int expire = CSrvTime::CurSecs() - 1;
    unsigned int ttl = m_BlobAccess->GetNewBlobTTL();
    m_BlobAccess->SetNewBlobExpire(expire, expire + ttl + 1);

// see
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

    return &CBulkCleaner::x_Finalize;
}

CBulkCleaner::State
CBulkCleaner::x_Finalize(void)
{
    if (m_BlobAccess) {
        m_BlobAccess->Finalize();
        m_BlobAccess->Release();
        m_BlobAccess = NULL;

        {
            CSrvDiagMsg diag_msg;
            GetDiagCtx()->SetRequestID();
            diag_msg.StartRequest().PrintParam("_type", "bulkrmv");
            CNCBlobKeyLight key(m_Key);
            diag_msg.PrintParam("cache",key.Cache()).PrintParam("key",key.RawKey()).PrintParam("subkey",key.SubKey());
            diag_msg.Flush();
            diag_msg.StopRequest();
        }
    }
    return &CBulkCleaner::x_FindNext;
}

CBulkCleaner::State
CBulkCleaner::x_FinishSession(void)
{
    ReleaseDiagCtx();

    s_ConsListLock.Lock();
    if (!s_ForgetKeys.empty()) {
        TForgets::const_iterator i = s_ForgetKeys.begin();
        if (m_Filter == i->first && m_CrTime == i->second) {
            s_ForgetKeys.erase(i);
        }
    }
    s_ConsListLock.Unlock();

    SetState(&CBulkCleaner::x_StartSession);
    SetRunnable();
    return NULL;
}

END_NCBI_SCOPE
