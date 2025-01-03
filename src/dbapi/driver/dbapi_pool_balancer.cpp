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
* Author:  Aaron Ucko
*
* File Description:
*   Help distribute connections within a pool across servers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_pool_balancer.hpp>
#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/error_codes.hpp>

#include <numeric>
#include <random>

#define NCBI_USE_ERRCODE_X   Dbapi_PoolBalancer

BEGIN_NCBI_SCOPE

class CDBConnParams_DNC : public CDBConnParamsDelegate
{
public:
    CDBConnParams_DNC(const CDBConnParams& other)
        : CDBConnParamsDelegate(other)
        { }

    string GetParam(const string& key) const
        {
            if (key == "do_not_connect") {
                return "true";
            } else {
                return CDBConnParamsDelegate::GetParam(key);
            }
        }
};


class CDB_FakeConnection : public IBalanceable
{
public:
    CDB_FakeConnection(TSvrRef server)
        : m_Server(server)
        { _ASSERT(server.NotEmpty()); }

    const string& ServerName(void) const override
        { return m_Server->GetName(); }
    Uint4 Host(void) const override
        { return m_Server->GetHost(); }
    Uint2 Port(void) const override
        { return m_Server->GetPort(); }

private:
    TSvrRef m_Server;
};
    

CDBPoolBalancer::CDBPoolBalancer(IDBServiceInfo& service_info,
                                 const string& pool_name,
                                 I_DriverContext* driver_ctx,
                                 bool is_pooled)
    : CPoolBalancer(service_info.GetServiceName(), service_info.GetOptions(),
                    x_GetFlags(driver_ctx, is_pooled)),
      m_ServiceInfo(&service_info), m_PoolName(pool_name),
      m_DriverCtx(driver_ctx)
{
    x_ReinitFromCounts();
}


CPoolBalancer::TFlags CDBPoolBalancer::x_GetFlags(I_DriverContext* driver_ctx,
                                                  bool is_pooled)
{
    TFlags flags = static_cast<EFlags>(0);
    if (driver_ctx != nullptr
        &&  !NStr::StartsWith(driver_ctx->GetDriverName(), "ftds")) {
        flags |= fIgnoreRawIPs;
    }
    if ( !is_pooled ) {
        flags |= fNoPooling;
    }
    return flags;
}


void CDBPoolBalancer::x_ReinitFromCounts(void)
{
    const impl::CDriverContext* ctx_impl
        = dynamic_cast<const impl::CDriverContext*>(m_DriverCtx);
    impl::CDriverContext::TCounts counts;
    if (ctx_impl == NULL) {
        if (m_DriverCtx != nullptr) {
            ERR_POST_X(1, Warning <<
                       "Called with non-standard IDriverContext");
        }
    } else if (x_NoPooling()) {
    } else if (m_PoolName.empty()) {
        ctx_impl->GetCountsForService(m_ServiceInfo->GetServiceName(),
                                      &counts);
    } else {
        ctx_impl->GetCountsForPool(m_PoolName, &counts);
    }
    x_InitFromCounts(counts);
}


IBalanceable* CDBPoolBalancer::x_TryPool(const void* params_in)
{
    auto params = static_cast<const CDBConnParams*>(params_in);
    _ASSERT(params != nullptr);
    if (m_DriverCtx == nullptr) {
        return nullptr;
    } else if (x_NoPooling()) {
        auto server = m_ServiceInfo->GetMappedServer();
        if (server.Empty()) {
            return nullptr;
        } else {
            CFastMutexGuard guard(m_FakeConnLock);
            auto it = m_FakeConnections.lower_bound(server);
            if (it == m_FakeConnections.end()  ||  it->first != server) {
                it = m_FakeConnections.emplace_hint
                    (it, server, new CDB_FakeConnection(server));
            }
            return it->second.get();
        }
    } else {
        CDBConnParams_DNC dnc_params(*params);
        IBalanceable * conn = nullptr;
        {{
            IDBServiceInfo::TAntiGuard anti_guard(*m_ServiceInfo);
            conn = IDBConnectionFactory::CtxMakeConnection(*m_DriverCtx,
                                                           dnc_params);
        }}
        if (conn == nullptr) {
            x_ReinitFromCounts();
        }
        return conn;
    }
}


unsigned int CDBPoolBalancer::x_GetCount(const void* params_in,
                                         const string& name)
{
    auto params = static_cast<const CDBConnParams*>(params_in);
    _ASSERT(params != nullptr);
    string pool_name = params->GetParam("pool_name");
    return m_DriverCtx->NofConnections(name, pool_name);
}


unsigned int CDBPoolBalancer::x_GetPoolMax(const void* params_in)
{
    auto params = static_cast<const CDBConnParams*>(params_in);
    _ASSERT(params != nullptr);
    string        pool_max_str  = params->GetParam("pool_maxsize");
    unsigned int  pool_max      = 0u;
    if ( !pool_max_str.empty()  &&  pool_max_str != "default") {
        NStr::StringToNumeric(pool_max_str, &pool_max,
                              NStr::fConvErr_NoThrow);
    }
    return pool_max;
}


void CDBPoolBalancer::x_Discard(const void* params_in, IBalanceable* conn_in)
{
    auto params = static_cast<const CDBConnParams*>(params_in);
    _ASSERT(params != nullptr);
    auto conn = dynamic_cast<const CDB_Connection*>(conn_in);
    if (conn == nullptr) {
        _ASSERT(dynamic_cast<const CDB_FakeConnection*>(conn_in) != nullptr);
        return;
    }
    _TRACE("Proceeding to request turnover");
    string  server_name   = conn->ServerName();
    bool    was_reusable  = conn->IsReusable();
    delete conn;
    if (was_reusable) {
        // This call might not close the exact connection we
        // considered, but closing any connection to the
        // relevant server is sufficient here.
        m_DriverCtx->CloseUnusedConnections
            (server_name, params->GetParam("pool_name"), 1u);
    }
}


END_NCBI_SCOPE
