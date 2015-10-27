#ifndef MISC_NETSTORAGE_IMPL__NETSTORAGE_INT__HPP
#define MISC_NETSTORAGE_IMPL__NETSTORAGE_INT__HPP

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
 */

#include <connect/services/netstorage.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/ncbi_param.hpp>

BEGIN_NCBI_SCOPE

/// @internal
class CDirectNetStorageObject : public CNetStorageObject
{
public:
    CDirectNetStorageObject(EVoid);
    string Relocate(TNetStorageFlags flags);
    bool Exists();
    void Remove();
    const CNetStorageObjectLoc& Locator();

private:
    CDirectNetStorageObject(SNetStorageObjectImpl* impl);
    friend class CDirectNetStorage;
    friend class CDirectNetStorageByKey;
};

/// @internal
struct SFileTrackConfig
{
    const string site;
    const string key;
    const STimeout read_timeout;
    const STimeout write_timeout;

    SFileTrackConfig(EVoid); // Means no FileTrack as a backend storage
    SFileTrackConfig(const IRegistry& registry, const string& section = kEmptyStr);
    SFileTrackConfig(const string& site, const string& key);
};

/// @internal
class CDirectNetStorage : public CNetStorage
{
public:
    CDirectNetStorage(
        const IRegistry&            registry,
        const string&               service_name,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorage(
        const SFileTrackConfig&     filetrack_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorageObject Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags);

    CNetStorageObject Create(TNetStorageFlags flags = 0)
    {
        return CNetStorage::Create(flags);
    }

    CDirectNetStorageObject Open(const string& object_loc);
};

/// @internal
class CDirectNetStorageByKey : public CNetStorageByKey
{
public:
    CDirectNetStorageByKey(
        const IRegistry&            registry,
        const string&               service_name,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorageByKey(
        const SFileTrackConfig&     filetrack_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
};

END_NCBI_SCOPE

#endif
