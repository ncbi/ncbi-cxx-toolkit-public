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

#include "nc_storage_blob.hpp"
#include "nc_storage.hpp"
#include "nc_stat.hpp"


BEGIN_NCBI_SCOPE


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
    if (cur_ver) {
        if (cur_ver->create_time != new_ver->create_time)
            return cur_ver->create_time < new_ver->create_time;
        else if (cur_ver->create_server != new_ver->create_server)
            return cur_ver->create_server < new_ver->create_server;
        else if (cur_ver->create_id != new_ver->create_id)
            return cur_ver->create_id < new_ver->create_id;
        else if (cur_ver->expire != new_ver->expire)
            return cur_ver->expire < new_ver->expire;
        else if (cur_ver->ver_expire != new_ver->ver_expire)
            return cur_ver->ver_expire < new_ver->ver_expire;
        else
            return cur_ver->dead_time < new_ver->dead_time;
    }
    return true;
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
    if (ver_data)
        wb_data->to_add_list.push_back(ver_data);
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
    wb_data->to_del_list.push_back(ver_data);
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
    wb_data->lock.Lock();
    s_WBCurSize += wb_data->cur_size;
    wb_data->cur_size = 0;
    s_WBReleasableSize += wb_data->releasable_size;
    wb_data->releasable_size = 0;
    s_WBReleasingSize += wb_data->releasing_size;
    wb_data->releasing_size = 0;
    s_TransferVerList(wb_data->to_add_list, s_WBToAddList);
    s_TransferVerList(wb_data->to_del_list, s_WBToDelList);
    wb_data->lock.Unlock();
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
        if (ver_data->is_linked())
            s_VersMap->erase(s_VersMap->iterator_to(*ver_data));
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
{}


CWriteBackControl::CWriteBackControl(void)
{}

CWriteBackControl::~CWriteBackControl(void)
{}

void
CWriteBackControl::Initialize(void)
{
    s_VersMap = new TVerDataMap();
    s_WBData = new SWriteBackData[CTaskServer::GetMaxRunningThreads()];
    s_WBControl = new CWriteBackControl();
    s_WBControl->SetRunnable();
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
    _ASSERT(m_CacheData->ver_mgr == this);
    if (m_CurVersion == ver_data)
        x_DeleteCurVersion();
    m_CacheData->lock.Unlock();
}

void
CNCBlobVerManager::DeleteDeadVersion(int cut_time)
{
    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->ver_mgr == this);
    CSrvRef<SNCBlobVerData> cur_ver(m_CurVersion);
    if (m_CurVersion) {
        if (m_CurVersion->dead_time <= cut_time)
            x_DeleteCurVersion();
    }
    else if (!m_CacheData->coord.empty()  &&  m_CacheData->dead_time <= cut_time) {
        x_DeleteCurVersion();
    }
    m_CacheData->lock.Unlock();
}

CNCBlobVerManager::CNCBlobVerManager(Uint2         time_bucket,
                                     const string& key,
                                     SNCCacheData* cache_data)
    : m_TimeBucket(time_bucket),
      m_NeedReleaseMem(false),
      m_CacheData(cache_data),
      m_CurVerReader(new CCurVerReader(this)),
      m_Key(key)
{
    //AtomicAdd(s_CntMgrs, 1);
    //Uint8 cnt = AtomicAdd(s_CntMgrs, 1);
    //INFO("CNCBlobVerManager, cnt=" << cnt);
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
    CNCBlobVerManager* mgr = cache_data->ver_mgr;
    if (mgr) {
        mgr->ObtainReference();
    }
    else if (for_new_version  ||  !cache_data->coord.empty()) {
        mgr = new CNCBlobVerManager(time_bucket, key, cache_data);
        cache_data->ver_mgr = mgr;
        mgr->AddReference();
        s_AddCurrentMem(kVerManagerSize);
        CNCBlobStorage::ReferenceCacheData(cache_data);
    }
    cache_data->lock.Unlock();

    return mgr;
}

void
CNCBlobVerManager::Release(void)
{
    m_CacheData->lock.Lock();
    // DeleteThis below should be executed under the lock
    RemoveReference();
    m_CacheData->lock.Unlock();
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
    m_CacheData->ver_mgr = NULL;
    m_CacheData->lock.Unlock();

    if (m_CurVersion) {
        m_CacheData->coord = m_CurVersion->coord;
        m_CurVersion.Reset();
    }
    else {
        s_SubCurrentMem(kVerManagerSize);
    }
    CNCBlobStorage::ReleaseCacheData(m_CacheData);
    m_CurVerReader->Terminate();
    Terminate();
}

void
CNCBlobVerManager::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// CNCBlobAccessor has a reference to this one.
// for each blob key there is only one CNCBlobVerManager.

    CSrvRef<SNCBlobVerData> cur_ver;

    m_CacheData->lock.Lock();
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
    int write_time = ACCESS_ONCE(cur_ver->need_write_time);
    if (write_time != 0) {
        m_CacheData->lock.Unlock();
        int cur_time = CSrvTime::CurSecs();
        if (write_time <= cur_time  ||  CTaskServer::IsInShutdown())
            cur_ver->RequestDataWrite();
        else
            RunAfter(write_time - cur_time);
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
    _ASSERT(m_CacheData->ver_mgr == this);
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
    SNCBlobVerData* data = new SNCBlobVerData();
    data->manager       = this;
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
    _ASSERT(m_CacheData->ver_mgr == this);
    if (ver_data->dead_time > CSrvTime::CurSecs()
        &&  s_IsCurVerOlder(m_CurVersion, ver_data))
    {
        old_ver.Swap(m_CurVersion);
        m_CacheData->coord = m_CurVersion->coord;
        m_CacheData->dead_time = m_CurVersion->dead_time;
        if (m_CacheData->saved_dead_time == 0)
            CNCBlobStorage::ChangeCacheDeadTime(m_CacheData);
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
{}

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

    SNCBlobVerData* ver_data = new SNCBlobVerData();
    ver_data->manager = m_VerMgr;
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
        SRV_LOG(Critical, "Problem reading meta-information about blob "
                          << CNCBlobStorage::UnpackKeyForLogs(m_VerMgr->m_Key));
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


SNCBlobVerData::SNCBlobVerData(void)
    : has_error(false),
      is_cur_version(false),
      meta_has_changed(false),
      move_or_rewrite(false),
      is_releasable(false),
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
      saved_access_time(0),
      meta_mem(0),
      data_mem(0),
      releasable_mem(0),
      releasing_mem(0)
{
    //AtomicAdd(s_CntVers, 1);
    //Uint8 cnt = AtomicAdd(s_CntVers, 1);
    //INFO("SNCBlobVerData, cnt=" << cnt);
}

SNCBlobVerData::~SNCBlobVerData(void)
{
    if (chunk_maps)
        abort();

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
    manager = NULL;
    wb_mem_lock.Unlock();

    SetRunnable();
}

void
SNCBlobVerData::RequestDataWrite(void)
{
    wb_mem_lock.Lock();
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
        if (releasable_mem < meta_mem)
            abort();
        releasable_mem -= meta_mem;
        s_SubReleasableMem(meta_mem);
    }
    else {
        if (releasing_mem < meta_mem)
            abort();
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

    CNCBlobVerManager* mgr = ACCESS_ONCE(manager);
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
    if (!CNCBlobStorage::WriteBlobInfo(manager->GetKey(), this, chunk_maps, cnt_chunks, mgr->GetCacheData()))
    {
        meta_has_changed = true;
        move_or_rewrite = false;
        need_write_all = true;
        RunAfter(s_WBFailedWriteDelay);
        return false;
    }
    if (new_write)
        CNCStat::DiskBlobWrite(size);
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
    CNCBlobVerManager* mgr = ACCESS_ONCE(manager);
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
    if (data_mem < write_size)
        abort();
    data_mem -= write_size;
    if (releasing_mem != 0) {
        if (releasing_mem < write_size)
            abort();
        releasing_mem -= write_size;
    }
    else {
        if (releasable_mem < write_size)
            abort();
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
    else {
        char* write_mem = chunks[cur_chunk_num];
        Uint4 write_size = chunk_size;
        if (cur_chunk_num == cnt_chunks - 1)
            write_size = Uint4(min(size - (cnt_chunks - 1) * chunk_size, Uint8(chunk_size)));
        wb_mem_lock.Unlock();

        if (x_WriteCurChunk(write_mem, write_size))
            SetRunnable();
        return true;
    }
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
            if (releasing_mem < chunk_size)
                abort();
            releasing_mem -= chunk_size;
        }
        Uint4 last_size = Uint4(min(size - (cnt_chunks - 1) * chunk_size, Uint8(chunk_size)));
        s_FreeWriteBackMem(chunks[cnt_chunks - 1], last_size, last_size);
        if (releasing_mem < last_size)
            abort();
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
    CNCBlobVerManager* mgr = ACCESS_ONCE(manager);
    if (mgr) {
        if (need_mem_release) {
            // if still has something to write, request that
            if (data_mem != 0  ||  (meta_has_changed  &&  is_cur_version)) {
                need_write_all = true;
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
        if (releasable_mem != 0  ||  releasing_mem != meta_mem)
            abort();
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
    //Uint8 cnt = AtomicAdd(s_CntAccs, 1);
    //INFO("CNCBlobAccessor, cnt=" << cnt);
}

CNCBlobAccessor::~CNCBlobAccessor(void)
{
    if (m_ChunkMaps)
        abort();

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
    //abort();
    SRV_LOG(Critical, "Database information about blob "
                      << CNCBlobStorage::UnpackKeyForLogs(m_BlobKey)
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
    if (GetPosition() >= m_CurData->size)
        abort();
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
        if (m_ChunkPos != m_NewData->chunk_size)
            abort();
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
    m_CurData = m_NewData;
}

bool
CNCBlobAccessor::ReplaceBlobInfo(const SNCBlobVerData& new_info)
{
    if (!s_IsCurVerOlder(m_CurData, &new_info))
        return false;

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

bool
CNCBlobAccessor::IsPurged(const string& cache) const
{
    if (cache.empty()) {
        return false;
    }
    bool res = false;
    s_ConsListLock.Lock();
    map<string, Uint8>::const_iterator i = s_Forgets.find(cache);
    if (i != s_Forgets.end()) {
        res = GetCurBlobCreateTime() < i->second;
    }
    s_ConsListLock.Unlock();
    return res;
}

bool
CNCBlobAccessor::Purge(const string& cache, Uint8 when)
{
    bool res=false;
    Uint8 lifespan = 9U * 24U * 3600U * (Uint8)kUSecsPerSecond; // 9 days
    Uint8 now = CSrvTime::Current().AsUSec();
    Uint8 longago = now > lifespan ? (now-lifespan) : 0;
    
    s_ConsListLock.Lock();
    ERASE_ITERATE( TForgets, f, s_Forgets) {
        if (f->second < longago) {
            s_Forgets.erase(f);
            res=true;
        }
    }
    if (!cache.empty()) {
        TForgets::const_iterator i = s_Forgets.find(cache);
        if (i == s_Forgets.end() || i->second < when) {
            s_Forgets[cache] = when;
            res=true;
        }
    }
    s_ConsListLock.Unlock();
    return res;
}

string CNCBlobAccessor::GetPurgeData(char separator)
{
    string res;
    Purge("",0);
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

END_NCBI_SCOPE
