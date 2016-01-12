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
 * Authors: Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>
#include "object.hpp"


BEGIN_NCBI_SCOPE


using namespace NDirectNetStorageImpl;


template <class TDerived, class TBaseRef>
TDerived* Impl(TBaseRef& base_ref)
{
    return static_cast<TDerived*>(base_ref.GetNonNullPointer());
}


CDirectNetStorageObject::CDirectNetStorageObject(EVoid)
    : CNetStorageObject(eVoid)
{
}


string CDirectNetStorageObject::Relocate(TNetStorageFlags flags)
{
    return Impl<CObj>(m_Impl)->Relocate(flags);
}


bool CDirectNetStorageObject::Exists()
{
    return Impl<CObj>(m_Impl)->Exists();
}


ENetStorageRemoveResult CDirectNetStorageObject::Remove()
{
    return Impl<CObj>(m_Impl)->Remove();
}


const CNetStorageObjectLoc& CDirectNetStorageObject::Locator()
{
    return Impl<CObj>(m_Impl)->Locator();
}


string CDirectNetStorageObject::FileTrack_Path()
{
    // TODO: Implement locking
    return kEmptyStr;
}


CDirectNetStorageObject::CDirectNetStorageObject(SNetStorageObjectImpl* impl)
    : CNetStorageObject(impl)
{}


struct SCombinedNetStorage
{
    typedef SCombinedNetStorageConfig TConfig;

    static SNetStorageImpl* CreateImpl(const string& init_string,
        TNetStorageFlags default_flags);

    static SNetStorageByKeyImpl* CreateByKeyImpl(const string& init_string,
        TNetStorageFlags default_flags);
};


struct SDirectNetStorageImpl : public SNetStorageImpl
{
    SDirectNetStorageImpl(const SCombinedNetStorageConfig& config,
            TNetStorageFlags default_flags)
        : m_Context(new SContext(config, default_flags))
    {
    }

    SDirectNetStorageImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
    }

    CObj* OpenImpl(const string&);

    CNetStorageObject Create(TNetStorageFlags = 0);
    CNetStorageObject Open(const string&);
    string Relocate(const string&, TNetStorageFlags);
    bool Exists(const string&);
    ENetStorageRemoveResult Remove(const string&);

    CObj* Create(TNetStorageFlags, const string&, Int8 = 0);
    CJsonNode ReportConfig() const;

private:
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections() { m_Context->AllowXSiteConnections(); }
#endif

    CRef<SContext> m_Context;
};


CObj* SDirectNetStorageImpl::OpenImpl(const string& object_loc)
{
    ISelector::Ptr selector(m_Context->Create(object_loc));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Create(TNetStorageFlags flags)
{
    ISelector::Ptr selector(m_Context->Create(flags));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Open(const string& object_loc)
{
    return OpenImpl(object_loc);
}


string SDirectNetStorageImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(m_Context->Create(object_loc));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageImpl::Exists(const string& object_loc)
{
    ISelector::Ptr selector(m_Context->Create(object_loc));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


ENetStorageRemoveResult SDirectNetStorageImpl::Remove(const string& object_loc)
{
    ISelector::Ptr selector(m_Context->Create(object_loc));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Remove();
}


CObj* SDirectNetStorageImpl::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    ISelector::Ptr selector(m_Context->Create(flags, service, id));

    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    selector->SetLocator();

    return new CObj(selector);
}


CJsonNode SDirectNetStorageImpl::ReportConfig() const
{
    CJsonNode result(CJsonNode::NewObjectNode());
    CJsonNode empty(CJsonNode::NewObjectNode());

    if (m_Context->icache_client) result.SetByKey("netcache", empty);
    if (m_Context->filetrack_api) result.SetByKey("filetrack", empty);

    return result;
}


struct SDirectNetStorageByKeyImpl : public SNetStorageByKeyImpl
{
    SDirectNetStorageByKeyImpl(const SCombinedNetStorageConfig& config,
            TNetStorageFlags default_flags)
        : m_Context(new SContext(config, default_flags))
    {
    }


    SDirectNetStorageByKeyImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
        if (app_domain.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "Domain name cannot be empty.");
        }
    }

    CObj* OpenImpl(const string&, TNetStorageFlags = 0);

    CNetStorageObject Open(const string&, TNetStorageFlags = 0);
    string Relocate(const string&, TNetStorageFlags, TNetStorageFlags = 0);
    bool Exists(const string&, TNetStorageFlags = 0);
    ENetStorageRemoveResult Remove(const string&, TNetStorageFlags = 0);

private:
    CRef<SContext> m_Context;
};


CNetStorageObject SDirectNetStorageByKeyImpl::Open(const string& key,
        TNetStorageFlags flags)
{
    return OpenImpl(key, flags);
}


CObj* SDirectNetStorageByKeyImpl::OpenImpl(const string& key,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(m_Context->Create(flags, key));
    return new CObj(selector);
}


string SDirectNetStorageByKeyImpl::Relocate(const string& key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    ISelector::Ptr selector(m_Context->Create(old_flags, key));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageByKeyImpl::Exists(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(m_Context->Create(flags, key));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


ENetStorageRemoveResult SDirectNetStorageByKeyImpl::Remove(const string& key,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(m_Context->Create(flags, key));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Remove();
}


string s_GetSection(const IRegistry& registry, const string& service,
        const string& name)
{
    if (!service.empty()) {
        const string section = "service_" + service;
        const string service_specific = registry.Get(section, name);

        if (!service_specific.empty()) {
            return service_specific;
        }
    }

    const string server_wide = registry.Get("netstorage_api", name);
    return server_wide;
}


SFileTrackConfig s_GetFTConfig(const IRegistry& registry, const string& service)
{
    const string ft_section = s_GetSection(registry, service, "filetrack");
    return ft_section.empty() ? eVoid : SFileTrackConfig(registry, ft_section);
}


CNetICacheClient s_GetICClient(const IRegistry& registry, const string& service)
{
    const string nc_section = s_GetSection(registry, service, "netcache");
    return nc_section.empty() ? eVoid : CNetICacheClient(registry, nc_section);
}


CDirectNetStorage::CDirectNetStorage(
        const IRegistry& registry,
        const string& service_name,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorage(
            new SDirectNetStorageImpl(app_domain, default_flags,
                s_GetICClient(registry, service_name),
                compound_id_pool, s_GetFTConfig(registry, service_name)))
{
}


CDirectNetStorageObject CDirectNetStorage::Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->Create(flags, service_name, object_id);
}


CDirectNetStorageObject CDirectNetStorage::Open(const string& object_loc)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->OpenImpl(object_loc);
}


CJsonNode CDirectNetStorage::ReportConfig() const
{
    return Impl<const SDirectNetStorageImpl>(m_Impl)->ReportConfig();
}


CDirectNetStorageByKey::CDirectNetStorageByKey(
        const IRegistry& registry,
        const string& service_name,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorageByKey(
            new SDirectNetStorageByKeyImpl(app_domain, default_flags,
                s_GetICClient(registry, service_name),
                compound_id_pool, s_GetFTConfig(registry, service_name)))
{
}


CDirectNetStorageObject CDirectNetStorageByKey::Open(const string& key,
        TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageByKeyImpl>(m_Impl)->OpenImpl(key, flags);
}


SNetStorageImpl* SCombinedNetStorage::CreateImpl(
        const string& init_string, TNetStorageFlags default_flags)
{
    TConfig config(TConfig::Build(init_string));

    return config.mode == TConfig::eDefault ?
        SNetStorage::CreateImpl(config, default_flags) :
        new SDirectNetStorageImpl(config, default_flags);
}


CCombinedNetStorage::CCombinedNetStorage(const string& init_string,
        TNetStorageFlags default_flags) :
    CNetStorage(
            SCombinedNetStorage::CreateImpl(init_string, default_flags))
{
}


SNetStorageByKeyImpl* SCombinedNetStorage::CreateByKeyImpl(
        const string& init_string, TNetStorageFlags default_flags)
{
    TConfig config(TConfig::Build(init_string));

    return config.mode == TConfig::eDefault ?
        SNetStorage::CreateByKeyImpl(config, default_flags) :
        new SDirectNetStorageByKeyImpl(config, default_flags);
}


CCombinedNetStorageByKey::CCombinedNetStorageByKey(const string& init_string,
        TNetStorageFlags default_flags) :
    CNetStorageByKey(
            SCombinedNetStorage::CreateByKeyImpl(init_string, default_flags))
{
}


END_NCBI_SCOPE
