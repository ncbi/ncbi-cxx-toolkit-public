#ifndef CORELIB___NCBI_POOL_BALANCER__HPP
#define CORELIB___NCBI_POOL_BALANCER__HPP

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
 */

/// @file ncbi_pool_balancer.hpp
/// Help distribute connections within a pool across servers.

#include <corelib/impl/ncbi_dbsvcmapper.hpp>

BEGIN_NCBI_SCOPE


class IBalanceable
{
public:
    virtual const string& ServerName(void) const = 0;
    virtual Uint4 Host(void) const = 0;
    virtual Uint2 Port(void) const = 0;
};


class NCBI_XNCBI_EXPORT CPoolBalancer : public CObject
{
public:
    CPoolBalancer(const string& service_name,
                  const IDBServiceMapper::TOptions& options,
                  bool ignore_raw_ips);

    TSvrRef GetServer(void)
        { return x_GetServer(nullptr, nullptr); }

protected:
    typedef map<string, unsigned int> TCounts; // by server name

    void x_InitFromCounts(const TCounts& counts);
    TSvrRef x_GetServer(const void* params, IBalanceable** conn);

    virtual IBalanceable* x_TryPool(const void* params)
        { return nullptr; }
    virtual unsigned int x_GetCount(const void* params, const string& name)
        { return 0u; }
    virtual unsigned int x_GetPoolMax(const void* params)
        { return 0u; }
    virtual void x_Discard(const void* params, IBalanceable* conn)
        { }
    
private:
    struct SEndpointInfo {
        SEndpointInfo()
            : effective_ranking(0.0), ideal_count(0.0), actual_count(0U),
              penalty_level(0U)
            { }
        
        CRef<CDBServerOption>  ref;
        double                 effective_ranking;
        double                 ideal_count;
        unsigned int           actual_count;
        unsigned int           penalty_level;
    };
    typedef map<CEndpointKey, SEndpointInfo> TEndpoints;

    CEndpointKey x_NameToKey(CTempString& name) const;
    
    string            m_ServiceName;
    TEndpoints        m_Endpoints;
    multiset<double>  m_Rankings;
    unsigned int      m_TotalCount;
    bool              m_IgnoreRawIPs;
};


END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_POOL_BALANCER__HPP */
