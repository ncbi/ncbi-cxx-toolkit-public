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


/// Exception which is thrown from all CNCBlobStorage-related classes
class CNCBlobStorageException : public CException
{
public:
    enum EErrCode {
        eUnknown,         ///< Unknown error
        eWrongFileName,   ///< Wrong file name given in storage settings
        eReadOnlyAccess,  ///< Operation is prohibited because of read-only
                          ///< access to the storage
        eCorruptedDB,     ///< Detected corruption of the database
        eTooBigBlob       ///< Attempt to write blob larger than maximum blob
                          ///< size
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
    /// Create scoped pointer to the file from database part with given id
    /// for given storage.
    CNCDBFileLock(CNCBlobStorage* storage, TNCDBPartId          part_id);
    /// Create scoped pointer to the file from given database part
    /// for given storage (convenient analogue to avoid many dereferences
    /// to obtain part_id).
    CNCDBFileLock(CNCBlobStorage* storage, const SNCDBPartInfo& part_info);
    ~CNCDBFileLock(void);

    /// Smart pointer conversions
    operator TFile*   (void) const;
    TFile& operator*  (void) const;
    TFile* operator-> (void) const;

private:
    /// Storage this object is bound to
    CNCBlobStorage* m_Storage;
    /// Database file object obtained from storage
    TFile*          m_File;
    /// Database part id which this object was created for
    TNCDBPartId     m_DBPartId;
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

    /// Print usage statistics for the storage
    void PrintStat(CPrintTextProxy& proxy);

    /// Acquire access to the blob identified by key, subkey and version
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
    /// in database.
    bool IsBlobExists(CTempString key, CTempString subkey = kEmptyStr);

public:
    // For internal use only

    /// Get object gathering database statistics
    CNCDBStat* GetStat(void);
    /// Get blob object from the pool
    CNCBlob* GetBlob(void);
    /// Return blob object to the pool
    void ReturnBlob(CNCBlob* blob);

    /// Get next blob coordinates that can be used for creating new blob.
    void GetNextBlobCoords(SNCBlobCoords* coords);
    /// Lock blob id, i.e. prevent access of others to blob with the same id
    TRWLockHolderRef LockBlobId(TNCBlobId blob_id, ENCBlobAccess access);
    /// Unlock blob id and free lock holder. If lock holder is null then do
    /// nothing.
    void UnlockBlobId(TNCBlobId blob_id, TRWLockHolderRef& rw_holder);

    /// Get coordinates of the blob by its key, subkey and version.
    ///
    /// @return
    ///   TRUE if blob exists and coordinates were successfully read.
    ///   FALSE if blob doesn't exist.
    bool ReadBlobCoords(SNCBlobIdentity* identity);
    /// Create new blob with given identity, obtain blob coordinates if it
    /// already exists.
    ///
    /// @return
    ///   TRUE if blob was successfully created. FALSE if blob with these key,
    ///   subkey and version already exists. Blob coordinates will be returned
    ///   in new_coords in the latter case.
    bool CreateBlob(const SNCBlobIdentity& identity,
                    SNCBlobCoords*         new_coords);
    /// Get key, subkey and version for the blob with given id (inside
    /// identity structure). Method should be called when blob has already
    /// locked.
    ///
    /// @return
    ///   TRUE if blob exists and key data was successfully read (database
    ///   part id inside identity structure is filled with correct number in
    ///   this case too). FALSE if blob doesn't exist.
    bool ReadBlobKey(SNCBlobIdentity* identity);
    /// Get meta information about blob with given coordinates.
    void ReadBlobInfo(SNCBlobInfo* info);
    /// Write meta information about blob into the database.
    void WriteBlobInfo(const SNCBlobInfo& info);
    /// Delete blob from database.
    void DeleteBlob(const SNCBlobCoords& coords);
    /// Get list of chunks ids
    ///
    /// @param coords
    ///   Coordinates of the blob to get chunks ids for
    /// @param id_list
    ///   Pointer to the list receiving chunks ids
    void GetChunkIds(const SNCBlobCoords& coords, TNCIdsList* id_list);
    /// Create new blob chunk
    ///
    /// @param coords
    ///   Coordinates of the blob to create new chunk for
    /// @param data
    ///   Data to populate to the new chunk
    /// @return
    ///   Chunk id for the newly created chunk
    TNCChunkId CreateNewChunk(const SNCBlobCoords& coords,
                              const TNCBlobBuffer& data);
    /// Write data into existing blob chunk
    ///
    /// @param coords
    ///   Coordinates of the blob to write chunk for
    /// @param chunk_id
    ///   Id of the chunk to write data to
    /// @param data
    ///   Data to write into the chunk
    void WriteChunkValue(const SNCBlobCoords& coords,
                         TNCChunkId           chunk_id,
                         const TNCBlobBuffer& data);
    /// Read data from blob chunk
    ///
    /// @param coords
    ///   Coordinates of the blob to read chunk for
    /// @param chunk_id
    ///   Id of the chunk to read
    /// @param buffer
    ///   Buffer to write data to
    /// @return
    ///   TRUE if data exists and was read successfully. FALSE if data doesn't
    ///   exist in the database
    bool ReadChunkValue(const SNCBlobCoords& coords,
                        TNCChunkId           chunk_id,
                        TNCBlobBuffer*       buffer);
    /// Delete last chunks for blob
    ///
    /// @param coords
    ///   Coordinates of the blob to delete chunks for
    /// @param min_chunk_id
    ///   Minimum id of chunks to delete - all blob chunks with ids equal or
    ///   greater than this will be deleted.
    void DeleteLastChunks(const SNCBlobCoords& coords,
                          TNCChunkId           min_chunk_id);

    /// Get database file object from pool
    ///
    /// @param part_id
    ///   Database part id of necessary file
    /// @param file_ptr
    ///   Pointer to variable where pointer to file object will be written to.
    ///   This cannot be returned as method result because GCC 3.0.4 doesn't
    ///   compile such method calls (without template parameter in arguments).
    template <class TFile>
    void GetFile   (TNCDBPartId part_id, TFile** file_ptr);
    /// Return database file object to the pool
    ///
    /// @param part_id
    ///   Database part id of the file
    /// @param file
    ///   Database file object to return
    template <class TFile>
    void ReturnFile(TNCDBPartId part_id, TFile*  file);
    /// Return blob lock holder to the pool
    void ReturnLockHolder(CNCBlobLockHolder* holder);

private:
    CNCBlobStorage(const CNCBlobStorage&);
    CNCBlobStorage& operator= (const CNCBlobStorage&);


    /// Storage operation flags that can be set via storage configuration
    enum EOperationFlags {
        fReadOnly         = 0x1,  ///< Storage is in read-only mode
        fChangeTimeOnRead = 0x2   ///< Storage will change access time on
                                  ///< every read of the blob
    };
    /// Bit mask of EOperationFlags
    typedef int  TOperationFlags;

    typedef set<SNCBlobIdentity, SNCBlobCompareKeys>  TKeyIdMap;
    typedef set<SNCBlobIdentity, SNCBlobCompareIds>   TIdKeyMap;

    typedef map<TNCBlobId, CYieldingRWLock*>          TId2LocksMap;
    typedef deque<CYieldingRWLock*>                   TLocksList;

    typedef CNCDBFileLock<CNCDBMetaFile>              TMetaFileLock;
    typedef CNCDBFileLock<CNCDBDataFile>              TDataFileLock;

    typedef AutoPtr<CNCDBFilesPool>                   TFilesPoolPtr;
    typedef map<TNCDBPartId, TFilesPoolPtr>           TFilesMap;


    /// Check if storage activity is monitored now
    bool x_IsMonitored(void) const;
    /// Post message to activity monitor and _TRACE it if necessary
    void x_MonitorPost(CTempString msg, bool do_trace = true);
    /// Put message about exception into diagnostics and activity monitor
    ///
    /// @param ex
    ///   Exception object to report about
    /// @param msg
    ///   Additional message to prepend to exception's text
    void x_MonitorError(exception& ex, CTempString msg);
    /// Check if storage is writable, throw exception if it is in read-only
    /// mode and access is not eRead.
    void x_CheckReadOnly(ENCBlobAccess access) const;
    /// Check if storage have been already stopped. Throw special exception in
    /// this case.
    void x_CheckStopped(void);
    /// Implementation of background thread. Mainly garbage collector plus
    /// caching of database data at the beginning of the storage work.
    void x_DoBackgroundWork(void);

    /// Read all storage parameters from registry
    void x_ReadStorageParams(const IRegistry& reg, CTempString section);
    /// Parse value of 'timestamp' parameter from registry
    void x_ParseTimestampParam(const string& timestamp);
    /// Make name of the index file in the storage
    string x_GetIndexFileName(void);
    /// Make name of file with meta-information in given database part
    string x_GetMetaFileName(TNCDBPartId part_id);
    /// Make name of file with blobs' data in given database part
    string x_GetDataFileName(TNCDBPartId part_id);
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
    bool x_LockInstanceGuard(void);
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
    void x_OpenIndexDB(EReinitMode* reinit_mode);
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
    void x_CheckDBInitialized(bool        guard_existed,
                              EReinitMode reinit_mode,
                              bool        reinit_dirty);
    /// Check that all information about database parts contains non-zero
    /// value of minimum blob id. If some value is zero then update it based
    /// on information from previous database parts.
    void x_CheckPartsMinBlobId(void);
    /// Read value of last blob coordinates which will be used to generate ids
    /// for inserted blobs.
    void x_ReadLastBlobCoords(void);

    /// Acquire access to the blob identified by its coordinates
    ///
    /// @param access
    ///   Type of access to blob required
    /// @param part_id
    ///   Id of the database part for blob
    /// @param blob_id
    ///   Id of the blob
    /// @param change_access_time
    ///   Flag if blob access time should change (meaningful only when eRead
    ///   access is requested).
    TNCBlobLockHolderRef x_GetBlobAccess(ENCBlobAccess access,
                                         TNCDBPartId   part_id,
                                         TNCBlobId     blob_id,
                                         bool          change_access_time);

    /// Release all id lock objects. Method to be called only from destructor.
    void x_FreeBlobIdLocks(void);

    /// Initialize pool of database files for given database part
    void x_InitFilesPool(const SNCDBPartInfo& part_info);
    /// Create database structure for new database part
    void x_CreateNewDBStructure(TNCDBPartId part_id);
    /// Switch storage to use newly created database part as current, i.e.
    /// add it to the list of all parts and make last blob coordinates
    /// pointing to this part. Also value of minimum blob id in the part
    /// information structure is set.
    void x_SwitchCurrentDBPart(SNCDBPartInfo* part_info);
    /// Do set of procedures creating and initializing new database part and
    /// switching storage to using new database part as current one.
    void x_CreateNewDBPart(void);
    /// Do the database parts rotation. If last and current database part were
    /// created too long ago (more than m_DBRotatePeriod) then new database
    /// part is created and becomes current. But only if current part contains
    /// any information (even about "non-exiting" blobs) otherwise its
    /// creation time is just changing giving a second life to the part.
    void x_RotateDBParts(void);

    /// Get id of the database part which is now in process of filling the
    /// cache. Cache is filled in the order of most recent part to most old
    /// one. So that all parts after returned one will be already in cache.
    /// Value of -1 means that all database is cached at the moment.
    TNCDBPartId x_GetNotCachedPartId(void);
    /// Set id of the database part which is now will be in process of filling
    /// the cache. Value of -1 means that caching process is completed.
    void x_SetNotCachedPartId(TNCDBPartId part_id);
    /// Copy list of all database parts. Method should be used to avoid
    /// holding of read lock over parts list for too long (requirement of
    /// CFastRWLock).
    void x_CopyPartsList(TNCDBPartsList* parts_list);
    /// Read coordinates of the blob identified by key, subkey and version
    /// from internal cache (without reading from database).
    bool x_ReadBlobCoordsFromCache(SNCBlobIdentity* identity);
    /// Create record about blob in the internal cache. If record was already
    /// created then copy blob coordinates into given coordinates structure.
    ///
    /// @return
    ///   TRUE if blob record was successfully added to the cache. FALSE if
    ///   record have already existed at the moment of insert and thus new
    ///   coordinates were written into the new_coords.
    bool x_CreateBlobInCache(const SNCBlobIdentity& identity,
                             SNCBlobCoords*         new_coords);
    /// Read key, subkey and version of the blob identified by id from the
    /// internal cache.
    ///
    /// @return
    ///   TRUE if record about the blob was found and key data was written.
    ///   FALSE if record doesn't exist in the cache.
    bool x_ReadBlobKeyFromCache(SNCBlobIdentity* identity);
    /// Delete blob identified by its coordinates from the internal cache.
    void x_DeleteBlobFromCache(const SNCBlobCoords& coords);
    /// Get list of blob ids with given key from internal cache. Method
    /// call have any sense only when all caching process is finished.
    void x_FillBlobIdsFromCache(const string& key, TNCIdsList* id_list);
    /// Check if record about any blob with given key and subkey exists in
    /// the internal cache.
    bool x_IsBlobExistsInCache(CTempString key, CTempString subkey);
    /// Fill internal cache data with information from given database part.
    void x_FillCacheFromDBPart(TNCDBPartId part_id);

    /// Delete expired blob from internal cache. Expiration time is checked
    /// in database. Method assumes that it's called only from garbage
    /// collector.
    ///
    /// @return
    ///   TRUE if blob was successfully deleted or if database error occurred
    ///   meaning that blob is inaccessible anyway. FALSE if other thread have
    ///   locked the blob and so method was unable to delete it.
    bool x_GC_DeleteExpired(TNCDBPartId part_id, TNCBlobId blob_id);
    /// Prepare pool of database files for given part to be deleted. Pointer
    /// to pool is assigned to given variable and deleted from map of all
    /// pools. Deleting from map is going under mutex but physical deleting of
    /// pool and all of the file objects in it could be rather long. So mutex
    /// is released before deleting takes place.
    void x_GC_PrepFilesPoolToDelete(TNCDBPartId    part_id,
                                    TFilesPoolPtr* pool_ptr);
    /// Delete given database part from list of all parts, from index file and
    /// from disk.
    void x_GC_DeleteDBPart(TNCDBPartsList::iterator part_it);
    /// Check if database file with meta-information about blobs contains any
    /// existing blobs. Only blobs live at the dead_after moment or after that
    /// are considered existing. If database error occurs during checking then
    /// file is assumed empty - it's not accessible anyway.
    ///
    /// @param part_id
    ///   Id of the database part checked
    /// @param metafile
    ///   Already acquired database file object to check
    /// @param dead_after
    ///   Moment in time for determining existence of blobs
    /// @return
    ///   TRUE if metafile is empty and can be deleted, FALSE otherwise
    bool x_GC_IsMetafileEmpty(TNCDBPartId          part_id,
                              const TMetaFileLock& metafile,
                              int                  dead_after);
    /// Clean database part deleting from internal cache all blobs from this
    /// part which have expired before dead_before moment. If as a result
    /// this part is not having any blobs it will be deleted.
    ///
    /// @return
    ///   TRUE if database part was successfully cleaned and current minimum
    ///   expiration time can be changed to dead_before moment. FALSE if some
    ///   blobs were locked by other threads and they should be cleaned once
    ///   more, thus changing minimum expiration time cannot be changed now.
    ///
    /// @sa m_LastDeadTime
    bool x_GC_CleanDBPart(TNCDBPartsList::iterator part_it,
                          int                      dead_before);
    /// Collect statistics about number of parts, database files sizes etc.
    void x_GC_CollectPartsStatistics(void);


    /// Directory for all database files of the storage
    string             m_Path;
    /// Name of the storage
    string             m_Name;
    /// Default value of blob's time-to-live
    int                m_DefBlobTTL;
    /// Maximum value of blob's time-to-live
    int                m_MaxBlobTTL;
    /// Maximum allowed blob size
    size_t             m_MaxBlobSize;
    /// Time period while one database part is current before creating new
    /// part and switching to it as current.
    int                m_DBRotatePeriod;
    /// Delay between GC activations
    int                m_GCRunDelay;
    /// Number of blobs treated by GC and by caching mechanism in one batch
    int                m_BlobsBatchSize;
    /// Time in milliseconds GC will sleep between batches
    int                m_GCBatchSleep;

    /// Name of guard file excluding several instances to run on the same
    /// database.
    string             m_GuardName;
    /// Lock for guard file representing this instance running.
    /// It will hold exclusive lock all the time while NetCache is
    /// working, so that other instance of NetCache on the same database will
    /// be unable to start.
    AutoPtr<CFileLock> m_GuardLock;
    /// Object monitoring activity on the storage
    CServer_Monitor*   m_Monitor;
    /// Object gathering statistics for the storage
    CNCDBStat          m_Stat;
    /// Operation flags read from configuration
    TOperationFlags    m_Flags;
    /// Flag if storage is stopped and in process of destroying
    bool               m_Stopped;
    /// Semaphore allowing immediate stopping of background thread
    CSemaphore         m_StopTrigger;
    /// Background thread running GC and caching
    CRef<CThread>      m_BGThread;

    /// Read-write lock to work with m_KeysCache
    CFastRWLock              m_KeysCacheLock;
    /// Internal cache of blobs identification information sorted to be able
    /// to search by key, subkey and version.
    TKeyIdMap                m_KeysCache;
    /// Read-write lock to work with m_IdsCache
    CFastRWLock              m_IdsCacheLock;
    /// Internal cache of blobs identification information sorted to be able
    /// to search by blob id.
    TIdKeyMap                m_IdsCache;
    /// Index database file
    AutoPtr<CNCDBIndexFile>  m_IndexDB;
    /// Read-write lock to work with m_DBParts
    CFastRWLock              m_DBPartsLock;
    /// List of all database parts in the storage
    TNCDBPartsList           m_DBParts;
    /// Flag showing that information from all database parts is already
    /// cached. Flag is used as optimization to avoid read lock on
    /// m_NotCachedPartIdLock which is necessary to access m_NotCachedPartId
    /// because it can be not atomic. This flag is false at the creation,
    /// experiences only one change to true after caching process is done and
    /// no harm will be caused if it will be read as FALSE when it's just
    /// changed to TRUE. So this variable can be used concurrently without
    /// mutexes.
    bool                     m_AllDBPartsCached;
    /// Read-write lock to work with m_NotCachedPartId
    CFastRWLock              m_NotCachedPartIdLock;
    /// Id of database part which is now in process of caching information
    /// from.
    TNCDBPartId              m_NotCachedPartId;
    /// Minimum expiration time of all blobs remembered now by the storage. 
    /// Variable is used with assumption that reads and writes for int are
    /// always atomic.
    volatile int             m_LastDeadTime;
    /// Read-write lock to work with m_DBFiles
    CFastRWLock              m_DBFilesMutex;
    /// Map containing database files pools sorted to be searchable by
    /// database part id.
    TFilesMap                m_DBFiles;
    /// Current size of storage database. Kept here for printing statistics.
    Int8                     m_CurDBSize;
    /// Pool of CNCBlobLockHolder objects
    TNCBlobLockObjPool       m_LockHoldersPool;
    /// Pool of CNCBlob objects
    TNCBlobsPool             m_BlobsPool;
    /// Mutex to work with m_LastBlob
    CSpinLock                m_LastBlobLock;
    /// Coordinates of the latest blob created by the storage
    SNCBlobCoords            m_LastBlob;
    /// Mutex for locking/unlocking blob ids
    CSpinLock                m_IdLockMutex;
    /// Map for CYieldingRWLock objects sorted to be searchable by blob id
    TId2LocksMap             m_IdLocks;
    /// CYieldingRWLock objects that are not used by anybody at the moment
    TLocksList               m_FreeLocks;
};



template <class TFile>
inline
CNCDBFileLock<TFile>::CNCDBFileLock(CNCBlobStorage*      storage,
                                    const SNCDBPartInfo& part_info)
    : m_Storage(storage),
      m_DBPartId(part_info.part_id)
{
    storage->GetFile(part_info.part_id, &m_File);
}

template <class TFile>
inline
CNCDBFileLock<TFile>::CNCDBFileLock(CNCBlobStorage* storage,
                                    TNCDBPartId     part_id)
    : m_Storage(storage),
      m_DBPartId(part_id)
{
    storage->GetFile(part_id, &m_File);
}

template <class TFile>
inline
CNCDBFileLock<TFile>::~CNCDBFileLock(void)
{
    m_Storage->ReturnFile(m_DBPartId, m_File);
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

inline CNCDBStat*
CNCBlobStorage::GetStat(void)
{
    return &m_Stat;
}

inline CNCBlob*
CNCBlobStorage::GetBlob(void)
{
    return m_BlobsPool.Get();
}

inline void
CNCBlobStorage::ReturnBlob(CNCBlob* blob)
{
    m_BlobsPool.Return(blob);
}

inline void
CNCBlobStorage::GetNextBlobCoords(SNCBlobCoords* coords)
{
    m_LastBlobLock.Lock();
    if (++m_LastBlob.blob_id <= 0)
        m_LastBlob.blob_id = 1;
    coords->AssignCoords(m_LastBlob);
    m_LastBlobLock.Unlock();
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

inline void
CNCBlobStorage::x_CheckReadOnly(ENCBlobAccess access) const
{
    if (access != eRead  &&  IsReadOnly()) {
        NCBI_THROW(CNCBlobStorageException, eReadOnlyAccess,
                   "Database is in read only mode");
    }
}

template <class TFile>
inline void
CNCBlobStorage::GetFile(TNCDBPartId part_id, TFile** file_ptr)
{
    CNCDBFilesPool* pool = NULL;
    {{
        CFastReadGuard guard(m_DBFilesMutex);
        pool = m_DBFiles[part_id].get();
    }}
    _ASSERT(pool);
    pool->GetFile(file_ptr);
}

template <class TFile>
inline void
CNCBlobStorage::ReturnFile(TNCDBPartId part_id, TFile* file)
{
    CNCDBFilesPool* pool;
    {{
        CFastReadGuard guard(m_DBFilesMutex);
        pool = m_DBFiles[part_id].get();
    }}    
    _ASSERT(pool);
    pool->ReturnFile(file);
}

inline void
CNCBlobStorage::ReturnLockHolder(CNCBlobLockHolder* holder)
{
    m_LockHoldersPool.Return(holder);
}

inline void
CNCBlobStorage::ReadBlobInfo(SNCBlobInfo* info)
{
    _ASSERT(info->part_id != 0  &&  info->blob_id != 0);

    TMetaFileLock metafile(this, info->part_id);
    metafile->ReadBlobInfo(info);
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
CNCBlobStorage::WriteChunkValue(const SNCBlobCoords& coords,
                                TNCChunkId           chunk_id,
                                const TNCBlobBuffer& data)
{
    TDataFileLock datafile(this, coords.part_id);
    datafile->WriteChunkValue(chunk_id, data);
}

inline TNCChunkId
CNCBlobStorage::CreateNewChunk(const SNCBlobCoords& coords,
                               const TNCBlobBuffer& data)
{
    TNCChunkId chunk_id;
    {{
        TDataFileLock datafile(this, coords.part_id);
        chunk_id = datafile->CreateChunkValue(data);
    }}
    {{
        TMetaFileLock metafile(this, coords.part_id);
        metafile->CreateChunk(coords.blob_id, chunk_id);
    }}
    return chunk_id;
}

inline bool
CNCBlobStorage::ReadChunkValue(const SNCBlobCoords& coords,
                               TNCChunkId           chunk_id,
                               TNCBlobBuffer*       buffer)
{
    TDataFileLock datafile(this, coords.part_id);
    return datafile->ReadChunkValue(chunk_id, buffer);
}

inline void
CNCBlobStorage::DeleteLastChunks(const SNCBlobCoords& coords,
                                 TNCChunkId           min_chunk_id)
{
    TMetaFileLock metafile(this, coords.part_id);
    metafile->DeleteLastChunks(coords.blob_id, min_chunk_id);
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
CNCBlobStorage::x_GetBlobAccess(ENCBlobAccess access,
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
    return x_GetBlobAccess(access, 0, blob_id, IsChangeTimeOnRead());
}

END_NCBI_SCOPE

#endif /* NETCACHE_NETCACHE_DB__HPP */
