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

#include <connect/services/netstorage.hpp>

BEGIN_NCBI_SCOPE

enum EDefaultStorage {
    eDefaultStorage_Undefined,
    eDefaultStorage_NetStorage,
    eDefaultStorage_NetCache,
    eDefaultStorage_NoCreate,
};

struct SNetStorageRPC : public SNetStorageImpl
{
    SNetStorageRPC(const string& init_string, TNetStorageFlags default_flags);

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0);
    virtual CNetStorageObject Open(const string& object_loc);
    virtual string Relocate(const string& object_loc, TNetStorageFlags flags);
    virtual bool Exists(const string& object_loc);
    virtual void Remove(const string& object_loc);

    CJsonNode Exchange(CNetService service,
            const CJsonNode& request,
            CNetServerConnection* conn = NULL,
            CNetServer::TInstance server_to_use = NULL);

    void x_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags);
    void x_SetICacheNames(CJsonNode& node);
    CJsonNode MkStdRequest(const string& request_type);
    CJsonNode MkObjectRequest(const string& request_type,
            const string& object_loc);
    CJsonNode MkObjectRequest(const string& request_type,
            const string& unique_key, TNetStorageFlags flags);
    void x_InitNetCacheAPI();
    bool x_NetCacheMode(const string& object_loc);

    CNetService GetServiceFromLocator(const string& object_loc)
    {
        CNetStorageObjectLoc locator_struct(m_CompoundIDPool, object_loc);
        string service_name = locator_struct.GetServiceName();

        return service_name.empty() ||
                service_name == m_Service.GetServiceName() ? m_Service :
                        m_ServiceMap.GetServiceByName(service_name, m_Service);
    }

    EDefaultStorage m_DefaultStorage;

    TNetStorageFlags m_DefaultFlags;
    CNetService m_Service;
    SNetServiceMap m_ServiceMap;

    string m_NetStorageServiceName;
    string m_NetCacheServiceName;
    string m_ClientName;
    string m_AppDomain;
    string m_MetadataOption;

    CAtomicCounter m_RequestNumber;

    CCompoundIDPool m_CompoundIDPool;

    CNetCacheAPI m_NetCacheAPI;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSTORAGE_RPC__HPP */
