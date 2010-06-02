#ifndef NETCACHE__NC_STORAGE__HPP
#define NETCACHE__NC_STORAGE__HPP
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

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/obj_pool.hpp>
#include <connect/server_monitor.hpp>

#include "nc_utils.hpp"
#include "nc_db_info.hpp"
#include "nc_db_files.hpp"
#include "nc_storage_blob.hpp"
#include "nc_stat.hpp"

#include "concurrent_map.hpp"

#ifdef NCBI_OS_UNIX
#  include <sys/time.h>
#endif


BEGIN_NCBI_SCOPE


/// Exception which is thrown from all CNCBlobStorage-related classes
class CNCBlobStorageException : public CException
{
public:
    enum EErrCode {
        eUnknown,         ///< Unknown error
        eWrongConfig,     ///< Wrong file name given in storage settings
        eCorruptedDB,     ///< Detected corruption of the database
        eWrongBlock       ///< Block() method is called when it cannot be
                          ///< executed
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CNCBlobStorageException, CException);
};


class CNCBlobStorage;


/// Scoped pointer to database file object. File object is acquired from
/// storage's pool in constructors and is returned to the pool in destructor.
template <class TFile>
class CNCDBFileLock
{
public:
    /// Create scoped pointer to the file from given volume of database part
    /// for given storage.
    CNCDBFileLock(CNCBlobStorage* storage,
                  TNCDBFileId     file_id);
    CNCDBFileLock(CNCDBFile*      file);
    ~CNCDBFileLock(void);

    /// Smart pointer conversions
    operator TFile*   (void) const;
    TFile& operator*  (void) const;
    TFile* operator-> (void) const;

private:
    /// Database file object obtained from storage
    TFile*          m_File;
};


/// Blob storage for NetCache
class CNCBlobStorage : public INCBlockedOpListener
{
public:
    /// Create blob storage or connect to existing one
    ///
    /// @param reg
    ///   Registry to read object configuration
    /// @param section
    ///   Section name inside registry to read configuration from
    /// @param reinit_mode
    ///   Flag for necessary reinitialization of the database
    CNCBlobStorage(bool do_reinit);
    virtual ~CNCBlobStorage(void);

    /// Block all operations on the storage.
    /// Work with all blobs that are blocked already will be finished as usual,
    /// all future requests for locking of blobs will be suspended until
    /// Unblock() is called. Method will return immediately and will not wait
    /// for already locked blobs to be unlocked - IsBlockActive() call should
    /// be used to understand if all operations on the storage are freezed.
    void Block(void);
    /// Unblock the storage and allow it to continue work as usual.
    void Unblock(void);
    /// Check if storage blocking is completed and all operations with storage
    /// are stopped.
    bool IsBlockActive(void);
    /// Reinitialize storage database.
    /// Method can be called only when storage is completely blocked and all
    /// operations with it are finished.
    void ReinitializeCache(const string& cache_key);
    /// Re-read configuration of the storage
    ///
    /// @param reg
    ///   Registry to read object configuration. All parameters that cannot be
    ///   changed on the fly are set to old values in the registry.
    /// @param section
    ///   Section name inside registry to read configuration from
    void Reconfigure(void);

    /// Print usage statistics for the storage
    void PrintStat(CPrintTextProxy& proxy);

    ///
    void PackBlobKey(string*      packed_key,
                     CTempString  cache_name,
                     CTempString  blob_key,
                     CTempString  blob_subkey,
                     unsigned int blob_version);
    ///
    string UnpackKeyForLogs(const string& packed_key);
    /// Acquire access to the blob identified by key, subkey and version
    CNCBlobAccessor* GetBlobAccess(ENCAccessType access,
                                   const string& key,
                                   const string& password);
    /// Check if blob with given key and (optionally) subkey exists
    /// in database. More than one blob with given key/subkey can exist.
    bool IsBlobFamilyExists(const string& key);

public:
    // For internal use only

    /// Get next blob coordinates that can be used for creating new blob.
    void GetNewBlobCoords(SNCBlobCoords* coords);
    /// Get meta information about blob with given coordinates.
    ///
    /// @return
    ///   TRUE if meta information exists in the database and was successfully
    ///   read. FALSE if database is corrupted.
    bool ReadBlobInfo(SNCBlobVerData* ver_data);
    /// Get list of chunks ids
    ///
    /// @param coords
    ///   Coordinates of the blob to get chunks ids for
    /// @param id_list
    ///   Pointer to the list receiving chunks ids
    void ReadChunkIds(SNCBlobVerData* ver_data);
    /// Write meta information about blob into the database.
    void WriteBlobInfo(const string& blob_key, SNCBlobVerData* ver_data);
    ///
    bool UpdateBlobInfo(const string& blob_key, SNCBlobVerData* ver_data);
    /// Delete blob from database.
    void DeleteBlobInfo(const SNCBlobVerData* ver_data);

    ///
    bool ReadChunkData(TNCDBFileId    file_id,
                       TNCChunkId     chunk_id,
                       CNCBlobBuffer* buffer);
    ///
    void WriteSingleChunk(SNCBlobVerData* blob_info, const CNCBlobBuffer* data);
    ///
    void WriteNextChunk(SNCBlobVerData* blob_info, const CNCBlobBuffer* data);

    ///
    void DeleteBlobKey(const string& key);
    ///
    void RestoreBlobKey(const string& key);
    ///
    void NotifyAfterCaching(INCBlockedOpListener* listener);

    /// Get database file object from pool
    ///
    /// @param part_id
    ///   Database part id of necessary file
    /// @param vol_id
    ///   Id of volume for necessary file
    /// @param file_ptr
    ///   Pointer to variable where pointer to file object will be written to.
    ///   This cannot be returned as method result because GCC 3.0.4 doesn't
    ///   compile such method calls (without template parameter in arguments).
    template <class TFile>
    void GetFile   (TNCDBFileId   part_id,
                    TFile**       file_ptr);
    /// Return blob lock holder to the pool
    void ReturnAccessor(CNCBlobAccessor* holder);

private:
    CNCBlobStorage(const CNCBlobStorage&);
    CNCBlobStorage& operator= (const CNCBlobStorage&);


    ///
    class CKeysCleaner
    {
    public:
        static void Delete(const string& key);
    };

    friend class CKeysCleaner;


    typedef CNCDBFileLock<CNCDBMetaFile>                        TMetaFileLock;
    typedef CNCDBFileLock<CNCDBDataFile>                        TDataFileLock;

    typedef map<TNCDBFileId, Uint8>                             TUsefulCntMap;
    typedef vector<TNCDBFileId>                                 TCurFilesList[2];

    typedef CConcurrentMap<string, CNCCacheData>                TKeysCacheMap;
    typedef CNCDeferredDeleter<string, CKeysCleaner, 5, 1000>   TKeysDeleter;


    ///
    static Int8 sx_GetCurTime(void);

    /// Check if storage have been already stopped. Throw special exception in
    /// this case.
    void x_CheckStopped(void);
    /// Implementation of background thread. Mainly garbage collector plus
    /// caching of database data at the beginning of the storage work.
    void x_DoBackgroundWork(void);

    /// Read all storage parameters from registry
    void x_ReadStorageParams(void);
    /// Read from registry only those parameters that can be changed on the
    /// fly, without re-starting the application.
    void x_ReadVariableParams(void);
    ///
    void x_EnsureDirExist(const string& dir_name);
    /// Make name of the index file in the storage
    string x_GetIndexFileName(void);
    /// Make name of file with meta-information in given database part
    string x_GetFileName(ENCDBFileType file_type,
                         unsigned int  part_num,
                         TNCDBFileId   file_id);
    /// Make sure that current storage database is used only with one instance
    /// of NetCache. Lock specially named file exclusively and hold the lock
    /// for all time while process is working. This file also is used for
    /// determining if previous instance of NetCache was closed gracefully.
    /// If another instance of NetCache using the database method will throw
    /// an exception.
    ///
    /// @return
    ///   TRUE if guard file existed and was unlocked meaning that previous
    ///   instance of NetCache was terminated inappropriately. FALSE if file
    ///   didn't exist so that this storage instance is a clean start.
    void x_LockInstanceGuard(void);
    /// Unlock and delete specially named file used to ensure only one
    /// instance working with database.
    /// Small race exists here now which cannot be resolved for Windows
    /// without re-writing of the mechanism using low-level functions. Race
    /// can appear like this: if another instance will start right when this
    /// instance stops then other instance can be thinking that this instance
    /// terminated incorrectly.
    void x_UnlockInstanceGuard(void);
    /// Open and read index database file
    ///
    /// @param reinit_mode
    ///   If some kind of reinit requested by this parameter and index file
    ///   was opened with an error then it's deleted and created empty. In
    ///   this case *reinit_mode variable is changed to avoid any later
    ///   reinits.
    void x_OpenIndexDB(void);
    /// Check if database need re-initialization depending on different
    /// parameters and state of the guard file protecting from several
    /// instances running on the same database.
    ///
    /// @param guard_existed
    ///   Flag if guard file from previous NetCache run existed
    /// @param reinit_mode
    ///   Mode of reinitialization requested in configuration and command line
    /// @param reinit_dirty
    ///   Flag if database should be reinitialized if guard file existed
    void x_ReinitializeStorage(void);
    /// Reinitialize database cleaning all data from it.
    /// Only database is cleaned, internal cache is left intact.
    void x_CleanDatabase(void);
    /// Read value of last blob coordinates which will be used to generate ids
    /// for inserted blobs.
    void x_ReadLastBlobCoords(void);

    /// Get lock holder available for new blob lock.
    /// need_initialize will be set to TRUE if initialization of holder can be
    /// made (storage is not blocked) and FALSE if initialization of holder
    /// will be executed later on storage unblocking.
    CNCBlobAccessor* x_GetAccessor(bool* need_initialize);
    ///
    void x_InitializeAccessor(CNCBlobAccessor* accessor);
    ///
    void x_FindBlobInFiles(const string& key);

    ///
    void x_IncFilePartNum(TNCDBFileId& file_num);
    ///
    TNCDBFileId x_GetNextMetaId(void);
    ///
    TNCChunkId x_GetNextChunkId(void);
    ///
    void x_WriteChunkData(TNCDBFileId          file_id,
                          TNCChunkId           chunk_id,
                          const CNCBlobBuffer* data,
                          bool                 add_blobs_cnt);

    /// Do set of procedures creating and initializing new database part and
    /// switching storage to using new database part as current one.
    void x_CreateNewFile(ENCDBFileType file_type, unsigned int part_num);
    ///
    void x_CreateNewCurFiles(void);
    ///
    void x_DeleteDBFile(TNCDBFilesMap::iterator files_it);

    ///
    bool x_CheckFileAging(TNCDBFileId file_id, CNCDBFile* file);

    ///
    void PrintCacheCounts(CPrintTextProxy& proxy);
    ///
    void CollectCacheStats(void);

    ///
    CNCCacheData* x_GetCacheData(const string& key, ENCAccessType access_type);
    /// Check if record about any blob with given key and subkey exists in
    /// the internal cache.
    bool x_IsBlobFamilyExistsInCache(const string& key);

    ///
    virtual void OnBlockedOpFinish(void);
    /// Wait while garbage collector is working.
    /// Method should be called only when storage is blocked.
    void x_WaitForGC(void);
    ///
    void x_GC_HeartBeat(void);
    /// Delete expired blob from internal cache. Expiration time is checked
    /// in database. Method assumes that it's called only from garbage
    /// collector.
    ///
    /// @return
    ///   TRUE if blob was successfully deleted or if database error occurred
    ///   meaning that blob is inaccessible anyway. FALSE if other thread have
    ///   locked the blob and so method was unable to delete it.
    void x_GC_DeleteExpired(const SNCBlobShortInfo& identity);
    ///
    void x_GC_CleanDBFile(CNCDBFile* metafile, int dead_before);
    /// Collect statistics about number of parts, database files sizes etc.
    void x_GC_CollectFilesStats(void);


    /// Directory for all database files of the storage
    string             m_MainPath;
    /// Name of the storage
    string             m_Prefix;
    /// Directory for all database files of the storage
    vector<string>     m_PartsPaths;
    /// Delay between GC activations
    int                m_GCRunDelay;
    /// Number of blobs treated by GC and by caching mechanism in one batch
    int                m_GCBatchSize;
    ///
    Int8               m_MinFileSize[2];
    ///
    Int8               m_MaxFileSize[2];
    ///
    double             m_MinUsefulPct[2];

    /// Name of guard file excluding several instances to run on the same
    /// database.
    string             m_GuardName;
    /// Lock for guard file representing this instance running.
    /// It will hold exclusive lock all the time while NetCache is
    /// working, so that other instance of NetCache on the same database will
    /// be unable to start.
    AutoPtr<CFileLock> m_GuardLock;
    /// Flag if storage is stopped and in process of destroying
    bool               m_Stopped;
    /// Semaphore allowing immediate stopping of background thread
    CSemaphore         m_StopTrigger;
    /// Background thread running GC and caching
    CRef<CThread>      m_BGThread;

    /// Index database file
    AutoPtr<CNCDBIndexFile>  m_IndexDB;
    /// Read-write lock to work with m_DBParts
    CSpinRWLock              m_DBFilesLock;
    /// List of all database parts in the storage
    TNCDBFilesMap            m_DBFiles;
    ///
    TCurFilesList            m_CurFiles;
    ///
    TNCDBFileId              m_LastFileId;
    /// Id of database part which is now in process of caching information
    /// from.
    volatile TNCDBFileId     m_NotCachedFileId;
    /// Minimum expiration time of all blobs remembered now by the storage. 
    /// Variable is used with assumption that reads and writes for int are
    /// always atomic.
    volatile int             m_LastDeadTime;
    /// Internal cache of blobs identification information sorted to be able
    /// to search by key, subkey and version.
    TKeysCacheMap            m_KeysCache;
    ///
    TKeysDeleter             m_KeysDeleter;
    /// Mutex to control work with pool of lock holders
    CSpinLock                m_HoldersPoolLock;
    /// List of holders available for acquiring new blob locks
    CNCBlobAccessor*         m_FreeAccessors;
    /// List of holders already acquired (or waiting to acquire) a blob lock
    CNCBlobAccessor*         m_UsedAccessors;
    /// Number of lock holders in use
    unsigned int             m_CntUsedHolders;
    /// Mutex to work with m_LastBlob
    CSpinLock                m_LastBlobLock;
    /// Coordinates of the latest blob created by the storage
    SNCBlobCoords            m_LastBlob;
    ///
    Uint8                    m_BlobGeneration;
    ///
    CSpinLock                m_LastChunkLock;
    ///
    TNCChunkId               m_LastChunkId;
    /// Flag showing that garbage collector is working at the moment
    volatile bool            m_GCInWork;
    ///
    CSemaphore               m_GCBlockWaiter;
    ///
    CNCBlobAccessor*         m_GCAccessor;
    /// Flag showing that storage block is requested
    volatile bool            m_Blocked;
    /// Number of blob locks that should be released before all operations on
    /// the storage will be completely stopped.
    unsigned int             m_CntLocksToWait;
    ///
    CNCLongOpTrigger         m_CachingTrigger;
    ///
    volatile bool            m_NeedRecache;
};


///
extern CNCBlobStorage* g_NCStorage;


template <class TFile>
inline
CNCDBFileLock<TFile>::CNCDBFileLock(CNCBlobStorage* storage,
                                    TNCDBFileId     part_id)
    : m_File(NULL)
{
    storage->GetFile(part_id, &m_File);
    m_File->LockDB();
}

template <class TFile>
inline
CNCDBFileLock<TFile>::CNCDBFileLock(CNCDBFile* file)
    : m_File(static_cast<TFile*>(file))
{
    _ASSERT(file->GetType() == TFile::GetClassType());
    m_File->LockDB();
}

template <class TFile>
inline
CNCDBFileLock<TFile>::~CNCDBFileLock(void)
{
    m_File->UnlockDB();
}

template <class TFile>
inline
CNCDBFileLock<TFile>::operator TFile* (void) const
{
    return m_File;
}

template <class TFile>
inline TFile&
CNCDBFileLock<TFile>::operator* (void) const
{
    return *m_File;
}

template <class TFile>
inline TFile*
CNCDBFileLock<TFile>::operator-> (void) const
{
    return m_File;
}



template <class TFile>
inline void
CNCBlobStorage::GetFile(TNCDBFileId file_id,
                        TFile**     file_ptr)
{
    CSpinReadGuard guard(m_DBFilesLock);
    *file_ptr = static_cast<TFile*>(m_DBFiles[file_id].file_obj.get());
    _ASSERT(*file_ptr  &&  (*file_ptr)->GetType() == TFile::GetClassType());
}

inline CNCBlobAccessor*
CNCBlobStorage::x_GetAccessor(bool* need_initialize)
{
    CSpinGuard guard(m_HoldersPoolLock);

    CNCBlobAccessor* holder = m_FreeAccessors;
    if (holder) {
        holder->RemoveFromList(m_FreeAccessors);
    }
    else {
        holder = new CNCBlobAccessor();
    }
    ++m_CntUsedHolders;

    *need_initialize = !m_Blocked;
    return holder;
}

inline void
CNCBlobStorage::ReturnAccessor(CNCBlobAccessor* holder)
{
    CSpinGuard guard(m_HoldersPoolLock);

    holder->AddToList(m_FreeAccessors);
    --m_CntUsedHolders;
    if (m_Blocked  &&  holder->IsInitialized())
        --m_CntLocksToWait;
}

inline CNCCacheData*
CNCBlobStorage::x_GetCacheData(const string& key, ENCAccessType access_type)
{
    CNCCacheData* data;
    if (access_type == eNCCreate) {
        if (m_NotCachedFileId != -1)
            x_FindBlobInFiles(key);
        data = m_KeysCache.GetPtr(key, TKeysCacheMap::eGetOrCreate);
        _ASSERT(data);
    }
    else {
        data = m_KeysCache.GetPtr(key, TKeysCacheMap::eGetOnlyActive);
        if (!data  &&  m_NotCachedFileId != -1) {
            x_FindBlobInFiles(key);
            data = m_KeysCache.GetPtr(key, TKeysCacheMap::eGetOnlyActive);
        }
    }
    return data;
}

inline void
CNCBlobStorage::x_InitializeAccessor(CNCBlobAccessor* acessor)
{
    acessor->Initialize(x_GetCacheData(acessor->GetBlobKey(),
                                       acessor->GetAccessType()));
}

inline CNCBlobAccessor*
CNCBlobStorage::GetBlobAccess(ENCAccessType access,
                              const string& key,
                              const string& password)
{
    bool need_initialize;
    CNCBlobAccessor* accessor(x_GetAccessor(&need_initialize));
    accessor->Prepare(key, password, access);
    if (need_initialize)
        x_InitializeAccessor(accessor);
    else {
        CSpinGuard guard(m_HoldersPoolLock);
        accessor->AddToList(m_UsedAccessors);
    }
    return accessor;
}

inline bool
CNCBlobStorage::IsBlockActive(void)
{
    _ASSERT(m_Blocked);
    return m_CntLocksToWait == 0;
}

inline void
CNCBlobStorage::x_IncFilePartNum(TNCDBFileId& file_num)
{
    if (++file_num == TNCDBFileId(m_CurFiles[0].size()))
        file_num = 0;
}

inline void
CNCBlobStorage::GetNewBlobCoords(SNCBlobCoords* coords)
{
    m_LastBlobLock.Lock();
    if ((++m_LastBlob.blob_id & kNCMaxBlobId) == 0)
        m_LastBlob.blob_id = 1;
    x_IncFilePartNum(m_LastBlob.meta_id);
    x_IncFilePartNum(m_LastBlob.data_id);
    coords->blob_id = m_LastBlob.blob_id;
    coords->meta_id = m_CurFiles[eNCMeta][m_LastBlob.meta_id];
    coords->data_id = m_CurFiles[eNCData][m_LastBlob.data_id];
    m_LastBlobLock.Unlock();
}

inline TNCDBFileId
CNCBlobStorage::x_GetNextMetaId(void)
{
    m_LastBlobLock.Lock();
    x_IncFilePartNum(m_LastBlob.meta_id);
    TNCDBFileId meta_id = m_CurFiles[eNCMeta][m_LastBlob.meta_id];
    m_LastBlobLock.Unlock();
    return meta_id;
}

inline Int8
CNCBlobStorage::sx_GetCurTime(void)
{
#ifdef NCBI_OS_MSWIN
    // Conversion from FILETIME to time since Epoch is taken from MSVC's
    // implementation of time().
    union {
        Uint8    ft_int8;
        FILETIME ft_struct;
    } ft;
    GetSystemTimeAsFileTime(&ft.ft_struct);
    Uint8 epoch_time = ft.ft_int8 - 116444736000000000i64;
    return ((epoch_time / 10000000) << 32) + (epoch_time % 10000000 / 10);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (Int8(tv.tv_sec) << 32) + tv.tv_usec;
#endif
}

inline void
CNCBlobStorage::WriteBlobInfo(const string& blob_key, SNCBlobVerData* ver_data)
{
    ver_data->create_time   = sx_GetCurTime();
    ver_data->dead_time     = static_cast<int>(ver_data->create_time >> 32)
                              + ver_data->ttl;
    ver_data->old_dead_time = ver_data->dead_time;
    ver_data->generation    = m_BlobGeneration;

    TMetaFileLock metafile(this, ver_data->coords.meta_id);
    metafile->WriteBlobInfo(blob_key, ver_data);
    metafile->AddUsefulBlobs(1);
}

inline void
CNCBlobStorage::x_WriteChunkData(TNCDBFileId          file_id,
                                 TNCChunkId           chunk_id,
                                 const CNCBlobBuffer* data,
                                 bool                 add_blobs_cnt)
{
    TDataFileLock datafile(this, file_id);
    datafile->WriteChunkData(chunk_id, data);
    datafile->AddUsefulBlobs((add_blobs_cnt? 1: 0), data->GetSize());
    CNCStat::AddChunkWritten(data->GetSize());
}

inline TNCChunkId
CNCBlobStorage::x_GetNextChunkId(void)
{
    CSpinGuard guard(m_LastChunkLock);
    if ((++m_LastChunkId & kNCMaxChunkId) == 0)
        m_LastChunkId = kNCMinChunkId;
    return m_LastChunkId;
}

inline void
CNCBlobStorage::WriteSingleChunk(SNCBlobVerData*      ver_data,
                                 const CNCBlobBuffer* data)
{
    x_WriteChunkData(ver_data->coords.data_id, ver_data->coords.blob_id, data, true);
    ver_data->disk_size += data->GetSize();
}

inline void
CNCBlobStorage::WriteNextChunk(SNCBlobVerData*      ver_data,
                               const CNCBlobBuffer* data)
{
    TNCChunkId chunk_id = x_GetNextChunkId();
    x_WriteChunkData(ver_data->coords.data_id, chunk_id, data,
                     ver_data->chunks.size() == 0);
    {{
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        metafile->CreateChunk(ver_data->coords.blob_id, chunk_id);
    }}
    ver_data->chunks.push_back(chunk_id);
    ver_data->disk_size += data->GetSize();
}

inline void
CNCBlobStorage::DeleteBlobKey(const string& key)
{
    _VERIFY(m_KeysCache.SetValueStatus(key, TKeysCacheMap::eValuePassive));
    m_KeysDeleter.AddElement(key);
}

inline void
CNCBlobStorage::RestoreBlobKey(const string& key)
{
    _VERIFY(m_KeysCache.SetValueStatus(key, TKeysCacheMap::eValueActive));
}

inline void
CNCBlobStorage::NotifyAfterCaching(INCBlockedOpListener* listener)
{
    if (m_CachingTrigger.StartWorking(listener) == eNCSuccessNoBlock) {
        _ASSERT(m_CachingTrigger.GetState() == eNCOpCompleted);
        listener->Notify();
    }
}


inline void
CNCBlobStorage::CKeysCleaner::Delete(const string& key)
{
    g_NCStorage->m_KeysCache.EraseIfPassive(key);
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE__HPP */
