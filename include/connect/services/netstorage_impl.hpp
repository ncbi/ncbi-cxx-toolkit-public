#ifndef CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP
#define CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP

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

#include <corelib/ncbi_url.hpp>
#include <connect/services/netstorage.hpp>

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

/// @internal
template <class TConfig>
struct SConfigBuilder
{
    SConfigBuilder(const string& init_string) :
        m_InitString(init_string)
    {}

    operator TConfig() const
    {
        CUrlArgs url_parser(m_InitString);
        TConfig cfg;

        ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
            if (!field->name.empty()) cfg.ParseArg(field->name, field->value);
        }

        cfg.Validate(m_InitString);
        return cfg;
    }

private:
    const string m_InitString;
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageImpl : public CObject
{
    struct SConfig;

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0) = 0;
    virtual CNetStorageObject Open(const string& object_loc) = 0;
    virtual string Relocate(const string& object_loc,
            TNetStorageFlags flags) = 0;
    virtual bool Exists(const string& object_loc) = 0;
    virtual void Remove(const string& object_loc) = 0;
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    virtual void AllowXSiteConnections() {}
#endif

    static SNetStorageImpl* Create(const string&, TNetStorageFlags);
};

struct SNetStorageImpl::SConfig
{
    enum EDefaultStorage {
        eUndefined,
        eNetStorage,
        eNetCache,
        eNoCreate,
    };

    string service;
    string nc_service;
    string app_domain;
    string client_name;
    string metadata;
    EDefaultStorage default_storage;

    SConfig() : default_storage(eUndefined) {}
    bool ParseArg(const string&, const string&);
    void Validate(const string&);

private:
    static EDefaultStorage GetDefaultStorage(const string&);
};

/// @internal
struct NCBI_XCONNECT_EXPORT SNetStorageByKeyImpl : public CObject
{
    typedef SNetStorageImpl::SConfig SConfig;

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0) = 0;
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0) = 0;
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0) = 0;
    virtual void Remove(const string& key, TNetStorageFlags flags = 0) = 0;

    static SNetStorageByKeyImpl* Create(const string&, TNetStorageFlags);
};

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        ENetStorageObjectLocation location,
        const CNetStorageObjectLoc* object_loc_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info);

NCBI_XCONNECT_EXPORT
CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        const CJsonNode& object_info_node);

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES_IMPL__NETSTORAGE_IMPL__HPP */
