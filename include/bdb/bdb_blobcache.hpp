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

#include <util/cache/icache.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_trans.hpp>

#include <deque>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB_Cache
 *
 * @{
 */

/// Register NCBI_BDB_ICacheEntryPoint
void BDB_Register_Cache(void);

struct NCBI_BDB_CACHE_EXPORT SCacheDB : public CBDB_BLobFile
{
    CBDB_FieldString       key;
    CBDB_FieldInt4         version;
    CBDB_FieldString       subkey;

    SCacheDB()
    {
        BindKey("key",     &key, 256);
        BindKey("version", &version);
        BindKey("subkey",  &subkey, 256);
    }
};

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
    enum EPageSize
    {
        eSmall,
        eLarge
    };

    /// Suggest page size. Should be called before Open.
    /// Does not have any effect if cache is already created.
    void SetPageSize(EPageSize page_size) { m_PageSizeHint = page_size; }

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
    virtual bool IsOpen() const { return m_CacheDB != 0; }

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
    void DropBlob(const string&  key,
                  int            version,
                  const string&  subkey,
                  bool           for_update);

    virtual bool SameCacheParams(const TCacheParams* params) const;
    virtual string GetCacheName(void) const
    {
        return m_Path + "<" + m_Name + ">";
    }
protected:

    /// Remove BLOB (using transaction) without updating statistics
    /// (for error recovery situations)
    /// No concurrent access locking
    void KillBlobNoLock(const char*    key,
                        int            version,
                        const char*    subkey,
                        int            overflow);
    void KillBlob(const char*    key,
                  int            version,
                  const char*    subkey,
                  int            overflow);

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
                                  unsigned int*  ttl);

	bool x_FetchBlobAttributes(const string&  key,
                               int            version,
                               const string&  subkey);

    void x_DropBlob(const char*    key,
                    int            version,
                    const char*    subkey,
                    int            overflow);
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
    size_t x_GetBlobSize(const char* key,
                         int         version,
                         const char* subkey);

private:
    CBDB_Cache(const CBDB_Cache&);
    CBDB_Cache& operator=(const CBDB_Cache);
private:

    /// Derivative from general purpose transaction
    /// Connects all cache tables automatically
    ///
    /// @internal
    class CCacheTransaction : public CBDB_Transaction
    {
    public:
        CCacheTransaction(CBDB_Cache& cache)
            : CBDB_Transaction(*(cache.m_Env),
              cache.GetWriteSync() == eWriteSync ?
                 CBDB_Transaction::eTransSync : CBDB_Transaction::eTransASync)
        {
            cache.m_CacheDB->SetTransaction(this);
            cache.m_CacheAttrDB->SetTransaction(this);
        }
    };

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

    CBDB_Env*               m_Env;          ///< Common environment for cache DBs
    SCacheDB*               m_CacheDB;      ///< Cache BLOB storage
    SCache_AttrDB*          m_CacheAttrDB;  ///< Cache attributes database
    mutable CFastMutex      m_DB_Lock;      ///< Database lock


    TTimeStampFlags         m_TimeStampFlag;///< Time stamp flag
    unsigned                m_Timeout;      ///< Timeout expiration policy
    unsigned                m_MaxTimeout;   ///< Maximum time to live
    EKeepVersions           m_VersionFlag;  ///< Version retention policy

    EPageSize               m_PageSizeHint; ///< Suggested page size
    EWriteSyncMode          m_WSync;        ///< Write syncronization
    /// Number of records to process in Purge() (with locking)
    unsigned                m_PurgeBatchSize;
    /// Sleep interval (milliseconds) between batches
    unsigned                m_BatchSleep;
    /// TRUE when Purge processing requested to stop
    //bool                    m_PurgeStop;
    CSemaphore              m_PurgeStopSignal;
    /// Number of bytes stored in cache since last checkpoint
    unsigned                m_BytesWritten;
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.67  2006/12/18 19:52:01  kuznets
 * Use string not const char* for db opening, etc.
 *
 * Revision 1.66  2006/08/17 20:46:48  kuznets
 * Added support of in-memory logs
 *
 * Revision 1.65  2006/01/11 15:07:23  vasilche
 * Cleaned SBlobCacheDescr structure.
 *
 * Revision 1.64  2005/12/05 13:44:57  kuznets
 * +InitStatistics()
 *
 * Revision 1.63  2005/12/01 14:37:48  kuznets
 * Improvements in collecting statistics
 *
 * Revision 1.62  2005/11/28 15:18:33  kuznets
 * Improved overflow file management
 *
 * Revision 1.61  2005/11/15 13:37:49  kuznets
 * Convert cache statistics to registry
 *
 * Revision 1.60  2005/11/01 15:09:54  kuznets
 * Fixed compilation warning
 *
 * Revision 1.59  2005/08/15 11:33:54  kuznets
 * Statistics: Added total number of BLOBs deleted by GC
 *
 * Revision 1.58  2005/08/08 14:49:06  kuznets
 * Improved logging
 *
 * Revision 1.57  2005/08/01 19:43:59  vakatov
 * NCBI_BDB_CACHE_EXPORT SBDB_CacheStatistics
 *
 * Revision 1.56  2005/08/01 16:51:37  kuznets
 * Added BDB cache statistics
 *
 * Revision 1.55  2005/06/30 16:54:32  grichenk
 * Moved cache ownership to GB loader. Improved cache sharing.
 * Added CGBDataLoader::PurgeCache().
 *
 * Revision 1.54  2005/05/12 15:49:49  grichenk
 * Share bdb cache between reader and writer
 *
 * Revision 1.53  2005/04/12 17:55:49  kuznets
 * Added BLOB TTL read prolongation limit(to let BLOBs expire)
 *
 * Revision 1.52  2005/04/07 17:45:54  kuznets
 * Added overflow limit cache parameter
 *
 * Revision 1.51  2005/03/30 13:09:06  kuznets
 * Use semaphor to stop purge execution
 *
 * Revision 1.50  2005/03/23 17:22:54  kuznets
 * SetLogFileMax()
 *
 * Revision 1.49  2005/02/22 13:01:53  kuznets
 * +HasBlobs()
 *
 * Revision 1.48  2005/02/02 19:49:53  grichenk
 * Fixed more warnings
 *
 * Revision 1.47  2005/01/03 14:26:43  kuznets
 * Implemented variable checkpoint interval (was hard coded)
 *
 * Revision 1.46  2004/12/28 19:43:40  kuznets
 * +x_CheckTimeStampExpired
 *
 * Revision 1.45  2004/12/28 16:46:15  kuznets
 * +DropBlob()
 *
 * Revision 1.44  2004/12/22 21:02:53  grichenk
 * BDB and DBAPI caches split into separate libs.
 * Added entry point registration, fixed driver names.
 *
 * Revision 1.43  2004/12/22 19:25:15  kuznets
 * Code cleanup
 *
 * Revision 1.42  2004/12/22 14:34:17  kuznets
 * +GetBlobAccess()
 *
 * Revision 1.41  2004/12/16 14:25:12  kuznets
 * + BDB_ConfigureCache (simple BDB configurator)
 *
 * Revision 1.40  2004/11/08 16:00:44  kuznets
 * Implemented individual timeouts
 *
 * Revision 1.39  2004/11/03 17:54:04  kuznets
 * All time related parameters made unsigned
 *
 * Revision 1.38  2004/11/03 17:07:29  kuznets
 * ICache revision2. Add individual timeouts
 *
 * Revision 1.37  2004/10/27 17:02:41  kuznets
 * Added option to run a background cleaning(Purge) thread
 *
 * Revision 1.36  2004/10/18 16:06:22  kuznets
 * Implemented automatic (on Purge) log cleaning
 *
 * Revision 1.35  2004/10/15 14:02:08  kuznets
 * Added counter of bytes written in cache
 *
 * Revision 1.34  2004/10/13 12:52:19  kuznets
 * Changes in Purge processing: more options to allow Purge in a thread
 *
 * Revision 1.33  2004/09/27 14:03:58  kuznets
 * +option not to use transactions, and method to clean log files
 *
 * Revision 1.32  2004/09/03 13:34:59  kuznets
 * Added async write option
 *
 * Revision 1.31  2004/08/24 15:04:19  kuznets
 * + SetPageSize() to fine control IO performance
 *
 * Revision 1.30  2004/08/24 14:06:53  kuznets
 * +x_PidLock
 *
 * Revision 1.29  2004/08/13 11:04:08  kuznets
 * +Verify()
 *
 * Revision 1.28  2004/08/10 12:19:50  kuznets
 * Entry points made non-inline to make sure they always instantiate
 *
 * Revision 1.27  2004/08/09 20:56:03  kuznets
 * Fixed compilation (WorkShop)
 *
 * Revision 1.26  2004/08/09 15:12:51  vasilche
 * Fixed syntax error.
 *
 * Revision 1.25  2004/08/09 14:26:33  kuznets
 * Add delayed attribute update (performance opt.)
 *
 * Revision 1.24  2004/07/27 14:04:58  kuznets
 * +IsOpen
 *
 * Revision 1.23  2004/07/27 13:37:23  kuznets
 * + ICache_bdbcache entry point
 *
 * Revision 1.22  2004/07/27 13:35:49  kuznets
 * + ICache entry point
 *
 * Revision 1.21  2004/07/19 16:11:10  kuznets
 * + Remove for key,version,subkey
 *
 * Revision 1.20  2004/07/13 14:54:06  kuznets
 * GetTimeout() made const
 *
 * Revision 1.19  2004/06/14 16:10:36  kuznets
 * Added read-only mode
 *
 * Revision 1.18  2004/06/10 17:14:57  kuznets
 * Fixed work with overflow files
 *
 * Revision 1.17  2004/05/24 18:02:49  kuznets
 * CBDB_Cache::Open added parameter to specify RAM cache size
 *
 * Revision 1.16  2004/04/28 12:21:46  kuznets
 * Cleaned up dead code
 *
 * Revision 1.15  2004/04/28 12:11:08  kuznets
 * Replaced static string with char* (fix crash on Linux)
 *
 * Revision 1.14  2004/04/27 19:11:49  kuznets
 * Commented old cache implementation
 *
 * Revision 1.13  2004/02/27 17:29:05  kuznets
 * +CBDB_CacheHolder
 *
 * Revision 1.12  2004/01/13 16:37:27  vasilche
 * Removed extra comma.
 *
 * Revision 1.11  2003/12/08 16:12:02  kuznets
 * Added plugin mananger support
 *
 * Revision 1.10  2003/11/25 19:36:24  kuznets
 * + ICache implementation
 *
 * Revision 1.9  2003/10/24 12:35:28  kuznets
 * Added cache locking options.
 *
 * Revision 1.8  2003/10/21 11:56:41  kuznets
 * Fixed bug with non-updated Int cache timestamp.
 *
 * Revision 1.7  2003/10/16 19:27:04  kuznets
 * Added Int cache (AKA id resolution cache)
 *
 * Revision 1.6  2003/10/15 18:12:49  kuznets
 * Implemented new cache architecture based on combination of BDB tables
 * and plain files. Fixes the performance degradation in Berkeley DB
 * when it has to work with a lot of overflow pages.
 *
 * Revision 1.5  2003/10/02 20:14:16  kuznets
 * Minor code cleanup
 *
 * Revision 1.4  2003/10/01 20:49:41  kuznets
 * Changed several function prototypes (were marked as pure virual)
 *
 * Revision 1.3  2003/09/29 15:46:18  kuznets
 * Fixed warning (Sun WorkShop)
 *
 * Revision 1.2  2003/09/26 20:54:37  kuznets
 * Documentaion change
 *
 * Revision 1.1  2003/09/24 14:29:42  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
