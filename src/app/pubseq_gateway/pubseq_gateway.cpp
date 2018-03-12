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
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

#include "acc_ver_cache_db.hpp"
#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "pubseq_gateway_exception.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
using namespace IdLogUtil;


const unsigned short    kWorkersMin = 1;
const unsigned short    kWorkersMax = 100;
const unsigned short    kWorkersDefault = 32;
const unsigned short    kHttpPortMin = 1;
const unsigned short    kHttpPortMax = 65534;
const unsigned int      kListenerBacklogMin = 5;
const unsigned int      kListenerBacklogMax = 2048;
const unsigned int      kListenerBacklogDefault = 256;
const unsigned short    kTcpMaxConnMax = 65000;
const unsigned short    kTcpMaxConnMin = 5;
const unsigned short    kTcpMaxConnDefault = 4096;
const unsigned int      kTimeoutMsMin = 0;
const unsigned int      kTimeoutMsMax = UINT_MAX;
const unsigned int      kTimeoutDefault = 30000;

static const string     kNodaemonArgName = "nodaemon";


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp() :
        m_LogLevel(0),
        m_LogLevelFile(1),
        m_HttpPort(0),
        m_HttpWorkers(kWorkersDefault),
        m_ListenerBacklog(kListenerBacklogDefault),
        m_TcpMaxConn(kTcpMaxConnDefault),
        m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
        m_TimeoutMs(kTimeoutDefault),
        m_CountFile(NULL)
    {}

    ~CPubseqGatewayApp()
    {
        if (m_CountFile != NULL)
            fclose(m_CountFile);
    }

    virtual void Init(void)
    {
        unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

        argdesc->AddFlag(kNodaemonArgName,
                         "Turn off daemonization of NetSchedule at the start.");

        m_CassConnectionFactory->AppInit(argdesc.get());
        argdesc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "Daemon to service Accession.Version Cache requests");
        SetupArgDescriptions(argdesc.release());
    }

    void ParseArgs(void)
    {
        const CArgs &           args = GetArgs();
        const CNcbiRegistry &   registry = GetConfig();

        if (!registry.HasEntry("SERVER", "port"))
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[SERVER]/port value is not fond in the configuration "
                       "file. The port must be provided to run the server. "
                       "Exiting.");

        m_DbPath = registry.GetString("LMDB_CACHE", "dbfile", "");
        m_HttpPort = registry.GetInt("SERVER", "port", 0);
        m_HttpWorkers = registry.GetInt("SERVER", "workers",
                                        kWorkersDefault);
        m_ListenerBacklog = registry.GetInt("SERVER", "backlog",
                                            kListenerBacklogDefault);
        m_TcpMaxConn = registry.GetInt("SERVER", "maxconn",
                                       kTcpMaxConnDefault);
        m_TimeoutMs = registry.GetInt("SERVER", "optimeout",
                                      kTimeoutDefault);
        m_CountFileName = registry.GetString("SERVER", "countfile", "");

        m_LogLevel = registry.GetInt("COMMON", "loglevel", 0);
        m_LogLevelFile = registry.GetInt("COMMON", "loglevelfile", 1);
        m_LogFile = registry.GetString("COMMON", "logfile", "");


        IdLogUtil::CAppLog::SetLogLevel(m_LogLevel);
        IdLogUtil::CAppLog::SetLogLevelFile(m_LogLevelFile);
        if (!m_LogFile.empty())
            IdLogUtil::CAppLog::SetLogFile(m_LogFile);

        m_CassConnectionFactory->AppParseArgs(args);
        m_CassConnectionFactory->LoadConfig(registry, "");

        list<string>    entries;
        registry.EnumerateEntries("satnames", &entries);
        for (const auto &  it : entries) {
            size_t      idx = NStr::StringToNumeric<size_t>(it);
            while (m_SatNames.size() <= idx)
                m_SatNames.push_back("");
            m_SatNames[idx] = registry.GetString("satnames", it, "");
        }

        // It throws an exception in case of inability to start
        x_ValidateArgs();
    }

    void OpenDb(bool  initialize, bool  readonly)
    {
        m_Db.reset(new CAccVerCacheDB());
        m_Db->Open(m_DbPath, initialize, readonly);
    }

    void OpenCass()
    {
        m_CassConnection = m_CassConnectionFactory->CreateInstance();
        m_CassConnection->Connect();
    }

    void CloseCass(void)
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
        const char *    accession;
        size_t          accession_len;

        if (req.GetParam("accession", sizeof("accession") - 1,
                         true, &accession, &accession_len)) {
            string          resolution_data;
            if (x_Resolve(resp, accession, accession_len, resolution_data))
                resp.SendOk(resolution_data.c_str(), resolution_data.length(),
                            false);
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
        const char *    accession;
        size_t          sat_len;
        size_t          sat_key_len;
        size_t          accession_len;

        if (req.GetParam("sat", sizeof("sat") - 1, true, &ssat, &sat_len) &&
            req.GetParam("sat_key", sizeof("sat_key") - 1, true,
                         &ssat_key, &sat_key_len))
        {
            int         sat = atoi(ssat);
            string      sat_name;

            if (SatToSatName(sat, sat_name)) {
                int         sat_key = atoi(ssat_key);
                resp.Postpone(CPendingOperation(std::move(sat_name), sat_key,
                                                m_CassConnection, m_TimeoutMs));
                return 0;
            }

            resp.Send404("not found",
                         "invalid/unsupported satellite number");
        } else if (req.GetParam("accession", sizeof("accession") - 1,
                   true, &accession, &accession_len)) {
            string          resolution_data;
            if (!x_Resolve(resp, accession, accession_len, resolution_data))
                return 0;   // resolution failed

            int             sat;
            int             sat_key;
            if (!x_UnpackAccessionData(resp, resolution_data, sat, sat_key))
                return 0;   // unpacking failed

            string      sat_name;
            if (SatToSatName(sat, sat_name)) {
                resp.Postpone(CPendingOperation(resolution_data,
                                                std::move(sat_name), sat_key,
                                                m_CassConnection, m_TimeoutMs));
                return 0;
            }

            resp.Send404("not found",
                         "invalid/unsupported satellite number");
        } else {
            resp.Send503("invalid", "invalid request");
        }
        return 0;
    }

    virtual int Run(void)
    {
        srand(time(NULL));
        ParseArgs();

        if (!GetArgs()[kNodaemonArgName]) {
            bool    is_good = CProcess::Daemonize(kEmptyCStr,
                                                  CProcess::fDontChroot);
            if (!is_good)
                NCBI_THROW(CPubseqGatewayException, eDaemonizationFailed,
                           "Error during daemonization");
        }

        OpenDb(false, true);
        OpenCass();

        if (!m_CountFileName.empty()) {
            m_CountFile = fopen(m_CountFileName.c_str(), "a");
            if (m_CountFile == NULL)
                LOG1(("Error opening the file to store counters "
                      "specified in [SERVER]/countfile"));
        }

        vector<HST::CHttpHandler<CPendingOperation>>    http_handler;
        HST::CHttpGetParser                             get_parser;

        http_handler.emplace_back(
                "/ID/resolve",
                [this](HST::CHttpRequest &  req,
                       HST::CHttpReply<CPendingOperation> &  resp)->int
                {
                    return OnAccVerResolver(req, resp);
                }, &get_parser, nullptr);
        http_handler.emplace_back(
                "/ID/getblob",
                [this](HST::CHttpRequest &  req,
                       HST::CHttpReply<CPendingOperation> &  resp)->int
                {
                    return OnGetBlob(req, resp);
                }, &get_parser, nullptr);


        unique_ptr<HST::CHttpDaemon<CPendingOperation>>     daemon(
                new HST::CHttpDaemon<CPendingOperation>(http_handler, "0.0.0.0",
                                                        m_HttpPort,
                                                        m_HttpWorkers,
                                                        m_ListenerBacklog,
                                                        m_TcpMaxConn));
        daemon->Run([this](TSL::CTcpDaemon<HST::CHttpProto<CPendingOperation>,
                           HST::CHttpConnection<CPendingOperation>,
                           HST::CHttpDaemon<CPendingOperation>> &  tcp_daemon)
                {
                    static bool     is_daemon = !GetArgs()[kNodaemonArgName];
                    uint64_t        num_of_requests = tcp_daemon.NumOfRequests();
                    uint16_t        num_of_conn = tcp_daemon.NumOfConnections();
                    int64_t         num_of_active_stmt = m_CassConnection->GetActiveStatements();

                    if (m_CountFile != NULL) {
                        fprintf(m_CountFile, "%ld %d %ld\n",
                                num_of_requests, num_of_conn,
                                num_of_active_stmt);
                        fflush(m_CountFile);
                    } else if (!is_daemon) {
                        printf("%ld %d %ld\n",
                               tcp_daemon.NumOfRequests(),
                               tcp_daemon.NumOfConnections(),
                               m_CassConnection->GetActiveStatements());
                    }
                });
        CloseCass();
        return 0;
    }

private:
    // Resolves an accession. If failed then sends a message to the client.
    // Returns true if succeeded (acc_bin_data is populated as well)
    //         false if failed (404 response is sent)
    template<typename P>
    bool  x_Resolve(HST::CHttpReply<P> &  resp,
                    const char *  accession, size_t  accession_length,
                    string &  acc_bin_data)
    {
        string      acc_str(accession, accession_length);
        if (m_Db->Storage().Get(acc_str, acc_bin_data))
            return true;

        string      msg = "Entry (" + acc_str + ") not found";
        resp.Send404("notfound", msg.c_str());
        return false;
    }


    // Unpacks the accession binary data and picks sat and sat_key.
    // Returns true on success.
    //         false if there was a problem in unpacking the data
    template<typename P>
    bool  x_UnpackAccessionData(HST::CHttpReply<P> &  resp,
                                const string &  resolution_data,
                                int &  sat,
                                int &  sat_key)
    {
        DDRPC::DataRow      rec;
        try {
            DDRPC::AccVerResolverUnpackData(rec, resolution_data);
            sat = rec[2].AsUint1;
            sat_key = rec[3].AsUint4;
        } catch (const DDRPC::EDdRpcException &  e) {
            resp.Send503("invalid", "accession data unpacking error");

            string  msg = string("accession data unpacking error: ") +
                          e.what();
            ERRLOG1((msg.c_str()));
            return false;
        } catch (const exception &  e) {
            resp.Send503("invalid", "accession data unpacking error");

            string  msg = string("accession data unpacking error: ") +
                          e.what();
            ERRLOG1((msg.c_str()));
            return false;
        } catch (...) {
            resp.Send503("invalid", "unknown accession data unpacking error");

            ERRLOG1(("Unknown accession data unpacking error"));
            return false;
        }
        return true;
    }


private:
    void x_ValidateArgs(void)
    {
        if (m_SatNames.size() == 0)
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[satnames] section in ini file is empty");

        if (m_DbPath.empty())
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[LMDB_CACHE]/dbfile is not found in the ini file");

        if (m_HttpPort < kHttpPortMin || m_HttpPort > kHttpPortMax) {
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[SERVER]/port value is out of range. Allowed range: " +
                       NStr::NumericToString(kHttpPortMin) + "..." +
                       NStr::NumericToString(kHttpPortMax) + ". Received: " +
                       NStr::NumericToString(m_HttpPort));
        }

        if (m_HttpWorkers < kWorkersMin || m_HttpWorkers > kWorkersMax) {
            string  err_msg =
                "The number of HTTP workers is out of range. Allowed "
                "range: " + NStr::NumericToString(kWorkersMin) + "..." +
                NStr::NumericToString(kWorkersMax) + ". Received: " +
                NStr::NumericToString(m_HttpWorkers) + ". Reset to "
                "default: " + NStr::NumericToString(kWorkersDefault);
            LOG1((err_msg.c_str()));
            m_HttpWorkers = kWorkersDefault;
        }

        if (m_ListenerBacklog < kListenerBacklogMin ||
            m_ListenerBacklog > kListenerBacklogMax) {
            string  err_msg =
                "The listener backlog is out of range. Allowed "
                "range: " + NStr::NumericToString(kListenerBacklogMin) + "..." +
                NStr::NumericToString(kListenerBacklogMax) + ". Received: " +
                NStr::NumericToString(m_ListenerBacklog) + ". Reset to "
                "default: " + NStr::NumericToString(kListenerBacklogDefault);
            LOG1((err_msg.c_str()));
            m_ListenerBacklog = kListenerBacklogDefault;
        }

        if (m_TcpMaxConn < kTcpMaxConnMin || m_TcpMaxConn > kTcpMaxConnMax) {
            string  err_msg =
                "The max number of connections is out of range. Allowed "
                "range: " + NStr::NumericToString(kTcpMaxConnMin) + "..." +
                NStr::NumericToString(kTcpMaxConnMax) + ". Received: " +
                NStr::NumericToString(m_TcpMaxConn) + ". Reset to "
                "default: " + NStr::NumericToString(kTcpMaxConnDefault);
            LOG1((err_msg.c_str()));
            m_TcpMaxConn = kTcpMaxConnDefault;
        }

        if (m_TimeoutMs < kTimeoutMsMin || m_TimeoutMs > kTimeoutMsMax) {
            string  err_msg =
                "The operation timeout is out of range. Allowed "
                "range: " + NStr::NumericToString(kTimeoutMsMin) + "..." +
                NStr::NumericToString(kTimeoutMsMax) + ". Received: " +
                NStr::NumericToString(m_TimeoutMs) + ". Reset to "
                "default: " + NStr::NumericToString(kTimeoutDefault);
            LOG1((err_msg.c_str()));
            m_TimeoutMs = kTimeoutDefault;
        }
    }

private:
    string                              m_DbPath;
    string                              m_LogFile;
    unsigned int                        m_LogLevel;
    unsigned int                        m_LogLevelFile;
    vector<string>                      m_SatNames;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    unique_ptr<CAccVerCacheDB>          m_Db;
    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;

    string                              m_CountFileName;
    FILE *                              m_CountFile;
};



int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();
    CAppLog::FormatTimeStamp(true);
    return CPubseqGatewayApp().AppMain(argc, argv);
}
