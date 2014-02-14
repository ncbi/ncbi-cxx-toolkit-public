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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_socket.h>

#include <corelib/ncbi_base64.h>
#include <corelib/ncbiargs.hpp>

#include <time.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X  NetStorage_Common

#define DEFAULT_CACHE_CHUNK_SIZE (1024 * 1024)

BEGIN_NCBI_SCOPE

EFileTrackSite g_StringToFileTrackSite(const char* ft_site_name)
{
    if (strcmp(ft_site_name, "submit") == 0 ||
            strcmp(ft_site_name, "prod") == 0)
        return eFileTrack_ProdSite;
    else if (strcmp(ft_site_name, "dsubmit") == 0 ||
            strcmp(ft_site_name, "dev") == 0)
        return eFileTrack_DevSite;
    else if (strcmp(ft_site_name, "qsubmit") == 0 ||
            strcmp(ft_site_name, "qa") == 0)
        return eFileTrack_QASite;
    else {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
                "unrecognized FileTrack site '" << ft_site_name << '\'');
    }
}

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        TNetStorageFlags flags, Uint8 random_number,
        const char* ft_site_name) :
    m_CompoundIDPool(cid_pool),
    m_StorageFlags(flags),
    m_Fields(0),
    m_ObjectID(0),
    m_Timestamp(time(NULL)),
    m_Random(random_number),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetFileTrackSite(ft_site_name);
    x_SetUniqueKeyFromRandom();
}

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        TNetStorageFlags flags,
        const string& app_domain,
        const string& unique_key,
        const char* ft_site_name) :
    m_CompoundIDPool(cid_pool),
    m_StorageFlags(flags),
    m_Fields(fNFID_KeyAndNamespace),
    m_ObjectID(0),
    m_AppDomain(app_domain),
    m_UserKey(unique_key),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetFileTrackSite(ft_site_name);
    x_SetUniqueKeyFromUserDefinedKey();
}

#define THROW_INVALID_LOC_ERROR(object_loc) \
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg, \
                "Invalid NetStorage object locator '" << (object_loc) << '\'')

#define VERIFY_FIELD_EXISTS(field) \
        if (!(field)) { \
            THROW_INVALID_LOC_ERROR(object_loc); \
        }

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        const string& object_loc) :
    m_CompoundIDPool(cid_pool),
    m_Dirty(false),
    m_Locator(object_loc)
{
    CCompoundID cid = m_CompoundIDPool.FromString(object_loc);

    // 1. Check the ID class.
    if (cid.GetClass() != eCIC_NetStorageObjectLoc) {
        THROW_INVALID_LOC_ERROR(object_loc);
    }

    // 2. Restore the storage flags.
    CCompoundIDField field = cid.GetFirst(eCIT_Flags);
    VERIFY_FIELD_EXISTS(field);
    m_StorageFlags = (TNetStorageFlags) field.GetFlags();

    // 3. Restore the field flags.
    VERIFY_FIELD_EXISTS(field = field.GetNextHomogeneous());
    m_Fields = (TNetStorageObjectLocFields) field.GetFlags();

    if ((m_StorageFlags & fNST_NoMetaData) == 0) {
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_ObjectID = field.GetID();
    }

    // 4. Restore file identification.
    if (m_Fields & fNFID_KeyAndNamespace) {
        // 4.1. Get the unique file key.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_UserKey = field.GetString();
        // 4.2. Get the domain name.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_AppDomain = field.GetString();

        x_SetUniqueKeyFromUserDefinedKey();
    } else {
        // 4.1. Get file creation timestamp.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Timestamp = field.GetTimestamp();
        // 4.2. Get the random ID.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Random = (Uint8) field.GetRandom() << (sizeof(Uint4) * 8);
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Random |= field.GetRandom();

        x_SetUniqueKeyFromRandom();
    }

    // 5. Restore NetCache info.
    if (m_Fields & fNFID_NetICache) {
        // 5.1. Get the service name.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_NCServiceName = field.GetServiceName();
        // 5.2. Get the cache name.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_CacheName = field.GetDatabaseName();
        // 5.3. Get the primary NetCache server IP and port.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_NetCacheIP = field.GetIPv4Address();
        m_NetCachePort = field.GetPort();
    }

    // 6. If this file is cacheable, load the size of cache chunks.
    if (m_StorageFlags & fNST_Cacheable) {
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_CacheChunkSize = (Uint8) field.GetInteger();
    }

    // 7. Restore the TTL if it's in the key.
    if (m_Fields & fNFID_TTL) {
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_TTL = (Uint8) field.GetInteger();
    }
}

void CNetStorageObjectLoc::ClearNetICacheParams()
{
    m_Dirty = true;

    ClearFieldFlags(fNFID_NetICache);
}

void CNetStorageObjectLoc::SetNetICacheParams(const string& service_name,
        const string& cache_name, Uint4 server_ip, unsigned short server_port
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        , bool allow_xsite_conn
#endif
        )
{
    m_Dirty = true;

    SetFieldFlags(fNFID_NetICache);

    m_NCServiceName = service_name;
    m_CacheName = cache_name;
    m_NetCacheIP = server_ip;
    m_NetCachePort = server_port;

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    if (allow_xsite_conn)
        SetFieldFlags(fNFID_AllowXSiteConn);
    else
        ClearFieldFlags(fNFID_AllowXSiteConn);
#endif
}

void CNetStorageObjectLoc::SetFileTrackSite(EFileTrackSite ft_site)
{
    m_Dirty = true;

    m_Fields &= ~(unsigned char) (fNFID_FileTrackDev | fNFID_FileTrackQA);

    if (ft_site == eFileTrack_DevSite)
        m_Fields |= fNFID_FileTrackDev;
    else if (ft_site == eFileTrack_QASite)
        m_Fields |= fNFID_FileTrackQA;
}

EFileTrackSite CNetStorageObjectLoc::GetFileTrackSite()
{
    return m_Fields & fNFID_FileTrackDev ? eFileTrack_DevSite :
            m_Fields & fNFID_FileTrackQA ? eFileTrack_QASite :
                    eFileTrack_ProdSite;
}

string CNetStorageObjectLoc::GetFileTrackURL()
{
    switch (GetFileTrackSite()) {
    default:
        return "https://submit.ncbi.nlm.nih.gov";
        break;
    case eFileTrack_DevSite:
        return "https://dsubmit.ncbi.nlm.nih.gov";
        break;
    case eFileTrack_QASite:
        return "https://qsubmit.ncbi.nlm.nih.gov";
    }
}

void CNetStorageObjectLoc::x_SetFileTrackSite(const char* ft_site_name)
{
    switch (g_StringToFileTrackSite(ft_site_name)) {
    case eFileTrack_DevSite:
        m_Fields |= fNFID_FileTrackDev;
        break;
    case eFileTrack_QASite:
        m_Fields |= fNFID_FileTrackQA;
    default:
        break;
    }
}

void CNetStorageObjectLoc::x_SetUniqueKeyFromRandom()
{
    m_UniqueKey = NStr::NumericToString(m_Timestamp) + '_';
    m_UniqueKey.append(NStr::NumericToString(m_Random));
}

void CNetStorageObjectLoc::x_SetUniqueKeyFromUserDefinedKey()
{
    m_UniqueKey = m_AppDomain + '_';
    m_UniqueKey.append(m_UserKey);
}

void CNetStorageObjectLoc::x_Pack()
{
    // 1. Allocate a new CompoundID object.
    CCompoundID cid = m_CompoundIDPool.NewID(eCIC_NetStorageObjectLoc);

    // 2. Save the storage flags.
    cid.AppendFlags(m_StorageFlags);
    // 3. Remember which fields are stored.
    cid.AppendFlags(m_Fields);

    // 4. Save file identification.
    if ((m_StorageFlags & fNST_NoMetaData) == 0)
        cid.AppendID(m_ObjectID);

    if (m_Fields & fNFID_KeyAndNamespace) {
        // 4.1. Save the unique file key.
        cid.AppendString(m_UserKey);
        // 4.2. Save the domain name.
        cid.AppendString(m_AppDomain);
    } else {
        // 4.1. Save file creation timestamp.
        cid.AppendTimestamp(m_Timestamp);
        // 4.2. Save the random ID.
        cid.AppendRandom(m_Random >> (sizeof(Uint4) * 8));
        cid.AppendRandom((Uint4) m_Random);
    }

    // 5. (Optional) Save NetCache info.
    if (m_Fields & fNFID_NetICache) {
        // 5.1. Save the service name.
        cid.AppendServiceName(m_NCServiceName);
        // 5.2. Save the cache name.
        cid.AppendDatabaseName(m_CacheName);
        // 5.3. Save the primary NetCache server IP and port.
        cid.AppendIPv4SockAddr(m_NetCacheIP, m_NetCachePort);
    }

    // 6. If this file is cacheable, save the size of cache chunks.
    if (m_StorageFlags & fNST_Cacheable)
        cid.AppendInteger((Int8) m_CacheChunkSize);

    // 7. Save the TTL if it's defined.
    if (m_Fields & fNFID_TTL)
        cid.AppendInteger((Int8) m_TTL);

    // Now pack it all up.
    m_Locator = cid.ToString();

    m_Dirty = false;
}

CJsonNode CNetStorageObjectLoc::ToJSON() const
{
    CJsonNode root(CJsonNode::NewObjectNode());

    root.SetInteger("Version", 1);

    CJsonNode storage_flags(CJsonNode::NewObjectNode());

    storage_flags.SetBoolean("Fast",
            (m_StorageFlags & fNST_Fast) != 0);
    storage_flags.SetBoolean("Persistent",
            (m_StorageFlags & fNST_Persistent) != 0);
    storage_flags.SetBoolean("Movable",
            (m_StorageFlags & fNST_Movable) != 0);
    storage_flags.SetBoolean("Cacheable",
            (m_StorageFlags & fNST_Cacheable) != 0);
    storage_flags.SetBoolean("NoMetaData",
            (m_StorageFlags & fNST_NoMetaData) != 0);

    root.SetByKey("StorageFlags", storage_flags);

    if ((m_StorageFlags & fNST_NoMetaData) == 0)
        root.SetInteger("ObjectID", (Int8) m_ObjectID);

    if (m_Fields & fNFID_KeyAndNamespace) {
        root.SetString("AppDomain", m_AppDomain);
        root.SetString("UserKey", m_UserKey);
    } else {
        root.SetInteger("Timestamp", (Int8) m_Timestamp);
        root.SetInteger("Random", (Int8) m_Random);
    }

    if (m_Fields & fNFID_NetICache) {
        CJsonNode icache_params(CJsonNode::NewObjectNode());

        icache_params.SetString("ServiceName", m_NCServiceName);
        icache_params.SetString("CacheName", m_CacheName);
        icache_params.SetString("ServerHost",
                g_NetService_gethostnamebyaddr(m_NetCacheIP));
        icache_params.SetInteger("ServerPort", m_NetCachePort);
        icache_params.SetBoolean("AllowXSiteConn",
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
                m_Fields & fNFID_AllowXSiteConn ? true :
#endif
                false);

        root.SetByKey("ICache", icache_params);
    }

    if (m_StorageFlags & fNST_Cacheable)
        root.SetInteger("CacheChunkSize", (Int8) m_CacheChunkSize);

    if (m_Fields & fNFID_TTL)
        root.SetInteger("TTL", (Int8) m_TTL);

    if (m_Fields & (fNFID_FileTrackDev | fNFID_FileTrackQA))
        root.SetString("FileTrackSite",
                m_Fields & fNFID_FileTrackDev ? "dev" : "qa");

    return root;
}

END_NCBI_SCOPE
