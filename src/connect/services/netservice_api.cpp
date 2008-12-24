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
 * Author:  Maxim Didenko
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include "netservice_api_impl.hpp"

#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <connect/services/netservice_params.hpp>

#include <corelib/ncbi_system.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE


std::string CNetServer::GetHost() const
{
    return m_Impl->m_Address.first;
}

unsigned short CNetServer::GetPort() const
{
    return m_Impl->m_Address.second;
}

CNetServerConnection CNetServer::Connect()
{
    return m_Impl->m_Service.GetSpecificConnection(GetHost(), GetPort());
}

int CNetServerGroup::GetCount() const
{
    return (int) m_Impl->m_Servers.size();
}

CNetServer CNetServerGroup::GetServer(int index)
{
    _ASSERT(index < (int) m_Impl->m_Servers.size());

    return new SNetServerImpl(m_Impl->m_Servers[index], m_Impl->m_Service);
}

SNetServiceImpl::SNetServiceImpl(
    const std::string& service_name,
    const std::string& client_name) :
    m_ServiceName(service_name),
    m_IsLoadBalanced(false),
    m_DiscoverLowPriorityServers(eOff),
    m_Timeout(s_GetDefaultCommTimeout()),
    m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()),
    m_PermanentConnection(eOn)
{
    m_RebalanceStrategy = CreateDefaultRebalanceStrategy();

    string sport, host;

    if (NStr::SplitInTwo(service_name, ":", host, sport)) {
       unsigned int port = NStr::StringToInt(sport);
       host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
       CWriteLockGuard g(m_ServersLock);
       m_Servers.push_back(TServerAddress(host, port));
    } else {
       m_IsLoadBalanced = true;
    }

    m_ClientName = !client_name.empty() &&
        NStr::FindNoCase(client_name, "sample") == NPOS &&
        NStr::FindNoCase(client_name, "unknown") == NPOS ?
        client_name : CNcbiApplication::Instance()->GetProgramDisplayName();
}

const string& CNetService::GetClientName() const
{
    return m_Impl->m_ClientName;
}

const string& CNetService::GetServiceName() const
{
    return m_Impl->m_ServiceName;
}

bool CNetService::IsLoadBalanced() const
{
    return m_Impl->m_IsLoadBalanced;
}

void CNetService::DiscoverLowPriorityServers(ESwitch on_off)
{
    if (m_Impl->m_DiscoverLowPriorityServers != on_off) {
        m_Impl->m_DiscoverLowPriorityServers = on_off;

        if (m_Impl->m_RebalanceStrategy)
            m_Impl->m_RebalanceStrategy->Reset();
    }
}

void CNetService::SetRebalanceStrategy(IRebalanceStrategy* strategy)
{
    m_Impl->m_RebalanceStrategy = strategy;
}


void SNetServiceImpl::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, ECmdOutputType output_type)
{
    TDiscoveredServers servers;

    DiscoverServers(servers);

    ITERATE(TDiscoveredServers, it, servers) {
        CNetServerConnection conn = GetConnection(*it);

        output_stream << conn.GetHost() << ":" << conn.GetPort() << endl;

        if (output_type == eSingleLineOutput)
            output_stream << conn.Exec(cmd);
        else {
            CNetServerCmdOutput output = conn.ExecMultiline(cmd);

            if (output_type == eMultilineOutput_NetCacheStyle)
                output->SetNetCacheCompatMode();

            std::string line;

            while (output.ReadLine(line))
                output_stream << line << "\n";
        }

        output_stream << endl;
    }
}

CNetServerConnection SNetServiceImpl::GetBestConnection()
{
    if (!m_IsLoadBalanced) {
        CReadLockGuard g(m_ServersLock);
        if (m_Servers.empty())
            NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "The service is not set.");
        return GetConnection(m_Servers[0]);
    }

    TDiscoveredServers servers;

    DiscoverServers(servers);

    ITERATE(TDiscoveredServers, it, servers) {
        CNetServerConnection conn = GetConnection(*it);
        try {
            conn->CheckConnect();
            return conn;
        } catch (CNetSrvConnException& ex) {
            if (ex.GetErrCode() == CNetSrvConnException::eConnectionFailure) {
                ERR_POST_X(2, ex.what());
            }
            else
                throw;
        }
    }

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any availbale servers for " +
        m_ServiceName + " service.");
}

CNetServerConnection CNetService::GetBestConnection()
{
    return m_Impl->GetBestConnection();
}

CNetServerConnection CNetService::GetSpecificConnection(
    const string& host, unsigned int port)
{
    return m_Impl->GetConnection(TServerAddress(
        CSocketAPI::ntoa(CSocketAPI::gethostbyname(host)), port));
}

void CNetService::SetCommunicationTimeout(const STimeout& to)
{
    CFastMutexGuard g(m_Impl->m_ConnectionMutex);
    m_Impl->m_Timeout = to;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_Impl->m_ServerAddressToConnectionPool) {
        it->second.SetCommunicationTimeout(to);
    }
}
const STimeout& CNetService::GetCommunicationTimeout() const
{
    return m_Impl->m_Timeout;
}

void CNetService::SetCreateSocketMaxRetries(unsigned int retries)
{
    CFastMutexGuard g(m_Impl->m_ConnectionMutex);
    m_Impl->m_MaxRetries = retries;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_Impl->m_ServerAddressToConnectionPool) {
        it->second.SetCreateSocketMaxRetries(retries);
    }
}

unsigned int CNetService::GetCreateSocketMaxRetries() const
{
    return m_Impl->m_MaxRetries;
}

void CNetService::SetPermanentConnection(ESwitch type)
{
    CFastMutexGuard g(m_Impl->m_ConnectionMutex);
    m_Impl->m_PermanentConnection = type;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_Impl->m_ServerAddressToConnectionPool) {
        it->second.PermanentConnection(type);
    }
}

CNetServerConnection SNetServiceImpl::GetConnection(const TServerAddress& srv)
{
    CFastMutexGuard g(m_ConnectionMutex);
    TServerAddressToConnectionPool::iterator it =
        m_ServerAddressToConnectionPool.find(srv);
    CNetServerConnectionPool pool;
    if (it != m_ServerAddressToConnectionPool.end())
        pool = it->second;
    if (!pool) {
        pool = new SNetServerConnectionPoolImpl(
            srv.first, srv.second, m_Timeout, m_Listener);
        pool.PermanentConnection(m_PermanentConnection);
        m_ServerAddressToConnectionPool[srv] = pool;
    }
    g.Release();
    if (m_RebalanceStrategy)
        m_RebalanceStrategy->OnResourceRequested();
    return pool.GetConnection();
}

void SNetServiceImpl::DiscoverServers(TDiscoveredServers& servers)
{
    if (m_IsLoadBalanced &&
        (!m_RebalanceStrategy || m_RebalanceStrategy->NeedRebalance())) {
        CWriteLockGuard g(m_ServersLock);
        m_Servers.clear();
        int try_count = 0;
        for (;;) {
            try {
                QueryLoadBalancer(m_ServiceName, m_Servers,
                    m_DiscoverLowPriorityServers == eOn);
                break;
            } catch (CNetSrvConnException& ex) {
                if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound)
                    throw;
                ERR_POST_X(4, "Communication Error : " << ex.what());
                if (++try_count > TServConn_MaxFineLBNameRetries::GetDefault())
                    throw;
                SleepMilliSec(1000 + try_count * 2000);
            }
        }
    }

    servers.clear();
    CReadLockGuard g(m_ServersLock);
    servers.insert(servers.begin(), m_Servers.begin(), m_Servers.end());
}

CNetServerGroup CNetService::DiscoverServers()
{
    CNetServerGroup server_group(new SNetServerGroupImpl(*this));

    m_Impl->DiscoverServers(server_group->m_Servers);

    return server_group;
}

END_NCBI_SCOPE
