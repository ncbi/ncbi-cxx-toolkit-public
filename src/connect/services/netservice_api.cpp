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

#include "../ncbi_comm.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"

#include "netservice_api_impl.hpp"

#include <connect/services/error_codes.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/srv_connections_expt.hpp>

#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_localip.hpp>

#include <corelib/ncbi_config.hpp>
#include <corelib/ncbi_message.hpp>
#include <corelib/ncbi_system.hpp>

#include <util/random_gen.hpp>
#include <util/checksum.hpp>

#include <deque>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

#define LBSMD_PENALIZED_RATE_BOUNDARY -0.01

BEGIN_NCBI_SCOPE

// The purpose of these classes is to execute commands suppressing possible errors and avoiding retries
struct SNoRetry : SNetServiceImpl::SRetry
{
    SNoRetry(SNetServiceImpl* service) :
        m_Service(service)
    {
        _ASSERT(m_Service);

        Swap(*m_Service, m_MaxRetries);
    }

    ~SNoRetry()
    {
        Swap(*m_Service, m_MaxRetries);
    }

protected:
    CNetRef<SNetServiceImpl> m_Service;

private:
    unsigned m_MaxRetries = 0;
};

struct SNoRetryNoErrors : SNoRetry
{
    SNoRetryNoErrors(SNetServiceImpl* service) :
        SNoRetry(service)
    {
        Set([](const string&, CNetServer) { return true; });
    }

    ~SNoRetryNoErrors()
    {
        Set(nullptr);
    }

private:
    void Set(CNetService::TEventHandler error_handler)
    {
        m_Service->m_Listener->SetErrorHandler(error_handler);
    }
};

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
    return m_Impl->GetServer();
}

CNetServer SNetServiceIteratorImpl::GetServer()
{
    auto& service = m_ServerGroup->m_Service;
    service->m_RebalanceStrategy.OnResourceRequested();
    return new SNetServerImpl(service, service->m_ServerPool->ReturnServer(m_Position->first));
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
    return m_Impl->GetRate();
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

bool SNetServiceIterator_RandomPivot::Prev()
{
    if (m_RandomIterators.empty() ||
            m_RandomIterator == m_RandomIterators.begin())
        return false;

    m_Position = *--m_RandomIterator;

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

SNetServiceIterator_Weighted::SNetServiceIterator_Weighted(
        SDiscoveredServers* server_group_impl, Uint4 key_crc32) :
    SNetServiceIteratorImpl(server_group_impl),
    m_KeyCRC32(key_crc32)
{
    TNetServerList::const_iterator server_list_iter(m_Position);

    if ((m_SingleServer =
            (++server_list_iter == server_group_impl->m_SuppressedBegin)))
        // Nothing to do if there's only one server.
        return;

    // Find the server with the highest rank.
    SServerRank highest_rank(x_GetServerRank(m_Position));

    do {
        SServerRank server_rank(x_GetServerRank(server_list_iter));
        if (highest_rank < server_rank)
            highest_rank = server_rank;
        // To avoid unnecessary memory allocations, do not save
        // the calculated server ranks in hope that Next()
        // will be called very rarely for this type of iterators.
    } while (++server_list_iter != server_group_impl->m_SuppressedBegin);

    m_Position = highest_rank.m_ServerListIter;
}

bool SNetServiceIterator_Weighted::Next()
{
    if (m_SingleServer)
        return false;

    if (m_ServerRanks.empty()) {
        TNetServerList::const_iterator server_list_iter(
                m_ServerGroup->m_Servers.begin());
        do
            m_ServerRanks.push_back(x_GetServerRank(server_list_iter));
        while (++server_list_iter != m_ServerGroup->m_SuppressedBegin);

        // Sort the ranks in *reverse* order.
        sort(m_ServerRanks.rbegin(), m_ServerRanks.rend());

        // Skip the server with the highest rank, which was the first
        // server returned by this iterator object.
        m_CurrentServerRank = m_ServerRanks.begin() + 1;
    } else if (++m_CurrentServerRank == m_ServerRanks.end())
        return false;

    m_Position = m_CurrentServerRank->m_ServerListIter;
    return true;
}

bool SNetServiceIterator_Weighted::Prev()
{
    if (m_SingleServer)
        return false;

    _ASSERT(!m_ServerRanks.empty());

    if (m_CurrentServerRank == m_ServerRanks.begin())
        return false;

    m_Position = (--m_CurrentServerRank)->m_ServerListIter;
    return true;
}

SNetServerPoolImpl::SNetServerPoolImpl(INetServerConnectionListener* listener) :
    m_PropCreator(listener->GetPropCreator()),
    m_EnforcedServer(0, 0),
    m_MaxTotalTime(CTimeout::eInfinite),
    m_UseOldStyleAuth(false)
{
}

SNetServiceImpl::SNetServiceImpl(const string& api_name, const string& service_name, const string& client_name,
        INetServerConnectionListener* listener, CSynRegistry& registry, const SRegSynonyms& sections) :
    m_Listener(listener),
    m_ServerPool(new SNetServerPoolImpl(listener)),
    m_ServiceName(service_name),
    m_RebalanceStrategy(registry, sections),
    m_RoundRobin(0),
    m_APIName(api_name),
    m_ClientName(client_name)
{
}

SNetServiceImpl::SNetServiceImpl(SNetServerInPool* server, SNetServiceImpl* prototype) :
    m_Listener(prototype->m_Listener->Clone()),
    m_ServerPool(prototype->m_ServerPool),
    m_ServiceName(server->m_Address.AsString()),
    m_RebalanceStrategy(prototype->m_RebalanceStrategy),
    m_RoundRobin(prototype->m_RoundRobin.load()),
    m_APIName(prototype->m_APIName),
    m_ClientName(prototype->m_ClientName),
    m_UseSmartRetries(prototype->m_UseSmartRetries),
    m_ConnectionMaxRetries(prototype->m_ConnectionMaxRetries),
    m_ConnectionRetryDelay(prototype->m_ConnectionRetryDelay),
    m_NetInfo(prototype->m_NetInfo)
{
    Construct(server);
}

SNetServiceImpl::SNetServiceImpl(const string& service_name, SNetServiceImpl* prototype) :
    m_Listener(prototype->m_Listener->Clone()),
    m_ServerPool(prototype->m_ServerPool),
    m_ServiceName(service_name),
    m_RebalanceStrategy(prototype->m_RebalanceStrategy),
    m_RoundRobin(prototype->m_RoundRobin.load()),
    m_APIName(prototype->m_APIName),
    m_ClientName(prototype->m_ClientName),
    m_UseSmartRetries(prototype->m_UseSmartRetries),
    m_ConnectionMaxRetries(prototype->m_ConnectionMaxRetries),
    m_ConnectionRetryDelay(prototype->m_ConnectionRetryDelay),
    m_NetInfo(prototype->m_NetInfo)
{
    Construct();
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
        if (auto address = SSocketAddress::Parse(m_ServiceName)) {
            Construct(m_ServerPool->FindOrCreateServerImpl(move(address)));
        } else {
            m_ServiceType = eLoadBalancedService;
        }
    }
}

SNetServiceImpl* SNetServiceImpl::Create(
        const string& api_name, const string& service_name, const string& client_name,
        INetServerConnectionListener* listener,
        CSynRegistry& registry, SRegSynonyms& sections, const string& ns_client_name)
{
    CNetRef<SNetServiceImpl> rv(new SNetServiceImpl(api_name, service_name, client_name, listener, registry, sections));
    rv->Init(registry, sections, ns_client_name);
    return rv.Release();
}

SNetServiceImpl* SNetServiceImpl::Clone(SNetServerInPool* server, SNetServiceImpl* prototype)
{
    return new SNetServiceImpl(server, prototype);
}

SNetServiceImpl* SNetServiceImpl::Clone(const string& service_name, SNetServiceImpl* prototype)
{
    return new SNetServiceImpl(service_name, prototype);
}

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
void CNetService::AllowXSiteConnections()
{
    SNetServiceXSiteAPI::AllowXSiteConnections();
}

bool CNetService::IsUsingXSiteProxy()
{
    return SNetServiceXSiteAPI::IsUsingXSiteProxy();
}

static const char kXSiteFwd[] = "XSITEFWD";

void SNetServiceXSiteAPI::AllowXSiteConnections()
{
    const auto local_ip = CSocketAPI::GetLocalHostAddress();
    const auto local_domain = GetDomain(local_ip);

    if (!local_domain) {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound, "Cannot determine local domain");
    }

    m_LocalDomain.store(local_domain);
    m_AllowXSiteConnections.store(true);
}

bool SNetServiceXSiteAPI::IsUsingXSiteProxy()
{
    return m_AllowXSiteConnections.load();
}

void SNetServiceXSiteAPI::InitXSite(CSynRegistry& registry, const SRegSynonyms& sections)
{
    if (registry.Get({ "netservice_api", sections }, "allow_xsite_conn", false)) {
        AllowXSiteConnections();
    }
}

void SNetServiceXSiteAPI::ConnectXSite(CSocket& socket,
        SNetServerImpl::SConnectDeadline& deadline,
        const SSocketAddress& original, const string& service)
{
    SSocketAddress actual(original);
    _ASSERT(actual.port);
    ticket_t ticket = 0;

    if (IsForeignAddr(actual.host)) {
        union {
            SFWDRequestReply rr;
            char buf[FWD_RR_MAX_SIZE + 1];
        };
        memset(&rr, 0, sizeof(rr));

        rr.host =                     actual.host;
        rr.port = SOCK_HostToNetShort(actual.port);
        rr.flag = SOCK_HostToNetShort(FWD_RR_FIREWALL | FWD_RR_KEEPALIVE);

        auto text_max = sizeof(buf)-1 - offsetof(SFWDRequestReply,text);
        auto text_len = service.size() ? min(service.size() + 1, text_max) : 0;
        memcpy(rr.text, service.c_str(), text_len);

        size_t len = 0;

        CConn_ServiceStream svc(kXSiteFwd);
        svc.rdbuf()->PUBSETBUF(0, 0);  // quick way to make stream unbuffered
        if (svc.write((const char*) &rr.ticket/*0*/, sizeof(rr.ticket))  &&
            svc.write(buf, offsetof(SFWDRequestReply,text) + text_len)) {
            svc.read(buf, sizeof(buf)-1);
            len = (size_t) svc.gcount();
            _ASSERT(len < sizeof(buf));
        }

        memset(buf + len, 0, sizeof(buf) - len); // NB: terminates "text" field

        if (len < offsetof(SFWDRequestReply,text)
            ||  (rr.flag & FWD_RR_ERRORMASK)  ||  !rr.port) {
            const char* err;
            if (len == 0)
                err = "Connection refused";
            else if (len < offsetof(SFWDRequestReply,text))
                err = "Short response received";
            else if (!(rr.flag & FWD_RR_ERRORMASK))
                err = rr.flag & FWD_RR_REJECTMASK
                    ? "Client rejected" : "Unknown error";
            else if (memcmp(buf, "NCBI", 4) != 0)
                err = rr.text[0] ? rr.text : "Unspecified error";
            else
                err = buf;
            NCBI_THROW_FMT(CNetSrvConnException, eConnectionFailure,
                           "Error while acquiring auth ticket from"
                           " cross-site connection proxy "
                           << kXSiteFwd << ": " << err);
        }

        if (rr.ticket) {
            ticket      = rr.ticket;
            actual.host =                     rr.host;
            actual.port = SOCK_NetToHostShort(rr.port);
        } else {
            SOCK sock;
            EIO_Status io_st = CONN_GetSOCK(svc.GetCONN(), &sock);
            if (sock)
                io_st = SOCK_CreateOnTop(sock, 0, &sock);
            _ASSERT(!sock == !(io_st == eIO_Success));
            if (sock) {
                // excess read data to return into sock
                text_len  = strlen(rr.text) + 1/*'\0'-terminated*/;
                if (text_len > text_max)
                    text_len = text_max;
                text_len += offsetof(SFWDRequestReply,text);
                _ASSERT(text_len <= len);
                io_st = SOCK_Pushback(sock, buf + text_len, len - text_len);
            }
            if (io_st != eIO_Success) {
                SOCK_Destroy(sock);
                const char* err = IO_StatusStr(io_st);
                NCBI_THROW_FMT(CNetSrvConnException, eConnectionFailure,
                               "Error while tunneling through proxy "
                               << kXSiteFwd << ": " << err);
            }
            socket.Reset(sock, eTakeOwnership, eCopyTimeoutsToSOCK);
            actual.port = 0;
        }
    }

    if (actual.port) {
        SNetServerImpl::ConnectImpl(socket, deadline, actual, original);
    }

    if (ticket  &&  socket.Write(&ticket, sizeof(ticket)) != eIO_Success) {
        NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                   "Error while sending proxy auth ticket");
    }
}

int SNetServiceXSiteAPI::GetDomain(unsigned int ip)
{
    TNCBI_IPv6Addr addr;
    NcbiIPv4ToIPv6(&addr, ip, 0);

    SNcbiDomainInfo info;
    NcbiIsLocalIPEx(&addr, &info);

    return info.num;
}

bool SNetServiceXSiteAPI::IsForeignAddr(unsigned int ip)
{
    if (!IsUsingXSiteProxy()) return false;

    const auto d = GetDomain(ip);
    return d && (d != m_LocalDomain);
}

atomic<int> SNetServiceXSiteAPI::m_LocalDomain{0};
atomic<bool> SNetServiceXSiteAPI::m_AllowXSiteConnections{false};

#else

void SNetServiceXSiteAPI::InitXSite(CSynRegistry&, const SRegSynonyms&)
{
}

void SNetServiceXSiteAPI::ConnectXSite(CSocket& socket,
        SNetServerImpl::SConnectDeadline& deadline,
        const SSocketAddress& original, const string&)
{
    SNetServerImpl::ConnectImpl(socket, deadline, original, original);
}

#endif

void SNetServiceImpl::Init(CSynRegistry& registry, SRegSynonyms& sections, const string& ns_client_name)
{
    // Initialize the connect library and LBSM structures
    // used in DiscoverServersIfNeeded().
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        public:
            void NoOp() {};
        } conn_initer;
        conn_initer.NoOp();
    }

    NStr::TruncateSpacesInPlace(m_ServiceName);

    // TODO:
    // Do not check for emptiness and always read values (client, service, etc) from registry
    // after values provided in ctors get into an underlying memory registry.

    // Do not override explicitly set client name
    if (m_ClientName.empty()) m_ClientName = registry.Get(sections, { "client_name", "client" }, "");

    // Use client name from NetSchedule API if it's not provided for NetCache API
    if (m_ClientName.empty()) m_ClientName = ns_client_name;

    if (m_ServiceName.empty()) {
        m_ServiceName = registry.Get(sections, { "service", "service_name" }, "");

        if (m_ServiceName.empty()) {
            string host = registry.Get(sections, { "server", "host" }, "");
            string port = registry.Get(sections, "port", "");

            if (!host.empty() && !port.empty()) m_ServiceName = host + ":" + port;
        }
    }

    InitXSite(registry, sections);

    m_UseSmartRetries = registry.Get(sections, "smart_service_retries", true);

    int max_retries = registry.Get({ sections, "netservice_api" }, "connection_max_retries", CONNECTION_MAX_RETRIES);
    m_ConnectionMaxRetries = max_retries >= 0 ? max_retries : CONNECTION_MAX_RETRIES;

    double retry_delay = registry.Get({ sections, "netservice_api" }, "retry_delay", RETRY_DELAY_DEFAULT);
    if (retry_delay < 0) retry_delay = RETRY_DELAY_DEFAULT;
    m_ConnectionRetryDelay = static_cast<unsigned long>(retry_delay * kMilliSecondsPerSecond);

    if (m_ClientName.empty() || m_ClientName == "noname" ||
            NStr::FindNoCase(m_ClientName, "unknown") != NPOS) {
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if (!app) {
            NCBI_THROW_FMT(CArgException, eNoValue,
                m_APIName << ": client name is not set");
        }
        m_ClientName = app->GetProgramDisplayName();
    }

    m_ServerPool->Init(registry, sections);

    Construct();
}

void SNetServerPoolImpl::Init(CSynRegistry& registry, const SRegSynonyms& sections)
{
    const auto kConnTimeoutDefault = 2.0;
    const auto kCommTimeoutDefault = 12.0;
    const auto kFirstServerTimeoutDefault = 0.0;

    m_LBSMAffinity.first = registry.Get(sections, "use_lbsm_affinity", "");

    // Get affinity value from the local LBSM configuration file.
    if (!m_LBSMAffinity.first.empty()) {
        m_LBSMAffinity.second = LBSMD_GetHostParameter(SERV_LOCALHOST, m_LBSMAffinity.first.c_str());
    }

    double conn_timeout = registry.Get(sections, "connection_timeout", kConnTimeoutDefault);
    g_CTimeoutToSTimeout(CTimeout(conn_timeout > 0 ? conn_timeout : kConnTimeoutDefault), m_ConnTimeout);

    double comm_timeout = registry.Get({ sections, "netservice_api" }, "communication_timeout", kCommTimeoutDefault);
    g_CTimeoutToSTimeout(CTimeout(comm_timeout > 0 ? comm_timeout : kCommTimeoutDefault), m_CommTimeout);

    double first_srv_timeout = registry.Get(sections, "first_server_timeout", kFirstServerTimeoutDefault);
    g_CTimeoutToSTimeout(CTimeout(first_srv_timeout > 0 ? first_srv_timeout : kFirstServerTimeoutDefault), m_FirstServerTimeout);

    double max_total_time = registry.Get(sections, "max_connection_time", 0.0);
    if (max_total_time > 0) m_MaxTotalTime = CTimeout(max_total_time);

    m_ThrottleParams.Init(registry, sections);
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
    auth += NStr::PrintableString(m_ClientName);
    auth += '\"';

    if (!m_ServerPool->m_UseOldStyleAuth) {
        if (m_ServiceType == eLoadBalancedService) {
            auth += " svc=\"";
            auth += NStr::PrintableString(m_ServiceName);
            auth += '\"';
        }

        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if (app) {
            auth += " client_path=\"";
            auth += NStr::PrintableString(app->GetProgramExecutablePath());
            auth += '\"';
        }
    }

    return auth;
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
    return m_Impl->IsLoadBalanced();
}

void CNetServerPool::StickToServer(SSocketAddress address)
{
    m_Impl->m_EnforcedServer = move(address);
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

        switch (output_style) {
        case eSingleLineOutput:
            output_stream << (*it).ExecWithRetry(cmd, false).response << endl;
            break;

        case eUrlEncodedOutput:
            {
                CUrlArgs url_parser((*it).ExecWithRetry(cmd, false).response);

                ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
                    output_stream << field->name <<
                            ": " << field->value << endl;
                }
            }
            break;

        default:
            {
                CNetServerMultilineCmdOutput output(
                        (*it).ExecWithRetry(cmd, true));

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
        SSocketAddress server_address)
{
    pair<TNetServerByAddress::iterator, bool> loc(m_Servers.insert(
            TNetServerByAddress::value_type(server_address,
                    (SNetServerInPool*) NULL)));

    if (!loc.second)
        return loc.first->second;

    auto* server = new SNetServerInPool(move(server_address), m_PropCreator(), m_ThrottleParams);

    loc.first->second = server;

    return server;
}

CRef<SNetServerInPool> SNetServerPoolImpl::ReturnServer(
        SNetServerInPool* server_impl)
{
    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    server_impl->m_ServerPool = this;
    return CRef<SNetServerInPool>(server_impl);
}

CNetServer SNetServerPoolImpl::GetServer(SNetServiceImpl* service, SSocketAddress server_address)
{
    CFastMutexGuard server_mutex_lock(m_ServerMutex);

    auto* server = FindOrCreateServerImpl(m_EnforcedServer.host == 0 ? move(server_address) : m_EnforcedServer);
    server->m_ServerPool = this;

    return new SNetServerImpl(service, server);
}

CNetServer SNetServiceImpl::GetServer(SSocketAddress server_address)
{
    m_RebalanceStrategy.OnResourceRequested();
    return m_ServerPool->GetServer(this, move(server_address));
}

CNetServer CNetService::GetServer(const string& host,
        unsigned short port)
{
    return m_Impl->GetServer(SSocketAddress(host, port));
}

CNetServer CNetService::GetServer(unsigned host, unsigned short port)
{
    return m_Impl->GetServer(SSocketAddress(host, port));
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

CNetServer::SExecResult SNetServiceImpl::FindServerAndExec(const string& cmd,
        bool multiline_output)
{
    switch (m_ServiceType) {
    default: // eServiceNotDefined
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                m_APIName << ": service name is not set");

    case eLoadBalancedService:
        {
            CNetServer::SExecResult exec_result;

            SRandomServiceTraversal random_traversal(this);

            IterateUntilExecOK(cmd, multiline_output,
                    exec_result, &random_traversal,
                    SNetServiceImpl::eIgnoreServerErrors);

            return exec_result;
        }

    case eSingleServerService:
        {
            CNetServer server(new SNetServerImpl(this,
                    m_ServerPool->ReturnServer(
                    m_DiscoveredServers->m_Servers.front().first)));

            return server.ExecWithRetry(cmd, multiline_output);
        }
    }
}

CNetServer::SExecResult CNetService::FindServerAndExec(const string& cmd,
        bool multiline_output)
{
    return m_Impl->FindServerAndExec(cmd, multiline_output);
}

void CNetService::ExecOnAllServers(const string& cmd)
{
    for (CNetServiceIterator it = Iterate(eIncludePenalized); it; ++it)
        (*it).ExecWithRetry(cmd, false);
}

void SNetServiceImpl::DiscoverServersIfNeeded()
{
    if (m_ServiceType == eServiceNotDefined) {
        NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
            m_APIName << ": service name is not set");
    }

    if (m_ServiceType == eLoadBalancedService) {
        // The service is load-balanced, check if rebalancing is required.
        m_RebalanceStrategy.OnResourceRequested();
        if (m_RebalanceStrategy.NeedRebalance())
            ++m_LatestDiscoveryIteration;

        if (m_DiscoveredServers == NULL ||
            m_DiscoveredServers->m_DiscoveryIteration !=
                m_LatestDiscoveryIteration) {
            // The requested server group either does not exist or
            // does not contain up-to-date server list, thus it needs
            // to be created anew.

            const TSERV_Type types = fSERV_Standalone | fSERV_IncludeStandby |
                fSERV_IncludeReserved | fSERV_IncludeSuppressed;

            auto discovered = CServiceDiscovery::DiscoverImpl(m_ServiceName, types, m_NetInfo, m_ServerPool->m_LBSMAffinity,
                    TServConn_MaxFineLBNameRetries::GetDefault(), m_ConnectionRetryDelay);

            SDiscoveredServers* server_group = m_DiscoveredServers;

            if (server_group != NULL && !server_group->m_Service)
                server_group->Reset(m_LatestDiscoveryIteration);
            else
                // Either the group does not exist or it has been
                // "issued" to the outside callers; allocate a new one.
                server_group = m_DiscoveredServers =
                    AllocServerGroup(m_LatestDiscoveryIteration);

            CFastMutexGuard server_mutex_lock(m_ServerPool->m_ServerMutex);

            TNetServerList& servers = server_group->m_Servers;
            TNetServerList::size_type number_of_regular_servers = 0;
            TNetServerList::size_type number_of_standby_servers = 0;
            double max_standby_rate = LBSMD_PENALIZED_RATE_BOUNDARY;

            // Fill the 'servers' array in accordance with the layout
            // described above the SDiscoveredServers::m_Servers declaration.
            for (const auto& d : discovered) {
                SNetServerInPool* server = m_ServerPool->FindOrCreateServerImpl(d.first);
                server->m_ThrottleStats.Discover();

                TServerRate server_rate(server, d.second);

                if (d.second > 0)
                    servers.insert(servers.begin() +
                        number_of_regular_servers++, server_rate);
                else if (d.second < max_standby_rate ||
                        d.second <= LBSMD_PENALIZED_RATE_BOUNDARY)
                    servers.push_back(server_rate);
                else {
                    servers.insert(servers.begin() +
                        number_of_regular_servers, server_rate);
                    if (d.second == max_standby_rate)
                        ++number_of_standby_servers;
                    else {
                        max_standby_rate = d.second;
                        number_of_standby_servers = 1;
                    }
                }
            }

            server_group->m_SuppressedBegin = servers.begin() +
                (number_of_regular_servers > 0 ?
                    number_of_regular_servers : number_of_standby_servers);

            server_mutex_lock.Release();
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

struct SFailOnlyWarnings : deque<pair<string, CNetServer>>
{
    SFailOnlyWarnings(CRef<INetServerConnectionListener> listener) : m_Listener(listener) {}
    ~SFailOnlyWarnings() { IssueAndClear(); }

    void IssueAndClear()
    {
        for (auto& w : *this) {
            m_Listener->OnWarning(w.first, w.second);
        }

        clear();
    }

private:
    CRef<INetServerConnectionListener> m_Listener;
};

void SNetServiceImpl::IterateUntilExecOK(const string& cmd,
    bool multiline_output,
    CNetServer::SExecResult& exec_result,
    IServiceTraversal* service_traversal,
    SNetServiceImpl::EServerErrorHandling error_handling)
{
    int retry_count = m_ConnectionMaxRetries;

    const unsigned long retry_delay = m_ConnectionRetryDelay;

    const CTimeout& max_total_time = m_ServerPool->m_MaxTotalTime;
    CDeadline deadline(max_total_time);

    enum EIterationMode {
        eInitialIteration,
        eRetry
    } iteration_mode = eInitialIteration;
    CNetServer server = service_traversal->BeginIteration();

    vector<CNetServer> servers_to_retry;
    unsigned current_server = 0;

    bool skip_server;

    unsigned number_of_servers = 0;
    unsigned ns_with_submits_disabled = 0;
    unsigned servers_throttled = 0;
    bool blob_not_found = false;

    const auto& fst = m_ServerPool->m_FirstServerTimeout;
    const bool use_fst = (fst.sec || fst.usec) && (retry_count > 0 || m_UseSmartRetries);
    const STimeout* timeout = use_fst ? &fst : nullptr;

    SFailOnlyWarnings fail_only_warnings(m_Listener);

    for (;;) {
        skip_server = false;

        try {
            server->ConnectAndExec(cmd, multiline_output, exec_result,
                    timeout);
            fail_only_warnings.clear();
            return;
        }
        catch (CNetCacheBlobTooOldException& /*ex rethrown*/) {
            throw;
        }
        catch (CNetCacheException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            if (error_handling == eRethrowAllServerErrors)
                throw;
            if (ex.GetErrCode() == CNetCacheException::eBlobNotFound) {
                blob_not_found = true;
                skip_server = true;
            } else if (error_handling == eRethrowServerErrors)
                throw;
            else
                m_Listener->OnWarning(ex.GetMsg(), server);
        }
        catch (CNetScheduleException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            if (error_handling == eRethrowAllServerErrors)
                throw;
            if (ex.GetErrCode() == CNetScheduleException::eSubmitsDisabled) {
                ++ns_with_submits_disabled;
                skip_server = true;
                fail_only_warnings.emplace_back(ex.GetMsg(), server);
            } else if (error_handling == eRethrowServerErrors)
                throw;
            else
                m_Listener->OnWarning(ex.GetMsg(), server);
        }
        catch (CNetSrvConnException& ex) {
            if (retry_count <= 0 && !m_UseSmartRetries)
                throw;
            switch (ex.GetErrCode()) {
            case CNetSrvConnException::eReadTimeout:
            case CNetSrvConnException::eConnectionFailure:
                m_Listener->OnWarning(ex.GetMsg(), server);
                break;

            case CNetSrvConnException::eServerThrottle:
                ++servers_throttled;
                fail_only_warnings.emplace_back(ex.GetMsg(), server);
                break;

            default:
                throw;
            }
        }

        ++number_of_servers;

        if (iteration_mode == eInitialIteration) {
            if (!skip_server)
                servers_to_retry.push_back(server);
            server = service_traversal->NextServer();
        } else {
            if (!skip_server)
                ++current_server;
            else
                servers_to_retry.erase(servers_to_retry.begin() +
                        current_server);

            if (current_server < servers_to_retry.size())
                server = servers_to_retry[current_server];
            else
                server = NULL;
        }

        if (!blob_not_found && !deadline.IsInfinite() &&
                deadline.GetRemainingTime().GetAsMilliSeconds() <= (server ? 0 : retry_delay)) {
            NCBI_THROW_FMT(CNetSrvConnException, eReadTimeout, "Exceeded max_connection_time=" <<
                    max_total_time.GetAsMilliSeconds() << "; cmd=[" << cmd << "]");
        }

        if (!server) {
            if (number_of_servers == ns_with_submits_disabled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all NetSchedule servers are "
                        "in REFUSESUBMITS mode for the " + m_ServiceName + " service.");
            }

            if (number_of_servers == servers_throttled) {
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Cannot execute ["  << cmd <<
                        "]: all servers are throttled for the " + m_ServiceName + " service.");
            }

            if (retry_count <= 0 || servers_to_retry.empty()) {
                if (blob_not_found) {
                    NCBI_THROW_FMT(CNetCacheException, eBlobNotFound,
                            "Cannot execute ["  << cmd << "]: blob not found.");
                }
                NCBI_THROW_FMT(CNetSrvConnException, eSrvListEmpty,
                        "Unable to execute [" << cmd <<
                        "] on any of the discovered servers for the " + m_ServiceName + " service.");
            }

            fail_only_warnings.IssueAndClear();
            ERR_POST(Warning << "Unable to send [" << cmd << "] to any "
                    "of the discovered servers; will retry after delay.");

            SleepMilliSec(retry_delay);

            number_of_servers = 0;
            ns_with_submits_disabled = 0;
            servers_throttled = 0;

            iteration_mode = eRetry;
            server = servers_to_retry[current_server = 0];
        }

        --retry_count;

        timeout = NULL;
    }
}

shared_ptr<void> SNetServiceImpl::CreateRetryGuard(SRetry::EType type)
{
    switch (type)
    {
        case SRetry::eNoRetryNoErrors: return make_shared<SNoRetryNoErrors>(this);
        case SRetry::eNoRetry:         return make_shared<SNoRetry>(this);
        default:                       return nullptr;
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

    if (m_LBSMAffinity.second) free(const_cast<char*>(m_LBSMAffinity.second));
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

CNetServiceIterator CNetService::Iterate(CNetService::EIterationMode mode)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    if (mode != eIncludePenalized) {
        if (servers->m_Servers.begin() < servers->m_SuppressedBegin) {
            if (mode == eSortByLoad)
                return new SNetServiceIterator_OmitPenalized(servers);
            else if (mode == eRoundRobin) {
                auto begin = servers->m_Servers.begin() + m_Impl->m_RoundRobin++ % servers->m_Servers.size();
                return new SNetServiceIterator_Circular(servers, begin);
            } else
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
    return m_Impl->Iterate(priority_server);
}

SNetServiceIteratorImpl* SNetServiceImpl::Iterate(CNetServer::TInstance priority_server)
{
    CRef<SDiscoveredServers> servers;
    GetDiscoveredServers(servers);

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
        m_ServiceName + " service.");
}

CNetServiceIterator CNetService::IterateByWeight(const string& key)
{
    CRef<SDiscoveredServers> servers;
    m_Impl->GetDiscoveredServers(servers);

    if (servers->m_Servers.begin() == servers->m_SuppressedBegin) {
        NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
            "Couldn't find any available servers for the " +
            m_Impl->m_ServiceName + " service.");
    }

    CChecksum key_crc32(CChecksum::eCRC32);

    key_crc32.AddChars(key.data(), key.length());

    return new SNetServiceIterator_Weighted(servers, key_crc32.GetChecksum());
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
            CNetServiceIterator circular_iter(
                    new SNetServiceIterator_Circular(servers, it));
            return ++circular_iter;
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

CNetService CNetService::Clone(const string& name)
{
    _ASSERT(m_Impl);
    return name == m_Impl->m_ServiceName ? m_Impl :
        SNetServiceImpl::Clone(name, m_Impl);
}

void CNetService::SetErrorHandler(TEventHandler error_handler)
{
    m_Impl->m_Listener->SetErrorHandler(error_handler);
}

void CNetService::SetWarningHandler(TEventHandler warning_handler)
{
    m_Impl->m_Listener->SetWarningHandler(warning_handler);
}

CNetService SNetServiceMap::GetServiceByName(const string& service_name,
        SNetServiceImpl* prototype)
{
    CFastMutexGuard guard(m_ServiceMapMutex);
    return GetServiceByNameImpl(service_name, prototype);
}

CNetService SNetServiceMap::GetServiceByNameImpl(const string& service_name,
        SNetServiceImpl* prototype)
{
    pair<TNetServiceByName::iterator, bool> loc(m_ServiceByName.insert(
            TNetServiceByName::value_type(service_name, CNetService())));

    return !loc.second ? loc.first->second :
            (loc.first->second =
                    SNetServiceImpl::Clone(service_name, prototype));
}

bool SNetServiceMap::IsAllowed(const string& service_name) const
{
    // Not restricted or found
    return !m_Restricted || m_Allowed.count(service_name);
}

bool SNetServiceMap::IsAllowed(CNetServer::TInstance server,
        SNetServiceImpl* prototype)
{
    if (!m_Restricted) return true;

    CFastMutexGuard guard(m_ServiceMapMutex);

    // Check if server belongs to some allowed service
    for (auto& service_name: m_Allowed) {
        CNetService service(GetServiceByNameImpl(service_name, prototype));

        if (service->IsInService(server)) return true;
    }

    return false;
}

void SNetServiceMap::AddToAllowed(const string& service_name)
{
    m_Allowed.insert(service_name);
}

CJsonNode g_ExecToJson(IExecToJson& exec_to_json, CNetService service,
        CNetService::EIterationMode iteration_mode)
{
    if (!service.IsLoadBalanced())
        return exec_to_json.ExecOn(service.Iterate().GetServer());

    CJsonNode result(CJsonNode::NewObjectNode());

    for (CNetServiceIterator it = service.Iterate(iteration_mode); it; ++it)
        result.SetByKey((*it).GetServerAddress(), exec_to_json.ExecOn(*it));

    return result;
}

CNetService CNetService::Create(const string& api_name, const string& service_name, const string& client_name)
{
    struct SNoOpConnectionListener : public INetServerConnectionListener
    {
        INetServerConnectionListener* Clone() override { return new SNoOpConnectionListener(*this); }
        void OnConnected(CNetServerConnection&) override {}

    private:
        void OnErrorImpl(const string&, CNetServer&) override {}
        void OnWarningImpl(const string&, CNetServer&) override {}
    };

    CSynRegistryBuilder registry_builder;
    SRegSynonyms sections(api_name);

    return SNetServiceImpl::Create(api_name, service_name, client_name, new SNoOpConnectionListener,
            registry_builder, sections);
}

void g_AppendClientIPSessionIDHitID(string& cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    g_AppendClientIPAndSessionID(cmd, req);
    g_AppendHitID(cmd, req);
}

END_NCBI_SCOPE
