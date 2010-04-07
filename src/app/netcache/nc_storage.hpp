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
#include "nc_db_stat.hpp"
#include "nc_db_files.hpp"
#include "nc_storage_blob.hpp"

#include "concurrent_map.hpp"

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
        eTooBigBlob,      ///< Attempt to write blob larger than maximum blob
                          ///< size
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
                  TNCDBPartId     part_id,
                  TNCDBVolumeId   vol_id);
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
    /// Id of volume in the database part
    TNCDBVolumeId   m_DBVolId;
};


/// Blob storage for NetCache
class CNCBlobStorage
{
public:
    /// Empty constructor. All initialization happens in Initialize().
    CNCBlobStorage(void);
    virtual ~CNCBlobStorage(void);

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
    void Initialize(const IRegistry& reg,
                    const string&    section,
                    EReinitMode      reinit_mode);

    /// Set object to monitor storage activity
    void SetMonitor(CServer_Monitor* monitor);
    /// Re-read configuration of the storage
    ///
    /// @param reg
    ///   Registry to read object configuration. All parameters that cannot be
    ///   changed on the fly are set to old values in the registry.
    /// @param section
    ///   Section name inside registry to read configuration from
    void Reconfigure(CNcbiRegistry& reg, const string& section);

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
                                       const string& subkey,
                                       int           version,
                                       const string& password);
    /// Check if blob with given key and (optionally) subkey exists
    /// in database. More than one blob with given key/subkey can exist.
    bool IsBlobFamilyExists(const string& key, const string& subkey);

    /// Block all operations on the storage.
    /// Work with all blobs that are blocked already will be finished as usual,
    /// all future requests for locking of blobs will be suspended until
    /// Unblock() is called. Method will return immediately and will not wait
    /// for already locked blobs to be unlocked - CanDoExclusive() call should
    /// be used to understand if all operations on the storage are freezed.
    void Block(void);
    /// Unblock the storage and allow it to continue work as usual.
    void Unblock(void);
    /// Check if blocking of storage is requested
    bool IsBlocked(void);
    /// Check if storage blocking is completed and all operations with storage
    /// are stopped.
    bool CanDoExclusive(void);
    /// Reinitialize storage database.
    /// Method can be called only when storage is completely blocked and all
    /// operations with it are finished.
    void Reinitialize(void);

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
    /// Move blob definition to another coordinates if they are from newer
    /// database part. It prevents some database parts to exist forever if
    /// some blob in them keeps to be re-written over and over again.
    bool MoveBlobIfNeeded(const SNCBlobIdentity& identity,
                          const SNCBlobCoords&   new_coords);

    /// Tri-state value that can be returned from IsBlobExists()
    enum EExistsOrMoved {
        eBlobNotExists,     ///< Blob doesn't exist in the storage
        eBlobMoved,         ///< Blob with given keys exists but its
                            ///< coordinates (including blob id) inside storage
                            ///< were changed
        eBlobExists         ///< Blob exists in the storage and is located at
                            ///< exactly the same coordinates
    };

    /// Check whether blob with given key/subkey/version exists in the storage
    /// and if it exists check if its coordinates are still the same. If blob
    /// was moved (inside MoveBlobIfNeeded() in some other thread) then fill
    /// new_coords with its new coordinates.
    EExistsOrMoved IsBlobExists(const SNCBlobIdentity& identity,
                                SNCBlobCoords*         new_coords);
    /// Get meta information about blob with given coordinates.
    ///
    /// @return
    ///   TRUE if meta information exists in the database and was successfully
    ///   read. FALSE if database is corrupted.
    bool ReadBlobInfo(SNCBlobInfo* info);
    /// Write meta information about blob into the database.
    void WriteBlobInfo(SNCBlobInfo& info);
    /// Delete blob from database.
    void DeleteBlob(const SNCBlobIdentity& identity);
    /// Get list of chunks ids
    ///
    /// @param coords
    ///   Coordinates of the blob to get chunks ids for
    /// @param id_list
    ///   Pointer to the list receiving chunks ids
    void GetChunkIds(const SNCBlobCoords& coords, TNCChunksList* id_list);
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
    /// @param vol_id
    ///   Id of volume for necessary file
    /// @param file_ptr
    ///   Pointer to variable where pointer to file object will be written to.
    ///   This cannot be returned as method result because GCC 3.0.4 doesn't
    ///   compile such method calls (without template parameter in arguments).
    template <class TFile>
    void GetFile   (TNCDBPartId   part_id,
                    TNCDBVolumeId vol_id,
                    TFile**       file_ptr);
    /// Return database file object to the pool
    ///
    /// @param part_id
    ///   Database part id of the file
    /// @param vol_id
    ///   Id of volume for the file
    /// @param file
    ///   Database file object to return
    template <class TFile>
    void ReturnFile(TNCDBPartId   part_id,
                    TNCDBVolumeId vol_id,
                    TFile*        file);
    /// Return blob lock holder to the pool
    void ReturnLockHolder(CNCBlobLockHolder* holder);

protected:
    /// Deinitilize storage and prepare for deletion.
    void Deinitialize(void);

    ///
    virtual void HeartBeat(void) = 0;
    ///
    virtual void PrintCacheCounts(CPrintTextProxy& proxy) = 0;
    ///
    virtual void CollectCacheStats(void) = 0;

    /// Create pool that will provide objects to work with database files
    virtual CNCDBFilesPool* CreateFilesPool(const string& meta_name,
                                            const string& data_name) = 0;
    /// Read coordinates of the blob identified by key, subkey and version
    /// from internal cache (without reading from database).
    virtual bool ReadBlobCoordsFromCache(SNCBlobIdentity* identity) = 0;
    /// Create record about blob in the internal cache. If record was already
    /// created then copy blob coordinates into given coordinates structure.
    ///
    /// @return
    ///   TRUE if blob record was successfully added to the cache. FALSE if
    ///   record have already existed at the moment of insert and thus new
    ///   coordinates were written into the new_coords.
    virtual bool CreateBlobInCache(const SNCBlobIdentity& identity,
                                   SNCBlobCoords*         new_coords) = 0;
    /// Change coordinates for given blob in the internal cache in such way
    /// so that other threads won't see blob disappearance and won't see
    /// inconsistency in coordinates.
    virtual void MoveBlobInCache(const SNCBlobKeys&   keys,
                                 const SNCBlobCoords& new_coords) = 0;
    /// Check whether blob with given key/subkey/version exists in the internal
    /// cache and if it exists check if its coordinates are still the same.
    /// If blob was moved (inside MoveBlobIfNeeded() in some other thread)
    /// then fill new_coords with its new coordinates.
    virtual EExistsOrMoved
                 IsBlobExistsInCache(const SNCBlobIdentity& identity,
                                     SNCBlobCoords*         new_coords) = 0;
    /// Delete blob identified by its coordinates from the internal cache.
    virtual void DeleteBlobFromCache(const SNCBlobKeys& keys) = 0;
    /// Clean all blobs from internal cache without cleaning the database
    virtual void CleanCache(void) = 0;
    /// Check if record about any blob with given key and subkey exists in
    /// the internal cache.
    virtual bool IsBlobFamilyExistsInCache(const SNCBlobKeys& keys) = 0;

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

    typedef map<TNCBlobId, CYieldingRWLock*>          TId2LocksMap;
    typedef deque<CYieldingRWLock*>                   TLocksList;

    typedef CNCDBFileLock<CNCDBMetaFile>              TMetaFileLock;
    typedef CNCDBFileLock<CNCDBDataFile>              TDataFileLock;

    typedef AutoPtr<CNCDBFilesPool>                   TFilesPoolPtr;
    typedef map<TNCDBVolumeId, TFilesPoolPtr>         TPartFilesMap;
    typedef map<TNCDBPartId, TPartFilesMap>           TFilesMap;


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
    void x_ReadStorageParams(const IRegistry& reg, const string& section);
    /// Read from registry only those parameters that can be changed on the
    /// fly, without re-starting the application.
    void x_ReadVariableParams(const IRegistry& reg, const string& section);
    /// Parse value of 'timestamp' parameter from registry
    void x_ParseTimestampParam(const string& timestamp);
    /// Make name of the index file in the storage
    string x_GetIndexFileName(void);
    /// Make name of file with meta-information in given database part
    string x_GetMetaFileName(TNCDBPartId part_id);
    /// Make name of file with blobs' data in given database part
    string x_GetDataFileName(TNCDBPartId part_id);
    /// Make name of database file in particular database volume for the given
    /// file name prefix (generated by x_GetMetaFileName() or
    /// x_GetDataFileName() ).
    string x_GetVolumeName(const string& prefix, TNCDBVolumeId vol_id);
    /// Get minimum id of volume in the given part. It can be 0 if part was
    /// created by previous version of NetCache and 1 if it was created by
    /// this version of NetCache.
    TNCDBVolumeId x_GetMinVolumeId(const SNCDBPartInfo& part_info);
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
    CNCBlobLockHolder* x_GetLockHolder(bool* need_initialize);
    /// Acquire access to the blob identified by its full coordinates
    /// information
    ///
    /// @param access
    ///   Type of access to blob required
    /// @param identity
    ///   Identifying information about blob
    TNCBlobLockHolderRef x_GetBlobAccess(ENCBlobAccess          access,
                                         const SNCBlobIdentity& identity);

    /// Release all id lock objects. Method to be called only from destructor.
    void x_FreeBlobIdLocks(void);

    /// Initialize pool of database files for given database part.
    /// Return initialized pools of volume files for this part.
    TPartFilesMap& x_InitFilePools(const SNCDBPartInfo& part_info);
    /// Create database structure for new database part
    void x_CreateNewDBStructure(const SNCDBPartInfo& part_info);
    /// Switch storage to use newly created database part as current, i.e.
    /// add it to the list of all parts and make last blob coordinates
    /// pointing to this part. Also value of minimum blob id in the part
    /// information structure is set.
    void x_SwitchCurrentDBPart(SNCDBPartInfo* part_info);
    /// Do set of procedures creating and initializing new database part and
    /// switching storage to using new database part as current one.
    void x_CreateNewDBPart(void);
    /// Prepare pool of database files for given part to be deleted. Pointer
    /// to pool is assigned to given variable and deleted from map of all
    /// pools. Deleting from map is going under mutex but physical deleting of
    /// pool and all of the file objects in it could be rather long. So mutex
    /// is released before deleting takes place.
    void x_PrepFilePoolsToDelete(TNCDBPartId    part_id,
                                 TPartFilesMap* pools_map);
    /// Delete given database part from list of all parts, from index file and
    /// from disk.
    void x_DeleteDBPart(TNCDBPartsList::iterator part_it);
    /// Do the database parts rotation. If last and current database part were
    /// created too long ago (more than m_DBRotatePeriod) then new database
    /// part is created and becomes current. But only if current part contains
    /// any information (even about "non-exiting" blobs) otherwise its
    /// creation time is just changing giving a second life to the part. Also
    /// rotation can be forced with parameter equal TRUE. In this case
    /// conditions of containing data and too old creation are not checked.
    void x_RotateDBParts(bool force_rotate = false);

    /// Get id of the database part which is now in process of filling the
    /// cache. Cache is filled in the order of most recent part to most old
    /// one. So that all parts after returned one will be already in cache.
    /// Value of -1 means that all database is cached at the moment.
    TNCDBPartId x_GetNotCachedPartId(void);
    /// Set id of the database part which is now will be in process of filling
    /// the cache. Value of -1 means that caching process is completed.
    void x_SetNotCachedPartId(TNCDBPartId part_id);
    /// Fill internal cache data with information from given volume in the
    /// database part.
    void x_FillCacheFromDBVolume(TNCDBPartId part_id, TNCDBVolumeId vol_id);
    /// Fill internal cache data with information from given database part.
    void x_FillCacheFromDBPart(TNCDBPartId part_id);

    /// Wait while garbage collector is working.
    /// Method should be called only when storage is blocked.
    void x_WaitForGC(void);
    /// Delete expired blob from internal cache. Expiration time is checked
    /// in database. Method assumes that it's called only from garbage
    /// collector.
    ///
    /// @return
    ///   TRUE if blob was successfully deleted or if database error occurred
    ///   meaning that blob is inaccessible anyway. FALSE if other thread have
    ///   locked the blob and so method was unable to delete it.
    bool x_GC_DeleteExpired(const SNCBlobIdentity& identity);
    /// Check if database file with meta-information about blobs contains any
    /// existing blobs. Only blobs live at the dead_after moment or after that
    /// are considered existing. If database error occurs during checking then
    /// file is assumed empty - it's not accessible anyway.
    ///
    /// @param part_info
    ///   Database part checked
    /// @param dead_after
    ///   Moment in time for determining existence of blobs
    /// @return
    ///   TRUE if metafile is empty and can be deleted, FALSE otherwise
    bool x_GC_IsDBPartEmpty(const SNCDBPartInfo& part_info, int dead_after);
    /// Clean volume in the database part deleting from internal cache all
    /// blobs from this volume which have expired before dead_before moment.
    ///
    /// @return
    ///   TRUE if volume was successfully cleaned and current minimum
    ///   expiration time can be changed to dead_before moment. FALSE if some
    ///   blobs were locked by other threads and they should be cleaned once
    ///   more, thus changing minimum expiration time cannot be changed now.
    ///
    /// @sa m_LastDeadTime, x_GC_CleanDBPart
    bool x_GC_CleanDBVolume(TNCDBPartId   part_id,
                            TNCDBVolumeId vol_id,
                            int           dead_before);
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
    void x_GC_CollectPartsStats(void);


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
    /// Number of volumes that will be used in newly created database parts
    TNCDBVolumeId      m_DBCntVolumes;
    /// Delay between GC activations
    int                m_GCRunDelay;
    /// Number of blobs treated by GC and by caching mechanism in one batch
    int                m_BlobsBatchSize;

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
    /// Mutex to control work with pool of lock holders
    CSpinLock                m_HoldersPoolLock;
    /// List of holders available for acquiring new blob locks
    CNCBlobLockHolder*       m_FreeHolders;
    /// List of holders already acquired (or waiting to acquire) a blob lock
    CNCBlobLockHolder*       m_UsedHolders;
    /// Number of lock holders in use
    unsigned int             m_CntUsedHolders;
    /// Pool of CNCBlob objects
    TNCBlobsPool             m_BlobsPool;
    /// Mutex to work with m_LastBlob
    CSpinLock                m_LastBlobLock;
    /// Coordinates of the latest blob created by the storage
    SNCBlobCoords            m_LastBlob;
    /// Information about the database part used to write the latest blob
    SNCDBPartInfo*           m_LastPart;
    /// Mutex for locking/unlocking blob ids
    CSpinLock                m_IdLockMutex;
    /// Map for CYieldingRWLock objects sorted to be searchable by blob id
    TId2LocksMap             m_IdLocks;
    /// CYieldingRWLock objects that are not used by anybody at the moment
    TLocksList               m_FreeLocks;
    /// Flag showing that garbage collector is working at the moment
    volatile bool            m_GCInWork;
    /// Flag showing that storage block is requested
    volatile bool            m_Blocked;
    /// Number of blob locks that should be released before all operations on
    /// the storage will be completely stopped.
    unsigned int             m_CntLocksToWait;
};


/// Special traits template to deal with differences between different
/// instantiations of CNCBlobStorage_Specific class. Template shouldn't be
/// implemented in general - it has specializations for both template
/// parameter types used in NetCache.
template <class C>
struct SNCStorageKeyTraits
{
    /// Check if two objects are from the same "family" (not necessarily fully
    /// equal).
    static bool IsFamilyEqual(const C& left, const C& right);
    /// Check if left object is less than right.
    /// Method is necessary to use traits class as comparator in STL container.
    bool operator() (const C& left, const C& right) const;
};

/// Specific implementation of blob storage.
/// Class is dedicated to dealing with differences between pure NetCache- and
/// ICache-related functionality. So that with pure NetCache (the most used
/// flavor of NetCache) we don't waste space/time by dealing with subkeys and
/// versions of blobs.
///
/// @param TFilesPool
///   Type of database files pool to use
/// @param TCacheKey
///   Type to use as key in internal cache of all blobs
template <class TFilesPool, class TCacheKey>
class CNCBlobStorage_Specific : public CNCBlobStorage
{
public:
    CNCBlobStorage_Specific(void);
    virtual ~CNCBlobStorage_Specific(void);

private:
    typedef const TCacheKey&                TKeyConstRef;
    typedef SNCStorageKeyTraits<TCacheKey>  TKeyTraits;

    ///
    virtual void HeartBeat(void);
    ///
    virtual void PrintCacheCounts(CPrintTextProxy& proxy);
    ///
    virtual void CollectCacheStats(void);
    /// Create pool that will provide objects to work with database files
    virtual CNCDBFilesPool* CreateFilesPool(const string& meta_name,
                                            const string& data_name);
    /// Read coordinates of the blob identified by key, subkey and version
    /// from internal cache (without reading from database).
    virtual bool ReadBlobCoordsFromCache(SNCBlobIdentity* identity);
    /// Create record about blob in the internal cache. If record was already
    /// created then copy blob coordinates into given coordinates structure.
    ///
    /// @return
    ///   TRUE if blob record was successfully added to the cache. FALSE if
    ///   record have already existed at the moment of insert and thus new
    ///   coordinates were written into the new_coords.
    virtual bool CreateBlobInCache(const SNCBlobIdentity& identity,
                                   SNCBlobCoords*         new_coords);
    /// Change coordinates for given blob in the internal cache in such way
    /// so that other threads won't see blob disappearance and won't see
    /// inconsistency in coordinates.
    virtual void MoveBlobInCache(const SNCBlobKeys&   keys,
                                 const SNCBlobCoords& new_coords);
    /// Check whether blob with given key/subkey/version exists in the internal
    /// cache and if it exists check if its coordinates are still the same.
    /// If blob was moved (inside MoveBlobIfNeeded() in some other thread)
    /// then fill new_coords with its new coordinates.
    virtual EExistsOrMoved
                 IsBlobExistsInCache(const SNCBlobIdentity& identity,
                                     SNCBlobCoords*         new_coords);
    /// Delete blob identified by its coordinates from the internal cache.
    virtual void DeleteBlobFromCache(const SNCBlobKeys& keys);
    /// Clean all blobs from internal cache without cleaning the database
    virtual void CleanCache(void);
    /// Check if record about any blob with given key and subkey exists in
    /// the internal cache.
    virtual bool IsBlobFamilyExistsInCache(const SNCBlobKeys& keys);


    typedef CConcurrentMap<TCacheKey, SNCBlobCoords, TKeyTraits> TKeyIdMap;

    /// Internal cache of blobs identification information sorted to be able
    /// to search by key, subkey and version.
    TKeyIdMap   m_KeysCache;
};

/// Type of blob storage dedicated to ICache-related functionality
typedef CNCBlobStorage_Specific<CNCDBFilesPool_ICache,
                                SNCBlobKeys>            CNCBlobStorage_ICache;
/// Type of blob storage dedicated to pure NetCache-related functionality
typedef CNCBlobStorage_Specific<CNCDBFilesPool_NCCache,
                                string>                 CNCBlobStorage_NCCache;

/// Specialization of traits template to use with ICache-related blob storage
template <>
struct SNCStorageKeyTraits<SNCBlobKeys>
{
    /// Check if two keys objects are from the same "family", i.e. key and
    /// subkey are equal but version can be different.
    static bool IsFamilyEqual(const SNCBlobKeys& left, const SNCBlobKeys& right)
    {
        return left.key == right.key  &&  left.subkey == right.subkey;
    }
    /// Check if left object is less than right.
    /// Method is necessary to use traits class as comparator in STL container.
    bool operator() (const SNCBlobKeys& left, const SNCBlobKeys& right) const
    {
        if (left.key.size() != right.key.size())
            return left.key.size() < right.key.size();
        if (left.subkey.size() != right.subkey.size())
            return left.subkey.size() < right.subkey.size();
        int ret = left.key.compare(right.key);
        if (ret != 0)
            return ret < 0;
        ret = left.subkey.compare(right.subkey);
        if (ret != 0)
            return ret < 0;
        return left.version < right.version;
    }
};

/// Specialization of traits template to use with pure NetCache-related blob
/// storage.
template <>
struct SNCStorageKeyTraits<string>
{
    /// Check if two objects are from the same "family" - for strings it's a
    /// simple equality comparison.
    static bool IsFamilyEqual(const string& left, const string& right) {
        return left == right;
    }
    /// Check if left object is less than right.
    /// Method is necessary to use traits class as comparator in STL container
    bool operator() (const string& left, const string& right) const {
        if (left.size() != right.size())
            return left.size() < right.size();
        return left < right;
    }
};



template <class TFile>
inline
CNCDBFileLock<TFile>::CNCDBFileLock(CNCBlobStorage* storage,
                                    TNCDBPartId     part_id,
                                    TNCDBVolumeId   vol_id)
    : m_Storage(storage),
      m_DBPartId(part_id),
      m_DBVolId(vol_id)
{
    storage->GetFile(part_id, vol_id, &m_File);
}

template <class TFile>
inline
CNCDBFileLock<TFile>::~CNCDBFileLock(void)
{
    m_Storage->ReturnFile(m_DBPartId, m_DBVolId, m_File);
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
           ||  (m_Monitor  &&  m_Monitor->IsMonitorActive());
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

inline TNCDBVolumeId
CNCBlobStorage::x_GetMinVolumeId(const SNCDBPartInfo& part_info)
{
    return part_info.cnt_volumes == 0? 0: 1;
}

inline void
CNCBlobStorage::GetNextBlobCoords(SNCBlobCoords* coords)
{
    m_LastBlobLock.Lock();
    if (++m_LastBlob.blob_id <= 0)
        m_LastBlob.blob_id = 1;
    if (m_LastBlob.volume_id == m_LastPart->cnt_volumes)
        m_LastBlob.volume_id = x_GetMinVolumeId(*m_LastPart);
    else
        ++m_LastBlob.volume_id;
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
CNCBlobStorage::GetFile(TNCDBPartId   part_id,
                        TNCDBVolumeId vol_id,
                        TFile**       file_ptr)
{
    CNCDBFilesPool* pool = NULL;
    {{
        CFastReadGuard guard(m_DBFilesMutex);
        pool = m_DBFiles[part_id][vol_id].get();
    }}
    _ASSERT(pool);
    pool->GetFile(file_ptr);
}

template <class TFile>
inline void
CNCBlobStorage::ReturnFile(TNCDBPartId   part_id,
                           TNCDBVolumeId vol_id,
                           TFile*        file)
{
    CNCDBFilesPool* pool;
    {{
        CFastReadGuard guard(m_DBFilesMutex);
        pool = m_DBFiles[part_id][vol_id].get();
    }}
    _ASSERT(pool);
    pool->ReturnFile(file);
}

inline bool
CNCBlobStorage::ReadBlobInfo(SNCBlobInfo* info)
{
    TMetaFileLock metafile(this, info->part_id, info->volume_id);
    return metafile->ReadBlobInfo(info);
}

inline void
CNCBlobStorage::WriteBlobInfo(SNCBlobInfo& info)
{
    TMetaFileLock metafile(this, info.part_id, info.volume_id);
    metafile->WriteBlobInfo(info, IsChangeTimeOnRead());
}

inline void
CNCBlobStorage::GetChunkIds(const SNCBlobCoords& coords, TNCChunksList* id_list)
{
    TMetaFileLock metafile(this, coords.part_id, coords.volume_id);
    metafile->GetChunkIds(coords.blob_id, id_list);
}

inline void
CNCBlobStorage::WriteChunkValue(const SNCBlobCoords& coords,
                                TNCChunkId           chunk_id,
                                const TNCBlobBuffer& data)
{
    TDataFileLock datafile(this, coords.part_id, coords.volume_id);
    datafile->WriteChunkValue(chunk_id, data);
}

inline TNCChunkId
CNCBlobStorage::CreateNewChunk(const SNCBlobCoords& coords,
                               const TNCBlobBuffer& data)
{
    TNCChunkId chunk_id;
    {{
        TDataFileLock datafile(this, coords.part_id, coords.volume_id);
        chunk_id = datafile->CreateChunkValue(data);
    }}
    {{
        TMetaFileLock metafile(this, coords.part_id, coords.volume_id);
        metafile->CreateChunk(coords.blob_id, chunk_id);
    }}
    return chunk_id;
}

inline bool
CNCBlobStorage::ReadChunkValue(const SNCBlobCoords& coords,
                               TNCChunkId           chunk_id,
                               TNCBlobBuffer*       buffer)
{
    TDataFileLock datafile(this, coords.part_id, coords.volume_id);
    return datafile->ReadChunkValue(chunk_id, buffer);
}

inline void
CNCBlobStorage::DeleteLastChunks(const SNCBlobCoords& coords,
                                 TNCChunkId           min_chunk_id)
{
    TMetaFileLock metafile(this, coords.part_id, coords.volume_id);
    metafile->DeleteLastChunks(coords.blob_id, min_chunk_id);
}

inline CNCBlobLockHolder*
CNCBlobStorage::x_GetLockHolder(bool* need_initialize)
{
    CSpinGuard guard(m_HoldersPoolLock);

    CNCBlobLockHolder* holder = m_FreeHolders;
    if (holder) {
        holder->RemoveFromList(m_FreeHolders);
    }
    else {
        holder = new CNCBlobLockHolder(this);
    }
    holder->AddToList(m_UsedHolders);
    ++m_CntUsedHolders;

    *need_initialize = !m_Blocked;
    return holder;
}

inline void
CNCBlobStorage::ReturnLockHolder(CNCBlobLockHolder* holder)
{
    CSpinGuard guard(m_HoldersPoolLock);

    holder->RemoveFromList(m_UsedHolders);
    holder->AddToList(m_FreeHolders);
    --m_CntUsedHolders;
    if (m_Blocked  &&  holder->IsLockInitialized())
        --m_CntLocksToWait;
}

inline TNCBlobLockHolderRef
CNCBlobStorage::GetBlobAccess(ENCBlobAccess access,
                              const string& key,
                              const string& subkey,
                              int           version,
                              const string& password)
{
    x_CheckReadOnly(access);

    bool need_initialize;
    TNCBlobLockHolderRef holder(x_GetLockHolder(&need_initialize));
    holder->PrepareLock(key, subkey, version, password, access);
    if (need_initialize)
        holder->InitializeLock();
    return holder;
}

inline TNCBlobLockHolderRef
CNCBlobStorage::x_GetBlobAccess(ENCBlobAccess          access,
                                const SNCBlobIdentity& identity)
{
    x_CheckReadOnly(access);

    bool need_initialize;
    TNCBlobLockHolderRef holder(x_GetLockHolder(&need_initialize));
    holder->PrepareLock(identity, access);
    if (need_initialize)
        holder->InitializeLock();
    return holder;
}

inline bool
CNCBlobStorage::IsBlocked(void)
{
    return m_Blocked;
}

inline bool
CNCBlobStorage::CanDoExclusive(void)
{
    _ASSERT(IsBlocked());
    return m_CntLocksToWait == 0;
}

inline
CNCBlobStorage::CNCBlobStorage(void)
    : m_Monitor(NULL),
      m_Flags(0),
      m_Stopped(false),
      m_StopTrigger(0, 100),
      m_AllDBPartsCached(false),
      m_CurDBSize(0),
      m_FreeHolders(NULL),
      m_UsedHolders(NULL),
      m_CntUsedHolders(0),
      m_BlobsPool(TNCBlobsFactory(this)),
      m_Blocked(false),
      m_CntLocksToWait(0)
{}


template <class TFilesPool, class TCacheKey>
inline
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::CNCBlobStorage_Specific(void)
{}

template <class TFilesPool, class TCacheKey>
inline
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::~CNCBlobStorage_Specific(void)
{
    Deinitialize();
    m_KeysCache.Clear();
}

template <class TFilesPool, class TCacheKey>
inline CNCDBFilesPool*
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::CreateFilesPool(const string& meta_name,
                                  const string& data_name)
{
    return new TFilesPool(meta_name, data_name, GetStat());
}

template <class TFilesPool, class TCacheKey>
inline bool
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::ReadBlobCoordsFromCache(SNCBlobIdentity* identity)
{
    TKeyConstRef key_ref(*identity);
    return m_KeysCache.Get(key_ref, identity);
}

template <class TFilesPool, class TCacheKey>
inline bool
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::CreateBlobInCache(const SNCBlobIdentity& identity,
                                    SNCBlobCoords*         new_coords)
{
    TKeyConstRef key_ref(identity);
    if (m_KeysCache.Insert(key_ref, identity, new_coords)) {
        return true;
    }
    return false;
}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::MoveBlobInCache(const SNCBlobKeys&   keys,
                                  const SNCBlobCoords& new_coords)
{
    TKeyConstRef key_ref(keys);
    _VERIFY(m_KeysCache.Change(key_ref, new_coords));
}

template <class TFilesPool, class TCacheKey>
inline CNCBlobStorage::EExistsOrMoved
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::IsBlobExistsInCache(const SNCBlobIdentity& identity,
                                      SNCBlobCoords*         new_coords)
{
    TKeyConstRef key_ref(identity);
    if (m_KeysCache.Get(key_ref, new_coords)) {
        // For now we don't allow to use the same blob_id when blob is moved
        // (see _ASSERT above).
        if (new_coords->blob_id == identity.blob_id)
            return eBlobExists;
        return eBlobMoved;
    }
    return eBlobNotExists;
}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::DeleteBlobFromCache(const SNCBlobKeys& keys)
{
    TKeyConstRef key_ref(keys);
    if (!m_KeysCache.Erase(key_ref)) {
        // I don't know yet why this can happen in corrupted database.
        ERR_POST("For some reason blob '" << keys.key << "', '" << keys.subkey
                 << "', " << keys.version << " doesn't exist in cache.");
    }

}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::CleanCache(void)
{
    m_KeysCache.Clear();
}

template <class TFilesPool, class TCacheKey>
inline bool
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::IsBlobFamilyExistsInCache(const SNCBlobKeys& keys)
{
    TKeyConstRef key_ref(keys);
    TCacheKey stored_key;
    return m_KeysCache.GetLowerBound(key_ref, stored_key)
           &&  TKeyTraits::IsFamilyEqual(stored_key, key_ref);
}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::HeartBeat(void)
{
    m_KeysCache.HeartBeat();
}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::PrintCacheCounts(CPrintTextProxy& proxy)
{
    proxy << "Now in cache    - "
                    << m_KeysCache.CountValues() << " blobs, "
                    << m_KeysCache.CountNodes()  << " nodes, "
                    << m_KeysCache.TreeHeight()  << " height" << endl;
}

template <class TFilesPool, class TCacheKey>
inline void
CNCBlobStorage_Specific<TFilesPool, TCacheKey>
                ::CollectCacheStats(void)
{
    GetStat()->AddCountBlobs(m_KeysCache.CountValues());
    GetStat()->AddCountCacheNodes(m_KeysCache.CountNodes());
    GetStat()->AddCacheTreeHeight(m_KeysCache.TreeHeight());
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE__HPP */
