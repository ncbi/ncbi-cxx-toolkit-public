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

#include <util/random_gen.hpp>
#include <util/util_exception.hpp>

#include <corelib/ncbi_base64.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X  NetStorage

#define DEFAULT_CACHE_CHUNK_SIZE (1024 * 1024)
#define RELOCATION_BUFFER_SIZE (128 * 1024)
#define READ_CHUNK_SIZE (4 * 1024)

BEGIN_NCBI_SCOPE

#define THROW_IO_EXCEPTION(err_code, message, status) \
        NCBI_THROW_FMT(CIOException, err_code, message << IO_StatusStr(status));

const char* CNetStorageException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidArg:
        return "eInvalidArg";
    case eNotExist:
        return "eNotExist";
    case eIOError:
        return "eIOError";
    case eTimeout:
        return "eTimeout";
    default:
        return CException::GetErrCodeString();
    }
}

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

    dst.assign(src, ptr);
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

        if (m_Fields & fNFID_AllowXSiteConn)
            m_NetICacheClient.GetService().AllowXSiteConnections();
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

        if (service.IsUsingXSiteProxy())
            SetFieldFlags(fNFID_AllowXSiteConn);
        else
            ClearFieldFlags(fNFID_AllowXSiteConn);

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

struct NCBI_XCONNECT_EXPORT SNetStorageImpl : public CObject
{
    SNetStorageImpl(CNetICacheClient::TInstance icache_client,
            TNetStorageFlags default_flags);

    TNetStorageFlags GetDefaultFlags(TNetStorageFlags flags)
    {
        return flags != 0 ? flags : m_DefaultFlags;
    }

    CNetICacheClient m_NetICacheClient;
    TNetStorageFlags m_DefaultFlags;

    SFileTrackAPI m_FileTrackAPI;
};

SNetStorageImpl::SNetStorageImpl(CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags) :
    m_NetICacheClient(icache_client),
    m_DefaultFlags(default_flags)
{
}

CNetStorage::CNetStorage(TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageImpl(NULL, default_flags))
{
}

CNetStorage::CNetStorage(CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageImpl(icache_client, default_flags))
{
}

enum ENetFileLocation {
    eNFL_Unknown,
    eNFL_NetCache,
    eNFL_FileTrack,
    eNFL_NotFound
};

enum ENetFileIOStatus {
    eNFS_Closed,
    eNFS_WritingToNetCache,
    eNFS_ReadingFromNetCache,
    eNFS_WritingToFileTrack,
    eNFS_ReadingFromFileTrack,
};

struct NCBI_XCONNECT_EXPORT SNetFileImpl : public CObject, public IReaderWriter
{
    SNetFileImpl(SNetStorageImpl* storage_impl,
            TNetStorageFlags flags,
            Uint8 random_number,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_FileID(flags, random_number),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        m_FileID.SetNetICacheClient(icache_client);
    }

    SNetFileImpl(SNetStorageImpl* storage_impl, const string& file_id) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_FileID(file_id),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetFileImpl(SNetStorageImpl* storage_impl,
            TNetStorageFlags flags,
            const string& domain_name,
            const string& unique_key,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_FileID(flags, domain_name, unique_key),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        m_FileID.SetNetICacheClient(icache_client);
    }

    SNetFileImpl(SNetStorageImpl* storage_impl, const CNetFileID& file_id) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_FileID(file_id),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    bool SetNetICacheClient();
    void DemandNetCache();

    const ENetFileLocation* GetPossibleFileLocations(); // For reading.
    ENetFileLocation ChooseLocation(); // For writing.

    ERW_Result s_ReadFromNetCache(char* buf, size_t count, size_t* bytes_read);

    bool s_TryReadLocation(ENetFileLocation location, ERW_Result* rw_res,
            char* buf, size_t count, size_t* bytes_read);

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);

    bool Eof();

    virtual ERW_Result PendingCount(size_t* count);

    virtual ERW_Result Write(const void* buf,
            size_t count, size_t* bytes_written = 0);

    virtual ERW_Result Flush();

    void Close();

    virtual ~SNetFileImpl();

    CNetStorage m_NetStorage;
    CNetICacheClient m_NetICacheClient;

    CNetFileID m_FileID;
    ENetFileLocation m_CurrentLocation;
    ENetFileIOStatus m_IOStatus;

    auto_ptr<IEmbeddedStreamWriter> m_NetCacheWriter;
    auto_ptr<IReader> m_NetCacheReader;
    size_t m_NetCache_BlobSize;
    size_t m_NetCache_BytesRead;

    CRef<SFileTrackRequest> m_FileTrackRequest;
};

bool SNetFileImpl::SetNetICacheClient()
{
    if (!m_NetICacheClient) {
        if (m_FileID.GetNetICacheClient())
            m_NetICacheClient = m_FileID.GetNetICacheClient();
        else if (m_NetStorage->m_NetICacheClient)
            m_NetICacheClient = m_NetStorage->m_NetICacheClient;
        else
            return false;
    }
    return true;
}

void SNetFileImpl::DemandNetCache()
{
    if (!SetNetICacheClient()) {
        NCBI_THROW(CNetStorageException, eInvalidArg,
                "The given storage preferences require a NetCache server.");
    }
}

static const ENetFileLocation s_TryNetCacheThenFileTrack[] =
        {eNFL_NetCache, eNFL_FileTrack, eNFL_NotFound};

static const ENetFileLocation s_NetCache[] =
        {eNFL_NetCache, eNFL_NotFound};

static const ENetFileLocation s_TryFileTrackThenNetCache[] =
        {eNFL_FileTrack, eNFL_NetCache, eNFL_NotFound};

static const ENetFileLocation s_FileTrack[] =
        {eNFL_FileTrack, eNFL_NotFound};

const ENetFileLocation* SNetFileImpl::GetPossibleFileLocations()
{
    TNetStorageFlags flags = m_FileID.GetStorageFlags();

    if (flags == 0)
        // Just guessing.
        return SetNetICacheClient() ? s_NetCache : s_FileTrack;

    if (flags & fNST_Movable) {
        if (!SetNetICacheClient())
            return s_FileTrack;

        return flags & fNST_Fast ? s_TryNetCacheThenFileTrack :
                s_TryFileTrackThenNetCache;
    }

    if (flags & fNST_Persistent)
        return s_FileTrack;

    DemandNetCache();
    return s_NetCache;
}

ENetFileLocation SNetFileImpl::ChooseLocation()
{
    TNetStorageFlags flags = m_FileID.GetStorageFlags();

    if (flags == 0)
        return SetNetICacheClient() ? eNFL_NetCache : eNFL_FileTrack;

    if (flags & fNST_Persistent)
        return eNFL_FileTrack;

    DemandNetCache();
    return eNFL_NetCache;
}

ERW_Result SNetFileImpl::s_ReadFromNetCache(char* buf,
        size_t count, size_t* bytes_read)
{
    size_t iter_bytes_read;
    size_t total_bytes_read = 0;
    ERW_Result rw_res = eRW_Success;

    while (count > 0) {
        rw_res = m_NetCacheReader->Read(buf, count, &iter_bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += iter_bytes_read;
            buf += iter_bytes_read;
            count -= iter_bytes_read;
        } else if (rw_res == eRW_Eof)
            break;
        else {
            NCBI_THROW(CNetStorageException, eIOError,
                    "I/O error while reading NetCache BLOB");
        }
    }

    m_NetCache_BytesRead += total_bytes_read;

    if (bytes_read != NULL)
        *bytes_read = total_bytes_read;

    return rw_res;
}

bool SNetFileImpl::s_TryReadLocation(ENetFileLocation location,
        ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read)
{
    if (location == eNFL_NetCache) {
        m_NetCacheReader.reset(m_NetICacheClient.GetReadStream(
                m_FileID.GetUniqueKey(), 0, kEmptyStr,
                &m_NetCache_BlobSize, CNetCacheAPI::eCaching_Disable));

        if (m_NetCacheReader.get() != NULL) {
            m_NetCache_BytesRead = 0;
            *rw_res = s_ReadFromNetCache(buf, count, bytes_read);
            m_CurrentLocation = eNFL_NetCache;
            m_IOStatus = eNFS_ReadingFromNetCache;
            return true;
        }
    } else { /* location == eNFL_FileTrack */
        m_FileTrackRequest =
                m_NetStorage->m_FileTrackAPI.StartDownload(&m_FileID);

        try {
            *rw_res = m_FileTrackRequest->Read(buf, count, bytes_read);
            m_CurrentLocation = eNFL_FileTrack;
            m_IOStatus = eNFS_ReadingFromFileTrack;
            return true;
        }
        catch (CNetStorageException& e) {
            m_FileTrackRequest.Reset();

            if (e.GetErrCode() != CNetStorageException::eNotExist)
                throw;
        }
    }

    return false;
}

ERW_Result SNetFileImpl::Read(void* buf, size_t count, size_t* bytes_read)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        {
            ERW_Result rw_res = eRW_Success;

            if (m_CurrentLocation == eNFL_Unknown) {
                const ENetFileLocation* location = GetPossibleFileLocations();

                do
                    if (s_TryReadLocation(*location, &rw_res,
                            reinterpret_cast<char*>(buf), count, bytes_read))
                        return rw_res;
                while (*++location != eNFL_NotFound);
            } else if (s_TryReadLocation(m_CurrentLocation, &rw_res,
                    reinterpret_cast<char*>(buf), count, bytes_read))
                return rw_res;
        }

        NCBI_THROW_FMT(CNetStorageException, eNotExist,
                "Cannot open \"" << m_FileID.GetID() << "\" for reading.");

    case eNFS_ReadingFromNetCache:
        return s_ReadFromNetCache(reinterpret_cast<char*>(buf),
                count, bytes_read);

    case eNFS_ReadingFromFileTrack:
        m_FileTrackRequest->Read(buf, count, bytes_read);
        break;

    default: /* eNFS_WritingToNetCache or eNFS_WritingToFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot read while writing.");
    }

    return eRW_Success;
}

bool SNetFileImpl::Eof()
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        return false;

    case eNFS_ReadingFromNetCache:
        return m_NetCache_BytesRead >= m_NetCache_BlobSize;

    case eNFS_ReadingFromFileTrack:
        return m_FileTrackRequest->m_HTTPStream.eof();

    default: /* eNFS_WritingToNetCache or eNFS_WritingToFileTrack */
        break;
    }

    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot check EOF status while writing.");
}

ERW_Result SNetFileImpl::PendingCount(size_t* count)
{
    *count = 0;

    return eRW_NotImplemented;
}

ERW_Result SNetFileImpl::Write(const void* buf, size_t count,
        size_t* bytes_written)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        if (m_CurrentLocation == eNFL_Unknown)
            m_CurrentLocation = ChooseLocation();

        if (m_CurrentLocation == eNFL_NetCache) {
            m_NetCacheWriter.reset(m_NetICacheClient.GetNetCacheWriter(
                    m_FileID.GetUniqueKey(), 0, kEmptyStr,
                            m_FileID.GetFields() & fNFID_TTL ?
                                    (unsigned) m_FileID.GetTTL() : 0));

            m_IOStatus = eNFS_WritingToNetCache;

            return m_NetCacheWriter->Write(buf, count, bytes_written);
        } else {
            m_FileTrackRequest =
                    m_NetStorage->m_FileTrackAPI.StartUpload(&m_FileID);

            m_IOStatus = eNFS_WritingToFileTrack;

            m_FileTrackRequest->Write(buf, count, bytes_written);
        }
        break;

    case eNFS_WritingToNetCache:
        return m_NetCacheWriter->Write(buf, count, bytes_written);

    case eNFS_WritingToFileTrack:
        m_FileTrackRequest->Write(buf, count, bytes_written);
        break;

    default: /* eNFS_ReadingToNetCache or eNFS_ReadingFromFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot write while reading.");
    }

    return eRW_Success;
}

ERW_Result SNetFileImpl::Flush()
{
    return eRW_Success;
}

void SNetFileImpl::Close()
{
    switch (m_IOStatus) {
    default: /* case eNFS_Closed: */
        break;

    case eNFS_WritingToNetCache:
        m_IOStatus = eNFS_Closed;
        m_NetCacheWriter->Close();
        m_NetCacheWriter.reset();
        break;

    case eNFS_ReadingFromNetCache:
        m_IOStatus = eNFS_Closed;
        m_NetCacheReader.reset();
        break;

    case eNFS_WritingToFileTrack:
        m_IOStatus = eNFS_Closed;
        m_FileTrackRequest->FinishUpload();
        m_FileTrackRequest.Reset();
        break;

    case eNFS_ReadingFromFileTrack:
        m_IOStatus = eNFS_Closed;
        m_FileTrackRequest->FinishDownload();
        m_FileTrackRequest.Reset();
    }
}

SNetFileImpl::~SNetFileImpl()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a file in ~SNetFileImpl()");
}

string CNetFile::GetID()
{
    return m_Impl->m_FileID.GetID();
}

size_t CNetFile::Read(void* buffer, size_t buf_size)
{
    size_t bytes_read;

    m_Impl->Read(buffer, buf_size, &bytes_read);

    return bytes_read;
}

void CNetFile::Read(string* data)
{
    data->resize(data->capacity() == 0 ? READ_CHUNK_SIZE : data->capacity());

    size_t bytes_read = Read(const_cast<char*>(data->data()), data->capacity());

    data->resize(bytes_read);

    if (bytes_read < data->capacity())
        return;

    vector<string> chunks(1, *data);

    while (!Eof()) {
        string buffer(READ_CHUNK_SIZE, '\0');

        bytes_read = Read(const_cast<char*>(buffer.data()), buffer.length());

        if (bytes_read < buffer.length()) {
            buffer.resize(bytes_read);
            chunks.push_back(buffer);
            break;
        }

        chunks.push_back(buffer);
    }

    *data = NStr::Join(chunks, kEmptyStr);
}

bool CNetFile::Eof()
{
    return m_Impl->Eof();
}

void CNetFile::Write(const void* buffer, size_t buf_size)
{
    m_Impl->Write(buffer, buf_size);
}

void CNetFile::Close()
{
    m_Impl->Close();
}

CNetFile CNetStorage::Create(TNetStorageFlags flags)
{
    Uint8 random_number;

#ifndef NCBI_OS_MSWIN
    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd < 0) {
        NCBI_USER_THROW_FMT("Cannot open /dev/urandom: " <<
                strerror(errno));
    }

    read(urandom_fd, &random_number, sizeof(random_number));
    close(urandom_fd);
#else
    random_number = m_Impl->m_FileTrackAPI.GetRandom();
#endif // NCBI_OS_MSWIN

    CNetFile netfile(new SNetFileImpl(m_Impl, m_Impl->GetDefaultFlags(flags),
            random_number, m_Impl->m_NetICacheClient));

    return netfile;
}

CNetFile CNetStorage::Open(const string& file_id, TNetStorageFlags flags)
{
    CNetFile netfile(new SNetFileImpl(m_Impl, file_id));

    if (flags != 0)
        netfile->m_FileID.SetStorageFlags(flags);

    return netfile;
}

string CNetStorage::Relocate(const string& file_id, TNetStorageFlags flags)
{
    if (flags == 0)
        return file_id;

    CNetFile orig_file(new SNetFileImpl(m_Impl, file_id));

    if ((orig_file->m_FileID.GetStorageFlags() & fNST_Movable) == 0) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "File \"" << file_id << "\" is not movable.");
    }

    if (orig_file->m_FileID.GetStorageFlags() == flags)
        return file_id;

    CNetFile new_file(new SNetFileImpl(m_Impl, orig_file->m_FileID));

    new_file->m_FileID.SetStorageFlags(flags);

    char buffer[RELOCATION_BUFFER_SIZE];
    size_t bytes_read;

    while (!orig_file.Eof()) {
        bytes_read = orig_file.Read(buffer, sizeof(buffer));
        new_file.Write(buffer, bytes_read);
    }

    new_file.Close();
    orig_file.Close();

    if (orig_file->m_CurrentLocation == eNFL_NetCache)
        orig_file->m_NetICacheClient.Remove(
                orig_file->m_FileID.GetUniqueKey(), 0, kEmptyStr);

    // TODO Delete orig_file from FileTrack.

    return new_file->m_FileID.GetID();
}

struct SNetStorageByKeyImpl : public SNetStorageImpl
{
    SNetStorageByKeyImpl(CNetICacheClient::TInstance icache_client,
            const string& domain_name, TNetStorageFlags default_flags) :
        SNetStorageImpl(icache_client, default_flags),
        m_DomainName(domain_name)
    {
        if (domain_name.empty()) {
            if (icache_client != NULL)
                m_DomainName = m_NetICacheClient.GetCacheName();
            else {
                NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                        "Domain name cannot be empty.");
            }
        }
    }

    string m_DomainName;
};

CNetStorageByKey::CNetStorageByKey(CNetICacheClient::TInstance icache_client,
        const string& domain_name, TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageByKeyImpl(icache_client, domain_name, default_flags))
{
}

CNetStorageByKey::CNetStorageByKey(const string& domain_name,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageByKeyImpl(NULL, domain_name, default_flags))
{
}

CNetFile CNetStorageByKey::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetFileImpl(m_Impl, m_Impl->GetDefaultFlags(flags),
            m_Impl->m_DomainName, unique_key, m_Impl->m_NetICacheClient);
}

string CNetStorageByKey::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CNetFile orig_file(new SNetFileImpl(m_Impl, old_flags,
            m_Impl->m_DomainName, unique_key, m_Impl->m_NetICacheClient));

    CNetFile new_file(new SNetFileImpl(m_Impl, old_flags,
            m_Impl->m_DomainName, unique_key, m_Impl->m_NetICacheClient));

    char buffer[RELOCATION_BUFFER_SIZE];
    size_t bytes_read;

    while (!orig_file.Eof()) {
        bytes_read = orig_file.Read(buffer, sizeof(buffer));
        new_file.Write(buffer, bytes_read);
    }

    new_file.Close();
    orig_file.Close();

    if (orig_file->m_CurrentLocation == eNFL_NetCache)
        orig_file->m_NetICacheClient.Remove(
                orig_file->m_FileID.GetUniqueKey(), 0, kEmptyStr);

    // TODO Delete orig_file from FileTrack.

    return new_file->m_FileID.GetID();
}

bool CNetStorageByKey::Exists(const string& key, TNetStorageFlags flags)
{
    return false;
}

void CNetStorageByKey::Remove(const string& key, TNetStorageFlags flags)
{
}

END_NCBI_SCOPE
