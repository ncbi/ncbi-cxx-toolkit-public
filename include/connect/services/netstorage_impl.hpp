#ifndef CONNECT_SERVICES___NETSTORAGE_IMPL__HPP
#define CONNECT_SERVICES___NETSTORAGE_IMPL__HPP

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
 *   NetStorage implementation declarations.
 *
 */

#include "netstorage.hpp"
#include "compound_id.hpp"
#include "netcache_api.hpp"

#include <corelib/rwstream.hpp>
#include <connect/services/neticache_client.hpp>

BEGIN_NCBI_SCOPE

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageObjectImpl :
    public CObject,
    public IReader,
    public IEmbeddedStreamWriter
{
    /* IReader methods */
    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read) = 0;
    virtual ERW_Result PendingCount(size_t* count);

    /* IEmbeddedStreamWriter methods */
    virtual ERW_Result Write(const void* buf, size_t count,
            size_t* bytes_written) = 0;
    virtual ERW_Result Flush();
    virtual void Close() = 0;
    virtual void Abort();

    /* More overridable methods */
    virtual IReader& GetReader();
    virtual IEmbeddedStreamWriter& GetWriter();

    virtual string GetLoc() = 0;
    virtual void Read(string* data);
    virtual bool Eof() = 0;
    virtual Uint8 GetSize() = 0;
    virtual list<string> GetAttributeList() const = 0;
    virtual string GetAttribute(const string& attr_name) const = 0;
    virtual void SetAttribute(const string& attr_name,
            const string& attr_value) = 0;
    virtual CNetStorageObjectInfo GetInfo() = 0;
    virtual void SetExpiration(const CTimeout&) = 0;
};

struct NCBI_XCONNECT_EXPORT SNetStorageObjectRWStream : public CRWStream
{
    SNetStorageObjectRWStream(SNetStorageObjectImpl* nst_object) :
        CRWStream(nst_object, nst_object, /*buf_size*/ 0, /*buf*/ NULL,
                  CRWStreambuf::fLeakExceptions),
        m_NetStorageObject(nst_object)
    {
    }

    virtual ~SNetStorageObjectRWStream();

    CNetStorageObject m_NetStorageObject;
};

inline string CNetStorageObject::GetLoc()
{
    return m_Impl->GetLoc();
}

inline size_t CNetStorageObject::Read(void* buffer, size_t buf_size)
{
    size_t bytes_read;
    m_Impl->Read(buffer, buf_size, &bytes_read);
    return bytes_read;
}

inline void CNetStorageObject::Read(string* data)
{
    m_Impl->Read(data);
}

inline IReader& CNetStorageObject::GetReader()
{
    return m_Impl->GetReader();
}

inline bool CNetStorageObject::Eof()
{
    return m_Impl->Eof();
}

inline void CNetStorageObject::Write(const void* buffer, size_t buf_size)
{
    m_Impl->Write(buffer, buf_size, NULL);
}

inline void CNetStorageObject::Write(const string& data)
{
    m_Impl->Write(data.data(), data.length(), NULL);
}

inline IEmbeddedStreamWriter& CNetStorageObject::GetWriter()
{
    return m_Impl->GetWriter();
}

inline CNcbiIostream* CNetStorageObject::GetRWStream()
{
    return new SNetStorageObjectRWStream(m_Impl);
}

inline Uint8 CNetStorageObject::GetSize()
{
    return m_Impl->GetSize();
}

inline CNetStorageObject::TAttributeList CNetStorageObject::GetAttributeList() const
{
    return m_Impl->GetAttributeList();
}

inline string CNetStorageObject::GetAttribute(const string& attr_name) const
{
    return m_Impl->GetAttribute(attr_name);
}

inline void CNetStorageObject::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    m_Impl->SetAttribute(attr_name, attr_value);
}

inline CNetStorageObjectInfo CNetStorageObject::GetInfo()
{
    return m_Impl->GetInfo();
}

inline void CNetStorageObject::SetExpiration(const CTimeout& ttl)
{
    m_Impl->SetExpiration(ttl);
}

inline void CNetStorageObject::Close()
{
    m_Impl->Close();
}

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageImpl : public CObject
{
    virtual CNetStorageObject Create(TNetStorageFlags flags = 0) = 0;
    virtual CNetStorageObject Open(const string& object_loc) = 0;
    virtual string Relocate(const string& object_loc,
            TNetStorageFlags flags) = 0;
    virtual bool Exists(const string& object_loc) = 0;
    virtual void Remove(const string& object_loc) = 0;
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    virtual void AllowXSiteConnections() {}
#endif
};

inline CNetStorageObject CNetStorage::Create(TNetStorageFlags flags)
{
    return m_Impl->Create(flags);
}

inline CNetStorageObject CNetStorage::Open(const string& object_loc)
{
    return m_Impl->Open(object_loc);
}

inline CNetStorageObject CNetStorage::Open(const string& object_loc,
        TNetStorageFlags /*flags*/)
{
    return m_Impl->Open(object_loc);
}

inline string CNetStorage::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    return m_Impl->Relocate(object_loc, flags);
}

inline bool CNetStorage::Exists(const string& object_loc)
{
    return m_Impl->Exists(object_loc);
}

inline void CNetStorage::Remove(const string& object_loc)
{
    m_Impl->Remove(object_loc);
}

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageByKeyImpl : public CObject
{
    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0) = 0;
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0) = 0;
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0) = 0;
    virtual void Remove(const string& key, TNetStorageFlags flags = 0) = 0;
};

inline CNetStorageObject CNetStorageByKey::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return m_Impl->Open(unique_key, flags);
}

inline string CNetStorageByKey::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    return m_Impl->Relocate(unique_key, flags, old_flags);
}

inline bool CNetStorageByKey::Exists(const string& key, TNetStorageFlags flags)
{
    return m_Impl->Exists(key, flags);
}

inline void CNetStorageByKey::Remove(const string& key, TNetStorageFlags flags)
{
    m_Impl->Remove(key, flags);
}

/// @internal
template <TNetStorageFlags MASK>
class CNetStorageFlagsSubset
{
    typedef CNetStorageFlagsSubset TSelf;
public:
    CNetStorageFlagsSubset(TNetStorageFlags flags) : m_Flags(flags & MASK) {}
    TSelf& operator &=(TSelf flag) { m_Flags &= flag; return *this; }
    TSelf& operator |=(TSelf flag) { m_Flags |= flag; return *this; }
    TSelf& operator ^=(TSelf flag) { m_Flags ^= flag; return *this; }
    operator TNetStorageFlags() const { return m_Flags; }

private:
    TNetStorageFlags m_Flags;
};

typedef CNetStorageFlagsSubset<fNST_AnyLoc> TNetStorageLocFlags;
typedef CNetStorageFlagsSubset<fNST_AnyAttr> TNetStorageAttrFlags;

/// @internal
class NCBI_XCONNECT_EXPORT CNetStorageObjectLoc
{
public:
    CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
            TNetStorageAttrFlags flags,
            const string& app_domain,
            Uint8 random_number,
            const char* ft_site_name);
    CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
            TNetStorageAttrFlags flags,
            const string& app_domain,
            const string& unique_key,
            const char* ft_site_name);
    CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
            const string& object_loc);
    CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
            const string& object_loc, TNetStorageAttrFlags flags);

    void SetObjectID(Uint8 object_id)
    {
        m_LocatorFlags &= ~(TLocatorFlags) fLF_NoMetaData;
        m_LocatorFlags |= fLF_HasObjectID;
        m_ObjectID = object_id;
        if ((m_LocatorFlags & fLF_HasUserKey) == 0)
            x_SetUniqueKeyFromRandom();
        m_Dirty = true;
    }

    Uint8 GetObjectID() const {return m_ObjectID;}

    void SetServiceName(const string& service_name)
    {
        if (service_name.empty() ||
                strchr(service_name.c_str(), ':') != NULL)
            ClearLocatorFlags(fLF_NetStorageService);
        else {
            m_ServiceName = service_name;
            SetLocatorFlags(fLF_NetStorageService);
        }
        m_Dirty = true;
    }

    string GetServiceName() const {return m_ServiceName;}

    ENetStorageObjectLocation GetLocation() const {return m_Location;}

    bool IsMovable() const {return (m_LocatorFlags & fLF_Movable) != 0;}
    bool IsCacheable() const {return (m_LocatorFlags & fLF_Cacheable) != 0;}
    bool IsMetaDataDisabled() const
    {
        return (m_LocatorFlags & fLF_NoMetaData) != 0;
    }

    Int8 GetTimestamp() const {return m_Timestamp;}
    CTime GetCreationTime() const {return CTime(m_Timestamp);}
    Uint8 GetRandom() const {return m_Random;}

    bool HasUserKey() const {return (m_LocatorFlags & fLF_HasUserKey) != 0;}
    string GetAppDomain() const {return m_AppDomain;}
    string GetUserKey() const {return m_UserKey;}

    string GetICacheKey() const {return m_ICacheKey;}
    string GetUniqueKey() const {return m_UniqueKey;}

    void SetCacheChunkSize(size_t cache_chunk_size)
    {
        m_CacheChunkSize = cache_chunk_size;
        m_Dirty = true;
    }

    Uint8 GetCacheChunkSize() const {return m_CacheChunkSize;}

    void SetLocation_NetCache(const string& service_name,
        Uint4 server_ip, unsigned short server_port,
        bool allow_xsite_conn);

    string GetNCServiceName() const {return m_NCServiceName;}
    Uint4 GetNetCacheIP() const {return m_NetCacheIP;}
    Uint2 GetNetCachePort() const {return m_NetCachePort;}

    bool IsXSiteProxyAllowed() const
    {
        return (m_NCFlags & fNCF_AllowXSiteConn) != 0;
    }

    void SetLocation_FileTrack(const char* ft_site_name);

    string GetFileTrackURL() const;

    string GetLocator() const
    {
        if (m_Dirty)
            x_Pack();
        return m_Locator;
    }

    TNetStorageAttrFlags GetStorageAttrFlags() const;

    // Serialize to a JSON object.
    void ToJSON(CJsonNode& root) const;
    CJsonNode ToJSON() const;

private:
    enum ELocatorFlags {
        fLF_NetStorageService   = (1 << 0),
        fLF_NoMetaData          = (1 << 1),
        fLF_HasObjectID         = (1 << 2),
        fLF_HasUserKey          = (1 << 3),
        fLF_Movable             = (1 << 4),
        fLF_Cacheable           = (1 << 5),
        fLF_DevEnv              = (1 << 6),
        fLF_QAEnv               = (1 << 7),

        eLF_AttrFlags = (
                fLF_NoMetaData |
                fLF_Movable |
                fLF_Cacheable),
        eLF_FieldFlags = (
                fLF_NetStorageService |
                fLF_HasObjectID |
                fLF_HasUserKey |
                fLF_DevEnv |
                fLF_QAEnv)
    };
    typedef unsigned TLocatorFlags;

    enum ENetCacheFlags {
        fNCF_AllowXSiteConn     = (1 << 0),
        fNCF_ServerSpecified    = (1 << 1),
    };
    typedef unsigned TNetCacheFlags;

    void Parse(const string& object_loc);
    void x_SetFileTrackSite(const char* ft_site_name);
    void x_SetUniqueKeyFromRandom();
    void x_SetUniqueKeyFromUserDefinedKey();

    void x_Pack() const;
    void SetLocatorFlags(TLocatorFlags flags) {m_LocatorFlags |= flags;}
    void ClearLocatorFlags(TLocatorFlags flags) {m_LocatorFlags &= ~flags;}

    TLocatorFlags x_StorageFlagsToLocatorFlags(
            TNetStorageAttrFlags storage_flags);

    mutable CCompoundIDPool m_CompoundIDPool;

    TLocatorFlags m_LocatorFlags;

    Uint8 m_ObjectID;

    string m_ServiceName;
    string m_LocationCode;
    ENetStorageObjectLocation m_Location;

    string m_AppDomain;

    Int8 m_Timestamp;
    Uint8 m_Random;

    string m_UserKey;

    // Either "m_Timestamp-m_Random[-m_ObjectID]" or m_UserKey.
    string m_ICacheKey;
    // "m_AppDomain-m_ICacheKey"
    string m_UniqueKey;

    Uint8 m_CacheChunkSize;

    TNetCacheFlags m_NCFlags;
    string m_NCServiceName;
    Uint4 m_NetCacheIP;
    Uint2 m_NetCachePort;

    mutable bool m_Dirty;

    mutable string m_Locator;
};

/// @internal
class NCBI_XCONNECT_EXPORT CDNCNetStorage
{
public:
    static CNetStorageObject Create(CNetCacheAPI::TInstance nc_api);
    static CNetStorageObject Open(CNetCacheAPI::TInstance nc_api,
        const string& blob_key);
};

/// @internal
class CNetStorageServerError
{
public:
    enum EErrCode {
        eInvalidArgument                    = 1001,
        eMandatoryFieldsMissed              = 1002,
        eHelloRequired                      = 1003,
        eInvalidMessageType                 = 1004,
        eInvalidIncomingMessage             = 1005,
        ePrivileges                         = 1006,
        eInvalidMessageHeader               = 1007,
        eShuttingDown                       = 1008,
        eMessageAfterBye                    = 1009,
        eStorageError                       = 1010,
        eWriteError                         = 1011,
        eReadError                          = 1012,
        eInternalError                      = 1013,
        eNetStorageObjectNotFound           = 1014,
        eNetStorageAttributeNotFound        = 1015,
        eNetStorageAttributeValueNotFound   = 1016,
        eNetStorageClientNotFound           = 1017,
        eNetStorageObjectExpired            = 1018,
        eDatabaseError                      = 1019,
        eInvalidConfig                      = 1020,
        eRemoteObjectNotFound               = 1021,

        // Meta info involving operation requested while the service
        // is not configured for meta or HELLO metadata option conflict
        eInvalidMetaInfoRequest             = 1022,
        eUnknownError                       = 1023
    };
};

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        ENetStorageObjectLocation location,
        const CNetStorageObjectLoc* object_loc_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info);

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        const CJsonNode& object_info_node);

/// @internal
class NCBI_XCONNECT_EXPORT CDirectNetStorageObject : public CNetStorageObject
{
public:
    CDirectNetStorageObject(EVoid);
    string Relocate(TNetStorageFlags flags);
    bool Exists();
    void Remove();
    const CNetStorageObjectLoc& Locator();

private:
    CDirectNetStorageObject(SNetStorageObjectImpl* impl);
    friend class CDirectNetStorage;
    friend class CDirectNetStorageByKey;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SFileTrackConfig
{
    const string site;
    const string key;
    const STimeout read_timeout;
    const STimeout write_timeout;

    SFileTrackConfig(const IRegistry& registry, const string& section = kEmptyStr);
    SFileTrackConfig(const string& site, const string& key);
};

/// @internal
class NCBI_XCONNECT_EXPORT CDirectNetStorage : public CNetStorage
{
public:
    CDirectNetStorage(
        const SFileTrackConfig&     filetrack_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorageObject Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags);

    CNetStorageObject Create(TNetStorageFlags flags = 0)
    {
        return CNetStorage::Create(flags);
    }

    CDirectNetStorageObject Open(const string& object_loc);
};

/// @internal
class NCBI_XCONNECT_EXPORT CDirectNetStorageByKey : public CNetStorageByKey
{
public:
    CDirectNetStorageByKey(
        const SFileTrackConfig&     filetrack_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance  compound_id_pool,
        const string&               app_domain,
        TNetStorageFlags            default_flags = 0);

    CDirectNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSTORAGE_IMPL__HPP */
