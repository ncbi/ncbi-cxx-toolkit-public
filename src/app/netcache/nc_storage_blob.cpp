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
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE


#define NCBI_USE_ERRCODE_X  NetCache_Storage


///
static CObjPool<CNCBlobBuffer>  s_BufferPool;
///
static CObjPool<SNCBlobVerData> s_VerDataPool;


inline
CNCBlobBuffer::CNCBlobBuffer(void)
{}

CNCBlobBuffer::~CNCBlobBuffer(void)
{}

void
CNCBlobBuffer::DeleteThis(void)
{
    m_Size = 0;
    s_BufferPool.Return(this);
}


inline
SNCBlobVerData::SNCBlobVerData(void)
{}

SNCBlobVerData::~SNCBlobVerData(void)
{}


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
CNCBlobVerManager::x_DeleteBlobKey(void)
{
    m_CacheData->SetDeleted(true);
    g_NCStorage->DeleteBlobKey(m_Key);
}

inline void
CNCBlobVerManager::x_RestoreBlobKey(void)
{
    m_CacheData->SetDeleted(false);
    g_NCStorage->RestoreBlobKey(m_Key);
}

inline void
CNCBlobVerManager::DeleteVersion(const SNCBlobVerData* ver_data)
{
    _VERIFY(sx_LockCacheData(m_CacheData) == this);
    if (m_CurVersion == ver_data) {
        m_CurVersion.Reset();
        x_SetFlag(fActionTaken, true);
    }
    sx_UnlockCacheData(m_CacheData, this);
}

inline
CNCBlobVerManager::CNCBlobVerManager(const string& key,
                                     CNCCacheData* cache_data)
    : m_State(0),
      m_CacheData(cache_data),
      m_Key(key)
{}

CNCBlobVerManager::~CNCBlobVerManager(void)
{}

inline CNCBlobVerManager*
CNCBlobVerManager::sx_LockCacheData(CNCCacheData* cache_data)
{
    CNCBlobVerManager* mgr = cache_data->SetVerManager(kLockedManager);
#ifdef _DEBUG
    int cnt = 0;
#endif
    while (mgr == kLockedManager) {
#ifdef _DEBUG
        if (++cnt > 5)
            INFO_POST("Locking cache data is too slow " << cnt);
#endif
        NCBI_SCHED_YIELD();
        mgr = cache_data->SetVerManager(kLockedManager);
    }
    return mgr;
}

inline void
CNCBlobVerManager::sx_UnlockCacheData(CNCCacheData*      cache_data,
                                      CNCBlobVerManager* new_mgr)
{
    _VERIFY(cache_data->SetVerManager(new_mgr) == kLockedManager);
}

inline CNCBlobVerManager*
CNCBlobVerManager::Get(const string& key,
                       CNCCacheData* cache_data,
                       bool          for_new_version)
{
    bool need_key_restore = false;

    CNCBlobVerManager* mgr = sx_LockCacheData(cache_data);
    if (mgr) {
        _ASSERT(mgr->m_Key == key  &&  mgr->m_CacheData == cache_data);
    }
    else {
        mgr = new CNCBlobVerManager(key, cache_data);
    }
    mgr->x_IncRef();
    if (for_new_version) {
        if (mgr->x_IsFlagSet(fDeletingKey)) {
            mgr->x_SetFlag(fNeedRestoreKey, true);
        }
        else if (cache_data->IsDeleted()) {
            need_key_restore = true;
            cache_data->SetDeleted(false);
        }
    }
    sx_UnlockCacheData(cache_data, mgr);

    if (need_key_restore) {
        g_NCStorage->RestoreBlobKey(key);
    }
    return mgr;
}

inline void
CNCBlobVerManager::Release(void)
{
    bool need_notify = false;
    _VERIFY(sx_LockCacheData(m_CacheData) == this);
    if (x_GetRef() == 1) {
        need_notify = true;
    }
    else {
        x_DecRef();
    }
    sx_UnlockCacheData(m_CacheData, this);
    if (need_notify) {
        if (m_CacheData->IsCreatedCaching()  &&  x_IsFlagSet(fActionTaken))
            g_NCStorage->NotifyAfterCaching(this);
        else
            Notify();
    }
}

void
CNCBlobVerManager::OnBlockedOpFinish(void)
{
    enum EAsyncAction {
        eNoAction,
        eUpdateVersion,
        eDeleteKey
    };

    _VERIFY(sx_LockCacheData(m_CacheData) == this);
    for (;;) {
        if (x_GetRef() > 1) {
            x_DecRef();
            sx_UnlockCacheData(m_CacheData, this);
            return;
        }
        EAsyncAction action = eNoAction;
        CRef<SNCBlobVerData> cur_ver;
        if (m_CurVersion  &&  m_CurVersion->need_write) {
            action = eUpdateVersion;
            cur_ver = m_CurVersion;
        }
        else if (!m_CurVersion  &&  !m_CacheData->IsDeleted()) {
            action = eDeleteKey;
            x_SetFlag(fDeletingKey, true);
        }

        if (action != eNoAction) {
            sx_UnlockCacheData(m_CacheData, this);
            switch (action) {
            case eUpdateVersion:
                cur_ver->need_write = false;
                g_NCStorage->UpdateBlobInfo(m_Key, cur_ver);
                break;
            case eDeleteKey:
                g_NCStorage->DeleteBlobKey(m_Key);
                break;
            case eNoAction:
                break;
            }
            _VERIFY(sx_LockCacheData(m_CacheData) == this);
            if (action == eDeleteKey) {
                x_SetFlag(fDeletingKey, false);
                if (x_IsFlagSet(fNeedRestoreKey)) {
                    x_SetFlag(fNeedRestoreKey, false);
                    sx_UnlockCacheData(m_CacheData, this);
                    g_NCStorage->RestoreBlobKey(m_Key);
                    _VERIFY(sx_LockCacheData(m_CacheData) == this);
                }
                else {
                    m_CacheData->SetDeleted(true);
                }
            }
            continue;
        }
        break;
    }
    if (m_CurVersion)
        m_CacheData->SetCoords(m_CurVersion->coords);
    else
        m_CacheData->SetCoords(0, 0);
    sx_UnlockCacheData(m_CacheData, NULL);

    x_SetFlag(fCleaningMgr, true);
    m_CurVersion.Reset();
    delete this;
}

inline ENCBlockingOpResult
CNCBlobVerManager::GetCurVersion(CRef<SNCBlobVerData>* ver_data,
                                 INCBlockedOpListener* listener)
{
    if (m_CurVerTrigger.GetState() == eNCOpNotDone
        &&  m_CacheData->GetBlobId() == 0)
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
        _VERIFY(sx_LockCacheData(m_CacheData) == this);
        *ver_data = m_CurVersion;
        sx_UnlockCacheData(m_CacheData, this);
    }}

    return eNCSuccessNoBlock;
}

void
CNCBlobVerManager::x_ReadCurVersion(void)
{
    TNCBlobId blob_id = m_CacheData->GetBlobId();
    _ASSERT(blob_id != 0);
    m_CurVersion.Reset(s_VerDataPool.Get());
    m_CurVersion->manager = this;
    m_CurVersion->coords.blob_id = blob_id;
    m_CurVersion->coords.meta_id = m_CacheData->GetMetaId();
    m_CurVersion->need_write = false;
    if (!g_NCStorage->ReadBlobInfo(m_CurVersion)) {
        abort();
        ERR_POST_X(20, Critical
                       << "Problem reading meta-information about blob "
                       << g_NCStorage->UnpackKeyForLogs(m_Key));
        DeleteVersion(m_CurVersion);
    }
}

CRef<SNCBlobVerData>
CNCBlobVerManager::CreateNewVersion(void)
{
    if (m_CacheData->IsDeleted())
        x_RestoreBlobKey();

    SNCBlobVerData* data = s_VerDataPool.Get();
    data->manager       = this;
    data->create_time   = 0;
    data->size          = 0;
    data->disk_size     = 0;
    data->old_style     = false;
    data->cnt_reads     = 0;
    data->need_write    = false;
    data->data_trigger.SetState(eNCOpCompleted);
    g_NCStorage->GetNewBlobCoords(&data->coords);
    return Ref(data);
}

bool
CNCBlobVerManager::SetCurVerIfNewer(SNCBlobVerData* ver_data)
{
    CRef<SNCBlobVerData> old_ver(ver_data);
    bool result = false;
    {{
        _VERIFY(sx_LockCacheData(m_CacheData) == this);
        if (!x_IsFlagSet(fActionTaken)
            &&  (ver_data->create_time > m_CurVersion->create_time
                 ||  ver_data->dead_time > m_CurVersion->dead_time))
        {
            old_ver->data_trigger.SetState(eNCOpNotDone);
            old_ver.Swap(m_CurVersion);
            result = true;
        }
        sx_UnlockCacheData(m_CacheData, this);
    }}
    old_ver.Reset();
    return result;
}

inline void
CNCBlobVerManager::FinalizeWriting(SNCBlobVerData* ver_data)
{
    g_NCStorage->WriteBlobInfo(m_Key, ver_data);
    CRef<SNCBlobVerData> old_ver(ver_data);
    {{
        _VERIFY(sx_LockCacheData(m_CacheData) == this);
        old_ver.Swap(m_CurVersion);
        x_SetFlag(fActionTaken, true);
        sx_UnlockCacheData(m_CacheData, this);
    }}
    old_ver.Reset();
}

inline void
CNCBlobVerManager::ReleaseVerData(const SNCBlobVerData* ver_data)
{
    if (!x_IsFlagSet(fCleaningMgr)) {
        g_NCStorage->DeleteBlobInfo(ver_data);
    }
}

void
SNCBlobVerData::DeleteThis(void)
{
    manager->ReleaseVerData(this);
    chunks.clear();
    password.clear();
    data.Reset();
    data_trigger.Reset();
    s_VerDataPool.Return(this);
}


void
CNCBlobAccessor::Prepare(const string& key,
                         const string& password,
                         ENCAccessType access_type)
{
    m_BlobKey       = key;
    m_Password      = password;
    m_AccessType    = access_type;
    m_Initialized   = false;
    m_InitListener  = NULL;
    m_VerManager    = NULL;
    m_CurChunk      = 0;
    m_ChunkPos      = 0;
    m_SizeRead      = 0;
}

void
CNCBlobAccessor::Initialize(CNCCacheData* cache_data)
{
    _ASSERT(!m_Initialized);

    if (cache_data) {
        m_VerManager = CNCBlobVerManager::Get(m_BlobKey, cache_data,
                                              m_AccessType == eNCCreate);
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
        if (IsBlobExists()) {
            CNCStat::AddBlobRead(m_SizeRead, m_CurData->size);
        }
        break;
    case eNCCreate:
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
    if (!m_VerManager) {
        _ASSERT(m_AccessType == eNCRead  ||  m_AccessType == eNCReadData
                ||  m_AccessType == eNCGCDelete);
        return eNCSuccessNoBlock;
    }
    if (m_VerManager->GetCurVersion(&m_CurData, listener) == eNCWouldBlock)
        return eNCWouldBlock;
    if (IsAuthorized()) {
        if (m_AccessType == eNCCreate) {
            m_NewData = m_VerManager->CreateNewVersion();
        }
        else if ((m_AccessType == eNCRead  || m_AccessType == eNCReadData)
                 &&  IsBlobExists())
        {
            Int8 cnt_reads = ++m_CurData->cnt_reads;
            m_CurData->need_write = true;
            CNCStat::AddBlobReadAttempt(cnt_reads,
                                        int(m_CurData->create_time >> 32));
        }
    }
    return eNCSuccessNoBlock;
}

inline bool
CNCBlobAccessor::sx_IsOnlyOneChunk(SNCBlobVerData* ver_data)
{
    return !ver_data->old_style  &&  ver_data->size <= Int8(kNCMaxBlobChunkSize);
}

NCBI_NORETURN
inline void
CNCBlobAccessor::x_DelCorruptedVersion(void)
{
    ERR_POST_X(19, "Database information about blob "
                   << g_NCStorage->UnpackKeyForLogs(m_BlobKey)
                   << " is corrupted. Blob will be deleted");
    m_VerManager->DeleteVersion(m_CurData);
    NCBI_THROW(CNCBlobStorageException, eCorruptedDB,
               "Corrupted blob information");
}

inline void
CNCBlobAccessor::x_ReadChunkData(TNCChunkId chunk_id, CNCBlobBuffer* buffer)
{
    if (!g_NCStorage->ReadChunkData(m_CurData->coords.data_id, chunk_id, buffer))
    {
        x_DelCorruptedVersion();
    }
}

inline void
CNCBlobAccessor::x_ReadNextChunk(void)
{
    x_ReadChunkData(m_CurData->chunks[m_CurChunk++], m_Buffer);
    bool is_last = m_CurChunk == m_CurData->chunks.size();
    if ((is_last  &&  m_Buffer->GetSize() != m_CurData->size % kNCMaxBlobChunkSize)
        ||  (!is_last  &&  m_Buffer->GetSize() != kNCMaxBlobChunkSize))
    {
        x_DelCorruptedVersion();
    }
}

inline void
CNCBlobAccessor::x_ReadSingleChunk(void)
{
    x_ReadChunkData(m_CurData->coords.blob_id, m_CurData->data);
    if (Int8(m_CurData->data->GetSize()) != m_CurData->size) {
        x_DelCorruptedVersion();
    }
}

inline void
CNCBlobAccessor::x_ReadChunkIds(void)
{
    if (m_CurData->size == 0)
        return;
    g_NCStorage->ReadChunkIds(m_CurData);
    Int8 max_size = Int8(m_CurData->chunks.size()) * kNCMaxBlobChunkSize;
    Int8 min_size = (max_size == 0? 0: max_size - kNCMaxBlobChunkSize);
    if (m_CurData->size <= min_size  ||  m_CurData->size > max_size) {
        x_DelCorruptedVersion();
    }
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
        else {
            x_ReadChunkIds();
        }
    }
    if (sx_IsOnlyOneChunk(m_CurData)) {
        m_Buffer.Reset(m_CurData->data);
    }
    else {
        m_Buffer.Reset(s_BufferPool.Get());
        m_Buffer->Resize(0);
    }
    return eNCSuccessNoBlock;
}

size_t
CNCBlobAccessor::ReadData(void* buffer, size_t buf_size)
{
    _ASSERT(IsBlobExists()  &&  m_AccessType == eNCReadData
            &&  m_Buffer  &&  m_CurData->size != 0);

    if (m_ChunkPos == m_Buffer->GetSize()) {
        m_Buffer->Resize(0);
        m_ChunkPos = 0;

        if (m_CurChunk >= m_CurData->chunks.size())
            return 0;
        x_ReadNextChunk();
    }
    buf_size = min(buf_size, m_Buffer->GetSize() - m_ChunkPos);
    memcpy(buffer, m_Buffer->GetData() + m_ChunkPos, buf_size);
    m_ChunkPos += buf_size;
    m_SizeRead += buf_size;
    return buf_size;
}

void
CNCBlobAccessor::WriteData(const void* data, size_t size)
{
    _ASSERT(m_AccessType == eNCCreate);

    if (!m_Buffer) {
        m_Buffer.Reset(s_BufferPool.Get());
        m_Buffer->Resize(0);
    }
    while (size != 0) {
        size_t write_size = min(size, kNCMaxBlobChunkSize - m_Buffer->GetSize());
        memcpy(m_Buffer->GetData() + m_Buffer->GetSize(), data, write_size);
        m_Buffer->Resize(m_Buffer->GetSize() + write_size);

        if (m_Buffer->GetSize() == kNCMaxBlobChunkSize) {
            g_NCStorage->WriteNextChunk(m_NewData, m_Buffer);
            m_Buffer->Resize(0);
        }
        data = static_cast<const char*>(data) + write_size;
        size -= write_size;
        m_NewData->size += write_size;
    }
}

void
CNCBlobAccessor::Finalize(void)
{
    _ASSERT(m_AccessType == eNCCreate);

    if (m_Buffer  &&  m_Buffer->GetSize() != 0) {
        if (sx_IsOnlyOneChunk(m_NewData)) {
            g_NCStorage->WriteSingleChunk(m_NewData, m_Buffer);
            m_NewData->data = m_Buffer;
        }
        else {
            g_NCStorage->WriteNextChunk(m_NewData, m_Buffer);
        }
    }
    m_VerManager->FinalizeWriting(m_NewData);
    m_CurData = m_NewData;
}

void
CNCBlobAccessor::DeleteBlob(void)
{
    _ASSERT(m_AccessType == eNCDelete  ||  m_AccessType == eNCGCDelete);

    if (!IsBlobExists())
        return;

    m_VerManager->DeleteVersion(m_CurData);
}

void
CNCBlobAccessor::CleanOvereatenMemory(void)
{
    s_BufferPool.Clear();
    s_VerDataPool.Clear();
}

END_NCBI_SCOPE
