#ifndef UTIL___INT_CACHE__HPP
#define UTIL___INT_CACHE__HPP

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
 * File Description: Int cache interface specs.
 *
 */

/// @file int_cache.hpp
/// Int cache interface specs. 
///
/// File describes interfaces used to create local cache of integer values
/// addressed by 2 keys. ( Cache formula: [Int,Int] ==> vector<int> )

#include <time.h>
#include <vector>


BEGIN_NCBI_SCOPE


/// Int cache read/write/maintanance interface.
///
/// IIntCache describes caching service for integer ids.
/// Cache formula: [Int,Int] ==> vector<int>
class IIntCache
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

    /// Add or replace cache entry
    ///
    /// @param key1 
    ///   Cache access key 1
    /// @param key2
    ///   Cache access key 2
    /// @param value
    ///   Cache value

    virtual void Store(int key1, int key2, const vector<int>& value) = 0;

    /// Get number of elements (not size in bytes!) in the cache entry
    /// @return
    ///   Number of elements in the cached vector, 0 if cache does not exist
    virtual size_t GetSize(int key1, int key2) = 0;

    /// Read cache entry.
    /// @return FALSE if BLOB doesn't exist
    virtual bool Read(int key1, int key2, vector<int>& value) = 0;

    /// Remove cache entry
    virtual void Remove(int key1, int key2) = 0;

    /// Set cache item expiration timeout.
    virtual void SetExpirationTime(time_t expiration_timeout) = 0;

    /// Clean the cache according to the caching policy
    /// (implementation dependent)
    virtual void Purge(time_t           time_point,
                       EKeepVersions    keep_last_version = eDropAll) = 0;



    virtual ~IIntCache() {}
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/04/28 15:23:28  ucko
 * Restore mistakenly removed files.
 *
 * Revision 1.3  2003/10/15 19:31:04  kuznets
 * Minor fix
 *
 * Revision 1.2  2003/10/15 18:56:43  kuznets
 * Minor comment change
 *
 *
 * ===========================================================================
 */

#endif  /* UTIL___INT_CACHE__HPP */
