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
            TRandomIterators::iterator it = m_RandomIterators.begin();
            while (++it != m_RandomIterators.end())
                swap(*it, m_RandomIterators[s_RandomIteratorGen.GetRand(1,
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

bool SNetServiceIterator_RandomPivot::Prev()
{
    if (m_RandomIterators.empty() ||
            m_RandomIterator == m_RandomIterators.begin())
        return false;

    m_Position = *--m_RandomIterator;

    return true;
}

SNetServerPoolImpl::SNetServerPoolImpl(const string& api_name,
        const string& client_name) :
    m_APIName(api_name),
    m_ClientName(client_name),
    m_LBSMAffinityName(kEmptyStr),
    m_LBSMAffinityValue(NULL),
    m_UseOldStyleAuth(false)
{
}

void SNetServiceImpl::ZeroInit()
{
    m_ServiceType = eServiceNotDefined;
    m_DiscoveredServers = NULL;
    m_ServerGroupPool = NULL;
    m_LatestDiscoveryIteration = 0;
}

void SNetServiceImpl::Construct(SNetServerInPool* server)
{
    m_ServiceType = eSingleServerService;
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
            m_ServiceType = eLoadBalancedService;
        else {
            Construct(m_ServerPool->FindOrCreateServerImpl(
                    g_NetService_gethostip(host),
                    (unsigned short) NStr::StringToInt(port)));
            return;
        }
    }
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

    m_ServiceName = service_name;
    NStr::TruncateSpacesInPlace(m_ServiceName);

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
    }

    m_ServerPool->Init(config, section);

    m_Listener->OnInit(api_impl, config, config_section);

    Construct();
}

void SNetServerPoolImpl::Init(CConfig* config, const string& section)
{
    if (config != NULL) {
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

            m_IOFailureThresholdNumerator = numerator;
            m_IOFailureThresholdDenominator = denominator;

            m_MaxConsecutiveIOFailures = config->GetInt(section,
                "throttle_by_subsequent_connection_failures",
                    CConfig::eErr_NoThrow,
                        THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT);

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

    auth += '\"';
    auth += NStr::PrintableString(m_ServerPool->m_ClientName);
    auth += '\"';

    if (!m_ServerPool->m_UseOldStyleAuth) {
        if (m_ServiceType == eLoadBalancedService) {
            auth += " svc=\"";
            auth += m_ServiceName;
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

bool CNetService::IsLoadBalanced() const
{
    return m_Impl->m_ServiceType == eLoadBalancedService;
}

void CNetServerPool::StickToServer(const string& host, unsigned port)
{
    m_Impl->m_EnforcedServer = m_Impl->FindOrCreateServerImpl(
            g_NetService_gethostip(host), port);
}

void CNetService::PrintCmdOutput(const string& cmd,
    CNcbiOstream& output_stream, CNetService::ECmdOutputStyle output_style)
{
    bool print_headers = IsLoadBalanced() ?
        output_style != eMultilineOutput_NoHeaders : false;

    for (CNetServiceIterator it = Iterate(); it; ++it) {
        if (print_headers)
            output_stream << '[' << (*it).GetServerAddress() << ']' << endl;

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

SNetServerInPool* SNetServerPoolImpl::FindOrCreateServerImpl(
    const string& host, unsigned short port)
{
    SServerAddress server_address(host, port);

    pair<TNetServerByAddress::iterator, bool> loc(m_Servers.insert(
            TNetServerByAddress::value_type(server_address,
                    (SNetServerInPool*) NULL)));

    if (!loc.second)
        return loc.first->second;

    SNetServerInPool* server = new SNetServerInPool(host, port);

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

CNetServer SNetServiceImpl::GetServer(const string& host, unsigned int port)
{
    m_ServerPool->m_RebalanceStrategy->OnResourceRequested();

    CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);

    SNetServerInPool* server = m_ServerPool->m_EnforcedServer ?
            (SNetServerInPool*) m_ServerPool->m_EnforcedServer :
                m_ServerPool->FindOrCreateServerImpl(host, port);

    server->m_ServerPool = m_ServerPool;

    return new SNetServerImpl(this, server);
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
    switch (m_Impl->m_ServiceType) {
    default: // eServiceNotDefined
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                m_Impl->m_ServerPool->m_APIName << ": command '" << cmd <<
                        "' requires a server but none specified");

    case eLoadBalancedService:
        {
            CNetServer::SExecResult exec_result;

            SRandomIterationBeginner iteration_beginner(*this);

            m_Impl->IterateUntilExecOK(cmd, exec_result, &iteration_beginner);

            return exec_result;
        }

    case eSingleServerService:
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
    for (CNetServiceIterator it = Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}

void SNetServiceImpl::DiscoverServersIfNeeded()
{
    if (m_ServiceType == eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_ServerPool->m_APIName << ": service name is not set");
    }

    if (m_ServiceType == eLoadBalancedService) {
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
                    SNetServerInPool* server =
                        m_ServerPool->FindOrCreateServerImpl(
                            CSocketAPI::ntoa(sinfo->host), sinfo->port);
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

static void SleepUntil(const CTime& time)
{
    CTime current_time(GetFastLocalTime());
    if (time > current_time) {
        CTimeSpan diff(time - current_time);
        SleepMicroSec(diff.GetCompleteSeconds() * 1000 * 1000 +
            diff.GetNanoSecondsAfterSecond() / 1000);
    }
}

void SNetServiceImpl::IterateUntilExecOK(const string& cmd,
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
                    (m_ServerPool->m_MaxConnectionTime == 0 ||
                        GetFastLocalTime() < max_connection_time.AddNanoSecond(
                            m_ServerPool->m_MaxConnectionTime * 1000 * 1000)))
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

        if (m_ServerPool->m_MaxConnectionTime > 0 &&
                GetFastLocalTime() >= max_connection_time) {
            LOG_POST(Error << "Timeout (max_connection_time=" <<
                m_ServerPool->m_MaxConnectionTime <<
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

CNetServiceIterator CNetService::Iterate(
        CNetServer::TInstance priority_server)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    // Find the requested server among the discovered servers.
    for (TNetServerList::const_iterator it = servers->m_Servers.begin();
            it != servers->m_SuppressedBegin; ++it)
        if (it->first == priority_server->m_ServerInPool)
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
            TServerRate(priority_server->m_ServerInPool, 1));
        servers->m_Service = m_Impl;
        servers->m_SuppressedBegin = servers->m_Servers.end();
        return new SNetServiceIterator_RandomPivot(servers,
            servers->m_Servers.begin());
    }
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

END_NCBI_SCOPE
