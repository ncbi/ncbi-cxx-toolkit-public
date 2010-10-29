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

#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE

void SNetServerGroupImpl::DeleteThis()
{
    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server group object yet (between
    // the time the reference counter went to zero, and the current moment
    // when m_Service is about to be reset).
    CFastMutexGuard g(m_Service->m_ServerGroupMutex);

    if (!Referenced())
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

    m_Impl.Reset(NULL);
    return false;
}

CNetServerGroupIterator CNetServerGroup::Iterate()
{
    TNetServerList::const_iterator it = m_Impl->m_Servers.begin();

    return it != m_Impl->m_Servers.end() ?
        new SNetServerGroupIteratorImpl(m_Impl, it) : NULL;
}

SNetServiceImpl::SNetServiceImpl(
        const string& api_name,
        const string& service_name,
        const string& client_name,
        INetServerConnectionListener* listener) :
    m_APIName(api_name),
    m_ServiceName(service_name),
    m_ClientName(client_name),
    m_Listener(listener),
    m_LBSMAffinityName(kEmptyStr),
    m_LBSMAffinityValue(NULL),
    m_ServiceType(eNotDefined)
{
}

void SNetServiceImpl::Init(CObject* api_impl,
    CConfig* config, const string& config_section,
    const char* const* default_config_sections)
{
    const char* const* default_section = default_config_sections;
    string section = !config_section.empty() ?
        config_section : *default_section++;

    auto_ptr<CConfig> app_reg_config;
    auto_ptr<CConfig::TParamTree> param_tree;

    if (config == NULL) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        CNcbiRegistry* reg;
        if (app != NULL && (reg = &app->GetConfig()) != NULL) {
            param_tree.reset(CConfig::ConvertRegToTree(*reg));

            for (;;) {
                const CConfig::TParamTree* section_param_tree =
                    param_tree->FindSubNode(section);

                if (section_param_tree != NULL)
                    app_reg_config.reset(new CConfig(section_param_tree));
                else if (*default_section == NULL)
                    app_reg_config.reset(new CConfig(*reg));
                else {
                    section = *default_section++;
                    continue;
                }
                break;
            }

            config = app_reg_config.get();
        }
    } else {
        const CConfig::TParamTree* supplied_param_tree = config->GetTree();

        for (;;) {
            const CConfig::TParamTree* section_param_tree =
                supplied_param_tree->FindSubNode(section);

            if (section_param_tree != NULL) {
                app_reg_config.reset(new CConfig(section_param_tree));
                config = app_reg_config.get();
            } else if (*default_section != NULL) {
                section = *default_section++;
                continue;
            }
            break;
        }
    }

    if (config != NULL) {
        if (m_ServiceName.empty()) {
            try {
                m_ServiceName = config->GetString(section,
                    "service", CConfig::eErr_Throw, kEmptyStr);
            }
            catch (exception&) {
                m_ServiceName = config->GetString(section,
                    "service_name", CConfig::eErr_NoThrow, kEmptyStr);
            }
            if (m_ServiceName.empty()) {
                string host;
                try {
                    host = config->GetString(section,
                        "server", CConfig::eErr_Throw, kEmptyStr);
                }
                catch (exception&) {
                    m_ServiceName = config->GetString(section,
                        "host", CConfig::eErr_NoThrow, kEmptyStr);
                }
                string port = config->GetString(section,
                    "port", CConfig::eErr_NoThrow, kEmptyStr);
                if (!host.empty() && !port.empty()) {
                    m_ServiceName = host + ":";
                    m_ServiceName += port;
                }
            }
        }

        if (m_ClientName.empty()) {
            try {
                m_ClientName = config->GetString(section,
                    "client_name", CConfig::eErr_Throw, kEmptyStr);
            }
            catch (exception&) {
                m_ClientName = config->GetString(section,
                    "client", CConfig::eErr_NoThrow, kEmptyStr);
            }
        }

        if (m_LBSMAffinityName.empty())
            m_LBSMAffinityName = config->GetString(section,
                "use_lbsm_affinity", CConfig::eErr_NoThrow, kEmptyStr);

        unsigned long timeout = s_SecondsToMilliseconds(config->GetString(
            section, "communication_timeout", CConfig::eErr_NoThrow, "0"), 0);

        if (timeout > 0)
            NcbiMsToTimeout(&m_Timeout, timeout);
        else
            m_Timeout = s_GetDefaultCommTimeout();

        m_ServerThrottlePeriod = config->GetInt(section,
            "throttle_relaxation_period", CConfig::eErr_NoThrow,
                THROTTLE_RELAXATION_PERIOD_DEFAULT);

        if (m_ServerThrottlePeriod > 0) {
            string numerator_str, denominator_str;

            NStr::SplitInTwo(config->GetString(section,
                "throttle_by_connection_error_rate", CConfig::eErr_NoThrow,
                    THROTTLE_BY_ERROR_RATE_DEFAULT), "/",
                        numerator_str, denominator_str);

            int numerator = NStr::StringToInt(numerator_str,
                NStr::fConvErr_NoThrow |
                    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);
            int denominator = NStr::StringToInt(denominator_str,
                NStr::fConvErr_NoThrow |
                    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);

            if (denominator < 1)
                denominator = 1;
            else if (denominator > CONNECTION_ERROR_HISTORY_MAX) {
                numerator = (numerator * CONNECTION_ERROR_HISTORY_MAX) /
                    denominator;
                denominator = CONNECTION_ERROR_HISTORY_MAX;
            }

            if (numerator < 0)
                numerator = 0;

            m_ReconnectionFailureThresholdNumerator = numerator;
            m_ReconnectionFailureThresholdDenominator = denominator;

            m_MaxSubsequentConnectionFailures = config->GetInt(section,
                "throttle_by_subsequent_connection_failures",
                    CConfig::eErr_NoThrow,
                        THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT);

            m_MaxQueryTime = s_SecondsToMilliseconds(config->GetString(section,
                "max_connection_time", CConfig::eErr_NoThrow,
                    NCBI_AS_STRING(MAX_CONNECTION_TIME_DEFAULT)),
                    SECONDS_DOUBLE_TO_MS_UL(MAX_CONNECTION_TIME_DEFAULT));

            m_ThrottleUntilDiscoverable = config->GetBool(section,
                "throttle_hold_until_active_in_lb", CConfig::eErr_NoThrow,
                    THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT);

            m_ForceRebalanceAfterThrottleWithin = config->GetInt(section,
                "throttle_forced_rebalance", CConfig::eErr_NoThrow,
                    THROTTLE_FORCED_REBALANCE_DEFAULT);
        }

        m_RebalanceStrategy = CreateSimpleRebalanceStrategy(*config, section);
    } else {
        m_Timeout = s_GetDefaultCommTimeout();

        // Throttling parameters.
        m_ServerThrottlePeriod = THROTTLE_RELAXATION_PERIOD_DEFAULT;
        m_ReconnectionFailureThresholdNumerator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_NUMERATOR;
        m_ReconnectionFailureThresholdDenominator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_DENOMINATOR;
        m_MaxSubsequentConnectionFailures =
            THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT;
        m_MaxQueryTime = SECONDS_DOUBLE_TO_MS_UL(MAX_CONNECTION_TIME_DEFAULT);
        m_ThrottleUntilDiscoverable = THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT;
        m_ForceRebalanceAfterThrottleWithin = THROTTLE_FORCED_REBALANCE_DEFAULT;

        m_RebalanceStrategy = CreateDefaultRebalanceStrategy();
    }

    m_LatestDiscoveryIteration = 0;
    m_PermanentConnection = eOn;

    NStr::TruncateSpacesInPlace(m_ServiceName);

    if (m_ServiceName.empty()) {
        m_ServiceType = eNotDefined;
    } else {
        string sport, host;

        if (NStr::SplitInTwo(m_ServiceName, ":", host, sport)) {
            m_ServiceType = eSingleServer;
            unsigned int port = NStr::StringToInt(sport);
            host = g_NetService_gethostip(host);
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

    if (m_ClientName.empty() || m_ClientName == "noname" ||
            NStr::FindNoCase(m_ClientName, "sample") != NPOS ||
            NStr::FindNoCase(m_ClientName, "unknown") != NPOS) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app == NULL) {
            NCBI_THROW_FMT(CArgException, eNoValue,
                m_APIName << ": client name is not set");
        }
        m_ClientName = app->GetProgramDisplayName();
    }

    // Get affinity value from the local LBSM configuration file.
    if (!m_LBSMAffinityName.empty())
        m_LBSMAffinityValue = LBSMD_GetHostParameter(SERV_LOCALHOST,
            m_LBSMAffinityName.c_str());

    m_Listener->OnInit(api_impl, config, section);
}

string SNetServiceImpl::MakeAuthString()
{
    string auth = m_ClientName;

    auth += " svc=\"";
    auth += m_ServiceName;
    auth += '\"';

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app != NULL) {
        auth += " app=\"";
        auth += app->GetProgramExecutablePath();
        auth += '\"';
    }

    return auth;
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

void CNetService::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style)
{
    for (CNetServerGroupIterator it = DiscoverServers().Iterate(); it; ++it) {
        if (output_style != eDumpNoHeaders)
            output_stream << (*it)->m_Address.AsString() << endl;

        CNetServer::SExecResult exec_result((*it).ExecWithRetry(cmd));

        if (output_style == eSingleLineOutput)
            output_stream << exec_result.response;
        else {
            CNetServerMultilineCmdOutput output(exec_result);

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

CNetServer SNetServiceImpl::GetSingleServer(const string& cmd)
{
    _ASSERT(m_ServiceType != eLoadBalanced);

    if (m_ServiceType == eNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": command '" << cmd <<
                "' requires a server but none specified");
    }

    return ReturnServer(m_SignleServerGroup->m_Servers.front());
}

CNetServer::SExecResult CNetService::FindServerAndExec(const string& cmd)
{
    if (m_Impl->m_ServiceType != SNetServiceImpl::eLoadBalanced)
        return m_Impl->GetSingleServer(cmd).ExecWithRetry(cmd);

    bool throttled = false;

    for (CNetServerGroupIterator it = DiscoverServers().Iterate(); it; ++it) {
        try {
            return (*it).ExecWithRetry(cmd);
        }
        catch (CNetSrvConnException& ex) {
            switch (ex.GetErrCode()) {
            case CNetSrvConnException::eConnectionFailure:
                ERR_POST_X(2, ex.what());
                break;

            case CNetSrvConnException::eServerThrottle:
                throttled = true;
                break;

            default:
                throw;
            }
        }
    }

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any available servers for the " + GetServiceName() +
        (!throttled ? " service." : " service (some servers are throttled)."));
}

CNetServer SNetServiceImpl::RequireStandAloneServerSpec(const string& cmd)
{
    if (m_ServiceType != eLoadBalanced)
        return GetSingleServer(cmd);

    NCBI_THROW_FMT(CNetServiceException, eCommandIsNotAllowed,
        m_APIName << ": command '" << cmd <<
            "' requires explicit server address (host:port)");
}

#define LBSMD_IS_PENALIZED_RATE(rate) (rate <= -0.01)

SNetServerGroupImpl* SNetServiceImpl::CreateServerGroup(
    CNetService::EDiscoveryMode discovery_mode)
{
    SNetServerGroupImpl** server_group = m_ServerGroups + discovery_mode;

    if (*server_group != NULL && !(*server_group)->Referenced())
        delete *server_group;

    return *server_group = new SNetServerGroupImpl(m_LatestDiscoveryIteration);
}

SNetServerGroupImpl* SNetServiceImpl::DiscoverServers(
    CNetService::EDiscoveryMode discovery_mode)
{
    switch (m_ServiceType) {
    case SNetServiceImpl::eNotDefined:
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": service name is not set");

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
                    "Load balancer cannot find service name '" +
                        m_ServiceName + "'");
            }
            ERR_POST_X(4, "Could not find LB service name '" <<
                m_ServiceName << "', will retry after delay");
            SleepMilliSec(s_GetRetryDelay());
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

                SNetServerImpl* server = FindOrCreateServerImpl(
                    CSocketAPI::ntoa(sinfo->host), sinfo->port);
                server->m_DiscoveryIteration = m_LatestDiscoveryIteration;
                base_group->m_Servers.push_back(server);
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
    CNetServer::SExecResult exec_result(
        RequireStandAloneServerSpec(cmd).ExecWithRetry(cmd));

    out << exec_result.response << "\n" << flush;

    STimeout rto = {1, 0};

    CSocket* the_socket = &exec_result.conn->m_Socket;

    the_socket->SetTimeout(eIO_Read, &rto);

    string line;

    for (;;)
        if (the_socket->ReadLine(line) == eIO_Success)
            out << line << "\n" << flush;
        else
            if (the_socket->GetStatus(eIO_Open) != eIO_Success)
                break;

    exec_result.conn->Close();
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

    if (had_comm_err) {
        NCBI_THROW(CNetServiceException,
            eCommunicationError, "Communication error (see details above)");
    }

    return false;
}

END_NCBI_SCOPE
