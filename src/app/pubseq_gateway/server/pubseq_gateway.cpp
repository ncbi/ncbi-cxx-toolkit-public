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
#include <corelib/ncbi_config.hpp>
#include <corelib/plugin_manager.hpp>
#include <connect/services/grid_app_version_info.hpp>
#include <util/random_gen.hpp>

#include <google/protobuf/stubs/common.h>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "shutdown_data.hpp"
#include "cass_monitor.hpp"
#include "introspection.hpp"
#include "resolve_processor.hpp"
#include "annot_processor.hpp"
#include "get_processor.hpp"
#include "getblob_processor.hpp"
#include "tse_chunk_processor.hpp"
#include "osg_processor.hpp"


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
const string            kDefaultRootKeyspace = "sat_info";
const unsigned int      kDefaultExcludeCacheMaxSize = 1000;
const unsigned int      kDefaultExcludeCachePurgePercentage = 20;
const unsigned int      kDefaultExcludeCacheInactivityPurge = 60;
const string            kDefaultAuthToken = "";
const bool              kDefaultAllowIOTest = false;
const unsigned long     kDefaultSlimMaxBlobSize = 10 * 1024;
const unsigned int      kDefaultMaxHops = 2;
const unsigned long     kDefaultSmallBlobSize = 16;

static const string     kDaemonizeArgName = "daemonize";


// Memorize the configured severity level to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
EDiagSev                g_ConfiguredSeverity = eDiag_Critical;

// Memorize the configured log on/off flag.
// It is used in the context resetter to avoid unnecessary context resets
bool                    g_Log = kDefaultLog;

// Create the shutdown related data. It is used in a few places:
// a URL handler, signal handlers, watchdog handlers
SShutdownData           g_ShutdownData;


CPubseqGatewayApp *     CPubseqGatewayApp::sm_PubseqApp = nullptr;


CPubseqGatewayApp::CPubseqGatewayApp() :
    m_MappingIndex(0),
    m_HttpPort(0),
    m_HttpWorkers(kWorkersDefault),
    m_ListenerBacklog(kListenerBacklogDefault),
    m_TcpMaxConn(kTcpMaxConnDefault),
    m_CassConnection(nullptr),
    m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
    m_TimeoutMs(kTimeoutDefault),
    m_MaxRetries(kMaxRetriesDefault),
    m_ExcludeCacheMaxSize(kDefaultExcludeCacheMaxSize),
    m_ExcludeCachePurgePercentage(kDefaultExcludeCachePurgePercentage),
    m_ExcludeCacheInactivityPurge(kDefaultExcludeCacheInactivityPurge),
    m_SmallBlobSize(kDefaultSmallBlobSize),
    m_MinStatValue(kMinStatValue),
    m_MaxStatValue(kMaxStatValue),
    m_NStatBins(kNStatBins),
    m_StatScaleType(kStatScaleType),
    m_TickSpan(kTickSpan),
    m_StartTime(GetFastLocalTime()),
    m_AllowIOTest(kDefaultAllowIOTest),
    m_SlimMaxBlobSize(kDefaultSlimMaxBlobSize),
    m_MaxHops(kDefaultMaxHops),
    m_ExcludeBlobCache(nullptr),
    m_StartupDataState(ePSGS_NoCassConnection)
{
    sm_PubseqApp = this;
    m_HelpMessage = GetIntrospectionNode().Repr(CJsonNode::fStandardJson);

    x_RegisterProcessors();
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

    m_Si2csiDbFile = registry.GetString("LMDB_CACHE", "dbfile_si2csi", "");
    m_BioseqInfoDbFile = registry.GetString("LMDB_CACHE", "dbfile_bioseq_info", "");
    m_BlobPropDbFile = registry.GetString("LMDB_CACHE", "dbfile_blob_prop", "");
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
    g_Log = registry.GetBool("SERVER", "log",
                             kDefaultLog);
    m_RootKeyspace = registry.GetString("SERVER", "root_keyspace",
                                        kDefaultRootKeyspace);

    m_ExcludeCacheMaxSize = registry.GetInt("AUTO_EXCLUDE", "max_cache_size",
                                            kDefaultExcludeCacheMaxSize);
    m_ExcludeCachePurgePercentage = registry.GetInt("AUTO_EXCLUDE",
                                                    "purge_percentage",
                                                    kDefaultExcludeCachePurgePercentage);
    m_ExcludeCacheInactivityPurge = registry.GetInt("AUTO_EXCLUDE",
                                                    "inactivity_purge_timeout",
                                                    kDefaultExcludeCacheInactivityPurge);
    m_AllowIOTest = registry.GetBool("DEBUG", "psg_allow_io_test",
                                     kDefaultAllowIOTest);

    m_SlimMaxBlobSize = x_GetDataSize(registry, "SERVER", "slim_max_blob_size",
                                      kDefaultSlimMaxBlobSize);
    m_MaxHops = registry.GetInt("SERVER", "max_hops", kDefaultMaxHops);

    try {
        m_AuthToken = registry.GetEncryptedString("ADMIN", "auth_token",
                                                  IRegistry::fPlaintextAllowed);
    } catch (const CRegistryException &  ex) {
        string  msg = "Decrypting error detected while reading "
                      "[ADMIN]/auth_token value: " + string(ex.what());
        ERR_POST(msg);
        m_Alerts.Register(ePSGS_ConfigAuthDecrypt, msg);

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    } catch (...) {
        string  msg = "Unknown decrypting error detected while reading "
                      "[ADMIN]/auth_token value";
        ERR_POST(msg);
        m_Alerts.Register(ePSGS_ConfigAuthDecrypt, msg);

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    }

    m_CassConnectionFactory->AppParseArgs(args);
    m_CassConnectionFactory->LoadConfig(registry, "");
    m_CassConnectionFactory->SetLogging(GetDiagPostLevel());

    m_SmallBlobSize = x_GetDataSize(registry, "STATISTICS", "small_blob_size",
                                    kDefaultSmallBlobSize);
    m_MinStatValue = registry.GetInt("STATISTICS", "min", kMinStatValue);
    m_MaxStatValue = registry.GetInt("STATISTICS", "max", kMaxStatValue);
    m_NStatBins = registry.GetInt("STATISTICS", "n_bins", kNStatBins);
    m_StatScaleType = registry.GetString("STATISTICS", "type", kStatScaleType);
    m_TickSpan = registry.GetInt("STATISTICS", "tick_span", kTickSpan);

    x_ReadIdToNameAndDescriptionConfiguration(registry, "COUNTERS");

    if ( registry.GetBool("OSG", "enabled", false) ) {
        m_OSGConnectionPool = new psg::osg::COSGConnectionPool();
        m_OSGConnectionPool->AppParseArgs(args);
        m_OSGConnectionPool->LoadConfig(registry);
        m_OSGConnectionPool->SetLogging(GetDiagPostLevel());
    }

    // It throws an exception in case of inability to start
    x_ValidateArgs();
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
        m_LookupCache.reset(new CPubseqGatewayCache(m_BioseqInfoDbFile,
                                                    m_Si2csiDbFile,
                                                    m_BlobPropDbFile));

        // The format of the sat ids is different
        set<int>        sat_ids;
        for (size_t  index = 0; index < m_SatNames.size(); ++index) {
            if (!m_SatNames[index].empty()) {
                sat_ids.insert(index);
            }
        }

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
    static bool need_logging = true;

    try {
        if (!m_CassConnection)
            m_CassConnection = m_CassConnectionFactory->CreateInstance();
        m_CassConnection->Connect();

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


bool CPubseqGatewayApp::SatToKeyspace(int  sat, string &  sat_name)
{
    if (sat >= 0) {
        if (sat < static_cast<int>(m_SatNames.size())) {
            sat_name = m_SatNames[sat];
            return !sat_name.empty();
        }
    }
    return false;
}


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

    bool    connected = OpenCass();
    bool    populated = false;
    if (connected) {
        PopulatePublicCommentsMapping();
        populated = PopulateCassandraMapping();
    }

    if (populated)
        OpenCache();

    // m_IdToNameAndDescription was populated at the time of
    // dealing with arguments
    UpdateIdToNameDescription(m_IdToNameAndDescription);

    auto purge_size = round(float(m_ExcludeCacheMaxSize) *
                            float(m_ExcludeCachePurgePercentage) / 100.0);
    m_ExcludeBlobCache.reset(
        new CExcludeBlobCache(m_ExcludeCacheInactivityPurge,
                              m_ExcludeCacheMaxSize,
                              m_ExcludeCacheMaxSize - static_cast<size_t>(purge_size)));

    m_Timing.reset(new COperationTiming(m_MinStatValue,
                                        m_MaxStatValue,
                                        m_NStatBins,
                                        m_StatScaleType,
                                        m_SmallBlobSize));

    vector<CHttpHandler<CPendingOperation>>     http_handler;
    CHttpGetParser                              get_parser;

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
            "/favicon.ico",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                // It's a browser, most probably admin request
                reply->Send404("Not found");
                return 0;
            }, &get_parser, nullptr);

    if (m_AllowIOTest) {
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
            "",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnBadURL(req, reply);
            }, &get_parser, nullptr);


    m_TcpDaemon.reset(
            new CHttpDaemon<CPendingOperation>(http_handler, "0.0.0.0",
                                               m_HttpPort,
                                               m_HttpWorkers,
                                               m_ListenerBacklog,
                                               m_TcpMaxConn));



    // Run the monitoring thread
    int             ret_code = 0;
    std::thread     monitoring_thread(CassMonitorThreadedFunction);

    try {
        m_TcpDaemon->Run([this](TSL::CTcpDaemon<CHttpProto<CPendingOperation>,
                           CHttpConnection<CPendingOperation>,
                           CHttpDaemon<CPendingOperation>> &  tcp_daemon)
                {
                    // This lambda is called once per second.
                    // Earlier implementations printed counters on stdout.

                    static unsigned long   tick_no = 0;
                    if (++tick_no % m_TickSpan == 0) {
                        tick_no = 0;
                        this->m_Timing->Rotate();
                    }
                });
    } catch (const CException &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
        g_ShutdownData.m_ShutdownRequested = true;
        monitoring_thread.detach();     // to avoid up to 60 sec delay
    } catch (const exception &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
        g_ShutdownData.m_ShutdownRequested = true;
        monitoring_thread.detach();     // to avoid up to 60 sec delay
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while running TCP daemon");
        ret_code = 1;
        g_ShutdownData.m_ShutdownRequested = true;
        monitoring_thread.detach();     // to avoid up to 60 sec delay
    }

    if (monitoring_thread.joinable()) {
        monitoring_thread.join();
    }

    CloseCass();
    return ret_code;
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


CPubseqGatewayCacheCounters &  CPubseqGatewayApp::GetCacheCounters(void)
{
    return m_CacheCounters;
}


CPubseqGatewayDBCounters &  CPubseqGatewayApp::GetDBCounters(void)
{
    return m_DBCounters;
}


void CPubseqGatewayApp::x_ValidateArgs(void)
{
    if (m_HttpPort < kHttpPortMin || m_HttpPort > kHttpPortMax) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[SERVER]/port value is out of range. Allowed range: " +
                   to_string(kHttpPortMin) + "..." +
                   to_string(kHttpPortMax) + ". Received: " +
                   to_string(m_HttpPort));
    }

    if (m_Si2csiDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_si2csi is not found "
                    "in the ini file. No si2csi cache will be used.");
    }

    if (m_BioseqInfoDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_bioseq_info is not found "
                    "in the ini file. No bioseq_info cache will be used.");
    }

    if (m_BlobPropDbFile.empty()) {
        PSG_WARNING("[LMDB_CACHE]/dbfile_blob_prop is not found "
                    "in the ini file. No blob_prop cache will be used.");
    }

    if (m_HttpWorkers < kWorkersMin || m_HttpWorkers > kWorkersMax) {
        string  err_msg =
            "The number of HTTP workers is out of range. Allowed "
            "range: " + to_string(kWorkersMin) + "..." +
            to_string(kWorkersMax) + ". Received: " +
            to_string(m_HttpWorkers) + ". Reset to "
            "default: " + to_string(kWorkersDefault);
        m_Alerts.Register(ePSGS_ConfigHttpWorkers, err_msg);
        PSG_ERROR(err_msg);
        m_HttpWorkers = kWorkersDefault;
    }

    if (m_ListenerBacklog < kListenerBacklogMin ||
        m_ListenerBacklog > kListenerBacklogMax) {
        string  err_msg =
            "The listener backlog is out of range. Allowed "
            "range: " + to_string(kListenerBacklogMin) + "..." +
            to_string(kListenerBacklogMax) + ". Received: " +
            to_string(m_ListenerBacklog) + ". Reset to "
            "default: " + to_string(kListenerBacklogDefault);
        m_Alerts.Register(ePSGS_ConfigListenerBacklog, err_msg);
        PSG_ERROR(err_msg);
        m_ListenerBacklog = kListenerBacklogDefault;
    }

    if (m_TcpMaxConn < kTcpMaxConnMin || m_TcpMaxConn > kTcpMaxConnMax) {
        string  err_msg =
            "The max number of connections is out of range. Allowed "
            "range: " + to_string(kTcpMaxConnMin) + "..." +
            to_string(kTcpMaxConnMax) + ". Received: " +
            to_string(m_TcpMaxConn) + ". Reset to "
            "default: " + to_string(kTcpMaxConnDefault);
        m_Alerts.Register(ePSGS_ConfigMaxConnections, err_msg);
        PSG_ERROR(err_msg);
        m_TcpMaxConn = kTcpMaxConnDefault;
    }

    if (m_TimeoutMs < kTimeoutMsMin || m_TimeoutMs > kTimeoutMsMax) {
        string  err_msg =
            "The operation timeout is out of range. Allowed "
            "range: " + to_string(kTimeoutMsMin) + "..." +
            to_string(kTimeoutMsMax) + ". Received: " +
            to_string(m_TimeoutMs) + ". Reset to "
            "default: " + to_string(kTimeoutDefault);
        m_Alerts.Register(ePSGS_ConfigTimeout, err_msg);
        PSG_ERROR(err_msg);
        m_TimeoutMs = kTimeoutDefault;
    }

    if (m_MaxRetries < kMaxRetriesMin || m_MaxRetries > kMaxRetriesMax) {
        string  err_msg =
            "The max retries is out of range. Allowed "
            "range: " + to_string(kMaxRetriesMin) + "..." +
            to_string(kMaxRetriesMax) + ". Received: " +
            to_string(m_MaxRetries) + ". Reset to "
            "default: " + to_string(kMaxRetriesDefault);
        m_Alerts.Register(ePSGS_ConfigRetries, err_msg);
        PSG_ERROR(err_msg);
        m_MaxRetries = kMaxRetriesDefault;
    }

    if (m_ExcludeCacheMaxSize < 0) {
        string  err_msg =
            "The max exclude cache size must be a positive integer. "
            "Received: " + to_string(m_ExcludeCacheMaxSize) + ". "
            "Reset to 0 (exclude blobs cache is disabled)";
        m_Alerts.Register(ePSGS_ConfigExcludeCacheSize, err_msg);
        PSG_ERROR(err_msg);
        m_ExcludeCacheMaxSize = 0;
    }

    if (m_ExcludeCachePurgePercentage < 0 || m_ExcludeCachePurgePercentage > 100) {
        string  err_msg = "The exclude cache purge percentage is out of range. "
            "Allowed: 0...100. Received: " +
            to_string(m_ExcludeCachePurgePercentage) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            err_msg += "Reset to " +
                to_string(kDefaultExcludeCachePurgePercentage);
            PSG_ERROR(err_msg);
        } else {
            err_msg += "The provided value has no effect "
                "because the exclude cache is disabled.";
            PSG_WARNING(err_msg);
        }
        m_ExcludeCachePurgePercentage = kDefaultExcludeCachePurgePercentage;
        m_Alerts.Register(ePSGS_ConfigExcludeCachePurgeSize, err_msg);
    }

    if (m_ExcludeCacheInactivityPurge <= 0) {
        string  err_msg = "The exclude cache inactivity purge timeout must be "
            "a positive integer greater than zero. Received: " +
            to_string(m_ExcludeCacheInactivityPurge) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            err_msg += "Reset to " +
                to_string(kDefaultExcludeCacheInactivityPurge);
            PSG_ERROR(err_msg);
        } else {
            err_msg += "The provided value has no effect "
                "because the exclude cache is disabled.";
            PSG_WARNING(err_msg);
        }
        m_ExcludeCacheInactivityPurge = kDefaultExcludeCacheInactivityPurge;
        m_Alerts.Register(ePSGS_ConfigExcludeCacheInactivity, err_msg);
    }

    bool        stat_settings_good = true;
    if (NStr::CompareNocase(m_StatScaleType, "log") != 0 &&
        NStr::CompareNocase(m_StatScaleType, "linear") != 0) {
        string  err_msg = "Invalid [STATISTICS]/type value '" +
            m_StatScaleType + "'. Allowed values are: log, linear. "
            "The statistics parameters are reset to default.";
        m_Alerts.Register(ePSGS_ConfigStatScaleType, err_msg);
        PSG_ERROR(err_msg);
        stat_settings_good = false;

        m_MinStatValue = kMinStatValue;
        m_MaxStatValue = kMaxStatValue;
        m_NStatBins = kNStatBins;
        m_StatScaleType = kStatScaleType;
    }

    if (stat_settings_good) {
        if (m_MinStatValue > m_MaxStatValue) {
            string  err_msg = "Invalid [STATISTICS]/min and max values. The "
                "max cannot be less than min. "
                "The statistics parameters are reset to default.";
            m_Alerts.Register(ePSGS_ConfigStatMinMaxVal, err_msg);
            PSG_ERROR(err_msg);
            stat_settings_good = false;

            m_MinStatValue = kMinStatValue;
            m_MaxStatValue = kMaxStatValue;
            m_NStatBins = kNStatBins;
            m_StatScaleType = kStatScaleType;
        }
    }

    if (stat_settings_good) {
        if (m_NStatBins <= 0) {
            string  err_msg = "Invalid [STATISTICS]/n_bins value. The "
                "number of bins must be greater than 0. "
                "The statistics parameters are reset to default.";
            m_Alerts.Register(ePSGS_ConfigStatNBins, err_msg);
            PSG_ERROR(err_msg);
            stat_settings_good = false;

            m_MinStatValue = kMinStatValue;
            m_MaxStatValue = kMaxStatValue;
            m_NStatBins = kNStatBins;
            m_StatScaleType = kStatScaleType;
        }
    }

    if (m_TickSpan <= 0) {
        PSG_WARNING("Invalid [STATISTICS]/tick_span value (" +
                    to_string(m_TickSpan) + "). "
                    "The tick span must be greater than 0. The tick span is "
                    "reset to the default value (" +
                    to_string(kTickSpan) + ").");
        m_TickSpan = kTickSpan;
    }

    if (m_MaxHops <= 0) {
        PSG_WARNING("Invalid [SERVER]/max_hops value (" +
                    to_string(m_MaxHops) + "). "
                    "The max hops must be greater than 0. The max hops is "
                    "reset to the default value (" +
                    to_string(kDefaultMaxHops) + ").");
        m_MaxHops = kDefaultMaxHops;
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


static string   kNcbiSidHeader = "HTTP_NCBI_SID";
static string   kNcbiPhidHeader = "HTTP_NCBI_PHID";
static string   kXForwardedForHeader = "X-Forwarded-For";
static string   kUserAgentHeader = "User-Agent";
static string   kUserAgentApplog = "USER_AGENT";
static string   kRequestPathApplog = "request_path";
CRef<CRequestContext> CPubseqGatewayApp::x_CreateRequestContext(
                                                CHttpRequest &  req) const
{
    CRef<CRequestContext>   context;
    if (g_Log) {
        context.Reset(new CRequestContext());
        context->SetRequestID();

        // NCBI SID may come from the header
        string      sid = req.GetHeaderValue(kNcbiSidHeader);
        if (!sid.empty())
            context->SetSessionID(sid);
        else
            context->SetSessionID();

        // NCBI PHID may come from the header
        string      phid = req.GetHeaderValue(kNcbiPhidHeader);
        if (!phid.empty())
            context->SetHitID(phid);
        else
            context->SetHitID();

        // Client IP may come from the headers
        string      client_ip = req.GetHeaderValue(kXForwardedForHeader);
        if (!client_ip.empty()) {
            vector<string>      ip_addrs;
            NStr::Split(client_ip, ",", ip_addrs);
            if (!ip_addrs.empty()) {
                // Take the first one, there could be many...
                context->SetClientIP(ip_addrs[0]);
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

        string      user_agent = req.GetHeaderValue(kUserAgentHeader);
        if (!user_agent.empty())
            extra.Print(kUserAgentApplog, user_agent);

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
CPubseqGatewayApp::x_GetParam(CHttpRequest &  req, const string &  name) const
{
    SRequestParameter       param;
    const char *            value;
    size_t                  value_size;

    param.m_Found = req.GetParam(name.data(), name.size(),
                                 true, &value, &value_size);
    if (param.m_Found)
        param.m_Value.assign(value, value_size);
    return param;
}


bool CPubseqGatewayApp::x_IsBoolParamValid(const string &  param_name,
                                           const CTempString &  param_value,
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


SPSGS_ResolveRequest::EPSGS_OutputFormat
CPubseqGatewayApp::x_GetOutputFormat(const string &  param_name,
                                     const CTempString &  param_value,
                                     string &  err_msg) const
{
    static string   protobuf = "protobuf";
    static string   json = "json";
    static string   native = "native";

    if (param_value == protobuf)
        return SPSGS_ResolveRequest::ePSGS_ProtobufFormat;
    if (param_value == json)
        return SPSGS_ResolveRequest::ePSGS_JsonFormat;
    if (param_value == native)
        return SPSGS_ResolveRequest::ePSGS_NativeFormat;

    err_msg = "Malformed '" + param_name + "' parameter. "
              "Acceptable values are '" +
              protobuf + "' and '" +
              json + "' and '" +
              native + "'.";
    return SPSGS_ResolveRequest::ePSGS_UnknownFormat;
}


SPSGS_BlobRequestBase::EPSGS_TSEOption
CPubseqGatewayApp::x_GetTSEOption(const string &  param_name,
                                  const CTempString &  param_value,
                                  string &  err_msg) const
{
    static string   none = "none";
    static string   whole = "whole";
    static string   orig = "orig";
    static string   smart = "smart";
    static string   slim = "slim";

    if (param_value == none)
        return SPSGS_BlobRequestBase::ePSGS_NoneTSE;
    if (param_value == whole)
        return SPSGS_BlobRequestBase::ePSGS_WholeTSE;
    if (param_value == orig)
        return SPSGS_BlobRequestBase::ePSGS_OrigTSE;
    if (param_value == smart)
        return SPSGS_BlobRequestBase::ePSGS_SmartTSE;
    if (param_value == slim)
        return SPSGS_BlobRequestBase::ePSGS_SlimTSE;

    err_msg = "Malformed '" + param_name + "' parameter. "
              "Acceptable values are '" +
              none + "', '" +
              whole + "', '" +
              orig + "', '" +
              smart + "' and '" +
              slim + "'.";
    return SPSGS_BlobRequestBase::ePSGS_UnknownTSE;
}


SPSGS_RequestBase::EPSGS_AccSubstitutioOption
CPubseqGatewayApp::x_GetAccessionSubstitutionOption(
                                    const string &  param_name,
                                    const CTempString &  param_value,
                                    string &  err_msg) const
{
    static string       default_option = "default";
    static string       limited_option = "limited";
    static string       never_option = "never";

    if (param_value == default_option)
        return SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
    if (param_value == limited_option)
        return SPSGS_RequestBase::ePSGS_LimitedAccSubstitution;
    if (param_value == never_option)
        return SPSGS_RequestBase::ePSGS_NeverAccSubstitute;

    err_msg = "Malformed '" + param_name + "' parameter. "
              "Acceptable values are '" +
              default_option + "', '" +
              limited_option + "', '" +
              never_option + "'.";
    return SPSGS_RequestBase::ePSGS_UnknownAccSubstitution;
}


bool
CPubseqGatewayApp::x_GetTraceParameter(CHttpRequest &  req,
                                       const string &  param_name,
                                       SPSGS_RequestBase::EPSGS_Trace &  trace,
                                       string &  err_msg)
{
    trace = SPSGS_RequestBase::ePSGS_NoTracing;
    SRequestParameter   trace_protocol_param = x_GetParam(req, param_name);

    if (trace_protocol_param.m_Found) {
        if (!x_IsBoolParamValid(param_name,
                                trace_protocol_param.m_Value, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            PSG_WARNING(err_msg);
            return false;
        }
        if (trace_protocol_param.m_Value == "yes")
            trace = SPSGS_RequestBase::ePSGS_WithTracing;
        else
            trace = SPSGS_RequestBase::ePSGS_NoTracing;;
    }
    return true;
}


bool
CPubseqGatewayApp::x_GetHops(CHttpRequest &  req,
                             shared_ptr<CPSGS_Reply>  reply,
                             int &  hops)
{
    static string   kHopsParam = "hops";

    hops = 0;   // Default value
    SRequestParameter   hops_param = x_GetParam(req, kHopsParam);

    if (hops_param.m_Found) {
        string      err_msg;
        if (!x_ConvertIntParameter(kHopsParam, hops_param.m_Value,
                                   hops, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }

        if (hops < 0) {
            err_msg = "Invalid '" + kHopsParam + "' value " + to_string(hops) +
                      ". It must be > 0.";
            m_ErrorCounters.IncMalformedArguments();
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }

        if (hops > m_MaxHops) {
            err_msg = "The '" + kHopsParam + "' value " + to_string(hops) +
                      " exceeds the server configured value " +
                      to_string(m_MaxHops) + ".";
            m_ErrorCounters.IncMaxHopsExceededError();
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }
    }
    return true;
}


vector<string>
CPubseqGatewayApp::x_GetExcludeBlobs(const string &  param_name,
                                     const CTempString &  param_value) const
{
    vector<string>              result;
    vector<string>              blob_ids;
    NStr::Split(param_value, ",", blob_ids);

    size_t                      empty_count = 0;
    for (const auto &  item : blob_ids) {
        if (item.empty())
            ++empty_count;
        else
            result.push_back(item);
    }

    if (empty_count > 0)
        PSG_WARNING("Found " << empty_count << " empty blob id(s) in the '" <<
                    param_name << "' list (empty blob ids are ignored)");

    return result;
}


bool CPubseqGatewayApp::x_ConvertIntParameter(const string &  param_name,
                                              const CTempString &  param_value,
                                              int &  converted,
                                              string &  err_msg) const
{
    try {
        converted = NStr::StringToInt(param_value);
    } catch (...) {
        err_msg = "Error converting '" + param_name + "' parameter "
                  "to integer (received value: '" + string(param_value) + "')";
        return false;
    }
    return true;
}


unsigned long
CPubseqGatewayApp::x_GetDataSize(const IRegistry &  reg,
                                 const string &  section,
                                 const string &  entry,
                                 unsigned long  default_val)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree *     param_tree = conf.GetTree();
    const TPluginManagerParamTree * section_tree =
                                        param_tree->FindSubNode(section);

    if (!section_tree)
        return default_val;

    CConfig     c((CConfig::TParamTree*)section_tree, eNoOwnership);
    return c.GetDataSize("psg", entry, CConfig::eErr_NoThrow,
                         default_val);
}


bool CPubseqGatewayApp::PopulateCassandraMapping(bool  need_accept_alert)
{
    static bool need_logging = true;

    size_t      vacant_index = m_MappingIndex + 1;
    if (vacant_index >= 2)
        vacant_index = 0;
    m_CassMapping[vacant_index].clear();

    try {
        string      err_msg;
        if (!FetchSatToKeyspaceMapping(
                    m_RootKeyspace, m_CassConnection,
                    m_SatNames, eBlobVer2Schema,
                    m_CassMapping[vacant_index].m_BioseqKeyspace, eResolverSchema,
                    m_CassMapping[vacant_index].m_BioseqNAKeyspaces, eNamedAnnotationsSchema,
                    err_msg)) {
            err_msg = "Error populating cassandra mapping: " + err_msg;
            if (need_logging)
                PSG_CRITICAL(err_msg);
            need_logging = false;
            m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
            return false;
        }
    } catch (const exception &  exc) {
        string      err_msg = "Cannot populate keyspace mapping from Cassandra.";
        if (need_logging) {
            PSG_CRITICAL(exc);
            PSG_CRITICAL(err_msg);
        }
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg + " " + exc.what());
        return false;
    } catch (...) {
        string      err_msg = "Unknown error while populating "
                              "keyspace mapping from Cassandra.";
        if (need_logging)
            PSG_CRITICAL(err_msg);
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
        return false;
    }

    auto    errors = m_CassMapping[vacant_index].validate(m_RootKeyspace);
    if (m_SatNames.empty())
        errors.push_back("No sat to keyspace resolutions found in the '" +
                         m_RootKeyspace + "' keyspace.");

    if (errors.empty()) {
        m_MappingIndex = vacant_index;

        // Next step in the startup data initialization
        m_StartupDataState = ePSGS_NoCassCache;

        if (need_accept_alert) {
            // We are not the first time here; It means that the alert about
            // the bad mapping has been set before. So now we accepted the
            // configuration so a change config alert is needed for the UI
            // visibility
            m_Alerts.Register(ePSGS_NewCassandraMappingAccepted,
                              "Keyspace mapping (sat names, bioseq info and named "
                              "annotations) has been populated");
        }
    } else {
        string      combined_error("Invalid Cassandra mapping:");
        for (const auto &  err : errors) {
            combined_error += "\n" + err;
        }
        if (need_logging)
            PSG_CRITICAL(combined_error);

        m_Alerts.Register(ePSGS_NoValidCassandraMapping, combined_error);
    }

    need_logging = false;
    return errors.empty();
}


void CPubseqGatewayApp::PopulatePublicCommentsMapping(void)
{
    if (m_PublicComments.get() != nullptr)
        return;     // We have already been here: the mapping needs to be
                    // populated once

    try {
        string      err_msg;

        m_PublicComments.reset(new CPSGMessages);

        if (!FetchMessages(m_RootKeyspace, m_CassConnection,
                           *m_PublicComments.get(), err_msg)) {
            err_msg = "Error populating cassandra public comments mapping: " + err_msg;
            PSG_ERROR(err_msg);
            m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping, err_msg);
        }
    } catch (const exception &  exc) {
        string      err_msg = "Cannot populate public comments mapping from Cassandra";
        PSG_ERROR(exc);
        PSG_ERROR(err_msg);
        m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping,
                          err_msg + ". " + exc.what());
    } catch (...) {
        string      err_msg = "Unknown error while populating "
                              "public comments mapping from Cassandra";
        PSG_ERROR(err_msg);
        m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping, err_msg);
    }
}


void CPubseqGatewayApp::CheckCassMapping(void)
{
    size_t      vacant_index = m_MappingIndex + 1;
    if (vacant_index >= 2)
        vacant_index = 0;
    m_CassMapping[vacant_index].clear();

    vector<string>      sat_names;
    try {
        string      err_msg;
        if (!FetchSatToKeyspaceMapping(
                    m_RootKeyspace, m_CassConnection,
                    sat_names, eBlobVer2Schema,
                    m_CassMapping[vacant_index].m_BioseqKeyspace, eResolverSchema,
                    m_CassMapping[vacant_index].m_BioseqNAKeyspaces, eNamedAnnotationsSchema,
                    err_msg)) {
            m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                              "Error checking cassandra mapping: " + err_msg);
            return;
        }

    } catch (const exception &  exc) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Cannot check keyspace mapping from Cassandra. " +
                          string(exc.what()));
        return;
    } catch (...) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Unknown error while checking "
                          "keyspace mapping from Cassandra.");
        return;
    }

    auto    errors = m_CassMapping[vacant_index].validate(m_RootKeyspace);
    if (sat_names.empty())
        errors.push_back("No sat to keyspace resolutions found in the " +
                         m_RootKeyspace + " keyspace.");

    if (errors.empty()) {
        // No errors detected in the DB; let's compare with what we use now
        if (sat_names != m_SatNames)
            m_Alerts.Register(ePSGS_NewCassandraSatNamesMapping,
                              "Cassandra has new sat names mapping in  the " +
                              m_RootKeyspace + " keyspace. The server needs "
                              "to restart to use it.");

        if (m_CassMapping[0] != m_CassMapping[1]) {
            m_MappingIndex = vacant_index;
            m_Alerts.Register(ePSGS_NewCassandraMappingAccepted,
                              "Keyspace mapping (bioseq info and named "
                              "annotations) has been updated");
        }
    } else {
        string      combined_error("Invalid Cassandra mapping detected "
                                   "during the periodic check:");
        for (const auto &  err : errors) {
            combined_error += "\n" + err;
        }
        m_Alerts.Register(ePSGS_InvalidCassandraMapping, combined_error);
    }
}


bool CPubseqGatewayApp::x_IsResolutionParamValid(const string &  param_name,
                                                 const CTempString &  param_value,
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


// Prepares the chunks for the case when it is a client error so only two
// chunks are required:
// - a message chunk
// - a reply completion chunk
void  CPubseqGatewayApp::x_SendMessageAndCompletionChunks(
        shared_ptr<CPSGS_Reply>  reply,
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
    reply->PrepareReplyCompletion();
    reply->Flush();
}


void CPubseqGatewayApp::x_MalformedArguments(
                                shared_ptr<CPSGS_Reply>  reply,
                                CRef<CRequestContext> &  context,
                                const string &  err_msg)
{
    m_ErrorCounters.IncMalformedArguments();
    x_SendMessageAndCompletionChunks(reply, err_msg,
                                     CRequestStatus::e400_BadRequest,
                                     ePSGS_MalformedParameter, eDiag_Error);
    PSG_WARNING(err_msg);
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
}


void CPubseqGatewayApp::x_InsufficientArguments(
                                shared_ptr<CPSGS_Reply>  reply,
                                CRef<CRequestContext> &  context,
                                const string &  err_msg)
{
    m_ErrorCounters.IncInsufficientArguments();
    x_SendMessageAndCompletionChunks(reply, err_msg,
                                     CRequestStatus::e400_BadRequest,
                                     ePSGS_InsufficientArguments, eDiag_Error);
    PSG_WARNING(err_msg);
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
}


void CPubseqGatewayApp::x_ReadIdToNameAndDescriptionConfiguration(
                                                    const IRegistry &  reg,
                                                    const string &  section)
{
    list<string>            entries;
    reg.EnumerateEntries(section, &entries);

    for(const auto &  value_id : entries) {
        string      name_and_description = reg.Get(section, value_id);
        string      name;
        string      description;
        if (NStr::SplitInTwo(name_and_description, ":::", name, description,
                             NStr::fSplit_ByPattern)) {
            m_IdToNameAndDescription[value_id] = {name, description};
        } else {
            PSG_WARNING("Malformed counter [" << section << "]/" << name <<
                        " information. Expected <name>:::<description");
        }
    }
}


void CPubseqGatewayApp::x_RegisterProcessors(void)
{
    // Note: the order of adding defines the order of running
    m_RequestDispatcher.AddProcessor(
            unique_ptr<IPSGS_Processor>(new CPSGS_ResolveProcessor()));
    m_RequestDispatcher.AddProcessor(
            unique_ptr<IPSGS_Processor>(new CPSGS_GetProcessor()));
    m_RequestDispatcher.AddProcessor(
            unique_ptr<IPSGS_Processor>(new CPSGS_GetBlobProcessor()));
    m_RequestDispatcher.AddProcessor(
            unique_ptr<IPSGS_Processor>(new CPSGS_AnnotProcessor()));
    m_RequestDispatcher.AddProcessor(
            unique_ptr<IPSGS_Processor>(new CPSGS_TSEChunkProcessor()));
    m_RequestDispatcher.AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::osg::CPSGS_OSGProcessor()));
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


void CollectGarbage(void)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->GetExcludeBlobCache()->Purge();
}

