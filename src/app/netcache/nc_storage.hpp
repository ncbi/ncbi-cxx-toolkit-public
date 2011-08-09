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

#include "nc_utils.hpp"
#include "nc_db_info.hpp"
#include "nc_db_files.hpp"
#include "nc_storage_blob.hpp"
#include "nc_stat.hpp"


#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/rbtree.hpp>


namespace intr = boost::intrusive;



BEGIN_NCBI_SCOPE


struct SLRUList_tag;
struct SCoordMap_tag;
struct SWriteQueue_tag;
struct STimeTable_tag;
struct SKeyMap_tag;

typedef intr::list_base_hook< intr::tag<SLRUList_tag> >     TLRUHook;
typedef intr::set_base_hook< intr::tag<SCoordMap_tag>,
                             intr::optimize_size<true> >    TCoordMapHook;
typedef intr::slist_base_hook< intr::tag<SWriteQueue_tag> > TWriteQueueHook;
typedef intr::set_base_hook< intr::tag<STimeTable_tag>,
                             intr::optimize_size<true> >    TTimeTableHook;
typedef intr::set_base_hook< intr::tag<SKeyMap_tag>,
                             intr::optimize_size<true> >    TKeyMapHook;

struct SCacheDataRec;
struct SCacheRecCompare;
struct SWriteTask;
struct SFileRecHeader;
struct SFileMetaRec;
struct SNCCacheData;
struct SCacheDeadCompare;
struct SCacheKeyCompare;

typedef intr::list  <SCacheDataRec,
                     intr::base_hook<TLRUHook>,
                     intr::constant_time_size<false> >  TDataRecLRU;
typedef intr::rbtree<SCacheDataRec,
                     intr::base_hook<TCoordMapHook>,
                     intr::constant_time_size<false>,
                     intr::compare<SCacheRecCompare> >  TDataRecMap;
typedef intr::slist <SWriteTask,
                     intr::base_hook<TWriteQueueHook>,
                     intr::constant_time_size<false>,
                     intr::linear<true>,
                     intr::cache_last<true> >           TWriteQueue;
typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TTimeTableHook>,
                     intr::constant_time_size<false>,
                     intr::compare<SCacheDeadCompare> > TTimeTableMap;
typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TKeyMapHook>,
                     intr::constant_time_size<true>,
                     intr::compare<SCacheKeyCompare> >  TKeyMap;


enum EFileRecType {
    eFileRecNone = 0,
    eFileRecMeta,
    eFileRecChunkMap,
    eFileRecChunkData,
    eFileRecAny
};

struct SFileRecHeader
{
    Uint8   rec_num;
    Uint2   rec_size;
    Uint1   rec_type;
};

struct SFileMetaRec : public SFileRecHeader
{
    Uint1   deleted;
    Uint2   slot;
    Uint2   key_size;
    Uint8   map_coord;
    Uint8   data_coord;
    Uint8   size;
    Uint8   create_time;
    Uint8   create_server;
    Uint4   create_id;
    Int4    dead_time;
    Int4    ttl;
    Int4    expire;
    Int4    blob_ver;
    Int4    ver_ttl;
    Int4    ver_expire;
    Uint1   has_password;
    char    key_data[1];
};

struct SFileChunkMapRec : public SFileRecHeader
{
    Uint4   map_num;
    Uint8   prev_coord;
    Uint8   next_coord;
    Uint8   meta_coord;
    Uint8   chunk_coords[1];
};

struct SFileChunkDataRec : public SFileRecHeader
{
    Uint8   chunk_num;
    Uint8   prev_coord;
    Uint8   next_coord;
    Uint8   map_coord;
    Uint1   chunk_data[1];
};

struct SCacheDataRec : public TLRUHook, public TCoordMapHook
{
    Uint2 lock_lru_refs;
    Uint8 coord;
    SFileRecHeader* data;
};

struct SCacheRecCompare
{
    bool operator() (const SCacheDataRec& x, const SCacheDataRec& y) const
    {
        return x.coord < y.coord;
    }
    bool operator() (Uint8 coord, const SCacheDataRec& y) const
    {
        return coord < y.coord;
    }
    bool operator() (const SCacheDataRec& x, Uint8 coord) const
    {
        return x.coord < coord;
    }
};

enum EWriteTask {
    eWriteRecord,
    eDelMetaInfo,
    eDelBlob,
    eMoveRecord
};

struct SWriteTask : public TWriteQueueHook
{
    EWriteTask  task_type;
    Uint8 coord;
    SCacheDataRec* cache_data;
};


struct SNCCacheData : public TTimeTableHook,
                      public TKeyMapHook,
                      public SNCBlobSummary
{
    Uint8 coord;
    bool  key_deleted;
    Uint2 slot;
    int   key_del_time;
    string key;
    CSpinLock lock;
    CNCBlobVerManager* ver_mgr;


    SNCCacheData(void);

private:
    SNCCacheData(const SNCCacheData&);
    SNCCacheData& operator= (const SNCCacheData&);
};

struct SCacheDeadCompare
{
    bool operator() (const SNCCacheData& x, const SNCCacheData& y) const
    {
        if (x.dead_time != y.dead_time)
            return x.dead_time < y.dead_time;
        else
            return &x < &y;
    }
    bool operator() (int dead_time, const SNCCacheData& y) const
    {
        return dead_time < y.dead_time;
    }
    bool operator() (const SNCCacheData& x, int dead_time) const
    {
        return x.dead_time < dead_time;
    }
};

struct SCacheKeyCompare
{
    bool operator() (const SNCCacheData& x, const SNCCacheData& y) const
    {
        return x.key < y.key;
    }
    bool operator() (const string& key, const SNCCacheData& y) const
    {
        return key < y.key;
    }
    bool operator() (const SNCCacheData& x, const string& key) const
    {
        return x.key < key;
    }
};



/// Blob storage for NetCache
class CNCBlobStorage : public INCBlockedOpListener
{
public:
    CNCBlobStorage(void);
    virtual ~CNCBlobStorage(void);

    bool Initialize(bool do_reinit);
    void Finalize(void);

    bool IsCleanStart(void);
    bool NeedStopWrite(void);

    /// Print usage statistics for the storage
    void PrintStat(CPrintTextProxy& proxy);

    void PackBlobKey(string*      packed_key,
                     CTempString  cache_name,
                     CTempString  blob_key,
                     CTempString  blob_subkey);
    void UnpackBlobKey(const string& packed_key,
                       string&       cache_name,
                       string&       key,
                       string&       subkey);
    string UnpackKeyForLogs(const string& packed_key);
    string PrintablePassword(const string& pass);
    /// Acquire access to the blob identified by key, subkey and version
    CNCBlobAccessor* GetBlobAccess(ENCAccessType access,
                                   const string& key,
                                   const string& password,
                                   Uint2         slot);
    /// Check if blob with given key and (optionally) subkey exists
    /// in database. More than one blob with given key/subkey can exist.
    bool IsBlobExists(Uint2 slot, const string& key);
    Uint4 GetNewBlobId(void);

    /// Get number of files in the database
    int GetNDBFiles(void);
    /// Get storage's path
    const string& GetPath(void);
    /// Get total size of database for the storage
    Uint8 GetDBSize(void);
    void GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst);
    Uint8 GetMaxSyncLogRecNo(void);
    void SaveMaxSyncLogRecNo(void);

public:
    // For internal use only

    /// Get meta information about blob with given coordinates.
    ///
    /// @return
    ///   TRUE if meta information exists in the database and was successfully
    ///   read. FALSE if database is corrupted.
    bool ReadBlobInfo(SNCBlobVerData* ver_data);
    /// Write meta information about blob into the database.
    bool WriteBlobInfo(const string& blob_key,
                       SNCBlobVerData* ver_data,
                       SNCChunkMapInfo* chunk_map);
    /// Delete blob from database.
    void DeleteBlobInfo(const SNCBlobVerData* ver_data);

    bool ReadChunkData(SNCBlobVerData* ver_data,
                       SNCChunkMapInfo* chunk_map,
                       Uint8 chunk_num,
                       CNCBlobBuffer* buffer);
    bool WriteChunkData(SNCBlobVerData* ver_data,
                        SNCChunkMapInfo* chunk_map,
                        Uint8 chunk_num,
                        const CNCBlobBuffer* data);

    void DeleteBlobKey(Uint2 slot, const string& key);
    void RestoreBlobKey(Uint2 slot, const string& key, SNCCacheData* cache_data);
    void ChangeCacheDeadTime(SNCCacheData* cache_data, Uint8 coord, int new_dead_time);

    /// Return blob lock holder to the pool
    void ReturnAccessor(CNCBlobAccessor* holder);

private:
    CNCBlobStorage(const CNCBlobStorage&);
    CNCBlobStorage& operator= (const CNCBlobStorage&);


    ///
    class CKeysCleaner
    {
    public:
        CKeysCleaner(Uint2 slot);

        void Delete(const string& key) const;

    private:
        Uint2 m_Slot;
    };

    friend class CKeysCleaner;

    typedef CNCDeferredDeleter<string, CKeysCleaner, 5, 10000>  TKeysDeleter;
    struct SSlotCache
    {
        CFastRWLock  lock;
        TKeyMap      key_map;
        TKeysDeleter deleter;

        SSlotCache(Uint2 slot);
    };
    typedef map<Uint2, SSlotCache*> TSlotCacheMap;

    typedef map<Uint8, Uint8>       TUnfinishedMap;


    /// Implementation of background thread. Mainly garbage collector plus
    /// caching of database data at the beginning of the storage work.
    void x_DoBackgroundWork(void);

    /// Read all storage parameters from registry
    bool x_ReadStorageParams(void);
    /// Read from registry only those parameters that can be changed on the
    /// fly, without re-starting the application.
    bool x_ReadVariableParams(void);
    ///
    bool x_EnsureDirExist(const string& dir_name);
    /// Make name of the index file in the storage
    string x_GetIndexFileName(void);
    /// Make name of file with meta-information in given database part
    string x_GetFileName(Uint4 file_id);
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
    bool x_OpenIndexDB(void);
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
    bool x_ReinitializeStorage(void);
    /// Reinitialize database cleaning all data from it.
    /// Only database is cleaned, internal cache is left intact.
    void x_CleanDatabase(void);

    /// Get lock holder available for new blob lock.
    /// need_initialize will be set to TRUE if initialization of holder can be
    /// made (storage is not blocked) and FALSE if initialization of holder
    /// will be executed later on storage unblocking.
    CNCBlobAccessor* x_GetAccessor(bool* need_initialize);
    SSlotCache* x_GetSlotCache(Uint2 slot);
    SNCCacheData* x_GetKeyCacheData(Uint2 slot,
                                    const string& key,
                                    bool need_create);
    void x_InitializeAccessor(CNCBlobAccessor* accessor);

    bool x_FlushCurFile(bool force_flush);
    /// Do set of procedures creating and initializing new database part and
    /// switching storage to using new database part as current one.
    bool x_CreateNewFile(void);
    void x_DeleteDBFile(TNCDBFilesMap::iterator files_it);
    void x_RemoveDataFromLRU(SCacheDataRec* cache_data);
    void x_PutDataToLRU(SCacheDataRec* cache_data);
    SCacheDataRec* x_GetCacheData(Uint8 coord);
    SCacheDataRec* x_ReadCacheDataImpl(Uint8 coord);
    SCacheDataRec* x_ReadCacheData(Uint8 coord, EFileRecType rec_type);
    void x_DeleteCacheDataImpl(SCacheDataRec* cache_data);
    void x_DeleteCacheData(SCacheDataRec* cache_data);
    void x_ChangeCacheDataCoord(SCacheDataRec* cache_data, Uint8 new_coord);
    void x_AddWriteRecordTask(SCacheDataRec* cache_data, EWriteTask task_type);
    void x_AddWriteTask(EWriteTask task_type, Uint8 coord);
    bool x_SaveChunkMap(SNCBlobVerData* ver_data, SNCChunkMapInfo* chunk_map);
    SCacheDataRec* x_FindChunkMap(Uint8 first_map_coord, Uint4 map_num);
    virtual void OnBlockedOpFinish(void);
    char* x_MapFile(TFileHandle fd, size_t file_size);
    void x_UnmapFile(char* mem_ptr, size_t file_size);
    Uint8 x_CalcBlobDiskSize(Uint2 key_size, Uint8 blob_size);
    void x_AddGarbageSize(Int8 size);
    void x_CacheMetaRec(SFileRecHeader* header,
                        Uint4 file_id,
                        size_t offset,
                        int cur_time);
    void x_CacheDatabase(void);
    void x_CollectStorageStats(void);
    void x_HeartBeat(void);
    bool x_WriteToDB(Uint8 coord, void* buf, size_t size);
    bool x_AdjustPrevChunkData(SWriteTask* task, bool& move_done);
    bool x_AdjustMapChunks(SWriteTask* task, bool& move_done);
    bool x_AdjustBlobMaps(SWriteTask* task, bool& move_done);
    bool x_DoWriteRecord(SWriteTask* task);
    void x_EraseFromDataCache(SCacheDataRec* cache_data);
    bool x_DoDelBlob(SWriteTask* task);
    bool x_ExecuteWriteTasks(void);
    void x_GC_DeleteExpired(const string& key, Uint2 slot, int dead_before);
    void x_RunGC(void);
    void x_CleanDataCache(void);
    void x_ReadRecDeadFromUnfinished(Uint8 data_coord, int& dead_time);
    void x_ReadRecDeadFromMeta(SCacheDataRec* cache_data, int& dead_time);
    void x_ReadRecDeadFromMap(SCacheDataRec* cache_data, int& dead_time);
    void x_ReadRecDeadFromData(SCacheDataRec* cache_data, int& dead_time);
    bool x_ReadRecDeadTime(Uint8 coord, int& dead_time, Uint2& rec_size);
    void x_AdvanceFirstRecPtr(void);
    void x_ShrinkDiskStorage(void);


    /// Directory for all database files of the storage
    string             m_Path;
    /// Name of the storage
    string             m_Prefix;
    /// Number of blobs treated by GC and by caching mechanism in one batch
    int                m_GCBatchSize;
    Uint4              m_FlushSizePeriod;
    int                m_FlushTimePeriod;
    Uint1              m_MaxGarbagePct;
    int                m_MinMoveLife;
    Uint4              m_MaxMoveSize;
    Uint8              m_MinDBSize;
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

    CFastMutex               m_IndexLock;
    /// Index database file
    AutoPtr<CNCDBIndexFile>  m_IndexDB;
    /// Read-write lock to work with m_DBParts
    CSpinRWLock              m_DBFilesLock;
    /// List of all database parts in the storage
    TNCDBFilesMap            m_DBFiles;
    CSpinRWLock              m_CurFileLock;
    char*                    m_CurFileMem;
    Uint4                    m_CurFileId;
    TFileHandle              m_CurFileFD;
    Uint4                    m_CurFileOff;
    SNCDBFileInfo*           m_CurFileInfo;
    Uint8                    m_FirstRecCoord;
    int                      m_FirstRecDead;
    CSpinLock                m_WriteQueueLock;
    Uint8                    m_NextWriteCoord;
    TWriteQueue              m_WriteQueue;
    Uint4                    m_CntWriteTasks;
    Uint4                    m_NewFileSize;
    CSpinLock                m_DataCacheLock;
    TDataRecLRU              m_DataLRU;
    TDataRecMap              m_DataCache;
    size_t                   m_DataCacheSize;
    size_t                   m_CntDataCacheRecs;
    CSpinLock                m_TimeTableLock;
    TTimeTableMap            m_TimeTable;
    Uint8                    m_CurBlobsCnt;
    /// Current size of storage database. Kept here for printing statistics.
    Uint8                    m_CurDBSize;
    CSpinLock                m_GarbageLock;
    Uint8                    m_GarbageSize;
    Uint8                    m_TotalDataWritten;
    Uint8                    m_CntMoveTasks;
    Uint8                    m_CntFailedMoves;
    Uint8                    m_MovedSize;
    Uint8                    m_LastFlushTotal;
    int                      m_LastFlushTime;
    TUnfinishedMap           m_UnfinishedBlobs;
    /// Minimum expiration time of all blobs remembered now by the storage. 
    /// Variable is used with assumption that reads and writes for int are
    /// always atomic.
    CFastRWLock              m_BigCacheLock;
    /// Internal cache of blobs identification information sorted to be able
    /// to search by key, subkey and version.
    TSlotCacheMap            m_SlotsCache;
    /// Mutex to control work with pool of lock holders
    CSpinLock                m_HoldersPoolLock;
    /// List of holders available for acquiring new blob locks
    CNCBlobAccessor*         m_FreeAccessors;
    /// List of holders already acquired (or waiting to acquire) a blob lock
    CNCBlobAccessor*         m_UsedAccessors;
    /// Number of lock holders in use
    unsigned int             m_CntUsedHolders;
    CSemaphore               m_GCBlockWaiter;
    CNCBlobAccessor*         m_GCAccessor;
    vector<string>           m_GCKeys;
    vector<Uint2>            m_GCSlots;
    Uint8                    m_GCDeleted;
    bool                     m_CleanStart;
    bool                     m_IsStopWrite;
    bool                     m_NeedSaveLogRecNo;
    bool                     m_DoExtraGC;
    Uint4                    m_ExtraGCTime;
    CAtomicCounter           m_BlobCounter;
    Uint8                    m_ExtraGCOnSize;
    Uint8                    m_ExtraGCOffSize;
    Uint8                    m_StopWriteOnSize;
    Uint8                    m_StopWriteOffSize;
    Uint8                    m_DiskFreeLimit;
    Uint8                    m_NextRecNum;
};


extern CNCBlobStorage* g_NCStorage;



//////////////////////////////////////////////////////////////////////////
//  Inline functions
//////////////////////////////////////////////////////////////////////////

inline
SNCCacheData::SNCCacheData(void)
    : coord(0),
      key_deleted(false),
      ver_mgr(NULL)
{
    create_id = 0;
    create_time = create_server = 0;
    dead_time = ver_expire = 0;
}


inline const string&
CNCBlobStorage::GetPath(void)
{
    return m_Path;
}

inline Uint8
CNCBlobStorage::GetDBSize(void)
{
    return m_CurDBSize;
}

inline int
CNCBlobStorage::GetNDBFiles(void)
{
    CSpinReadGuard guard(m_DBFilesLock);
    return int(m_DBFiles.size());
}

inline bool
CNCBlobStorage::IsCleanStart(void)
{
    return m_CleanStart;
}

inline bool
CNCBlobStorage::NeedStopWrite(void)
{
    return m_IsStopWrite;
}

inline Uint4
CNCBlobStorage::GetNewBlobId(void)
{
    return Uint4(m_BlobCounter.Add(1));
}

END_NCBI_SCOPE

namespace boost
{

inline void
assertion_failed(char const*, char const*, char const*, long)
{
    abort();
}

}

#endif /* NETCACHE__NC_STORAGE__HPP */
