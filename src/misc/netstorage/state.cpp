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
#include "state.hpp"


BEGIN_NCBI_SCOPE


namespace
{

using namespace NImpl;


class CROState : public IState
{
public:
    ERW_Result WriteImpl(const void*, size_t, size_t*);
};


class CWOState : public IState
{
public:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
};


class CRWNotFound : public IState
{
public:
    CRWNotFound(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    ERW_Result ReadImpl(void*, size_t, size_t*);
    ERW_Result WriteImpl(const void*, size_t, size_t*);
    bool EofImpl();

private:
    TObjLoc& m_ObjectLoc;
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


class CLocation : public ILocation
{
public:
    CLocation(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    virtual bool Init() { return true; }
    virtual void SetLocator() = 0;

protected:
    TObjLoc& m_ObjectLoc;
};


class CNotFound : public CLocation
{
public:
    CNotFound(TObjLoc& object_loc)
        : CLocation(object_loc),
          m_RW(object_loc)
    {}

    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRWNotFound m_RW;
};


class CNetCache : public CLocation
{
public:
    CNetCache(TObjLoc& object_loc, SContext* context)
        : CLocation(object_loc),
          m_Context(context),
          m_Client(eVoid)
    {}

    // TODO: This ctor is not used, investigate possible loss of functionality
    CNetCache(TObjLoc& object_loc, SContext* context, bool ctx_client)
        : CLocation(object_loc),
          m_Context(context),
          m_Client(ctx_client ? m_Context->icache_client : CNetICacheClient(eVoid))
    {}

    bool Init();
    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRef<SContext> m_Context;
    CNetICacheClient m_Client;
    CRONetCache m_Read;
    CWONetCache m_Write;
};


class CFileTrack : public CLocation
{
public:
    CFileTrack(TObjLoc& object_loc, SContext* context)
        : CLocation(object_loc),
          m_Context(context)
    {}

    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRef<SContext> m_Context;
    CROFileTrack m_Read;
    CWOFileTrack m_Write;
};


ERW_Result CROState::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot write while reading.");
    return eRW_Error; // Not reached
}


ERW_Result CWOState::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot read while writing.");
    return eRW_Error; // Not reached
}


bool CWOState::EofImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot check EOF status while writing.");
    return true; // Not reached
}


ERW_Result CRWNotFound::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for reading.");
    return eRW_Error; // Not reached
}


ERW_Result CRWNotFound::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
    return eRW_Error; // Not reached
}


bool CRWNotFound::EofImpl()
{
    return false;
}


ERW_Result CRONetCache::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    size_t bytes_read_local = 0;
    ERW_Result rw_res = g_ReadFromNetCache(m_Reader.get(),
            reinterpret_cast<char*>(buf), count, &bytes_read_local);
    m_BytesRead += bytes_read_local;
    if (bytes_read != NULL)
        *bytes_read = bytes_read_local;
    return rw_res;
}


bool CRONetCache::EofImpl()
{
    return m_BytesRead >= m_BlobSize;
}


void CRONetCache::CloseImpl()
{
    m_Reader.reset();
}


ERW_Result CWONetCache::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    return m_Writer->Write(buf, count, bytes_written);
}


void CWONetCache::CloseImpl()
{
    m_Writer->Close();
    m_Writer.reset();
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
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
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
    _ASSERT(result);
    *result = m_RW.WriteImpl(buf, count, bytes_written);
    return &m_RW;
}


Uint8 CNotFound::GetSizeImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
            "\" could not be found in any of the designated locations.");
    return 0; // Not reached
}


CNetStorageObjectInfo CNotFound::GetInfoImpl()
{
    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_NotFound, &m_ObjectLoc, 0, NULL);
}


bool CNotFound::ExistsImpl()
{
    return false;
}


void CNotFound::RemoveImpl()
{
}


bool CNetCache::Init()
{
    if (!m_Client) {
        if (m_ObjectLoc.GetLocation() == eNFL_NetCache) {
            m_Client = CNetICacheClient(m_ObjectLoc.GetNCServiceName(),
                    m_ObjectLoc.GetAppDomain(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (m_ObjectLoc.IsXSiteProxyAllowed())
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
    m_ObjectLoc.SetLocation_NetCache(
            service.GetServiceName(), 0, 0,
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            service.IsUsingXSiteProxy() ? true :
#endif
                    false);
}


IState* CNetCache::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    size_t blob_size;
    CRONetCache::TReaderPtr reader(m_Client.GetReadStream(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr, &blob_size,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_cache_name = m_ObjectLoc.GetAppDomain())));

    if (!reader.get()) {
        return NULL;
    }

    m_Read.Set(reader, blob_size);
    *result =  m_Read.ReadImpl(buf, count, bytes_read);
    return &m_Read;
}


IState* CNetCache::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CWONetCache::TWriterPtr writer(m_Client.GetNetCacheWriter(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                nc_cache_name = m_ObjectLoc.GetAppDomain()));

    if (!writer.get()) {
        return NULL;
    }

    m_Write.Set(writer);
    *result = m_Write.WriteImpl(buf, count, bytes_written);
    SetLocator();
    return &m_Write;
}


Uint8 CNetCache::GetSizeImpl()
{
    return m_Client.GetBlobSize(
            m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());
}


CNetStorageObjectInfo CNetCache::GetInfoImpl()
{
    CNetServerMultilineCmdOutput output = m_Client.GetBlobInfo(
            m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());

    CJsonNode blob_info = CJsonNode::NewObjectNode();
    string line, key, val;

    while (output.ReadLine(line))
        if (NStr::SplitInTwo(line, ": ", key, val, NStr::fSplit_ByPattern))
            blob_info.SetByKey(key, CJsonNode::GuessType(val));

    CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

    Uint8 blob_size = size_node && size_node.IsInteger() ?
            (Uint8) size_node.AsInteger() : GetSizeImpl();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(), eNFL_NetCache,
            &m_ObjectLoc, blob_size, blob_info);
}


bool CNetCache::ExistsImpl()
{
    LOG_POST(Trace << "Checking existence in NetCache " << m_ObjectLoc.GetLocator());

    if (m_Client.HasBlob(m_ObjectLoc.GetICacheKey(),
            kEmptyStr, nc_cache_name = m_ObjectLoc.GetAppDomain())) {
        return true;
    } else {
        // Have to throw to let other locations do this check
        NCBI_THROW_FMT(CNetCacheException, eBlobNotFound, "Not found");
        return false; // Not reached
    }
}


void CNetCache::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from NetCache " << m_ObjectLoc.GetLocator());

    if (ExistsImpl()) {
        m_Client.RemoveBlob(m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                nc_cache_name = m_ObjectLoc.GetAppDomain());
    } else {
        // Have to throw to let other locations try to remove
        NCBI_THROW_FMT(CNetCacheException, eBlobNotFound, "Not found");
    }
}


void CFileTrack::SetLocator()
{
    m_ObjectLoc.SetLocation_FileTrack(m_Context->filetrack_api.site.c_str());
}


IState* CFileTrack::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CROFileTrack::TRequest request = m_Context->filetrack_api.StartDownload(&m_ObjectLoc);

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

    CWOFileTrack::TRequest request = m_Context->filetrack_api.StartUpload(&m_ObjectLoc);
    m_Write.Set(request);
    *result = m_Write.WriteImpl(buf, count, bytes_written);
    SetLocator();
    return &m_Write;
}


Uint8 CFileTrack::GetSizeImpl()
{
    return (Uint8) m_Context->filetrack_api.GetFileInfo(
            &m_ObjectLoc).GetInteger("size");
}


CNetStorageObjectInfo CFileTrack::GetInfoImpl()
{
    CJsonNode file_info_node =
            m_Context->filetrack_api.GetFileInfo(&m_ObjectLoc);

    Uint8 file_size = 0;

    CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

    if (size_node)
        file_size = (Uint8) size_node.AsInteger();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_FileTrack, &m_ObjectLoc, file_size, file_info_node);
}


bool CFileTrack::ExistsImpl()
{
    LOG_POST(Trace << "Checking existence in FileTrack " << m_ObjectLoc.GetLocator());
    return !m_Context->filetrack_api.GetFileInfo(&m_ObjectLoc).GetBoolean("deleted");
}


void CFileTrack::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from FileTrack " << m_ObjectLoc.GetLocator());
    m_Context->filetrack_api.Remove(&m_ObjectLoc);
}


class CSelector : public ISelector
{
public:
    CSelector(const TObjLoc&, SContext*);
    CSelector(const TObjLoc&, SContext*, TNetStorageFlags);

    ILocation* First();
    ILocation* Next();
    const TObjLoc& Locator();
    void SetLocator();
    Ptr Clone(TNetStorageFlags);

private:
    void InitLocations(ENetStorageObjectLocation, TNetStorageFlags);
    CLocation* Top();

    TObjLoc m_ObjectLoc;
    CRef<SContext> m_Context;
    CNotFound m_NotFound;
    CNetCache m_NetCache;
    CFileTrack m_FileTrack;
    stack<CLocation*> m_Locations;
};


CSelector::CSelector(const TObjLoc& loc, SContext* context)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc),
        m_NetCache(m_ObjectLoc, m_Context),
        m_FileTrack(m_ObjectLoc, m_Context)
{
    InitLocations(m_ObjectLoc.GetLocation(), m_ObjectLoc.GetStorageAttrFlags());
}

CSelector::CSelector(const TObjLoc& loc, SContext* context, TNetStorageFlags flags)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc),
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

    m_Locations.push(&m_NotFound);

    if (!primary_nc && !secondary_nc && (flags & fNST_Movable)) {
        LOG_POST(Trace << "NetCache (movable)");
        m_Locations.push(&m_NetCache);
    }

    if (!primary_ft && !secondary_ft && (flags & fNST_Movable)) {
        LOG_POST(Trace << "FileTrack (movable)");
        m_Locations.push(&m_FileTrack);
    }

    if (!primary_nc && secondary_nc) {
        LOG_POST(Trace << "NetCache (flag)");
        m_Locations.push(&m_NetCache);
    }

    if (!primary_ft && secondary_ft) {
        LOG_POST(Trace << "FileTrack (flag)");
        m_Locations.push(&m_FileTrack);
    }

    if (primary_nc) {
        LOG_POST(Trace << "NetCache (location)");
        m_Locations.push(&m_NetCache);
    }
    
    if (primary_ft) {
        LOG_POST(Trace << "FileTrack (location)");
        m_Locations.push(&m_FileTrack);
    }

    // No real locations, only CNotFound
    if (m_Locations.size() == 1) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "No storages available for locator=\"" <<
                m_ObjectLoc.GetLocator() << "\" and flags=" << flags);
    }
}


ILocation* CSelector::First()
{
    return m_Locations.size() ? Top() : NULL;
}


ILocation* CSelector::Next()
{
    if (m_Locations.size() > 1) {
        m_Locations.pop();
        return Top();
    }
   
    return NULL;
}


CLocation* CSelector::Top()
{
    _ASSERT(m_Locations.size());
    CLocation* location = m_Locations.top();
    _ASSERT(location);
    return location->Init() ? location : NULL;
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


ISelector::Ptr CSelector::Clone(TNetStorageFlags flags)
{
    flags = m_Context->DefaultFlags(flags);
    return Ptr(new CSelector(TObjLoc(m_Context->compound_id_pool,
                    m_ObjectLoc.GetLocator(), flags), m_Context, flags));
}


}


namespace NImpl
{

SContext::SContext(const string& domain, CNetICacheClient client,
        TNetStorageFlags flags, CCompoundIDPool::TInstance id_pool,
        const IRegistry& registry)
    : icache_client(client),
      filetrack_api(registry),
      compound_id_pool(id_pool ? CCompoundIDPool(id_pool) : CCompoundIDPool()),
      default_flags(flags),
      valid_flags_mask(0),
      app_domain(domain)
{
    string backend_storage(TNetStorageAPI_BackendStorage::GetDefault());

    if (strstr(backend_storage.c_str(), "netcache"))
        valid_flags_mask |= fNST_NetCache;
    if (strstr(backend_storage.c_str(), "filetrack"))
        valid_flags_mask |= fNST_FileTrack;

    // If there were specific underlying storages requested
    if (TNetStorageLocFlags(default_flags)) {
        // Reduce storages to the ones that are available
        default_flags &= valid_flags_mask;
    } else {
        // Use all available underlying storages
        default_flags |= valid_flags_mask;
    }

    if (TNetStorageLocFlags(default_flags)) {
        valid_flags_mask |= fNST_AnyAttr;

        if (app_domain.empty() && icache_client) {
            app_domain = icache_client.GetCacheName();
        }
    } else {
        // If no available underlying storages left
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "No storages available when using backend_storage=\"" <<
                backend_storage << "\" and flags=" << default_flags);
    }
}


ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags)
{
    _ASSERT(context);
    flags = context->DefaultFlags(flags);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool,
                    flags, context->app_domain, context->GetRandomNumber(),
                    context->filetrack_api.site.c_str()), context, flags));
}


ISelector::Ptr ISelector::Create(SContext* context, const string& object_loc)
{
    _ASSERT(context);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool, object_loc),
                context));
}


ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags,
        const string& service, Int8 id)
{
    _ASSERT(context);
    flags = context->DefaultFlags(flags);
    TObjLoc loc(context->compound_id_pool, flags, context->app_domain,
            context->GetRandomNumber(), context->filetrack_api.site.c_str());
    loc.SetServiceName(service);
    if (id) loc.SetObjectID(id);
    return Ptr(new CSelector(loc, context, flags));
}


ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags,
        const string& key)
{
    _ASSERT(context);
    flags = context->DefaultFlags(flags);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool,
                    flags, context->app_domain, key,
                    context->filetrack_api.site.c_str()), context, flags));
}


}


NCBI_PARAM_DEF(string, netstorage_api, backend_storage, "netcache, filetrack");


END_NCBI_SCOPE
