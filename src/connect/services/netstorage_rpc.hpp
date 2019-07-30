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
    SNetStorageRPC(SNetServerInPool* server, SNetStorageRPC* parent);

    SNetStorageObjectImpl* Create(TNetStorageFlags flags) override;
    SNetStorageObjectImpl* Open(const string& object_loc) override;

    CJsonNode Exchange(CNetService service,
            const CJsonNode& request,
            CNetServerConnection* conn = NULL,
            CNetServer::TInstance server_to_use = NULL) const;

    CJsonNode MkStdRequest(const string& request_type) const;
    CJsonNode MkObjectRequest(const string& request_type,
            const string& object_loc) const;
    CJsonNode MkObjectRequest(const string& request_type,
            const string& unique_key, TNetStorageFlags flags) const;
    EVoid x_InitNetCacheAPI();

    CNetService GetServiceIfLocator(const string& object_loc);

    TNetStorageFlags GetFlags(TNetStorageFlags flags) const
    {
        return flags ? flags : m_DefaultFlags;
    }

private:
    TNetStorageFlags m_DefaultFlags;

public:
    CNetService m_Service;

    const TConfig m_Config;

    mutable CAtomicCounter m_RequestNumber;

    CCompoundIDPool m_CompoundIDPool;

    CNetCacheAPI m_NetCacheAPI;

private:
    map<string, CNetService> m_ServiceMap;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSTORAGE_RPC__HPP */
