#ifndef UTIL___BLOB_CACHE__HPP
#define UTIL___BLOB_CACHE__HPP

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
 * File Description: BLOB cache interface specs.
 *
 */

/// @file blob_cache.hpp
/// BLOB cache interface specs. 
///
/// File describes interfaces used to create local cache of remote
/// binary large objects (BLOBS).


#include <util/reader_writer.hpp>
#include <string>


BEGIN_NCBI_SCOPE



/// BLOB cache read/write/maintanance interface.
///
/// IBLOB_Cache describes caching service. Any large binary object
/// can be stored in cache and later retrived. Such cache is a 
/// temporary storage and some objects can be purged from cache based
/// on an immediate request, version or access time 
/// based replacement (or another implementation specific depreciation rule).
class IBLOB_Cache
{
public:
    /// If to keep already cached versions of the BLOB when storing
    /// another version of it (not necessarily a newer one)
    /// @sa Store(), GetWriteStream()
    enum EKeepVersions {
        /// Do not delete other versions of the BLOB from the cache
        eKeepAll,
        /// Delete the earlier (than the one being stored) versions of
        /// the BLOB
        eDropOlder,
        /// Delete all versions of the BLOB, even those which are newer
        /// than the one being stored
        eDropAll
    };

    /// Add or replace BLOB
    ///
    /// @param key BLOB identification key
    /// @param version BLOB version
    /// @param data pointer on data buffer
    /// @param size data buffer size
    /// @param flag indicator to keep old BLOBs or drop it from the cache
    virtual void Store(const string& key,
                       int           version,
                       const void*   data,
                       size_t        size,
                       EKeepVersions keep_versions = eDropOlder) = 0;

    /// Check if BLOB exists, return BLOB size.
    ///
    /// @param key - BLOB identification key
    /// @param version - BLOB version
    /// @return BLOB size or 0 if it doesn't exist
    virtual size_t GetSize(const string& key,
                           int           version) = 0;

    /// Fetch the BLOB
    ///
    /// @param key BLOB identification key
    /// @param version BLOB version
    /// @param buf pointer on destination buffer
    /// @param size buffer size
    /// @return FALSE if BLOB doesn't exist
    ///
    /// @note Throws an exception if provided memory buffer is insufficient 
    /// to read the BLOB
    virtual bool Read(const string& key, 
                      int           version, 
                      void*         buf, 
                      size_t        buf_size) = 0;

    /// Return sequential stream interface to read BLOB data.
    /// 
    /// @param key BLOB identification key
    /// @param version BLOB version
    /// @return Interface pointer or NULL if BLOB does not exist
    virtual IReader* GetReadStream(const string& key, 
                                   int   version) = 0;

    /// Return sequential stream interface to write BLOB data.
    ///
    /// @param key BLOB identification key
    /// @param version BLOB version
    /// @param flag indicator to keep old BLOBs or drop it from the cache
    /// @return Interface pointer or NULL if BLOB does not exist
    virtual IWriter* GetWriteStream(const string&    key, 
                                    int              version,
                                    EKeepVersions    keep_versions = eDropOlder) = 0;

    /// Remove all versions of the specified BLOB
    ///
    /// @param key BLOB identification key
    virtual void Remove(const string& key) = 0;

    /// Return last access time for the specified BLOB
    ///
    /// Class implementation may want to implement access time based
    /// aging scheme for cache managed objects. In this case it needs to
    /// track time of every request to BLOB data.
    ///
    /// @param key BLOB identification key
    /// @param version BLOB version
    /// @return last access time
    virtual time_t GetAccessTime(const string& key,
                                 int           version) = 0;

    /// Delete all BLOBs with access time older than specified
    ///
    /// @param access_time time point, indicates objects 
    /// newer than that which to keep in cache
    /// @param keep_last_version type of cleaning action
    virtual void Purge(time_t           access_time,
                       EKeepVersions    keep_last_version = eDropAll) = 0;
    /// Delete BLOBs with access time older than specified
    /// 
    /// Function finds all BLOB versions with the specified key
    /// and removes the old instances.
    /// @param key - BLOB key
    /// @param access_time time point, indicates objects 
    /// newer than that which to keep in cache
    /// @param keep_last_version type of cleaning action
    virtual void Purge(const string&    key,
                       time_t           access_time,
                       EKeepVersions    keep_last_version = eDropAll) = 0;


    virtual ~IBLOB_Cache() {}
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/04/28 15:23:28  ucko
 * Restore mistakenly removed files.
 *
 * Revision 1.6  2003/09/23 16:40:24  kuznets
 * Fixed minor compilation errors
 *
 * Revision 1.5  2003/09/23 16:20:31  kuznets
 * "Doxygenized" header comments.
 *
 * Revision 1.4  2003/09/22 22:42:14  vakatov
 * Extended EKeepVersions.
 * + EKeepLastVersion -- for Purge().
 * Self-doc and style fixes.
 *
 * Revision 1.3  2003/09/22 19:15:13  kuznets
 * Added support of reader-writer interface (util/readerwriter.hpp)
 *
 * Revision 1.2  2003/09/22 18:40:34  kuznets
 * Minor cosmetics fixed
 *
 * Revision 1.1  2003/09/17 20:51:15  kuznets
 * Local cache interface - first revision
 * ===========================================================================
 */

#endif  /* UTIL___BLOB_CACHE__HPP */
