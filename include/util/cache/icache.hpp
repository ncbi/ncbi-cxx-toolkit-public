#ifndef UTIL___ICACHE__HPP
#define UTIL___ICACHE__HPP

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
 * File Description: cache interface specs.
 *
 */

/// @file icache.hpp
/// Cache interface specs. 
///
/// File describes interfaces used to create local cache of 
/// binary large objects (BLOBS).


#include <corelib/plugin_manager_impl.hpp>
#include <util/reader_writer.hpp>
#include <string>


BEGIN_NCBI_SCOPE



/// BLOB cache read/write/maintanance interface.
///
/// ICache describes caching service. Any large binary object
/// can be stored in cache and later retrived. Such cache is a 
/// temporary storage and some objects can be purged from cache based
/// on an immediate request, version or access time 
/// based replacement (or another implementation specific depreciation rule).
///
/// Cache elements are accesed by key-subkey pair. 
/// 
class ICache
{
public:
    
    /// ICache keeps timestamps of every cache entry.
    /// This enum defines the policy how it is managed.
    /// Different policies can be combined by OR (|)
    ///
    /// @sa SetTimeStampPolicy, TTimeStampFlags
    ///
    enum ETimeStampFlags {

        /// Timestamp management disabled
        fNoTimeStamp               = 0,

        /// Cache element is created with a certain timestamp (default)
        fTimeStampOnCreate         = (1 << 0),

        /// Timestamp is updated every on every access (read or write)
        fTimeStampOnRead            = (1 << 1),
        
        /// Timestamp full key-subkey pair. By default only key is taken
        /// into account
        fTrackSubKey                = (1 << 2),

        /// Expire objects older than a certain time frame
        /// Example: If object is not accessed within a week it is 
        ///          droped from the cache.
        fExpireLeastFrequentlyUsed  = (1 << 3),

        /// Expired objects should be deleted on cache mount (Open)
        fPurgeOnStartup             = (1 << 4),

        /// Expiration timeout is checked on any access to cache element
        fCheckExpirationAlways      = (1 << 5)
    };

    typedef ETimeStampFlags ETimeStampPolicy;

    /// @sa ETimeStampFlags
    typedef int TTimeStampFlags;

    /// Set timestamp update policy
    /// @param policy
    ///   A bitwise combination of "ETimeStampFlags".
    /// @param timeout
    ///   Default expiration timeout for the stored BLOBs.
    /// @param max_timeout
    ///   Maximum value for individually set BLOB timeouts. 
    ///   If "max_timeout" < "timeout", then it 'll be set to "timeout".
    ///
    virtual void SetTimeStampPolicy(TTimeStampFlags policy, 
                                    unsigned int    timeout,
                                    unsigned int    max_timeout = 0) = 0;

    /// Get timestamp policy
    /// @return
    ///    Timestamp policy
    virtual TTimeStampFlags GetTimeStampPolicy() const = 0;

    /// Get expiration timeout
    /// @return
    ///    Expiration timeout in seconds
    /// @sa SetTimeStampPolicy
    virtual int GetTimeout() const = 0;

    /// @return
    ///    TRUE if cache is open and operational
    virtual bool IsOpen() const = 0;


    /// If to keep already cached versions of the BLOB when storing
    /// another version of it (not necessarily a newer one)
    /// @sa Store(), GetWriteStream()
    enum EKeepVersions {
        /// Do not delete other versions of cache entries
        eKeepAll,
        /// Delete the earlier (than the one being stored) versions of
        /// the BLOB
        eDropOlder,
        /// Delete all versions of the BLOB, even those which are newer
        /// than the one being stored
        eDropAll
    };

    /// Set version retention policy
    ///
    /// @param policy
    ///    Version retetion mode
    virtual void SetVersionRetention(EKeepVersions policy) = 0;

    /// Get version retention
    virtual EKeepVersions GetVersionRetention() const = 0;

    /// Add or replace BLOB
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param key 
    ///    BLOB identification sub-key
    /// @param version 
    ///    BLOB version
    /// @param data 
    ///    pointer on data buffer
    /// @param size 
    ///    data buffer size in bytes (chars)
    /// @param time_to_live
    ///    Individual timeout. Cannot exceed max timeout.
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live = 0) = 0;

    /// Check if BLOB exists, return BLOB size.
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    /// @return 
    ///    BLOB size or 0 if it doesn't exist or expired
    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey) = 0;

    /// Fetch the BLOB
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    /// @param 
    ///    buf pointer on destination buffer
    /// @param 
    ///    size buffer size in bytes (chars)
    /// @return 
    ///    FALSE if BLOB doesn't exist or expired
    ///
    /// @note Throws an exception if provided memory buffer is insufficient 
    /// to read the BLOB
    virtual bool Read(const string& key, 
                      int           version, 
                      const string& subkey,
                      void*         buf, 
                      size_t        buf_size) = 0;

    /// Return sequential stream interface to read BLOB data.
    /// 
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    /// @return Interface pointer or NULL if BLOB does not exist
    virtual IReader* GetReadStream(const string&  key, 
                                   int            version,
                                   const string&  subkey) = 0;

    /// BLOB access descriptor
    struct BlobAccessDescr
    {
        IReader*   reader;
        char*      buf;
        size_t     buf_size;
        size_t     blob_size;
    };

    /// Get BLOB access using BlobAccessDescr.
    ///
    /// Method fills blob_descr parameter.
    /// If provided buffer has sufficient capacity for BLOB storage, BLOB
    /// is saved into the buffer, otherwise IReader is created.
    /// 
    /// @note 
    ///  Method supposed to provide fast access to relatively small BLOBs
    virtual void GetBlobAccess(const string&     key, 
                               int               version,
                               const string&     subkey,
                               BlobAccessDescr*  blob_descr) = 0;

    /// Return sequential stream interface to write BLOB data.
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    /// @param time_to_live
    ///    Individual timeout
    /// @return Interface pointer or NULL if BLOB does not exist
    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live = 0) = 0;

    /// Remove all versions of the specified BLOB
    ///
    /// @param key BLOB identification key
    virtual void Remove(const string& key) = 0;

    /// Remove specific cache entry
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey) = 0;

    /// Return last access time for the specified cache entry
    ///
    /// Class implementation may want to implement access time based
    /// aging scheme for cache managed objects. In this case it needs to
    /// track time of every request to BLOB data.
    ///
    /// @param key 
    ///    BLOB identification key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param version 
    ///    BLOB version
    /// @return 
    ///    last access time
    /// @sa TimeStampUpdatePolicy
    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey) = 0;

    /// Check if any BLOB exists (any version)
    ///
    virtual bool HasBlobs(const string&  key,
                          const string&  subkey) = 0;
    /// Delete all BLOBs older than specified
    ///
    /// @param access_timeout
    ///    Time in seconds. All objects older than this are deleted.
    /// @param keep_last_version 
    ///    type of cleaning action
    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll) = 0;

    /// Delete BLOBs with access time older than specified
    /// 
    /// Function finds all BLOB versions with the specified key
    /// and removes the old instances.
    /// @param key
    ///    BLOB key
    /// @param subkey
    ///    BLOB identification subkey
    /// @param access_timeout 
    ///    Time in seconds. All objects older than this are deleted.
    /// @param keep_last_version 
    ///    type of cleaning action
    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll) = 0;


    virtual ~ICache() {}
};


NCBI_DECLARE_INTERFACE_VERSION(ICache,  "xcache", 3, 1, 0);

template<>
class CDllResolver_Getter<ICache>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<ICache>::GetName(), 
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);

        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/03/07 14:43:15  ssikorsk
 * Do not unload PluginManager drivers by default
 *
 * Revision 1.14  2005/02/22 13:01:29  kuznets
 * +HasBlobs()
 *
 * Revision 1.13  2004/12/22 19:32:34  grichenk
 * +CDllResolver_Getter<ICache>
 * Changed interface name from icache to xcache
 *
 * Revision 1.12  2004/12/22 14:33:52  kuznets
 * +GetBlobAccess()
 *
 * Revision 1.11  2004/11/03 17:53:47  kuznets
 * All time related parameters made unsigned
 *
 * Revision 1.10  2004/11/03 17:21:15  kuznets
 * Formatting...cosmetics...
 *
 * Revision 1.9  2004/11/03 17:07:05  kuznets
 * ICache revision2. Add individual timeouts
 *
 * Revision 1.8  2004/09/21 14:40:14  kuznets
 * Cosmetics
 *
 * Revision 1.7  2004/07/27 14:04:00  kuznets
 * +IsOpen
 *
 * Revision 1.6  2004/07/19 16:10:47  kuznets
 * + Remove for key,version,subkey
 *
 * Revision 1.5  2004/07/13 14:53:48  kuznets
 * GetTimeout() made const
 *
 * Revision 1.4  2004/04/19 18:05:40  kuznets
 * Clarification in the comments department
 *
 * Revision 1.3  2003/12/08 16:11:43  kuznets
 * Added plugin mananger support
 *
 * Revision 1.2  2003/11/25 17:12:54  kuznets
 * Reworked cache maintanance methods.
 * Unified cache items expiration functions.
 *
 * Revision 1.1  2003/11/21 12:51:31  kuznets
 * Added new refactored interface for local caching.
 * It's supposed after some work to replace the existing BLOB cache and Int
 * cache and work for all our caching needs.
 * Interface is in the design stage now, all comments are welcome.
 *
 * ===========================================================================
 */

#endif  /* UTIL___ICACHE__HPP */
