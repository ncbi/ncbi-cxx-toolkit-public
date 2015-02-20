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
#include <misc/netstorage/error_codes.hpp>
#include <util/util_exception.hpp>
#include "object.hpp"

#define NCBI_USE_ERRCODE_X  NetStorage


BEGIN_NCBI_SCOPE


using namespace NImpl;


struct SNetStorageAPIImpl : public SNetStorageImpl
{
    SNetStorageAPIImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client = NULL)
        : m_Context(new SContext(app_domain, icache_client, default_flags))
    {}

    CNetStorageObject Create(TNetStorageFlags = 0);
    CNetStorageObject Create(TNetStorageFlags, const string&, Int8 = 0);
    CNetStorageObject Open(const string&);
    string Relocate(const string&, TNetStorageFlags);
    bool Exists(const string&);
    void Remove(const string&);

private:
    CRef<SContext> m_Context;
};


CNetStorageObject SNetStorageAPIImpl::Create(TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags));
    CRef<CObj> object(new CObj(selector));
    return object.GetPointerOrNull();
}


CNetStorageObject SNetStorageAPIImpl::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, service, id));
    CRef<CObj> object(new CObj(selector));
    return object.GetPointerOrNull();
}


CNetStorageObject SNetStorageAPIImpl::Open(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    return new CObj(selector);
}


string SNetStorageAPIImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    ISelector::Ptr orig_selector(ISelector::Create(m_Context, object_loc));
    ISelector::Ptr new_selector(ISelector::Create(m_Context, object_loc, flags));
    CRef<CObj> orig_file(new CObj(orig_selector));
    return orig_file->Relocate(new_selector);
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


struct SNetStorageByKeyAPIImpl : public SNetStorageByKeyImpl
{
    SNetStorageByKeyAPIImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client = NULL)
        : m_Context(new SContext(app_domain, icache_client, default_flags))
    {
        if (app_domain.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "Domain name cannot be empty.");
        }
    }

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
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    return new CObj(selector);
}


string SNetStorageByKeyAPIImpl::Relocate(const string& key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    ISelector::Ptr orig_selector(ISelector::Create(m_Context, old_flags, key));
    ISelector::Ptr new_selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> orig_file(new CObj(orig_selector));
    return orig_file->Relocate(new_selector);
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


// TODO: It might worth merging these two into one
// TODO: dynamic_cast is not good, needs reconsidering
CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags)
{
    SNetStorageImpl* impl = netstorage_api;
    SNetStorageAPIImpl* api_impl = dynamic_cast<SNetStorageAPIImpl*>(impl);
    _ASSERT(api_impl);
    return api_impl->Create(flags, service_name, object_id);
}


CNetStorageObject g_CreateNetStorageObject(
        CNetStorage netstorage_api,
        const string& service_name,
        TNetStorageFlags flags)
{
    SNetStorageImpl* impl = netstorage_api;
    SNetStorageAPIImpl* api_impl = dynamic_cast<SNetStorageAPIImpl*>(impl);
    _ASSERT(api_impl);
    return api_impl->Create(flags, service_name);
}


CNetStorage g_CreateNetStorage(
        CNetICacheClient::TInstance icache_client,
        const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(app_domain, default_flags, icache_client);
}


CNetStorage g_CreateNetStorage(const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(app_domain, default_flags);
}


CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string& app_domain, TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(app_domain, default_flags, icache_client);
}


CNetStorageByKey g_CreateNetStorageByKey(const string& app_domain,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(app_domain, default_flags);
}


END_NCBI_SCOPE
