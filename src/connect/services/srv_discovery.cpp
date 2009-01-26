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

#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"

#include <connect/services/srv_discovery.hpp>
#include <connect/services/srv_connections_expt.hpp>

#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

CNetServiceDiscovery::~CNetServiceDiscovery()
{
    if (m_LBSMAffinityValue != NULL)
        free((void*) m_LBSMAffinityValue);
}

void CNetServiceDiscovery::Init(CConfig& conf, const string& driver_name)
{
    try {
        m_LBSMAffinityName = conf.GetString(driver_name,
            "use_lbsm_affinity", CConfig::eErr_Throw);

        // Get affinity value from the local LBSM configuration file.
        m_LBSMAffinityValue = LBSMD_GetHostParameter(SERV_LOCALHOST,
            m_LBSMAffinityName.c_str());
    }
    catch (CConfigException& e) {
        if (e.GetErrCode() != CConfigException::eParameterMissing)
            throw;
    }
}

void CNetServiceDiscovery::QueryLoadBalancer(TDiscoveredServers& servers,
    bool include_suppressed)
{
    SConnNetInfo* net_info =
        ConnNetInfo_Create(m_ServiceName.c_str());

    SERV_ITER srv_it = SERV_OpenP(m_ServiceName.c_str(),
        include_suppressed ?
            fSERV_Standalone | fSERV_IncludeSuppressed :
            fSERV_Standalone,
        SERV_LOCALHOST, 0, 0.0, net_info, NULL, 0, 0 /*false*/,
        m_LBSMAffinityName.c_str(), m_LBSMAffinityValue);

    ConnNetInfo_Destroy(net_info);

    if (srv_it == 0) {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound,
            "Load balancer cannot find service name " + m_ServiceName + ".");
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
    virtual void OnResourceRequested() {
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


CNetObjectRef<IRebalanceStrategy>
    CreateSimpleRebalanceStrategy(CConfig& conf, const string& driver_name)
{
    return new CSimpleRebalanceStrategy(
        conf.GetInt(driver_name, "rebalance_requests",
            CConfig::eErr_NoThrow, 100),
        conf.GetInt(driver_name, "rebalance_time",
            CConfig::eErr_NoThrow, 10));
}

CNetObjectRef<IRebalanceStrategy>
    CreateSimpleRebalanceStrategy(int rebalance_requests, int rebalance_time)
{
    return new CSimpleRebalanceStrategy(rebalance_requests, rebalance_time);
}

CNetObjectRef<IRebalanceStrategy> CreateDefaultRebalanceStrategy()
{
    return new CSimpleRebalanceStrategy(50, 10);
}


END_NCBI_SCOPE
