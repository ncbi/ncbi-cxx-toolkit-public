#ifndef MISC_NETSTORAGE___NETSTORAGEIMPL__HPP
#define MISC_NETSTORAGE___NETSTORAGEIMPL__HPP

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

#include <connect/services/compound_id.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/netstorage.hpp>
#include "filetrack.hpp"


BEGIN_NCBI_SCOPE


struct SCombinedNetStorageConfig : SNetStorage::SConfig
{
    enum EMode {
        eDefault,
        eServerless,
    };

    EMode mode;
    SFileTrackConfig ft;

    SCombinedNetStorageConfig() : mode(eDefault) {}
    void ParseArg(const string&, const string&);

    static SCombinedNetStorageConfig Build(const string& init_string)
    {
        return BuildImpl<SCombinedNetStorageConfig>(init_string);
    }

private:
    static EMode GetMode(const string&);
};

template <class TBase>
struct SNetStorageObjectDirectState : TBase
{
    template <class... TArgs>
    SNetStorageObjectDirectState(CNetStorageObjectLoc& locator, TArgs&&... args) :
        TBase(std::forward<TArgs>(args)...),
        m_Locator(locator)
    {
    }

    string GetLoc() const override           { return m_Locator.GetLocator(); }
    CNetStorageObjectLoc& Locator() override { return m_Locator; }

private:
    CNetStorageObjectLoc& m_Locator;
};


namespace NDirectNetStorageImpl
{

class ILocation
{
public:
    virtual ~ILocation() {}

    virtual INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) = 0;
    virtual INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) = 0;
    virtual Uint8 GetSize() = 0;
    virtual CNetStorageObjectInfo GetInfo() = 0;
    virtual bool Exists() = 0;
    virtual ENetStorageRemoveResult Remove() = 0;
    virtual void SetExpiration(const CTimeout&) = 0;
    virtual string FileTrack_Path() = 0;
    virtual pair<string, string> GetUserInfo() = 0;
    virtual string GetLoc() const = 0;
    virtual CNetStorageObjectLoc& Locator() = 0;
    virtual void SetLocator() = 0;
};

using CLocation = SNetStorageObjectDirectState<ILocation>;

template <class TBase>
using TState = SNetStorageObjectState<SNetStorageObjectDirectState<TBase>>;

struct SContext : CObject
{
    CNetICacheClientExt icache_client;
    SFileTrackAPI filetrack_api;
    CCompoundIDPool compound_id_pool;
    TNetStorageFlags default_flags = 0;
    string app_domain;
    size_t relocate_chunk;
    CRandom random;

    SContext(const SCombinedNetStorageConfig&, TNetStorageFlags);
    SContext(const string&, const string&,
            CCompoundIDPool::TInstance, const IRegistry&);

    CNetStorageObjectLoc Create(TNetStorageFlags flags)
    {
        return CNetStorageObjectLoc(compound_id_pool, flags, app_domain, random.GetRandUint8(), filetrack_api.config.site);
    }

    CNetStorageObjectLoc Create(const string& key, TNetStorageFlags flags, const CNetStorageObjectLoc::TVersion& version = 0, const string& subkey = kEmptyStr)
    {
        return CNetStorageObjectLoc(compound_id_pool, flags, app_domain, key, filetrack_api.config.site, version, subkey);
    }

private:
    void Init();
};

class CROState : public SNetStorageObjectIState
{
public:
    CROState(bool* cancel_relocate) : m_CancelRelocate(cancel_relocate) {}
    void CancelRelocate() override { _ASSERT(m_CancelRelocate); *m_CancelRelocate = true; }

private:
    bool* m_CancelRelocate;
};

class CRWNotFound : public SNetStorageObjectIoState
{
public:
    ERW_Result Read(void* buf, size_t count, size_t* read) override;
    ERW_Result PendingCount(size_t* count) override;
    bool Eof() override;
    ERW_Result Write(const void* buf, size_t count, size_t* written) override;
    ERW_Result Flush() override { return eRW_Success; }

    void Close() override { ExitState(); }
    void Abort() override { ExitState(); }
};

class CRONetCache : public CROState
{
public:
    typedef unique_ptr<IReader> TReaderPtr;

    CRONetCache(bool* cancel_relocate) : CROState(cancel_relocate) {}

    void Set(TReaderPtr reader, size_t blob_size)
    {
        m_Reader = move(reader);
        m_BlobSize = blob_size;
        m_BytesRead = 0;
    }

    ERW_Result Read(void* buf, size_t count, size_t* read) override;
    ERW_Result PendingCount(size_t* count) override;
    bool Eof() override;

    void Close() override;
    void Abort() override;

private:
    TReaderPtr m_Reader;
    size_t m_BlobSize;
    size_t m_BytesRead;
};

class CWONetCache : public SNetStorageObjectOState
{
public:
    typedef unique_ptr<IEmbeddedStreamWriter> TWriterPtr;

    void Set(TWriterPtr writer)
    {
        m_Writer = move(writer);
    }

    ERW_Result Write(const void* buf, size_t count, size_t* written) override;
    ERW_Result Flush() override;

    void Close() override;
    void Abort() override;

private:
    TWriterPtr m_Writer;
};

class CROFileTrack : public CROState
{
public:
    typedef CRef<SFileTrackDownload> TRequest;

    CROFileTrack(bool* cancel_relocate) : CROState(cancel_relocate) {}

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result Read(void* buf, size_t count, size_t* read) override;
    ERW_Result PendingCount(size_t* count) override;
    bool Eof() override;

    void Close() override;
    void Abort() override;

private:
    TRequest m_Request;
};

class CWOFileTrack : public SNetStorageObjectOState
{
public:
    typedef CRef<SFileTrackUpload> TRequest;

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result Write(const void* buf, size_t count, size_t* written) override;
    ERW_Result Flush() override;

    void Close() override;
    void Abort() override;

private:
    TRequest m_Request;
};

class CNotFound : public CLocation
{
public:
    CNotFound(CNetStorageObjectLoc& object_loc, SNetStorageObjectImpl& fsm)
        : CLocation(object_loc),
          m_RW(fsm, object_loc)
    {}

    void SetLocator() override;

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
    Uint8 GetSize() override;
    CNetStorageObjectInfo GetInfo() override;
    bool Exists() override;
    ENetStorageRemoveResult Remove() override;
    void SetExpiration(const CTimeout&) override;
    string FileTrack_Path() override;
    pair<string, string> GetUserInfo() override;

private:
    TState<CRWNotFound> m_RW;
};

class CNetCache : public CLocation
{
public:
    CNetCache(CNetStorageObjectLoc& object_loc, SNetStorageObjectImpl& fsm, SContext* context, bool* cancel_relocate)
        : CLocation(object_loc),
          m_Context(context),
          m_Read(fsm, object_loc, cancel_relocate),
          m_Write(fsm, object_loc)
    {}

    bool Init();
    void SetLocator() override;

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
    Uint8 GetSize() override;
    CNetStorageObjectInfo GetInfo() override;
    bool Exists() override;
    ENetStorageRemoveResult Remove() override;
    void SetExpiration(const CTimeout&) override;
    string FileTrack_Path() override;
    pair<string, string> GetUserInfo() override;

private:
    CRef<SContext> m_Context;
    CNetICacheClientExt m_Client;
    TState<CRONetCache> m_Read;
    TState<CWONetCache> m_Write;
};

class CFileTrack : public CLocation
{
public:
    CFileTrack(CNetStorageObjectLoc& object_loc, SNetStorageObjectImpl& fsm, SContext* context, bool* cancel_relocate)
        : CLocation(object_loc),
          m_Context(context),
          m_Read(fsm, object_loc, cancel_relocate),
          m_Write(fsm, object_loc)
    {}

    bool Init() { return m_Context->filetrack_api; }
    void SetLocator() override;

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
    Uint8 GetSize() override;
    CNetStorageObjectInfo GetInfo() override;
    bool Exists() override;
    ENetStorageRemoveResult Remove() override;
    void SetExpiration(const CTimeout&) override;
    string FileTrack_Path() override;
    pair<string, string> GetUserInfo() override;

private:
    CRef<SContext> m_Context;
    TState<CROFileTrack> m_Read;
    TState<CWOFileTrack> m_Write;
};

}

END_NCBI_SCOPE

#endif
