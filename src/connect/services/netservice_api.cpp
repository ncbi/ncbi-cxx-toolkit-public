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

#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"

#include "netservice_api_impl.hpp"
#include "netservice_params.hpp"

#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE

void SNetServerGroupImpl::Delete()
{
    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server group object yet (between
    // the time the reference counter went to zero, and the current moment
    // when m_Service is about to be reset).
    CFastMutexGuard g(m_Service->m_ServerGroupMutex);

    if (GetRefCount() == 0)
        m_Service = NULL;
}

CNetServer CNetServerGroupIterator::GetServer()
{
    return m_Impl->m_ServerGroup->m_Service->ReturnServer(*m_Impl->m_Position);
}

bool CNetServerGroupIterator::Next()
{
    if (++m_Impl->m_Position != m_Impl->m_ServerGroup->m_Servers.end())
        return true;

        m_Impl.Assign(NULL);
        return false;
    }

CNetServerGroupIterator CNetServerGroup::Iterate()
{
    TNetServerList::const_iterator it = m_Impl->m_Servers.begin();

    return it != m_Impl->m_Servers.end() ?
        new SNetServerGroupIteratorImpl(m_Impl, it) : NULL;
}

SNetServiceImpl::SNetServiceImpl(
    const string& service_name,
    const string& client_name,
    const string& lbsm_affinity_name) :
        m_ServiceName(service_name),
        m_LBSMAffinityName(lbsm_affinity_name),
        m_LatestDiscoveryIteration(0),
        m_Timeout(s_GetDefaultCommTimeout()),
        m_PermanentConnection(eOn)
{
    m_RebalanceStrategy = CreateDefaultRebalanceStrategy();

    if (service_name.empty()) {
        m_ServiceType = eNotDefined;
    } else {
        string sport, host;

        if (NStr::SplitInTwo(service_name, ":", host, sport)) {
            m_ServiceType = eSingleServer;
            unsigned int port = NStr::StringToInt(sport);
            host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
            // No need to lock in the constructor:
            // CFastMutexGuard g(m_ServerGroupMutex);
            SNetServerImpl* single_server = new SNetServerImplReal(host, port);
            m_Servers.insert(single_server);
            (m_SignleServerGroup = new SNetServerGroupImpl(0))->
                m_Servers.push_back(single_server);
        } else {
            m_ServiceType = eLoadBalanced;
            memset(&m_ServerGroups, 0, sizeof(m_ServerGroups));
        }
    }

    m_ClientName = !client_name.empty() &&
        NStr::FindNoCase(client_name, "sample") == NPOS &&
        NStr::FindNoCase(client_name, "unknown") == NPOS ?
        client_name : CNcbiApplication::Instance()->GetProgramDisplayName();

    // Get affinity value from the local LBSM configuration file.
    m_LBSMAffinityValue = lbsm_affinity_name.empty() ? NULL :
        LBSMD_GetHostParameter(SERV_LOCALHOST, m_LBSMAffinityName.c_str());
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
    return m_Impl->m_ServiceType == SNetServiceImpl::eLoadBalanced;
}

void CNetService::SetRebalanceStrategy(IRebalanceStrategy* strategy)
{
    m_Impl->m_RebalanceStrategy = strategy != NULL ? strategy :
        CreateDefaultRebalanceStrategy().GetPtr();
}


void CNetService::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style)
{
    for (CNetServerGroupIterator it = DiscoverServers().Iterate(); it; ++it) {
        CNetServerConnection conn = (*it).Connect();

        if (output_style != eDumpNoHeaders)
            output_stream << (*it)->m_Address.AsString() << endl;

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

SNetServerImpl* SNetServiceImpl::FindOrCreateServerImpl(
    const string& host, unsigned short port)
{
    SNetServerImpl search_image(host, port);

    TNetServerSet::iterator it = m_Servers.find(&search_image);

    if (it != m_Servers.end())
        return *it;

    SNetServerImpl* server = new SNetServerImplReal(host, port);

    m_Servers.insert(server);

    return server;
}

CNetServer SNetServiceImpl::ReturnServer(SNetServerImpl* server_impl)
{
    m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard g(m_ServerMutex);

    server_impl->m_Service = this;
    return server_impl;
}

CNetServer SNetServiceImpl::GetServer(const string& host, unsigned int port)
{
    m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard g(m_ServerMutex);

    SNetServerImpl* server = FindOrCreateServerImpl(host, port);
    server->m_Service = this;
    return server;
}

CNetServerConnection SNetServiceImpl::GetSingleServerConnection()
{
    _ASSERT(m_ServiceType != eLoadBalanced);

    if (m_ServiceType == eNotDefined)
        NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
            "The service is not set.");

    return ReturnServer(m_SignleServerGroup->m_Servers.front()).Connect();
}

CNetServerConnection CNetService::GetBestConnection()
{
    _ASSERT(m_Impl->m_ServiceType != SNetServiceImpl::eNotDefined);

    if (m_Impl->m_ServiceType != SNetServiceImpl::eLoadBalanced)
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
            GetServiceName() + " service.");
}

CNetServerConnection SNetServiceImpl::RequireStandAloneServerSpec()
{
    if (m_ServiceType != eLoadBalanced)
        return GetSingleServerConnection();

    NCBI_THROW(CNetServiceException, eCommandIsNotAllowed,
        "This command requires explicit server address (host:port)");
}

#define LBSMD_IS_PENALIZED_RATE(rate) (rate <= -0.01)

SNetServerGroupImpl* SNetServiceImpl::CreateServerGroup(
    CNetService::EDiscoveryMode discovery_mode)
{
    SNetServerGroupImpl** server_group = m_ServerGroups + discovery_mode;

    if (*server_group != NULL && (*server_group)->GetRefCount() == 0)
        delete *server_group;

    return *server_group = new SNetServerGroupImpl(m_LatestDiscoveryIteration);
}

SNetServerGroupImpl* SNetServiceImpl::DiscoverServers(
    CNetService::EDiscoveryMode discovery_mode)
{
    switch (m_ServiceType) {
    case SNetServiceImpl::eNotDefined:
        NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
            "Service name is not set.");

    case SNetServiceImpl::eSingleServer:
        m_SignleServerGroup->m_Service = this;
        return m_SignleServerGroup;

    case SNetServiceImpl::eLoadBalanced:
        break;
    }

    // The service is load-balanced, check if rebalancing is required.
    m_RebalanceStrategy->OnResourceRequested();
    if (m_RebalanceStrategy->NeedRebalance())
        ++m_LatestDiscoveryIteration;

    _ASSERT(discovery_mode >= 0 &&
        discovery_mode < CNetService::eNumberOfDiscoveryModes);

    SNetServerGroupImpl* result = m_ServerGroups[discovery_mode];

    if (result != NULL &&
        result->m_DiscoveryIteration == m_LatestDiscoveryIteration) {
        result->m_Service = this;
        return result;
    }

    // The requested server group either does not exist or
    // does not contain up-to-date server list, thus it needs
    // to be created anew.
    // Check if the "base" server list is up-to-date.
    SNetServerGroupImpl* base_group = m_ServerGroups[CNetService::eSortByLoad];

    bool base_group_requested = discovery_mode == CNetService::eSortByLoad ||
        discovery_mode == CNetService::eIncludePenalized;

    if (base_group_requested || base_group == NULL ||
        base_group->m_DiscoveryIteration != m_LatestDiscoveryIteration) {

        SERV_ITER srv_it;

        int try_count = TServConn_MaxFineLBNameRetries::GetDefault();
        for (;;) {
            SConnNetInfo* net_info = ConnNetInfo_Create(m_ServiceName.c_str());

            srv_it = SERV_OpenP(m_ServiceName.c_str(),
                discovery_mode == CNetService::eIncludePenalized ?
                    fSERV_Standalone | fSERV_IncludeSuppressed :
                    fSERV_Standalone,
                SERV_LOCALHOST, 0, 0.0, net_info, NULL, 0, 0 /*false*/,
                m_LBSMAffinityName.c_str(), m_LBSMAffinityValue);

            ConnNetInfo_Destroy(net_info);

            if (srv_it != 0)
                break;

            if (--try_count < 0) {
                NCBI_THROW(CNetSrvConnException, eLBNameNotFound,
                    "Load balancer cannot find service name " +
                        m_ServiceName + ".");
            }
            ERR_POST_X(4, "Could not find LB service name '" <<
                m_ServiceName << "', will retry after delay");
            SleepMilliSec(TServConn_RetryDelay::GetDefault());
        }

        base_group = CreateServerGroup(
            discovery_mode == CNetService::eIncludePenalized ?
                CNetService::eIncludePenalized : CNetService::eSortByLoad);

        CFastMutexGuard g(m_ServerMutex);

        const SSERV_Info* sinfo;

        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {
            if (sinfo->time > 0 && sinfo->time != NCBI_TIME_INFINITE &&
                (sinfo->rate > 0.0 ||
                    (discovery_mode == CNetService::eIncludePenalized &&
                        LBSMD_IS_PENALIZED_RATE(sinfo->rate)))) {

                base_group->m_Servers.push_back(FindOrCreateServerImpl(
                    CSocketAPI::ntoa(sinfo->host), sinfo->port));
            }
        }

        g.Release();

        SERV_Close(srv_it);

        if (base_group_requested) {
            base_group->m_Service = this;
            return base_group;
        }
    }

    result = CreateServerGroup(discovery_mode);

    if (discovery_mode == CNetService::eRandomize) {
        size_t servers_size = base_group->m_Servers.size();

        if (servers_size > 0) {
            // Pick a random pivot element, so we do not always
            // fetch jobs using the same lookup order.
            for (unsigned current = rand() % servers_size,
                    last = current + servers_size; current < last; ++current)
                result->m_Servers.push_back(
                    base_group->m_Servers[current % servers_size]);
        }
    } else { // discovery_mode is CNetService::eSortByAddress
        result->m_Servers.insert(result->m_Servers.begin(),
            base_group->m_Servers.begin(), base_group->m_Servers.end());
        sort(result->m_Servers.begin(), result->m_Servers.end());
    }

    result->m_Service = this;
    return result;
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
    // Clean up m_Servers
    NON_CONST_ITERATE(TNetServerSet, it, m_Servers) {
        delete *it;
    }

    switch (m_ServiceType) {
    case eLoadBalanced:
        {{
            // Clean up m_ServerGroups
            int i = (int) CNetService::eNumberOfDiscoveryModes;
            SNetServerGroupImpl** server_group = m_ServerGroups;
            while (--i >= 0) {
                if (*server_group != NULL)
                    delete *server_group;
                ++server_group;
            }
        }}
        break;

    case eSingleServer:
        delete m_SignleServerGroup;
        break;

    case eNotDefined:
        break;
    }

    if (m_LBSMAffinityValue != NULL)
        free((void*) m_LBSMAffinityValue);
}

void CNetService::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_Timeout = to;
}
const STimeout& CNetService::GetCommunicationTimeout() const
{
    return m_Impl->m_Timeout;
}

void CNetService::SetPermanentConnection(ESwitch type)
{
    m_Impl->m_PermanentConnection = type;
}

CNetServerGroup CNetService::DiscoverServers(
    CNetService::EDiscoveryMode discovery_mode)
{
    CFastMutexGuard g(m_Impl->m_ServerGroupMutex);

    return m_Impl->DiscoverServers(discovery_mode);
}

bool CNetServerGroup::FindServer(INetServerFinder* finder)
{
    bool had_comm_err = false;

    for (CNetServerGroupIterator it = Iterate(); it; ++it) {
        CNetServer server = *it;

        try {
            if (finder->Consider(server))
                return true;
        }
        catch (CNetServiceException& ex) {
            ERR_POST_X(5, server->m_Address.AsString() <<
                " returned error: \"" << ex.what() << "\"");

            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;

            had_comm_err = true;
        }
        catch (CIO_Exception& ex) {
            ERR_POST_X(6, server->m_Address.AsString() <<
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
