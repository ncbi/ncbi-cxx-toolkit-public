#ifndef MISC_NETSTORAGE__NETSTORAGE__HPP
#define MISC_NETSTORAGE__NETSTORAGE__HPP

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
 * Authors:  Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   A generic API for accessing heterogeneous storage services
 *   (direct serverless access to the backeend storage services).
 *
 */

/// @file netstorage_api.hpp
///

#include <connect/services/netstorage.hpp>
#include <corelib/ncbi_param.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetStorage
 *  @internal
 *
 * @{
 */


/// Direct serverless data object I/O
/// @see CNetStorageObject
class NCBI_XCONNECT_EXPORT CDirectNetStorageObject : public CNetStorageObject
{
public:
    /// Create uninitialized object
    ///
    CDirectNetStorageObject(EVoid);

    /// @see CNetStorage*::Relocate
    ///
    string Relocate(TNetStorageFlags flags);

    /// @see CNetStorage*::Exists
    ///
    bool Exists();

    /// @see CNetStorage*::Remove
    ///
    void Remove();

    /// Return object locator
    ///
    const CNetStorageObjectLoc& Locator();

private:
    CDirectNetStorageObject(SNetStorageObjectImpl* impl);
    friend class CDirectNetStorage;
    friend class CDirectNetStorageByKey;
};

/// Direct serverless BLOB storage API
/// @see CNetStorage
class NCBI_XCONNECT_EXPORT CDirectNetStorage : public CNetStorage
{
public:
    /// Construct a CDirectNetStorage object
    ///
    /// @param app_domain
    ///  Namespace for new objects.
    /// @param icache_client
    ///  NetCache service used for storing movable blobs and for caching
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    CDirectNetStorage(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    /// Create a new CDirectNetStorageObject object.
    ///
    CDirectNetStorageObject Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags);

    /// @see CNetStorage::Create
    ///
    CNetStorageObject Create(TNetStorageFlags flags = 0)
    {
        return CNetStorage::Create(flags);
    }

    /// @see CNetStorage::Open
    ///
    CDirectNetStorageObject Open(const string& object_loc);
};

/// Direct serverless BLOB storage API -- with access by user-defined keys
/// @see CNetStorageByKey
class NCBI_XCONNECT_EXPORT CDirectNetStorageByKey : public CNetStorageByKey
{
public:
    /// Construct a CDirectNetStorageByKey object.
    ///
    /// @param icache_client
    ///  NetCache service used for storing movable blobs and for caching
    /// @param app_domain
    ///  Namespace for the keys that will be created by this object.
    ///  If not specified, then the cache name from the 'icache_client' object
    ///  will be used.
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    CDirectNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    /// @see CNetStorageByKey::Open
    ///
    CDirectNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
};


/** @deprecated Use classes above instead.
 *
 * @{
 */

NCBI_DEPRECATED
CNetStorage g_CreateNetStorage(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

NCBI_DEPRECATED
CNetStorage g_CreateNetStorage(
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);


NCBI_DEPRECATED
CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

NCBI_DEPRECATED
CNetStorageByKey g_CreateNetStorageByKey(
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

NCBI_DEPRECATED
CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags);

NCBI_DEPRECATED
CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        TNetStorageFlags flags);

/* @} */

/* @} */


END_NCBI_SCOPE

#endif  /* MISC_NETSTORAGE__NETSTORAGE__HPP */
