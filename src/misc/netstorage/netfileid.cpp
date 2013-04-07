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

#include "filetrack.hpp"
#include "pack_int.hpp"

#include <misc/netstorage/error_codes.hpp>

#include <corelib/ncbi_base64.h>

#include <time.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X  NetStorage

#define DEFAULT_CACHE_CHUNK_SIZE (1024 * 1024)

BEGIN_NCBI_SCOPE

CNetFileID::CNetFileID(TNetStorageFlags flags, Uint8 random_number) :
    m_StorageFlags(flags),
    m_Fields(0),
    m_Timestamp(time(NULL)),
    m_Random(random_number),
    m_NetICacheClient(eVoid),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetUniqueKeyFromRandom();
}

CNetFileID::CNetFileID(TNetStorageFlags flags,
            const string& domain_name,
            const string& unique_key) :
    m_StorageFlags(flags),
    m_Fields(fNFID_KeyAndNamespace),
    m_DomainName(domain_name),
    m_Key(unique_key),
    m_NetICacheClient(eVoid),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetUniqueKeyFromUserDefinedKey();
}

#define NET_FILE_ID_SIGNATURE "NF"
#define NET_FILE_ID_SIGNATURE_LEN (sizeof(NET_FILE_ID_SIGNATURE) - 1)
#define MIN_BINARY_ID_LEN (NET_FILE_ID_SIGNATURE_LEN + \
        sizeof(unsigned char) + /* storage flags */ \
        sizeof(TNetFileIDFields) + /* field flags */ \
        1 + /* zero after the key or the ninth byte of the timestamp */ \
        1) /* zero after the namespace or the ninth byte of the random ID */

#define THROW_INVALID_ID_ERROR(packed_id) \
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg, \
                "Invalid NetFile ID '" << packed_id << '\'')

#define CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id) \
        delete[] binary_id; \
        THROW_INVALID_ID_ERROR(packed_id)

static size_t s_LoadString(string& dst, unsigned char* src, size_t& src_len)
{
    size_t len = 0;
    unsigned char* ptr = src;

    while (len < src_len) {
        ++len;
        if (*ptr == '\0')
            break;
        ++ptr;
    }

    dst.assign((const char*)src, len);
    src_len -= len;
    return len;
}

CNetFileID::CNetFileID(const string& packed_id) :
    m_NetICacheClient(eVoid),
    m_Dirty(false),
    m_PackedID(packed_id)
{
    size_t binary_id_len;
    base64url_decode(NULL, packed_id.length(), NULL, 0, &binary_id_len);

    if (binary_id_len < MIN_BINARY_ID_LEN) {
        THROW_INVALID_ID_ERROR(packed_id);
    }

    unsigned char* binary_id = new unsigned char[binary_id_len];

    if (base64url_decode(packed_id.data(), packed_id.length(),
            binary_id, binary_id_len, NULL) != eBase64_OK) {
        CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
    }

    unsigned char* ptr = binary_id;

    // 1. Check the signature.
    if (memcmp(binary_id, NET_FILE_ID_SIGNATURE,
            NET_FILE_ID_SIGNATURE_LEN) != 0) {
        CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
    }
    ptr += NET_FILE_ID_SIGNATURE_LEN;

    // 2. Restore the storage flags.
    m_StorageFlags = (TNetStorageFlags) *ptr++;
    // 3. Restore the field flags.
    m_Fields = (TNetFileIDFields) *ptr++;

    // Adjust the remaining length.
    binary_id_len -= NET_FILE_ID_SIGNATURE_LEN +
            sizeof(unsigned char) + sizeof(TNetFileIDFields);

    unsigned packed_int_len;

    // 4. Load file identification.
    if (m_Fields & fNFID_KeyAndNamespace) {
        // 4.1. Load the unique file key.
        ptr += s_LoadString(m_Key, ptr, binary_id_len);
        // 4.2. Load the domain name.
        ptr += s_LoadString(m_DomainName, ptr, binary_id_len);

        x_SetUniqueKeyFromUserDefinedKey();
    } else {
        // 4.1. Load file creation timestamp.
        packed_int_len = g_UnpackInteger(ptr, binary_id_len, &m_Timestamp);
        if (packed_int_len == 0 || binary_id_len <= packed_int_len) {
            CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
        }
        binary_id_len -= packed_int_len;
        ptr += packed_int_len;
        // 4.2. Load the random ID.
        packed_int_len = g_UnpackInteger(ptr, binary_id_len, &m_Random);
        if (packed_int_len == 0 || binary_id_len < packed_int_len) {
            CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
        }
        binary_id_len -= packed_int_len;
        ptr += packed_int_len;

        x_SetUniqueKeyFromRandom();
    }

    // 5. Load NetCache info.
    if (m_Fields & fNFID_NetICache) {
        string nc_service_name, cache_name;

        // 5.1. Load the service name.
        ptr += s_LoadString(nc_service_name, ptr, binary_id_len);
        // 5.2. Load the cache name.
        ptr += s_LoadString(cache_name, ptr, binary_id_len);
        // 5.3. Load the primary NetCache server IP.
        if (binary_id_len < sizeof(m_NetCacheIP) + sizeof(m_NetCachePort)) {
            CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
        }
        binary_id_len -= sizeof(m_NetCacheIP) + sizeof(m_NetCachePort);
        memcpy(&m_NetCacheIP, ptr, sizeof(m_NetCacheIP));
        ptr += sizeof(m_NetCacheIP);
        // 5.4. Load the primary NetCache server port.
        unsigned short port;
        memcpy(&port, ptr, sizeof(port));
        m_NetCachePort = SOCK_NetToHostShort(port);
        ptr += sizeof(port);

        m_NetICacheClient = CNetICacheClient(nc_service_name,
                cache_name, kEmptyStr);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (m_Fields & fNFID_AllowXSiteConn)
            m_NetICacheClient.GetService().AllowXSiteConnections();
#endif
    }

    // 6. If this file is cacheable, load the size of cache chunks.
    if (m_StorageFlags & fNST_Cacheable) {
        packed_int_len = g_UnpackInteger(ptr, binary_id_len, &m_CacheChunkSize);
        if (packed_int_len == 0 || binary_id_len < packed_int_len) {
            CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
        }
        binary_id_len -= packed_int_len;
        ptr += packed_int_len;
    }

    // 7. If TTL is in the ID, load it.
    if (m_Fields & fNFID_TTL) {
        packed_int_len = g_UnpackInteger(ptr, binary_id_len, &m_TTL);
        if (packed_int_len == 0 || binary_id_len < packed_int_len) {
            CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
        }
        // Because this is the last field, pointer adjustment is not needed:
        // binary_id_len -= packed_int_len;
        // ptr += packed_int_len;
    }

    delete[] binary_id;
}

void CNetFileID::SetNetICacheClient(CNetICacheClient::TInstance icache_client)
{
    m_Dirty = true;
    m_NetICacheClient = icache_client;

    if (icache_client == NULL)
        ClearFieldFlags(fNFID_NetICache);
    else {
        SetFieldFlags(fNFID_NetICache);

        CNetService service(m_NetICacheClient.GetService());

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (service.IsUsingXSiteProxy())
            SetFieldFlags(fNFID_AllowXSiteConn);
        else
            ClearFieldFlags(fNFID_AllowXSiteConn);
#endif

        CNetServer icache_server(service.Iterate().GetServer());

        m_NetCacheIP = icache_server.GetHost();
        m_NetCachePort = icache_server.GetPort();
    }
}

void CNetFileID::x_SetUniqueKeyFromRandom()
{
    m_UniqueKey = NStr::NumericToString(m_Timestamp) + '_';
    m_UniqueKey.append(NStr::NumericToString(m_Random));
}

void CNetFileID::x_SetUniqueKeyFromUserDefinedKey()
{
    m_UniqueKey = m_DomainName + '_';
    m_UniqueKey.append(m_Key);
}

void CNetFileID::Pack()
{
    size_t max_binary_id_len = MIN_BINARY_ID_LEN +
            (m_Fields & fNFID_KeyAndNamespace ?
                    m_Key.length() + m_DomainName.length() :
                    sizeof(Uint8) + sizeof(Uint8)); // Timestamp and random.

    string nc_service_name, cache_name;

    if (m_Fields & fNFID_NetICache) {
        nc_service_name = m_NetICacheClient.GetService().GetServiceName();
        cache_name = m_NetICacheClient.GetCacheName();
        max_binary_id_len += nc_service_name.length() + 1 +
                cache_name.length() + 1 +
                sizeof(m_NetCacheIP) +
                sizeof(m_NetCachePort);
    }

    if (m_StorageFlags & fNST_Cacheable)
        max_binary_id_len += sizeof(Uint8) + 1;

    if (m_Fields & fNFID_TTL)
        max_binary_id_len += sizeof(Uint8) + 1;

    unsigned char* binary_id = new unsigned char[max_binary_id_len];
    unsigned char* ptr = binary_id;

    // 1. Save the signature.
    memcpy(ptr, NET_FILE_ID_SIGNATURE, NET_FILE_ID_SIGNATURE_LEN);
    ptr += NET_FILE_ID_SIGNATURE_LEN;

    // 2. Save the storage flags.
    *ptr++ = (unsigned char) m_StorageFlags;
    // 3. Remember which fields are stored.
    *ptr++ = m_Fields;

    // 4. Save file identification.
    if (m_Fields & fNFID_KeyAndNamespace) {
        // 4.1. Save the unique file key.
        memcpy(ptr, m_Key.c_str(), m_Key.length() + 1);
        ptr += m_Key.length() + 1;
        // 4.2. Save the domain name.
        memcpy(ptr, m_DomainName.c_str(), m_DomainName.length() + 1);
        ptr += m_DomainName.length() + 1;
    } else {
        // 4.1. Save file creation timestamp.
        ptr += g_PackInteger(ptr, 9, m_Timestamp);
        // 4.2. Save the random ID.
        ptr += g_PackInteger(ptr, 9, m_Random);
    }

    // 5. (Optional) Save NetCache info.
    if (m_Fields & fNFID_NetICache) {
        // 5.1. Save the service name.
        memcpy(ptr, nc_service_name.c_str(), nc_service_name.length() + 1);
        ptr += nc_service_name.length() + 1;
        // 5.2. Save the cache name.
        memcpy(ptr, cache_name.c_str(), cache_name.length() + 1);
        ptr += cache_name.length() + 1;
        // 5.3. Save the primary NetCache server IP.
        memcpy(ptr, &m_NetCacheIP, sizeof(m_NetCacheIP));
        ptr += sizeof(m_NetCacheIP);
        // 5.4. Save the primary NetCache server port.
        unsigned short port = SOCK_HostToNetShort(m_NetCachePort);
        memcpy(ptr, &port, sizeof(port));
        ptr += sizeof(port);
    }

    // 6. If this file is cacheable, save the size of cache chunks.
    if (m_StorageFlags & fNST_Cacheable)
        ptr += g_PackInteger(ptr, 9, m_CacheChunkSize);

    // 7. Save the TTL if it's defined.
    if (m_Fields & fNFID_TTL)
        ptr += g_PackInteger(ptr, 9, m_TTL);

    // Now pack it all up.
    size_t binary_id_len = ptr - binary_id;
    size_t packed_id_len;

    base64url_encode(NULL, binary_id_len, NULL, 0, &packed_id_len);

    m_PackedID.resize(packed_id_len);

    base64url_encode(binary_id, binary_id_len,
            const_cast<char*>(m_PackedID.data()),
            packed_id_len, NULL);

    delete[] binary_id;

    m_Dirty = false;
}

END_NCBI_SCOPE
