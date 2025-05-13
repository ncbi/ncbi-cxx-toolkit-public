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

#include <math.h>
#include <thread>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/services/grid_app_version_info.hpp>
#include <util/random_gen.hpp>

#include <google/protobuf/stubs/common.h>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_huge_report_helper.hpp>
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>

#include "http_request.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "shutdown_data.hpp"
#include "cass_monitor.hpp"
#include "myncbi_monitor.hpp"
#include "introspection.hpp"
#include "backlog_per_request.hpp"
#include "active_proc_per_request.hpp"
#include "cass_processor_dispatch.hpp"
#include "cdd_processor.hpp"
#include "wgs_processor.hpp"
#include "snp_processor.hpp"
#include "dummy_processor.hpp"
#include "favicon.hpp"


USING_NCBI_SCOPE;

const EDiagSev          kDefaultSeverity = eDiag_Critical;
const bool              kDefaultTrace = false;
const float             kSplitInfoBlobCacheSizeMultiplier = 0.8;    // Used to calculate low mark
const float             kUserInfoCacheSizeMultiplier = 0.8;         // Used to calculate low mark

static const string     kDaemonizeArgName = "daemonize";


// Memorize the configured severity level to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
EDiagSev                g_ConfiguredSeverity = kDefaultSeverity;

// Memorize the configured tracing to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
bool                    g_Trace = kDefaultTrace;

// Memorize the configured log on/off flag.
// It is used in the context resetter to avoid unnecessary context resets
bool                    g_Log = true;

// Memorize the configured on/off flag for the processor timing
// It is used in the processor base class and the dispatcher
bool                    g_AllowProcessorTiming = false;

// Create the shutdown related data. It is used in a few places:
// a URL handler, signal handlers, watchdog handlers
SShutdownData           g_ShutdownData;


CPubseqGatewayApp *     CPubseqGatewayApp::sm_PubseqApp = nullptr;


CPubseqGatewayApp::CPubseqGatewayApp() :
    m_CassConnection(nullptr),
    m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
    m_StartTime(GetFastLocalTime()),
    m_ExcludeBlobCache(nullptr),
    m_SplitInfoCache(nullptr),
    m_StartupDataState(ePSGS_NoCassConnection),
    m_LogFields("http"),
    m_LastMyNCBIResolveOK(true),
    m_LastMyNCBITestOk(true)
{
    sm_PubseqApp = this;

    // The static checksum tables are used when a session id checksum is
    // calculated. To avoid races later the initialization is called
    // explicitly once.
    CChecksumBase::InitTables();

    m_HelpMessageJson = GetIntrospectionNode().Repr(CJsonNode::fStandardJson);
    m_HelpMessageHtml =
        "\n\n\n<!DOCTYPE html>\n"
        #include "introspection_html.hpp"
        ;

    x_FixIntrospectionVersion();
    x_FixIntrospectionBuildDate();
}


CPubseqGatewayApp::~CPubseqGatewayApp()
{}


void CPubseqGatewayApp::Init(void)
{
    unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

    argdesc->AddFlag(kDaemonizeArgName,
                     "Turn on daemonization of Pubseq Gateway at the start.");

    argdesc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Daemon to service Accession.Version Cache requests");
    SetupArgDescriptions(argdesc.release());

    // Memorize the configured severity
    g_ConfiguredSeverity = GetDiagPostLevel();

    // Memorize the configure trace
    g_Trace = GetDiagTrace();
}


void CPubseqGatewayApp::ParseArgs(void)
{
    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   registry = GetConfig();

    m_Settings.Read(registry, m_Alerts);
    g_Log = m_Settings.m_Log;
    g_AllowProcessorTiming = m_Settings.m_AllowProcessorTiming;

    m_CassConnectionFactory->AppParseArgs(args);
    m_CassConnectionFactory->LoadConfig(registry, "");
    m_CassConnectionFactory->SetLogging(GetDiagPostLevel());

    // It throws an exception in case of inability to start
    m_Settings.Validate(m_Alerts);
}


void CPubseqGatewayApp::OpenCache(void)
{
    // It was decided to work with and without the cache even if the wrapper
    // has not been created. So the cache initialization is called once and
    // always succeed.
    m_StartupDataState = ePSGS_StartupDataOK;

    try {
        // NB. It was decided that the configuration may ommit the cache file
        // paths. In this case the server should not use the corresponding
        // cache at all. This is covered in the CPubseqGatewayCache class.
        m_LookupCache.reset(
                new CPubseqGatewayCache(m_Settings.m_BioseqInfoDbFile,
                                        m_Settings.m_Si2csiDbFile,
                                        m_Settings.m_BlobPropDbFile));

        // The format of the sat ids is different
        set<int>        sat_ids;
        auto            schema_snapshot = m_CassSchemaProvider->GetSchema();
        int32_t         max_sat = schema_snapshot->GetMaxBlobKeyspaceSat();

        for (int  index = 0; index <= max_sat; ++index) {
            if (schema_snapshot->GetBlobKeyspace(index).has_value()) {
                sat_ids.insert(index);
            }
        }

        m_LookupCache->UseReadAhead(m_Settings.m_LmdbReadAhead);
        m_LookupCache->Open(sat_ids);
        const auto        errors = m_LookupCache->GetErrors();
        if (!errors.empty()) {
            string  msg = "Error opening the LMDB cache:";
            for (const auto &  err : errors) {
                msg += "\n" + err.message;
            }

            PSG_ERROR(msg);
            m_Alerts.Register(ePSGS_OpenCache, msg);
            m_LookupCache->ResetErrors();
        }
    } catch (const exception &  exc) {
        string      msg = "Error initializing the LMDB cache: " +
                          string(exc.what()) +
                          ". The server continues without cache.";
        PSG_ERROR(exc);
        m_Alerts.Register(ePSGS_OpenCache, msg);
        m_LookupCache.reset(nullptr);
    } catch (...) {
        string      msg = "Unknown initializing LMDB cache error. "
                          "The server continues without cache.";
        PSG_ERROR(msg);
        m_Alerts.Register(ePSGS_OpenCache, msg);
        m_LookupCache.reset(nullptr);
    }
}


bool CPubseqGatewayApp::OpenCass(void)
{
    static bool             need_logging = true;
    static const string     insecure_cass_section = "CASSANDRA_DB";
    static const string     secure_cass_section = "CASSANDRA_SECURE_DB";

    try {
        if (!m_CassConnection)
            m_CassConnection = m_CassConnectionFactory->CreateInstance();
        m_CassConnection->Connect();

        // CSatInfoSchemaProvider does not throw exceptions
        auto    registry = make_shared<CCompoundRegistry>();
        registry->Add(GetConfig());
        m_CassSchemaProvider = make_shared<CSatInfoSchemaProvider>(
                                                m_Settings.m_RootKeyspace,
                                                m_Settings.m_ConfigurationDomain,
                                                m_CassConnection,
                                                registry,
                                                insecure_cass_section);

        // Debugging: to simulate cassandra timeouts do the following:
        // - set a very short timeout in the configuration file
        // - uncomment the line below (to make mapping loaded properly)
        // m_CassSchemaProvider->SetTimeout(chrono::seconds(15));

        // To start using HUP (hold until publication) a secure section needs
        // to be set for the provider. Set the section only if it exists in the
        // configuration file.
        list<string>        sections;
        GetConfig().EnumerateSections(&sections);
        for (auto &  section : sections) {
            if (NStr::CompareNocase(section, secure_cass_section) == 0) {
                m_CassSchemaProvider->SetSecureSatRegistrySection(secure_cass_section);
                break;
            }
        }

        // Next step in the startup data initialization
        m_StartupDataState = ePSGS_NoValidCassMapping;
    } catch (const exception &  exc) {
        string      msg = "Error connecting to Cassandra: " +
                          string(exc.what());
        if (need_logging)
            PSG_CRITICAL(exc);

        need_logging = false;
        m_Alerts.Register(ePSGS_OpenCassandra, msg);
        return false;
    } catch (...) {
        string      msg = "Unknown Cassandra connecting error";
        if (need_logging)
            PSG_CRITICAL(msg);

        need_logging = false;
        m_Alerts.Register(ePSGS_OpenCassandra, msg);
        return false;
    }

    need_logging = false;
    return true;
}


void CPubseqGatewayApp::CloseCass(void)
{
    m_CassConnection = nullptr;
    m_CassConnectionFactory = nullptr;
}


void CPubseqGatewayApp::CreateMyNCBIFactory(void)
{
    string  error;
    if (CPSG_MyNCBIFactory::InitGlobal(error) == false) {
        PSG_ERROR("Error initializing My NCBI factory: " + error);
    }

    m_MyNCBIFactory = make_shared<CPSG_MyNCBIFactory>();
    m_MyNCBIFactory->SetMyNCBIURL(m_Settings.m_MyNCBIURL);
    if (!m_Settings.m_MyNCBIHttpProxy.empty()) {
        m_MyNCBIFactory->SetHttpProxy(m_Settings.m_MyNCBIHttpProxy);
    }
    m_MyNCBIFactory->SetRequestTimeout(chrono::milliseconds(m_Settings.m_MyNCBITimeoutMs));

    // Initiate a resolution of linkerd. It needs to be done once at the start.
    // Supposedly the resolved name is ending up in some kind of cache so the
    // further requests will go faster.
    m_MyNCBIFactory->SetResolveTimeout(chrono::milliseconds(m_Settings.m_MyNCBIResolveTimeoutMs));
}


void CPubseqGatewayApp::DoMyNCBIDnsResolve(void)
{
    // Called periodically from the my ncbi monitoring thread
    try {
        m_MyNCBIFactory->ResolveAccessPoint();
        m_LastMyNCBIResolveOK = true;
    } catch (const exception &  exc) {
        string      err_msg = "Error while periodically resolving My NCBI "
                              "access point: " + string(exc.what());
        PSG_WARNING(err_msg);
        m_Alerts.Register(ePSGS_MyNCBIResolveDNS, err_msg);
        m_LastMyNCBIResolveOK = false;
    } catch (...) {
        string      err_msg = "Unknown error while periodically resolving "
                              "My NCBI access point.";
        PSG_WARNING(err_msg);
        m_Alerts.Register(ePSGS_MyNCBIResolveDNS, err_msg);
        m_LastMyNCBIResolveOK = false;
    }
}


void CPubseqGatewayApp::TestMyNCBI(uv_loop_t *  loop)
{
    // Called periodically from the my ncbi monitoring thread
    if (m_Settings.m_MyNCBITestWebCubbyUser.empty()) {
        // Effectively disables the test
        return;
    }

    m_LastMyNCBITestOk = true;
    try {
        auto whoami_response = m_MyNCBIFactory->ExecuteWhoAmI(loop,
                                                              m_Settings.m_MyNCBITestWebCubbyUser);
        if (whoami_response.response_status == EPSG_MyNCBIResponseStatus::eError) {
            if (whoami_response.error.status >= CRequestStatus::e500_InternalServerError) {
                string      err_msg = "Error while periodically testing My NCBI: " +
                                      whoami_response.error.message;
                PSG_WARNING(err_msg);
                m_Alerts.Register(ePSGS_MyNCBITest, err_msg);
                m_LastMyNCBITestOk = false;
            }
        }
    } catch (const exception &  exc) {
        string      err_msg = "Error while periodically testing My NCBI: " +
                              string(exc.what());
        PSG_WARNING(err_msg);
        m_Alerts.Register(ePSGS_MyNCBITest, err_msg);
        m_LastMyNCBITestOk = false;
    } catch (...) {
        string      err_msg = "Unknown error while periodically testing "
                              "My NCBI.";
        PSG_WARNING(err_msg);
        m_Alerts.Register(ePSGS_MyNCBITest, err_msg);
        m_LastMyNCBITestOk = false;
    }
}


static unsigned long    s_TickNo = 0;
static unsigned long    s_TickNoFor1Min = 0;
static unsigned long    s_TickNoFor5Sec = 0;

int CPubseqGatewayApp::Run(void)
{
    srand(time(NULL));

    try {
        ParseArgs();
    } catch (const exception &  exc) {
        PSG_CRITICAL(exc.what());
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown argument parsing error");
        return 1;
    }

    if (GetArgs()[kDaemonizeArgName]) {
        // NOTE: if the stdin/stdout are not kept (default daemonize behavior)
        // then libuv fails to close its events loop. There are some asserts
        // with fds, one of them fails so a core dump is generated.
        // With stdin/stdout kept open libuv stays happy and does not crash
        bool    is_good = CCurrentProcess::Daemonize(kEmptyCStr,
                                                     CCurrentProcess::fDF_KeepCWD |
                                                     CCurrentProcess::fDF_KeepStdin |
                                                     CCurrentProcess::fDF_KeepStdout);
        if (!is_good)
            NCBI_THROW(CPubseqGatewayException, eDaemonizationFailed,
                       "Error during daemonization");
    }

    // The config file cannot be reloaded while the server is up and running.
    // So it is better to prepare the config request reply once and then use
    // it.
    m_ConfigReplyJson = x_PrepareConfigJson();


    bool    connected = OpenCass();
    bool    populated = false;
    if (connected) {
        populated = PopulateCassandraMapping(true); // true => initialization stage
    }

    if (populated)
        OpenCache();

    // Creates the factory to resolve web cubby user cookie into a user info
    CreateMyNCBIFactory();

    m_RequestDispatcher.reset(new CPSGS_Dispatcher(m_Settings.m_RequestTimeoutSec));
    x_RegisterProcessors();


    // Needs to initialize the configured health commands in the z end points
    // data structures and some other data
    x_InitialzeZEndPointData();


    auto purge_size = round(float(m_Settings.m_ExcludeCacheMaxSize) *
                            float(m_Settings.m_ExcludeCachePurgePercentage) / 100.0);
    m_ExcludeBlobCache.reset(
        new CExcludeBlobCache(m_Settings.m_ExcludeCacheInactivityPurge,
                              m_Settings.m_ExcludeCacheMaxSize,
                              m_Settings.m_ExcludeCacheMaxSize - static_cast<size_t>(purge_size)));

    m_SplitInfoCache.reset(new CSplitInfoCache(m_Settings.m_SplitInfoBlobCacheSize,
                                               m_Settings.m_SplitInfoBlobCacheSize * kSplitInfoBlobCacheSizeMultiplier));
    m_MyNCBIOKCache.reset(new CMyNCBIOKCache(this, m_Settings.m_MyNCBIOKCacheSize,
                                             m_Settings.m_MyNCBIOKCacheSize * kUserInfoCacheSizeMultiplier));
    m_MyNCBINotFoundCache.reset(new CMyNCBINotFoundCache(this, m_Settings.m_MyNCBINotFoundCacheSize,
                                             m_Settings.m_MyNCBINotFoundCacheSize * kUserInfoCacheSizeMultiplier,
                                             m_Settings.m_MyNCBINotFoundCacheExpirationSec));
    m_MyNCBIErrorCache.reset(new CMyNCBIErrorCache(this, m_Settings.m_MyNCBIErrorCacheSize,
                                             m_Settings.m_MyNCBIErrorCacheSize * kUserInfoCacheSizeMultiplier,
                                             m_Settings.m_MyNCBIErrorCacheBackOffMs));

    m_Timing.reset(new COperationTiming(m_Settings.m_MinStatValue,
                                        m_Settings.m_MaxStatValue,
                                        m_Settings.m_NStatBins,
                                        m_Settings.m_StatScaleType,
                                        m_Settings.m_SmallBlobSize,
                                        m_Settings.m_OnlyForProcessor,
                                        m_Settings.m_LogTimingThreshold,
                                        m_RequestDispatcher->GetProcessorGroupToIndexMap()));
    m_Counters.reset(new CPSGSCounters(m_RequestDispatcher->GetProcessorGroupToIndexMap()));
    // m_IdToNameAndDescription was populated at the time of
    // dealing with arguments
    m_Counters->UpdateConfiguredNameDescription(m_Settings.m_IdToNameAndDescription);


    // Setup IPG huge report
    ipg::CPubseqGatewayHugeIpgReportHelper::SetHugeIpgDisabled(
                                            !m_Settings.m_EnableHugeIPG);

    vector<CHttpHandler>        http_handler;
    CHttpGetParser              get_parser;

    http_handler.emplace_back(
            "/ID/getblob",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetBlob(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGet(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/resolve",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnResolve(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_tse_chunk",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetTSEChunk(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_na",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetNA(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_acc_ver_history",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnAccessionVersionHistory(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/IPG/resolve",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnIPGResolve(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/cassandra",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzCassandra(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/connections",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzConnections(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/lmdb",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzLMDB(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/wgs",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzWGS(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/cdd",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzCDD(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz/snp",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyzSNP(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/readyz",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnReadyz(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/livez",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnLivez(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/healthz",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnHealthz(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/health",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnHealth(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/deep-health",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnDeepHealth(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/hello",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnHello(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/config",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnConfig(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/info",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnInfo(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/status",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnStatus(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/shutdown",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnShutdown(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/get_alerts",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetAlerts(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/ack_alert",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnAckAlert(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/statistics",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnStatistics(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/dispatcher_status",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnDispatcherStatus(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/connections_status",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnConnectionsStatus(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_sat_mapping",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetSatMapping(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/favicon.ico",
            [/*this*/](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                // It's a browser, most probably admin request
                reply->SetContentType(ePSGS_ImageMime);
                reply->SetContentLength(sizeof(favicon));
                reply->SendOk((const char *)(favicon), sizeof(favicon), true);
                return 0;
            }, &get_parser, nullptr);

    if (m_Settings.m_AllowIOTest) {
        m_IOTestBuffer.reset(new char[kMaxTestIOSize]);
        CRandom     random;
        char *      current = m_IOTestBuffer.get();
        for (size_t  k = 0; k < kMaxTestIOSize; k += 8) {
            Uint8   random_val = random.GetRandUint8();
            memcpy(current, &random_val, 8);
            current += 8;
        }

        http_handler.emplace_back(
                "/TEST/io",
                [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
                {
                    return OnTestIO(req, reply);
                }, &get_parser, nullptr);
    }

    http_handler.emplace_back(
            "/",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnBadURL(req, reply);
            }, &get_parser, nullptr);


    x_InitSSL();
    m_HttpDaemon.reset(
            new CHttpDaemon(http_handler, "0.0.0.0",
                            m_Settings.m_HttpPort,
                            m_Settings.m_HttpWorkers,
                            m_Settings.m_ListenerBacklog,
                            m_Settings.m_TcpMaxConn,
                            m_Settings.m_TcpMaxConnSoftLimit,
                            m_Settings.m_TcpMaxConnAlertLimit));

    // Run the monitoring thread
    int             ret_code = 0;
    std::thread     cass_monitoring_thread(CassMonitorThreadedFunction);
    std::thread     myncbi_monitoring_thread(MyNCBIMonitorThreadedFunction,
                                             m_Settings.m_MyNCBIDnsResolveOkPeriodSec,
                                             m_Settings.m_MyNCBIDnsResolveFailPeriodSec,
                                             m_Settings.m_MyNCBITestOkPeriodSec,
                                             m_Settings.m_MyNCBITestFailPeriodSec,
                                             &m_LastMyNCBIResolveOK,
                                             &m_LastMyNCBITestOk);

    try {
        m_HttpDaemon->Run([this](CTcpDaemon &  tcp_daemon)
                {
                    // This lambda is called once per second.
                    // Earlier implementations printed counters on stdout.

                    if (++s_TickNo % m_Settings.m_TickSpan == 0) {
                        s_TickNo = 0;
                        this->m_Timing->Rotate();
                    }

                    if (++s_TickNoFor5Sec % 5 == 0) {
                        ++s_TickNoFor5Sec = 0;
                        this->m_Timing->CollectMomentousStat(
                            m_HttpDaemon->NumOfConnections(),
                            GetActiveProcGroupCounter(),
                            GetBacklogSize()
                                );
                    }

                    if (++s_TickNoFor1Min % 60 == 0) {
                        s_TickNoFor1Min = 0;
                        this->m_Timing->RotateRequestStat();
                        this->m_Timing->RotateAvgPerfTimeSeries();
                    }
                });
    } catch (const CException &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
    } catch (const exception &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while running TCP daemon");
        ret_code = 1;
    }

    // To stop the monitoring threads a shutdown flag needs to be set.
    // It is going to take no more than 0.1 second because the period of
    // checking the shutdown flag in the monitoring threads is 100ms
    g_ShutdownData.m_ShutdownRequested = true;
    myncbi_monitoring_thread.join();
    cass_monitoring_thread.join();

    CloseCass();
    return ret_code;
}



CPubseqGatewayApp *  CPubseqGatewayApp::GetInstance(void)
{
    return sm_PubseqApp;
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


static string       kNcbiSidHeader = "HTTP_NCBI_SID";
static string       kNcbiPhidHeader = "HTTP_NCBI_PHID";
static string       kXForwardedForHeader = "X-Forwarded-For";
static string       kUserAgentHeader = "User-Agent";
static string       kUserAgentApplog = "USER_AGENT";
static string       kRequestPathApplog = "request_path";
static string       kConnectionId = "connection_id";


CRef<CRequestContext> CPubseqGatewayApp::x_CreateRequestContext(
                                                CHttpRequest &  req,
                                                shared_ptr<CPSGS_Reply>  reply)
{
    CRef<CRequestContext>       context;

    // A request context is created unconditionally of the g_Log value
    context.Reset(new CRequestContext());

    string      sid = req.GetHeaderValue(kNcbiSidHeader);
    if (!sid.empty()) {
        context->SetSessionID(sid);
    } else {
        context->SetSessionID();
    }

    bool    overall_need_log = g_Log;
    if (!overall_need_log) {
        // May be overridden by log sampling
        if (m_Settings.m_LogSamplingRatio > 0) {
            // Check the session ID checksum against the configured log samplig
            CChecksum       session_id_checksum(CChecksum::eCRC32);
            session_id_checksum.AddLine(context->GetSessionID());
            auto            checksum = session_id_checksum.GetChecksum();

            if (checksum % m_Settings.m_LogSamplingRatio == 0) {
                overall_need_log = true;
            }
        }
    }

    if (!overall_need_log) {
        context->SetDisabledAppLog(true);
        context->SetDisabledAppLogEvents(CDiagContext::eDisable_All);
    }

    context->SetRequestID();

    // NCBI PHID may come from the header
    string      phid = req.GetHeaderValue(kNcbiPhidHeader);
    if (!phid.empty())
        context->SetHitID(phid);
    else
        context->SetHitID();

    bool    need_peer_ip = false;

    // Client IP may come from the headers
    string      client_ip = req.GetHeaderValue(kXForwardedForHeader);
    if (!client_ip.empty()) {
        vector<string>      ip_addrs;
        NStr::Split(client_ip, ",", ip_addrs);
        if (!ip_addrs.empty()) {
            // Take the first one, there could be many...
            context->SetClientIP(ip_addrs[0]);
        } else {
            need_peer_ip = true;
        }
    } else {
        client_ip = req.GetPeerIP();
        if (!client_ip.empty()) {
            context->SetClientIP(client_ip);
        }
    }

    // It was decided not to use the standard C++ Toolkit function because
    // it searches in the CGI environment and does quite a bit of
    // unnecessary things. The PSG server only needs X-Forwarded-For
    // TNCBI_IPv6Addr  client_address = req.GetClientIP();
    // if (!NcbiIsEmptyIPv6(&client_address)) {
    //     char        buf[256];
    //     if (NcbiIPv6ToString(buf, sizeof(buf), &client_address) != 0) {
    //         context->SetClientIP(buf);
    //     }
    // }

    CDiagContext::SetRequestContext(context);
    CDiagContext_Extra  extra = GetDiagContext().PrintRequestStart();

    // This is the URL path
    extra.Print(kRequestPathApplog, req.GetPath());
    extra.Print(kConnectionId, reply->GetConnectionId());

    string      user_agent = req.GetHeaderValue(kUserAgentHeader);
    if (!user_agent.empty()) {
        extra.Print(kUserAgentApplog, user_agent);
        // This will update the user agent in the connection properties if
        // needed, i.e. if the field was not set before
        reply->UpdatePeerUserAgentIfNeeded(user_agent);
    }


    // The [log]/log_peer_ip_always may overwrite the need_peer_ip value
    if (m_Settings.m_LogPeerIPAlways)
        need_peer_ip = true;

    req.PrintParams(extra, need_peer_ip);
    req.PrintLogFields(m_LogFields);

    // If extra is not flushed then it picks read-only even though it is
    // done after...
    extra.Flush();

    // Just in case, avoid to have 0
    context->SetRequestStatus(CRequestStatus::e200_Ok);
    context->SetReadOnly(true);

    return context;
}


void CPubseqGatewayApp::x_PrintRequestStop(CRef<CRequestContext>   context,
                                           CPSGS_Request::EPSGS_Type  request_type,
                                           CRequestStatus::ECode  status,
                                           size_t  bytes_sent)
{
    m_Counters->IncrementRequestStopCounter(status);

    if (request_type != CPSGS_Request::ePSGS_UnknownRequest) {
        GetTiming().RegisterForTimeSeries(request_type, status);
    }

    CDiagContext::SetRequestContext(context);
    context->SetReadOnly(false);
    context->SetRequestStatus(status);
    context->SetBytesWr(bytes_sent);
    GetDiagContext().PrintRequestStop();
    context.Reset();
    CDiagContext::SetRequestContext(NULL);
}


bool CPubseqGatewayApp::PopulateCassandraMapping(bool  initialization)
{
    static bool need_logging = true;

    string      stage;
    try {
        stage = "keyspace mapping";
        auto     refresh_schema_result = m_CassSchemaProvider->RefreshSchema(true);
        if (refresh_schema_result != ESatInfoRefreshSchemaResult::eSatInfoUpdated) {
            string  err_msg = "Error populating " + stage + " from Cassandra: " +
                              m_CassSchemaProvider->GetRefreshErrorMessage();
            if (need_logging)
                PSG_CRITICAL(err_msg);
            need_logging = false;
            m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
            return false;
        }

        if (!m_CassSchemaProvider->GetIPGKeyspace().has_value()) {
            string  err_msg = "IPG keyspace is not provisioned. "
                              "This essentially switches off the IPG resolve processor.";
            PSG_WARNING(err_msg);
            m_Alerts.Register(ePSGS_NoIPGKeyspace, err_msg);
        } else if (m_CassSchemaProvider->GetIPGKeyspace()->keyspace.empty()) {
            string  err_msg = "IPG keyspace is provisioned as an empty string. "
                              "This essentially switches off the IPG resolve processor.";
            PSG_WARNING(err_msg);
            m_Alerts.Register(ePSGS_NoIPGKeyspace, err_msg);
        }

        stage = "public comment mapping";
        auto    refresh_msgs_result = m_CassSchemaProvider->RefreshMessages(true);
        if (refresh_msgs_result != ESatInfoRefreshMessagesResult::eMessagesUpdated) {
            string  err_msg = "Error populating " + stage + " from Cassandra: " +
                              m_CassSchemaProvider->GetRefreshErrorMessage();
            if (need_logging)
                PSG_CRITICAL(err_msg);
            need_logging = false;
            m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
            return false;
        }
    } catch (const exception &  exc) {
        string      err_msg = "Cannot populate " + stage + " from Cassandra.";
        if (need_logging) {
            PSG_CRITICAL(exc);
            PSG_CRITICAL(err_msg);
        }
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg + " " + exc.what());
        return false;
    } catch (...) {
        string      err_msg = "Unknown error while populating " + stage +
                              " from Cassandra.";
        if (need_logging)
            PSG_CRITICAL(err_msg);
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
        return false;
    }

    if (!initialization) {
        // We are not the first time here; It means that the alert about
        // the bad mapping has been set before. So now we accepted the
        // configuration so a change config alert is needed for the UI
        // visibility
        m_Alerts.Register(ePSGS_NewCassandraMappingAccepted,
                          "Keyspace and public comment mapping "
                          "from Cassandra has been populated");
    }

    // Next step in the startup data initialization
    m_StartupDataState = ePSGS_NoCassCache;

    need_logging = false;
    return true;
}


void CPubseqGatewayApp::CheckCassMapping(void)
{
    string      stage;
    try {
        stage = "keyspace mapping";
        auto     refresh_schema_result = m_CassSchemaProvider->RefreshSchema(true);

        switch (refresh_schema_result) {
            case ESatInfoRefreshSchemaResult::eSatInfoUpdated:
                m_Alerts.Register(ePSGS_NewCassandraSatNamesMapping,
                                  "New " + stage + " found in Cassandra. "
                                  "Server accepted it for Cassandra operations "
                                  "however the server needs to be restarted "
                                  "to use it for LMDB cache");
                break;
            case ESatInfoRefreshSchemaResult::eSatInfoUnchanged:
                break;  // No actions required, all is fine
            default:
                // Some kind of error
                m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                                  "Error checking " + stage + " in Cassandra: " +
                                  m_CassSchemaProvider->GetRefreshErrorMessage());
                return;
        }

        if (!m_CassSchemaProvider->GetIPGKeyspace().has_value()) {
            string  err_msg = "IPG keyspace is not provisioned. "
                              "This essentially switches off the IPG resolve processor.";
            PSG_WARNING(err_msg);
            m_Alerts.Register(ePSGS_NoIPGKeyspace, err_msg);
        } else if (m_CassSchemaProvider->GetIPGKeyspace()->keyspace.empty()) {
            string  err_msg = "IPG keyspace is provisioned as an empty string. "
                              "This essentially switches off the IPG resolve processor.";
            PSG_WARNING(err_msg);
            m_Alerts.Register(ePSGS_NoIPGKeyspace, err_msg);
        }

        stage = "public comment mapping";
        auto    refresh_msgs_result = m_CassSchemaProvider->RefreshMessages(true);
        switch (refresh_msgs_result) {
            case ESatInfoRefreshMessagesResult::eMessagesUpdated:
                m_Alerts.Register(ePSGS_NewCassandraPublicCommentMapping,
                                  "New " + stage + " found in Cassandra. "
                                  "Server accepted it for Cassandra operations.");
                break;
            case ESatInfoRefreshMessagesResult::eMessagesUnchanged:
                break;  // No actions required, all is fine
            default:
                // Some kind of error
                m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                                  "Error checking " + stage + " in Cassandra: " +
                                  m_CassSchemaProvider->GetRefreshErrorMessage());
                return;
        }
    } catch (const exception &  exc) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Error checking " + stage + " in Cassandra: " +
                          string(exc.what()));
        return;
    } catch (...) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Unknown error checking " + stage + " in Cassandra");
        return;
    }
}


// Prepares the chunks for the case when it is a client error so only two
// chunks are required:
// - a message chunk
// - a reply completion chunk
void  CPubseqGatewayApp::x_SendMessageAndCompletionChunks(
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  create_timestamp,
        const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity)
{
    if (reply->IsFinished()) {
        // This is the case when a reply is already formed and sent to
        // the client.
        return;
    }

    reply->SetContentType(ePSGS_PSGMime);
    reply->PrepareReplyMessage(message, status, code, severity);
    reply->PrepareReplyCompletion(status, create_timestamp);
    reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
    reply->SetCompleted();
}


void CPubseqGatewayApp::x_InitSSL(void)
{
    if (m_Settings.m_SSLEnable) {
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_algorithms();
    }
}


void CPubseqGatewayApp::x_RegisterProcessors(void)
{
    // Note: the order of adding defines the priority.
    //       Earleir added - higher priority
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new CPSGS_CassProcessorDispatcher()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::cdd::CPSGS_CDDProcessor()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::wgs::CPSGS_WGSProcessor()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::snp::CPSGS_SNPProcessor()));

    #if 0
    // For testing during development. The processor code does nothing and can
    // be switched on to receive requests.
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new CPSGS_DummyProcessor()));
    #endif

    m_RequestDispatcher->RegisterProcessorsForMomentousCounters();
}


CPSGS_UvLoopBinder &
CPubseqGatewayApp::GetUvLoopBinder(uv_thread_t  uv_thread_id)
{
    auto it = m_ThreadToBinder.find(uv_thread_id);
    if (it == m_ThreadToBinder.end()) {
        // Binding is suppposed only for the processors (which work in their
        // threads, which in turn have their thread id registered at the moment
        // they start)
        PSG_ERROR("Binding is supported only for the worker threads");
        NCBI_THROW(CPubseqGatewayException, eLogic,
                   "Binding is supported only for the worker threads");
    }
    return *(it->second.get());
}


// The HTML help is generated at the time of development when neither version
// nor build date are available. So at the start the version and build date
// should be updated in the already generated html.
// See x_FixIntrospectionVersion() and x_FixIntrospectionBuildDate()


void CPubseqGatewayApp::x_FixIntrospectionVersion(void)
{
    NStr::ReplaceInPlace(m_HelpMessageHtml,
                         "<td>0.0.0</td>",
                         "<td>" PUBSEQ_GATEWAY_VERSION "</td>");
}


void CPubseqGatewayApp::x_FixIntrospectionBuildDate(void)
{
    // The generated build date at the time of the development looks like that:
    // <td>build-date</td>
    // <td>Jul 14 2023 10:44:41</td>

    const string    td_begin = "<td>";
    const string    td_end = "</td>";
    const string    pattern = td_begin + "build-date" + td_end;

    auto            label_pos = m_HelpMessageHtml.find(pattern);
    if (label_pos == string::npos) {
        PSG_WARNING("Cannot find build date label pattern in the generated htlm help");
        return;
    }

    auto            build_date_begin_pos = label_pos + pattern.size();
    build_date_begin_pos = m_HelpMessageHtml.find(td_begin, build_date_begin_pos);
    if (build_date_begin_pos == string::npos) {
        PSG_WARNING("Cannot find build date begin in the generated htlm help");
        return;
    }

    build_date_begin_pos += td_begin.size();
    auto            build_date_end_pos = m_HelpMessageHtml.find(td_end, build_date_begin_pos);
    if (build_date_end_pos == string::npos) {
        PSG_WARNING("Cannot find build date end in the generated htlm help");
        return;
    }

    m_HelpMessageHtml.replace(build_date_begin_pos,
                              build_date_end_pos - build_date_begin_pos,
                              PUBSEQ_GATEWAY_BUILD_DATE);
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


    int ret = CPubseqGatewayApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}

