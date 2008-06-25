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
 * File Description:
 *   Contains definitions related to server discovery by the load balancer.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/srv_discovery.hpp>
#include <connect/services/srv_connections_expt.hpp>

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_service.h>

#include <corelib/ncbi_config.hpp>

BEGIN_NCBI_SCOPE

void QueryLoadBalancer(
    const std::string& service,
    TDiscoveredServers& servers,
    bool include_suppressed)
{
    SConnNetInfo* net_info =
        ConnNetInfo_Create(service.c_str());

    SERV_ITER srv_it = SERV_Open(service.c_str(),
        include_suppressed ? fSERV_Any | fSERV_IncludeSuppressed : fSERV_Any,
            0, net_info);

    ConnNetInfo_Destroy(net_info);

    if (srv_it == 0) {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound,
            "Load balancer cannot find service name " + service + ".");
    }

    const SSERV_Info* sinfo;

    while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0)
        servers.push_back(TServerAddress(
            CSocketAPI::ntoa(sinfo->host), sinfo->port));

    SERV_Close(srv_it);
}

class CSimpleRebalanceStrategy : public IRebalanceStrategy
{
public:
    CSimpleRebalanceStrategy(int rebalance_requests, int rebalance_time)
        : m_RebalanceRequests(rebalance_requests), m_RebalanceTime(rebalance_time),
          m_RequestCounter(0), m_LastRebalanceTime(0) {}

    virtual bool NeedRebalance() {
        CFastMutexGuard g(m_Mutex);
        time_t curr = time(0);
        if ( !m_LastRebalanceTime ||
             (m_RebalanceTime && int(curr - m_LastRebalanceTime) >= m_RebalanceTime) ||
             (m_RebalanceRequests && (m_RequestCounter >= m_RebalanceRequests)) )  {
            m_RequestCounter = 0;
            m_LastRebalanceTime = curr;
            return true;
        }
        return false;
    }
    virtual void OnResourceRequested( const CNetServerConnectionPool& ) {
        CFastMutexGuard g(m_Mutex);
        ++m_RequestCounter;
    }
    virtual void Reset() {
        CFastMutexGuard g(m_Mutex);
        m_RequestCounter = 0;
        m_LastRebalanceTime = 0;
    }

private:
    int     m_RebalanceRequests;
    int     m_RebalanceTime;
    int     m_RequestCounter;
    time_t  m_LastRebalanceTime;
    CFastMutex m_Mutex;
};


IRebalanceStrategy*
    CreateSimpleRebalanceStrategy(CConfig& conf, const string& driver_name)
{
    return new CSimpleRebalanceStrategy(
        conf.GetInt(driver_name, "rebalance_requests",
            CConfig::eErr_NoThrow, 100),
        conf.GetInt(driver_name, "rebalance_time",
            CConfig::eErr_NoThrow, 10));
}

IRebalanceStrategy* CreateDefaultRebalanceStrategy()
{
    return new CSimpleRebalanceStrategy(50, 10);
}


END_NCBI_SCOPE
