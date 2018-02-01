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

#include <objtools/pubseq_gateway/diag/AppPerf.hpp>
#include <objtools/pubseq_gateway/cassandra/CassFactory.hpp>
#include <objtools/pubseq_gateway/cassandra/LbsmResolver.hpp>
#include <objtools/pubseq_gateway/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;
using namespace IdLogUtil;

/* CCassConnectionFactory */

CCassConnectionFactory::~CCassConnectionFactory() {
    CCassConnection::UpdateLogging(true);
}

void CCassConnectionFactory::AppInit(CArgDescriptions* argdesc) {
    argdesc->AddOptionalKey( "BDS", "bigdata_host", "BigData connection host (comma delimited list of database nodes)", CArgDescriptions::eString);
    argdesc->AddOptionalKey( "BDN", "bigdata_namespace", "BigData namespace name", CArgDescriptions::eString);
    argdesc->AddOptionalKey( "bpf", "bigdata_password_file", "Bigdata password file name", CArgDescriptions::eString);
    argdesc->AddOptionalKey( "bps", "bigdata_password_section", "Bigdata password file section", CArgDescriptions::eString);
    argdesc->AddOptionalKey( "BDCT", "bigdata_ctimeout", "Bigdata connection timeout, ms", CArgDescriptions::eInteger);
    argdesc->AddOptionalKey( "BDQT", "bigdata_qtimeout", "Bigdata query timeout, ms", CArgDescriptions::eInteger);

    argdesc->AddOptionalKey( "BDFBRDC", "bigdata_fallback_readconsistency", "Lower down consistency of BD read operations if local quorum can't be achieved", CArgDescriptions::eInteger);
    argdesc->AddOptionalKey( "BDFBWRC", "bigdata_fallback_writeconsistency", "Lower down consistency of BD write operations if local quorum can't be achieved", CArgDescriptions::eInteger);

    argdesc->AddOptionalKey( "dl", "dblog", "Output db-related messages to log", CArgDescriptions::eString);

    argdesc->AddOptionalKey( "lb", "loadbalancing", "Load ballancing policy. DCAware is default.", CArgDescriptions::eString);
    argdesc->SetConstraint
            ("lb",
            &(*new CArgAllow_Strings, "", "DCAware", "RoundRobin"),
            CArgDescriptions::eConstraint);
    argdesc->AddOptionalKey( "rta", "tokenaware", "Enables TokenAware routing. True is default.", CArgDescriptions::eString);
    argdesc->AddOptionalKey( "rla", "latencyware", "Enables LatencyAware routing. False is default.", CArgDescriptions::eString);
    argdesc->SetConstraint
            ("rta",
            &(*new CArgAllow_Strings, "", "True", "False"),
            CArgDescriptions::eConstraint);

    argdesc->AddOptionalKey( "nti", "numthreadio", "Number of io threads to async processing.", CArgDescriptions::eInteger);
    argdesc->SetConstraint( "nti", &(*new CArgAllow_Integers(1,32)), CArgDescriptions::eConstraint);

    argdesc->AddOptionalKey( "ncon", "numconnperhost", "Number of connections per node.", CArgDescriptions::eInteger);
    argdesc->SetConstraint( "ncon", &(*new CArgAllow_Integers(1,8)), CArgDescriptions::eConstraint);

    argdesc->AddOptionalKey( "mcon", "maxconnperhost", "Maximum count of connections per node.", CArgDescriptions::eInteger);
    argdesc->SetConstraint( "mcon", &(*new CArgAllow_Integers(1,8)), CArgDescriptions::eConstraint);
}

void CCassConnectionFactory::AppParseArgs(const CArgs& args) {
    if (args["dl"])
        m_BigDataLog = args["dl"].AsString();

    if (args["BDS"])
        m_BigDataHost = args["BDS"].AsString();
    if (args["BDN"])
        m_BigDataNameSpace = args["BDN"].AsString();
    if (args["bpf"])
        m_PassFile = args["bpf"].AsString();
    if (args["bps"])
        m_PassSection = args["bps"].AsString();

    if (args["BDFBRDC"])
        m_BigDataFallBackRdConsistency = args["BDFBRDC"].AsBoolean();

    if (args["BDCT"])
        m_BigDataCTimeoutMS = args["BDCT"].AsInteger();
    if (args["BDQT"])
        m_BigDataQTimeoutMS = args["BDQT"].AsInteger();

    if (args["lb"])
        m_LoadBalancingStr = args["lb"].AsString();
    if (args["rta"])
        m_tokenAwareStr = args["rta"].AsString();
    if (args["rla"])
        m_latencyAwareStr = args["rla"].AsString();
    if (args["nti"])
        m_numThreadsIo = args["nti"].AsInteger();
    if (args["ncon"])
        m_numConnPerHost = args["ncon"].AsInteger();
    if (args["mcon"])
        m_maxConnPerHost = args["mcon"].AsInteger();

    ProcessParams();
}

void CCassConnectionFactory::ProcessParams() {
    LOG5(("CCassDataConnectionFactory::ProcessParams"));
    if (!m_PassFile.empty()) {
        filebuf fb;
        if (!fb.open(m_PassFile.c_str(), ios::in | ios::binary))
            RAISE_ERROR(eGeneric, string(" Cannot open file: ") + m_PassFile);
        CNcbiIstream is( &fb);
        CNcbiRegistry Registry( is, 0);
        fb.close();

        m_BigDataUser = Registry.GetString(m_PassSection, "user", "");
        m_BigDataPassword = Registry.GetString(m_PassSection, "password", "");
    }

    //RoundRobin,TokenAware,TokenAwareRoundRobin,None
    map<string, loadbalancing_policy_t> policy_arg_map = {
        {"", LB_DCAWARE},
        {"dcaware", LB_DCAWARE},
        {"roundrobin", LB_ROUNDROBIN}
    };
    std::transform(m_LoadBalancingStr.begin(), m_LoadBalancingStr.end(), m_LoadBalancingStr.begin(), ::tolower);
    auto policy_item = policy_arg_map.find(m_LoadBalancingStr);
    if (policy_item != policy_arg_map.end()) {
        m_LoadBalancing = policy_item->second;
    } else {
        NCBI_THROW_FMT(CArgException, CArgException::eConstraint, "Unexpected load balancing policy value!");
    }

    //True, False, None
    map<string, bool> boolean_arg_map = { {"", true}, {"true", true}, {"false", false} };

    std::transform(m_tokenAwareStr.begin(), m_tokenAwareStr.end(), m_tokenAwareStr.begin(), ::tolower);
    auto tokenaware_item = boolean_arg_map.find(m_tokenAwareStr);
    if (tokenaware_item != boolean_arg_map.end()) {
        m_tokenAware = tokenaware_item->second;
    } else {
        NCBI_THROW_FMT(CArgException, CArgException::eConstraint, "Unexpected tokenaware value!");
    }

    std::transform(m_latencyAwareStr.begin(), m_latencyAwareStr.end(), m_latencyAwareStr.begin(), ::tolower);
    auto latencyaware_item = boolean_arg_map.find(m_latencyAwareStr);
    if (latencyaware_item != boolean_arg_map.end()) {
        m_latencyAware = latencyaware_item->second;
    } else {
        NCBI_THROW_FMT(CArgException, CArgException::eConstraint, "Unexpected latencyaware value!");
    }
}


void CCassConnectionFactory::LoadConfig(const string &  CfgName, const string &  section) {
    LOG5(("CCassDataConnectionFactory::LoadConfig"));
    m_Section = section;
    m_CfgName = CfgName;
    ReloadConfig();
}

void CCassConnectionFactory::LoadConfig(const CNcbiRegistry &Registry, const string &  section) {
    LOG5(("CCassDataConnectionFactory::LoadConfig"));
    m_Section = section;
    m_CfgName = "";
    ReloadConfig(Registry);
}

void CCassConnectionFactory::ReloadConfig() {
    LOG5(("CCassDataConnectionFactory::ReloadConfig"));
    CFastMutexGuard _(m_RunTimeParams);

    if (m_Section.empty())
        m_Section = "BIGDATA";
    if (m_CfgName.empty())
        RAISE_ERROR(eGeneric, string("Configuration file is not specified"));

    filebuf fb;
    fb.open(m_CfgName.c_str(), ios::in | ios::binary);
    CNcbiIstream is(&fb);
    CNcbiRegistry Registry(is, 0);
    fb.close();

    ReloadConfig(Registry);
}

void CCassConnectionFactory::ReloadConfig(const CNcbiRegistry &Registry) {
    if (!Registry.Empty()) {
        m_BigDataHost           = Registry.GetString( m_Section, "HOST", "");
        m_BigDataPort           = Registry.GetInt(    m_Section, "PORT", 0);
        m_BigDataNameSpace      = Registry.GetString( m_Section, "NAMESPACE", "");
        m_PassFile              = Registry.GetString( m_Section, "PASSWORD_FILE", "");
        m_PassSection           = Registry.GetString( m_Section, "PASSWORD_SECTION", "");
        m_BigDataCTimeoutMS     = Registry.GetInt(    m_Section, "CTIMEOUT", DFLT_C_TIMEOUT_MS);
        m_BigDataQTimeoutMS     = Registry.GetInt(    m_Section, "QTIMEOUT", DFLT_Q_TIMEOUT_MS);

        m_BigDataFallBackRdConsistency = Registry.GetBool(   m_Section, "FBRDC", false);

        m_LoadBalancingStr      = Registry.GetString( m_Section, "LOADBALANCING", "");
        m_tokenAwareStr         = Registry.GetString( m_Section, "TOKENAWARE", "");
        m_latencyAwareStr       = Registry.GetString( m_Section, "LATENCYAWARE", "");
        m_numThreadsIo          = Registry.GetInt(    m_Section, "NUMTHREADSIO", 0);
        m_numConnPerHost        = Registry.GetInt(    m_Section, "NUMCONNPERHOST", 0);
        m_maxConnPerHost        = Registry.GetInt(    m_Section, "MAXCONNPERHOST", 0);
        m_keepalive             = Registry.GetInt(    m_Section, "KEEPALIVE", 0);

        m_BigDataLog            = Registry.GetString( m_Section, "DRVLOG", "");
        ProcessParams();
    }
}

void CCassConnectionFactory::GetHostPort(string &host, short &port) {
    bool is_lbsm;

    host = m_BigDataHost;
    port = m_BigDataPort;
    is_lbsm = host.find(':') != string::npos && host.find("lbsm:") == 0;

    if (is_lbsm) {
        string cleanhost = host.substr(sizeof("lbsm:") - 1);
        host = LbsmLookup::Resolve(cleanhost, ',');
        if (host.empty())
            RAISE_ERROR(eGeneric, string("Failed to resolve: ") + cleanhost);
    }
    if (host.find(':') != string::npos || host.find(',') != string::npos) {
        string cleanedhost;
        stringstream sshosts(host);
        string item;
        while (getline(sshosts, item, ',')) {
            size_t pos = item.find(':');
            if (pos != string::npos) {
                int aport = atoi(item.substr(pos + 1).c_str());
                if (aport <= 0)
                    RAISE_ERROR(eGeneric, string("Invalid port found in host:port pair: " + item));
                item = item.substr(0, pos);
                if (port == 0)
                    port = aport;
                else if (port != aport)
                    RAISE_ERROR(eGeneric, string("Unexpected port found in host:port pair: " + item + ", expected: " + NStr::NumericToString(port) + ", found: " + NStr::NumericToString(aport)));
            }
            if (!cleanedhost.empty())
                cleanedhost.append(1, ',');
            cleanedhost.append(item);
        }
        host = cleanedhost;
    }
}


shared_ptr<CCassConnection> CCassConnectionFactory::CreateInstance() {
    CAppOp op;
    shared_ptr<CCassConnection> rv(new CCassConnection());

    rv->SetLoadBalancing(m_LoadBalancing);
    rv->SetTokenAware(m_tokenAware);
    rv->SetLatencyAware(m_latencyAware);
    rv->SetRtLimits(m_numThreadsIo, m_numConnPerHost, m_maxConnPerHost);
    rv->SetKeepAlive(m_keepalive);

    rv->SetTimeouts(m_BigDataCTimeoutMS, m_BigDataQTimeoutMS);
    rv->SetFallBackRdConsistency(m_BigDataFallBackRdConsistency);
    if (!m_BigDataLog.empty())
        rv->SetLogFile(m_BigDataLog);

    string host;
    short port;
    GetHostPort(host, port);
    rv->SetConnProp(host, m_BigDataUser, m_BigDataPassword, port);
    rv->SetKeyspace(m_BigDataNameSpace);
    return rv;
}

END_IDBLOB_SCOPE
