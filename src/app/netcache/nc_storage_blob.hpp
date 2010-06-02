#ifndef NETCACHE__NC_STORAGE_BLOB__HPP
#define NETCACHE__NC_STORAGE_BLOB__HPP
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
 * Authors:  Pavel Ivanov
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/obj_pool.hpp>

#include "nc_db_info.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCBlobStorage;
class CNCBlobAccessor;


///
class CNCCacheData
{
public:
    ///
    CNCCacheData(void);
    ///
    CNCCacheData(TNCBlobId blob_id, TNCDBFileId meta_id);
    ///
    CNCCacheData(const CNCCacheData& other);
    ///
    CNCCacheData& operator= (const CNCCacheData& other);

    ///
    TNCBlobId GetBlobId(void);
    ///
    TNCDBFileId GetMetaId(void);
    ///
    bool IsDeleted(void);
    ///
    bool IsCreatedCaching(void);
    ///
    void SetCoords(const SNCBlobCoords& coords);
    ///
    void SetCoords(TNCBlobId blob_id, TNCDBFileId meta_id);
    ///
    void SetDeleted(bool is_deleted);
    ///
    void SetCreatedCaching(bool value);
    ///
    CNCBlobVerManager* SetVerManager(CNCBlobVerManager* manager);

private:
    /// 8 bits maximum
    enum EFlags {
        fDeleted        = 0x01, ///<
        fCreatedCaching = 0x02, ///<

        kFlagsMask      = 0xFF  ///<
    };


    ///
    Uint8           m_Coords;
    ///
    void* volatile  m_VerManager;
};


///
class CNCBlobVerManager : public INCBlockedOpListener
{
public:
    ///
    static CNCBlobVerManager* Get(const string& key,
                                  CNCCacheData* cache_data,
                                  bool          for_new_version);
    ///
    void Release(void);

    ///
    ENCBlockingOpResult  GetCurVersion(CRef<SNCBlobVerData>* ver_data,
                                       INCBlockedOpListener* listener);
    ///
    CRef<SNCBlobVerData> CreateNewVersion(void);

    ///
    bool SetCurVerIfNewer(SNCBlobVerData* ver_data);
    ///
    void FinalizeWriting(SNCBlobVerData* ver_data);
    ///
    void DeleteVersion(const SNCBlobVerData* ver_data);

public:
    ///
    void ReleaseVerData(const SNCBlobVerData* ver_data);

private:
    CNCBlobVerManager(const CNCBlobVerManager&);
    CNCBlobVerManager& operator= (const CNCBlobVerManager&);

    ///
    static CNCBlobVerManager* sx_LockCacheData(CNCCacheData* cache_data);
    ///
    static void sx_UnlockCacheData(CNCCacheData*      cache_data,
                                   CNCBlobVerManager* new_mgr);
    ///
    CNCBlobVerManager(const string& key, CNCCacheData* cache_data);
    virtual ~CNCBlobVerManager(void);
    ///
    virtual void OnBlockedOpFinish(void);

    ///
    unsigned int x_IncRef(void);
    ///
    unsigned int x_DecRef(void);
    ///
    unsigned int x_GetRef(void);
    ///
    void x_SetFlag(unsigned int flag, bool value);
    ///
    bool x_IsFlagSet(unsigned int flag);

    ///
    void x_ReadCurVersion(void);
    ///
    void x_DeleteBlobKey(void);
    ///
    void x_RestoreBlobKey(void);


    ///
    enum {
        kRefCntMask     = 0x0000FFFF,
        kFlagsMask      = 0xFFFF0000,

        fCleaningMgr    = 0x00010000,
        fActionTaken    = 0x00020000,
        fDeletingKey    = 0x00040000,
        fNeedRestoreKey = 0x00080000
    };


    ///
    unsigned int volatile   m_State;
    ///
    CNCCacheData*           m_CacheData;
    ///
    string                  m_Key;
    ///
    CRef<SNCBlobVerData>    m_CurVersion;
    ///
    CNCLongOpTrigger        m_CurVerTrigger;
};


/// Type of access to NetCache blob
enum ENCAccessType {
    eNCRead,        ///< Read meta information only
    eNCReadData,    ///< Read blob data
    eNCCreate,      ///< Create blob or re-write its contents
    eNCDelete,      ///< Delete the blob
    eNCGCDelete     ///<
};


/// Object holding lock on NetCache blob.
/// Object takes care of blob creation when necessary. When it is destroyed
/// lock for the blob is released. Lock can also be released by explicit call
/// of ReleaseLock() method.
class CNCBlobAccessor
{
public:
    ///
    ENCBlockingOpResult ObtainMetaInfo(INCBlockedOpListener* listener);
    /// Check if blob exists.
    /// Method can be called only after lock is acquired.
    bool IsBlobExists(void) const;
    /// Check if password provided for accessing the blob was correct
    bool IsAuthorized(void) const;
    /// Get key of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobKey       (void) const;
    /// Get size of the blob.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    Int8          GetBlobSize      (void) const;
    /// Check if blob is already expired but not yet deleted by GC.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    bool          IsBlobExpired    (void) const;
    /// Get type of access this holder was created for
    ENCAccessType GetAccessType    (void) const;

    ///
    void SetPosition(Int8 pos);
    ///
    ENCBlockingOpResult ObtainFirstData(INCBlockedOpListener* listener);
    ///
    size_t ReadData(void* buffer, size_t buf_size);
    /// Set blob's timeout after last access before it will be deleted.
    /// Method can be called only after lock is acquired, if blob exists and
    /// lock is acquired for eNCDelete or eNCCreate access.
    void SetBlobTTL(int ttl);
    ///
    void ProlongBlobsLife(void);
    ///
    void WriteData(const void* data, size_t size);
    ///
    void Finalize(void);
    /// Delete the blob.
    /// Method can be called only after lock is acquired and
    /// lock is acquired for eNCDelete or eNCCreate access. If blob doesn't exist
    /// or was already deleted by call to this method then method is no-op.
    void DeleteBlob(void);

    /// Release blob lock.
    /// No other method can be called after call to this one.
    void Release(void);

public:
    // For internal use only

    /// Create holder of blob lock bound to the given NetCache storage.
    /// Lock holder is yet not usable after construction until Prepare()
    /// and InitializeLock() are called.
    CNCBlobAccessor(void);
    /// Add holder to the list with provided head
    void AddToList(CNCBlobAccessor*& list_head);
    /// Remove holder from the list with provided head
    void RemoveFromList(CNCBlobAccessor*& list_head);

    /// Prepare lock for the blob identified by key, subkey and version.
    /// Method only initializes necessary variables, actual acquiring of the
    /// lock happens in InitializeLock() method.
    /// If access is eNCRead and storage's IsChangeTimeOnRead() returns TRUE
    /// then holder will change blob's access time when lock is released. If
    /// access is eNCCreate then blob will be automatically deleted
    /// when lock is released if blob wasn't properly finalized.
    ///
    /// @param key
    ///   Key of the blob
    /// @param access
    ///   Required access to the blob
    void Prepare(const string& key,
                       const string& password,
                       ENCAccessType access_type);
    /// Initialize and acquire the lock.
    /// Should be called only after Prepare().
    void Initialize(CNCCacheData* cache_data);
    /// Check if lock was initialized and process of lock acquiring was started
    bool IsInitialized(void) const;
    ///
    void Deinitialize(void);

    ///
    TNCBlobId GetBlobId(void);
    ///
    TNCDBFileId GetMetaId(void);
    ///
    CRef<SNCBlobVerData> GetNewVersion(void);
    ///
    bool SetCurVerIfNewer(SNCBlobVerData* ver_data);

private:
    CNCBlobAccessor(const CNCBlobAccessor&);
    CNCBlobAccessor& operator= (const CNCBlobAccessor&);

    ///
    static bool sx_IsOnlyOneChunk(SNCBlobVerData* ver_data);
    ///
    NCBI_NORETURN void x_DelCorruptedVersion(void);
    ///
    void x_ReadChunkData(TNCChunkId chunk_id, CNCBlobBuffer* buffer);
    ///
    void x_ReadSingleChunk(void);
    ///
    void x_ReadChunkIds(void);
    ///
    void x_ReadNextChunk(void);


    /// Previous holder in double-linked list of holders
    CNCBlobAccessor*        m_PrevAccessor;
    /// Next holder in list of holders
    CNCBlobAccessor*        m_NextAccessor;
    /// Type of access requested for the blob
    ENCAccessType           m_AccessType;
    ///
    string                  m_BlobKey;
    /// Password that was used for access to the blob
    string                  m_Password;
    ///
    CNCBlobVerManager*      m_VerManager;
    /// 
    CRef<SNCBlobVerData>    m_CurData;
    /// 
    CRef<SNCBlobVerData>    m_NewData;
    ///
    bool                    m_Initialized;
    ///
    INCBlockedOpListener*   m_InitListener;
    ///
    unsigned int            m_CurChunk;
    /// Current position of reading/writing inside blob's chunk
    size_t                  m_ChunkPos;
    ///
    Int8                    m_SizeRead;
    ///
    CRef<CNCBlobBuffer>     m_Buffer;
};



inline void
CNCCacheData::SetCoords(TNCBlobId blob_id, TNCDBFileId meta_id)
{
    m_Coords = (m_Coords & kFlagsMask)
               + (Uint8(meta_id) << 40) + (Uint8(blob_id) << 8);
}

inline TNCBlobId
CNCCacheData::GetBlobId(void)
{
    return TNCBlobId((m_Coords >> 8) & kNCMaxBlobId);
}

inline TNCDBFileId
CNCCacheData::GetMetaId(void)
{
    return TNCDBFileId((m_Coords >> 40) & kNCMaxDBFileId);
}

inline bool
CNCCacheData::IsDeleted(void)
{
    return (m_Coords & fDeleted) != 0;
}

inline bool
CNCCacheData::IsCreatedCaching(void)
{
    return (m_Coords & fCreatedCaching) != 0;
}

inline void
CNCCacheData::SetDeleted(bool is_deleted)
{
    if (is_deleted)
        m_Coords |= fDeleted;
    else
        m_Coords &= ~Uint8(fDeleted);
}

inline void
CNCCacheData::SetCreatedCaching(bool value)
{
    if (value)
        m_Coords |= fCreatedCaching;
    else
        m_Coords &= ~Uint8(fCreatedCaching);
}

inline void
CNCCacheData::SetCoords(const SNCBlobCoords& coords)
{
    SetCoords(coords.blob_id, coords.meta_id);
}

inline
CNCCacheData::CNCCacheData(void)
    : m_Coords(0),
      m_VerManager(NULL)
{}

inline
CNCCacheData::CNCCacheData(TNCBlobId blob_id, TNCDBFileId meta_id)
    : m_Coords(0),
      m_VerManager(NULL)
{
    SetCoords(blob_id, meta_id);
}

inline CNCCacheData&
CNCCacheData::operator= (const CNCCacheData& other)
{
    m_Coords     = other.m_Coords;
    m_VerManager = other.m_VerManager;
    return *this;
}

inline
CNCCacheData::CNCCacheData(const CNCCacheData& other)
{
    operator=(other);
}

inline CNCBlobVerManager*
CNCCacheData::SetVerManager(CNCBlobVerManager* manager)
{
    return static_cast<CNCBlobVerManager*>(SwapPointers(&m_VerManager, manager));
}


inline
CNCBlobAccessor::CNCBlobAccessor(void)
    : m_PrevAccessor(NULL),
      m_NextAccessor(NULL)
{}

inline void
CNCBlobAccessor::AddToList(CNCBlobAccessor*& list_head)
{
    m_NextAccessor = list_head;
    if (list_head)
        list_head->m_PrevAccessor = this;
    list_head = this;
}

inline void
CNCBlobAccessor::RemoveFromList(CNCBlobAccessor*& list_head)
{
    if (m_PrevAccessor) {
        m_PrevAccessor->m_NextAccessor = m_NextAccessor;
    }
    else {
        _ASSERT(list_head == this);
        list_head = m_NextAccessor;
    }
    if (m_NextAccessor) {
        m_NextAccessor->m_PrevAccessor = m_PrevAccessor;
    }
    m_NextAccessor = m_PrevAccessor = NULL;
}

inline bool
CNCBlobAccessor::IsInitialized(void) const
{
    return m_Initialized;
}

inline bool
CNCBlobAccessor::IsBlobExists(void) const
{
    return m_CurData.NotNull();
}

inline bool
CNCBlobAccessor::IsAuthorized(void) const
{
    return !IsBlobExists()  ||  m_CurData->password == m_Password;
}

inline const string&
CNCBlobAccessor::GetBlobKey(void) const
{
    return m_BlobKey;
}

inline Int8
CNCBlobAccessor::GetBlobSize(void) const
{
    _ASSERT(IsBlobExists());
    return m_CurData->size;
}

inline void
CNCBlobAccessor::SetBlobTTL(int ttl)
{
    _ASSERT(m_AccessType == eNCCreate);
    m_NewData->ttl = ttl;
}

inline bool
CNCBlobAccessor::IsBlobExpired(void) const
{
    return m_CurData->dead_time < int(time(NULL));
}

inline ENCAccessType
CNCBlobAccessor::GetAccessType(void) const
{
    return m_AccessType;
}

inline void
CNCBlobAccessor::SetPosition(Int8 pos)
{
    _ASSERT(IsBlobExists()  &&  m_AccessType == eNCReadData);
    _ASSERT(!m_Buffer  &&  pos <= m_CurData->size);

    m_CurChunk = static_cast<unsigned int>(Uint8(pos) / kNCMaxBlobChunkSize);
    m_ChunkPos = static_cast<size_t>(Uint8(pos) % kNCMaxBlobChunkSize);
}

inline void
CNCBlobAccessor::ProlongBlobsLife(void)
{
    _ASSERT(IsBlobExists());

    m_CurData->dead_time  = int(time(NULL)) + m_CurData->ttl;
    m_CurData->need_write = true;
}

inline TNCBlobId
CNCBlobAccessor::GetBlobId(void)
{
    return m_CurData->coords.blob_id;
}

inline TNCDBFileId
CNCBlobAccessor::GetMetaId(void)
{
    return m_CurData->coords.meta_id;
}

inline CRef<SNCBlobVerData>
CNCBlobAccessor::GetNewVersion(void)
{
    return m_VerManager->CreateNewVersion();
}

inline bool
CNCBlobAccessor::SetCurVerIfNewer(SNCBlobVerData* ver_data)
{
    return m_VerManager->SetCurVerIfNewer(ver_data);
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE_BLOB__HPP */
