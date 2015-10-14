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
#include <misc/netstorage/netstorage.hpp>
#include "object.hpp"


BEGIN_NCBI_SCOPE


using namespace NImpl;


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


struct SNetStorageAPIImpl : public SNetStorageImpl
{
    SNetStorageAPIImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const IRegistry& registry)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, registry))
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


CObj* SNetStorageAPIImpl::OpenImpl(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    return new CObj(selector);
}


CNetStorageObject SNetStorageAPIImpl::Create(TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags));
    return new CObj(selector);
}


CNetStorageObject SNetStorageAPIImpl::Open(const string& object_loc)
{
    return OpenImpl(object_loc);
}


string SNetStorageAPIImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SNetStorageAPIImpl::Exists(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SNetStorageAPIImpl::Remove(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
}


CObj* SNetStorageAPIImpl::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, service, id));

    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    selector->SetLocator();

    return new CObj(selector);
}


struct SNetStorageByKeyAPIImpl : public SNetStorageByKeyImpl
{
    SNetStorageByKeyAPIImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const IRegistry& registry)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, registry))
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


CNetStorageObject SNetStorageByKeyAPIImpl::Open(const string& key,
        TNetStorageFlags flags)
{
    return OpenImpl(key, flags);
}


CObj* SNetStorageByKeyAPIImpl::OpenImpl(const string& key,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    return new CObj(selector);
}


string SNetStorageByKeyAPIImpl::Relocate(const string& key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, old_flags, key));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SNetStorageByKeyAPIImpl::Exists(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SNetStorageByKeyAPIImpl::Remove(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
}

CDirectNetStorage::CDirectNetStorage(
        const IRegistry& registry,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorage(
            new SNetStorageAPIImpl(app_domain, default_flags, icache_client,
                compound_id_pool, registry))
{}


CDirectNetStorageObject CDirectNetStorage::Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags)
{
    return Impl<SNetStorageAPIImpl>(m_Impl)->Create(flags, service_name, object_id);
}


CDirectNetStorageObject CDirectNetStorage::Open(const string& object_loc)
{
    return Impl<SNetStorageAPIImpl>(m_Impl)->OpenImpl(object_loc);
}


CDirectNetStorageByKey::CDirectNetStorageByKey(
        const IRegistry& registry,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorageByKey(
            new SNetStorageByKeyAPIImpl(app_domain, default_flags, icache_client,
                compound_id_pool, registry))
{
}


CDirectNetStorageObject CDirectNetStorageByKey::Open(const string& key,
        TNetStorageFlags flags)
{
    return Impl<SNetStorageByKeyAPIImpl>(m_Impl)->OpenImpl(key, flags);
}


string CDirectNetStorageRegistry::CFileTrack::GetSite(const IRegistry& registry)
{
    return SFileTrackAPI::GetSite(registry);
}


bool CDirectNetStorageRegistry::CFileTrack::SetSite(IRWRegistry& registry, const string& value)
{
    return SFileTrackAPI::SetSite(registry, value);
}


string CDirectNetStorageRegistry::CFileTrack::GetKey(const IRegistry& registry)
{
    return SFileTrackAPI::GetKey(registry);
}


bool CDirectNetStorageRegistry::CFileTrack::SetKey(IRWRegistry& registry, const string& value)
{
    return SFileTrackAPI::SetKey(registry, value);
}


END_NCBI_SCOPE
