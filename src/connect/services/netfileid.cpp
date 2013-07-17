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

#include "pack_int.hpp"

#include <connect/services/netstorage.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_socket.h>

#include <corelib/ncbi_base64.h>

#include <time.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X  NetStorage_Common

#define DEFAULT_CACHE_CHUNK_SIZE (1024 * 1024)

BEGIN_NCBI_SCOPE

CNetFileID::CNetFileID(TNetStorageFlags flags, Uint8 random_number) :
    m_StorageFlags(flags),
    m_Fields(0),
    m_Timestamp(time(NULL)),
    m_Random(random_number),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetUniqueKeyFromRandom();
}

CNetFileID::CNetFileID(TNetStorageFlags flags,
            const string& app_domain,
            const string& unique_key) :
    m_StorageFlags(flags),
    m_Fields(fNFID_KeyAndNamespace),
    m_AppDomain(app_domain),
    m_UserKey(unique_key),
    m_CacheChunkSize(DEFAULT_CACHE_CHUNK_SIZE),
    m_Dirty(true)
{
    x_SetUniqueKeyFromUserDefinedKey();
}

#define SCRAMBLE_PASS() \
    pos = seq; \
    counter = seq_len - 1; \
    do { \
        pos[1] ^= *pos ^ length_factor--; \
        ++pos; \
    } while (--counter > 0);

static void s_Scramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = ((unsigned char) seq_len << 1) - 1;
        unsigned char* pos;
        size_t counter;

        SCRAMBLE_PASS();

        *seq ^= *pos ^ length_factor--;

        SCRAMBLE_PASS();
    }
}

#define UNSCRAMBLE_PASS() \
    counter = seq_len - 1; \
    do { \
        *pos ^= pos[-1] ^ ++length_factor; \
        --pos; \
    } while (--counter > 0);

static void s_Unscramble(unsigned char* seq, size_t seq_len)
{
    if (seq_len > 1) {
        unsigned char length_factor = 0;
        unsigned char* pos = seq + seq_len - 1;
        size_t counter;

        UNSCRAMBLE_PASS();

        pos = seq + seq_len - 1;

        *seq ^= *pos ^ ++length_factor;

        UNSCRAMBLE_PASS();
    }
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

    dst.assign((const char*) src, ptr - src);
    src_len -= len;
    return len;
}

CNetFileID::CNetFileID(const string& packed_id) :
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

    s_Unscramble(binary_id, binary_id_len);

    // 1. Check the signature.
    if (memcmp(binary_id, NET_FILE_ID_SIGNATURE,
            NET_FILE_ID_SIGNATURE_LEN) != 0) {
        CLEAN_UP_AND_THROW_INVALID_ID_ERROR(binary_id, packed_id);
    }

    unsigned char* ptr = binary_id + NET_FILE_ID_SIGNATURE_LEN;

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
        ptr += s_LoadString(m_UserKey, ptr, binary_id_len);
        // 4.2. Load the domain name.
        ptr += s_LoadString(m_AppDomain, ptr, binary_id_len);

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
        // 5.1. Load the service name.
        ptr += s_LoadString(m_NCServiceName, ptr, binary_id_len);
        // 5.2. Load the cache name.
        ptr += s_LoadString(m_CacheName, ptr, binary_id_len);
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

void CNetFileID::ClearNetICacheParams()
{
    m_Dirty = true;

    ClearFieldFlags(fNFID_NetICache);
}

void CNetFileID::SetNetICacheParams(const string& service_name,
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

void CNetFileID::x_SetUniqueKeyFromRandom()
{
    m_UniqueKey = NStr::NumericToString(m_Timestamp) + '_';
    m_UniqueKey.append(NStr::NumericToString(m_Random));
}

void CNetFileID::x_SetUniqueKeyFromUserDefinedKey()
{
    m_UniqueKey = m_AppDomain + '_';
    m_UniqueKey.append(m_UserKey);
}

void CNetFileID::Pack()
{
    size_t max_binary_id_len = MIN_BINARY_ID_LEN +
            (m_Fields & fNFID_KeyAndNamespace ?
                    m_UserKey.length() + m_AppDomain.length() :
                    sizeof(Uint8) + sizeof(Uint8)); // Timestamp and random.

    if (m_Fields & fNFID_NetICache)
        max_binary_id_len += m_NCServiceName.length() + 1 +
                m_CacheName.length() + 1 +
                sizeof(m_NetCacheIP) +
                sizeof(m_NetCachePort);

    if (m_StorageFlags & fNST_Cacheable)
        max_binary_id_len += sizeof(Uint8) + 1;

    if (m_Fields & fNFID_TTL)
        max_binary_id_len += sizeof(Uint8) + 1;

    unsigned char* binary_id = new unsigned char[max_binary_id_len];

    // 1. Save the signature.
    memcpy(binary_id, NET_FILE_ID_SIGNATURE, NET_FILE_ID_SIGNATURE_LEN);

    unsigned char* ptr = binary_id + NET_FILE_ID_SIGNATURE_LEN;

    // 2. Save the storage flags.
    *ptr++ = (unsigned char) m_StorageFlags;
    // 3. Remember which fields are stored.
    *ptr++ = m_Fields;

    // 4. Save file identification.
    if (m_Fields & fNFID_KeyAndNamespace) {
        // 4.1. Save the unique file key.
        memcpy(ptr, m_UserKey.c_str(), m_UserKey.length() + 1);
        ptr += m_UserKey.length() + 1;
        // 4.2. Save the domain name.
        memcpy(ptr, m_AppDomain.c_str(), m_AppDomain.length() + 1);
        ptr += m_AppDomain.length() + 1;
    } else {
        // 4.1. Save file creation timestamp.
        ptr += g_PackInteger(ptr, 9, m_Timestamp);
        // 4.2. Save the random ID.
        ptr += g_PackInteger(ptr, 9, m_Random);
    }

    // 5. (Optional) Save NetCache info.
    if (m_Fields & fNFID_NetICache) {
        // 5.1. Save the service name.
        memcpy(ptr, m_NCServiceName.c_str(), m_NCServiceName.length() + 1);
        ptr += m_NCServiceName.length() + 1;
        // 5.2. Save the cache name.
        memcpy(ptr, m_CacheName.c_str(), m_CacheName.length() + 1);
        ptr += m_CacheName.length() + 1;
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

    s_Scramble(binary_id, binary_id_len);

    base64url_encode(NULL, binary_id_len, NULL, 0, &packed_id_len);

    m_PackedID.resize(packed_id_len);

    m_PackedID[0] = '\0';

    base64url_encode(binary_id, binary_id_len,
            const_cast<char*>(m_PackedID.data()),
            packed_id_len, NULL);

    delete[] binary_id;

    m_Dirty = false;
}

CJsonNode CNetFileID::ToJSON() const
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

    root.SetByKey("StorageFlags", storage_flags);

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

    return root;
}

END_NCBI_SCOPE
