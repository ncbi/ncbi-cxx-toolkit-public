#ifndef NETCACHE_NETCACHE_DB__HPP
#define NETCACHE_NETCACHE_DB__HPP
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
 *
 * File Description: 
 *
 */

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/obj_pool.hpp>
#include <connect/server_monitor.hpp>

#include "nc_utils.hpp"
#include "nc_db_info.hpp"
#include "nc_db_stat.hpp"
#include "nc_db_files.hpp"
#include "nc_storage_blob.hpp"

#include <set>
#include <map>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(NetCache_Storage, 2002,  14);


/// Exception which is thrown from all CNCBlobStorage-related classes
class CNCBlobStorageException : public CException
{
public:
    enum EErrCode {
        eUnknown,         ///< Unknown error
        eWrongFileName,   ///< Wrong file name given in storage settings
        eReadOnlyAccess,  ///< Operation is prohibited because of read-only
                          ///< access to the storage
        eMalformedDB      ///< Data in database is in inconsistent state due
                          ///< to NetCache crash
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CNCBlobStorageException, CException);
};


class CNCBlobStorage;


template <class TFile>
class CNCDB_FileLock
{
public:
    CNCDB_FileLock(CNCBlobStorage* storage, const SNCDBPartInfo& part_info);
    CNCDB_FileLock(CNCBlobStorage* storage, TNCDBPartId part_id);
    ~CNCDB_FileLock(void);

    operator TFile* (void) const;
    TFile& operator* (void) const;
    TFile* operator-> (void) const;

private:
    CNCBlobStorage* m_Storage;
    TFile*          m_File;
    TNCDBPartId     m_DBPartId;
};


template <class TFile>
class CNCFileObjFactory : public CObjFactory_New<TFile>
{
public:
    CNCFileObjFactory(void);
    CNCFileObjFactory(const string& file_name, CNCDB_Stat* stat);

    TFile* CreateObject(void);
    void Delete(TFile* file);

private:
    string      m_FileName;
    CNCDB_Stat* m_Stat;
};


/// Blob storage for NetCache
class CNCBlobStorage
{
public:
    /// The mode of database reinitialization if needed
    enum EReinitMode {
        eNoReinit,    ///< No reinitialization is necessary
        eReinitBad,   ///< Reinitialize if some errors appear while opening
        eForceReinit  ///< Reinitialize right away no matter if there's
                      ///< something alive in database
    };

    /// Create blob storage or connect to existing one
    ///
    /// @param reg
    ///   Registry to read object configuration
    /// @param section
    ///   Section name inside registry to read configuration from
    /// @param reinit_mode
    ///   Flag for necessary reinitialization of the database
    CNCBlobStorage(const IRegistry& reg,
                   CTempString      section,
                   EReinitMode      reinit_mode);
    ~CNCBlobStorage(void);

    /// Set object to monitor storage activity
    void SetMonitor(CServer_Monitor* monitor);

    /// Check if storage is in read-only mode
    bool IsReadOnly        (void) const;
    /// Check if storage changes access time of blobs on every read access
    bool IsChangeTimeOnRead(void) const;

    /// Get default value for blob's timeout before being deleted
    int    GetDefBlobTTL  (void) const;
    /// Get maximum value for blob's time-to-live
    int    GetMaxBlobTTL  (void) const;
    /// Get maximum blob size that is allowed in the storage
    size_t GetMaxBlobSize (void) const;
    /// Get size of disk space to free up on each GC iteration
    size_t GetDBShrinkStep(void) const;

    /// Print usage statistics for the storage
    void PrintStat(CPrintTextProxy& proxy);

    /// Acquire access to the blob identified by key-subkey-version
    TNCBlobLockHolderRef GetBlobAccess(ENCBlobAccess access,
                                       const string& key,
                                       const string& subkey = kEmptyStr,
                                       int           version = 0);
    /// Acquire access to the blob identified by blob id
    TNCBlobLockHolderRef GetBlobAccess(ENCBlobAccess access,
                                       TNCBlobId     blob_id);

    /// Get ids of all blobs with given key
    void FillBlobIds(const string& key, TNCIdsList* id_list);

    /// Check if blob with given key and (optionally) subkey exists
    /// in database
    bool IsBlobExists(CTempString key, CTempString subkey = kEmptyStr);

public:
    // Methods for internal use only

    TNCBlobLockHolderRef GetBlobAccess(ENCBlobAccess access,
                                       TNCDBPartId   part_id,
                                       TNCBlobId     blob_id,
                                       bool          change_access_time);

    /// Get object gathering storage statistics
    CNCDB_Stat* GetStat(void);
    TNCBlobsPool& GetBlobsPool(void);
    TNCBlobBufferPool& GetBlobBufferPool(void);

    /// Get next blob id (along with database part id) that can be used
    /// for creating new blob
    void GetNextBlobCoords(SNCBlobCoords* coords);
    /// Lock blob id, i.e. prevent access of others to blob with the same id
    TRWLockHolderRef LockBlobId(TNCBlobId blob_id, ENCBlobAccess access);
    /// Unlock blob id and free lock holder
    void UnlockBlobId(TNCBlobId blob_id, TRWLockHolderRef& rw_holder);
    ///
    bool ReadBlobId(SNCBlobIdentity* identity);
    ///
    bool CreateBlob(SNCBlobIdentity* identity);
    ///
    bool ReadBlobKey(SNCBlobIdentity* identity);
    ///
    bool ReadBlobInfo(SNCBlobInfo* info);
    ///
    void WriteBlobInfo(const SNCBlobInfo& info);
    ///
    void DeleteBlob(const SNCBlobCoords& coords);

    ///
    void GetChunkIds(const SNCBlobCoords& coords, TNCIdsList* id_list);
    ///
    TNCChunkId CreateNewChunk(const SNCBlobCoords& coords,
                              const TNCBlobBuffer& buffer);
    ///
    void WriteChunkValue(TNCDBPartId          part_id,
                         TNCChunkId           chunk_id,
                         const TNCBlobBuffer& buffer);
    ///
    void ReadChunkValue(TNCDBPartId    part_id,
                        TNCChunkId     chunk_id,
                        TNCBlobBuffer* buffer);
    ///
    void DeleteLastChunks(const SNCBlobCoords& coords,
                          TNCChunkId           min_chunk_id);

    template <class TFile>
    TFile* GetFile(TNCDBPartId part_id);
    void ReturnFile(TNCDBPartId part_id, CNCDB_MetaFile* file);
    void ReturnFile(TNCDBPartId part_id, CNCDB_DataFile* file);
    void ReturnLockHolder(CNCBlobLockHolder* holder);

    /// Garbage collector code
    void RunBackground(void);

private:
    CNCBlobStorage(const CNCBlobStorage&);
    CNCBlobStorage& operator= (const CNCBlobStorage&);


    /// Storage operation flags that can be set via storage configuration
    enum EOperationFlags {
        fReadOnly         = 0x1,
        fChangeTimeOnRead = 0x2
    };
    typedef int  TOperationFlags;

    typedef set<SNCBlobIdentity, SNCBlobCompareKeys/*,
                CNCSmallDataAllocator<SNCBlobIdentity>*/ >     TKeyIdMap;
    typedef set<SNCBlobIdentity, SNCBlobCompareIds/*,
                CNCSmallDataAllocator<SNCBlobIdentity>*/ >     TIdKeyMap;
    typedef map<TNCBlobId, CYieldingRWLock* /*, less<TNCBlobId>,
                CNCSmallDataAllocator<pair<const TNCBlobId,
                                           CYieldingRWLock*> >*/ >
                                                             TId2LocksMap;
    typedef deque<CYieldingRWLock* /*,
                  CNCSmallDataAllocator<CYieldingRWLock*>*/ >  TLocksList;

    typedef CNCDB_FileLock<CNCDB_MetaFile>                   TMetaFileLock;
    typedef CNCFileObjFactory<CNCDB_MetaFile>                TMetaFileFactory;
    typedef AutoPtr<CNCDB_MetaFile, TMetaFileFactory>        TMetaFilePtr;
    typedef CObjPool<CNCDB_MetaFile, TMetaFileFactory>       TMetaFilePool;
    typedef AutoPtr<TMetaFilePool>                           TMetaFilePoolPtr;
    typedef map<TNCDBPartId, TMetaFilePoolPtr/*, less<TNCDBPartId>,
                CNCSmallDataAllocator<pair<const TNCDBPartId,
                                           TMetaFilePoolPtr> >*/ >
                                                             TMetaFilesMap;

    typedef CNCDB_FileLock<CNCDB_DataFile>                   TDataFileLock;
    typedef CNCFileObjFactory<CNCDB_DataFile>                TDataFileFactory;
    typedef AutoPtr<CNCDB_DataFile, TDataFileFactory>        TDataFilePtr;
    typedef CObjPool<CNCDB_DataFile, TDataFileFactory>       TDataFilePool;
    typedef AutoPtr<TDataFilePool>                           TDataFilePoolPtr;
    typedef map<TNCDBPartId, TDataFilePoolPtr/*, less<TNCDBPartId>,
                CNCSmallDataAllocator<pair<const TNCDBPartId,
                                           TDataFilePoolPtr> >*/ >
                                                             TDataFilesMap;


    /// Check if storage activity is monitored now
    bool x_IsMonitored(void) const;
    /// Post message to activity monitor and _TRACE it if necessary
    void x_MonitorPost(CTempString msg, bool do_trace = true);
    ///
    void x_MonitorError(exception& ex, CTempString msg);
    /// Check if storage is writable, throw exception if it is in read-only
    /// mode.
    void x_CheckReadOnly(ENCBlobAccess access) const;
    ///
    void x_CheckStopped(void);


    /// Read all storage parameters from registry
    void x_ReadStorageParams(const IRegistry& reg, CTempString section);
    /// Parse value of 'timestamp' parameter from registry
    void x_ParseTimestampParam(const string& timestamp);
    string x_GetIndexFileName(void);
    string x_GetMetaFileName(TNCDBPartId part_id);
    string x_GetDataFileName(TNCDBPartId part_id);
    bool x_LockInstanceGuard(void);
    void x_UnlockInstanceGuard(void);
    void x_OpenIndexDB(EReinitMode* reinit_mode);
    void x_CheckDBInitialized(bool        guard_existed,
                              EReinitMode reinit_mode,
                              bool        reinit_dirty);
    void x_ReadLastBlobCoords(void);
    void x_FreeBlobIdLocks(void);
    void x_InitFilePools(const SNCDBPartInfo& part_info);
    void x_CreateNewDBStructure(const SNCDBPartInfo& part_info);
    void x_SwitchCurrentDBPart(SNCDBPartInfo* part_info);
    void x_CreateNewDBPart(void);
    TNCDBPartId x_GetNotCachedPartId(void);
    void x_SetNotCachedPartId(TNCDBPartId part_id);
    void x_CopyPartsList(TNCDBPartsList* parts_list);
    bool x_ReadBlobIdFromCache(SNCBlobIdentity* identity);
    void x_AddBlobKeyIdToCache(const SNCBlobIdentity& identity);
    bool x_CreateBlobInCache(SNCBlobIdentity* identity);
    bool x_ReadBlobKeyFromCache(SNCBlobIdentity* identity);
    void x_DeleteBlobFromCache(const SNCBlobCoords& coords);
    void x_FillBlobIdsFromCache(TNCIdsList* id_list, const string& key);
    bool x_IsBlobExistsInCache(CTempString key, CTempString subkey);
    void x_FillCacheFromDBPart(TNCDBPartId part_id);
    bool x_GC_DeleteExpired(TNCDBPartId part_id, TNCBlobId blob_id);
    bool x_GC_IsMetafileEmpty(TNCDBPartId          part_id,
                              const TMetaFileLock& metafile,
                              int                  dead_time);
    bool x_GC_CleanDBPart(TNCDBPartsList::iterator part_it,
                          int                      dead_before);
    void x_GC_PrepFilePoolsToDelete(TNCDBPartId       part_id,
                                    TMetaFilePoolPtr* meta_pool,
                                    TDataFilePoolPtr* data_pool);
    void x_GC_DeleteDBPart(TNCDBPartsList::iterator part_it);
    void x_GC_RotateDBParts(void);

    template <class TFilesMap>
    TFilesMap& x_FilesMap(void);


    /// Directory for all database files of the storage
    string             m_Path;
    /// Name of the storage from configuration
    string             m_Name;
    /// Default value of blob's time-to-live
    int                m_DefBlobTTL;
    /// Maximum value of blob's time-to-live
    int                m_MaxBlobTTL;
    /// Maximum allowed blob size
    size_t             m_MaxBlobSize;
    int                m_DBRotatePeriod;
    /// Delay between GC activations
    int                m_GCRunDelay;
    /// Number of blobs treated by GC in one batch
    int                m_GCBatchSize;
    /// Time in milliseconds GC will sleep between batches
    int                m_GCBatchSleep;
    /// Maximum size of disk space that GC will shrink on each iteration
    //size_t             m_DBShrinkStep;

    /// Name of guard file showing that storage is in work now
    string             m_GuardName;
    /// Lock for guard file representing this instance running.
    /// It will hold exclusive lock all the time while NetCache is
    /// working, so that other instance of NetCache on the same database will
    /// be unable to start.
    AutoPtr<CFileLock> m_GuardLock;
    /// Object monitoring activity on the storage
    CServer_Monitor*   m_Monitor;
    /// Object gathering statistics for the storage
    CNCDB_Stat         m_Stat;
    /// Operation flags read from configuration
    TOperationFlags    m_Flags;
    /// Flag if storage is stopped and in process of destroying
    bool               m_Stopped;
    /// Semaphore allowing immediate stopping of GC thread
    CSemaphore         m_StopTrigger;
    /// Thread running GC
    CRef<CThread>      m_BGThread;

    CFastRWLock              m_KeysCacheLock;
    TKeyIdMap                m_KeysCache;
    CFastRWLock              m_IdsCacheLock;
    TIdKeyMap                m_IdsCache;
    AutoPtr<CNCDB_IndexFile> m_IndexDB;
    CFastRWLock              m_DBPartsLock;
    TNCDBPartsList           m_DBParts;
    bool                     m_AllDBPartsCached;
    CFastRWLock              m_NotCachedPartIdLock;
    TNCDBPartId              m_NotCachedPartId;
    // Assuming that reads and writes for int are atomic.
    volatile int             m_LastDeadTime;
    CFastRWLock              m_FileMapsMutex;
    TMetaFilesMap            m_MetaFiles;
    TDataFilesMap            m_DataFiles;

    TNCBlobBufferPool        m_BuffersPool;
    TNCBlobLockObjPool       m_LockHoldersPool;
    TNCBlobsPool             m_BlobsPool;

    /// Mutex for changing blob id counter
    CFastMutex         m_LastCoordsMutex;
    SNCBlobCoords      m_LastBlob;

    /// Mutex for locking/unlocking blob ids
    CFastMutex         m_IdLockMutex;
    /// Map blob ids to CYieldingRWLock objects
    TId2LocksMap       m_IdLocks;
    /// CYieldingRWLock objects that are not used by anybody at the moment
    TLocksList         m_FreeLocks;
};



template <class TFile>
inline
CNCDB_FileLock<TFile>::CNCDB_FileLock(CNCBlobStorage*      storage,
                                      const SNCDBPartInfo& part_info)
    : m_Storage(storage),
      m_File(storage->GetFile<TFile>(part_info.part_id)),
      m_DBPartId(part_info.part_id)
{}

template <class TFile>
inline
CNCDB_FileLock<TFile>::CNCDB_FileLock(CNCBlobStorage* storage,
                                      TNCDBPartId     part_id)
    : m_Storage(storage),
      m_File(storage->GetFile<TFile>(part_id)),
      m_DBPartId(part_id)
{}

template <class TFile>
inline
CNCDB_FileLock<TFile>::~CNCDB_FileLock(void)
{
    m_Storage->ReturnFile(m_DBPartId, m_File);
}

template <class TFile>
inline
CNCDB_FileLock<TFile>::operator TFile* (void) const
{
    return m_File;
}

template <class TFile>
inline TFile&
CNCDB_FileLock<TFile>::operator* (void) const
{
    return *m_File;
}

template <class TFile>
inline TFile*
CNCDB_FileLock<TFile>::operator-> (void) const
{
    return m_File;
}


template <class TFile>
inline
CNCFileObjFactory<TFile>::CNCFileObjFactory(void)
    : m_Stat(NULL)
{}

template <class TFile>
inline
CNCFileObjFactory<TFile>::CNCFileObjFactory(const string& file_name,
                                            CNCDB_Stat* stat)
    : m_FileName(file_name),
      m_Stat(stat)
{}

template <class TFile>
inline TFile*
CNCFileObjFactory<TFile>::CreateObject(void)
{
    _ASSERT(!m_FileName.empty()  &&  m_Stat);
    return new TFile(m_FileName, m_Stat);
}

template <class TFile>
inline void
CNCFileObjFactory<TFile>::Delete(TFile* file)
{
    DeleteObject(file);
}



inline void
CNCBlobStorage::SetMonitor(CServer_Monitor* monitor)
{
    m_Monitor = monitor;
}

inline bool
CNCBlobStorage::x_IsMonitored(void) const
{
    return IsVisibleDiagPostLevel(eDiag_Trace)
           ||  m_Monitor  &&  m_Monitor->IsMonitorActive();
}

inline CNCDB_Stat*
CNCBlobStorage::GetStat(void)
{
    return &m_Stat;
}

inline TNCBlobsPool&
CNCBlobStorage::GetBlobsPool(void)
{
    return m_BlobsPool;
}

inline TNCBlobBufferPool&
CNCBlobStorage::GetBlobBufferPool(void)
{
    return m_BuffersPool;
}

inline void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    proxy << "Usage statistics for storage "
                                 << m_Name << " at " << m_Path << ":" << endl
          << endl;
    m_Stat.Print(proxy);
}

inline void
CNCBlobStorage::GetNextBlobCoords(SNCBlobCoords* coords)
{
    CFastMutexGuard guard(m_LastCoordsMutex);

    if (++m_LastBlob.blob_id <= 0)
        m_LastBlob.blob_id = 1;
    *coords = m_LastBlob;
}

inline bool
CNCBlobStorage::IsReadOnly(void) const
{
    return (m_Flags & fReadOnly) != 0;
}

inline bool
CNCBlobStorage::IsChangeTimeOnRead(void) const
{
    return (m_Flags & fChangeTimeOnRead) != 0;
}

inline int
CNCBlobStorage::GetDefBlobTTL(void) const
{
    return m_DefBlobTTL;
}

inline int
CNCBlobStorage::GetMaxBlobTTL(void) const
{
    return m_MaxBlobTTL;
}

inline size_t
CNCBlobStorage::GetMaxBlobSize(void) const
{
    return m_MaxBlobSize;
}
/*
inline size_t
CNCBlobStorage::GetDBShrinkStep(void) const
{
    return m_DBShrinkStep;
}
*/
inline void
CNCBlobStorage::x_CheckReadOnly(ENCBlobAccess access) const
{
    if (access != eRead  &&  IsReadOnly()) {
        NCBI_THROW(CNCBlobStorageException, eReadOnlyAccess,
                   "Database is in read only mode");
    }
}

template <>
inline CNCBlobStorage::TMetaFilesMap&
CNCBlobStorage::x_FilesMap<CNCBlobStorage::TMetaFilesMap>(void)
{
    return m_MetaFiles;
}

template <>
inline CNCBlobStorage::TDataFilesMap&
CNCBlobStorage::x_FilesMap<CNCBlobStorage::TDataFilesMap>(void)
{
    return m_DataFiles;
}

template <class TFile>
inline TFile*
CNCBlobStorage::GetFile(TNCDBPartId part_id)
{
    // This is the (unfortunately) necessary repeat from class declaration
    typedef CNCFileObjFactory<TFile>                          TFileFactory;
    typedef CObjPool<TFile, TFileFactory>                     TFilePool;
    typedef AutoPtr<TFilePool>                                TFilePoolPtr;
    typedef map<TNCDBPartId, TFilePoolPtr/*, less<TNCDBPartId>,
                CNCSmallDataAllocator<pair<const TNCDBPartId,
                                           TFilePoolPtr> >*/ >  TFilesMap;

    TFilesMap& files_map = x_FilesMap<TFilesMap>();
    TFilePool* pool = NULL;
    {{
        CFastReadGuard guard(m_FileMapsMutex);
        pool = files_map[part_id].get();
    }}
    _ASSERT(pool);
    return pool->Get();
}

inline void
CNCBlobStorage::ReturnFile(TNCDBPartId part_id, CNCDB_MetaFile* file)
{
    TMetaFilePool* pool;
    {{
        CFastReadGuard guard(m_FileMapsMutex);
        TMetaFilesMap::const_iterator it = m_MetaFiles.find(part_id);
        _ASSERT(it != m_MetaFiles.end());
        pool = it->second.get();
    }}    

    _ASSERT(pool);
    pool->Return(file);
}

inline void
CNCBlobStorage::ReturnFile(TNCDBPartId part_id, CNCDB_DataFile* file)
{
    TDataFilePool* pool;
    {{
        CFastReadGuard guard(m_FileMapsMutex);
        TDataFilesMap::const_iterator it = m_DataFiles.find(part_id);
        _ASSERT(it != m_DataFiles.end());
        pool = it->second.get();
    }}    

    _ASSERT(pool);
    pool->Return(file);
}

inline void
CNCBlobStorage::ReturnLockHolder(CNCBlobLockHolder* holder)
{
    m_LockHoldersPool.Return(holder);
}

inline bool
CNCBlobStorage::ReadBlobInfo(SNCBlobInfo* info)
{
    _ASSERT(info->part_id != 0  &&  info->blob_id != 0);

    TMetaFileLock metafile(this, info->part_id);
    return metafile->ReadBlobInfo(info);
}

inline void
CNCBlobStorage::WriteBlobInfo(const SNCBlobInfo& info)
{
    TMetaFileLock metafile(this, info.part_id);
    metafile->WriteBlobInfo(info);
}

inline void
CNCBlobStorage::GetChunkIds(const SNCBlobCoords& coords, TNCIdsList* id_list)
{
    TMetaFileLock metafile(this, coords.part_id);
    metafile->GetChunkIds(coords.blob_id, id_list);
}

inline void
CNCBlobStorage::WriteChunkValue(TNCDBPartId          part_id,
                                TNCChunkId           chunk_id,
                                const TNCBlobBuffer& buffer)
{
    TDataFileLock datafile(this, part_id);
    datafile->WriteChunkValue(chunk_id, buffer);
}

inline TNCChunkId
CNCBlobStorage::CreateNewChunk(const SNCBlobCoords& coords,
                               const TNCBlobBuffer& buffer)
{
    TNCChunkId chunk_id;
    {{
        TDataFileLock datafile(this, coords.part_id);
        chunk_id = datafile->CreateChunkValue(buffer);
    }}
    {{
        TMetaFileLock metafile(this, coords.part_id);
        metafile->CreateChunkRow(coords.blob_id, chunk_id);
    }}
    return chunk_id;
}

inline void
CNCBlobStorage::ReadChunkValue(TNCDBPartId    part_id,
                               TNCChunkId     chunk_id,
                               TNCBlobBuffer* buffer)
{
    TDataFileLock datafile(this, part_id);
    datafile->ReadChunkValue(chunk_id, buffer);
}

inline void
CNCBlobStorage::DeleteLastChunks(const SNCBlobCoords& coords,
                                 TNCChunkId           min_chunk_id)
{
    TMetaFileLock metafile(this, coords.part_id);
    metafile->DeleteLastChunkRows(coords.blob_id, min_chunk_id);
}

inline TNCBlobLockHolderRef
CNCBlobStorage::GetBlobAccess(ENCBlobAccess access,
                              const string& key,
                              const string& subkey /* = kEmptyStr */,
                              int           version /* = 0 */)
{
    x_CheckReadOnly(access);

    TNCBlobLockHolderRef holder(m_LockHoldersPool.Get());
    holder->AcquireLock(key, subkey, version, access);
    return holder;
}

inline TNCBlobLockHolderRef
CNCBlobStorage::GetBlobAccess(ENCBlobAccess access,
                              TNCDBPartId   part_id,
                              TNCBlobId     blob_id,
                              bool          change_access_time)
{
    x_CheckReadOnly(access);

    TNCBlobLockHolderRef holder(m_LockHoldersPool.Get());
    holder->AcquireLock(part_id, blob_id, access, change_access_time);
    return holder;
}

inline TNCBlobLockHolderRef
CNCBlobStorage::GetBlobAccess(ENCBlobAccess access, TNCBlobId blob_id)
{
    return GetBlobAccess(access, 0, blob_id, IsChangeTimeOnRead());
}

END_NCBI_SCOPE

#endif /* NETCACHE_NETCACHE_DB__HPP */
