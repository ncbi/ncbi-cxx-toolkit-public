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
#include <util/cache/icache.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_trans.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB_Cache
 *
 * @{
 */


struct NCBI_BDB_EXPORT SCacheDB : public CBDB_BLobFile
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


struct NCBI_BDB_EXPORT SCache_AttrDB : public CBDB_File
{
    CBDB_FieldString       key;
    CBDB_FieldInt4         version;
    CBDB_FieldString       subkey;
    CBDB_FieldInt4         time_stamp;
    CBDB_FieldInt4         overflow;

    SCache_AttrDB()
    {
        BindKey("key",     &key, 256);
        BindKey("version", &version);
        BindKey("subkey",  &subkey, 256);

        BindData("time_stamp", &time_stamp);
        BindData("overflow",   &overflow);
    }
};

class CPIDGuard;


/// BDB cache implementation.
///
/// Class implements ICache interface using local Berkeley DB
/// database.

class NCBI_BDB_EXPORT CBDB_Cache : public ICache
{
public:
    CBDB_Cache();
    virtual ~CBDB_Cache();

    enum ELockMode 
    {
        eNoLock,     ///< Do not lock-protect cache instance
        ePidLock     ///< Create PID lock on cache (exception if failed) 
    };

    /// Open local cache instance (read-write access)
    /// If cache does not exists it is created.
    ///
    /// @param cache_path  Path to cache
    /// @param cache_name  Cache instance name
    /// @param lm          Locking mode, protection against using the 
    ///                    cache from multiple applications
    /// @param cache_size  Berkeley DB memory cache settings
    ///
    /// @sa OpenReadOnly
    ///
    void Open(const char*  cache_path, 
              const char*  cache_name,
              ELockMode    lm = eNoLock,
              unsigned int cache_ram_size = 0);

    /// Open local cache in read-only mode.
    /// This is truely passive mode of operations. 
    /// All modification calls are ignored, no statistics is going to 
    /// be collected, no timestamps. PID locking is also not available.
    ///
    /// @sa Open
    ///
    void OpenReadOnly(const char*  cache_path, 
                      const char*  cache_name,
                      unsigned int cache_ram_size = 0);

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
    void SetReadUpdateLimit(unsigned limit) { m_MemAttr.SetLimit(limit); }


    // ICache interface 

    virtual void SetTimeStampPolicy(TTimeStampFlags policy, int timeout);
    virtual bool IsOpen() const { return m_CacheDB != 0; }

    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size);

    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);

    virtual bool Read(const string& key, 
                      int           version, 
                      const string& subkey,
                      void*         buf, 
                      size_t        buf_size);

    virtual IReader* GetReadStream(const string&  key, 
                                   int            version,
                                   const string&  subkey);

    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey);

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

private:
    /// Return TRUE if cache item expired according to the current timestamp
    bool x_CheckTimestampExpired(const string&  key,
                                 int            version,
                                 const string&  subkey);

    void x_UpdateReadAccessTime(const string&  key,
                                int            version,
                                const string&  subkey);

	/// Transactional update of access time attributes
    void x_UpdateAccessTime(const string&  key,
                            int            version,
                            const string&  subkey);

	/// Non transactional update of access time
    void x_UpdateAccessTime_NonTrans(const string&  key,
                                     int            version,
                                     const string&  subkey);

	/// Non transactional update of access time
    void x_UpdateAccessTime_NonTrans(const string&  key,
                                     int            version,
                                     const string&  subkey,
                                     int            timeout);

	/// 1. Retrive overflow attributes for the BLOB (using subkey)
	/// 2. If required retrive empty subkey attribute record 
	///    (for timestamp check)
	///
	/// @return TRUE if the attribute record found
	bool x_RetrieveBlobAttributes(const string&  key,
                                 int            version,
                                 const string&  subkey,
								 int*           overflow);

    void x_DropBlob(const char*    key,
                    int            version,
                    const char*    subkey,
                    int            overflow);

    void x_TruncateDB();

    /// Save in-memory attributes
    void x_SaveAttrStorage();
    void x_SaveAttrStorage_NonTrans();

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
            : CBDB_Transaction(*(cache.m_Env))
        {
            cache.m_CacheDB->SetTransaction(this);
            cache.m_CacheAttrDB->SetTransaction(this);
        }
    };

    friend class CCacheTransaction;

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

    /// @internal
    class CMemAttrStorage
    {
    public:
        CMemAttrStorage();
        void UpdateAccessTime(const CacheKey& cache_key, int timeout)
        {
            m_Attr[cache_key] = timeout;
        }
        void SetLimit(unsigned limit) { m_Limit = limit; }

        void Remove(const CacheKey& cache_key)
        {
            m_Attr.erase(cache_key);
        }

        /// Save all attributes (non-transactional)
        void DumpToStorage(CBDB_Cache& cache);

        /// Forget all records
        void Clear() { m_Attr.clear(); }

        unsigned Size() const { return (unsigned)m_Attr.size(); }
        unsigned Limit() const { return m_Limit; }
        /// Return TRUE when storage accumulated above the limits
        bool IsLimitReached() const { return Size() > Limit(); }
        /// TRUE if storage was configured to work
        bool IsActive() const { return Limit() != 0; }

        /// Return access time or 0 if key is not in the storage
        int GetAccessTime(const CacheKey& cache_key) const;

    private:
        typedef map<CacheKey, int>  TAttrMap;
    private:
        unsigned   m_Limit;    ///< limit for in-memory storage
        TAttrMap   m_Attr;     ///< in-memory attributes
    };

    friend class MemAttrStorage;

private:
    string                  m_Path;       ///< Path to storage
    string                  m_Name;       ///< Cache name
    CPIDGuard*              m_PidGuard;   ///< Cache lock
    bool                    m_ReadOnly;   ///< read-only flag
    CBDB_Env*               m_Env;          ///< Common environment for cache DBs
    SCacheDB*               m_CacheDB;      ///< Cache BLOB storage
    SCache_AttrDB*          m_CacheAttrDB;  ///< Cache attributes database
    TTimeStampFlags         m_TimeStampFlag;///< Time stamp flag
    int                     m_Timeout;      ///< Timeout expiration policy
    EKeepVersions           m_VersionFlag;  ///< Version retention policy

    CMemAttrStorage         m_MemAttr;      ///< In-memory cache for attributes
};



extern NCBI_BDB_EXPORT const char* kBDBCacheDriverName;

extern "C" 
{

void NCBI_BDB_EXPORT NCBI_BDB_ICacheEntryPoint(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);

inline 
void NCBI_BDB_EXPORT NCBI_EntryPoint_ICache_bdb(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    NCBI_BDB_ICacheEntryPoint(info_list, method);
}

inline 
void NCBI_BDB_EXPORT NCBI_EntryPoint_ICache_bdbcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    NCBI_BDB_ICacheEntryPoint(info_list, method);
}


} // extern C



///
/// Class container for BDB cache objects.
///
/// @internal

class NCBI_BDB_EXPORT CBDB_CacheHolder : public CObject
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
