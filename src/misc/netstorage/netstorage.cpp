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


string CDirectNetStorageObject::Relocate(TNetStorageFlags flags,
        TNetStorageProgressCb cb)
{
    return m_Impl--->Relocate(flags, cb);
}


void CDirectNetStorageObject::CancelRelocate()
{
    return m_Impl--->CancelRelocate();
}


ENetStorageRemoveResult CDirectNetStorageObject::Remove()
{
    return m_Impl--->Remove();
}


const CNetStorageObjectLoc& CDirectNetStorageObject::Locator()
{
    return m_Impl--->Locator();
}


string CDirectNetStorageObject::FileTrack_Path()
{
    return m_Impl--->FileTrack_Path();
}


pair<string, string> CDirectNetStorageObject::GetUserInfo()
{
    return m_Impl--->GetUserInfo();
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

    SNetStorageObjectImpl* Create(TNetStorageFlags);
    SNetStorageObjectImpl* Open(const string&);

    // For direct NetStorage API only
    SDirectNetStorageImpl(const string&, CCompoundIDPool::TInstance,
            const IRegistry&);
    SNetStorageObjectImpl* Create(TNetStorageFlags, const string&);
    bool Exists(const string& db_loc, const string& client_loc);
    CJsonNode ReportConfig() const;

private:
    CRef<SContext> m_Context;
};


SNetStorageObjectImpl* SDirectNetStorageImpl::Create(TNetStorageFlags flags)
{
    if (!flags) flags = m_Context->default_flags;

    return SNetStorageObjectImpl::Create<CObj>(m_Context, m_Context->Create(flags), flags);
}


SNetStorageObjectImpl* SDirectNetStorageImpl::Open(const string& object_loc)
{
    CNetStorageObjectLoc loc(m_Context->compound_id_pool, object_loc);

    return SNetStorageObjectImpl::Create<CObj>(m_Context, loc, loc.GetStorageAttrFlags(), true, loc.GetLocation());
}


void s_SetServiceName(CNetStorageObjectLoc& loc, const string& service)
{
    // Do not set fake service name used by NST health check script
    if (NStr::CompareNocase(service, "LBSMDNSTTestService")) {
        loc.SetServiceName(service);
    }
}


SNetStorageObjectImpl* SDirectNetStorageImpl::Create(TNetStorageFlags flags, const string& service)
{
    if (!flags) flags = m_Context->default_flags;

    CNetStorageObjectLoc loc = m_Context->Create(flags);
    s_SetServiceName(loc, service);

    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    auto l = [&](CObj& state) { state.SetLocator(); };

    return SNetStorageObjectImpl::CreateAndStart<CObj>(l, m_Context, loc, flags);
}


SDirectNetStorageImpl::SDirectNetStorageImpl(const string& service_name,
        CCompoundIDPool::TInstance compound_id_pool,
        const IRegistry& registry)
    // NetStorage server does not get app_domain from clients
    // if object locators are used (opposed to object keys).
    // Instead of introducing new configuration parameter
    // we notify the context that app_domain not available (thus kEmptyStr).
    : m_Context(new SContext(service_name, kEmptyStr,
                compound_id_pool, registry))
{
}


bool s_TrustOldLoc(CCompoundIDPool& pool, const string& old_loc,
        TNetStorageFlags new_flags)
{
    if (old_loc.empty()) return false;

    auto old_location = CNetStorageObjectLoc(pool, old_loc).GetLocation();

    // Old location is unknown/not found
    if (old_location == eNFL_Unknown) return false;
    if (old_location == eNFL_NotFound) return false;

    // New location is unknown
    if (!(new_flags & fNST_AnyLoc)) return true;

    // Even if new location is wrong,
    // the object should be still accessible (due to "movable")
    if (new_flags & fNST_Movable) return true;

    // Both locations are known, check for discrepancies

    // Old location is NetCache
    if (old_location == eNFL_NetCache) {
        return new_flags & fNST_NetCache;
    }

    // Old location is FileTrack
    return new_flags & fNST_FileTrack;
}

bool SDirectNetStorageImpl::Exists(const string& db_loc, const string& client_loc)
{
    CNetStorageObjectLoc new_loc(m_Context->compound_id_pool, client_loc);
    TNetStorageFlags flags = new_loc.GetStorageAttrFlags();

    if (new_loc.GetLocation() == eNFL_NetCache) {
        flags |= fNST_NetCache;
    } else if (new_loc.GetLocation() == eNFL_FileTrack) {
        flags |= fNST_FileTrack;
    }

    if (s_TrustOldLoc(m_Context->compound_id_pool, db_loc, flags)) {
        return true;
    }

    CNetStorageObject object = Open(client_loc);
    return object--->Exists();
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

    SNetStorageObjectImpl* Open(const string&, TNetStorageFlags);

    // For direct NetStorage API only
    SDirectNetStorageByKeyImpl(const string&, const string&,
            CCompoundIDPool::TInstance, const IRegistry&);
    SNetStorageObjectImpl* Open(TNetStorageFlags, const string&);
    bool Exists(const string& db_loc, const string& key, TNetStorageFlags flags);

private:
    CRef<SContext> m_Context;
    const string m_ServiceName;
};


SNetStorageObjectImpl* SDirectNetStorageByKeyImpl::Open(const string& key,
        TNetStorageFlags flags)
{
    if (!flags) flags = m_Context->default_flags;

    return SNetStorageObjectImpl::Create<CObj>(m_Context, m_Context->Create(key, flags), flags, true);
}


SDirectNetStorageByKeyImpl::SDirectNetStorageByKeyImpl(
        const string& service_name,
        const string& app_domain,
        CCompoundIDPool::TInstance compound_id_pool,
        const IRegistry& registry)
    : m_Context(new SContext(service_name, app_domain,
                compound_id_pool, registry)),
        m_ServiceName(service_name)
{
    if (app_domain.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Domain name cannot be empty.");
    }
}


SNetStorageObjectImpl* SDirectNetStorageByKeyImpl::Open(TNetStorageFlags flags, const string& key)
{
    if (!flags) flags = m_Context->default_flags;

    CNetStorageObjectLoc loc = m_Context->Create(key, flags);
    s_SetServiceName(loc, m_ServiceName);

    return SNetStorageObjectImpl::Create<CObj>(m_Context, loc, flags, true);
}


bool SDirectNetStorageByKeyImpl::Exists(const string& db_loc,
        const string& key, TNetStorageFlags flags)
{
    if (s_TrustOldLoc(m_Context->compound_id_pool, db_loc, flags)) {
        return true;
    }

    CNetStorageObject object = Open(key, flags);
    return object--->Exists();
}


CDirectNetStorage::CDirectNetStorage(
        const IRegistry& registry,
        const string& service_name,
        CCompoundIDPool::TInstance compound_id_pool)
    : CNetStorage(
            new SDirectNetStorageImpl(service_name, compound_id_pool, registry))
{
}


CDirectNetStorageObject CDirectNetStorage::Create(const string& service_name,
        TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->Create(flags, service_name);
}


CDirectNetStorageObject CDirectNetStorage::Open(const string& object_loc)
{
    return m_Impl->Open(object_loc);
}


bool CDirectNetStorage::Exists(const string& db_loc, const string& client_loc)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->Exists(db_loc, client_loc);
}


CJsonNode CDirectNetStorage::ReportConfig() const
{
    return Impl<const SDirectNetStorageImpl>(m_Impl)->ReportConfig();
}


CDirectNetStorageByKey::CDirectNetStorageByKey(
        const IRegistry& registry,
        const string& service_name,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain)
    : CNetStorageByKey(
            new SDirectNetStorageByKeyImpl(service_name, app_domain,
                compound_id_pool, registry))
{
}


CDirectNetStorageObject CDirectNetStorageByKey::Open(const string& key,
        TNetStorageFlags flags)
{
    SNetStorage::SLimits::Check<SNetStorage::SLimits::SUserKey>(key);
    return Impl<SDirectNetStorageByKeyImpl>(m_Impl)->Open(flags, key);
}


bool CDirectNetStorageByKey::Exists(const string& db_loc,
        const string& key, TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageByKeyImpl>(m_Impl)->Exists(db_loc, key, flags);
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
