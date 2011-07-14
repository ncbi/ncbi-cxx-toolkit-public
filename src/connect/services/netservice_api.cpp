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
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>

#include <util/random_gen.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

#define LBSMD_PENALIZED_RATE_BOUNDARY -0.01

BEGIN_NCBI_SCOPE

void SDiscoveredServers::DeleteThis()
{
    SNetServiceImpl* service_impl = m_Service;

    if (service_impl == NULL)
        return;

    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server group object yet (between
    // the time the reference counter went to zero, and the current moment
    // when m_Service is about to be reset).
    CFastMutexGuard discovery_mutex_lock(service_impl->m_DiscoveryMutex);

    if (!Referenced() && m_Service) {
        if (m_ActualService == NULL ||
                m_ActualService->m_DiscoveredServers != this) {
            m_NextGroupInPool = m_Service->m_ServerGroupPool;
            m_Service->m_ServerGroupPool = this;
        }
        m_Service = NULL;
    }
}

CNetServer CNetServiceIterator::GetServer()
{
    return m_Impl->m_ServerGroup->
        m_Service->ReturnServer(m_Impl->m_Position->first);
}

bool CNetServiceIterator::Next()
{
    if (m_Impl->Next())
        return true;

    m_Impl.Reset(NULL);
    return false;
}

bool SNetServiceIteratorImpl::Next()
{
    return ++m_Position != m_ServerGroup->m_Servers.end();
}

bool SNetServiceIterator_OmitPenalized::Next()
{
    return ++m_Position != m_ServerGroup->m_SuppressedBegin;
}

static CRandom s_RandomIteratorGen((CRandom::TValue) time(NULL));

SNetServiceIterator_RandomPivot::SNetServiceIterator_RandomPivot(
        SDiscoveredServers* server_group_impl) :
    SNetServiceIteratorImpl(server_group_impl,
        server_group_impl->m_Servers.begin() + s_RandomIteratorGen.GetRand(0,
            CRandom::TValue((server_group_impl->m_SuppressedBegin -
                server_group_impl->m_Servers.begin()) - 1)))
{
}

bool SNetServiceIterator_RandomPivot::Next()
{
    if (m_RandomIterators.empty()) {
        TNetServerList::const_iterator it = m_ServerGroup->m_Servers.begin();
        size_t more_servers = (m_ServerGroup->m_SuppressedBegin - it) - 1;
        if (more_servers == 0)
            return false;
        m_RandomIterators.reserve(more_servers);
        do {
            if (it != m_Position) {
                m_RandomIterators.push_back(it);
                --more_servers;
            }
            ++it;
        } while (more_servers > 0);
        // Shuffle m_RandomIterators.
        if (m_RandomIterators.size() > 1) {
            NON_CONST_ITERATE(TRandomIterators, it, m_RandomIterators) {
                swap(*it, m_RandomIterators[s_RandomIteratorGen.GetRand(0,
                    CRandom::TValue(m_RandomIterators.size() - 1))]);
            }
        }
        m_RandomIterator = m_RandomIterators.begin();
    } else
        if (++m_RandomIterator == m_RandomIterators.end())
            return false;

    m_Position = *m_RandomIterator;

    return true;
}

SNetServiceImpl::SNetServiceImpl(
        const string& api_name,
        const string& client_name,
        INetServerConnectionListener* listener) :
    m_APIName(api_name),
    m_ClientName(client_name),
    m_Listener(listener),
    m_LBSMAffinityName(kEmptyStr),
    m_LBSMAffinityValue(NULL),
    m_ServerGroupPool(NULL),
    m_UseOldStyleAuth(false)
{
}

void SNetServiceImpl::Init(CObject* api_impl, const string& service_name,
    CConfig* config, const string& config_section,
    const char* const* default_config_sections)
{
    // Initialize the connect library and LBSM structures
    // used in DiscoverServersIfNeeded().
    {
        class CInPlaceConnIniter : public CConnIniter
        {
        } conn_initer;
    }

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

    string actual_service_name = service_name;

    if (config != NULL) {
        if (actual_service_name.empty()) {
            try {
                actual_service_name = config->GetString(section,
                    "service", CConfig::eErr_Throw, kEmptyStr);
            }
            catch (exception&) {
                actual_service_name = config->GetString(section,
                    "service_name", CConfig::eErr_NoThrow, kEmptyStr);
            }
            if (actual_service_name.empty()) {
                string host;
                try {
                    host = config->GetString(section,
                        "server", CConfig::eErr_Throw, kEmptyStr);
                }
                catch (exception&) {
                    actual_service_name = config->GetString(section,
                        "host", CConfig::eErr_NoThrow, kEmptyStr);
                }
                string port = config->GetString(section,
                    "port", CConfig::eErr_NoThrow, kEmptyStr);
                if (!host.empty() && !port.empty()) {
                    actual_service_name = host + ":";
                    actual_service_name += port;
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
            section, "connection_timeout", CConfig::eErr_NoThrow, "0"), 0);

        NcbiMsToTimeout(&m_ConnTimeout, timeout > 0 ? timeout :
            SECONDS_DOUBLE_TO_MS_UL(CONNECTION_TIMEOUT_DEFAULT));

        timeout = s_SecondsToMilliseconds(config->GetString(
            section, "communication_timeout", CConfig::eErr_NoThrow, "0"), 0);

        if (timeout > 0)
            NcbiMsToTimeout(&m_CommTimeout, timeout);
        else
            m_CommTimeout = s_GetDefaultCommTimeout();

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

            m_ThrottleUntilDiscoverable = config->GetBool(section,
                "throttle_hold_until_active_in_lb", CConfig::eErr_NoThrow,
                    THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT);

            m_ForceRebalanceAfterThrottleWithin = config->GetInt(section,
                "throttle_forced_rebalance", CConfig::eErr_NoThrow,
                    THROTTLE_FORCED_REBALANCE_DEFAULT);
        }

        m_MaxConnectionTime = s_SecondsToMilliseconds(config->GetString(section,
            "max_connection_time", CConfig::eErr_NoThrow,
                NCBI_AS_STRING(MAX_CONNECTION_TIME_DEFAULT)),
                SECONDS_DOUBLE_TO_MS_UL(MAX_CONNECTION_TIME_DEFAULT));

        m_RebalanceStrategy = CreateSimpleRebalanceStrategy(*config, section);
    } else {
        NcbiMsToTimeout(&m_ConnTimeout,
            SECONDS_DOUBLE_TO_MS_UL(CONNECTION_TIMEOUT_DEFAULT));
        m_CommTimeout = s_GetDefaultCommTimeout();

        // Throttling parameters.
        m_ServerThrottlePeriod = THROTTLE_RELAXATION_PERIOD_DEFAULT;
        m_ReconnectionFailureThresholdNumerator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_NUMERATOR;
        m_ReconnectionFailureThresholdDenominator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_DENOMINATOR;
        m_MaxSubsequentConnectionFailures =
            THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT;
        m_ThrottleUntilDiscoverable = THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT;
        m_ForceRebalanceAfterThrottleWithin = THROTTLE_FORCED_REBALANCE_DEFAULT;

        m_MaxConnectionTime =
            SECONDS_DOUBLE_TO_MS_UL(MAX_CONNECTION_TIME_DEFAULT);

        m_RebalanceStrategy = CreateDefaultRebalanceStrategy();
    }

    m_PermanentConnection = eOn;

    m_MainService = FindOrCreateActualService(actual_service_name);

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

SDiscoveredServers* SNetServiceImpl::AllocServerGroup(
    unsigned discovery_iteration)
{
    if (m_ServerGroupPool == NULL)
        return new SDiscoveredServers(discovery_iteration);
    else {
        SDiscoveredServers* server_group = m_ServerGroupPool;
        m_ServerGroupPool = server_group->m_NextGroupInPool;

        server_group->Reset(discovery_iteration);

        return server_group;
    }
}

SActualService* SNetServiceImpl::FindOrCreateActualService(
    const string& service_name)
{
    string actual_service_name(service_name);

    NStr::TruncateSpacesInPlace(actual_service_name);

    SActualService search_image(actual_service_name);

    TActualServiceSet::iterator it = m_ActualServices.find(&search_image);

    if (it != m_ActualServices.end())
        return *it;

    SActualService* actual_service = new SActualService(service_name);
    m_ActualServices.insert(actual_service);

    if (!actual_service_name.empty()) {
        string host, port_str;

        if (NStr::SplitInTwo(actual_service_name, ":", host, port_str)) {
            host = g_NetService_gethostip(host);
            unsigned short port = NStr::StringToInt(port_str);
            actual_service->m_ServiceType = eSingleServerService;
            actual_service->m_DiscoveredServers = AllocServerGroup(0);
            CFastMutexGuard server_mutex_lock(m_ServerMutex);
            actual_service->m_DiscoveredServers->m_Servers.push_back(
                TServerRate(FindOrCreateServerImpl(host, port), 1));
            actual_service->m_DiscoveredServers->m_SuppressedBegin =
                actual_service->m_DiscoveredServers->m_Servers.end();
        } else
            actual_service->m_ServiceType = eLoadBalancedService;
    }

    return actual_service;
}

string SNetServiceImpl::MakeAuthString()
{
    string auth = m_ClientName;

    if (!m_UseOldStyleAuth) {
        if (m_MainService->m_ServiceType == eLoadBalancedService) {
            auth += " svc=\"";
            auth += m_MainService->m_ServiceName;
            auth += '\"';
        }

        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL) {
            auth += " client_path=\"";
            auth += app->GetProgramExecutablePath();
            auth += '\"';
        }
    }

    return auth;
}

const string& CNetService::GetClientName() const
{
    return m_Impl->m_ClientName;
}

const string& CNetService::GetServiceName() const
{
    return m_Impl->m_MainService->m_ServiceName;
}

bool CNetService::IsLoadBalanced() const
{
    return m_Impl->m_MainService->m_ServiceType ==
        eLoadBalancedService;
}

void CNetService::StickToServer(const string& host, unsigned port)
{
    m_Impl->m_EnforcedServerHost = g_NetService_gethostip(host);
    m_Impl->m_EnforcedServerPort = port;
}

void CNetService::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style)
{
    bool print_headers = IsLoadBalanced() ?
        output_style != eMultilineOutput_NoHeaders : false;

    for (CNetServiceIterator it = Iterate(); it; ++it) {
        if (print_headers)
            output_stream << '[' << (*it)->m_Address.AsString() << ']' << endl;

        CNetServer::SExecResult exec_result((*it).ExecWithRetry(cmd));

        if (output_style == eSingleLineOutput)
            output_stream << exec_result.response << endl;
        else {
            CNetServerMultilineCmdOutput output(exec_result);

            if (output_style == eMultilineOutput_NetCacheStyle)
                output->SetNetCacheCompatMode();

            string line;

            while (output.ReadLine(line))
                output_stream << line << endl;

            if (print_headers)
                output_stream << endl;
        }
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

    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    server_impl->m_Service = this;
    return server_impl;
}

CNetServer SNetServiceImpl::GetServer(const string& host, unsigned int port)
{
    m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    SNetServerImpl* server = m_EnforcedServerHost.empty() ?
        FindOrCreateServerImpl(host, port) :
            FindOrCreateServerImpl(m_EnforcedServerHost, m_EnforcedServerPort);

    server->m_Service = this;
    return server;
}

CNetServer SNetServiceImpl::GetSingleServer(const string& cmd)
{
    _ASSERT(m_MainService->m_ServiceType != eLoadBalancedService);

    if (m_MainService->m_ServiceType == eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": command '" << cmd <<
                "' requires a server but none specified");
    }

    return ReturnServer(m_MainService->m_DiscoveredServers->
        m_Servers.front().first);
}

class SRandomIterationBeginner : public IIterationBeginner
{
public:
    SRandomIterationBeginner(CNetService& service) :
        m_Service(service)
    {
    }

    virtual CNetServiceIterator BeginIteration();

    CNetService& m_Service;
};

CNetServiceIterator SRandomIterationBeginner::BeginIteration()
{
    return m_Service.Iterate(CNetService::eRandomize);
}

CNetServer::SExecResult CNetService::FindServerAndExec(const string& cmd)
{
    if (m_Impl->m_MainService->m_ServiceType != eLoadBalancedService)
        return m_Impl->GetSingleServer(cmd).ExecWithRetry(cmd);

    CNetServer::SExecResult exec_result;

    SRandomIterationBeginner iteration_beginner(*this);

    m_Impl->IterateAndExec(cmd, exec_result, &iteration_beginner);

    return exec_result;
}

CNetServer SNetServiceImpl::RequireStandAloneServerSpec(const string& cmd)
{
    if (m_MainService->m_ServiceType != eLoadBalancedService)
        return GetSingleServer(cmd);

    NCBI_THROW_FMT(CNetServiceException, eCommandIsNotAllowed,
        m_APIName << ": command '" << cmd <<
            "' requires explicit server address (host:port)");
}

void SNetServiceImpl::DiscoverServersIfNeeded(SActualService* actual_service)
{
    if (actual_service->m_ServiceType == eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": service name is not set");
    }

    if (actual_service->m_ServiceType == eLoadBalancedService) {
        // The service is load-balanced, check if rebalancing is required.
        m_RebalanceStrategy->OnResourceRequested();
        if (m_RebalanceStrategy->NeedRebalance())
            ++actual_service->m_LatestDiscoveryIteration;

        if (actual_service->m_DiscoveredServers == NULL ||
            actual_service->m_DiscoveredServers->m_DiscoveryIteration !=
                actual_service->m_LatestDiscoveryIteration) {
            // The requested server group either does not exist or
            // does not contain up-to-date server list, thus it needs
            // to be created anew.

            // Query the Load Balancer.
            SERV_ITER srv_it;

            // FIXME Retry logic can be removed as soon as LBSMD with
            // packet compression is installed universally.
            int try_count = TServConn_MaxFineLBNameRetries::GetDefault();
            for (;;) {
                SConnNetInfo* net_info =
                    ConnNetInfo_Create(actual_service->m_ServiceName.c_str());

                srv_it = SERV_OpenP(actual_service->m_ServiceName.c_str(),
                    fSERV_Standalone | fSERV_IncludeSuppressed,
                    SERV_LOCALHOST, 0, 0.0, net_info, NULL, 0, 0 /*false*/,
                    m_LBSMAffinityName.c_str(), m_LBSMAffinityValue);

                ConnNetInfo_Destroy(net_info);

                if (srv_it != 0 || --try_count < 0)
                    break;

                ERR_POST_X(4, "Could not find LB service name '" <<
                    actual_service->m_ServiceName <<
                        "', will retry after delay");
                SleepMilliSec(s_GetRetryDelay());
            }

            SDiscoveredServers* server_group =
                actual_service->m_DiscoveredServers;

            if (server_group != NULL && !server_group->m_Service)
                server_group->Reset(actual_service->m_LatestDiscoveryIteration);
            else
                // Either the group does not exist or it has been
                // "issued" to the outside callers; allocate a new one.
                server_group = actual_service->m_DiscoveredServers =
                    AllocServerGroup(
                        actual_service->m_LatestDiscoveryIteration);

            CFastMutexGuard server_mutex_lock(m_ServerMutex);

            const SSERV_Info* sinfo;

            TNetServerList& servers = server_group->m_Servers;
            TNetServerList::size_type number_of_regular_servers = 0;
            TNetServerList::size_type number_of_standby_servers = 0;
            double max_standby_rate = LBSMD_PENALIZED_RATE_BOUNDARY;

            // Fill the 'servers' array in accordance with the layout
            // described above the SDiscoveredServers::m_Servers declaration.
            while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0)
                if (sinfo->time > 0 && sinfo->time != NCBI_TIME_INFINITE &&
                        sinfo->rate != 0.0) {
                    SNetServerImpl* server = FindOrCreateServerImpl(
                        CSocketAPI::ntoa(sinfo->host), sinfo->port);
                    server->m_ActualServiceDiscoveryIteration[actual_service] =
                        actual_service->m_LatestDiscoveryIteration;

                    TServerRate server_rate(server, sinfo->rate);

                    if (sinfo->rate > 0)
                        servers.insert(servers.begin() +
                            number_of_regular_servers++, server_rate);
                    else if (sinfo->rate < max_standby_rate ||
                            sinfo->rate <= LBSMD_PENALIZED_RATE_BOUNDARY)
                        servers.push_back(server_rate);
                    else {
                        servers.insert(servers.begin() +
                            number_of_regular_servers, server_rate);
                        if (sinfo->rate == max_standby_rate)
                            ++number_of_standby_servers;
                        else {
                            max_standby_rate = sinfo->rate;
                            number_of_standby_servers = 1;
                        }
                    }
                }

            server_group->m_SuppressedBegin = servers.begin() +
                (number_of_regular_servers > 0 ?
                    number_of_regular_servers : number_of_standby_servers);

            server_mutex_lock.Release();

            SERV_Close(srv_it);
        }
    }
}

void SNetServiceImpl::GetDiscoveredServers(const string* service_name,
    CRef<SDiscoveredServers>& discovered_servers)
{
    CFastMutexGuard discovery_mutex_lock(m_DiscoveryMutex);
    SActualService* actual_service =
        service_name == NULL || service_name->empty() ?
            m_MainService : FindOrCreateActualService(*service_name);
    DiscoverServersIfNeeded(actual_service);
    discovered_servers = actual_service->m_DiscoveredServers;
    discovered_servers->m_Service = this;
    discovered_servers->m_ActualService = actual_service;
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

static void SleepUntil(const CTime& time)
{
    CTime current_time(GetFastLocalTime());
    if (time > current_time) {
        CTimeSpan diff(time - current_time);
        SleepMicroSec(diff.GetCompleteSeconds() * 1000 * 1000 +
            diff.GetNanoSecondsAfterSecond() / 1000);
    }
}

void SNetServiceImpl::IterateAndExec(const string& cmd,
    CNetServer::SExecResult& exec_result,
    IIterationBeginner* iteration_beginner)
{
    CNetServiceIterator it = iteration_beginner->BeginIteration();

    CTime max_connection_time(GetFastLocalTime());
    CTime retry_delay_until(max_connection_time);

    unsigned retry_count;

    try {
        (*it)->ConnectAndExec(cmd, exec_result);
        return;
    }
    catch (CNetSrvConnException& ex) {
        switch (ex.GetErrCode()) {
        case CNetSrvConnException::eConnectionFailure:
        case CNetSrvConnException::eServerThrottle:
            retry_count = TServConn_ConnMaxRetries::GetDefault();
            if ((++it || retry_count > 0) &&
                    (m_MaxConnectionTime == 0 || GetFastLocalTime() <
                        max_connection_time.AddNanoSecond(
                            m_MaxConnectionTime * 1000 * 1000)))
                break;
            /* else: FALL THROUGH */

        default:
            throw;
        }
    }

    // At this point, 'it' points to the next server (or NULL);
    // variable 'retry_count' is set from the respective configuration
    // parameter; 'max_connection_time' is set correctly unless
    // m_MaxConnectionTime is zero.

    unsigned long retry_delay = s_GetRetryDelay() * 1000 * 1000;
    retry_delay_until.AddNanoSecond(retry_delay);

    string last_error;
    bool throttled;

    for (;;) {
        throttled = false;
        while (it) {
            try {
                (*it)->ConnectAndExec(cmd, exec_result);
                return;
            }
            catch (CNetSrvConnException& ex) {
                switch (ex.GetErrCode()) {
                case CNetSrvConnException::eConnectionFailure:
                    last_error = ex.what();
                    break;

                case CNetSrvConnException::eServerThrottle:
                    throttled = true;
                    if (last_error.empty())
                        last_error = ex.what();
                    break;

                default:
                    throw;
                }
            }
            ++it;
        }
        if (retry_count == 0)
            break;
        --retry_count;

        if (m_MaxConnectionTime > 0 &&
                GetFastLocalTime() >= max_connection_time) {
            LOG_POST(Error <<
                "Timeout (max_connection_time=" << m_MaxConnectionTime <<
                "); cmd=" << cmd <<
                "; exception=" << last_error);
            break;
        }

        SleepUntil(retry_delay_until);
        retry_delay_until = GetFastLocalTime();
        retry_delay_until.AddNanoSecond(retry_delay);
        it = iteration_beginner->BeginIteration();

        LOG_POST("Unable to execute '" << cmd <<
            (!throttled ?
                "' on any of the discovered servers; last error seen: " :
                "' on any of the discovered servers (some servers are "
                    "throttled); last error seen: ") << last_error <<
            "; remaining attempts: " << retry_count);
    }

    NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
        "Unable to execute '" << cmd <<
        (!throttled ?
            "' on any of the discovered servers; last error seen: " :
            "' on any of the discovered servers (some servers are "
                "throttled); last error seen: ") << last_error);
}

SNetServiceImpl::~SNetServiceImpl()
{
    // Clean up m_Servers
    NON_CONST_ITERATE(TNetServerSet, it, m_Servers) {
        delete *it;
    }

    // Clean up m_ActualServices
    NON_CONST_ITERATE(TActualServiceSet, it, m_ActualServices) {
        // Delete fake server "groups" in single-server "services".
        // See SNetServiceImpl::FindOrCreateActualService().
        if ((*it)->m_ServiceType == eSingleServerService)
            delete (*it)->m_DiscoveredServers;
        delete *it;
    }

    // Clean up m_ServerGroupPool
    SDiscoveredServers* server_group = m_ServerGroupPool;
    while (server_group != NULL) {
        SDiscoveredServers* next_group = server_group->m_NextGroupInPool;
        delete server_group;
        server_group = next_group;
    }

    if (m_LBSMAffinityValue != NULL)
        free((void*) m_LBSMAffinityValue);
}

void CNetService::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_CommTimeout = to;
}
const STimeout& CNetService::GetCommunicationTimeout() const
{
    return m_Impl->m_CommTimeout;
}

void CNetService::SetPermanentConnection(ESwitch type)
{
    m_Impl->m_PermanentConnection = type;
}

CNetServiceIterator CNetService::Iterate(CNetService::EIterationMode mode,
    const string* service_name)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(service_name, servers);

    if (mode != eIncludePenalized) {
        if (servers->m_Servers.begin() < servers->m_SuppressedBegin) {
            if (mode == eSortByLoad)
                return new SNetServiceIterator_OmitPenalized(servers);
            else
                return new SNetServiceIterator_RandomPivot(servers);
        }
    } else
        if (!servers->m_Servers.empty())
            return new SNetServiceIteratorImpl(servers);

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any available servers for the " +
        (service_name == NULL || service_name->empty() ?
        m_Impl->m_MainService->m_ServiceName : *service_name) + " service.");
}

CNetServiceIterator CNetService::Iterate(CNetServer::TInstance priority_server,
    const string* service_name)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(service_name, servers);

    // Find the requested server among the discovered servers.
    for (TNetServerList::const_iterator it = servers->m_Servers.begin();
            it != servers->m_SuppressedBegin; ++it)
        if (it->first == priority_server)
            return new SNetServiceIterator_RandomPivot(servers, it);

    if (servers->m_Servers.begin() < servers->m_SuppressedBegin)
        // The requested server is not found in this service,
        // however there are non-penalized servers, so return them.
        return new SNetServiceIterator_RandomPivot(servers);
    else {
        // The service is empty, allocate a server group to contain
        // solely the requested server.
        servers.Reset();
        CFastMutexGuard discovery_mutex_lock(m_Impl->m_DiscoveryMutex);
        (servers = m_Impl->AllocServerGroup(0))->m_Servers.push_back(
            TServerRate(priority_server, 1));
        servers->m_Service = m_Impl;
        servers->m_ActualService = NULL;
        servers->m_SuppressedBegin = servers->m_Servers.end();
        return new SNetServiceIterator_RandomPivot(servers,
            servers->m_Servers.begin());
    }

    return NULL;
}

bool CNetService::FindServer(INetServerFinder* finder,
    CNetService::EIterationMode mode)
{
    bool had_comm_err = false;
    string error_message;

    for (CNetServiceIterator it = Iterate(mode); it; ++it) {
        CNetServer server = *it;

        try {
            if (finder->Consider(server))
                return true;
        }
        catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;

            had_comm_err = true;
            error_message = server->m_Address.AsString();
            error_message += ": ";
            error_message += ex.what();
        }
        catch (CIO_Exception& ex) {
            had_comm_err = true;
            error_message = server->m_Address.AsString();
            error_message += ": ";
            error_message += ex.what();
        }
    }

    if (had_comm_err) {
        NCBI_THROW(CNetServiceException, eCommunicationError, error_message);
    }

    return false;
}

END_NCBI_SCOPE
