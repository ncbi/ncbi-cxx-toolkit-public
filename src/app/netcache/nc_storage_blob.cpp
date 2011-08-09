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

#include <ncbi_pch.hpp>

#include "nc_storage_blob.hpp"
#include "nc_storage.hpp"
#include "nc_stat.hpp"
#include "netcached.hpp"
#include "distribution_conf.hpp"
#include "nc_memory.hpp"


BEGIN_NCBI_SCOPE


///
static CObjPool<CNCBlobBuffer>  s_BufferPool(50);
///
static CObjPool<SNCBlobVerData> s_VerDataPool(50);


CNCBlobBuffer::CNCBlobBuffer(void)
    : m_Size(0)
{}

CNCBlobBuffer::~CNCBlobBuffer(void)
{}

void
CNCBlobBuffer::DeleteThis(void)
{
    m_Size = 0;
    if (CNCMemManager::IsOnAlert())
        delete this;
    else
        s_BufferPool.Return(this);
}


SNCBlobVerData::SNCBlobVerData(void)
{}

SNCBlobVerData::~SNCBlobVerData(void)
{}


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
        else if (cur_ver->dead_time != new_ver->dead_time)
            return cur_ver->dead_time < new_ver->dead_time;
        else if (cur_ver->expire != new_ver->expire)
            return cur_ver->expire < new_ver->expire;
        else
            return cur_ver->ver_expire < new_ver->ver_expire;
    }
    return true;
}


///
static CNCBlobVerManager* const kLockedManager
                                = reinterpret_cast<CNCBlobVerManager*>(size_t(1));


inline unsigned int
CNCBlobVerManager::x_IncRef(void)
{
    unsigned int ref_cnt = (m_State & kRefCntMask) + 1;
    m_State = (m_State & kFlagsMask) + ref_cnt;
    return ref_cnt;
}

inline unsigned int
CNCBlobVerManager::x_DecRef(void)
{
    unsigned int ref_cnt = (m_State & kRefCntMask) - 1;
    _ASSERT(ref_cnt + 1 != 0);
    m_State = (m_State & kFlagsMask) + ref_cnt;
    return ref_cnt;
}

inline unsigned int
CNCBlobVerManager::x_GetRef(void)
{
    return m_State & kRefCntMask;
}

inline void
CNCBlobVerManager::x_SetFlag(unsigned int flag, bool value)
{
    if (value)
        m_State |= flag;
    else
        m_State &= ~flag;
}

inline bool
CNCBlobVerManager::x_IsFlagSet(unsigned int flag)
{
    return (m_State & flag) != 0;
}

inline void
CNCBlobVerManager::x_RestoreBlobKey(void)
{
    g_NCStorage->RestoreBlobKey(m_Slot, m_Key, m_CacheData);
}

void
CNCBlobVerManager::x_DeleteCurVersion(void)
{
    g_NCStorage->ChangeCacheDeadTime(m_CacheData, 0, 0);
    m_CacheData->expire = 0;
    m_CurVersion.Reset();
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

inline
CNCBlobVerManager::CNCBlobVerManager(Uint2         slot,
                                     const string& key,
                                     SNCCacheData* cache_data)
    : m_State(0),
      m_Slot(slot),
      m_CacheData(cache_data),
      m_Key(key)
{}

CNCBlobVerManager::~CNCBlobVerManager(void)
{}

CNCBlobVerManager*
CNCBlobVerManager::Get(Uint2         slot,
                       const string& key,
                       SNCCacheData* cache_data,
                       bool          for_new_version)
{
    bool need_key_restore = false;

    cache_data->lock.Lock();
    CNCBlobVerManager* mgr = cache_data->ver_mgr;
    if (mgr) {
        _ASSERT(mgr->m_Key == key  &&  mgr->m_CacheData == cache_data);
    }
    else {
        mgr = new CNCBlobVerManager(slot, key, cache_data);
        cache_data->ver_mgr = mgr;
    }
    mgr->x_IncRef();
    if (for_new_version) {
        if (mgr->x_IsFlagSet(fDeletingKey)) {
            mgr->x_SetFlag(fNeedRestoreKey, true);
        }
        else if (cache_data->key_deleted) {
            need_key_restore = true;
            cache_data->key_deleted = false;
        }
    }
    cache_data->lock.Unlock();

    if (need_key_restore) {
        g_NCStorage->RestoreBlobKey(slot, key, cache_data);
    }
    return mgr;
}

void
CNCBlobVerManager::Release(void)
{
    bool need_notify = false;
    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->ver_mgr == this);
    if (x_GetRef() == 1) {
        need_notify = true;
    }
    else {
        x_DecRef();
    }
    m_CacheData->lock.Unlock();
    if (need_notify)
        Notify();
}

void
CNCBlobVerManager::OnBlockedOpFinish(void)
{
    enum EAsyncAction {
        eNoAction,
        eUpdateVersion,
        eDeleteKey,
        eDeleteCurVer
    };

    m_CacheData->lock.Lock();
    _ASSERT(m_CacheData->ver_mgr == this);
    for (;;) {
        if (x_GetRef() > 1) {
            x_DecRef();
            m_CacheData->lock.Unlock();
            return;
        }
        EAsyncAction action = eNoAction;
        CRef<SNCBlobVerData> cur_ver;
        if (m_CurVersion) {
            if (m_CurVersion->need_write) {
                action = eUpdateVersion;
                cur_ver = m_CurVersion;
            }
            else if (m_CurVersion->dead_time <= int(time(NULL))) {
                action = eDeleteCurVer;
                cur_ver = m_CurVersion;
            }
        }
        else if (m_CurVerTrigger.GetState() == eNCOpCompleted
                 &&  !m_CacheData->key_deleted)
        {
            action = eDeleteKey;
            x_SetFlag(fDeletingKey, true);
        }

        if (action != eNoAction) {
            m_CacheData->lock.Unlock();
            switch (action) {
            case eUpdateVersion:
                cur_ver->need_write = false;
                g_NCStorage->WriteBlobInfo(m_Key, cur_ver, NULL);
                break;
            case eDeleteKey:
                g_NCStorage->DeleteBlobKey(m_Slot, m_Key);
                break;
            case eDeleteCurVer:
                DeleteVersion(cur_ver);
                cur_ver.Reset();
                break;
            case eNoAction:
                break;
            }
            m_CacheData->lock.Lock();
            _ASSERT(m_CacheData->ver_mgr == this);
            if (action == eDeleteKey) {
                x_SetFlag(fDeletingKey, false);
                if (x_IsFlagSet(fNeedRestoreKey)) {
                    x_SetFlag(fNeedRestoreKey, false);
                    m_CacheData->lock.Unlock();
                    g_NCStorage->RestoreBlobKey(m_Slot, m_Key, m_CacheData);
                    m_CacheData->lock.Lock();
                    _ASSERT(m_CacheData->ver_mgr == this);
                }
                else {
                    m_CacheData->key_deleted = true;
                }
            }
            else if (action == eUpdateVersion  &&  cur_ver == m_CurVersion) {
                g_NCStorage->ChangeCacheDeadTime(m_CacheData, cur_ver->coord, cur_ver->dead_time);
                m_CacheData->expire = m_CurVersion->expire;
                m_CacheData->ver_expire = m_CurVersion->ver_expire;
            }
            continue;
        }
        break;
    }
    m_CacheData->ver_mgr = NULL;
    m_CacheData->lock.Unlock();

    x_SetFlag(fCleaningMgr, true);
    m_CurVersion.Reset();
    delete this;
}

ENCBlockingOpResult
CNCBlobVerManager::GetCurVersion(CRef<SNCBlobVerData>* ver_data,
                                 INCBlockedOpListener* listener)
{
    if (m_CurVerTrigger.GetState() == eNCOpNotDone
        &&  m_CacheData->coord == 0)
    {
        // We have to detect non-existent blobs asap to avoid segmentation
        // faults.
        m_CurVerTrigger.SetState(eNCOpCompleted);
    }
    {{
        CNCLongOpGuard op_guard(m_CurVerTrigger);

        if (op_guard.Start(listener) == eNCWouldBlock)
            return eNCWouldBlock;

        if (!op_guard.IsCompleted())
            x_ReadCurVersion();
    }}

    {{
        CRef<SNCBlobVerData> cur_ver;
        m_CacheData->lock.Lock();
        _ASSERT(m_CacheData->ver_mgr == this);
        if (m_CurVersion  &&  m_CurVersion->dead_time <= int(time(NULL))) {
            cur_ver = m_CurVersion;
            x_DeleteCurVersion();
        }
        *ver_data = m_CurVersion;
        m_CacheData->lock.Unlock();
    }}

    return eNCSuccessNoBlock;
}

void
CNCBlobVerManager::x_ReadCurVersion(void)
{
    _ASSERT(m_CacheData->coord != 0);
    m_CurVersion.Reset(s_VerDataPool.Get());
    m_CurVersion->manager = this;
    m_CurVersion->coord = m_CacheData->coord;
    m_CurVersion->create_time = m_CacheData->create_time;
    m_CurVersion->size = m_CacheData->size;
    m_CurVersion->need_write = false;
    m_CurVersion->has_error  = false;
    if (!g_NCStorage->ReadBlobInfo(m_CurVersion)) {
        ERR_POST(Critical << "Problem reading meta-information about blob "
                          << g_NCStorage->UnpackKeyForLogs(m_Key));
        CRef<SNCBlobVerData> cur_ver(m_CurVersion);
        DeleteVersion(cur_ver);
    }
}

CRef<SNCBlobVerData>
CNCBlobVerManager::CreateNewVersion(void)
{
    if (m_CacheData->key_deleted)
        x_RestoreBlobKey();

    SNCBlobVerData* data = s_VerDataPool.Get();
    data->manager       = this;
    data->coord         = 0;
    data->create_time   = 0;
    data->expire        = 0;
    data->dead_time     = 0;
    data->size          = 0;
    data->blob_ver      = 0;
    data->disk_size     = 0;
    data->first_chunk_coord = 0;
    data->first_map_coord = 0;
    data->last_map_coord = 0;
    data->need_write    = false;
    data->has_error     = false;
    data->data_trigger.SetState(eNCOpCompleted);
    return Ref(data);
}

bool
CNCBlobVerManager::FinalizeWriting(SNCBlobVerData* ver_data)
{
    CRef<SNCBlobVerData> old_ver(ver_data);
    {{
        m_CacheData->lock.Lock();
        _ASSERT(m_CacheData->ver_mgr == this);
        if (ver_data->dead_time > int(time(NULL))
            &&  s_IsCurVerOlder(m_CurVersion, ver_data))
        {
            old_ver.Swap(m_CurVersion);
            g_NCStorage->ChangeCacheDeadTime(m_CacheData, m_CurVersion->coord, m_CurVersion->dead_time);
            m_CacheData->create_id = m_CurVersion->create_id;
            m_CacheData->create_server = m_CurVersion->create_server;
            m_CacheData->create_time = m_CurVersion->create_time;
            m_CacheData->expire = m_CurVersion->expire;
            m_CacheData->ver_expire = m_CurVersion->ver_expire;
            m_CacheData->size = m_CurVersion->size;
        }
        m_CacheData->lock.Unlock();
    }}
    old_ver.Reset();
    return true;
}

inline void
CNCBlobVerManager::ReleaseVerData(const SNCBlobVerData* ver_data)
{
    if (!x_IsFlagSet(fCleaningMgr))
        g_NCStorage->DeleteBlobInfo(ver_data);
}

void
SNCBlobVerData::DeleteThis(void)
{
    manager->ReleaseVerData(this);
    password.clear();
    data.Reset();
    data_trigger.Reset();
    if (CNCMemManager::IsOnAlert())
        delete this;
    else
        s_VerDataPool.Return(this);
}


void
CNCBlobAccessor::Prepare(const string& key,
                         const string& password,
                         Uint2         slot,
                         ENCAccessType access_type)
{
    m_BlobKey       = key;
    m_Password      = password;
    m_BlobSlot      = slot;
    m_AccessType    = access_type;
    m_Initialized   = false;
    m_HasError      = false;
    m_InitListener  = NULL;
    m_VerManager    = NULL;
    m_CurChunk      = 0;
    m_ChunkPos      = 0;
    m_SizeRead      = 0;
}

void
CNCBlobAccessor::Initialize(SNCCacheData* cache_data)
{
    _ASSERT(!m_Initialized);

    if (cache_data) {
        bool new_version = m_AccessType == eNCCreate
                           ||  m_AccessType == eNCCopyCreate;
        m_VerManager = CNCBlobVerManager::Get(m_BlobSlot, m_BlobKey,
                                              cache_data, new_version);
    }
    m_Initialized = true;
    if (m_InitListener) {
        m_InitListener->Notify();
        m_InitListener = NULL;
    }
}

void
CNCBlobAccessor::Deinitialize(void)
{
    _ASSERT(m_Initialized);

    switch (m_AccessType) {
    case eNCReadData:
        if (IsBlobExists()  &&  !IsCurBlobExpired())
            CNCStat::AddBlobRead(m_SizeRead, m_CurData->size);
        break;
    case eNCCreate:
    case eNCCopyCreate:
        if (m_NewData)
            CNCStat::AddBlobWritten(m_NewData->size, m_NewData == m_CurData);
        break;
    default:
        break;
    }

    m_Buffer.Reset();
    m_NewData.Reset();
    m_CurData.Reset();
    if (m_VerManager) {
        m_VerManager->Release();
        m_VerManager = NULL;
    }
}

// Should be inline and in the header but it will cause headers circular
// dependency.
void
CNCBlobAccessor::Release(void)
{
    Deinitialize();
    g_NCStorage->ReturnAccessor(this);
}

ENCBlockingOpResult
CNCBlobAccessor::ObtainMetaInfo(INCBlockedOpListener* listener)
{
    if (!IsInitialized()) {
        m_InitListener = listener;
        return eNCWouldBlock;
    }
    if (!m_VerManager)
        return eNCSuccessNoBlock;
    if (m_VerManager->GetCurVersion(&m_CurData, listener) == eNCWouldBlock)
        return eNCWouldBlock;
    m_ChunkMap.map_num = Uint4(-1);
    if ((IsAuthorized()  &&  m_AccessType == eNCCreate)
        ||  m_AccessType == eNCCopyCreate)
    {
        m_NewData = m_VerManager->CreateNewVersion();
        m_NewData->password = m_Password;
        m_ChunkMap.map_num = 0;
        m_ChunkMap.cur_chunk_idx = 0;
    }
    return eNCSuccessNoBlock;
}

inline bool
CNCBlobAccessor::sx_IsOnlyOneChunk(SNCBlobVerData* ver_data)
{
    return ver_data->size <= kNCMaxBlobChunkSize;
}

void
CNCBlobAccessor::x_DelCorruptedVersion(void)
{
    //abort();
    ERR_POST(Critical << "Database information about blob "
                      << g_NCStorage->UnpackKeyForLogs(m_BlobKey)
                      << " is corrupted. Blob will be deleted");
    m_VerManager->DeleteVersion(m_CurData);
    m_CurData->has_error = true;
    m_HasError = true;
}

void
CNCBlobAccessor::x_ReadChunkData(CNCBlobBuffer* buffer)
{
    if (!g_NCStorage->ReadChunkData(m_CurData, &m_ChunkMap, m_CurChunk, buffer))
        x_DelCorruptedVersion();
}

void
CNCBlobAccessor::x_ReadNextChunk(void)
{
    x_ReadChunkData(m_Buffer);
    if (m_HasError)
        return;

    size_t need_size = size_t(m_CurData->size - GetPosition());
    if (need_size > kNCMaxBlobChunkSize)
        need_size = kNCMaxBlobChunkSize;
    if (m_Buffer->GetSize() != need_size)
        x_DelCorruptedVersion();
}

void
CNCBlobAccessor::x_ReadSingleChunk(void)
{
    if (m_CurData->size != 0)
        x_ReadChunkData(m_CurData->data);
    if (m_CurData->data->GetSize() != m_CurData->size)
        x_DelCorruptedVersion();
}

ENCBlockingOpResult
CNCBlobAccessor::ObtainFirstData(INCBlockedOpListener* listener)
{
    _ASSERT(!m_Buffer);
    CNCLongOpGuard op_guard(m_CurData->data_trigger);

    if (op_guard.Start(listener) == eNCWouldBlock)
        return eNCWouldBlock;

    if (!op_guard.IsCompleted()) {
        _ASSERT(!m_CurData->data);
        if (sx_IsOnlyOneChunk(m_CurData)) {
            m_CurData->data.Reset(s_BufferPool.Get());
            x_ReadSingleChunk();
        }
    }
    if (m_CurData->has_error) {
        m_HasError = true;
    }
    else if (sx_IsOnlyOneChunk(m_CurData)) {
        m_Buffer.Reset(m_CurData->data);
    }
    else {
        m_Buffer.Reset(s_BufferPool.Get());
        m_Buffer->Resize(0);
        x_ReadNextChunk();
    }
    return eNCSuccessNoBlock;
}

size_t
CNCBlobAccessor::GetDataSize(void)
{
    if (!m_Buffer)
        return 0;
    if (m_CurData->has_error) {
        m_HasError = true;
        return 0;
    }
    if (m_ChunkPos == m_Buffer->GetSize()) {
        if (GetPosition() == m_CurData->size)
            return 0;

        m_Buffer->Resize(0);
        m_ChunkPos = 0;
        ++m_CurChunk;
        x_ReadNextChunk();
    }
    if (m_HasError)
        return 0;

    return m_Buffer->GetSize() - m_ChunkPos;
}

size_t
CNCBlobAccessor::GetWriteMemSize(void)
{
    _ASSERT(m_AccessType == eNCCreate  ||  m_AccessType == eNCCopyCreate);

    if (!m_Buffer) {
        m_Buffer.Reset(s_BufferPool.Get());
        m_Buffer->Resize(0);
    }
    if (m_Buffer->GetSize() == kNCMaxBlobChunkSize) {
        if (!g_NCStorage->WriteChunkData(m_NewData, &m_ChunkMap, m_CurChunk, m_Buffer)) {
            m_HasError = true;
            return 0;
        }
        m_Buffer->Resize(0);
        ++m_CurChunk;
    }

    return kNCMaxBlobChunkSize - m_Buffer->GetSize();
}

void
CNCBlobAccessor::Finalize(void)
{
    if (m_Buffer  &&  m_Buffer->GetSize() != 0) {
        if (!g_NCStorage->WriteChunkData(m_NewData, &m_ChunkMap, m_CurChunk, m_Buffer)) {
            m_HasError = true;
            return;
        }
        if (sx_IsOnlyOneChunk(m_NewData))
            m_NewData->data = m_Buffer;
    }

    if (g_NCStorage->WriteBlobInfo(m_BlobKey, m_NewData, &m_ChunkMap)
        &&  m_VerManager->FinalizeWriting(m_NewData))
    {
        m_CurData = m_NewData;
    }
    else {
        m_HasError = true;
    }
}

bool
CNCBlobAccessor::ReplaceBlobInfo(const SNCBlobVerData& new_info)
{
    if (!s_IsCurVerOlder(m_CurData, &new_info))
        return false;
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

void
CNCBlobAccessor::DeleteBlob(int dead_time)
{
    _ASSERT(m_AccessType == eNCGCDelete);

    if (!IsBlobExists())
        return;

    m_VerManager->DeleteVersion(m_CurData);
}

string
CNCBlobAccessor::GetCurPassword(void) const
{
    if (m_CurData->password.empty())
        return m_CurData->password;
    else
        return g_NCStorage->PrintablePassword(m_CurData->password);
}

END_NCBI_SCOPE
