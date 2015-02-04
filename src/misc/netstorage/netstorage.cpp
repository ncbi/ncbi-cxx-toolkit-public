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

#include <misc/netstorage/netstorage.hpp>
#include <misc/netstorage/error_codes.hpp>

#include <util/util_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef NCBI_OS_MSWIN
#  include <unistd.h>
#endif

#define NCBI_USE_ERRCODE_X  NetStorage

#define RELOCATION_BUFFER_SIZE (128 * 1024)

BEGIN_NCBI_SCOPE

NCBI_PARAM_DEF(string, netstorage_api, backend_storage, "netcache, filetrack");

NCBI_PARAM_DEF(string, filetrack, site, "prod");
NCBI_PARAM_DEF(string, filetrack, api_key, kEmptyStr);

struct SNetStorageObjectAPIImpl;

struct SNetStorageAPIImpl : public SNetStorageImpl
{
    SNetStorageAPIImpl(const string& app_domain,
            CNetICacheClient::TInstance icache_client,
            TNetStorageFlags default_flags);

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0);
    virtual CNetStorageObject Open(const string& object_loc,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& object_loc, TNetStorageFlags flags);
    virtual bool Exists(const string& object_loc);
    virtual void Remove(const string& object_loc);

    TNetStorageFlags GetDefaultFlags(TNetStorageFlags flags)
    {
        return flags != 0 ? flags : m_DefaultFlags;
    }

    string MoveFile(SNetStorageObjectAPIImpl* orig_file,
            SNetStorageObjectAPIImpl* new_file);

    void x_InitNetCacheAPI();

    CCompoundIDPool m_CompoundIDPool;

    string m_AppDomain;

    CNetICacheClient m_NetICacheClient;
    TNetStorageFlags m_DefaultFlags;
    TNetStorageFlags m_AvailableStorageMask;

    SFileTrackAPI m_FileTrackAPI;

    CNetCacheAPI m_NetCacheAPI;
};

SNetStorageAPIImpl::SNetStorageAPIImpl(const string& app_domain,
        CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags) :
    m_AppDomain(app_domain),
    m_NetICacheClient(icache_client),
    m_DefaultFlags(default_flags),
    m_AvailableStorageMask(0)
{
    if (app_domain.empty() && icache_client != NULL)
        m_AppDomain = CNetICacheClient(icache_client).GetCacheName();

    string backend_storage(TNetStorageAPI_BackendStorage::GetDefault());

    if (strstr(backend_storage.c_str(), "netcache") != NULL)
        m_AvailableStorageMask |= fNST_NetCache;
    if (strstr(backend_storage.c_str(), "filetrack") != NULL)
        m_AvailableStorageMask |= fNST_FileTrack;

    m_DefaultFlags &= m_AvailableStorageMask;

    if (m_DefaultFlags == 0)
        m_DefaultFlags = m_AvailableStorageMask;

    m_AvailableStorageMask |= fNST_Movable | fNST_Cacheable;
}

enum ENetStorageObjectIOStatus {
    eNFS_Closed,
    eNFS_WritingToNetCache,
    eNFS_ReadingFromNetCache,
    eNFS_WritingToFileTrack,
    eNFS_ReadingFromFileTrack,
};

class ITryLocation
{
public:
    virtual bool TryLocation(ENetStorageObjectLocation location) = 0;
    virtual ~ITryLocation() {}
};

struct SNetStorageObjectAPIImpl : public SNetStorageObjectImpl
{
    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const string& app_domain,
            TNetStorageFlags flags,
            Uint8 random_number,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_ObjectLoc(storage_impl->m_CompoundIDPool, flags,
                app_domain, random_number,
                TFileTrack_Site::GetDefault().c_str()),
        m_StorageFlags(flags),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const string& object_loc) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(storage_impl->m_CompoundIDPool, object_loc),
        m_StorageFlags(m_ObjectLoc.GetStorageFlags()),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const string& object_loc,
            TNetStorageFlags flags) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(storage_impl->m_CompoundIDPool, object_loc),
        m_StorageFlags(flags),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            TNetStorageFlags flags,
            const string& app_domain,
            const string& unique_key,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_ObjectLoc(storage_impl->m_CompoundIDPool,
                flags, app_domain, unique_key,
                TFileTrack_Site::GetDefault().c_str()),
        m_StorageFlags(flags),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const CNetStorageObjectLoc& object_loc) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(object_loc),
        m_StorageFlags(m_ObjectLoc.GetStorageFlags()),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const CNetStorageObjectLoc& object_loc,
            TNetStorageFlags flags) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(object_loc),
        m_StorageFlags(flags),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read);

    virtual ERW_Result Write(const void* buf, size_t count,
            size_t* bytes_written);
    virtual void Close();
    virtual void Abort();

    virtual string GetLoc();
    virtual bool Eof();
    virtual Uint8 GetSize();
    virtual string GetAttribute(const string& attr_name);
    virtual void SetAttribute(const string& attr_name,
            const string& attr_value);
    virtual CNetStorageObjectInfo GetInfo();

    bool SetNetICacheClient();
    void DemandNetCache();

    const ENetStorageObjectLocation* GetPossibleFileLocations(); // For reading.
    void ChooseLocation(); // For writing.

    bool s_TryReadLocation(ENetStorageObjectLocation location,
            ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read);

    void Locate();
    bool LocateAndTry(ITryLocation* try_object);

    bool x_TryGetFileSizeFromLocation(ENetStorageObjectLocation location,
            Uint8* file_size);

    bool x_TryGetInfoFromLocation(ENetStorageObjectLocation location,
            CNetStorageObjectInfo* file_info);

    bool x_ExistsAtLocation(ENetStorageObjectLocation location);

    bool Exists();
    void Remove();

    virtual ~SNetStorageObjectAPIImpl();

    CRef<SNetStorageAPIImpl,
            CNetComponentCounterLocker<SNetStorageAPIImpl> > m_NetStorage;
    CNetICacheClient m_NetICacheClient;

    CNetStorageObjectLoc m_ObjectLoc;
    TNetStorageFlags m_StorageFlags;
    ENetStorageObjectLocation m_CurrentLocation;
    ENetStorageObjectIOStatus m_IOStatus;

    auto_ptr<IEmbeddedStreamWriter> m_NetCacheWriter;
    auto_ptr<IReader> m_NetCacheReader;
    size_t m_NetCache_BlobSize;
    size_t m_NetCache_BytesRead;

    CRef<SFileTrackRequest> m_FileTrackRequest;
    CRef<SFileTrackPostRequest> m_FileTrackPostRequest;
};

void g_SetNetICacheParams(CNetStorageObjectLoc& object_loc,
        CNetICacheClient icache_client)
{
    if (icache_client) {
        CNetService service(icache_client.GetService());

#if 0
        CNetServer icache_server(service.IterateByWeight(
                object_loc.GetICacheKey()).GetServer());
        const Uint4 nc_server_ip = icache_server.GetHost();
        const Uint2 nc_server_port = icache_server.GetPort();
#else
        const Uint4 nc_server_ip = 0;
        const Uint2 nc_server_port = 0;
#endif
        object_loc.SetLocation_NetCache(
                service.GetServiceName(),
                nc_server_ip, nc_server_port,
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
                service.IsUsingXSiteProxy() ? true :
#endif
                        false);
    }
}

bool SNetStorageObjectAPIImpl::SetNetICacheClient()
{
    if (!m_NetICacheClient) {
        if (m_ObjectLoc.GetLocation() == eNFL_NetCache) {
            m_NetICacheClient = CNetICacheClient(m_ObjectLoc.GetNCServiceName(),
                    m_ObjectLoc.GetAppDomain(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (m_ObjectLoc.IsXSiteProxyAllowed())
                m_NetICacheClient.GetService().AllowXSiteConnections();
#endif
        } else if (m_NetStorage->m_NetICacheClient)
            m_NetICacheClient = m_NetStorage->m_NetICacheClient;
        else
            return false;
    }
    return true;
}

void SNetStorageObjectAPIImpl::DemandNetCache()
{
    if (!SetNetICacheClient()) {
        NCBI_THROW(CNetStorageException, eInvalidArg,
                "The given storage preferences require a NetCache server.");
    }
}

static const ENetStorageObjectLocation s_TryNetCacheThenFileTrack[] =
        {eNFL_NetCache, eNFL_FileTrack, eNFL_NotFound};

static const ENetStorageObjectLocation s_NetCache[] =
        {eNFL_NetCache, eNFL_NotFound};

static const ENetStorageObjectLocation s_TryFileTrackThenNetCache[] =
        {eNFL_FileTrack, eNFL_NetCache, eNFL_NotFound};

static const ENetStorageObjectLocation s_FileTrack[] =
        {eNFL_FileTrack, eNFL_NotFound};

const ENetStorageObjectLocation*
        SNetStorageObjectAPIImpl::GetPossibleFileLocations()
{
    switch (m_ObjectLoc.GetLocation()) {
    case eNFL_NetCache:
        DemandNetCache();
        return m_ObjectLoc.IsMovable() ?
                s_TryNetCacheThenFileTrack : s_NetCache;
    default: /* eNFL_FileTrack */
        return m_ObjectLoc.IsMovable() && SetNetICacheClient() ?
                s_TryFileTrackThenNetCache : s_FileTrack;
    }
}

void SNetStorageObjectAPIImpl::ChooseLocation()
{
    TNetStorageFlags flags = m_StorageFlags;

    if (flags == 0)
        flags = (SetNetICacheClient() ? fNST_NetCache : fNST_FileTrack) &
                m_NetStorage->m_AvailableStorageMask;

    if (flags & fNST_FileTrack) {
        m_CurrentLocation = eNFL_FileTrack;
        m_ObjectLoc.SetLocation_FileTrack(
                TFileTrack_Site::GetDefault().c_str());
    } else {
        DemandNetCache();
        g_SetNetICacheParams(m_ObjectLoc, m_NetICacheClient);
        m_CurrentLocation = eNFL_NetCache;
    }
}

bool SNetStorageObjectAPIImpl::s_TryReadLocation(
        ENetStorageObjectLocation location,
        ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read)
{
    if (location == eNFL_NetCache) {
        m_NetCacheReader.reset(m_NetICacheClient.GetReadStream(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                &m_NetCache_BlobSize,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_cache_name = m_ObjectLoc.GetAppDomain())));

        if (m_NetCacheReader.get() != NULL) {
            m_NetCache_BytesRead = 0;
            *rw_res = g_ReadFromNetCache(m_NetCacheReader.get(),
                    buf, count, bytes_read);
            m_NetCache_BytesRead += *bytes_read;
            m_CurrentLocation = eNFL_NetCache;
            m_IOStatus = eNFS_ReadingFromNetCache;
            return true;
        }
    } else { /* location == eNFL_FileTrack */
        m_FileTrackRequest =
                m_NetStorage->m_FileTrackAPI.StartDownload(&m_ObjectLoc);

        try {
            *rw_res = m_FileTrackRequest->Read(buf, count, bytes_read);
            m_CurrentLocation = eNFL_FileTrack;
            m_IOStatus = eNFS_ReadingFromFileTrack;
            return true;
        }
        catch (CNetStorageException& e) {
            m_FileTrackRequest.Reset();

            if (e.GetErrCode() != CNetStorageException::eNotExists)
                throw;
        }
    }

    return false;
}

ERW_Result SNetStorageObjectAPIImpl::Read(
        void* buf, size_t count, size_t* bytes_read)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        switch (m_CurrentLocation) {
        case eNFL_Unknown:
            {
                size_t bytes_read_local = 0;
                ERW_Result rw_res = eRW_Success;
                const ENetStorageObjectLocation* location =
                        GetPossibleFileLocations();

                do
                    if (s_TryReadLocation(*location, &rw_res,
                            reinterpret_cast<char*>(buf), count,
                                    &bytes_read_local)) {
                        if (bytes_read != NULL)
                            *bytes_read = bytes_read_local;
                        return rw_res;
                    }
                while (*++location != eNFL_NotFound);
            }
            m_CurrentLocation = eNFL_NotFound;

        case eNFL_NotFound:
            break;

        default:
            {
                size_t bytes_read_local = 0;
                ERW_Result rw_res = eRW_Success;
                if (s_TryReadLocation(m_CurrentLocation, &rw_res,
                        reinterpret_cast<char*>(buf), count,
                                &bytes_read_local)) {
                    if (bytes_read != NULL)
                        *bytes_read = bytes_read_local;
                    return rw_res;
                }
            }
        }

        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot open \"" << m_ObjectLoc.GetLocator() <<
                "\" for reading.");

    case eNFS_ReadingFromNetCache:
        {
            size_t bytes_read_local = 0;
            ERW_Result rw_res = g_ReadFromNetCache(m_NetCacheReader.get(),
                    reinterpret_cast<char*>(buf), count, &bytes_read_local);
            m_NetCache_BytesRead += bytes_read_local;
            if (bytes_read != NULL)
                *bytes_read = bytes_read_local;
            return rw_res;
        }

    case eNFS_ReadingFromFileTrack:
        return m_FileTrackRequest->Read(buf, count, bytes_read);

    default: /* eNFS_WritingToNetCache or eNFS_WritingToFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot read while writing.");
    }

    return eRW_Success;
}

bool SNetStorageObjectAPIImpl::Eof()
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

ERW_Result SNetStorageObjectAPIImpl::Write(const void* buf, size_t count,
        size_t* bytes_written)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        if (m_CurrentLocation == eNFL_Unknown)
            ChooseLocation();

        if (m_CurrentLocation == eNFL_NetCache) {
            m_NetCacheWriter.reset(m_NetICacheClient.GetNetCacheWriter(
                    m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                    nc_cache_name = m_ObjectLoc.GetAppDomain()));

            m_IOStatus = eNFS_WritingToNetCache;

            return m_NetCacheWriter->Write(buf, count, bytes_written);
        } else {
            m_FileTrackPostRequest =
                    m_NetStorage->m_FileTrackAPI.StartUpload(&m_ObjectLoc);

            m_IOStatus = eNFS_WritingToFileTrack;

            m_FileTrackPostRequest->Write(buf, count, bytes_written);
        }
        break;

    case eNFS_WritingToNetCache:
        return m_NetCacheWriter->Write(buf, count, bytes_written);

    case eNFS_WritingToFileTrack:
        m_FileTrackPostRequest->Write(buf, count, bytes_written);
        break;

    default: /* eNFS_ReadingToNetCache or eNFS_ReadingFromFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot write while reading.");
    }

    return eRW_Success;
}

struct SCheckForExistence : public ITryLocation
{
    SCheckForExistence(SNetStorageObjectAPIImpl* netstorage_object_api) :
        m_NetStorageObjectAPI(netstorage_object_api)
    {
    }

    virtual bool TryLocation(ENetStorageObjectLocation location);

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > m_NetStorageObjectAPI;
};

bool SCheckForExistence::TryLocation(ENetStorageObjectLocation location)
{
    try {
        if (location == eNFL_NetCache) {
            if (m_NetStorageObjectAPI->m_NetICacheClient.HasBlob(
                    m_NetStorageObjectAPI->m_ObjectLoc.GetICacheKey(),
                    kEmptyStr,
                    nc_cache_name =
                            m_NetStorageObjectAPI->m_ObjectLoc.GetAppDomain()))
                return true;
        } else { /* location == eNFL_FileTrack */
            if (!m_NetStorageObjectAPI->m_NetStorage->
                    m_FileTrackAPI.GetFileInfo(
                    &m_NetStorageObjectAPI->m_ObjectLoc).GetBoolean("deleted"))
                return true;
        }
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

void SNetStorageObjectAPIImpl::Locate()
{
    SCheckForExistence check_for_existence(this);

    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (check_for_existence.TryLocation(*location)) {
                    m_CurrentLocation = *location;
                    return;
                }
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        {
            NCBI_THROW_FMT(CNetStorageException, eNotExists,
                    "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
                    "\" could not be found.");
        }
        break;

    default:
        break;
    }
}

bool SNetStorageObjectAPIImpl::LocateAndTry(ITryLocation* try_object)
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (try_object->TryLocation(*location))
                    return true;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            CNetStorageObjectInfo file_info;
            if (try_object->TryLocation(m_CurrentLocation))
                return true;
        }
    }

    return false;
}

bool SNetStorageObjectAPIImpl::x_TryGetFileSizeFromLocation(
        ENetStorageObjectLocation location,
        Uint8* file_size)
{
    try {
        if (location == eNFL_NetCache) {
            *file_size = m_NetICacheClient.GetBlobSize(
                    m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                    nc_cache_name = m_ObjectLoc.GetAppDomain());

            m_CurrentLocation = eNFL_NetCache;
            return true;
        } else { /* location == eNFL_FileTrack */
            *file_size = (Uint8) m_NetStorage->m_FileTrackAPI.GetFileInfo(
                    &m_ObjectLoc).GetInteger("size");

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

Uint8 SNetStorageObjectAPIImpl::GetSize()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            Uint8 file_size;
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_TryGetFileSizeFromLocation(*location, &file_size))
                    return file_size;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            Uint8 file_size;

            if (x_TryGetFileSizeFromLocation(m_CurrentLocation, &file_size))
                return file_size;
        }
    }

    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
            "\" could not be found in any of the designated locations.");
}

string SNetStorageObjectAPIImpl::GetAttribute(const string& attr_name)
{
    NCBI_THROW(CNetStorageException, eInvalidArg,
        "Attribute support is only implemented in NetStorage server.");
}

void SNetStorageObjectAPIImpl::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    NCBI_THROW(CNetStorageException, eInvalidArg,
        "Attribute support is only implemented in NetStorage server.");
}

bool SNetStorageObjectAPIImpl::x_TryGetInfoFromLocation(
        ENetStorageObjectLocation location,
        CNetStorageObjectInfo* file_info)
{
    if (location == eNFL_NetCache) {
        try {
            CNetServerMultilineCmdOutput output = m_NetICacheClient.GetBlobInfo(
                    m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                    nc_cache_name = m_ObjectLoc.GetAppDomain());

            CJsonNode blob_info = CJsonNode::NewObjectNode();
            string line, key, val;

            while (output.ReadLine(line))
                if (NStr::SplitInTwo(line, ": ",
                        key, val, NStr::fSplit_ByPattern))
                    blob_info.SetByKey(key, CJsonNode::GuessType(val));

            CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

            Uint8 blob_size = size_node && size_node.IsInteger() ?
                    (Uint8) size_node.AsInteger() :
                    m_NetICacheClient.GetBlobSize(
                            m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                            nc_cache_name = m_ObjectLoc.GetAppDomain());

            *file_info = g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
                    eNFL_NetCache, &m_ObjectLoc, blob_size, blob_info);

            m_CurrentLocation = eNFL_NetCache;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    } else { /* location == eNFL_FileTrack */
        try {
            CJsonNode file_info_node =
                    m_NetStorage->m_FileTrackAPI.GetFileInfo(&m_ObjectLoc);

            Uint8 file_size = 0;

            CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

            if (size_node)
                file_size = (Uint8) size_node.AsInteger();

            *file_info = g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
                    eNFL_FileTrack, &m_ObjectLoc, file_size, file_info_node);

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    }

    return false;
}

CNetStorageObjectInfo SNetStorageObjectAPIImpl::GetInfo()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            CNetStorageObjectInfo file_info;
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_TryGetInfoFromLocation(*location, &file_info))
                    return file_info;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            CNetStorageObjectInfo file_info;
            if (x_TryGetInfoFromLocation(m_CurrentLocation, &file_info))
                return file_info;
        }
    }

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_NotFound, &m_ObjectLoc, 0, NULL);
}

bool SNetStorageObjectAPIImpl::x_ExistsAtLocation(
        ENetStorageObjectLocation location)
{
    if (location == eNFL_NetCache) {
        try {
            if (!m_NetICacheClient.HasBlob(m_ObjectLoc.GetICacheKey(),
                    kEmptyStr,
                    nc_cache_name = m_ObjectLoc.GetAppDomain()))
                return false;

            m_CurrentLocation = eNFL_NetCache;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    } else { /* location == eNFL_FileTrack */
        try {
            if (m_NetStorage->m_FileTrackAPI.GetFileInfo(
                    &m_ObjectLoc).GetBoolean("deleted"))
                return false;

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    }

    return false;
}

bool SNetStorageObjectAPIImpl::Exists()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_ExistsAtLocation(*location))
                    return true;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        return x_ExistsAtLocation(m_CurrentLocation);
    }

    return false;
}

void SNetStorageObjectAPIImpl::Remove()
{
    if (m_CurrentLocation == eNFL_Unknown ||
            m_CurrentLocation == eNFL_FileTrack)
        try {
            m_NetStorage->m_FileTrackAPI.Remove(&m_ObjectLoc);
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }

    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        if (!SetNetICacheClient())
            break;
        /* FALL THROUGH */
    case eNFL_NetCache:
        try {
            m_NetICacheClient.RemoveBlob(m_ObjectLoc.GetICacheKey(), 0,
                    kEmptyStr,
                    nc_cache_name = m_ObjectLoc.GetAppDomain());
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
        break;
    default: /* eNFL_NotFound or eNFL_FileTrack */
        break;
    }

    m_CurrentLocation = eNFL_NotFound;
}

void SNetStorageObjectAPIImpl::Close()
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
        m_FileTrackPostRequest->FinishUpload();
        m_FileTrackPostRequest.Reset();
        break;

    case eNFS_ReadingFromFileTrack:
        m_IOStatus = eNFS_Closed;
        m_FileTrackRequest->FinishDownload();
        m_FileTrackRequest.Reset();
    }
}

void SNetStorageObjectAPIImpl::Abort()
{
    if (m_IOStatus == eNFS_WritingToNetCache) {
        m_IOStatus = eNFS_Closed;
        m_NetCacheWriter->Abort();
        m_NetCacheWriter.reset();
    } else
        Close();
}

SNetStorageObjectAPIImpl::~SNetStorageObjectAPIImpl()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage file.");
}

string SNetStorageObjectAPIImpl::GetLoc()
{
    return m_ObjectLoc.GetLocator();
}

CNetStorageObject SNetStorageAPIImpl::Create(TNetStorageFlags flags)
{
    // Restrict to available storage backends.
    flags &= m_AvailableStorageMask;

    Uint8 random_number = 0;

#ifndef NCBI_OS_MSWIN
    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd < 0) {
        NCBI_USER_THROW_FMT("Cannot open /dev/urandom: " <<
                strerror(errno));
    }

    char* read_buffer = reinterpret_cast<char*>(&random_number) + 1;
    ssize_t bytes_read;
    ssize_t bytes_remain = sizeof(random_number) - 2;
    do {
        bytes_read = read(urandom_fd, read_buffer, bytes_remain);
        if (bytes_read < 0 && errno != EINTR) {
            NCBI_USER_THROW_FMT("Error while reading "
                    "/dev/urandom: " << strerror(errno));
        }
        read_buffer += bytes_read;
    } while ((bytes_remain -= bytes_read) > 0);

    close(urandom_fd);

    random_number >>= 8;
#else
    random_number = m_FileTrackAPI.GetRandom();
#endif // NCBI_OS_MSWIN

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > netstorage_object(
                    new SNetStorageObjectAPIImpl(this,
                            m_AppDomain, GetDefaultFlags(flags),
                            random_number, m_NetICacheClient));

    netstorage_object->ChooseLocation();

    return netstorage_object.GetPointerOrNull();
}

CNetStorageObject SNetStorageAPIImpl::Open(
        const string& object_loc, TNetStorageFlags flags)
{
    if (CNetCacheKey::ParseBlobKey(object_loc.data(), object_loc.length(),
            NULL, m_CompoundIDPool)) {
        x_InitNetCacheAPI();
        return g_CreateNetStorage_NetCacheBlob(m_NetCacheAPI, object_loc);
    }

    flags &= m_AvailableStorageMask;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > netstorage_object(flags != 0 ?
                    new SNetStorageObjectAPIImpl(this, object_loc, flags) :
                    new SNetStorageObjectAPIImpl(this, object_loc));

    return netstorage_object.GetPointerOrNull();
}

string SNetStorageAPIImpl::MoveFile(SNetStorageObjectAPIImpl* orig_file,
        SNetStorageObjectAPIImpl* new_file)
{
    new_file->ChooseLocation();

    if (new_file->m_CurrentLocation == eNFL_NetCache &&
            new_file->m_ObjectLoc.GetLocation() != eNFL_NetCache)
        g_SetNetICacheParams(new_file->m_ObjectLoc, m_NetICacheClient);

    char buffer[RELOCATION_BUFFER_SIZE];

    // Use Read() to detect the current location of orig_file.
    size_t bytes_read;

    orig_file->Read(buffer, sizeof(buffer), &bytes_read);

    if (orig_file->m_CurrentLocation != new_file->m_CurrentLocation) {
        for (;;) {
            new_file->Write(buffer, bytes_read, NULL);
            if (orig_file->Eof())
                break;
            orig_file->Read(buffer, sizeof(buffer), &bytes_read);
        }

        new_file->Close();
        orig_file->Close();

        orig_file->Remove();
    }

    return new_file->m_ObjectLoc.GetLocator();
}

string SNetStorageAPIImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    flags &= m_AvailableStorageMask;

    if (flags == 0)
        return object_loc;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    orig_file(new SNetStorageObjectAPIImpl(this, object_loc));

    if (orig_file->m_ObjectLoc.GetStorageFlags() == flags)
        return object_loc;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    new_file(new SNetStorageObjectAPIImpl(this,
                                orig_file->m_ObjectLoc, flags));

    return MoveFile(orig_file, new_file);
}

bool SNetStorageAPIImpl::Exists(const string& object_loc)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(this, object_loc));

    return net_file->Exists();
}

void SNetStorageAPIImpl::Remove(const string& object_loc)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(this, object_loc));

    return net_file->Remove();
}

void SNetStorageAPIImpl::x_InitNetCacheAPI()
{
    if (!m_NetCacheAPI) {
        CNetCacheAPI nc_api(CNetCacheAPI::eAppRegistry);
        nc_api.SetCompoundIDPool(m_CompoundIDPool);
        nc_api.SetDefaultParameters(nc_use_compound_id = true);
        m_NetCacheAPI = nc_api;
    }
}

struct SNetStorageByKeyAPIImpl : public SNetStorageByKeyImpl
{
    SNetStorageByKeyAPIImpl(CNetICacheClient::TInstance icache_client,
            const string& app_domain, TNetStorageFlags default_flags) :
        m_APIImpl(new SNetStorageAPIImpl(app_domain,
                icache_client, default_flags))
    {
        if (m_APIImpl->m_AppDomain.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "Domain name cannot be empty.");
        }
    }

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);

    CRef<SNetStorageAPIImpl,
        CNetComponentCounterLocker<SNetStorageAPIImpl> > m_APIImpl;
};

CNetStorageObject SNetStorageByKeyAPIImpl::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetStorageObjectAPIImpl(m_APIImpl,
            m_APIImpl->GetDefaultFlags(flags) &
                    m_APIImpl->m_AvailableStorageMask,
            m_APIImpl->m_AppDomain,
            unique_key, m_APIImpl->m_NetICacheClient);
}

string SNetStorageByKeyAPIImpl::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    orig_file(new SNetStorageObjectAPIImpl(m_APIImpl, old_flags,
                            m_APIImpl->m_AppDomain, unique_key,
                            m_APIImpl->m_NetICacheClient));

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    new_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            flags & m_APIImpl->m_AvailableStorageMask,
                            m_APIImpl->m_AppDomain, unique_key,
                            m_APIImpl->m_NetICacheClient));

    return m_APIImpl->MoveFile(orig_file, new_file);
}

bool SNetStorageByKeyAPIImpl::Exists(const string& unique_key,
        TNetStorageFlags flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            m_APIImpl->GetDefaultFlags(flags),
                            m_APIImpl->m_AppDomain, unique_key,
                            m_APIImpl->m_NetICacheClient));

    return net_file->Exists();
}

void SNetStorageByKeyAPIImpl::Remove(const string& unique_key,
        TNetStorageFlags flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            m_APIImpl->GetDefaultFlags(flags),
                            m_APIImpl->m_AppDomain, unique_key,
                            m_APIImpl->m_NetICacheClient));

    net_file->Remove();
}

CNetStorage g_CreateNetStorage(
        CNetICacheClient::TInstance icache_client,
        const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(app_domain, icache_client, default_flags);
}

CNetStorage g_CreateNetStorage(const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(app_domain, NULL, default_flags);
}

CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string& app_domain, TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(icache_client,
            app_domain, default_flags);
}

CNetStorageByKey g_CreateNetStorageByKey(const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(NULL,
            app_domain, default_flags);
}

CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags)
{
    CNetStorageObject result = netstorage_api.Create(flags);

    SNetStorageObjectAPIImpl* object_impl =
            static_cast<SNetStorageObjectAPIImpl*>(
                    (SNetStorageObjectImpl*) result);

    object_impl->m_ObjectLoc.SetObjectID((Uint8) object_id);

    if (!service_name.empty())
        object_impl->m_ObjectLoc.SetServiceName(service_name);

    return result;
}

CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        TNetStorageFlags flags)
{
    CNetStorageObject result = netstorage_api.Create(flags);

    SNetStorageObjectAPIImpl* object_impl =
            static_cast<SNetStorageObjectAPIImpl*>(
                    (SNetStorageObjectImpl*) result);

    if (!service_name.empty())
        object_impl->m_ObjectLoc.SetServiceName(service_name);

    return result;
}

END_NCBI_SCOPE
