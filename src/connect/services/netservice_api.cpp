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
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netschedule_api_expt.hpp>

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
    CNetService service(m_Service);

    if (!service)
        return;

    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server group object yet (between
    // the time the reference counter went to zero, and the current moment
    // when m_Service is about to be reset).
    CFastMutexGuard discovery_mutex_lock(service->m_DiscoveryMutex);

    service = NULL;

    if (!Referenced() && m_Service) {
        if (m_Service->m_DiscoveredServers != this) {
            m_NextGroupInPool = m_Service->m_ServerGroupPool;
            m_Service->m_ServerGroupPool = this;
        }
        m_Service = NULL;
    }
}

CNetServer CNetServiceIterator::GetServer()
{
    return new SNetServerImpl(m_Impl->m_ServerGroup->m_Service,
            m_Impl->m_ServerGroup->m_Service->m_ServerPool->
            ReturnServer(m_Impl->m_Position->first));
}

bool CNetServiceIterator::Next()
{
    if (m_Impl->Next())
        return true;

    m_Impl.Reset(NULL);
    return false;
}

bool CNetServiceIterator::Prev()
{
    if (m_Impl->Prev())
        return true;

    m_Impl.Reset(NULL);
    return false;
}

double CNetServiceIterator::GetRate() const
{
    return m_Impl->m_Position->second;
}

bool SNetServiceIteratorImpl::Next()
{
    return ++m_Position != m_ServerGroup->m_Servers.end();
}

bool SNetServiceIteratorImpl::Prev()
{
    if (m_Position == m_ServerGroup->m_Servers.begin())
        return false;
    --m_Position;
    return true;
}

bool SNetServiceIterator_OmitPenalized::Next()
{
    return ++m_Position != m_ServerGroup->m_SuppressedBegin;
}

DEFINE_STATIC_FAST_MUTEX(s_RndLock);
static CRandom s_RandomIteratorGen((CRandom::TValue) time(NULL));

static CRandom::TValue
s_GetRand(CRandom::TValue max_value)
{
    CFastMutexGuard guard(s_RndLock);
    return s_RandomIteratorGen.GetRand(0, max_value);
}

SNetServiceIterator_RandomPivot::SNetServiceIterator_RandomPivot(
        SDiscoveredServers* server_group_impl) :
    SNetServiceIteratorImpl(server_group_impl,
        server_group_impl->m_Servers.begin() + s_GetRand(
            CRandom::TValue((server_group_impl->m_SuppressedBegin -
                server_group_impl->m_Servers.begin()) - 1)))
{
}

bool SNetServiceIterator_RandomPivot::Next()
{
    if (m_RandomIterators.empty()) {
        TNetServerList::const_iterator it = m_ServerGroup->m_Servers.begin();
        size_t number_of_servers = m_ServerGroup->m_SuppressedBegin - it;
        if (number_of_servers <= 1)
            return false; // There are no servers to advance to.
        m_RandomIterators.reserve(number_of_servers);
        m_RandomIterators.push_back(m_Position);
        --number_of_servers;
        do {
            if (it != m_Position) {
                m_RandomIterators.push_back(it);
                --number_of_servers;
            }
            ++it;
        } while (number_of_servers > 0);
        // Shuffle m_RandomIterators starting from the element with index '1'.
        if (m_RandomIterators.size() > 2) {
            TRandomIterators::iterator rnt_it = m_RandomIterators.begin();
            while (++rnt_it != m_RandomIterators.end())
                swap(*rnt_it, m_RandomIterators[s_RandomIteratorGen.GetRand(1,
                    CRandom::TValue(m_RandomIterators.size() - 1))]);
        }
        m_RandomIterator = m_RandomIterators.begin();
        ++m_RandomIterator;
    } else
        if (++m_RandomIterator == m_RandomIterators.end())
            return false;

    m_Position = *m_RandomIterator;

    return true;
}

bool SNetServiceIterator_Circular::Next()
{
    if (++m_Position == m_ServerGroup->m_Servers.end())
        m_Position = m_ServerGroup->m_Servers.begin();
    return m_Position != m_Pivot;
}

bool SNetServiceIterator_Circular::Prev()
{
    if (m_Position == m_Pivot)
        return false;
    if (m_Position == m_ServerGroup->m_Servers.begin())
        m_Position = m_ServerGroup->m_Servers.end();
    --m_Position;
    return true;
}

bool SNetServiceIterator_RandomPivot::Prev()
{
    if (m_RandomIterators.empty() ||
            m_RandomIterator == m_RandomIterators.begin())
        return false;

    m_Position = *--m_RandomIterator;

    return true;
}

SNetServerPoolImpl::SNetServerPoolImpl(const string& api_name,
        const string& client_name,
        INetServerConnectionListener* listener) :
    m_APIName(api_name),
    m_ClientName(client_name),
    m_Listener(listener),
    m_EnforcedServerHost(0),
    m_LBSMAffinityName(kEmptyStr),
    m_LBSMAffinityValue(NULL),
    m_UseOldStyleAuth(false)
{
}

void SNetServiceImpl::ZeroInit()
{
    m_ServiceType = CNetService::eServiceNotDefined;
    m_DiscoveredServers = NULL;
    m_ServerGroupPool = NULL;
    m_LatestDiscoveryIteration = 0;
}

void SNetServiceImpl::Construct(SNetServerInPool* server)
{
    m_ServiceType = CNetService::eSingleServerService;
    m_DiscoveredServers = AllocServerGroup(0);
    CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);
    m_DiscoveredServers->m_Servers.push_back(TServerRate(server, 1));
    m_DiscoveredServers->m_SuppressedBegin =
        m_DiscoveredServers->m_Servers.end();
}

void SNetServiceImpl::Construct()
{
    if (!m_ServiceName.empty()) {
        string host, port;

        if (!NStr::SplitInTwo(m_ServiceName, ":", host, port))
            m_ServiceType = CNetService::eLoadBalancedService;
        else
            Construct(m_ServerPool->FindOrCreateServerImpl(
                    g_NetService_gethostbyname(host),
                    (unsigned short) NStr::StringToInt(port)));
    }
}

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
void CNetService::AllowXSiteConnections()
{
    m_Impl->AllowXSiteConnections();
}

bool CNetService::IsUsingXSiteProxy()
{
    return m_Impl->m_AllowXSiteConnections;
}

void SNetServiceImpl::AllowXSiteConnections()
{
    m_AllowXSiteConnections = true;

    SConnNetInfo* net_info = ConnNetInfo_Create(SNetServerImpl::kXSiteFwd);

    SSERV_Info* sinfo = SERV_GetInfo(SNetServerImpl::kXSiteFwd,
            fSERV_Standalone, SERV_LOCALHOST, net_info);

    ConnNetInfo_Destroy(net_info);

    if (sinfo == NULL) {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound,
            "Cannot find cross-site proxy");
    }

    m_ColoNetwork = SOCK_NetToHostLong(sinfo->host) >> 16;

    free(sinfo);
}
#endif

void SNetServiceImpl::Init(CObject* api_impl, const string& service_name,
    CConfig* config, const string& config_section,
    const char* const* default_config_sections)
{
    // Initialize the connect library and LBSM structures
    // used in DiscoverServersIfNeeded().
    {
        class CInPlaceConnIniter : protected CConnIniter
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
                else if (*default_section != NULL) {
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

    m_ServiceName = service_name;
    NStr::TruncateSpacesInPlace(m_ServiceName);

    if (m_ServiceName.empty() && config == NULL &&
            (config = m_Listener->LoadConfigFromAltSource(api_impl,
                    &section)) != NULL)
        app_reg_config.reset(config);

    if (config != NULL) {
        if (m_ServiceName.empty()) {
            m_ServiceName = config->GetString(section, "service",
                    CConfig::eErr_NoThrow, kEmptyStr);
            if (m_ServiceName.empty())
                m_ServiceName = config->GetString(section, "service_name",
                        CConfig::eErr_NoThrow, kEmptyStr);
            if (m_ServiceName.empty()) {
                string host(config->GetString(section, "server",
                        CConfig::eErr_NoThrow, kEmptyStr));
                if (host.empty())
                    host = config->GetString(section, "host",
                            CConfig::eErr_NoThrow, kEmptyStr);
                string port = config->GetString(section,
                    "port", CConfig::eErr_NoThrow, kEmptyStr);
                if (!host.empty() && !port.empty()) {
                    m_ServiceName = host + ":";
                    m_ServiceName += port;
                }
            }
        }

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (config->GetBool(section, "allow_xsite_conn",
                CConfig::eErr_NoThrow, false))
            AllowXSiteConnections();
#endif

        m_UseSmartRetries = config->GetBool(section,
                "smart_service_retries", CConfig::eErr_NoThrow, true);
    }

    m_ServerPool->Init(config, section, m_Listener.GetPointerOrNull());

    m_Listener->OnInit(api_impl, config, section);

    Construct();
}

void SNetServerPoolImpl::Init(CConfig* config, const string& section,
        INetServerConnectionListener* listener)
{
    if (config != NULL) {
        if (m_ClientName.empty()) {
            m_ClientName = config->GetString(section, "client_name",
                    CConfig::eErr_NoThrow, kEmptyStr);
            if (m_ClientName.empty())
                m_ClientName = config->GetString(section, "client",
                        CConfig::eErr_NoThrow, kEmptyStr);
        }

        if (m_LBSMAffinityName.empty())
            m_LBSMAffinityName = config->GetString(section,
                "use_lbsm_affinity", CConfig::eErr_NoThrow, kEmptyStr);

        unsigned long conn_timeout = s_SecondsToMilliseconds(config->GetString(
            section, "connection_timeout", CConfig::eErr_NoThrow, "0"), 0);

        NcbiMsToTimeout(&m_ConnTimeout, conn_timeout > 0 ? conn_timeout :
            SECONDS_DOUBLE_TO_MS_UL(CONNECTION_TIMEOUT_DEFAULT));

        unsigned long comm_timeout = s_SecondsToMilliseconds(config->GetString(
            section, "communication_timeout", CConfig::eErr_NoThrow, "0"), 0);

        if (comm_timeout > 0)
            NcbiMsToTimeout(&m_CommTimeout, comm_timeout);
        else
            m_CommTimeout = s_GetDefaultCommTimeout();

        unsigned long first_srv_timeout = s_SecondsToMilliseconds(
                config->GetString(section, "first_server_timeout",
                CConfig::eErr_NoThrow, "0"), 0);

        if (first_srv_timeout > 0)
            NcbiMsToTimeout(&m_FirstServerTimeout, first_srv_timeout);
        else if (comm_timeout > 0)
            m_FirstServerTimeout = m_CommTimeout;
        else
            NcbiMsToTimeout(&m_FirstServerTimeout,
                    SECONDS_DOUBLE_TO_MS_UL(FIRST_SERVER_TIMEOUT_DEFAULT));

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

            m_IOFailureThresholdNumerator = numerator;
            m_IOFailureThresholdDenominator = denominator;

            try {
                m_MaxConsecutiveIOFailures = config->GetInt(section,
                        "throttle_by_consecutive_connection_failures",
                        CConfig::eErr_NoThrow, 0);
            }
            catch (exception&) {
                m_MaxConsecutiveIOFailures = config->GetInt(section,
                        "throttle_by_subsequent_connection_failures",
                        CConfig::eErr_NoThrow,
                        THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT);
            }

            m_ThrottleUntilDiscoverable = config->GetBool(section,
                "throttle_hold_until_active_in_lb", CConfig::eErr_NoThrow,
                    THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT);
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

        NcbiMsToTimeout(&m_FirstServerTimeout,
            SECONDS_DOUBLE_TO_MS_UL(FIRST_SERVER_TIMEOUT_DEFAULT));

        // Throttling parameters.
        m_ServerThrottlePeriod = THROTTLE_RELAXATION_PERIOD_DEFAULT;
        m_IOFailureThresholdNumerator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_NUMERATOR;
        m_IOFailureThresholdDenominator =
            THROTTLE_BY_ERROR_RATE_DEFAULT_DENOMINATOR;
        m_MaxConsecutiveIOFailures =
            THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT;
        m_ThrottleUntilDiscoverable = THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT;

        m_MaxConnectionTime =
            SECONDS_DOUBLE_TO_MS_UL(MAX_CONNECTION_TIME_DEFAULT);

        m_RebalanceStrategy = CreateDefaultRebalanceStrategy();
    }

    m_PermanentConnection = eOn;

    if (m_ClientName.empty() || m_ClientName == "noname" ||
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

    m_Listener = listener;
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

string SNetServiceImpl::MakeAuthString()
{
    string auth;
    auth.reserve(256);

    auth += "client=\"";
    auth += NStr::PrintableString(m_ServerPool->m_ClientName);
    auth += '\"';

    if (!m_ServerPool->m_UseOldStyleAuth) {
        if (m_ServiceType == CNetService::eLoadBalancedService) {
            auth += " svc=\"";
            auth += NStr::PrintableString(m_ServiceName);
            auth += '\"';
        }

        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL) {
            auth += " client_path=\"";
            auth += NStr::PrintableString(app->GetProgramExecutablePath());
            auth += '\"';
        }
    }

    return auth;
}

const string& CNetServerPool::GetClientName() const
{
    return m_Impl->m_ClientName;
}

const string& CNetService::GetServiceName() const
{
    return m_Impl->m_ServiceName;
}

CNetServerPool CNetService::GetServerPool()
{
    return m_Impl->m_ServerPool;
}

CNetService::EServiceType CNetService::GetServiceType() const
{
    return m_Impl->m_ServiceType;
}

bool CNetService::IsLoadBalanced() const
{
    return m_Impl->m_ServiceType == CNetService::eLoadBalancedService;
}

void CNetServerPool::StickToServer(const string& host, unsigned short port)
{
    m_Impl->m_EnforcedServerHost = g_NetService_gethostbyname(host);
    m_Impl->m_EnforcedServerPort = port;
}

void CNetService::PrintCmdOutput(const string& cmd,
        CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style,
        CNetService::EIterationMode iteration_mode)
{
    bool load_balanced = IsLoadBalanced() ?
        output_style != eMultilineOutput_NoHeaders : false;

    for (CNetServiceIterator it = Iterate(iteration_mode); it; ++it) {
        if (load_balanced)
            output_stream << '[' << (*it).GetServerAddress() << ']' << endl;

        CNetServer::SExecResult exec_result((*it).ExecWithRetry(cmd));

        switch (output_style) {
        case eSingleLineOutput:
            output_stream << exec_result.response << endl;
            break;

        case eUrlEncodedOutput:
            {
                CUrlArgs url_parser(exec_result.response);

                ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
                    output_stream << field->name <<
                            ": " << field->value << endl;
                }
            }
            break;

        default:
            {
                CNetServerMultilineCmdOutput output(exec_result);

                if (output_style == eMultilineOutput_NetCacheStyle)
                    output->SetNetCacheCompatMode();

                string line;

                while (output.ReadLine(line))
                    output_stream << line << endl;
            }
        }

        if (load_balanced)
            output_stream << endl;
    }
}

SNetServerInPool* SNetServerPoolImpl::FindOrCreateServerImpl(
    unsigned host, unsigned short port)
{
    SServerAddress server_address(host, port);

    pair<TNetServerByAddress::iterator, bool> loc(m_Servers.insert(
            TNetServerByAddress::value_type(server_address,
                    (SNetServerInPool*) NULL)));

    if (!loc.second)
        return loc.first->second;

    SNetServerInPool* server = new SNetServerInPool(host, port,
            m_Listener->AllocServerProperties().GetPointerOrNull());

    loc.first->second = server;

    return server;
}

CRef<SNetServerInPool> SNetServerPoolImpl::ReturnServer(
        SNetServerInPool* server_impl)
{
    m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    server_impl->m_ServerPool = this;
    return CRef<SNetServerInPool>(server_impl);
}

CNetServer CNetService::GetServer(unsigned host, unsigned short port)
{
    m_Impl->m_ServerPool->m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_Impl->m_ServerPool->m_ServerMutex);

    SNetServerInPool* server = m_Impl->m_ServerPool->m_EnforcedServerHost == 0 ?
            m_Impl->m_ServerPool->FindOrCreateServerImpl(host, port) :
            m_Impl->m_ServerPool->FindOrCreateServerImpl(
                    m_Impl->m_ServerPool->m_EnforcedServerHost,
                    m_Impl->m_ServerPool->m_EnforcedServerPort);

    server->m_ServerPool = m_Impl->m_ServerPool;

    return new SNetServerImpl(m_Impl, server);
}

class SRandomServiceTraversal : public IServiceTraversal
{
public:
    SRandomServiceTraversal(CNetService::TInstance service) :
        m_Service(service)
    {
    }

    virtual CNetServer BeginIteration();
    virtual CNetServer NextServer();

private:
    CNetService m_Service;
    CNetServiceIterator m_Iterator;
};

CNetServer SRandomServiceTraversal::BeginIteration()
{
    return *(m_Iterator = m_Service.Iterate(CNetService::eRandomize));
}

CNetServer SRandomServiceTraversal::NextServer()
{
    return ++m_Iterator ? *m_Iterator : CNetServer();
}

CNetServer::SExecResult CNetService::FindServerAndExec(const string& cmd)
{
    switch (m_Impl->m_ServiceType) {
    default: // CNetService::eServiceNotDefined
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                m_Impl->m_ServerPool->m_APIName << ": command '" << cmd <<
                        "' requires a server but none specified");

    case CNetService::eLoadBalancedService:
        {
            CNetServer::SExecResult exec_result;

            SRandomServiceTraversal random_traversal(*this);

            m_Impl->IterateUntilExecOK(cmd, exec_result, &random_traversal,
                    SNetServiceImpl::eIgnoreServerErrors);

            return exec_result;
        }

    case CNetService::eSingleServerService:
        {
            CNetServer server(new SNetServerImpl(m_Impl,
                    m_Impl->m_ServerPool->ReturnServer(
                    m_Impl->m_DiscoveredServers->m_Servers.front().first)));

            return server.ExecWithRetry(cmd);
        }
    }
}

void CNetService::ExecOnAllServers(const string& cmd)
{
    for (CNetServiceIterator it = Iterate(eIncludePenalized); it; ++it)
        (*it).ExecWithRetry(cmd);
}

void SNetServiceImpl::DiscoverServersIfNeeded()
{
    if (m_ServiceType == CNetService::eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_ServerPool->m_APIName << ": service name is not set");
    }

    if (m_ServiceType == CNetService::eLoadBalancedService) {
        // The service is load-balanced, check if rebalancing is required.
        m_ServerPool->m_RebalanceStrategy->OnResourceRequested();
        if (m_ServerPool->m_RebalanceStrategy->NeedRebalance())
            ++m_LatestDiscoveryIteration;

        if (m_DiscoveredServers == NULL ||
            m_DiscoveredServers->m_DiscoveryIteration !=
                m_LatestDiscoveryIteration) {
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
                    ConnNetInfo_Create(m_ServiceName.c_str());

                srv_it = SERV_OpenP(m_ServiceName.c_str(),
                    fSERV_Standalone | fSERV_IncludeSuppressed,
                    SERV_LOCALHOST, 0, 0.0, net_info, NULL, 0, 0 /*false*/,
                    m_ServerPool->m_LBSMAffinityName.c_str(),
                    m_ServerPool->m_LBSMAffinityValue);

                ConnNetInfo_Destroy(net_info);

                if (srv_it != 0 || --try_count < 0)
                    break;

                ERR_POST_X(4, "Could not find LB service name '" <<
                    m_ServiceName <<
                        "', will retry after delay");
                SleepMilliSec(s_GetRetryDelay());
            }

            SDiscoveredServers* server_group = m_DiscoveredServers;

            if (server_group != NULL && !server_group->m_Service)
                server_group->Reset(m_LatestDiscoveryIteration);
            else
                // Either the group does not exist or it has been
                // "issued" to the outside callers; allocate a new one.
                server_group = m_DiscoveredServers =
                    AllocServerGroup(m_LatestDiscoveryIteration);

            CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);

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
                    SNetServerInPool* server = m_ServerPool->
                            FindOrCreateServerImpl(sinfo->host, sinfo->port);
                    {{
                        CFastMutexGuard guard(server->m_ThrottleLock);
                        server->m_DiscoveredAfterThrottling = true;
                    }}

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

void SNetServiceImpl::GetDiscoveredServers(
    CRef<SDiscoveredServers>& discovered_servers)
{
    CFastMutexGuard discovery_mutex_lock(m_DiscoveryMutex);
    DiscoverServersIfNeeded();
    discovered_servers = m_DiscoveredServers;
    discovered_servers->m_Service = this;
}

bool SNetServiceImpl::IsInService(CNetServer::TInstance server)
{
    CRef<SDiscoveredServers> servers;
    GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == server->m_ServerInPool)
            return true;
    }

    return false;
}

void SNetServiceImpl::IterateUntilExecOK(const string& cmd,
    CNetServer::SExecResult& exec_result,
    IServiceTraversal* service_traversal,
    SNetServiceImpl::EServerErrorHandling error_handling)
{
    int retry_count = (int) TServConn_ConnMaxRetries::GetDefault();

    const unsigned long retry_delay = s_GetRetryDelay();

    CDeadline max_connection_time
        (m_ServerPool->m_MaxConnectionTime / 1000,
         (m_ServerPool->m_MaxConnectionTime % 1000) * 1000 * 1000);

    CNetServer server = service_traversal->BeginIteration();

    unsigned number_of_servers = 0;
    unsigned ns_with_submits_disabled = 0;
    unsigned servers_throttled = 0;

    STimeout* timeout = retry_count <= 0 && !m_UseSmartRetries ?
            NULL : &m_ServerPool->m_FirstServerTimeout;

    for (;;) {
        try {
            server->ConnectAndExec(cmd, exec_result, timeout);
            return;
        }
        catch (CNetCacheException& ex) {
            if (error_handling == eRethrowServerErrors ||
                    (retry_count <= 0 && !m_UseSmartRetries))
                throw;
            LOG_POST(Warning << server.GetServerAddress() << ": " << ex);
        }
        catch (CNetScheduleException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            if (ex.GetErrCode() == CNetScheduleException::eSubmitsDisabled)
                ++ns_with_submits_disabled;
            else if (error_handling == eRethrowServerErrors)
                throw;
            else {
                LOG_POST(Warning << server.GetServerAddress() << ": " << ex);
            }
        }
        catch (CNetSrvConnException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            switch (ex.GetErrCode()) {
            case CNetSrvConnException::eReadTimeout:
                break;
            case CNetSrvConnException::eConnectionFailure:
                LOG_POST(Warning << ex);
                break;

            case CNetSrvConnException::eServerThrottle:
                ++servers_throttled;
                break;

            default:
                throw;
            }
        }

        ++number_of_servers;
        server = service_traversal->NextServer();

        if (m_ServerPool->m_MaxConnectionTime > 0 &&
                max_connection_time.GetRemainingTime().GetAsMilliSeconds() <=
                        (server ? 0 : retry_delay)) {
            NCBI_THROW_FMT(CNetSrvConnException, eReadTimeout,
                    "Exceeded max_connection_time=" <<
                    m_ServerPool->m_MaxConnectionTime <<
                    "; cmd=[" << cmd << "]");
        }

        if (!server) {
            if (number_of_servers == ns_with_submits_disabled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all NetSchedule servers are "
                        "in REFUSESUBMITS mode.");
            }
            if (number_of_servers == servers_throttled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all servers are throttled.");
            }
            if (retry_count <= 0) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Unable to execute [" << cmd <<
                        "] on any of the discovered servers.");
            }

            LOG_POST(Warning << "Unable to execute [" << cmd << "] on any "
                    "of the discovered servers; will retry after delay.");

            SleepMilliSec(retry_delay);

            server = service_traversal->BeginIteration();

            number_of_servers = 0;
            ns_with_submits_disabled = 0;
            servers_throttled = 0;
        }

        --retry_count;

        timeout = NULL;
    }
}

void SNetServerPoolImpl::ResetServerConnections()
{
    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    NON_CONST_ITERATE(TNetServerByAddress, it, m_Servers) {
        it->second->m_CurrentConnectionGeneration.Add(1);
    }
}

SNetServerPoolImpl::~SNetServerPoolImpl()
{
    // Clean up m_Servers
    NON_CONST_ITERATE(TNetServerByAddress, it, m_Servers) {
        delete it->second;
    }

    if (m_LBSMAffinityValue != NULL)
        free((void*) m_LBSMAffinityValue);
}

SNetServiceImpl::~SNetServiceImpl()
{
    delete m_DiscoveredServers;

    // Clean up m_ServerGroupPool
    SDiscoveredServers* server_group = m_ServerGroupPool;
    while (server_group != NULL) {
        SDiscoveredServers* next_group = server_group->m_NextGroupInPool;
        delete server_group;
        server_group = next_group;
    }
}

void CNetServerPool::SetCommunicationTimeout(const STimeout& to)
{
    m_Impl->m_CommTimeout = to;
}
const STimeout& CNetServerPool::GetCommunicationTimeout() const
{
    return m_Impl->m_CommTimeout;
}

void CNetServerPool::SetPermanentConnection(ESwitch type)
{
    m_Impl->m_PermanentConnection = type;
}

CNetServiceIterator CNetService::Iterate(CNetService::EIterationMode mode)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

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
        m_Impl->m_ServiceName + " service.");
}

CNetServiceIterator CNetService::Iterate(CNetServer::TInstance priority_server)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == priority_server->m_ServerInPool)
            return new SNetServiceIterator_Circular(servers, it);
    }

    if (!servers->m_Servers.empty())
        // The requested server is not found in this service,
        // however there are servers, so return them.
        return new SNetServiceIteratorImpl(servers);

    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any available servers for the " +
        m_Impl->m_ServiceName + " service.");
}

CNetServiceIterator CNetService::ExcludeServer(CNetServer::TInstance server)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    ITERATE(TNetServerList, it, servers->m_Servers) {
        if (it->first == server->m_ServerInPool) {
            // The server is found. Make an iterator and
            // skip to the next server (the iterator may become NULL).
            CNetServiceIterator server_it(
                    new SNetServiceIterator_Circular(servers, it));
            return ++server_it;
        }
    }

    // The requested server is not found in this service, so
    // return the rest of servers or NULL if there are none.
    return !servers->m_Servers.empty() ?
            new SNetServiceIteratorImpl(servers) : NULL;
}

CNetServiceIterator CNetService::FindServer(INetServerFinder* finder,
    CNetService::EIterationMode mode)
{
    string error_messages;

    CNetServiceIterator it = Iterate(mode);

    for (; it; ++it) {
        try {
            if (finder->Consider(*it))
                break;
        }
        catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;

            if (!error_messages.empty())
                error_messages += '\n';

            error_messages += (*it)->m_ServerInPool->m_Address.AsString();
            error_messages += ": ";
            error_messages += ex.what();
        }
        catch (CIO_Exception& ex) {
            if (!error_messages.empty())
                error_messages += '\n';

            error_messages += (*it)->m_ServerInPool->m_Address.AsString();
            error_messages += ": ";
            error_messages += ex.what();
        }
    }

    if (!error_messages.empty()) {
        LOG_POST(error_messages);
    }

    return it;
}

CJsonNode g_ExecToJson(IExecToJson& exec_to_json, CNetService service,
        CNetService::EServiceType service_type,
        CNetService::EIterationMode iteration_mode)
{
    if (service_type == CNetService::eSingleServerService)
        return exec_to_json.ExecOn(service.Iterate().GetServer());

    CJsonNode result(CJsonNode::NewObjectNode());

    for (CNetServiceIterator it = service.Iterate(iteration_mode); it; ++it)
        result.SetByKey((*it).GetServerAddress(), exec_to_json.ExecOn(*it));

    return result;
}

END_NCBI_SCOPE
