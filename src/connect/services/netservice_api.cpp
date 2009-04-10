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
 * Author:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include "netservice_api_impl.hpp"

#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <connect/services/netservice_params.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE


CNetServer CNetServerGroupIterator::GetServer()
{
    return m_Impl->m_ServerGroup->m_Service->GetServer(
        m_Impl->m_Position->first, m_Impl->m_Position->second);
}

bool CNetServerGroupIterator::Next()
{
    if (++m_Impl->m_Position == m_Impl->m_ServerGroup->m_Servers.end()) {
        m_Impl.Assign(NULL);
        return false;
    }

    return true;
}

int CNetServerGroup::GetCount() const
{
    return (int) m_Impl->m_Servers.size();
}

CNetServer CNetServerGroup::GetServer(int index)
{
    _ASSERT(index < (int) m_Impl->m_Servers.size());

    const TServerAddress& server_address = m_Impl->m_Servers[index];

    return m_Impl->m_Service->GetServer(
        server_address.first, server_address.second);
}

CNetServerGroupIterator CNetServerGroup::Iterate()
{
    TDiscoveredServers::const_iterator it = m_Impl->m_Servers.begin();

    return it != m_Impl->m_Servers.end() ?
        new SNetServerGroupIteratorImpl(m_Impl, it) : NULL;
}

SNetServiceImpl::SNetServiceImpl(
    const string& service_name,
    const string& client_name,
    const string& lbsm_affinity_name) :
    m_ServiceDiscovery(new CNetServiceDiscovery(service_name,
        lbsm_affinity_name)),
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
       // No need to lock in the c-tor: CWriteLockGuard g(m_ServersLock);
       m_DiscoveredServers.push_back(TServerAddress(host, port));
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
    return m_Impl->m_ServiceDiscovery->GetServiceName();
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


void CNetService::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style)
{
    for (CNetServerGroupIterator it = DiscoverServers().Iterate(); it; ++it) {
        CNetServerConnection conn = (*it).Connect();

        if (output_style != eDumpNoHeaders)
            output_stream << conn.GetHost() << ":" << conn.GetPort() << endl;

        if (output_style == eSingleLineOutput)
            output_stream << conn.Exec(cmd);
        else {
            CNetServerCmdOutput output = conn.ExecMultiline(cmd);

            if (output_style == eMultilineOutput_NetCacheStyle)
                output->SetNetCacheCompatMode();

            string line;

            while (output.ReadLine(line))
                output_stream << line << endl;
        }

        if (output_style != eDumpNoHeaders)
            output_stream << endl;
    }
}

CNetServer SNetServiceImpl::GetServer(const string& host, unsigned int port)
{
    SNetServerImpl search_image(host, port);

    CFastMutexGuard g(m_ConnectionMutex);

    TNetServerSet::iterator it = m_Servers.find(&search_image);

    CNetServer server;
    if (it != m_Servers.end())
        server = *it;
    if (!server) {
        server = new SNetServerImplReal(host, port);
        m_Servers.insert(server);
    }
    server->m_Service = this;
    g.Release();
    if (m_RebalanceStrategy)
        m_RebalanceStrategy->OnResourceRequested();
    return server;
}

CNetServerConnection SNetServiceImpl::GetSingleServerConnection()
{
    _ASSERT(!m_IsLoadBalanced);

    {{
        CReadLockGuard g(m_ServersLock);
        if (m_DiscoveredServers.empty())
            NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
                "The service is not set.");
    }}

    return GetServer(m_DiscoveredServers[0].first,
        m_DiscoveredServers[0].second).Connect();
}

CNetServerConnection CNetService::GetBestConnection()
{
    if (!m_Impl->m_IsLoadBalanced)
        return m_Impl->GetSingleServerConnection();

    for (CNetServerGroupIterator it = DiscoverServers().Iterate(); it; ++it) {
        try {
            return (*it).Connect();
        } catch (CNetSrvConnException& ex) {
            if (ex.GetErrCode() == CNetSrvConnException::eConnectionFailure) {
                ERR_POST_X(2, ex.what());
            }
            else
                throw;
        }
    }

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any availbale servers for the " +
            m_Impl->m_ServiceDiscovery->GetServiceName() + " service.");
}

CNetServerConnection SNetServiceImpl::RequireStandAloneServerSpec()
{
    if (!m_IsLoadBalanced)
        return GetSingleServerConnection();

    NCBI_THROW(CNetServiceException, eCommandIsNotAllowed,
        "This command requires explicit server address (host:port)");
}

void SNetServiceImpl::Monitor(CNcbiOstream& out, const string& cmd)
{
    CNetServerConnection conn = RequireStandAloneServerSpec();

    conn->WriteLine(cmd);

    STimeout rto = {1, 0};

    CSocket* the_socket = &conn->m_Socket;

    the_socket->SetTimeout(eIO_Read, &rto);

    string line;

    for (;;)
        if (the_socket->ReadLine(line) == eIO_Success)
            out << line << "\n" << flush;
        else
            if (the_socket->GetStatus(eIO_Open) != eIO_Success)
                break;

    conn->Close();
}

SNetServiceImpl::~SNetServiceImpl()
{
    NON_CONST_ITERATE(TNetServerSet, it, m_Servers) {
        delete *it;
    }
}

void CNetService::SetCommunicationTimeout(const STimeout& to)
{
    CFastMutexGuard g(m_Impl->m_ConnectionMutex);
    m_Impl->m_Timeout = to;
}
const STimeout& CNetService::GetCommunicationTimeout() const
{
    return m_Impl->m_Timeout;
}

void CNetService::SetCreateSocketMaxRetries(unsigned int retries)
{
    m_Impl->m_MaxRetries = retries;
}

unsigned int CNetService::GetCreateSocketMaxRetries() const
{
    return m_Impl->m_MaxRetries;
}

void CNetService::SetPermanentConnection(ESwitch type)
{
    m_Impl->m_PermanentConnection = type;
}

CNetServerGroup CNetService::DiscoverServers(
    CNetService::EServerSortMode sort_mode)
{
    CNetServerGroup server_group(new SNetServerGroupImpl(*this));

    if (m_Impl->m_IsLoadBalanced && (!m_Impl->m_RebalanceStrategy ||
            m_Impl->m_RebalanceStrategy->NeedRebalance())) {
        CWriteLockGuard g(m_Impl->m_ServersLock);
        m_Impl->m_DiscoveredServers.clear();
        int try_count = 0;
        for (;;) {
            try {
                m_Impl->m_ServiceDiscovery->QueryLoadBalancer(m_Impl->m_DiscoveredServers,
                    m_Impl->m_DiscoverLowPriorityServers == eOn);
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

    server_group->m_Servers.clear();
    CReadLockGuard g(m_Impl->m_ServersLock);

    switch (sort_mode) {
    case CNetService::eSortByLoad:
        server_group->m_Servers.insert(server_group->m_Servers.begin(),
            m_Impl->m_DiscoveredServers.begin(), m_Impl->m_DiscoveredServers.end());
        break;

    case CNetService::eRandomize:
        if (!m_Impl->m_DiscoveredServers.empty()) {
            size_t servers_size = m_Impl->m_DiscoveredServers.size();

            // Pick a random pivot element, so we do not always
            // fetch jobs using the same lookup order.
            unsigned current = rand() % servers_size;
            unsigned last = current + servers_size;

            for (; current < last; ++current)
                server_group->m_Servers.push_back(
                    m_Impl->m_DiscoveredServers[current % servers_size]);
        }
        break;

    case CNetService::eSortByAddress:
        server_group->m_Servers.insert(server_group->m_Servers.begin(),
            m_Impl->m_DiscoveredServers.begin(), m_Impl->m_DiscoveredServers.end());
        sort(server_group->m_Servers.begin(), server_group->m_Servers.end());
    }

    return server_group;
}

bool CNetService::FindServer(const CNetObjectRef<INetServerFinder>& finder,
    CNetService::EServerSortMode sort_mode)
{
    CNetServerGroup servers = DiscoverServers(sort_mode);

    bool had_comm_err = false;

    for (CNetServerGroupIterator it = servers.Iterate(); it; ++it) {
        CNetServer server = *it;

        try {
            if (finder->Consider(server))
                return true;
        }
        catch (CNetServiceException& ex) {
            ERR_POST_X(5, server->m_Host << ":" << server->m_Port <<
                " returned error: \"" << ex.what() << "\"");

            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;

            had_comm_err = true;
        }
        catch (CIO_Exception& ex) {
            ERR_POST_X(6, server->m_Host << ":" << server->m_Port <<
                " returned error: \"" << ex.what() << "\"");

            had_comm_err = true;
        }
    }

    if (had_comm_err)
        NCBI_THROW(CNetServiceException,
        eCommunicationError, "Communication error");

    return false;
}

END_NCBI_SCOPE
