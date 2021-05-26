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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <array>

#include "connect_misc_impl.hpp"

#include "ncbi_servicep.h"

#include <connect/impl/connect_misc.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbisys.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif

BEGIN_NCBI_SCOPE

// This class also initializes CONNECT library in its constructor (via CConnIniter base)
struct SSocketAddressImpl : protected CConnIniter
{
    // Do not make static (see above)
    unsigned GetHost(const string& name) const
    {
        return CSocketAPI::gethostbyname(name, eOn);
    }

    const string& GetName(unsigned host)
    {
        auto& name = m_Data[host];

        // Name was not looked up yet or host changed
        if (name.empty()) {
            name = CSocketAPI::gethostbyaddr(host, eOn);

            if (name.empty()) {
                name = CSocketAPI::ntoa(host);
            }
        }

        return name;
    }

    static SSocketAddressImpl& GetInstance()
    {
        thread_local static SSocketAddressImpl impl;
        return impl;
    }

private:
    map<unsigned, string> m_Data;
};

SSocketAddress::SHost::SHost(const string& h) :
    host(SSocketAddressImpl::GetInstance().GetHost(h))
{
}

string SSocketAddress::GetHostName() const
{
    return SSocketAddressImpl::GetInstance().GetName(host);
}

SSocketAddress SSocketAddress::Parse(const string& address)
{
    string host, port;

    if (NStr::SplitInTwo(address, ":", host, port)) {
        return { host, port };
    }

    return { 0, 0 };
}

bool operator==(const SSocketAddress& lhs, const SSocketAddress& rhs)
{
    return lhs.host == rhs.host && lhs.port == rhs.port;
}

bool operator< (const SSocketAddress& lhs, const SSocketAddress& rhs)
{
    if (lhs.host != rhs.host) return lhs.host < rhs.host;

    return lhs.port < rhs.port;
}

// This class also initializes CONNECT library in its constructor (via CConnIniter base)
struct SServiceDiscoveryImpl : protected CConnIniter
{
    // Do not make static (see above)
    shared_ptr<void> GetSingleServer(const string& service_name) const
    {
        if (auto address = SSocketAddress::Parse(service_name)) {
            CServiceDiscovery::TServer server(move(address), 1.0);
            return make_shared<CServiceDiscovery::TServers>(1, move(server));
        }

        return {};
    }
};

CServiceDiscovery::CServiceDiscovery(const string& service_name) :
    m_ServiceName(service_name),
    m_Data(SServiceDiscoveryImpl().GetSingleServer(m_ServiceName)),
    m_IsSingleServer(m_Data)
{
}

CServiceDiscovery::TServers CServiceDiscovery::operator()()
{
    // Single server "discovery"
    if (m_IsSingleServer) {
        _ASSERT(m_Data);
        return *static_pointer_cast<TServers>(m_Data);
    }

    const TSERV_Type types = fSERV_Standalone | fSERV_IncludeStandby;
    return DiscoverImpl(m_ServiceName, types, m_Data, {}, 0, 0);
}

CServiceDiscovery::TServers CServiceDiscovery::DiscoverImpl(const string& service_name, unsigned types,
        shared_ptr<void>& net_info, pair<string, const char*> lbsm_affinity,
        int try_count, unsigned long retry_delay)
{
    TServers rv;

    // Query the Load Balancer.
    for (;;) {
        if (!net_info) {
            net_info.reset(ConnNetInfo_Create(service_name.c_str()), ConnNetInfo_Destroy);
        }

        if (auto it = make_c_unique(SERV_OpenP(service_name.c_str(), types, SERV_LOCALHOST, 0, 0.0,
                        static_cast<const SConnNetInfo*>(net_info.get()), NULL, 0, 0 /*false*/,
                        lbsm_affinity.first.c_str(), lbsm_affinity.second), SERV_Close)) {

            while (auto info = SERV_GetNextInfoEx(it.get(), 0)) {
                if (info->time > 0 && info->time != NCBI_TIME_INFINITE && info->rate != 0.0) {
                    rv.emplace_back(SSocketAddress(info->host, info->port), info->rate);
                }
            }

            break;
        }

        // FIXME Retry logic can be removed as soon as LBSMD with
        // packet compression is installed universally.
        if (--try_count < 0) {
            break;
        }

        ERR_POST("Could not find LB service name '" << service_name << "', will retry after delay");
        SleepMilliSec(retry_delay);
    }

    return rv;
}

CLogLatencies::TResult CLogLatencies::Parse(const TData& data)
{
    using namespace chrono;

    enum : size_t { eServer = 1 };
    enum : size_t { fNothing = 0, fStart, fStop, eBoth };
    array<const char*, eBoth> prefixes{ "         ", "Start -> ", "Stop ->  " };

    cmatch m;

    using TServer = pair<size_t, array<system_clock::time_point, eBoth>>;
    unordered_map<string, TServer> servers;
    TServer* current = nullptr;
    TResult latencies;

    for (const auto& msg : data) {
        const auto msg_start = msg.m_Buffer;
        const auto msg_len = min((size_t)1024, msg.m_BufferLen);
        const auto msg_end = msg_start + msg_len;
        size_t matched = fNothing;

        if (regex_match(msg_start, msg_end, m, m_Start)) {
            matched = fStart;
            current = &servers[m[eServer].str()];

        } else if (regex_match(msg_start, msg_end, m, m_Stop)) {
            matched = fStop;

            // If there is a server specified
            if (m.size() > eServer) {
                current = &servers[m[eServer].str()];
            }
        }

        if (matched && current) {
            auto t = msg.GetTime();
            auto tp = system_clock::from_time_t(t.GetTimeT()) + microseconds(t.MicroSecond());
            current->first |= matched;
            current->second[matched] = tp;
        }

        if (m_Debug) {
            cerr << prefixes[matched];
            msg.Write(cerr);
        }
    }

    for (const auto& server : servers) {
        const auto& server_name = server.first;
        const auto& server_data = server.second;

        if (server_data.first == eBoth) {
            const auto& start = server_data.second[fStart];
            const auto& stop = server_data.second[fStop];
            latencies.emplace(server_name, duration_cast<microseconds>(stop - start));
        }
    }

    return latencies;
}

CLogLatencyReport::~CLogLatencyReport()
{
    // It has not been started, nothing to report
    if (!m_Handler) {
        return;
    }

    try {
        SetDiagHandler(nullptr);
        const auto latencies = Parse(*m_Handler);

        for (const auto& server : latencies) {
            const auto& server_name = server.first;
            const auto& server_latency = server.second;
            cerr << "server=" << server_name << "&latency=" << server_latency.count() << endl;
        }
    }
    catch (exception& ex) {
        cerr << "Exception on calculating latencies: " << ex.what();
    }
}

void CLogLatencyReport::Start()
{
    // If it has already been started
    if (m_Handler) {
        return;
    }

    m_Handler.reset(new SHandler);
    GetDiagContext().SetOldPostFormat(false);
    SetDiagFilter(eDiagFilter_All, m_Filter.c_str());
    SetDiagHandler(m_Handler.get(), false);

    // Swapping stdout and stderr, so latency is reported to stdout and output of measured code to stderr
    auto saved_stderr = NcbiSys_dup(NcbiSys_fileno(stderr));
    _ASSERT(saved_stderr >= 0);
    _VERIFY(NcbiSys_dup2(NcbiSys_fileno(stdout), NcbiSys_fileno(stderr)) >= 0);
    _VERIFY(NcbiSys_dup2(saved_stderr, NcbiSys_fileno(stdout)) >= 0);
    _VERIFY(NcbiSys_close(saved_stderr) >= 0);
}

NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ctr_drbg_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ctr_drbg_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ctr_drbg_random);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ctr_drbg_seed);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_entropy_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_entropy_func);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_entropy_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_pk_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_pk_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_pk_parse_key);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_close_notify);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_conf_alpn_protocols);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_conf_authmode);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_conf_own_cert);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_conf_rng);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_config_defaults);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_config_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_config_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_get_verify_result);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_handshake);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_read);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_session_reset);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_set_bio);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_set_hostname);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_setup);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_ssl_write);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_strerror);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_x509_crt_free);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_x509_crt_init);
NCBI_EXPORT_FUNC_DEFINE(XCONNECT, mbedtls_x509_crt_parse);

END_NCBI_SCOPE
