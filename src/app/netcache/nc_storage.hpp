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


struct SFileRecHeader;
struct SFileMetaRec;
struct SNCCacheData;
struct SCacheDeadCompare;
struct SCacheKeyCompare;

struct STimeTable_tag;
struct SKeyMap_tag;

typedef intr::set_base_hook< intr::tag<STimeTable_tag>,
                             intr::optimize_size<true> >    TTimeTableHook;
typedef intr::set_base_hook< intr::tag<SKeyMap_tag>,
                             intr::optimize_size<true> >    TKeyMapHook;
typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TTimeTableHook>,
                     intr::constant_time_size<false>,
                     intr::compare<SCacheDeadCompare> >     TTimeTableMap;
typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TKeyMapHook>,
                     intr::constant_time_size<true>,
                     intr::compare<SCacheKeyCompare> >      TKeyMap;


enum EFileRecType {
    eFileRecNone = 0,
    eFileRecMeta,
    eFileRecChunkMap,
    eFileRecChunkData,
    eFileRecAny
};

struct ATTR_PACKED SFileIndexRec
{
    Uint4   prev_num;
    Uint4   next_num;
    Uint4   offset;
    Uint4   rec_size:24;
    Uint1   rec_type:8;
    SNCDataCoord chain_coord;
    SNCCacheData* cache_data;
};

struct ATTR_PACKED SFileMetaRec
{
    Uint1   has_password;
    Uint1   align_reserve;
    Uint2   map_size;
    Uint4   chunk_size;
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
    char    key_data[1];
};

struct ATTR_PACKED SFileChunkMapRec
{
    Uint2 map_idx;
    Uint1 map_depth;
    SNCDataCoord ATTR_ALIGNED_8 down_coords[1];
};

struct ATTR_PACKED SFileChunkDataRec
{
    Uint8   chunk_num;
    Uint2   chunk_idx;
    Uint1   chunk_data[1];
};


struct SNCCacheData : public TTimeTableHook,
                      public TKeyMapHook,
                      public SNCBlobSummary
{
    SNCDataCoord coord;
    string key;
    Uint2 time_bucket;
    Uint2 map_size;
    Uint4 chunk_size;
    int   key_del_time;
    CMiniMutex lock;
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


struct SWritingInfo
{
    CNCRef<SNCDBFileInfo> cur_file;
    CNCRef<SNCDBFileInfo> next_file;
    Uint4 next_rec_num;
    Uint4 next_offset;
    Uint4 left_file_size;
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
    bool AcceptWritesFromPeers(void);

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
                                   Uint2         time_bucket);
    /// Check if blob with given key and (optionally) subkey exists
    /// in database. More than one blob with given key/subkey can exist.
    bool IsBlobExists(Uint2 time_bucket, const string& key);
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
                       SNCChunkMapInfo** maps,
                       Uint8 cnt_chunks);
    /// Delete blob from database.
    void DeleteBlobInfo(const SNCBlobVerData* ver_data,
                        SNCChunkMapInfo** maps);

    bool ReadChunkData(SNCBlobVerData* ver_data,
                       SNCChunkMapInfo** maps,
                       Uint8 chunk_num,
                       CNCBlobBuffer* buffer);
    bool WriteChunkData(SNCBlobVerData* ver_data,
                        SNCChunkMapInfo** maps,
                        Uint8 chunk_num,
                        const CNCBlobBuffer* data);

    void DeleteBlobKey(Uint2 time_bucket, const string& key);
    void RestoreBlobKey(Uint2 time_bucket, const string& key, SNCCacheData* cache_data);
    void ChangeCacheDeadTime(SNCCacheData* cache_data,
                             SNCDataCoord coord,
                             int new_dead_time);

    /// Return blob lock holder to the pool
    void ReturnAccessor(CNCBlobAccessor* holder);

private:
    CNCBlobStorage(const CNCBlobStorage&);
    CNCBlobStorage& operator= (const CNCBlobStorage&);


    ///
    class CKeysCleaner
    {
    public:
        CKeysCleaner(Uint2 bucket);

        void Delete(const string& key) const;

    private:
        Uint2 m_Bucket;
    };

    friend class CKeysCleaner;

    typedef CNCDeferredDeleter<string, CKeysCleaner, 5, 10000>  TKeysDeleter;
    struct SBucketCache
    {
        CMiniMutex   lock;
        TKeyMap      key_map;
        TKeysDeleter deleter;

        SBucketCache(Uint2 bucket);
    };
    typedef map<Uint2, SBucketCache*> TBucketCacheMap;

    struct STimeTable
    {
        CMiniMutex lock;
        TTimeTableMap time_map;
    };
    typedef map<Uint2, STimeTable*> TTimeBuckets;


    typedef set<Uint4>              TRecNumsSet;
    typedef map<Uint4, TRecNumsSet> TFileRecsMap;


    /// Implementation of background thread. Mainly garbage collector plus
    /// caching of database data at the beginning of the storage work.
    void x_DoBackgroundWork(void);
    void x_DoGCWork(void);
    void x_DoFlushWork(void);

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
    string x_GetFileName(Uint4 file_id, ENCDBFileType file_type);
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
    CNCBlobAccessor* x_GetAccessor(void);
    SBucketCache* x_GetBucketCache(Uint2 bucket);
    SNCCacheData* x_GetKeyCacheData(Uint2 time_bucket,
                                    const string& key,
                                    bool need_create);
    void x_InitializeAccessor(CNCBlobAccessor* accessor);

    /// Do set of procedures creating and initializing new database part and
    /// switching storage to using new database part as current one.
    bool x_CreateNewFile(size_t type_idx);
    void x_DeleteDBFile(const CNCRef<SNCDBFileInfo>& file_info);
    bool x_SaveOneMapImpl(SNCChunkMapInfo* save_map,
                          SNCChunkMapInfo* up_map,
                          Uint2 cnt_downs,
                          Uint2 map_size,
                          Uint1 map_depth,
                          SNCCacheData* cache_data);
    bool x_SaveChunkMap(SNCBlobVerData* ver_data,
                        SNCChunkMapInfo** maps,
                        Uint2 cnt_downs,
                        bool save_all_deps);
    virtual void OnBlockedOpFinish(void);
    bool x_IsIndexDeleted(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    SFileIndexRec* x_GetIndOrDeleted(SNCDBFileInfo* file_info, Uint4 rec_num);
    SFileIndexRec* x_GetIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num);
    void x_SwitchToNextFile(SWritingInfo& w_info);
    bool x_GetNextWriteCoord(EDBFileIndex file_index,
                             Uint4 rec_size,
                             SNCDataCoord& coord,
                             CNCRef<SNCDBFileInfo>& file_info,
                             SFileIndexRec*& ind_rec);
    Uint1 x_CalcMapDepthImpl(Uint8 size, Uint4 chunk_size, Uint2 map_size);
    Uint1 x_CalcMapDepth(Uint8 size, Uint4 chunk_size, Uint2 map_size);
    Uint2 x_CalcCntMapDowns(Uint4 rec_size);
    size_t x_CalcChunkDataSize(Uint4 rec_size);
    Uint4 x_CalcMetaRecSize(Uint2 key_size);
    Uint4 x_CalcMapRecSize(Uint2 cnt_downs);
    Uint4 x_CalcChunkRecSize(size_t data_size);
    char* x_CalcRecordAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    SFileMetaRec* x_CalcMetaAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    SFileChunkMapRec* x_CalcMapAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    SFileChunkDataRec* x_CalcChunkAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    CNCRef<SNCDBFileInfo> x_GetDBFile(Uint4 file_id);
    void x_DeleteIndexRec(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    void x_DeleteIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num);
    void x_MoveRecToGarbage(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    void x_UpdateUpCoords(SFileChunkMapRec* map_rec,
                          SFileIndexRec* map_ind,
                          SNCDataCoord coord);
    void x_MoveDataToGarbage(SNCDataCoord coord, Uint1 map_depth);
    bool x_ReadMapInfo(SNCDataCoord map_coord,
                       SNCChunkMapInfo* map_info,
                       Uint1 map_depth);
    void x_CacheDeleteIndexes(SNCDataCoord map_coord, Uint1 map_depth);
    bool x_CacheMapRecs(SNCDataCoord map_coord,
                        Uint1 map_depth,
                        SNCDataCoord up_coord,
                        Uint2 up_index,
                        SNCCacheData* cache_data,
                        Uint8 cnt_chunks,
                        Uint8& chunk_num,
                        map<Uint4, Uint4>& sizes_map,
                        TFileRecsMap& recs_map);
    bool x_CacheMetaRec(SNCDBFileInfo* file_info,
                        SFileIndexRec* ind_rec,
                        SNCDataCoord coord,
                        int cur_time,
                        TFileRecsMap& recs_map);
    bool x_CreateInitialFiles(set<Uint4>& new_file_ids);
    void x_PreCacheFileRecNums(SNCDBFileInfo* file_info, TRecNumsSet& recs_set);
    void x_CacheDatabase(void);
    void x_CollectStorageStats(void);
    void x_HeartBeat(void);
    void x_GC_DeleteExpired(SNCCacheData* cache_data, int dead_before);
    void x_RunGC(void);
    void x_ReadRecDeadFromUnfinished(Uint8 data_coord, int& dead_time);
    bool x_MoveRecord(SNCDBFileInfo* rec_file,
                      SFileIndexRec* rec_ind,
                      int cur_time,
                      bool& move_done);
    bool x_ShrinkDiskStorage(void);
    void x_SaveLogRecNo(void);
    void x_FlushStorage(void);
    void x_DoNewFileWork(void);


    enum EStopCause {
        eNoStop,
        eStopWarning,
        eStopDBSize,
        eStopDiskSpace,
        eStopDiskCritical
    };


    /// Directory for all database files of the storage
    string             m_Path;
    /// Name of the storage
    string             m_Prefix;
    /// Number of blobs treated by GC and by caching mechanism in one batch
    int                m_GCBatchSize;
    int                m_FlushTimePeriod;
    Uint1              m_MaxGarbagePct;
    int                m_MinMoveLife;
    int                m_FailedMoveDelay;
    int                m_MaxIOWaitTime;
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
    CFastMutex         m_StopLock;
    CConditionVariable m_StopCond;
    /// Background thread running GC and caching
    CNCRef<CThread>    m_BGThread;
    CNCRef<CThread>    m_GCThread;
    CNCRef<CThread>    m_NewFileThread;
    CNCRef<CThread>    m_FlushThread;

    CFastMutex               m_IndexLock;
    /// Index database file
    AutoPtr<CNCDBIndexFile>  m_IndexDB;
    /// Read-write lock to work with m_DBParts
    CFastMutex               m_DBFilesLock;
    /// List of all database parts in the storage
    TNCDBFilesMap            m_DBFiles;
    Uint4                    m_LastFileId;

    CFastMutex               m_NextWriteLock;
    SWritingInfo             m_AllWritings[s_CntAllFiles];
    Uint4                    m_NewFileSize;
    Uint2                    m_NextWaiters;
    CConditionVariable       m_NextWait;
    CConditionVariable       m_NextFileSwitch;
    TTimeBuckets             m_TimeTables;
    Uint8                    m_CurBlobsCnt;
    /// Current size of storage database. Kept here for printing statistics.
    Uint8                    m_CurDBSize;
    CMiniMutex               m_GarbageLock;
    Uint8                    m_GarbageSize;
    Uint8                    m_CntMoveTasks;
    Uint8                    m_CntMoveFiles;
    Uint8                    m_CntFailedMoves;
    Uint8                    m_MovedSize;
    int                      m_LastFlushTime;
    /// Internal cache of blobs identification information sorted to be able
    /// to search by key, subkey and version.
    TBucketCacheMap            m_BucketsCache;
    /// Mutex to control work with pool of lock holders
    CMiniMutex               m_HoldersPoolLock;
    /// List of holders available for acquiring new blob locks
    CNCBlobAccessor*         m_FreeAccessors;
    /// Number of lock holders in use
    unsigned int             m_CntUsedHolders;
    CFastMutex               m_GCBlockLock;
    CConditionVariable       m_GCBlockWaiter;
    CNCBlobAccessor*         m_GCAccessor;
    vector<SNCCacheData*>    m_GCDatas;
    EStopCause               m_IsStopWrite;
    bool                     m_CleanStart;
    bool                     m_NeedSaveLogRecNo;
    bool                     m_DoExtraGC;
    Uint1                    m_WarnLimitOnPct;
    Uint1                    m_WarnLimitOffPct;
    int                      m_MinRecNoSavePeriod;
    int                      m_LastRecNoSaveTime;
    Uint4                    m_ExtraGCTime;
    CAtomicCounter           m_BlobCounter;
    Uint8                    m_ExtraGCOnSize;
    Uint8                    m_ExtraGCOffSize;
    Uint8                    m_StopWriteOnSize;
    Uint8                    m_StopWriteOffSize;
    Uint8                    m_DiskFreeLimit;
    Uint8                    m_DiskCritical;
};


extern CNCBlobStorage* g_NCStorage;



//////////////////////////////////////////////////////////////////////////
//  Inline functions
//////////////////////////////////////////////////////////////////////////

inline
SNCCacheData::SNCCacheData(void)
    : key_del_time(0),
      ver_mgr(NULL)
{
    coord.file_id = 0;
    coord.rec_num = 0;
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
    CFastMutexGuard guard(m_DBFilesLock);
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
    return m_IsStopWrite != eNoStop  &&  m_IsStopWrite != eStopWarning;
}

inline bool
CNCBlobStorage::AcceptWritesFromPeers(void)
{
    return m_IsStopWrite != eStopDiskCritical;
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
