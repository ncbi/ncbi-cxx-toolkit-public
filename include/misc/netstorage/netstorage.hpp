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
#include <connect/services/neticache_client.hpp>

#include <corelib/ncbi_param.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetStorage
 *
 * @{
 */

/// Construct a CNetStorage object
///
/// @param icache_client
///  NetCache service used for storing movable blobs and for caching
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorage g_CreateNetStorage(
        CNetICacheClient::TInstance icache_client,
        TNetStorageFlags            default_flags = 0);

/// Construct a CNetStorage object for use with FileTrack only.
///
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorage g_CreateNetStorage(TNetStorageFlags default_flags);


/// Construct a CNetStorageByKey object.
///
/// @param icache_client
///  NetCache service used for storing movable blobs and for caching
/// @param domain_name
///  Namespace for the keys that will be created by this object.
///  If not specified, then the cache name from the 'icache_client' object
///  will be used.
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string&               domain_name   = kEmptyStr,
        TNetStorageFlags            default_flags = 0);

/// Construct a CNetStorageByKey object for use with FileTrack only.
///
/// @param domain_name
///  Namespace for the keys that will be created by this object
/// @param default_flags
///  Default storage preferences for files created by this object.
///
CNetStorageByKey g_CreateNetStorageByKey(
        const string&    domain_name,
        TNetStorageFlags default_flags = 0);

/// @internal
void g_SetNetICacheParams(CNetFileID& file_id, CNetICacheClient icache_client);

NCBI_PARAM_DECL(string, filetrack, site);
typedef NCBI_PARAM_TYPE(filetrack, site) TFileTrack_Site;

NCBI_PARAM_DECL(string, filetrack, api_key);
typedef NCBI_PARAM_TYPE(filetrack, api_key) TFileTrack_APIKey;

/* @} */


END_NCBI_SCOPE

#endif  /* MISC_NETSTORAGE__NETSTORAGE__HPP */
