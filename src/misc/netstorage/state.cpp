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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/impl/netstorage_impl.hpp>

#include "state.hpp"

#include <limits>
#include <sstream>


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{


ERW_Result CRWNotFound::Read(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists, "On calling Read() cannot open " << GetLoc());
    return eRW_Error; // Not reached
}


ERW_Result CRWNotFound::PendingCount(size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists, "On calling PendingCount() cannot open " << GetLoc());
    return eRW_Error; // Not reached
}


bool CRWNotFound::Eof()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists, "On calling Eof() cannot open " << GetLoc());
    return false; // Not reached
}


ERW_Result CRWNotFound::Write(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists, "On calling Write() cannot open " << GetLoc());
    return eRW_Error; // Not reached
}


ERW_Result CRONetCache::Read(void* buf, size_t count, size_t* bytes_read)
{
    try {
        size_t bytes_read_local = 0;
        ERW_Result rw_res = m_Reader->Read(buf, count, &bytes_read_local);
        m_BytesRead += bytes_read_local;
        if (bytes_read != NULL)
            *bytes_read = bytes_read_local;
        return rw_res;
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + GetLoc())
    return eRW_Error; // Not reached
}


ERW_Result CRONetCache::PendingCount(size_t* count)
{
    try {
        return m_Reader->PendingCount(count);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + GetLoc())
    return eRW_Error; // Not reached
}


bool CRONetCache::Eof()
{
    return m_BytesRead >= m_BlobSize;
}


void CRONetCache::Close()
{
    ExitState();
    try {
        m_Reader.reset();
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("after reading " + GetLoc())
}


void CRONetCache::Abort()
{
    Close();
}


ERW_Result CWONetCache::Write(const void* buf, size_t count, size_t* bytes_written)
{
    try {
        return m_Writer->Write(buf, count, bytes_written);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + GetLoc())
    return eRW_Error; // Not reached
}


ERW_Result CWONetCache::Flush()
{
    try {
        return m_Writer->Flush();
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + GetLoc())
    return eRW_Error; // Not reached
}


void CWONetCache::Close()
{
    ExitState();
    try {
        m_Writer->Close();
        m_Writer.reset();
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("after writing " + GetLoc())
}


void CWONetCache::Abort()
{
    ExitState();
    m_Writer->Abort();
    m_Writer.reset();
}


ERW_Result CROFileTrack::Read(void* buf, size_t count, size_t* bytes_read)
{
    return m_Request->Read(buf, count, bytes_read);
}


ERW_Result CROFileTrack::PendingCount(size_t* count)
{
    *count = 0;
    return eRW_Success;
}


bool CROFileTrack::Eof()
{
    return m_Request->Eof();
}


void CROFileTrack::Close()
{
    ExitState();
    m_Request->FinishDownload();
    m_Request.Reset();
}


void CROFileTrack::Abort()
{
    ExitState();
    m_Request.Reset();
}


ERW_Result CWOFileTrack::Write(const void* buf, size_t count, size_t* bytes_written)
{
    m_Request->Write(buf, count, bytes_written);
    return eRW_Success;
}


ERW_Result CWOFileTrack::Flush()
{
    return eRW_Success;
}


void CWOFileTrack::Close()
{
    ExitState();
    m_Request->FinishUpload();
    m_Request.Reset();
}


void CWOFileTrack::Abort()
{
    ExitState();
    m_Request.Reset();
}


void CNotFound::SetLocator()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << GetLoc() << "\" for writing.");
}


INetStorageObjectState* CNotFound::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);
    *result = m_RW.Read(buf, count, bytes_read);
    return &m_RW;
}


INetStorageObjectState* CNotFound::StartWrite(const void*, size_t, size_t*, ERW_Result*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Object creation is disabled (no backend storages were provided)");
    return NULL;
}


Uint8 CNotFound::GetSize()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << GetLoc() <<
            "\" could not be found in any of the designated locations.");
    return 0; // Not reached
}


CNetStorageObjectInfo CNotFound::GetInfo()
{
    CNetStorageObjectLoc& object_loc(Locator());
    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(),
            eNFL_NotFound, &object_loc, 0, NULL);
}


bool CNotFound::Exists()
{
    return false;
}


ENetStorageRemoveResult CNotFound::Remove()
{
    return eNSTRR_NotFound;
}


void CNotFound::SetExpiration(const CTimeout&)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << GetLoc() <<
            "\" could not be found in any of the designated locations.");
}


string CNotFound::FileTrack_Path()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << GetLoc() <<
            "\" could not be found in any of the designated locations.");
    return kEmptyStr; // Not reached
}


pair<string, string> CNotFound::GetUserInfo()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << GetLoc() <<
            "\" could not be found in any of the designated locations.");
    return make_pair(kEmptyStr, kEmptyStr); // Not reached
}


bool CNetCache::Init()
{
    if (!m_Client) {
        CNetStorageObjectLoc& object_loc(Locator());
        if (object_loc.GetLocation() == eNFL_NetCache) {
            m_Client = CNetICacheClient(object_loc.GetNCServiceName(),
                    object_loc.GetAppDomain(), kEmptyStr);
        } else if (m_Context->icache_client)
            m_Client = m_Context->icache_client;
        else
            return false;
    }
    return true;
}


void CNetCache::SetLocator()
{
    CNetService service(m_Client.GetService());
    const string& service_name(service.GetServiceName());

    Locator().SetLocation(service_name);
}


INetStorageObjectState* CNetCache::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CNetStorageObjectLoc& object_loc(Locator());

    try {
        size_t blob_size;
        CRONetCache::TReaderPtr reader;
        const auto& key = object_loc.GetShortUniqueKey();
        const auto& version = object_loc.GetVersion();
        const auto& subkey = object_loc.GetSubKey();

        if (version.IsNull()) {
            int not_used;
            reader.reset(m_Client.GetReadStream(key, subkey, &not_used, &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_cache_name = object_loc.GetAppDomain())));
        } else {
            reader.reset(m_Client.GetReadStream(key, version, subkey, &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_cache_name = object_loc.GetAppDomain())));
        }

        if (!reader.get()) {
            return NULL;
        }

        m_Read.Set(move(reader), blob_size);
        *result =  m_Read.Read(buf, count, bytes_read);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + object_loc.GetLocator())

    return &m_Read;
}


INetStorageObjectState* CNetCache::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CNetStorageObjectLoc& object_loc(Locator());

    try {
        CWONetCache::TWriterPtr writer(m_Client.GetNetCacheWriter(
                    object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                    nc_cache_name = object_loc.GetAppDomain()));

        _ASSERT(writer.get());

        m_Write.Set(move(writer));
        *result = m_Write.Write(buf, count, bytes_written);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + object_loc.GetLocator())

    SetLocator();
    return &m_Write;
}


Uint8 CNetCache::GetSize()
{
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        return m_Client.GetBlobSize(
                object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + object_loc.GetLocator())
    return 0; // Not reached
}


CNetStorageObjectInfo CNetCache::GetInfo()
{
    CJsonNode blob_info = CJsonNode::NewObjectNode();
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        CNetServerMultilineCmdOutput output = m_Client.GetBlobInfo(
                object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());

        string line, key, val;

        while (output.ReadLine(line))
            if (NStr::SplitInTwo(line, ": ", key, val, NStr::fSplit_ByPattern))
                blob_info.SetByKey(key, CJsonNode::GuessType(val));
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + GetLoc())

    CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

    Uint8 blob_size = size_node && size_node.IsInteger() ?
            (Uint8) size_node.AsInteger() : GetSize();

    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(), eNFL_NetCache,
            &object_loc, blob_size, blob_info);
}


// Cannot use Exists() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define NC_EXISTS_IMPL(object_loc)                                          \
    if (!m_Client.HasBlob(object_loc.GetShortUniqueKey(),                   \
                kEmptyStr, nc_cache_name = object_loc.GetAppDomain())) {    \
        /* Have to throw to let other locations try */                      \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,                    \
            "NetStorageObject \"" << object_loc.GetLocator() <<             \
            "\" does not exist in NetCache.");                              \
    }


bool CNetCache::Exists()
{
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        NC_EXISTS_IMPL(object_loc);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + GetLoc())

    return true;
}


ENetStorageRemoveResult CNetCache::Remove()
{
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        // NetCache returns OK on removing already-removed/non-existent blobs,
        // so have to check for existence first and throw if not
        NC_EXISTS_IMPL(object_loc);
        m_Client.RemoveBlob(object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on removing " + GetLoc())

    return eNSTRR_Removed;
}


void CNetCache::SetExpiration(const CTimeout& requested_ttl)
{
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        NC_EXISTS_IMPL(object_loc);

        CTimeout ttl(requested_ttl);

        if (!ttl.IsFinite()) {
            // NetCache does not support infinite TTL, use max possible instead
            ttl.Set(numeric_limits<unsigned>::max(), 0);
        }

        m_Client.ProlongBlobLifetime(object_loc.GetShortUniqueKey(), ttl,
                nc_cache_name = object_loc.GetAppDomain());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + GetLoc())
}


string CNetCache::FileTrack_Path()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "NetStorageObject \"" << GetLoc() <<
            "\" is not a FileTrack object");
    return kEmptyStr; // Not reached
}


pair<string, string> CNetCache::GetUserInfo()
{
    CNetStorageObjectLoc& object_loc(Locator());

    try {
        NC_EXISTS_IMPL(object_loc);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + GetLoc())

    // Not supported
    return make_pair(kEmptyStr, kEmptyStr);
}


void CFileTrack::SetLocator()
{
    Locator().SetLocation(kEmptyStr);
}


INetStorageObjectState* CFileTrack::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CROFileTrack::TRequest request = m_Context->filetrack_api.StartDownload(Locator());

    try {
        m_Read.Set(request);
        *result = m_Read.Read(buf, count, bytes_read);
        return &m_Read;
    }
    catch (CNetStorageException& e) {
        request.Reset();

        if (e.GetErrCode() != CNetStorageException::eNotExists) {
            throw;
        }
    }

    return NULL;
}


INetStorageObjectState* CFileTrack::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CWOFileTrack::TRequest request = m_Context->filetrack_api.StartUpload(Locator());
    m_Write.Set(request);
    *result = m_Write.Write(buf, count, bytes_written);
    SetLocator();
    return &m_Write;
}


Uint8 CFileTrack::GetSize()
{
    return (Uint8) m_Context->filetrack_api.GetFileInfo(
            Locator()).GetInteger("size");
}


CNetStorageObjectInfo CFileTrack::GetInfo()
{
    CNetStorageObjectLoc& object_loc(Locator());
    CJsonNode file_info_node =
            m_Context->filetrack_api.GetFileInfo(object_loc);

    Uint8 file_size = 0;

    CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

    if (size_node)
        file_size = (Uint8) size_node.AsInteger();

    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(),
            eNFL_FileTrack, &object_loc, file_size, file_info_node);
}


// Cannot use Exists() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define FT_EXISTS_IMPL(object_loc)                                  \
    if (!m_Context->filetrack_api.GetFileInfo(object_loc)) {        \
        /* Have to throw to let other locations try */              \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,            \
            "NetStorageObject \"" << object_loc.GetLocator() <<     \
            "\" does not exist in FileTrack.");                     \
    }


bool CFileTrack::Exists()
{
    CNetStorageObjectLoc& object_loc(Locator());
    FT_EXISTS_IMPL(object_loc);
    return true;
}


ENetStorageRemoveResult CFileTrack::Remove()
{
    CNetStorageObjectLoc& object_loc(Locator());
    m_Context->filetrack_api.Remove(object_loc);
    return eNSTRR_Removed;
}


void CFileTrack::SetExpiration(const CTimeout&)
{
    CNetStorageObjectLoc& object_loc(Locator());
    // By default objects in FileTrack do not have expiration,
    // so checking only object existence
    FT_EXISTS_IMPL(object_loc);
    NCBI_THROW_FMT(CNetStorageException, eNotSupported,
            "SetExpiration() is not supported for FileTrack");
}


string CFileTrack::FileTrack_Path()
{
    return m_Context->filetrack_api.GetPath(Locator());
}


pair<string, string> CFileTrack::GetUserInfo()
{
    CJsonNode file_info = m_Context->filetrack_api.GetFileInfo(Locator());
    CJsonNode my_ncbi_id = file_info.GetByKeyOrNull("myncbi_id");

    if (my_ncbi_id) return make_pair(string("my_ncbi_id"), my_ncbi_id.Repr());

    return make_pair(kEmptyStr, kEmptyStr);
}


CNetICacheClient s_GetICClient(const SCombinedNetStorageConfig& c)
{
    if (c.nc_service.empty() || c.app_domain.empty() || c.client_name.empty()) {
        return eVoid;
    }

    return CNetICacheClient(c.nc_service, c.app_domain, c.client_name);
}


string s_GetSection(const IRegistry& registry, const string& service,
        const string& name)
{
    if (!service.empty()) {
        const string section = "service_" + service;

        if (registry.HasEntry(section, name)) {
            return section;
        }
    }

    return "netstorage_api";
}


SFileTrackConfig s_GetFTConfig(const IRegistry& registry, const string& service)
{
    const string param_name = "filetrack";
    const string service_section = s_GetSection(registry, service, param_name);
    const string ft_section = registry.Get(service_section, param_name);
    return ft_section.empty() ? eVoid : SFileTrackConfig(registry, ft_section);
}


CNetICacheClient s_GetICClient(const IRegistry& registry, const string& service)
{
    const string param_name = "netcache";
    const string service_section = s_GetSection(registry, service, param_name);
    const string nc_section = registry.Get(service_section, param_name);
    return nc_section.empty() ? eVoid : CNetICacheClient(registry, nc_section);
}


string s_GetAppDomain(const string& app_domain, CNetICacheClient& nc_client)
{
    // In general, app_domain may not be avaiable.
    // Since its value does not actually affect anything,
    // we just use cache name from CNetICacheClient.
    // If that is not avaiable, "default" value is used instead.

    if (!app_domain.empty()) return app_domain;

    if (nc_client) {
        const string cache_name(nc_client.GetCacheName());

        if (!cache_name.empty()) return cache_name;
    }

    return "default";
}


size_t s_GetRelocateChunk(const IRegistry& registry, const string& service,
        size_t default_value)
{
    const string param_name = "relocate_chunk";
    const string service_section = s_GetSection(registry, service, param_name);
    return registry.GetInt(service_section, param_name, static_cast<int>(default_value));
}


const size_t kRelocateChunk = 1024 * 1024;


SContext::SContext(const SCombinedNetStorageConfig& config, TNetStorageFlags flags)
    : icache_client(s_GetICClient(config)),
      filetrack_api(config.ft),
      default_flags(flags),
      app_domain(s_GetAppDomain(config.app_domain, icache_client)),
      relocate_chunk(kRelocateChunk)
{
    Init();
}


SContext::SContext(const string& service_name, const string& domain,
        CCompoundIDPool::TInstance id_pool,
        const IRegistry& registry)
    : icache_client(s_GetICClient(registry, service_name)),
      filetrack_api(s_GetFTConfig(registry, service_name)),
      compound_id_pool(id_pool ? CCompoundIDPool(id_pool) : CCompoundIDPool()),
      app_domain(s_GetAppDomain(domain, icache_client)),
      relocate_chunk(s_GetRelocateChunk(registry, service_name, kRelocateChunk))
{
    Init();
}


void SContext::Init()
{
    random.Randomize();

    const TNetStorageFlags backend_storage =
        (icache_client ? fNST_NetCache  : 0) |
        (filetrack_api ? fNST_FileTrack : 0);

    // If there were specific underlying storages requested
    if (TNetStorageLocFlags(default_flags)) {
        // Reduce storages to the ones that are available
        default_flags &= (backend_storage | fNST_AnyAttr);
    } else {
        // Use all available underlying storages
        default_flags |= backend_storage;
    }

    if (TNetStorageLocFlags(default_flags)) {
        if (app_domain.empty() && icache_client) {
            app_domain = icache_client.GetCacheName();
        }
    }
}


}


SCombinedNetStorageConfig::EMode
SCombinedNetStorageConfig::GetMode(const string& value)
{
    if (NStr::CompareNocase(value, "direct") == 0)
        return eServerless;
    else
        return eDefault;
}


void SCombinedNetStorageConfig::ParseArg(const string& name,
        const string& value)
{
    if (name == "mode")
        mode = GetMode(value);
    else if (!ft.ParseArg(name, value))
        SNetStorage::SConfig::ParseArg(name, value);
}


END_NCBI_SCOPE
