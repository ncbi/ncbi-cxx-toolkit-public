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


template <class Derived, class Base>
Derived* Impl(CRef<Base, CNetComponentCounterLocker<Base> >& base_ref)
{
    return static_cast<Derived*>(base_ref.GetNonNullPointer());
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


void CDirectNetStorageObject::Remove()
{
    Impl<CObj>(m_Impl)->Remove();
}


const CNetStorageObjectLoc& CDirectNetStorageObject::Locator()
{
    return Impl<CObj>(m_Impl)->Locator();
}


CDirectNetStorageObject::CDirectNetStorageObject(SNetStorageObjectImpl* impl)
    : CNetStorageObject(impl)
{}


struct SCombinedNetStorage
{
    struct SConfig;

    static SNetStorageImpl* CreateImpl(const string& init_string,
        TNetStorageFlags default_flags);

    static SNetStorageByKeyImpl* CreateByKeyImpl(const string& init_string,
        TNetStorageFlags default_flags);
};


struct SCombinedNetStorage::SConfig : SNetStorage::SConfig
{
    enum EMode {
        eDefault,
        eServerless,
    };

    EMode mode;
    string ft_site;
    string ft_key;

    SConfig() : mode(eDefault) {}
    void ParseArg(const string&, const string&);

    static SConfig Build(const string& init_string)
    {
        return BuildImpl<SConfig>(init_string);
    }

private:
    static EMode GetMode(const string&);
};


SCombinedNetStorage::SConfig::EMode
SCombinedNetStorage::SConfig::GetMode(const string& value)
{
    if (NStr::CompareNocase(value, "direct") == 0)
        return eServerless;
    else
        return eDefault;
}


void SCombinedNetStorage::SConfig::ParseArg(const string& name,
        const string& value)
{
    if (name == "mode")
        mode = GetMode(value);
    if (name == "ft_site")
        ft_site = value;
    else if (name == "ft_key")
        ft_key = NStr::URLDecode(value);
    else
        SNetStorage::SConfig::ParseArg(name, value);
}


struct SDirectNetStorageImpl : public SNetStorageImpl
{
    typedef SCombinedNetStorage::SConfig TConfig;

    SDirectNetStorageImpl(const TConfig& config,
            TNetStorageFlags default_flags)
        : m_Context(new SContext(config.app_domain,
                    CNetICacheClient(config.nc_service, config.app_domain,
                        config.client_name),
                    default_flags, NULL,
                    SFileTrackConfig(config.ft_site, config.ft_key)))
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
    void Remove(const string&);

    CObj* Create(TNetStorageFlags, const string&, Int8 = 0);

private:
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections() { m_Context->AllowXSiteConnections(); }
#endif

    CRef<SContext> m_Context;
};


CObj* SDirectNetStorageImpl::OpenImpl(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Create(TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Open(const string& object_loc)
{
    return OpenImpl(object_loc);
}


string SDirectNetStorageImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageImpl::Exists(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SDirectNetStorageImpl::Remove(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
}


CObj* SDirectNetStorageImpl::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, service, id));

    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    selector->SetLocator();

    return new CObj(selector);
}


struct SDirectNetStorageByKeyImpl : public SNetStorageByKeyImpl
{
    typedef SCombinedNetStorage::SConfig TConfig;

    SDirectNetStorageByKeyImpl(const TConfig& config,
            TNetStorageFlags default_flags)
        : m_Context(new SContext(config.app_domain,
                    CNetICacheClient(config.nc_service, config.app_domain,
                        config.client_name),
                    default_flags, NULL,
                    SFileTrackConfig(config.ft_site, config.ft_key)))
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
    void Remove(const string&, TNetStorageFlags = 0);

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
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    return new CObj(selector);
}


string SDirectNetStorageByKeyImpl::Relocate(const string& key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, old_flags, key));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageByKeyImpl::Exists(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SDirectNetStorageByKeyImpl::Remove(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
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
    const string nc_section = s_GetSection(registry, service, "netcache_api");
    return nc_section.empty() ? eVoid :
        CNetICacheClient(CNetICacheClient::eAppRegistry, nc_section);
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


CDirectNetStorage::CDirectNetStorage(
        const SFileTrackConfig& ft_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorage(
            new SDirectNetStorageImpl(app_domain, default_flags, icache_client,
                compound_id_pool, ft_config))
{}


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


CDirectNetStorageByKey::CDirectNetStorageByKey(
        const SFileTrackConfig& ft_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorageByKey(
            new SDirectNetStorageByKeyImpl(app_domain, default_flags, icache_client,
                compound_id_pool, ft_config))
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
    SConfig config(SConfig::Build(init_string));

    return config.mode == SConfig::eDefault ?
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
    SConfig config(SConfig::Build(init_string));

    return config.mode == SConfig::eDefault ?
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
