#ifndef BDB___BLOBCACHE__HPP
#define BDB___BLOBCACHE__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BerkeleyDB based local BLOB cache.
 *
 */

/// @file bdb_blobcache.hpp
/// ICache interface implementation on top of Berkeley DB

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbicntr.hpp>

#include <util/cache/icache.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_split_blob.hpp>

#include <deque>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB_Cache
 *
 * @{
 */

/// Register NCBI_BDB_ICacheEntryPoint
void BDB_Register_Cache(void);

typedef bm::bvector<> TNCBVector;

/// Split store for BLOBs
typedef CBDB_BlobSplitStore<TNCBVector, 
                            CBDB_BlobDeMux_RoundRobin, 
                            CFastMutex>                 TSplitStore;


/// BLOB storage table id->BLOB, id is supposed to be incremental,
/// Berkeley DB is reduculously faster when it writes records in sorted order.
/// 
/*
struct NCBI_BDB_CACHE_EXPORT SCacheBLOB_DB : public CBDB_BLobFile
{
    CBDB_FieldUint4  blob_id;

    SCacheBLOB_DB()
    {
        BindKey("blob_id",  &blob_id);
    }
};
*/


/// BLOB attributes DB
struct NCBI_BDB_CACHE_EXPORT SCache_AttrDB : public CBDB_File
{
    CBDB_FieldString       key;
    CBDB_FieldInt4         version;
    CBDB_FieldString       subkey;
    CBDB_FieldUint4        time_stamp;
    CBDB_FieldInt4         overflow;
    CBDB_FieldUint4        ttl;         ///< time-to-live
    CBDB_FieldUint4        max_time;    ///< max ttl limit for BLOB
    CBDB_FieldUint4        upd_count;   ///< update counter
    CBDB_FieldUint4        read_count;  ///< read counter
    CBDB_FieldUint4        blob_id;     ///< BLOB counter
    CBDB_FieldUint4        volume_id;   ///< demux coord[0]
    CBDB_FieldUint4        split_id;    ///< demux coord[1]


    CBDB_FieldString       owner_name;  ///< owner's name

    SCache_AttrDB()
    {
        DisableNull();

        BindKey("key",     &key, 256);
        BindKey("version", &version);
        BindKey("subkey",  &subkey, 256);

        BindData("time_stamp", &time_stamp);
        BindData("overflow",   &overflow);
        BindData("ttl",        &ttl);
        BindData("max_time",   &max_time);

        BindData("upd_count",  &upd_count);
        BindData("read_count", &read_count);
        BindData("blob_id",    &blob_id);
        BindData("volume_id",  &volume_id);
        BindData("split_id",   &split_id);

        BindData("owner_name", &owner_name, 512);
    }
};

class CPIDGuard;
class CCacheCleanerThread;


/// BDB cache access statistics
///
struct SBDB_TimeAccessStatistics
{
    unsigned   day;
    unsigned   hour;
    unsigned   put_count;
    unsigned   get_count;

    SBDB_TimeAccessStatistics(unsigned d,   unsigned hr, 
                              unsigned put, unsigned get)
    : day(d), hour(hr), put_count(put), get_count(get)
    {}
};


/// BDB cache statistics (covers one elemntary unit of measurements)
///
struct NCBI_BDB_CACHE_EXPORT SBDB_CacheUnitStatistics
{
    /// Blob size to number of blobs
    typedef map<unsigned, unsigned>          TBlobSizeHistogram;
    typedef deque<SBDB_TimeAccessStatistics> TTimeAccess;


    unsigned  blobs_stored_total;       ///< Total number of blobs
    unsigned  blobs_overflow_total;     ///< number of overflow blobs
    unsigned  blobs_updates_total;      ///< How many updates registered
    unsigned  blobs_never_read_total;   ///< BLOBs never read before
    unsigned  blobs_read_total;         ///< Number of reads
    unsigned  blobs_expl_deleted_total; ///< BLOBs explicitly removed
    unsigned  blobs_purge_deleted_total;///< BLOBs garbage collected
    double    blobs_size_total;         ///< Size of BLOBs total
    unsigned  blob_size_max_total;      ///< Largest BLOB ever

    unsigned  blobs_db;                 ///< Current database number of records
    double    blobs_size_db;            ///< Current size of all BLOBs

    unsigned  err_protocol;             ///< Protocol errors
    unsigned  err_communication;        ///< Communication error
    unsigned  err_internal;             ///< Internal errors of all sorts
    unsigned  err_no_blob;              ///< BLOB not found errors    
    unsigned  err_blob_get;             ///< retrive errors
    unsigned  err_blob_put;             ///< store errors
    unsigned  err_blob_over_quota;      ///< BLOB max quota

    TBlobSizeHistogram  blob_size_hist; ///< Blob size historgam
    TTimeAccess         time_access;    ///< Hourly access history

public:
    SBDB_CacheUnitStatistics();

    void Init();

    void AddStore(time_t tm,
                  unsigned store, 
                  unsigned update, 
                  unsigned blob_size, 
                  unsigned overflow);

    void AddRead(time_t tm);
    void AddExplDelete() { ++blobs_expl_deleted_total; }
    void AddPurgeDelete() { ++blobs_purge_deleted_total; }
    void AddNeverRead() { ++blobs_never_read_total; }

    /// Put/Get errors
    enum EErrGetPut {
        eErr_Unknown,   ///< no info on operation 
        eErr_Put,       ///< Put error
        eErr_Get        ///< Get error
    };

    void AddInternalError(EErrGetPut operation);
    void AddProtocolError(EErrGetPut operation);
    void AddNoBlobError(EErrGetPut operation);
    void AddCommError(EErrGetPut operation);
    void AddBlobQuotaError();


    static
    void AddToHistogram(TBlobSizeHistogram* hist, unsigned size);

    /// Convert statistics into registry sections and entries
    /// @param sect_name_postfix
    ///     Registry section name postfix 
    void ConvertToRegistry(IRWRegistry* reg, 
                           const string& sect_name_postfix) const;

private:
    void InitHistorgam(TBlobSizeHistogram* hist);
    void x_AddErrGetPut(EErrGetPut operation);
};


/// BDB cache statistics
///
///
class NCBI_BDB_CACHE_EXPORT SBDB_CacheStatistics
{
public:
    SBDB_CacheStatistics();

    /// Drop all collected statistics
    void Init();

    void AddStore(const string& client,
                  time_t        tm,
                  unsigned      store, 
                  unsigned      update, 
                  unsigned      blob_size, 
                  unsigned      overflow);

    void AddRead(const string&        client, time_t tm);
    void AddExplDelete(const string&  client);
    void AddPurgeDelete(const string& client);
    void AddNeverRead(const string&   client);


    void AddInternalError(const string& client,
                          SBDB_CacheUnitStatistics::EErrGetPut operation);
    void AddProtocolError(const string& client,
                          SBDB_CacheUnitStatistics::EErrGetPut operation);
    void AddNoBlobError(const string& client,
                        SBDB_CacheUnitStatistics::EErrGetPut operation);
    void AddCommError(const string& client,
                      SBDB_CacheUnitStatistics::EErrGetPut operation);
    void AddBlobQuotaError(const string& client);


    void ConvertToRegistry(IRWRegistry* reg) const;

    SBDB_CacheUnitStatistics& GlobalStatistics() { return m_GlobalStat; }

private:
    typedef map<string, SBDB_CacheUnitStatistics> TOwnerStatMap;
private:
    SBDB_CacheUnitStatistics  m_GlobalStat; 
    TOwnerStatMap             m_OwnerStatMap;
};



/// BDB cache implementation.
///
/// Class implements ICache interface using local Berkeley DB
/// database.
///
class NCBI_BDB_CACHE_EXPORT CBDB_Cache : public ICache
{
public:
    CBDB_Cache();
    virtual ~CBDB_Cache();

    /// Hint to CBDB_Cache about size of cache entry
    /// Large BLOBs work faster with large pages
    /*
    enum EPageSize
    {
        eSmall,
        eLarge
    };
    */

    /// Suggest page size. Should be called before Open.
    /// Does not have any effect if cache is already created.
    //void SetPageSize(EPageSize page_size) { m_PageSizeHint = page_size; }

    /// Options controlling transaction and write syncronicity.
    enum EWriteSyncMode
    {
        eWriteSync,
        eWriteNoSync
    };

    /// Set write syncronization
    void SetWriteSync(EWriteSyncMode wsync) { m_WSync = wsync; }
    EWriteSyncMode GetWriteSync() const { return m_WSync; }

    /// Locking modes to protect cache instance
    enum ELockMode
    {
        eNoLock,     ///< Do not lock-protect cache instance
        ePidLock     ///< Create PID lock on cache (exception if failed)
    };

    /// Schedule a background Purge thread
    /// (cleans the cache from the obsolete entries)
    /// SetPurgeBatchSize and SetBatchSleep should be used to regulate the thread
    /// priority
    ///
    /// @param purge_delay
    ///    Sleep delay in seconds between consequtive Purge Runs
    /// @note Actual thread is started by Open()
    /// (works only in MT builds)
    ///
    /// @sa SetPurgeBatchSize, SetBatchSleep
    void RunPurgeThread(unsigned purge_delay =  30);

    /// Stop background thread
    void StopPurgeThread();

    /// Underlying BDB database can be configured using transactional
    /// or non-transactional API. Transactional provides better protection
    /// from failures, non-transactional offers better performance.
    enum ETRansact
    {
        eUseTrans, ///< Use transaction environment
        eNoTrans   ///< Non-transactional environment
    };

    /// Open local cache instance (read-write access)
    /// If cache does not exists it is created.
    ///
    /// @param cache_path  Path to cache
    /// @param cache_name  Cache instance name
    /// @param lm           Locking mode, protection against using the
    ///                     cache from multiple applications
    /// @param cache_size   Berkeley DB memory cache settings
    /// @param log_mem_size Size of in memory transaction log
    ///
    /// @sa OpenReadOnly
    ///
    void Open(const string&  cache_path,
              const string&  cache_name,
              ELockMode    lm = eNoLock,
              unsigned int cache_ram_size = 0,
              ETRansact    use_trans = eUseTrans,
              unsigned int log_mem_size = 0);

    /// Run verification of the cache database
    ///
    /// @param cache_path  Path to cache
    /// @param cache_name  Cache instance name
    /// @param err_file
    ///    Name of the error file
    ///    When NULL errors are dumped to stderr
    void Verify(const string&  cache_path,
                const string&  cache_name,
                const string&  err_file = 0,
                bool           force_remove = false);

    /// Open local cache in read-only mode.
    /// This is truely passive mode of operations.
    /// All modification calls are ignored, no statistics is going to
    /// be collected, no timestamps. PID locking is also not available.
    ///
    /// @sa Open
    ///
    void OpenReadOnly(const string&  cache_path,
                      const string&  cache_name,
                      unsigned int   cache_ram_size = 0);

    /// Get environment
    CBDB_Env* GetEnv() { return m_Env; }

    /// Return true if environment is not created but joined
    bool IsJoinedEnv() { return m_JoinedEnv; }

    void Close();

    /// Return TRUE if cache is read-only
    bool IsReadOnly() const { return m_ReadOnly; }

    /// Set update attribute limit (0 by default)
    ///
    /// When cache is configured to update BLOB time stamps on read
    /// it can cause delays because it needs to initiate a transaction
    /// every time you read. This method configures a delayed update.
    /// Cache keeps all updates until it reaches the limit and saves it
    /// all in one transaction.
    ///
    void SetReadUpdateLimit(unsigned /* limit */)
    { /*m_MemAttr.SetLimit(limit);*/ }

    /// Remove all non-active LOG files
    void CleanLog();

    /// Set transaction log file size
    void SetLogFileMax(unsigned fl_size)
    {
        m_LogSizeMax = fl_size;
    }

    /// Number of records in scanned at once by Purge
    /// Cache is exclusively locks an internal mutex while
    /// scanning the batch and all other threads is getting locked.
    void SetPurgeBatchSize(unsigned batch_size);

    unsigned GetPurgeBatchSize() const;

    /// Set sleep in milliseconds between Purge batches
    /// (for low priority Purge processes)
    void SetBatchSleep(unsigned sleep);

    unsigned GetBatchSleep() const;

    /// Request to stop Purge. Use this method when Purge runs in
    /// it's own thread and you need the stop that thread in the
    /// middle of processing
    void StopPurge();

    /// When factor is non zero every factor-even Purge will
    /// remove old log files.
    /// (For factor == 1 every Purge will try to remove logs)
    void CleanLogOnPurge(unsigned int factor) { m_CleanLogOnPurge = factor; }

    /// Checkpoint the database at least as often as every bytes of
    /// log file are written.
    void SetCheckpoint(unsigned int bytes) { m_CheckPointInterval = bytes; }

    void SetOverflowLimit(unsigned limit);
    unsigned GetOverflowLimit() const { return m_OverflowLimit; }

    /// Maximum limit for read updates
    /// (blob expires eventually even if it is accessed)
    /// 0 - unlimited prolongation (default)
    void SetTTL_Prolongation(unsigned ttl_prolong);

    /// Get max limit for read update
    unsigned GetTTL_Prolongation() const { return m_MaxTTL_prolong; }

    /// Set max allowed BLOB size. 
    void SetMaxBlobSize(unsigned max_size) { m_MaxBlobSize = max_size; }

    /// Get max allowed BLOB size. 
    unsigned GetMaxBlobSize() const { return m_MaxBlobSize; }

    /// Set number of rotated round-robin volumes
    void SetRR_Volumes(unsigned rrv) { m_RoundRobinVolumes = rrv; }

    /// Get cache operations statistics
    const SBDB_CacheStatistics& GetStatistics() const
    {
        return m_Statistics;
    }

    /// Get cache operations statistics.
    /// Thread safe (syncronized) operation
    void GetStatistics(SBDB_CacheStatistics* cache_stat) const;

    /// Drop the previously collected statistics
    void InitStatistics();


    /// Lock cache access
    void Lock();

    /// Unlock cache access
    void Unlock();


    // Error logging functions

    void RegisterInternalError(SBDB_CacheUnitStatistics::EErrGetPut operation,
                               const string&                        owner);

    void RegisterProtocolError(SBDB_CacheUnitStatistics::EErrGetPut operation, 
                               const string&                        owner);

    void RegisterNoBlobError(SBDB_CacheUnitStatistics::EErrGetPut operation, 
                             const string&                        owner);

    void RegisterCommError(SBDB_CacheUnitStatistics::EErrGetPut operation, 
                           const string&                        owner);


    // ICache interface

    virtual void SetTimeStampPolicy(TTimeStampFlags policy,
                                    unsigned int    timeout,
                                    unsigned int     max_timeout = 0);
    virtual bool IsOpen() const { return m_BLOB_SplitStore != 0; }

    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live = 0,
                       const string&  owner = kEmptyStr);

    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);

    virtual void GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner);

    virtual bool Read(const string& key,
                      int           version,
                      const string& subkey,
                      void*         buf,
                      size_t        buf_size);

    virtual IReader* GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey);
    virtual void GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr* blob_descr);

    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live = 0,
                                    const string&    owner = kEmptyStr);
    virtual bool HasBlobs(const string&  key,
                          const string&  subkey);

    virtual void Remove(const string& key);

    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey);

    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey);

    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    /// Delete BLOB
    /// @param for_update
    ///    When TRUE blob statistics record will not be deleted
    ///    (since it's going to be recreated immediately)
    ///
    /// Returns non-zero blob_id if blob exists
    ///
    void DropBlob(const string&  key,
                  int            version,
                  const string&  subkey,
                  bool           for_update,
                  unsigned*      blob_id,
                  unsigned*      coord);

    virtual bool SameCacheParams(const TCacheParams* params) const;
    virtual string GetCacheName(void) const
    {
        return m_Path + "<" + m_Name + ">";
    }
    const string& GetName() const { return m_Name; }
protected:

    /// Remove BLOB (using transaction) without updating statistics
    /// (for error recovery situations)
    /// No concurrent access locking
    void KillBlobNoLock(const string&  key,
                        int            version,
                        const string&  subkey,
                        int            overflow,
                        unsigned       blob_id);
    void KillBlob(const string&  key,
                  int            version,
                  const string&  subkey,        
                  int            overflow,
                  unsigned       blob_id);

    /// Write to the overflow file with error checking
    /// If write failes, method deletes the file to avoid file system
    /// space leak then throws an exception
    ///
    void WriteOverflow(CNcbiOfstream& overflow_file,
                       const string&  overflow_file_path,
                       const char*    buf, 
                       streamsize     count);
private:
    /// Return TRUE if cache item expired according to the current timestamp
    /// prerequisite: attributes record fetched to memory
    bool x_CheckTimestampExpired();

    bool x_CheckTimestampExpired(time_t         curr);


    /// access type for "UpdateAccessTime" methods
    enum EBlobAccessType
    {
        eBlobStore,
        eBlobUpdate,
        eBlobRead
    };

    void x_UpdateReadAccessTime(const string&  key,
                                int            version,
                                const string&  subkey);

	/// Transactional update of access time attributes
    void x_UpdateAccessTime(const string&   key,
                            int             version,
                            const string&   subkey,
                            EBlobAccessType access_type);

	/// Non transactional update of access time
    void x_UpdateAccessTime_NonTrans(const string&  key,
                                     int            version,
                                     const string&  subkey,
                                     EBlobAccessType access_type);

	/// Non transactional update of access time
    void x_UpdateAccessTime_NonTrans(const string&  key,
                                     int            version,
                                     const string&  subkey,
                                     unsigned       timeout,
                                     EBlobAccessType access_type);

	/// 1. Retrive overflow attributes for the BLOB (using subkey)
	/// 2. If required retrive empty subkey attribute record
	///    (for timestamp check)
	///
	/// @return TRUE if the attribute record found
	bool x_RetrieveBlobAttributes(const string&  key,
                                  int            version,
                                  const string&  subkey,
	 							  int*           overflow,
                                  unsigned int*  ttl,
                                  unsigned int*  blob_id,
                                  unsigned int*  volume_id,
                                  unsigned int*  split_id);

	bool x_FetchBlobAttributes(const string&  key,
                               int            version,
                               const string&  subkey);

    void x_DropBlob(const char*    key,
                    int            version,
                    const char*    subkey,
                    int            overflow,
                    unsigned       blob_id);

    void x_DropOverflow(const char*    key,
                        int            version,
                        const char*    subkey);
    void x_DropOverflow(const string&  file_path);

    void x_TruncateDB();

    void x_Close();

    void x_PidLock(ELockMode lm);

    void x_PerformCheckPointNoLock(unsigned bytes_written);

    IReader* x_CreateOverflowReader(int overflow,
                                    const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    size_t&        file_length);

    /// update BLOB owners' statistics on BLOB delete
    void x_UpdateOwnerStatOnDelete(const string& owner, bool expl_delete);

    /// Determines BLOB size (requires fetched attribute record)
    /*
    size_t x_GetBlobSize(const char* key,
                         int         version,
                         const char* subkey);
    */

private:
    CBDB_Cache(const CBDB_Cache&);
    CBDB_Cache& operator=(const CBDB_Cache);
private:

    /// Derivative from general purpose transaction
    /// Connects all cache tables automatically
    ///
    /// @internal
/*
    class CCacheTransaction : public CBDB_Transaction
    {
    public:
        CCacheTransaction(CBDB_Cache& cache)
            : CBDB_Transaction(*(cache.m_Env),
              CBDB_Transaction::eEnvDefault,
              CBDB_Transaction::eNoAssociation)
        {
            cache.m_CacheAttrDB->SetTransaction(this);
            cache.m_BLOB_SplitStore->SetTransaction(this);
        }
    };
*/

public:

    /// Cache accession for internal in-memory storage
    ///
    /// @internal
    struct CacheKey
    {
        string key;
        int    version;
        string subkey;

        CacheKey(const string& x_key, int x_version, const string& x_subkey);
        bool operator < (const CacheKey& cache_key) const;
    };

private:
    friend class CCacheTransaction;
    friend class CBDB_CacheIWriter;
    friend class CBDB_CacheIReader;

private:
    string                  m_Path;         ///< Path to storage
    string                  m_Name;         ///< Cache name
    CPIDGuard*              m_PidGuard;     ///< Cache lock
    bool                    m_ReadOnly;     ///< read-only flag

    bool                    m_JoinedEnv;    ///< Joined environment
    CBDB_Env*               m_Env;          ///< Common environment for cache DBs
    //SCacheBLOB_DB*          m_CacheBLOB_DB; ///< Cache BLOB storage
    TSplitStore*            m_BLOB_SplitStore;///< Cache BLOB storage
    SCache_AttrDB*          m_CacheAttrDB;  ///< Cache attributes database
    mutable CFastMutex      m_DB_Lock;      ///< Database lock


    TTimeStampFlags         m_TimeStampFlag;///< Time stamp flag
    unsigned                m_Timeout;      ///< Timeout expiration policy
    unsigned                m_MaxTimeout;   ///< Maximum time to live
    EKeepVersions           m_VersionFlag;  ///< Version retention policy

//    EPageSize               m_PageSizeHint; ///< Suggested page size
    EWriteSyncMode          m_WSync;        ///< Write syncronization
    /// Number of records to process in Purge() (with locking)
    unsigned                m_PurgeBatchSize;
    /// Sleep interval (milliseconds) between batches
    unsigned                m_BatchSleep;
    /// TRUE when Purge processing requested to stop
    //bool                    m_PurgeStop;
    CSemaphore              m_PurgeStopSignal;
    /// Number of bytes stored in cache since last checkpoint
    //unsigned                m_BytesWritten;
    /// Clean log on Purge (factor)
    unsigned                m_CleanLogOnPurge;
    /// Number of times we run purge
    unsigned                m_PurgeCount;
    /// transaction log size
    unsigned                m_LogSizeMax;
    /// Purge thread
    CRef<CCacheCleanerThread>  m_PurgeThread;
    /// Flag that Purge is already running
    bool                       m_PurgeNowRunning;
    /// Run a background purge thread
    bool                       m_RunPurgeThread;
    /// Delay in seconds between Purge runs
    unsigned                   m_PurgeThreadDelay;
    /// Trnasaction checkpoint interval (bytes)
    unsigned                   m_CheckPointInterval;
    /// Overflow limit (objects lower than that stored as BLOBs)
    unsigned                   m_OverflowLimit;
    /// Number of m_MaxTimeout values object lives (read-prolongation)
    unsigned                   m_MaxTTL_prolong;

    /// Stat counters
    SBDB_CacheStatistics       m_Statistics;
    /// used by x_UpdateOwnerStatOnDelete
    string                     m_TmpOwnerName;
    /// Fast local timer
    CFastLocalTime             m_LocalTimer;
    /// Atomic counter for BLOB ids
    CAtomicCounter             m_BlobIdCounter;

    /// Max.allowed BLOB size 
    unsigned                   m_MaxBlobSize;

    /// Number of rotated volumes
    unsigned                   m_RoundRobinVolumes;
};


/// Utility for simple cache configuration.
///
/// @param bdb_cache
///   Cache instance to configure
/// @param path
///   Path to the cache database
/// @param name
///   Cache database name
/// @param timeout
///   Cache elements time to live value (in seconds)
///   (when "0" 24hrs value is taken)
/// @param tflags
///   Timestamp flags (see ICache)
///   (when 0 some reasonable default combination is taken)
///
NCBI_BDB_CACHE_EXPORT
void BDB_ConfigureCache(CBDB_Cache&             bdb_cache,
                        const string&           path,
                        const string&           name,
                        unsigned                timeout,
                        ICache::TTimeStampFlags tflags);


extern NCBI_BDB_CACHE_EXPORT const char* kBDBCacheDriverName;

extern "C"
{

NCBI_BDB_CACHE_EXPORT
void  NCBI_BDB_ICacheEntryPoint(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);

NCBI_BDB_CACHE_EXPORT
void NCBI_EntryPoint_xcache_bdb(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);


} // extern C



///
/// Class container for BDB cache objects.
///
/// @internal

class NCBI_BDB_CACHE_EXPORT CBDB_CacheHolder : public CObject
{
public:
    CBDB_CacheHolder(ICache* blob_cache, ICache* id_cache);
    ~CBDB_CacheHolder();

    ICache* GetBlobCache() { return m_BlobCache; }
    ICache* GetIdCache() { return m_IdCache; }

private:
    CBDB_CacheHolder(const CBDB_CacheHolder&);
    CBDB_CacheHolder& operator=(const CBDB_CacheHolder&);

private:
    ICache*           m_BlobCache;
    ICache*           m_IdCache;
};


/* @} */


END_NCBI_SCOPE

#endif
