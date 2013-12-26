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


#include "nc_db_info.hpp"


BEGIN_NCBI_SCOPE


class CNCBlobStorage;
class CNCBlobAccessor;
struct SNCCacheData;
class CCurVerReader;
struct SNCStateStat;


class CNCBlobVerManager : public CObject, public CSrvTask
{
public:
    static CNCBlobVerManager* Get(Uint2         time_bucket,
                                  const string& key,
                                  SNCCacheData* cache_data,
                                  bool          for_new_version);
    void ObtainReference(void);
    void Release(void);

    void RequestCurVersion(CSrvTransConsumer* consumer);
    CSrvRef<SNCBlobVerData> GetCurVersion(void);
    CSrvRef<SNCBlobVerData> CreateNewVersion(void);

    void FinalizeWriting(SNCBlobVerData* ver_data);
    void DeadTimeChanged(SNCBlobVerData* ver_data);
    void DeleteVersion(const SNCBlobVerData* ver_data);
    void DeleteDeadVersion(int cut_time);

    SNCCacheData* GetCacheData(void);

public:
    void RequestMemRelease(void);
    const string& GetKey(void);

private:
    CNCBlobVerManager(const CNCBlobVerManager&);
    CNCBlobVerManager& operator= (const CNCBlobVerManager&);

    CNCBlobVerManager(Uint2 time_bucket,
                      const string& key,
                      SNCCacheData* cache_data);
    virtual ~CNCBlobVerManager(void);

    virtual void DeleteThis(void);
    virtual void ExecuteSlice(TSrvThreadNum thr_num);

    void x_DeleteCurVersion(void);
    void x_ReleaseMgr(void);


    friend class CCurVerReader;


    Uint2 m_TimeBucket;
    bool  m_NeedReleaseMem;
    SNCCacheData*  m_CacheData;
    CCurVerReader* m_CurVerReader;
    string m_Key;
    CSrvRef<SNCBlobVerData>  m_CurVersion;
};


class CNCBlobAccessor : public CSrvTransConsumer
{
public:
    void RequestMetaInfo(CSrvTask* owner);
    bool IsMetaInfoReady(void);
    /// Check if blob exists.
    /// Method can be called only after lock is acquired.
    bool IsBlobExists(void) const;
    /// Check if password provided for accessing the blob was correct
    bool IsAuthorized(void) const;
    bool HasError(void) const;
    /// Get key of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobKey       (void) const;
    Uint2         GetTimeBucket    (void) const;
    /// Get size of the blob.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    Uint8         GetCurBlobSize   (void) const;
    Uint8         GetNewBlobSize   (void) const;
    Uint8         GetSizeRead      (void) const;
    /// Check if blob is already expired but not yet deleted by GC.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    bool          IsCurBlobExpired (void) const;
    bool          IsCurVerExpired  (void) const;
    /// Get type of access this holder was created for
    ENCAccessType GetAccessType    (void) const;

    /// Initially set current position in the blob to start reading from
    void SetPosition(Uint8 pos);
    Uint8 GetPosition(void);
    Uint4 GetReadMemSize(void);
    const void* GetReadMemPtr(void);
    void MoveReadPos(Uint4 move_size);
    unsigned int GetCurBlobTTL(void) const;
    unsigned int GetNewBlobTTL(void) const;
    /// Set blob's timeout after last access before it will be deleted.
    /// Method can be called only after lock is acquired, if blob exists and
    /// lock is acquired for eNCDelete or eNCCreate access.
    void SetBlobTTL(unsigned int ttl);
    unsigned int GetCurVersionTTL(void) const;
    unsigned int GetNewVersionTTL(void) const;
    void SetVersionTTL(int ttl);
    int GetCurBlobVersion(void) const;
    void SetBlobVersion(int ver);
    Uint8 GetCurBlobCreateTime(void) const;
    Uint8 GetNewBlobCreateTime(void) const;
    void SetBlobCreateTime(Uint8 create_time);
    int GetCurBlobDeadTime(void) const;
    int GetCurBlobExpire(void) const;
    int GetNewBlobExpire(void) const;
    int GetCurVerExpire(void) const;
    void SetCurBlobExpire(int expire, int dead_time = 0);
    void SetNewBlobExpire(int expire, int dead_time = 0);
    void SetCurVerExpire(int dead_time);
    void SetNewVerExpire(int dead_time);
    Uint8 GetCurCreateServer(void) const;
    Uint8 GetNewCreateServer(void) const;
    Uint4 GetCurCreateId(void) const;
    void SetCreateServer(Uint8 create_server, Uint4 create_id);
    string GetCurPassword(void) const;
    void SetPassword(CTempString password);
    bool ReplaceBlobInfo(const SNCBlobVerData& new_info);

    size_t GetWriteMemSize(void);
    void* GetWriteMemPtr(void);
    void MoveWritePos(Uint4 move_size);
    void Finalize(void);

    /// Release blob lock.
    /// No other method can be called after call to this one.
    void Release(void);

public:
    // For internal use only

    /// Create holder of blob lock bound to the given NetCache storage.
    /// Lock holder is yet not usable after construction until Prepare()
    /// and InitializeLock() are called.
    CNCBlobAccessor(void);
    virtual ~CNCBlobAccessor(void);

    bool IsPurged(const string& cache) const;
    static bool Purge(const string& cache, Uint8 when);
    static string GetPurgeData(char separator='\n');
    static bool UpdatePurgeData(const string& data, char separator='\n');

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
                 Uint2         time_bucket,
                 ENCAccessType access_type);
    /// Initialize and acquire the lock.
    /// Should be called only after Prepare().
    void Initialize(SNCCacheData* cache_data);
    void Deinitialize(void);

private:
    CNCBlobAccessor(const CNCBlobAccessor&);
    CNCBlobAccessor& operator= (const CNCBlobAccessor&);

    virtual void ExecuteSlice(TSrvThreadNum thr_num);

    void x_CreateNewData(void);
    void x_DelCorruptedVersion(void);


    /// Type of access requested for the blob
    ENCAccessType           m_AccessType;
    string                  m_BlobKey;
    /// Password that was used for access to the blob
    string                  m_Password;
    CNCBlobVerManager*      m_VerManager;
    CSrvRef<SNCBlobVerData> m_CurData;
    CSrvRef<SNCBlobVerData> m_NewData;
    SNCChunkMaps*           m_ChunkMaps;
    bool        m_HasError;
    bool        m_MetaInfoReady;
    bool        m_WriteMemRequested;
    Uint2       m_TimeBucket;
    Uint8       m_CurChunk;
    /// Current position of reading/writing inside blob's chunk
    Uint4       m_ChunkPos;
    Uint4       m_ChunkSize;
    Uint8       m_SizeRead;
    char*       m_Buffer;
    CSrvTask*   m_Owner;
};


class CCurVerReader : public CSrvTransitionTask
{
public:
    CCurVerReader(CNCBlobVerManager* mgr);
    virtual ~CCurVerReader(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);


    CNCBlobVerManager* m_VerMgr;
};


struct SWriteBackData
{
    CMiniMutex lock;
    size_t cur_size;
    size_t releasable_size;
    size_t releasing_size;
    vector<SNCBlobVerData*> to_add_list;
    vector<SNCBlobVerData*> to_del_list;


    SWriteBackData(void);
};


class CWriteBackControl : public CSrvTask
{
public:
    static void Initialize(void);
    static void ReadState(SNCStateStat& state);

private:
    CWriteBackControl(void);
    virtual ~CWriteBackControl(void);

    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


Uint8 GetWBSoftSizeLimit(void);
Uint8 GetWBHardSizeLimit(void);
int GetWBWriteTimeout(void);
int GetWBFailedWriteDelay(void);

void SetWBSoftSizeLimit(Uint8 limit);
void SetWBHardSizeLimit(Uint8 limit);
void SetWBWriteTimeout(int timeout1, int timeout2);
void SetWBFailedWriteDelay(int delay);
void SetWBInitialSyncComplete(void);


class CWBMemDeleter : public CSrvRCUUser
{
public:
    CWBMemDeleter(char* mem, Uint4 mem_size);
    virtual ~CWBMemDeleter(void);

private:
    virtual void ExecuteRCU(void);


    char* m_Mem;
    Uint4 m_MemSize;
};



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline SNCCacheData*
CNCBlobVerManager::GetCacheData(void)
{
    return m_CacheData;
}

inline const string&
CNCBlobVerManager::GetKey(void)
{
    return m_Key;
}

inline void
CNCBlobVerManager::RequestCurVersion(CSrvTransConsumer* consumer)
{
    m_CurVerReader->RequestTransition(consumer);
}


inline bool
CNCBlobAccessor::IsMetaInfoReady(void)
{
    return m_MetaInfoReady;
}

inline bool
CNCBlobAccessor::IsBlobExists(void) const
{
    return m_CurData.NotNull();
}

inline bool
CNCBlobAccessor::IsAuthorized(void) const
{
    return !IsBlobExists()  ||  IsCurBlobExpired()
           ||  m_CurData->password == m_Password;
}

inline bool
CNCBlobAccessor::HasError(void) const
{
    return m_HasError;
}

inline const string&
CNCBlobAccessor::GetBlobKey(void) const
{
    return m_BlobKey;
}

inline Uint2
CNCBlobAccessor::GetTimeBucket(void) const
{
    return m_TimeBucket;
}

inline Uint8
CNCBlobAccessor::GetCurBlobSize(void) const
{
    return m_CurData->size;
}

inline Uint8
CNCBlobAccessor::GetNewBlobSize(void) const
{
    return m_NewData? m_NewData->size: 0;
}

inline Uint8
CNCBlobAccessor::GetSizeRead(void) const
{
    return m_SizeRead;
}

inline int
CNCBlobAccessor::GetCurBlobVersion(void) const
{
    return m_CurData->blob_ver;
}

inline unsigned int
CNCBlobAccessor::GetCurBlobTTL(void) const
{
    return m_CurData->ttl;
}

inline unsigned int
CNCBlobAccessor::GetNewBlobTTL(void) const
{
    return m_NewData->ttl;
}

inline bool
CNCBlobAccessor::IsCurBlobExpired(void) const
{
    return m_CurData->expire <= CSrvTime::CurSecs();
}

inline bool
CNCBlobAccessor::IsCurVerExpired(void) const
{
    return m_CurData->ver_expire <= CSrvTime::CurSecs();
}

inline void
CNCBlobAccessor::SetBlobCreateTime(Uint8 create_time)
{
    x_CreateNewData();
    m_NewData->create_time = create_time;
}

inline Uint8
CNCBlobAccessor::GetCurBlobCreateTime(void) const
{
    return m_CurData.NotNull()? m_CurData->create_time: 0;
}

inline Uint8
CNCBlobAccessor::GetNewBlobCreateTime(void) const
{
    return m_NewData->create_time;
}

inline int
CNCBlobAccessor::GetCurBlobDeadTime(void) const
{
    return m_CurData->dead_time;
}

inline int
CNCBlobAccessor::GetCurBlobExpire(void) const
{
    return m_CurData->expire;
}

inline int
CNCBlobAccessor::GetNewBlobExpire(void) const
{
    return m_NewData->expire;
}

inline int
CNCBlobAccessor::GetCurVerExpire(void) const
{
    return m_CurData->ver_expire;
}

inline void
CNCBlobAccessor::SetNewBlobExpire(int expire, int dead_time /* = 0 */)
{
    x_CreateNewData();
    m_NewData->expire = expire;
    dead_time = max(dead_time, expire);
    if (m_CurData)
        dead_time = max(m_CurData->dead_time, dead_time);
    m_NewData->dead_time = dead_time;
}

inline unsigned int
CNCBlobAccessor::GetCurVersionTTL(void) const
{
    return m_CurData->ver_ttl;
}

inline unsigned int
CNCBlobAccessor::GetNewVersionTTL(void) const
{
    return m_NewData->ver_ttl;
}

inline void
CNCBlobAccessor::SetVersionTTL(int ttl)
{
    x_CreateNewData();
    m_NewData->ver_ttl = ttl;
}

inline void
CNCBlobAccessor::SetBlobTTL(unsigned int ttl)
{
    x_CreateNewData();
    m_NewData->ttl = ttl;
}

inline void
CNCBlobAccessor::SetBlobVersion(int ver)
{
    x_CreateNewData();
    m_NewData->blob_ver = ver;
}

inline void
CNCBlobAccessor::SetCurBlobExpire(int expire, int dead_time /* = 0 */)
{
    m_CurData->expire = expire;
    m_CurData->dead_time = max(expire, max(m_CurData->dead_time, dead_time));
    m_VerManager->DeadTimeChanged(m_CurData);
}

inline void
CNCBlobAccessor::SetCurVerExpire(int expire)
{
    m_CurData->ver_expire = expire;
    m_VerManager->DeadTimeChanged(m_CurData);
}

inline void
CNCBlobAccessor::SetNewVerExpire(int expire)
{
    x_CreateNewData();
    m_NewData->ver_expire = expire;
}

inline void
CNCBlobAccessor::SetCreateServer(Uint8 create_server,
                                 Uint4 create_id)
{
    x_CreateNewData();
    m_NewData->create_server = create_server;
    m_NewData->create_id = create_id;
}

inline Uint8
CNCBlobAccessor::GetCurCreateServer(void) const
{
    return m_CurData.NotNull()? m_CurData->create_server: 0;
}

inline Uint8
CNCBlobAccessor::GetNewCreateServer(void) const
{
    return m_NewData->create_server;
}

inline Uint4
CNCBlobAccessor::GetCurCreateId(void) const
{
    return m_CurData.NotNull()? m_CurData->create_id: 0;
}

inline void
CNCBlobAccessor::SetPassword(CTempString password)
{
    x_CreateNewData();
    m_NewData->password = password;
}

inline ENCAccessType
CNCBlobAccessor::GetAccessType(void) const
{
    return m_AccessType;
}

inline void
CNCBlobAccessor::SetPosition(Uint8 pos)
{
    m_CurChunk = pos / m_CurData->chunk_size;
    m_ChunkPos = size_t(pos % m_CurData->chunk_size);
}

inline Uint8
CNCBlobAccessor::GetPosition(void)
{
    return m_CurChunk * m_CurData->chunk_size + m_ChunkPos;
}

inline const void*
CNCBlobAccessor::GetReadMemPtr(void)
{
    return m_Buffer + m_ChunkPos;
}

inline void*
CNCBlobAccessor::GetWriteMemPtr(void)
{
    return m_Buffer + m_ChunkPos;
}

inline void
CNCBlobAccessor::MoveWritePos(Uint4 move_size)
{
    m_ChunkPos += move_size;
    m_NewData->size += move_size;
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE_BLOB__HPP */
