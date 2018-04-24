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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Casssandra connection factory class
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/lbsm_resolver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;
using namespace IdLogUtil;


const string                    kCassConfigSection = "CASSANDRA_DB";

const unsigned int              kCassConnTimeoutDefault = 30000;
const unsigned int              kCassConnTimeoutMin = 0;
const unsigned int              kCassConnTimeoutMax = UINT_MAX;
const unsigned int              kCassQueryTimeoutDefault = 5000;
const unsigned int              kCassQueryTimeoutMin = 0;
const unsigned int              kCassQueryTimeoutMax = UINT_MAX;
const loadbalancing_policy_t    kLoadBalancingDefaultPolicy = LB_DCAWARE;
const unsigned int              kNumThreadsIoMin = 1;
const unsigned int              kNumThreadsIoMax = 32;
const unsigned int              kNumThreadsIoDefault = 4;
const unsigned int              kNumConnPerHostMin = 1;
const unsigned int              kNumConnPerHostMax = 8;
const unsigned int              kNumConnPerHostDefault = 2;
const unsigned int              kMaxConnPerHostMin = 1;
const unsigned int              kMaxConnPerHostMax = 8;
const unsigned int              kMaxConnPerHostDefault = 4;
const unsigned int              kKeepaliveMin = 0;
const unsigned int              kKeepaliveMax = UINT_MAX;
const unsigned int              kKeepaliveDefault = 0;
const unsigned int              kCassFallbackWrConsistencyMin = 0;
const unsigned int              kCassFallbackWrConsistencyMax = UINT_MAX;
const unsigned int              kCassFallbackWrConsistencyDefault = 0;


const map<string, loadbalancing_policy_t>     kPolicyArgMap = {
    {"", kLoadBalancingDefaultPolicy},
    {"dcaware", LB_DCAWARE},
    {"roundrobin", LB_ROUNDROBIN}
};


/* CCassConnectionFactory */
CCassConnectionFactory::CCassConnectionFactory() :
    m_CassConnTimeoutMs(kCassConnTimeoutDefault),
    m_CassQueryTimeoutMs(kCassQueryTimeoutDefault),
    m_CassFallbackRdConsistency(false),
    m_CassFallbackWrConsistency(kCassFallbackWrConsistencyDefault),
    m_LoadBalancing(kLoadBalancingDefaultPolicy),
    m_TokenAware(true),
    m_LatencyAware(true),
    m_NumThreadsIo(kNumThreadsIoDefault),
    m_NumConnPerHost(kNumConnPerHostDefault),
    m_MaxConnPerHost(kMaxConnPerHostDefault),
    m_Keepalive(kKeepaliveDefault)
{}


CCassConnectionFactory::~CCassConnectionFactory()
{
    CCassConnection::UpdateLogging(true);
}


void CCassConnectionFactory::AppInit(CArgDescriptions *  argdesc)
{
}


void CCassConnectionFactory::AppParseArgs(const CArgs &  args)
{
    ProcessParams();
}


void CCassConnectionFactory::ProcessParams(void)
{
    LOG5(("CCassDataConnectionFactory::ProcessParams"));
    if (!m_PassFile.empty()) {
        filebuf     fb;
        if (!fb.open(m_PassFile.c_str(), ios::in | ios::binary))
            RAISE_ERROR(eGeneric, string(" Cannot open file: ") + m_PassFile);

        CNcbiIstream        is(&fb);
        CNcbiRegistry       registry(is, 0);
        fb.close();

        m_CassUserName = registry.GetString(m_PassSection, "user", "");
        m_CassPassword = registry.GetString(m_PassSection, "password", "");
    }

    x_ValidateArgs();
}


void CCassConnectionFactory::LoadConfig(const string &  cfg_name,
                                        const string &  section)
{
    LOG5(("CCassDataConnectionFactory::LoadConfig"));
    m_Section = section;
    m_CfgName = cfg_name;
    ReloadConfig();
}


void CCassConnectionFactory::LoadConfig(const CNcbiRegistry &  registry,
                                        const string &  section)
{
    LOG5(("CCassDataConnectionFactory::LoadConfig"));
    m_Section = section;
    m_CfgName = "";
    ReloadConfig(registry);
}


void CCassConnectionFactory::ReloadConfig(void)
{
    LOG5(("CCassDataConnectionFactory::ReloadConfig"));
    CFastMutexGuard _(m_RunTimeParams);

    if (m_CfgName.empty())
        RAISE_ERROR(eGeneric, string("Configuration file is not specified"));

    filebuf fb;
    fb.open(m_CfgName.c_str(), ios::in | ios::binary);
    CNcbiIstream is(&fb);
    CNcbiRegistry Registry(is, 0);
    fb.close();

    ReloadConfig(Registry);
}


void CCassConnectionFactory::ReloadConfig(const CNcbiRegistry &  registry)
{
    if (m_Section.empty())
        m_Section = kCassConfigSection;

    if (!registry.Empty()) {
        m_CassConnTimeoutMs = registry.GetInt(m_Section, "ctimeout",
                                              kCassConnTimeoutDefault);
        m_CassQueryTimeoutMs = registry.GetInt(m_Section, "qtimeout",
                                               kCassQueryTimeoutDefault);
        m_CassDataNamespace = registry.GetString(m_Section, "namespace", "");
        m_CassFallbackRdConsistency = registry.GetBool(
            m_Section, "fallbackrdconsistency", false);
        m_CassFallbackWrConsistency = registry.GetInt(
            m_Section, "fallbackwriteconsistency",
            kCassFallbackWrConsistencyDefault);
        m_LoadBalancingStr = registry.GetString(m_Section, "loadbalancing", "");
        m_TokenAware = registry.GetBool(m_Section, "tokenaware", true);
        m_LatencyAware = registry.GetBool(m_Section, "latencyaware", true);
        m_NumThreadsIo = registry.GetInt(m_Section, "numthreadsio",
                                         kNumThreadsIoDefault);
        m_NumConnPerHost = registry.GetInt(m_Section, "numconnperhost",
                                           kNumConnPerHostDefault);
        m_MaxConnPerHost = registry.GetInt(m_Section, "maxconnperhost",
                                           kMaxConnPerHostDefault);
        m_Keepalive = registry.GetInt(m_Section, "keepalive",
                                      kKeepaliveDefault);
        m_CassDriverLogFile = registry.GetString(m_Section, "drvlog", "");
        m_PassFile = registry.GetString(m_Section, "password_file", "");
        m_PassSection = registry.GetString(m_Section, "password_section", "");
        m_CassHosts = registry.GetString(m_Section, "service", "");

        ProcessParams();
    }
}


void CCassConnectionFactory::GetHostPort(string &  cass_hosts,
                                         short &  cass_port)
{
    string  hosts = m_CassHosts;
    bool    is_lbsm = (m_CassHosts.find(':') == string::npos) &&
                      (m_CassHosts.find(' ') == string::npos) &&
                      (m_CassHosts.find(',') == string::npos);

    if (is_lbsm) {
        hosts = LbsmLookup::s_Resolve(m_CassHosts, ',');
        if (hosts.empty())
            RAISE_ERROR(eGeneric,
                        string("Failed to resolve: ") + m_CassHosts);
    }

    // Here: the 'hosts' variable has a list of host[:port] items came
    //       from a config file or from an LBSM resolver.
    vector<string>      items;
    NStr::Split(hosts, ", ", items,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    cass_port = 0;
    for (const auto &  item: items) {
        string      item_host;
        string      item_port;

        if (NStr::SplitInTwo(item, ":", item_host, item_port)) {
            // Delimiter was found, i.e. there is a port number
            short   item_port_number = NStr::StringToNumeric<short>(item_port);
            if (cass_port == 0) {
                cass_port = item_port_number;
            } else {
                if (item_port_number != cass_port)
                    RAISE_ERROR(eGeneric,
                                "Unmatching port numbers found: " +
                                NStr::NumericToString(cass_port) + " and " +
                                NStr::NumericToString(item_port_number));
            }
        }

        if (!cass_hosts.empty())
            cass_hosts += ",";
        cass_hosts += item_host;
    }
}


shared_ptr<CCassConnection> CCassConnectionFactory::CreateInstance(void)
{
    CAppOp                          op;
    shared_ptr<CCassConnection>     rv(new CCassConnection());

    rv->SetLoadBalancing(m_LoadBalancing);
    rv->SetTokenAware(m_TokenAware);
    rv->SetLatencyAware(m_LatencyAware);
    rv->SetRtLimits(m_NumThreadsIo, m_NumConnPerHost, m_MaxConnPerHost);
    rv->SetKeepAlive(m_Keepalive);

    rv->SetTimeouts(m_CassConnTimeoutMs, m_CassQueryTimeoutMs);
    rv->SetFallBackRdConsistency(m_CassFallbackRdConsistency);
    if (!m_CassDriverLogFile.empty())
        rv->SetLogFile(m_CassDriverLogFile);

    if (m_CassFallbackWrConsistency != 0)
        rv->SetFallBackWrConsistency(m_CassFallbackWrConsistency);

    string      host;
    short       port;
    GetHostPort(host, port);
    rv->SetConnProp(host, m_CassUserName, m_CassPassword, port);
    rv->SetKeyspace(m_CassDataNamespace);
    return rv;
}


void CCassConnectionFactory::x_ValidateArgs(void)
{
    if (m_CassConnTimeoutMs < kCassConnTimeoutMin ||
        m_CassConnTimeoutMs > kCassConnTimeoutMax) {
        string  err_msg =
            "The cassandra connection timeout is out of range. Allowed "
            "range: " + NStr::NumericToString(kCassConnTimeoutMin) + "..." +
            NStr::NumericToString(kCassConnTimeoutMax) + ". Received: " +
            NStr::NumericToString(m_CassConnTimeoutMs) + ". Reset to "
            "default: " + NStr::NumericToString(kCassConnTimeoutDefault);
        LOG1((err_msg.c_str()));
        m_CassConnTimeoutMs = kCassConnTimeoutDefault;
    }

    if (m_CassQueryTimeoutMs < kCassQueryTimeoutMin ||
        m_CassQueryTimeoutMs > kCassQueryTimeoutMax) {
        string  err_msg =
            "The cassandra query timeout is out of range. Allowed "
            "range: " + NStr::NumericToString(kCassQueryTimeoutMin) + "..." +
            NStr::NumericToString(kCassQueryTimeoutMax) + ". Received: " +
            NStr::NumericToString(m_CassQueryTimeoutMs) + ". Reset to "
            "default: " + NStr::NumericToString(kCassQueryTimeoutDefault);
        LOG1((err_msg.c_str()));
        m_CassQueryTimeoutMs = kCassQueryTimeoutDefault;
    }

    string      lowercase_policy = NStr::ToLower(m_LoadBalancingStr);
    auto        policy_item = kPolicyArgMap.find(lowercase_policy);
    if (policy_item != kPolicyArgMap.end()) {
        m_LoadBalancing = policy_item->second;
    } else {
        string      allowed;
        string      default_name;
        for (const auto &  item: kPolicyArgMap) {
            if (!item.first.empty()) {
                if (!allowed.empty())
                    allowed += ", ";
                allowed += item.second;
                if (item.second == kLoadBalancingDefaultPolicy)
                    default_name = item.first;
            }
        }
        string  err_msg =
            "The load balancing value is not recognized. Allowed values: " +
            allowed + ". Received: " + m_LoadBalancingStr + ". Reset to: " +
            default_name;
        m_LoadBalancing = LB_DCAWARE;
    }

    if (m_NumThreadsIo < kNumThreadsIoMin ||
        m_NumThreadsIo > kNumThreadsIoMax) {
        string  err_msg =
            "The number of IO threads is out of range. Allowed "
            "range: " + NStr::NumericToString(kNumThreadsIoMin) + "..." +
            NStr::NumericToString(kNumThreadsIoMax) + ". Received: " +
            NStr::NumericToString(m_NumThreadsIo) + ". Reset to "
            "default: " + NStr::NumericToString(kNumThreadsIoDefault);
        LOG1((err_msg.c_str()));
        m_NumThreadsIo = kNumThreadsIoDefault;
    }

    if (m_NumConnPerHost < kNumConnPerHostMin ||
        m_NumConnPerHost > kNumConnPerHostMax) {
        string  err_msg =
            "The number of connections per host is out of range. Allowed "
            "range: " + NStr::NumericToString(kNumConnPerHostMin) + "..." +
            NStr::NumericToString(kNumConnPerHostMax) + ". Received: " +
            NStr::NumericToString(m_NumConnPerHost) + ". Reset to "
            "default: " + NStr::NumericToString(kNumConnPerHostDefault);
        LOG1((err_msg.c_str()));
        m_NumConnPerHost = kNumConnPerHostDefault;
    }

    if (m_MaxConnPerHost < kMaxConnPerHostMin ||
        m_MaxConnPerHost > kMaxConnPerHostMax) {
        string  err_msg =
            "The maximum count of connections per host is out of range. Allowed "
            "range: " + NStr::NumericToString(kMaxConnPerHostMin) + "..." +
            NStr::NumericToString(kMaxConnPerHostMax) + ". Received: " +
            NStr::NumericToString(m_MaxConnPerHost) + ". Reset to "
            "default: " + NStr::NumericToString(kMaxConnPerHostDefault);
        LOG1((err_msg.c_str()));
        m_MaxConnPerHost = kMaxConnPerHostDefault;
    }

    if (m_Keepalive < kKeepaliveMin ||
        m_Keepalive > kKeepaliveMax) {
        string  err_msg =
            "The TCP keep-alive the initial delay is out of range. Allowed "
            "range: " + NStr::NumericToString(kKeepaliveMin) + "..." +
            NStr::NumericToString(kKeepaliveMax) + ". Received: " +
            NStr::NumericToString(m_Keepalive) + ". Reset to "
            "default: " + NStr::NumericToString(kKeepaliveDefault);
        LOG1((err_msg.c_str()));
        m_Keepalive = kKeepaliveDefault;
    }

    if (m_CassFallbackWrConsistency < kCassFallbackWrConsistencyMin ||
        m_CassFallbackWrConsistency > kCassFallbackWrConsistencyMax) {
        string  err_msg =
            "The cassandra write quorum is out of range. Allowed "
            "range: " + NStr::NumericToString(kCassFallbackWrConsistencyMin) + "..." +
            NStr::NumericToString(kCassFallbackWrConsistencyMax) + ". Received: " +
            NStr::NumericToString(m_CassFallbackWrConsistency) + ". Reset to "
            "default: " + NStr::NumericToString(kCassFallbackWrConsistencyDefault);
        LOG1((err_msg.c_str()));
        m_CassFallbackWrConsistency = kKeepaliveDefault;
    }
}

END_IDBLOB_SCOPE
