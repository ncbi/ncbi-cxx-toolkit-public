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
 */
#include <ncbi_pch.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <cstdlib>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/CassBlobOp.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/CassFactory.hpp>

#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/impl/rpc/HttpServerTransport.hpp>
#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

#include "AccVerCacheDB.hpp"
#include "pending_operation.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
using namespace IdLogUtil;

class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp() :
        m_LogLevel(0),
        m_LogLevelFile(1),
        m_HttpPort(2180),
        m_HttpWorkers(32),
        m_ListenerBacklog(256),
        m_TcpMaxConn(4096),
        m_CassConnectionFactory(CCassConnectionFactory::Create()),
        m_TimeoutMs(30000)
    {}

    virtual void Init()
    {
        unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

        m_CassConnectionFactory->AppInit(argdesc.get());

        argdesc->SetUsageContext(GetArguments().GetProgramBasename(),
                                 "Daemon to service Accession.Version Cache requests");
        argdesc->AddDefaultKey("P", "port", "HTTP port",
                               CArgDescriptions::eInteger, "2180");
        argdesc->SetConstraint("P",   new CArgAllow_Integers(1, 65534));

        argdesc->AddDefaultKey("H", "Address", "Address to bind listening socket to, 0.0.0.0 by default",  CArgDescriptions::eString, "0.0.0.0");
        argdesc->AddOptionalKey("o", "log",          "Output log to",                        CArgDescriptions::eString);
        argdesc->AddOptionalKey("l", "loglevel",     "Output verbosity level from 0 to 5",   CArgDescriptions::eInteger);
        argdesc->AddOptionalKey("lf", "loglevelfile", "File logging verbosity level from 0 to 5",   CArgDescriptions::eInteger);
        argdesc->AddOptionalKey("db", "database",     "Path to LMDB database",                CArgDescriptions::eString);
        argdesc->AddOptionalKey("w", "HttpWorkers",  "HTTP workers from 1 to 100",           CArgDescriptions::eInteger);
        argdesc->SetConstraint("w", new CArgAllow_Integers(1, 100));
        argdesc->AddOptionalKey("b", "backlog",      "Listener backlog from 5 to 2048",      CArgDescriptions::eInteger);
        argdesc->SetConstraint("b", new CArgAllow_Integers(5, 2048));
        argdesc->AddOptionalKey("n", "maxcon",       "Max number of connections 5 to 65000",      CArgDescriptions::eInteger);
        argdesc->SetConstraint("n", new CArgAllow_Integers(5, 65000));

        SetupArgDescriptions(argdesc.release());
    }

    void ParseArgs()
    {
        const CArgs &           args = GetArgs();
        const CNcbiRegistry &   Registry = GetConfig();

        if (!Registry.Empty() ) {
            m_LogLevel = Registry.GetInt("COMMON", "LOGLEVEL", 0);
            m_LogLevelFile = Registry.GetInt("COMMON", "LOGLEVELFILE", 1);
            m_LogFile = Registry.GetString("COMMON", "LOGFILE", "");
            m_HttpPort = Registry.GetInt("HTTP", "PORT", 2180);
            m_Address = Registry.GetString("HTTP", "Address", "");
            m_HttpWorkers = Registry.GetInt("HTTP", "Workers", 32);
            m_ListenerBacklog = Registry.GetInt("HTTP", "Backlog", 256);
            m_TcpMaxConn = Registry.GetInt("HTTP", "MaxConn", 4096);
            m_TimeoutMs = Registry.GetInt("COMMON", "OPTIMEOUT", 30000);
        }
        if (args["o"])
            m_LogFile = args["o"].AsString();
        if (args["l"])
            m_LogLevel = args["l"].AsInteger();
        if (args["lf"])
            m_LogLevelFile = args["lf"].AsInteger();
        IdLogUtil::CAppLog::SetLogLevel(m_LogLevel);
        IdLogUtil::CAppLog::SetLogLevelFile(m_LogLevelFile);
        if (!m_LogFile.empty())
            IdLogUtil::CAppLog::SetLogFile(m_LogFile);

        m_CassConnectionFactory->AppParseArgs(args);
        m_CassConnectionFactory->LoadConfig(GetConfigPath(), "");

        if (args["P"])
            m_HttpPort = args["P"].AsInteger();
        if (args["H"])
            m_Address = args["H"].AsString();
        if (args["w"])
            m_HttpWorkers = args["w"].AsInteger();
        if (args["b"])
            m_ListenerBacklog = args["b"].AsInteger();
        if (args["n"])
            m_TcpMaxConn = args["n"].AsInteger();
        if (args["db"])
            m_DbPath = args["db"].AsString();

        list<string> entries;
        Registry.EnumerateEntries("satnames", &entries);
        for (const auto& it : entries) {
            size_t idx = NStr::StringToNumeric<size_t>(it);
            while (m_SatNames.size() <= idx)
                m_SatNames.push_back("");
            m_SatNames[idx] = Registry.GetString("satnames", it, "");
        }
        if (m_SatNames.size() == 0)
            EAccVerException::raise("[satnames] section in ini file is empty");
    }

    void OpenDb(bool  Initialize, bool  Readonly)
    {
        if (m_DbPath.empty())
            m_DbPath = "./accvercache.db";
        m_Db.reset(new CAccVerCacheDB());
        m_Db->Open(m_DbPath, Initialize, Readonly);
    }

    void OpenCass()
    {
        m_CassConnection = m_CassConnectionFactory->CreateInstance();
        m_CassConnection->Connect();
    }

    void CloseCass()
    {
        m_CassConnection = nullptr;
        m_CassConnectionFactory = nullptr;
    }

    bool SatToSatName(size_t  sat, string &  sat_name)
    {
        if (sat < m_SatNames.size()) {
            sat_name = m_SatNames[sat];
            return !sat_name.empty();
        }
        return false;
    }

    template<typename P>
    int OnAccVerResolver(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp)
    {
        const char *    value;
        size_t          value_len;

        if (req.GetParam("accver", sizeof("accver") - 1,
                         true, &value, &value_len)) {
            string AccVer(value, value_len);

            string Data;
            if (m_Db->Storage().Get(AccVer, Data)) {
                resp.SendOk(Data.c_str(), Data.length(), false);
            } else {
                stringstream    ss;
                ss << "Entry (" << AccVer << ") not found";
                resp.Send404("notfound", ss.str().c_str());
            }
        } else {
            resp.Send503("invalid", "invalid request");
        }
        return 0;
    }

    template<typename P>
    int OnGetBlob(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp)
    {
        const char *    ssat;
        const char *    ssat_key;
        size_t          sat_len;
        size_t          sat_key_len;

        if (req.GetParam("sat", sizeof("sat") - 1, true, &ssat, &sat_len) &&
            req.GetParam("sat_key", sizeof("sat_key") - 1, true, &ssat_key, &sat_key_len))
        {
            int sat = atoi(ssat);
            string sat_name;
            if (SatToSatName(sat, sat_name)) {
                int sat_key = atoi(ssat_key);
                resp.Postpone(CPendingOperation(std::move(sat_name), sat_key,
                                                m_CassConnection, m_TimeoutMs));
                return 0;
            } else {
                resp.Send404("not found",
                             "invalid/unsupported satellite number");
            }
        } else {
            resp.Send503("invalid", "invalid request");
        }
        return 0;
    }

    virtual int Run(void)
    {
        srand(time(NULL));
        ParseArgs();
        OpenDb(false, true);
        OpenCass();

        vector<HST::CHttpHandler<CPendingOperation>>    h;
        HST::CHttpGetParser                             get_parser;

        h.emplace_back("/ID/accver.resolver",
                       [this](HST::CHttpRequest &  req,
                              HST::CHttpReply<CPendingOperation> &resp)->int
                       {
                            return OnAccVerResolver(req, resp);
                       }, &get_parser, nullptr);
        h.emplace_back("/ID/getblob",
                       [this](HST::CHttpRequest &  req,
                              HST::CHttpReply<CPendingOperation> &resp)->int
                       {
                            return OnGetBlob(req, resp);
                       }, &get_parser, nullptr);

        unique_ptr<HST::CHttpDaemon<CPendingOperation>>     d(
                new HST::CHttpDaemon<CPendingOperation>(h, m_Address,
                                                        m_HttpPort,
                                                        m_HttpWorkers,
                                                        m_ListenerBacklog,
                                                        m_TcpMaxConn));
        d->Run([this](TSL::CTcpDaemon<HST::CHttpProto<CPendingOperation>,
                      HST::CHttpConnection<CPendingOperation>,
                      HST::CHttpDaemon<CPendingOperation>>& tcp_daemon)
                {
                    printf("%ld %d %ld\n", tcp_daemon.NumOfRequests(),
                                           tcp_daemon.NumOfConnections(),
                                           m_CassConnection->GetActiveStatements());
                });
        CloseCass();
        return 0;
    }

private:
    string                              m_DbPath;
    string                              m_LogFile;
    unsigned int                        m_LogLevel;
    unsigned int                        m_LogLevelFile;
    string                              m_Address;
    vector<string>                      m_SatNames;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    unique_ptr<CAccVerCacheDB>          m_Db;
    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;
};



int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();
    CAppLog::FormatTimeStamp(true);
    return CPubseqGatewayApp().AppMain(argc, argv);
}
