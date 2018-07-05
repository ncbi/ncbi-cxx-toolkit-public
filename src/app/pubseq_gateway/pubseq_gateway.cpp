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

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"


USING_NCBI_SCOPE;


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
const unsigned int      kMaxRetriesDefault = 1;
const unsigned int      kMaxRetriesMin = 0;
const unsigned int      kMaxRetriesMax = UINT_MAX;
const bool              kDefaultLog = true;

static const string     kNodaemonArgName = "nodaemon";


CPubseqGatewayApp *     CPubseqGatewayApp::sm_PubseqApp = nullptr;


CPubseqGatewayApp::CPubseqGatewayApp() :
    m_HttpPort(0),
    m_HttpWorkers(kWorkersDefault),
    m_ListenerBacklog(kListenerBacklogDefault),
    m_TcpMaxConn(kTcpMaxConnDefault),
    m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
    m_TimeoutMs(kTimeoutDefault),
    m_MaxRetries(kMaxRetriesDefault),
    m_StartTime(GetFastLocalTime()),
    m_Log(kDefaultLog)
{
    sm_PubseqApp = this;
}


CPubseqGatewayApp::~CPubseqGatewayApp()
{}


void CPubseqGatewayApp::Init(void)
{
    unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

    argdesc->AddFlag(kNodaemonArgName,
                     "Turn off daemonization of Pubseq Gateway at the start.");

    argdesc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Daemon to service Accession.Version Cache requests");
    SetupArgDescriptions(argdesc.release());
}


void CPubseqGatewayApp::ParseArgs(void)
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
    m_MaxRetries = registry.GetInt("SERVER", "maxretries",
                                   kMaxRetriesDefault);
    m_Log = registry.GetBool("SERVER", "log",
                             kDefaultLog);
    m_BioseqKeyspace = registry.GetString("SERVER", "bioseqkeyspace", "");

    m_CassConnectionFactory->AppParseArgs(args);
    m_CassConnectionFactory->LoadConfig(registry, "");
    m_CassConnectionFactory->SetLogging(GetDiagPostLevel());

    // It throws an exception in case of inability to start
    x_ValidateArgs();
}


void CPubseqGatewayApp::OpenDb(bool  initialize, bool  readonly)
{
//    m_Db.reset(new CAccVerCacheDB());
//    m_Db->Open(m_DbPath, initialize, readonly);
}


void CPubseqGatewayApp::OpenCass(void)
{
    m_CassConnection = m_CassConnectionFactory->CreateInstance();
    m_CassConnection->Connect();
}


void CPubseqGatewayApp::CloseCass(void)
{
    m_CassConnection = nullptr;
    m_CassConnectionFactory = nullptr;
}


bool CPubseqGatewayApp::SatToSatName(size_t  sat, string &  sat_name)
{
    if (sat < m_SatNames.size()) {
        sat_name = m_SatNames[sat];
        return !sat_name.empty();
    }
    return false;
}


int CPubseqGatewayApp::Run(void)
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
    int     ret = x_PopulateSatToKeyspaceMap();
    if (ret != 0)
        return ret;

    vector<HST::CHttpHandler<CPendingOperation>>    http_handler;
    HST::CHttpGetParser                             get_parser;

    http_handler.emplace_back(
            "/ID/getblob",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnGetBlob(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnGet(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/config",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnConfig(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/info",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnInfo(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/status",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnStatus(req, resp);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "",
            [this](HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp)->int
            {
                return OnBadURL(req, resp);
            }, &get_parser, nullptr);


    m_TcpDaemon.reset(
            new HST::CHttpDaemon<CPendingOperation>(http_handler, "0.0.0.0",
                                                    m_HttpPort,
                                                    m_HttpWorkers,
                                                    m_ListenerBacklog,
                                                    m_TcpMaxConn));


    m_TcpDaemon->Run([this](TSL::CTcpDaemon<HST::CHttpProto<CPendingOperation>,
                       HST::CHttpConnection<CPendingOperation>,
                       HST::CHttpDaemon<CPendingOperation>> &  tcp_daemon)
            {
                // This lambda is called once per second.
                // Earlier implementations printed counters on stdout.
            });
    CloseCass();
    return 0;
}



CPubseqGatewayApp *  CPubseqGatewayApp::GetInstance(void)
{
    return sm_PubseqApp;
}


CPubseqGatewayErrorCounters &  CPubseqGatewayApp::GetErrorCounters(void)
{
    return m_ErrorCounters;
}


CPubseqGatewayRequestCounters &  CPubseqGatewayApp::GetRequestCounters(void)
{
    return m_RequestCounters;
}


void CPubseqGatewayApp::x_ValidateArgs(void)
{
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

    if (m_BioseqKeyspace.empty()) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[SERVER]/bioseqkeyspace is not provided. It must "
                   "be supplied for the server to start");
    }

    if (m_HttpWorkers < kWorkersMin || m_HttpWorkers > kWorkersMax) {
        string  err_msg =
            "The number of HTTP workers is out of range. Allowed "
            "range: " + NStr::NumericToString(kWorkersMin) + "..." +
            NStr::NumericToString(kWorkersMax) + ". Received: " +
            NStr::NumericToString(m_HttpWorkers) + ". Reset to "
            "default: " + NStr::NumericToString(kWorkersDefault);
        ERR_POST(err_msg);
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
        ERR_POST(err_msg);
        m_ListenerBacklog = kListenerBacklogDefault;
    }

    if (m_TcpMaxConn < kTcpMaxConnMin || m_TcpMaxConn > kTcpMaxConnMax) {
        string  err_msg =
            "The max number of connections is out of range. Allowed "
            "range: " + NStr::NumericToString(kTcpMaxConnMin) + "..." +
            NStr::NumericToString(kTcpMaxConnMax) + ". Received: " +
            NStr::NumericToString(m_TcpMaxConn) + ". Reset to "
            "default: " + NStr::NumericToString(kTcpMaxConnDefault);
        ERR_POST(err_msg);
        m_TcpMaxConn = kTcpMaxConnDefault;
    }

    if (m_TimeoutMs < kTimeoutMsMin || m_TimeoutMs > kTimeoutMsMax) {
        string  err_msg =
            "The operation timeout is out of range. Allowed "
            "range: " + NStr::NumericToString(kTimeoutMsMin) + "..." +
            NStr::NumericToString(kTimeoutMsMax) + ". Received: " +
            NStr::NumericToString(m_TimeoutMs) + ". Reset to "
            "default: " + NStr::NumericToString(kTimeoutDefault);
        ERR_POST(err_msg);
        m_TimeoutMs = kTimeoutDefault;
    }

    if (m_MaxRetries < kMaxRetriesMin || m_MaxRetries > kMaxRetriesMax) {
        string  err_msg =
            "The max retries is out of range. Allowed "
            "range: " + NStr::NumericToString(kMaxRetriesMin) + "..." +
            NStr::NumericToString(kMaxRetriesMax) + ". Received: " +
            NStr::NumericToString(m_MaxRetries) + ". Reset to "
            "default: " + NStr::NumericToString(kMaxRetriesDefault);
        ERR_POST(err_msg);
        m_MaxRetries = kMaxRetriesDefault;
    }
}


string CPubseqGatewayApp::x_GetCmdLineArguments(void) const
{
    const CNcbiArguments &  arguments = CNcbiApplication::Instance()->
                                                            GetArguments();
    size_t                  args_size = arguments.Size();
    string                  cmdline_args;

    for (size_t index = 0; index < args_size; ++index) {
        if (index != 0)
            cmdline_args += " ";
        cmdline_args += arguments[index];
    }
    return cmdline_args;
}


CRef<CRequestContext> CPubseqGatewayApp::x_CreateRequestContext(
                                                HST::CHttpRequest &  req) const
{
    CRef<CRequestContext>   context;
    if (IsLog()) {
        context.Reset(new CRequestContext());
        context->SetRequestID();

        // NCBI SID may come from the header
        string      sid = req.GetHeaderValue("HTTP_NCBI_SID");
        if (!sid.empty())
            context->SetSessionID(sid);
        else
            context->SetSessionID();

        // NCBI PHID may come from the header
        string      phid = req.GetHeaderValue("HTTP_NCBI_PHID");
        if (!phid.empty())
            context->SetHitID(phid);
        else
            context->SetHitID();

        // Client IP may come from the headers
        TNCBI_IPv6Addr  client_address = req.GetClientIP();
        if (!NcbiIsEmptyIPv6(&client_address)) {
            char        buf[256];
            if (NcbiIPv6ToString(buf, sizeof(buf), &client_address) != 0) {
                context->SetClientIP(buf);
            }
        }

        CDiagContext::SetRequestContext(context);
        CDiagContext_Extra  extra = GetDiagContext().PrintRequestStart();

        // This is the URL path
        extra.Print("request_path", req.GetPath());
        req.PrintParams(extra);

        // If extra is not flushed then it picks read-only even though it is
        // done after...
        extra.Flush();

        // Just in case, avoid to have 0
        context->SetRequestStatus(CRequestStatus::e200_Ok);
        context->SetReadOnly(true);
    }
    return context;
}


void CPubseqGatewayApp::x_PrintRequestStop(CRef<CRequestContext>   context,
                                           int  status)
{
    if (context.NotNull()) {
        CDiagContext::SetRequestContext(context);
        context->SetReadOnly(false);
        context->SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        context.Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


CPubseqGatewayApp::SRequestParameter
CPubseqGatewayApp::x_GetParam(HST::CHttpRequest &  req,
                              const string &  name) const
{
    const char *            value;
    size_t                  value_size;
    SRequestParameter       param;

    param.m_Found = req.GetParam(name.data(), name.size(),
                                 true, &value, &value_size);
    if (param.m_Found)
        param.m_Value = string(value, value_size);
    return param;
}


bool CPubseqGatewayApp::x_IsBoolParamValid(const string &  param_name,
                                           const string &  param_value,
                                           string &  err_msg) const
{
    static string   yes = "yes";
    static string   no = "no";

    if (param_value != yes && param_value != no) {
        err_msg = "Malformed '" + param_name + "' parameter. "
                  "Acceptable values are '" + yes + "' and '" + no + "'.";
        return false;
    }
    return true;
}


int CPubseqGatewayApp::x_PopulateSatToKeyspaceMap(void)
{
    try {
        m_SatNames = FetchSatToKeyspaceMapping("sat_info", m_CassConnection);
    } catch (const exception &  exc) {
        ERR_POST(exc);
        ERR_POST("Cannot populate the sat to keyspace mapping. "
                 "PSG server cannot start.");
        return 1;
    } catch (...) {
        ERR_POST("Unknown error while populating the sat to keyspace mapping. "
                 "PSG server cannot start.");
        return 1;
    }

    if (m_SatNames.empty()) {
        ERR_POST("No sat to keyspace resolutions found "
                 "in the sat_info keyspace. PSG server cannot start.");
        return 1;
    }

    return 0;
}


bool CPubseqGatewayApp::x_IsResolutionParamValid(const string &  param_name,
                                                 const string &  param_value,
                                                 string &  err_msg) const
{
    static string   fast = "fast";
    static string   full = "full";

    if (param_value != fast && param_value != full) {
        err_msg = "Malformed '" + param_name + "' parameter. "
                  "Acceptable values are '" + fast + "' and '" + full + "'.";
        return false;
    }
    return true;
}



int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();


    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetAllowedSessionIDFormat(CRequestContext::eSID_Other);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);

    return CPubseqGatewayApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}
