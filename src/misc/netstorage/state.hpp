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

template <class TBase>
using TState = SNetStorageObjectState<SNetStorageObjectDirectState<TBase>>;


namespace NDirectNetStorageImpl
{

typedef CNetStorageObjectLoc TObjLoc;

class ILocation
{
public:
    virtual ~ILocation() {}

    virtual INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) = 0;
    virtual INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) = 0;
    virtual Uint8 GetSizeImpl() = 0;
    virtual CNetStorageObjectInfo GetInfoImpl() = 0;
    virtual bool ExistsImpl() = 0;
    virtual ENetStorageRemoveResult RemoveImpl() = 0;
    virtual void SetExpirationImpl(const CTimeout&) = 0;

    virtual string FileTrack_PathImpl() = 0;

    typedef pair<string, string> TUserInfo;
    virtual TUserInfo GetUserInfoImpl() = 0;

    virtual bool IsSame(const ILocation* other) const = 0;

protected:
    template <class TLocation>
    static const TLocation* To(const ILocation* location)
    {
        return dynamic_cast<const TLocation*>(location);
    }
};

struct SContext;

class ISelector
{
public:
    typedef auto_ptr<ISelector> Ptr;

    virtual ~ISelector() {}

    virtual ILocation* First() = 0;
    virtual ILocation* Next() = 0;
    virtual bool InProgress() const = 0;
    virtual void Restart() = 0;
    virtual TObjLoc& Locator() = 0;
    virtual void SetLocator() = 0;

    virtual ISelector* Clone(SNetStorageObjectImpl&, TNetStorageFlags) = 0;
    virtual const SContext& GetContext() const = 0;
};

struct SContext : CObject
{
    CNetICacheClientExt icache_client;
    SFileTrackAPI filetrack_api;
    CCompoundIDPool compound_id_pool;
    TNetStorageFlags default_flags = 0;
    string app_domain;
    size_t relocate_chunk;

    SContext(const SCombinedNetStorageConfig&, TNetStorageFlags);
    SContext(const string&, const string&,
            CCompoundIDPool::TInstance, const IRegistry&);

    TNetStorageFlags DefaultFlags(TNetStorageFlags flags) const
    {
        return flags ? flags : default_flags;
    }

    ISelector* Create(SNetStorageObjectImpl&, bool* cancel_relocate, const string&);
    ISelector* Create(SNetStorageObjectImpl&, TNetStorageFlags, const string& = kEmptyStr);
    ISelector* Create(SNetStorageObjectImpl&, bool* cancel_relocate, const string&, TNetStorageFlags, const string& = kEmptyStr);

private:
    void Init();

    CRandom m_Random;
};

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

template <class TBase>
class CLocatorHolding : public TBase
{
public:
    template <class... TArgs>
    CLocatorHolding(TObjLoc& object_loc, TArgs&&... args) :
        TBase(object_loc, std::forward<TArgs>(args)...),
        m_ObjectLoc(object_loc)
    {}

private:
    virtual TObjLoc& Locator() { return m_ObjectLoc; }

    TObjLoc& m_ObjectLoc;
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
    typedef auto_ptr<IReader> TReaderPtr;

    CRONetCache(bool* cancel_relocate) : CROState(cancel_relocate) {}

    void Set(TReaderPtr reader, size_t blob_size)
    {
        m_Reader = reader;
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
    typedef auto_ptr<IEmbeddedStreamWriter> TWriterPtr;

    void Set(TWriterPtr writer)
    {
        m_Writer = writer;
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

class CLocation : public ILocatorHolding<ILocation>
{
public:
    virtual void SetLocator() = 0;
};

class CNotFound : public CLocation
{
public:
    CNotFound(TObjLoc& object_loc, SNetStorageObjectImpl& fsm)
        : m_RW(fsm, object_loc)
    {}

    void SetLocator();

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    ENetStorageRemoveResult RemoveImpl();
    void SetExpirationImpl(const CTimeout&);
    string FileTrack_PathImpl();
    TUserInfo GetUserInfoImpl();

private:
    bool IsSame(const ILocation* other) const { return To<CNotFound>(other); }

    TState<CRWNotFound> m_RW;
};

class CNetCache : public CLocation
{
public:
    CNetCache(TObjLoc& object_loc, SNetStorageObjectImpl& fsm, SContext* context, bool* cancel_relocate)
        : m_Context(context),
          m_Read(fsm, object_loc, cancel_relocate),
          m_Write(fsm, object_loc)
    {}

    bool Init();
    void SetLocator();

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
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
    TState<CRONetCache> m_Read;
    TState<CWONetCache> m_Write;
};

class CFileTrack : public CLocation
{
public:
    CFileTrack(TObjLoc& object_loc, SNetStorageObjectImpl& fsm, SContext* context, bool* cancel_relocate)
        : m_Context(context),
          m_Read(fsm, object_loc, cancel_relocate),
          m_Write(fsm, object_loc)
    {}

    bool Init() { return m_Context->filetrack_api; }
    void SetLocator();

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*) override;
    INetStorageObjectState* StartWrite(const void*, size_t, size_t*, ERW_Result*) override;
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
    TState<CROFileTrack> m_Read;
    TState<CWOFileTrack> m_Write;
};

class CSelector : public ISelector
{
public:
    CSelector(SNetStorageObjectImpl& fsm, const TObjLoc&, SContext*, bool* cancel_relocate);
    CSelector(SNetStorageObjectImpl& fsm, const TObjLoc&, SContext*, bool* cancel_relocate, TNetStorageFlags);

    ILocation* First();
    ILocation* Next();
    bool InProgress() const;
    void Restart();
    TObjLoc& Locator();
    void SetLocator();
    ISelector* Clone(SNetStorageObjectImpl&, TNetStorageFlags);
    const SContext& GetContext() const;

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

}

END_NCBI_SCOPE

#endif
