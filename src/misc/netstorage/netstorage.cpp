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

#include <misc/netstorage/error_codes.hpp>

#include <util/util_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define NCBI_USE_ERRCODE_X  NetStorage

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
