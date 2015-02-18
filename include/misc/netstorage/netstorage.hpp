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
 * Authors:  Dmitry Kazimirov
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
 *
 * @{
 */

// TODO:
// Replace following functions with CDirectNetStorage[ByKey] classes
// These classes will have additional
// 1) Constructors, with merge of similar into one (icache_client = NULL)
// 2) Create methods
// these will replace everything below

/// Construct a CNetStorage object
///
/// @param app_domain
///  Namespace for new objects.
/// @param icache_client
///  NetCache service used for storing movable blobs and for caching
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorage g_CreateNetStorage(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

/// Construct a CNetStorage object for use with FileTrack only.
///
/// @param app_domain
///  Namespace for new objects.
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorage g_CreateNetStorage(
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);


/// Construct a CNetStorageByKey object.
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
CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

/// Construct a CNetStorageByKey object for use with FileTrack only.
///
/// @param app_domain
///  Namespace for the keys that will be created by this object
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorageByKey g_CreateNetStorageByKey(
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);



/// @internal
CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags);

/// @internal
CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        TNetStorageFlags flags);

/* @} */


END_NCBI_SCOPE

#endif  /* MISC_NETSTORAGE__NETSTORAGE__HPP */
