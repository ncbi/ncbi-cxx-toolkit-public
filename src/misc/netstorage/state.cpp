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


BEGIN_NCBI_SCOPE


namespace
{

using namespace NDirectNetStorageImpl;


// The purpose of LocatorHolding templates is to provide object locator
// for any interface/class that needs it (e.g. for exception/log purposes).

template <class TBase>
class ILocatorHolding : public TBase
{
public:
    virtual TObjLoc& Locator() = 0;

    // Convenience method
    string LocatorToStr() { return Locator().GetLocator(); }
};


// This is used for CLocatorHolding constructor overloading
enum EBaseCtorExpectsLocator { eBaseCtorExpectsLocator };


template <class TBase>
class CLocatorHolding : public TBase
{
public:
    CLocatorHolding(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    CLocatorHolding(TObjLoc& object_loc, EBaseCtorExpectsLocator)
        : TBase(object_loc),
          m_ObjectLoc(object_loc)
    {}

    template <class TParam>
    CLocatorHolding(TObjLoc& object_loc, TParam param)
        : TBase(object_loc, param),
          m_ObjectLoc(object_loc)
    {}

private:
    virtual TObjLoc& Locator() { return m_ObjectLoc; }

    TObjLoc& m_ObjectLoc;
};


class CROState : public ILocatorHolding<IState>
{
public:
    ERW_Result WriteImpl(const void*, size_t, size_t*);
};


class CWOState : public ILocatorHolding<IState>
{
public:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
};


class CRWNotFound : public ILocatorHolding<IState>
{
public:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    ERW_Result WriteImpl(const void*, size_t, size_t*);
    bool EofImpl();
};


class CRONetCache : public CROState
{
public:
    typedef auto_ptr<IReader> TReaderPtr;

    void Set(TReaderPtr reader, size_t blob_size)
    {
        m_Reader = reader;
        m_BlobSize = blob_size;
        m_BytesRead = 0;
    }

    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    void CloseImpl();

private:
    TReaderPtr m_Reader;
    size_t m_BlobSize;
    size_t m_BytesRead;
};


class CWONetCache : public CWOState
{
public:
    typedef auto_ptr<IEmbeddedStreamWriter> TWriterPtr;

    void Set(TWriterPtr writer)
    {
        m_Writer = writer;
    }

    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl();
    void AbortImpl();

private:
    TWriterPtr m_Writer;
};


class CROFileTrack : public CROState
{
public:
    typedef CRef<SFileTrackRequest> TRequest;

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    void CloseImpl();

private:
    TRequest m_Request;
};


class CWOFileTrack : public CWOState
{
public:
    typedef CRef<SFileTrackPostRequest> TRequest;

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl();

private:
    TRequest m_Request;
};


class CLocation : public ILocatorHolding<ILocation>
{
public:
    virtual void SetLocator() = 0;
};


class CNotFound : public CLocation
{
public:
    CNotFound(TObjLoc& object_loc)
        : m_RW(object_loc)
    {}

    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

private:
    bool IsSame(const ILocation* other) const { return To<CNotFound>(other); }

    CLocatorHolding<CRWNotFound> m_RW;
};


class CNetCache : public CLocation
{
public:
    CNetCache(TObjLoc& object_loc, SContext* context)
        : m_Context(context),
          m_Read(object_loc),
          m_Write(object_loc)
    {}

    bool Init();
    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

private:
    bool IsSame(const ILocation* other) const { return To<CNetCache>(other); }

    CRef<SContext> m_Context;
    CNetICacheClientExt m_Client;
    CLocatorHolding<CRONetCache> m_Read;
    CLocatorHolding<CWONetCache> m_Write;
};


class CFileTrack : public CLocation
{
public:
    CFileTrack(TObjLoc& object_loc, SContext* context)
        : m_Context(context),
          m_Read(object_loc),
          m_Write(object_loc)
    {}

    bool Init() { return m_Context->filetrack_api; }
    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

private:
    bool IsSame(const ILocation* other) const { return To<CFileTrack>(other); }

    CRef<SContext> m_Context;
    CLocatorHolding<CROFileTrack> m_Read;
    CLocatorHolding<CWOFileTrack> m_Write;
};


ERW_Result CROState::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot write \"" << LocatorToStr() << "\" while reading.");
    return eRW_Error; // Not reached
}


ERW_Result CWOState::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot read \"" << LocatorToStr() << "\" while writing.");
    return eRW_Error; // Not reached
}


bool CWOState::EofImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot check EOF status of \"" << LocatorToStr() <<
            "\" while writing.");
    return true; // Not reached
}


ERW_Result CRWNotFound::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << LocatorToStr() << "\" for reading.");
    return eRW_Error; // Not reached
}


ERW_Result CRWNotFound::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << LocatorToStr() << "\" for writing.");
    return eRW_Error; // Not reached
}


bool CRWNotFound::EofImpl()
{
    return false;
}


ERW_Result CRONetCache::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    try {
        size_t bytes_read_local = 0;
        ERW_Result rw_res = m_Reader->Read(buf, count, &bytes_read_local);
        m_BytesRead += bytes_read_local;
        if (bytes_read != NULL)
            *bytes_read = bytes_read_local;
        return rw_res;
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + LocatorToStr())
    return eRW_Error; // Not reached
}


bool CRONetCache::EofImpl()
{
    return m_BytesRead >= m_BlobSize;
}


void CRONetCache::CloseImpl()
{
    try {
        m_Reader.reset();
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("after reading " + LocatorToStr())
}


ERW_Result CWONetCache::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    try {
        return m_Writer->Write(buf, count, bytes_written);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + LocatorToStr())
    return eRW_Error; // Not reached
}


void CWONetCache::CloseImpl()
{
    try {
        m_Writer->Close();
        m_Writer.reset();
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("after writing " + LocatorToStr())
}


void CWONetCache::AbortImpl()
{
    m_Writer->Abort();
    m_Writer.reset();
}


ERW_Result CROFileTrack::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    return m_Request->Read(buf, count, bytes_read);
}


bool CROFileTrack::EofImpl()
{
    return m_Request->m_HTTPStream.eof();
}


void CROFileTrack::CloseImpl()
{
    m_Request->FinishDownload();
    m_Request.Reset();
}


ERW_Result CWOFileTrack::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    m_Request->Write(buf, count, bytes_written);
    return eRW_Success;
}


void CWOFileTrack::CloseImpl()
{
    m_Request->FinishUpload();
    m_Request.Reset();
}


void CNotFound::SetLocator()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << LocatorToStr() << "\" for writing.");
}


IState* CNotFound::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);
    *result = m_RW.ReadImpl(buf, count, bytes_read);
    return &m_RW;
}


IState* CNotFound::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Object creation is disabled (no backend storages were provided)");
    return NULL;
}


Uint8 CNotFound::GetSizeImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << LocatorToStr() <<
            "\" could not be found in any of the designated locations.");
    return 0; // Not reached
}


CNetStorageObjectInfo CNotFound::GetInfoImpl()
{
    TObjLoc& object_loc(Locator());
    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(),
            eNFL_NotFound, &object_loc, 0, NULL);
}


bool CNotFound::ExistsImpl()
{
    return false;
}


ENetStorageRemoveResult CNotFound::RemoveImpl()
{
    return eNSTRR_NotFound;
}


void CNotFound::SetExpirationImpl(const CTimeout&)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << LocatorToStr() <<
            "\" could not be found in any of the designated locations.");
}


string CNotFound::FileTrack_PathImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << LocatorToStr() <<
            "\" could not be found in any of the designated locations.");
    return kEmptyStr; // Not reached
}


ILocation::TUserInfo CNotFound::GetUserInfoImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << LocatorToStr() <<
            "\" could not be found in any of the designated locations.");
    return TUserInfo(kEmptyStr, kEmptyStr); // Not reached
}


bool CNetCache::Init()
{
    if (!m_Client) {
        TObjLoc& object_loc(Locator());
        if (object_loc.GetLocation() == eNFL_NetCache) {
            m_Client = CNetICacheClient(object_loc.GetNCServiceName(),
                    object_loc.GetAppDomain(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (object_loc.IsXSiteProxyAllowed())
                m_Client.GetService().AllowXSiteConnections();
#endif
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
    Locator().SetLocation_NetCache(
            service.GetServiceName(),
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            service.IsUsingXSiteProxy() ? true :
#endif
                    false);
}


IState* CNetCache::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    TObjLoc& object_loc(Locator());

    try {
        size_t blob_size;
        CRONetCache::TReaderPtr reader(m_Client.GetReadStream(
                    object_loc.GetShortUniqueKey(), 0, kEmptyStr, &blob_size,
                    (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                    nc_cache_name = object_loc.GetAppDomain())));

        if (!reader.get()) {
            return NULL;
        }

        m_Read.Set(reader, blob_size);
        *result =  m_Read.ReadImpl(buf, count, bytes_read);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + object_loc.GetLocator())

    return &m_Read;
}


IState* CNetCache::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    TObjLoc& object_loc(Locator());

    try {
        CWONetCache::TWriterPtr writer(m_Client.GetNetCacheWriter(
                    object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                    nc_cache_name = object_loc.GetAppDomain()));

        if (!writer.get()) {
            return NULL;
        }

        m_Write.Set(writer);
        *result = m_Write.WriteImpl(buf, count, bytes_written);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + object_loc.GetLocator())

    SetLocator();
    return &m_Write;
}


Uint8 CNetCache::GetSizeImpl()
{
    TObjLoc& object_loc(Locator());

    try {
        return m_Client.GetBlobSize(
                object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + object_loc.GetLocator())
    return 0; // Not reached
}


CNetStorageObjectInfo CNetCache::GetInfoImpl()
{
    CJsonNode blob_info = CJsonNode::NewObjectNode();
    TObjLoc& object_loc(Locator());

    try {
        CNetServerMultilineCmdOutput output = m_Client.GetBlobInfo(
                object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());

        string line, key, val;

        while (output.ReadLine(line))
            if (NStr::SplitInTwo(line, ": ", key, val, NStr::fSplit_ByPattern))
                blob_info.SetByKey(key, CJsonNode::GuessType(val));
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + LocatorToStr())

    CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

    Uint8 blob_size = size_node && size_node.IsInteger() ?
            (Uint8) size_node.AsInteger() : GetSizeImpl();

    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(), eNFL_NetCache,
            &object_loc, blob_size, blob_info);
}


// Cannot use ExistsImpl() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define NC_EXISTS_IMPL(object_loc)                                          \
    if (!m_Client.HasBlob(object_loc.GetShortUniqueKey(),                   \
                kEmptyStr, nc_cache_name = object_loc.GetAppDomain())) {    \
        /* Have to throw to let other locations try */                      \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,                    \
            "NetStorageObject \"" << object_loc.GetLocator() <<             \
            "\" does not exist in NetCache.");                              \
    }


bool CNetCache::ExistsImpl()
{
    TObjLoc& object_loc(Locator());

    try {
        NC_EXISTS_IMPL(object_loc);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + LocatorToStr())

    return true;
}


ENetStorageRemoveResult CNetCache::RemoveImpl()
{
    TObjLoc& object_loc(Locator());

    try {
        // NetCache returns OK on removing already-removed/non-existent blobs,
        // so have to check for existence first and throw if not
        NC_EXISTS_IMPL(object_loc);
        m_Client.RemoveBlob(object_loc.GetShortUniqueKey(), 0, kEmptyStr,
                nc_cache_name = object_loc.GetAppDomain());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on removing " + LocatorToStr())

    return eNSTRR_Removed;
}


void CNetCache::SetExpirationImpl(const CTimeout& requested_ttl)
{
    TObjLoc& object_loc(Locator());

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
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + LocatorToStr())
}


string CNetCache::FileTrack_PathImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "NetStorageObject \"" << LocatorToStr() <<
            "\" is not a FileTrack object");
    return kEmptyStr; // Not reached
}


ILocation::TUserInfo CNetCache::GetUserInfoImpl()
{
    // Not supported
    return TUserInfo(kEmptyStr, kEmptyStr);
}


void CFileTrack::SetLocator()
{
    Locator().SetLocation_FileTrack(m_Context->filetrack_api.config.site);
}


IState* CFileTrack::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CROFileTrack::TRequest request = m_Context->filetrack_api.StartDownload(Locator());

    try {
        m_Read.Set(request);
        *result = m_Read.ReadImpl(buf, count, bytes_read);
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


IState* CFileTrack::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CWOFileTrack::TRequest request = m_Context->filetrack_api.StartUpload(Locator());
    m_Write.Set(request);
    *result = m_Write.WriteImpl(buf, count, bytes_written);
    SetLocator();
    return &m_Write;
}


Uint8 CFileTrack::GetSizeImpl()
{
    return (Uint8) m_Context->filetrack_api.GetFileInfo(
            Locator()).GetInteger("size");
}


CNetStorageObjectInfo CFileTrack::GetInfoImpl()
{
    TObjLoc& object_loc(Locator());
    CJsonNode file_info_node =
            m_Context->filetrack_api.GetFileInfo(object_loc);

    Uint8 file_size = 0;

    CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

    if (size_node)
        file_size = (Uint8) size_node.AsInteger();

    return g_CreateNetStorageObjectInfo(object_loc.GetLocator(),
            eNFL_FileTrack, &object_loc, file_size, file_info_node);
}


// Cannot use ExistsImpl() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define FT_EXISTS_IMPL(object_loc)                                  \
    if (m_Context->filetrack_api.GetFileInfo(object_loc).           \
            GetBoolean("deleted")) {                                \
        /* Have to throw to let other locations try */              \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,            \
            "NetStorageObject \"" << object_loc.GetLocator() <<     \
            "\" does not exist in FileTrack.");                     \
    }


bool CFileTrack::ExistsImpl()
{
    TObjLoc& object_loc(Locator());
    FT_EXISTS_IMPL(object_loc);
    return true;
}


ENetStorageRemoveResult CFileTrack::RemoveImpl()
{
    TObjLoc& object_loc(Locator());
    m_Context->filetrack_api.Remove(object_loc);
    return eNSTRR_Removed;
}


void CFileTrack::SetExpirationImpl(const CTimeout&)
{
    TObjLoc& object_loc(Locator());
    // By default objects in FileTrack do not have expiration,
    // so checking only object existence
    FT_EXISTS_IMPL(object_loc);
    NCBI_THROW_FMT(CNetStorageException, eNotSupported,
            "SetExpiration() is not supported for FileTrack");
}


string CFileTrack::FileTrack_PathImpl()
{
    return m_Context->filetrack_api.GetPath(Locator());
}


ILocation::TUserInfo CFileTrack::GetUserInfoImpl()
{
    CJsonNode file_info = m_Context->filetrack_api.GetFileInfo(Locator());
    CJsonNode my_ncbi_id = file_info.GetByKeyOrNull("myncbi_id");

    if (my_ncbi_id) return TUserInfo("my_ncbi_id", my_ncbi_id.AsString());

    return TUserInfo(kEmptyStr, kEmptyStr);
}


class CSelector : public ISelector
{
public:
    CSelector(const TObjLoc&, SContext*);
    CSelector(const TObjLoc&, SContext*, TNetStorageFlags);

    ILocation* First();
    ILocation* Next();
    void Restart();
    const TObjLoc& Locator();
    void SetLocator();
    ISelector* Clone(TNetStorageFlags);

private:
    void InitLocations(ENetStorageObjectLocation, TNetStorageFlags);
    CLocation* Top();

    TObjLoc m_ObjectLoc;
    CRef<SContext> m_Context;
    CLocatorHolding<CNotFound> m_NotFound;
    CLocatorHolding<CNetCache> m_NetCache;
    CLocatorHolding<CFileTrack> m_FileTrack;
    vector<CLocation*> m_Locations;
    size_t m_CurrentLocation;
};


CSelector::CSelector(const TObjLoc& loc, SContext* context)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc, eBaseCtorExpectsLocator),
        m_NetCache(m_ObjectLoc, m_Context),
        m_FileTrack(m_ObjectLoc, m_Context)
{
    InitLocations(m_ObjectLoc.GetLocation(), m_ObjectLoc.GetStorageAttrFlags());
}

CSelector::CSelector(const TObjLoc& loc, SContext* context, TNetStorageFlags flags)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc, eBaseCtorExpectsLocator),
        m_NetCache(m_ObjectLoc, m_Context),
        m_FileTrack(m_ObjectLoc, m_Context)
{
    InitLocations(eNFL_Unknown, flags);
}

void CSelector::InitLocations(ENetStorageObjectLocation location,
        TNetStorageFlags flags)
{
    // The order does matter:
    // First, primary locations
    // Then, secondary locations
    // After, all other locations that have not yet been used
    // And finally, the 'not found' location

    bool primary_nc = location == eNFL_NetCache;
    bool primary_ft = location == eNFL_FileTrack;
    bool secondary_nc = flags & (fNST_NetCache | fNST_Fast);
    bool secondary_ft = flags & (fNST_FileTrack | fNST_Persistent);
    LOG_POST(Trace << "location: " << location << ", flags: " << flags);

    m_Locations.push_back(&m_NotFound);

    if (!primary_nc && !secondary_nc && (flags & fNST_Movable)) {
        if (m_NetCache.Init()) {
            LOG_POST(Trace << "NetCache (movable)");
            m_Locations.push_back(&m_NetCache);
        }
    }

    if (!primary_ft && !secondary_ft && (flags & fNST_Movable)) {
        if (m_FileTrack.Init()) {
            LOG_POST(Trace << "FileTrack (movable)");
            m_Locations.push_back(&m_FileTrack);
        }
    }

    if (!primary_nc && secondary_nc) {
        if (m_NetCache.Init()) {
            LOG_POST(Trace << "NetCache (flag)");
            m_Locations.push_back(&m_NetCache);
        }
    }

    if (!primary_ft && secondary_ft) {
        if (m_FileTrack.Init()) {
            LOG_POST(Trace << "FileTrack (flag)");
            m_Locations.push_back(&m_FileTrack);
        }
    }

    if (primary_nc) {
        if (m_NetCache.Init()) {
            LOG_POST(Trace << "NetCache (location)");
            m_Locations.push_back(&m_NetCache);
        }
    }
    
    if (primary_ft) {
        if (m_FileTrack.Init()) {
            LOG_POST(Trace << "FileTrack (location)");
            m_Locations.push_back(&m_FileTrack);
        }
    }

    // No real locations, only CNotFound
    if (m_Locations.size() == 1) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "No storages available for locator=\"" <<
                m_ObjectLoc.GetLocator() << "\" and flags=" << flags);
    }

    Restart();
}


ILocation* CSelector::First()
{
    return Top();
}


ILocation* CSelector::Next()
{
    if (m_CurrentLocation) {
        --m_CurrentLocation;
        return Top();
    }
   
    return NULL;
}


CLocation* CSelector::Top()
{
    _ASSERT(m_Locations.size() > m_CurrentLocation);
    CLocation* location = m_Locations[m_CurrentLocation];
    _ASSERT(location);
    return location;
}


void CSelector::Restart()
{
    m_CurrentLocation = m_Locations.size() - 1;
}


const TObjLoc& CSelector::Locator()
{
    return m_ObjectLoc;
}


void CSelector::SetLocator()
{
    if (CLocation* l = Top()) {
        l->SetLocator();
    } else {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
    }
}


ISelector* CSelector::Clone(TNetStorageFlags flags)
{
    flags = m_Context->DefaultFlags(flags);
    return new CSelector(TObjLoc(m_Context->compound_id_pool,
                    m_ObjectLoc.GetLocator(), flags), m_Context, flags);
}


}


namespace NDirectNetStorageImpl
{


CNetICacheClient s_GetICClient(const SCombinedNetStorageConfig& c)
{
    if (c.nc_service.empty() || c.app_domain.empty() || c.client_name.empty()) {
        return eVoid;
    }

    return CNetICacheClient(c.nc_service, c.app_domain, c.client_name);
}


SContext::SContext(const SCombinedNetStorageConfig& config, TNetStorageFlags flags)
    : icache_client(s_GetICClient(config)),
      filetrack_api(config.ft),
      default_flags(flags),
      app_domain(config.app_domain)
{
    Init();
}


SContext::SContext(const string& domain, CNetICacheClient::TInstance client,
        CCompoundIDPool::TInstance id_pool,
        const SFileTrackConfig& ft_config)
    : icache_client(client),
      filetrack_api(ft_config),
      compound_id_pool(id_pool ? CCompoundIDPool(id_pool) : CCompoundIDPool()),
      app_domain(domain)
{
    Init();
}


void SContext::Init()
{
    m_Random.Randomize();

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


ISelector* SContext::Create(TNetStorageFlags flags)
{
    flags = DefaultFlags(flags);
    return new CSelector(TObjLoc(compound_id_pool,
                    flags, app_domain, m_Random.GetRandUint8(),
                    filetrack_api.config.site), this, flags);
}


ISelector* SContext::Create(const string& object_loc)
{
    return new CSelector(TObjLoc(compound_id_pool, object_loc), this);
}


ISelector* SContext::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    flags = DefaultFlags(flags);
    TObjLoc loc(compound_id_pool, flags, app_domain,
            m_Random.GetRandUint8(), filetrack_api.config.site);
    loc.SetServiceName(service);
    if (id) loc.SetObjectID(id);
    return new CSelector(loc, this, flags);
}


ISelector* SContext::Create(TNetStorageFlags flags,
        const string& service, const string& key)
{
    flags = DefaultFlags(flags);
    TObjLoc loc(compound_id_pool, flags, app_domain,
            key, filetrack_api.config.site);
    loc.SetServiceName(service);
    return new CSelector(loc, this, flags);
}


ISelector* SContext::Create(TNetStorageFlags flags, const string& key)
{
    flags = DefaultFlags(flags);
    return new CSelector(TObjLoc(compound_id_pool,
                    flags, app_domain, key,
                    filetrack_api.config.site), this, flags);
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


void SCombinedNetStorageConfig::Validate(const string& init_string)
{
    if (mode == eServerless && app_domain.empty()) {
        // TODO: Turn on nocreate/readonly mode instead? (CXX-7801)
        app_domain = "default";
    }

    SNetStorage::SConfig::Validate(init_string);
}


END_NCBI_SCOPE
