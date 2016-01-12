#ifndef CONNECT_SERVICES__NETSTORAGE_RPC__HPP
#define CONNECT_SERVICES__NETSTORAGE_RPC__HPP

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
 *   Declarations for the NetStorage client API implementation.
 *
 */

#include "netservice_api_impl.hpp"

#include <connect/services/netcache_api.hpp>
#include <connect/services/impl/netstorage_impl.hpp>

BEGIN_NCBI_SCOPE

struct SNetStorageRPC : public SNetStorageImpl
{
    SNetStorageRPC(const TConfig& config, TNetStorageFlags default_flags);

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0);
    virtual CNetStorageObject Open(const string& object_loc);
    virtual string Relocate(const string& object_loc, TNetStorageFlags flags);
    virtual bool Exists(const string& object_loc);
    virtual ENetStorageRemoveResult Remove(const string& object_loc);

    CJsonNode Exchange(CNetService service,
            const CJsonNode& request,
            CNetServerConnection* conn = NULL,
            CNetServer::TInstance server_to_use = NULL) const;

    static void x_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags);
    void x_SetICacheNames(CJsonNode& node) const;
    CJsonNode MkStdRequest(const string& request_type) const;
    CJsonNode MkObjectRequest(const string& request_type,
            const string& object_loc) const;
    CJsonNode MkObjectRequest(const string& request_type,
            const string& unique_key, TNetStorageFlags flags) const;
    void x_InitNetCacheAPI();
    bool x_NetCacheMode(const string& object_loc);

    CNetService GetServiceFromLocator(const string& object_loc)
    {
        CNetStorageObjectLoc locator_struct(m_CompoundIDPool, object_loc);
        string service_name = locator_struct.GetServiceName();

        if (service_name.empty())
            return m_Service;

        // Clone will return itself if it has the same name and
        // insert would not do anything if such entry already exists,
        // so no need to check for existence.
        CNetService service(m_Service.Clone(service_name));
        m_ServiceMap.insert(make_pair(service_name, service));
        return service;
    }

    TNetStorageFlags m_DefaultFlags;
    CNetService m_Service;

    const TConfig m_Config;

    mutable CAtomicCounter m_RequestNumber;

    CCompoundIDPool m_CompoundIDPool;

    CNetCacheAPI m_NetCacheAPI;

private:
    map<string, CNetService> m_ServiceMap;

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections()
    {
        m_AllowXSiteConnections = true;
        m_Service.AllowXSiteConnections();
        if (m_NetCacheAPI) m_NetCacheAPI.GetService().AllowXSiteConnections();
    }

    bool m_AllowXSiteConnections;
#endif
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSTORAGE_RPC__HPP */
