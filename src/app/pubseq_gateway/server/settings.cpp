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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_config.hpp>
#include <corelib/plugin_manager.hpp>

#include "settings.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "alerts.hpp"
#include "timing.hpp"


const string            kServerSection = "SERVER";
const string            kLmdbCacheSection = "LMDB_CACHE";
const string            kStatisticsSection = "STATISTICS";
const string            kAutoExcludeSection = "AUTO_EXCLUDE";
const string            kDebugSection = "DEBUG";
const string            kIPGSection = "IPG";
const string            kSSLSection = "SSL";
const string            kHealthSection = "HEALTH";
const string            kCDDProcessorSection = "CDD_PROCESSOR";
const string            kWGSProcessorSection = "WGS_PROCESSOR";
const string            kSNPProcessorSection = "SNP_PROCESSOR";
const string            kCassandraProcessorSection = "CASSANDRA_PROCESSOR";
const string            kLMDBProcessorSection = "LMDB_PROCESSOR";
const string            kAdminSection = "ADMIN";
const string            kMyNCBISection = "MY_NCBI";
const string            kCountersSection = "COUNTERS";
const string            kLogSection = "LOG";
const string            kH2OSection = "H2O";


const unsigned short    kWorkersDefault = 64;
const unsigned int      kListenerBacklogDefault = 256;
const int64_t           kTcpMaxConnDefault = 4096;
const double            kDefaultTcpMaxConnSoftLimitPercent = 90.0;
const double            kDefaultTcpMaxConnAlertLimitPercent = 90.0;
const int64_t           kTcpConnHardSoftDiffDefault = 256;
const int64_t           kTcpMaxConnSoftLimitDefault = kTcpMaxConnDefault - kTcpConnHardSoftDiffDefault;
const int64_t           kTcpConnSoftAlertDiffDefault = 512;
const int64_t           kTcpMaxConnAlertLimitDefault = kTcpMaxConnSoftLimitDefault - kTcpConnSoftAlertDiffDefault;
const unsigned int      kTimeoutDefault = 30000;
const unsigned int      kMaxRetriesDefault = 2;
const string            kDefaultRootKeyspace = "sat_info3";
const string            kDefaultConfigurationDomain = "PSG";
const size_t            kDefaultHttpMaxBacklog = 1024;
const size_t            kDefaultHttpMaxRunning = 64;
const size_t            kDefaultLogSamplingRatio = 0;
const size_t            kDefaultLogTimingThreshold = 1000;

const double            kDefaultConnThrottleThresholdPercent = 75.0;
const size_t            kDefaultConnThrottleThreshold = round(double(kTcpMaxConnAlertLimitDefault) * kDefaultConnThrottleThresholdPercent / 100.0);

const double            kDefaultConnThrottleByHostPercent = 25.0;
const size_t            kDefaultConnThrottleByHost = round(double(kDefaultConnThrottleThreshold) * kDefaultConnThrottleByHostPercent / 100.0);

const double            kDefaultConnThrottleBySitePercent = 35.0;
const size_t            kDefaultConnThrottleBySite = round(double(kDefaultConnThrottleThreshold) * kDefaultConnThrottleBySitePercent / 100.0);

const double            kDefaultConnThrottleByProcessPercent = 15.0;
const size_t            kDefaultConnThrottleByProcess = round(double(kDefaultConnThrottleThreshold) * kDefaultConnThrottleByProcessPercent / 100.0);

const double            kDefaultConnThrottleByUserAgentPercent = 40.0;
const size_t            kDefaultConnThrottleByUserAgent = round(double(kDefaultConnThrottleThreshold) * kDefaultConnThrottleByUserAgentPercent / 100.0);

const double            kDefaultConnThrottleCloseIdleSec = 5.0;
const double            kDefaultConnForceCloseWaitSec = 0.1;
const double            kDefaultThrottlingDataValidSec = 1.0;

const unsigned long     kDefaultSendBlobIfSmall = 10 * 1024;
const unsigned long     kDefaultSmallBlobSize = 16;
const bool              kDefaultLog = true;
const unsigned int      kDefaultExcludeCacheMaxSize = 1000;
const unsigned int      kDefaultExcludeCachePurgePercentage = 20;
const unsigned int      kDefaultExcludeCacheInactivityPurge = 60;
const unsigned int      kDefaultMaxHops = 2;
const bool              kDefaultAllowIOTest = false;
const bool              kDefaultAllowProcessorTiming = false;
const string            kDefaultOnlyForProcessor = "";
const bool              kDefaultLmdbReadAhead = false;
const double            kDefaultResendTimeoutSec = 0.2;
const double            kDefaultRequestTimeoutSec = 30.0;
const size_t            kDefaultProcessorMaxConcurrency = 1200;
const size_t            kDefaultSplitInfoBlobCacheSize = 1000;
const size_t            kDefaultIPGPageSize = 1024;
const bool              kDefaultEnableHugeIPG = true;
const string            kDefaultAuthToken = "";
// All ADMIN/ urls except of 'info'
const string            kDefaultAuthCommands = "config status shutdown get_alerts ack_alert statistics";
const bool              kDefaultSSLEnable = false;
const string            kDefaultSSLCertFile = "";
const string            kDefaultSSLKeyFile = "";
const string            kDefaultSSLCiphers = "EECDH+aRSA+AESGCM EDH+aRSA+AESGCM EECDH+aRSA EDH+aRSA !SHA !SHA256 !SHA384";
const size_t            kDefaultShutdownIfTooManyOpenFDforHTTP = 0;
const size_t            kDefaultShutdownIfTooManyOpenFDforHTTPS = 8000;
const string            kDefaultCriticalDataSources = "cassandra";
const double            kDefaultHealthTimeout = 0.5;
const bool              kDefaultCassandraProcessorsEnabled = true;
const string            kDefaultLMDBProcessorHealthCommand = "/ID/resolve?seq_id=gi|2&use_cache=yes";
const double            kDefaultLMDBHealthTimeoutSec = 0.0;
const string            kDefaultCassandraProcessorHealthCommand = "/ID/resolve?seq_id=gi|2&use_cache=no";
const double            kDefaultCassandraHealthTimeoutSec = 0.0;
const bool              kDefaultSeqIdResolveAlways = false;
const bool              kDefaultCDDProcessorsEnabled = true;
const string            kDefaultCDDProcessorHealthCommand = "/ID/get_na?seq_id=6&names=CDD";
const double            kDefaultCDDHealthTimeoutSec = 0.0;
const bool              kDefaultWGSProcessorsEnabled = true;
const string            kDefaultWGSProcessorHealthCommand = "/ID/resolve?seq_id=EAB1000000&disable_processor=cassandra";
const double            kDefaultWGSHealthTimeoutSec = 0.0;
const bool              kDefaultSNPProcessorsEnabled = true;
const string            kDefaultSNPProcessorHealthCommand = "/ID/get_na?seq_id_type=12&seq_id=568801899&seq_ids=ref|NT_187403.1&names=SNP";
const double            kDefaultSNPHealthTimeoutSec = 0.0;
const size_t            kDefaultMyNCBIOKCacheSize = 10000;
const size_t            kDefaultMyNCBINotFoundCacheSize = 10000;
const size_t            kDefaultMyNCBINotFoundCacheExpirationSec = 3600;
const size_t            kDefaultMyNCBIErrorCacheSize = 10000;
const size_t            kDefaultMyNCBIErrorCacheBackOffMs = 1000;
const string            kDefaultMyNCBIURL = "http://txproxy.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount";
const string            kDefaultMyNCBIHttpProxy = "linkerd:4140";
const size_t            kDefaultMyNCBITimeoutMs = 100;
const size_t            kDefaultMyNCBIResolveTimeoutMs = 300;
const size_t            kDefaultMyNCBIDnsResolveOkPeriodSec = 60;
const size_t            kDefaultMyNCBIDnsResolveFailPeriodSec = 10;
const string            kDefaultMyNCBITestWebCubbyUser = "MVWNUIMYDR41F2XRDBT8JTFGDFFR9KJM;logged-in=true;my-name=vakatov@ncbi.nlm.nih.gov;persistent=true@E7321B44700304B1_0000SID";
const size_t            kDefaultMyNCBITestOkPeriodSec = 180;
const size_t            kDefaultMyNCBITestFailPeriodSec = 20;
const bool              kDefaultLogPeerIPAlways = false;
const size_t            kDefaultIdleTimeoutSec = 100000;


SPubseqGatewaySettings::SPubseqGatewaySettings() :
    m_HttpPort(0),
    m_HttpWorkers(kWorkersDefault),
    m_ListenerBacklog(kListenerBacklogDefault),
    m_TcpMaxConn(kTcpMaxConnDefault),
    m_TcpMaxConnSoftLimit(kTcpMaxConnSoftLimitDefault),
    m_TcpMaxConnAlertLimit(kTcpMaxConnAlertLimitDefault),
    m_TimeoutMs(kTimeoutDefault),
    m_MaxRetries(kMaxRetriesDefault),
    m_SendBlobIfSmall(kDefaultSendBlobIfSmall),
    m_Log(kDefaultLog),
    m_MaxHops(kDefaultMaxHops),
    m_ResendTimeoutSec(kDefaultResendTimeoutSec),
    m_RequestTimeoutSec(kDefaultRequestTimeoutSec),
    m_ProcessorMaxConcurrency(kDefaultProcessorMaxConcurrency),
    m_SplitInfoBlobCacheSize(kDefaultSplitInfoBlobCacheSize),
    m_ShutdownIfTooManyOpenFD(0),
    m_RootKeyspace(kDefaultRootKeyspace),
    m_ConfigurationDomain(kDefaultConfigurationDomain),
    m_HttpMaxBacklog(kDefaultHttpMaxBacklog),
    m_HttpMaxRunning(kDefaultHttpMaxRunning),
    m_LogSamplingRatio(kDefaultLogSamplingRatio),
    m_LogTimingThreshold(kDefaultLogTimingThreshold),
    m_ConnThrottleThreshold(kDefaultConnThrottleThreshold),
    m_ConnThrottleByHost(kDefaultConnThrottleByHost),
    m_ConnThrottleBySite(kDefaultConnThrottleBySite),
    m_ConnThrottleByProcess(kDefaultConnThrottleByProcess),
    m_ConnThrottleByUserAgent(kDefaultConnThrottleByUserAgent),
    m_ConnThrottleCloseIdleSec(kDefaultConnThrottleCloseIdleSec),
    m_ConnForceCloseWaitSec(kDefaultConnForceCloseWaitSec),
    m_ThrottlingDataValidSec(kDefaultThrottlingDataValidSec),
    m_SmallBlobSize(kDefaultSmallBlobSize),
    m_MinStatValue(kMinStatValue),
    m_MaxStatValue(kMaxStatValue),
    m_NStatBins(kNStatBins),
    m_StatScaleType(kStatScaleType),
    m_TickSpan(kTickSpan),
    m_OnlyForProcessor(kDefaultOnlyForProcessor),
    m_LmdbReadAhead(kDefaultLmdbReadAhead),
    m_ExcludeCacheMaxSize(kDefaultExcludeCacheMaxSize),
    m_ExcludeCachePurgePercentage(kDefaultExcludeCachePurgePercentage),
    m_ExcludeCacheInactivityPurge(kDefaultExcludeCacheInactivityPurge),
    m_AllowIOTest(kDefaultAllowIOTest),
    m_AllowProcessorTiming(kDefaultAllowProcessorTiming),
    m_SSLEnable(kDefaultSSLEnable),
    m_SSLCiphers(kDefaultSSLCiphers),
    m_HealthTimeoutSec(kDefaultHealthTimeout),
    m_CassandraProcessorsEnabled(kDefaultCassandraProcessorsEnabled),
    m_CassandraProcessorHealthCommand(kDefaultCassandraProcessorHealthCommand),
    m_CassandraHealthTimeoutSec(kDefaultCassandraHealthTimeoutSec),
    m_SeqIdResolveAlways(kDefaultSeqIdResolveAlways),
    m_LMDBProcessorHealthCommand(kDefaultLMDBProcessorHealthCommand),
    m_LMDBHealthTimeoutSec(kDefaultLMDBHealthTimeoutSec),
    m_CDDProcessorsEnabled(kDefaultCDDProcessorsEnabled),
    m_CDDProcessorHealthCommand(kDefaultCDDProcessorHealthCommand),
    m_CDDHealthTimeoutSec(kDefaultCDDHealthTimeoutSec),
    m_WGSProcessorsEnabled(kDefaultWGSProcessorsEnabled),
    m_WGSProcessorHealthCommand(kDefaultWGSProcessorHealthCommand),
    m_WGSHealthTimeoutSec(kDefaultWGSHealthTimeoutSec),
    m_SNPProcessorsEnabled(kDefaultSNPProcessorsEnabled),
    m_SNPProcessorHealthCommand(kDefaultSNPProcessorHealthCommand),
    m_SNPHealthTimeoutSec(kDefaultSNPHealthTimeoutSec),
    m_MyNCBIOKCacheSize(kDefaultMyNCBIOKCacheSize),
    m_MyNCBINotFoundCacheSize(kDefaultMyNCBINotFoundCacheSize),
    m_MyNCBINotFoundCacheExpirationSec(kDefaultMyNCBINotFoundCacheExpirationSec),
    m_MyNCBIErrorCacheSize(kDefaultMyNCBIErrorCacheSize),
    m_MyNCBIErrorCacheBackOffMs(kDefaultMyNCBIErrorCacheBackOffMs),
    m_MyNCBIURL(kDefaultMyNCBIURL),
    m_MyNCBIHttpProxy(kDefaultMyNCBIHttpProxy),
    m_MyNCBITimeoutMs(kDefaultMyNCBITimeoutMs),
    m_MyNCBIResolveTimeoutMs(kDefaultMyNCBIResolveTimeoutMs),
    m_MyNCBIDnsResolveOkPeriodSec(kDefaultMyNCBIDnsResolveOkPeriodSec),
    m_MyNCBIDnsResolveFailPeriodSec(kDefaultMyNCBIDnsResolveFailPeriodSec),
    m_MyNCBITestWebCubbyUser(kDefaultMyNCBITestWebCubbyUser),
    m_MyNCBITestOkPeriodSec(kDefaultMyNCBITestOkPeriodSec),
    m_MyNCBITestFailPeriodSec(kDefaultMyNCBITestFailPeriodSec),
    m_LogPeerIPAlways(kDefaultLogPeerIPAlways),
    m_IdleTimeoutSec(kDefaultIdleTimeoutSec)
{}


SPubseqGatewaySettings::~SPubseqGatewaySettings()
{}


void SPubseqGatewaySettings::Read(const CNcbiRegistry &   registry)
{
    // Note: the Validate() method will use errors/warnings containers to
    // produce a log message and set an alert if needed
    m_CriticalErrors.clear();
    m_Errors.clear();
    m_Warnings.clear();

    // Note: reading of some values in the [SERVER] depends if SSL is on/off
    //       So, reading of the SSL settings is done first
    x_ReadSSLSection(registry);

    x_ReadServerSection(registry);
    x_ReadStatisticsSection(registry);
    x_ReadLmdbCacheSection(registry);
    x_ReadAutoExcludeSection(registry);
    x_ReadIPGSection(registry);
    x_ReadDebugSection(registry);
    x_ReadHealthSection(registry);
    x_ReadAdminSection(registry);

    x_ReadCassandraProcessorSection(registry);  // Must be after x_ReadHealthSection()
    x_ReadLMDBProcessorSection(registry);       // Must be after x_ReadHealthSection()
    x_ReadCDDProcessorSection(registry);        // Must be after x_ReadHealthSection()
    x_ReadWGSProcessorSection(registry);        // Must be after x_ReadHealthSection()
    x_ReadSNPProcessorSection(registry);        // Must be after x_ReadHealthSection()

    x_ReadMyNCBISection(registry);
    x_ReadCountersSection(registry);

    x_ReadLogSection(registry);
    x_ReadH2OSection(registry);
}


void SPubseqGatewaySettings::x_ReadServerSection(const CNcbiRegistry &   registry)
{
    if (!registry.HasEntry(kServerSection, "port"))
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[" + kServerSection +
                   "]/port value is not found in the configuration "
                   "file. The port must be provided to run the server. "
                   "Exiting.");

    m_HttpPort = registry.GetInt(kServerSection, "port", 0);
    m_HttpWorkers = registry.GetInt(kServerSection, "workers",
                                    kWorkersDefault);
    m_ListenerBacklog = registry.GetInt(kServerSection, "backlog",
                                        kListenerBacklogDefault);
    m_TcpMaxConn = registry.GetInt(kServerSection, "maxconn",
                                   kTcpMaxConnDefault);
    if (x_IsPercentValue(registry, kServerSection, "maxconnsoftlimit")) {
        // Up to date: percentage value
        double      val = x_GetPercentValue(registry, kServerSection,
                                            "maxconnsoftlimit",
                                            kDefaultTcpMaxConnSoftLimitPercent);
        m_TcpMaxConnSoftLimit = round(double(m_TcpMaxConn) * val / 100.0);
    } else {
        // Obsolete: absolute value
        m_TcpMaxConnSoftLimit = registry.GetInt(kServerSection, "maxconnsoftlimit",
                                                kTcpMaxConnSoftLimitDefault);
    }
    if (x_IsPercentValue(registry, kServerSection, "maxconnalertlimit")) {
        // Up to date: percentage value
        double      val = x_GetPercentValue(registry, kServerSection,
                                            "maxconnalertlimit",
                                            kDefaultTcpMaxConnAlertLimitPercent);
        m_TcpMaxConnAlertLimit = round(double(m_TcpMaxConnSoftLimit) * val / 100.0);
    } else {
        // Obsolete: absolute value
        m_TcpMaxConnAlertLimit = registry.GetInt(kServerSection, "maxconnalertlimit",
                                                 kTcpMaxConnAlertLimitDefault);
    }
    m_TimeoutMs = registry.GetInt(kServerSection, "optimeout",
                                  kTimeoutDefault);
    m_MaxRetries = registry.GetInt(kServerSection, "maxretries",
                                   kMaxRetriesDefault);
    m_RootKeyspace = registry.GetString(kServerSection, "root_keyspace",
                                        kDefaultRootKeyspace);
    m_ConfigurationDomain = registry.GetString(kServerSection, "configuration_domain",
                                               kDefaultConfigurationDomain);
    m_HttpMaxBacklog = registry.GetInt(kServerSection, "http_max_backlog",
                                       kDefaultHttpMaxBacklog);
    m_HttpMaxRunning = registry.GetInt(kServerSection, "http_max_running",
                                       kDefaultHttpMaxRunning);
    m_LogSamplingRatio = registry.GetInt(kServerSection, "log_sampling_ratio",
                                         kDefaultLogSamplingRatio);
    m_LogTimingThreshold = registry.GetInt(kServerSection, "log_timing_threshold",
                                           kDefaultLogTimingThreshold);

    double  percent_val;
    percent_val = x_GetPercentValue(registry, kServerSection,
                                    "conn_throttle_threshold",
                                    kDefaultConnThrottleThresholdPercent);
    m_ConnThrottleThreshold = round(double(m_TcpMaxConnSoftLimit) * percent_val / 100.0);

    percent_val = x_GetPercentValue(registry, kServerSection,
                                    "conn_throttle_by_host",
                                    kDefaultConnThrottleByHostPercent);
    m_ConnThrottleByHost = round(double(m_ConnThrottleThreshold) * percent_val / 100.0);

    percent_val = x_GetPercentValue(registry, kServerSection,
                                    "conn_throttle_by_site",
                                    kDefaultConnThrottleBySitePercent);
    m_ConnThrottleBySite = round(double(m_ConnThrottleThreshold) * percent_val / 100.0);

    percent_val = x_GetPercentValue(registry, kServerSection,
                                    "conn_throttle_by_process",
                                    kDefaultConnThrottleByProcessPercent);
    m_ConnThrottleByProcess = round(double(m_ConnThrottleThreshold) * percent_val / 100.0);

    percent_val = x_GetPercentValue(registry, kServerSection,
                                    "conn_throttle_by_user_agent",
                                    kDefaultConnThrottleByUserAgentPercent);
    m_ConnThrottleByUserAgent = round(double(m_ConnThrottleThreshold) * percent_val / 100.0);


    m_ConnThrottleCloseIdleSec = registry.GetDouble(kServerSection, "conn_throttle_close_idle",
                                                    kDefaultConnThrottleCloseIdleSec);
    m_ConnForceCloseWaitSec = registry.GetDouble(kServerSection, "conn_force_close_wait",
                                                 kDefaultConnForceCloseWaitSec);
    m_ThrottlingDataValidSec = registry.GetDouble(kServerSection, "conn_throttle_data_valid",
                                                  kDefaultThrottlingDataValidSec);
    m_SendBlobIfSmall = x_GetDataSize(registry, kServerSection,
                                      "send_blob_if_small",
                                      kDefaultSendBlobIfSmall);
    m_Log = registry.GetBool(kServerSection, "log", kDefaultLog);
    m_MaxHops = registry.GetInt(kServerSection, "max_hops", kDefaultMaxHops);
    m_ResendTimeoutSec = registry.GetDouble(kServerSection, "resend_timeout",
                                            kDefaultResendTimeoutSec);
    m_RequestTimeoutSec = registry.GetDouble(kServerSection, "request_timeout",
                                             kDefaultRequestTimeoutSec);
    m_ProcessorMaxConcurrency = registry.GetInt(kServerSection,
                                                "ProcessorMaxConcurrency",
                                                kDefaultProcessorMaxConcurrency);
    m_SplitInfoBlobCacheSize = registry.GetInt(kServerSection,
                                               "split_info_blob_cache_size",
                                               kDefaultSplitInfoBlobCacheSize);

    if (m_SSLEnable) {
        m_ShutdownIfTooManyOpenFD =
                registry.GetInt(kServerSection, "ShutdownIfTooManyOpenFD",
                                kDefaultShutdownIfTooManyOpenFDforHTTPS);
    } else {
        m_ShutdownIfTooManyOpenFD =
                registry.GetInt(kServerSection, "ShutdownIfTooManyOpenFD",
                                kDefaultShutdownIfTooManyOpenFDforHTTP);
    }
}


void SPubseqGatewaySettings::x_ReadStatisticsSection(const CNcbiRegistry &   registry)
{
    m_SmallBlobSize = x_GetDataSize(registry, kStatisticsSection,
                                    "small_blob_size", kDefaultSmallBlobSize);
    m_MinStatValue = registry.GetInt(kStatisticsSection,
                                     "min", kMinStatValue);
    m_MaxStatValue = registry.GetInt(kStatisticsSection,
                                     "max", kMaxStatValue);
    m_NStatBins = registry.GetInt(kStatisticsSection,
                                  "n_bins", kNStatBins);
    m_StatScaleType = registry.GetString(kStatisticsSection,
                                         "type", kStatScaleType);
    m_TickSpan = registry.GetInt(kStatisticsSection,
                                 "tick_span", kTickSpan);
    m_OnlyForProcessor = registry.GetString(kStatisticsSection,
                                            "only_for_processor",
                                            kDefaultOnlyForProcessor);
}


void SPubseqGatewaySettings::x_ReadLmdbCacheSection(const CNcbiRegistry &   registry)
{
    m_Si2csiDbFile = registry.GetString(kLmdbCacheSection,
                                        "dbfile_si2csi", "");
    m_BioseqInfoDbFile = registry.GetString(kLmdbCacheSection,
                                            "dbfile_bioseq_info", "");
    m_BlobPropDbFile = registry.GetString(kLmdbCacheSection,
                                          "dbfile_blob_prop", "");
    m_LmdbReadAhead = registry.GetBool(kLmdbCacheSection, "read_ahead",
                                       kDefaultLmdbReadAhead);
}


void SPubseqGatewaySettings::x_ReadAutoExcludeSection(const CNcbiRegistry &   registry)
{
    m_ExcludeCacheMaxSize = registry.GetInt(
                            kAutoExcludeSection, "max_cache_size",
                            kDefaultExcludeCacheMaxSize);
    m_ExcludeCachePurgePercentage = registry.GetInt(
                            kAutoExcludeSection, "purge_percentage",
                            kDefaultExcludeCachePurgePercentage);
    m_ExcludeCacheInactivityPurge = registry.GetInt(
                            kAutoExcludeSection, "inactivity_purge_timeout",
                            kDefaultExcludeCacheInactivityPurge);
}


void SPubseqGatewaySettings::x_ReadDebugSection(const CNcbiRegistry &   registry)
{
    m_AllowIOTest = registry.GetBool(kDebugSection, "psg_allow_io_test",
                                     kDefaultAllowIOTest);
    m_AllowProcessorTiming = registry.GetBool(kDebugSection, "allow_processor_timing",
                                              kDefaultAllowProcessorTiming);
}


void SPubseqGatewaySettings::x_ReadIPGSection(const CNcbiRegistry &   registry)
{
    m_IPGPageSize = registry.GetInt(kIPGSection, "page_size",
                                    kDefaultIPGPageSize);
    m_EnableHugeIPG = registry.GetBool(kIPGSection, "enable_huge_ipg",
                                       kDefaultEnableHugeIPG);
}


void SPubseqGatewaySettings::x_ReadSSLSection(const CNcbiRegistry &   registry)
{
    m_SSLEnable = registry.GetBool(kSSLSection,
                                   "ssl_enable", kDefaultSSLEnable);
    m_SSLCertFile = registry.GetString(kSSLSection,
                                       "ssl_cert_file", kDefaultSSLCertFile);
    m_SSLKeyFile = registry.GetString(kSSLSection,
                                      "ssl_key_file", kDefaultSSLKeyFile);
    m_SSLCiphers = registry.GetString(kSSLSection,
                                      "ssl_ciphers", kDefaultSSLCiphers);
}


void SPubseqGatewaySettings::x_ReadHealthSection(const CNcbiRegistry &   registry)
{
    // Read a list of critical backends
    string      critical_data_sources = registry.GetString(kHealthSection,
                                                           "critical_data_sources",
                                                           kDefaultCriticalDataSources);
    NStr::Split(critical_data_sources, ", ", m_CriticalDataSources,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    if (!m_CriticalDataSources.empty()) {
        for (size_t k = 0; k < m_CriticalDataSources.size(); ++k) {
            transform(m_CriticalDataSources[k].begin(), m_CriticalDataSources[k].end(),
                      m_CriticalDataSources[k].begin(), ::tolower);
        }
    }

    // Remove duplicates
    sort(m_CriticalDataSources.begin(), m_CriticalDataSources.end());
    m_CriticalDataSources.erase(unique(m_CriticalDataSources.begin(),
                                       m_CriticalDataSources.end()),
                                m_CriticalDataSources.end());

    // Sanity check
    if (m_CriticalDataSources.size() == 1) {
        if (m_CriticalDataSources[0] == "none") {
            // Special case: only one item and this item is "none"
            m_CriticalDataSources.clear();
        }
    } else {
        vector<string>::iterator    none_it = find(m_CriticalDataSources.begin(),
                                                   m_CriticalDataSources.end(),
                                                   "none");
        if (none_it != m_CriticalDataSources.end()) {
            m_CriticalDataSources.erase(none_it);
            m_Warnings.push_back("The [HEALTH]/critical_data_sources value "
                                 "has NONE together with other items. "
                                 "The NONE item is ignored.");
        }
    }

    m_HealthTimeoutSec = registry.GetDouble(kHealthSection, "timeout",
                                            kDefaultHealthTimeout);
    if (m_HealthTimeoutSec <= 0) {
        m_CriticalDataSources.push_back(
            "The [HEALTH]/timeout value is out of range. It must be > 0. "
            "Received: " + to_string(m_HealthTimeoutSec) + ". Resetting to " +
            to_string(kDefaultHealthTimeout));
        m_HealthTimeoutSec = kDefaultHealthTimeout;
    }
}


void SPubseqGatewaySettings::x_ReadCassandraProcessorSection(const CNcbiRegistry &   registry)
{
    m_CassandraProcessorsEnabled =
            registry.GetBool(kCassandraProcessorSection, "enabled",
                             kDefaultCassandraProcessorsEnabled);
    m_CassandraProcessorHealthCommand =
            registry.GetString(kCassandraProcessorSection, "health_command",
                               kDefaultCassandraProcessorHealthCommand);

    m_CassandraHealthTimeoutSec =
            registry.GetDouble(kCassandraProcessorSection, "health_timeout",
                               kDefaultCassandraHealthTimeoutSec);
    if (m_CassandraHealthTimeoutSec == 0.0) {
        // Valid configuration: fall back to the default value from
        // the [HEALTH] section
        m_CassandraHealthTimeoutSec = m_HealthTimeoutSec;
    } else if (m_CassandraHealthTimeoutSec < 0) {
        // Error
        m_CriticalDataSources.push_back(
            "The [CASSANDRA]/health_timeout value is out of range. It must be >= 0. "
            "Received: " + to_string(m_CassandraHealthTimeoutSec) + ". Resetting to "
            "default from [HEALTH]/timeout: " + to_string(m_HealthTimeoutSec));
        m_CassandraHealthTimeoutSec = kDefaultHealthTimeout;
    }

    m_SeqIdResolveAlways =
        registry.GetBool(kCassandraProcessorSection, "seq_id_resolve_always",
                         kDefaultSeqIdResolveAlways);
}


void SPubseqGatewaySettings::x_ReadLMDBProcessorSection(const CNcbiRegistry &   registry)
{
    m_LMDBProcessorHealthCommand =
            registry.GetString(kLMDBProcessorSection, "health_command",
                               kDefaultLMDBProcessorHealthCommand);

    m_LMDBHealthTimeoutSec =
            registry.GetDouble(kLMDBProcessorSection, "health_timeout",
                               kDefaultLMDBHealthTimeoutSec);
    if (m_LMDBHealthTimeoutSec == 0.0) {
        // Valid configuration: fall back to the default value from
        // the [HEALTH] section
        m_LMDBHealthTimeoutSec = m_HealthTimeoutSec;
    } else if (m_LMDBHealthTimeoutSec < 0) {
        // Error
        m_CriticalErrors.push_back(
            "The LMDB health timeout is out of range. It must be >= 0. "
            "Received: " + to_string(m_LMDBHealthTimeoutSec) + ". Resetting to "
            "default from [HEALTH]/timeout: " + to_string(m_HealthTimeoutSec));
        m_LMDBHealthTimeoutSec = kDefaultHealthTimeout;
    }
}


void SPubseqGatewaySettings::x_ReadCDDProcessorSection(const CNcbiRegistry &   registry)
{
    m_CDDProcessorsEnabled = registry.GetBool(kCDDProcessorSection,
                                              "enabled",
                                              kDefaultCDDProcessorsEnabled);
    m_CDDProcessorHealthCommand =
            registry.GetString(kCDDProcessorSection, "health_command",
                               kDefaultCDDProcessorHealthCommand);

    m_CDDHealthTimeoutSec =
            registry.GetDouble(kCDDProcessorSection, "health_timeout",
                               kDefaultCDDHealthTimeoutSec);
    if (m_CDDHealthTimeoutSec == 0.0) {
        // Valid configuration: fall back to the default value from
        // the [HEALTH] section
        m_CDDHealthTimeoutSec = m_HealthTimeoutSec;
    } else if (m_CDDHealthTimeoutSec < 0) {
        // Error
        m_CriticalErrors.push_back(
            "The CDD health timeout is out of range. It must be >= 0. "
            "Received: " + to_string(m_CDDHealthTimeoutSec) + ". Resetting to "
            "default from [HEALTH]/timeout: " + to_string(m_HealthTimeoutSec));
        m_CDDHealthTimeoutSec = kDefaultHealthTimeout;
    }
}


void SPubseqGatewaySettings::x_ReadWGSProcessorSection(const CNcbiRegistry &   registry)
{
    m_WGSProcessorsEnabled = registry.GetBool(kWGSProcessorSection,
                                              "enabled",
                                              kDefaultWGSProcessorsEnabled);
    m_WGSProcessorHealthCommand =
            registry.GetString(kWGSProcessorSection, "health_command",
                               kDefaultWGSProcessorHealthCommand);

    m_WGSHealthTimeoutSec =
            registry.GetDouble(kWGSProcessorSection, "health_timeout",
                               kDefaultWGSHealthTimeoutSec);
    if (m_WGSHealthTimeoutSec == 0.0) {
        // Valid configuration: fall back to the default value from
        // the [HEALTH] section
        m_WGSHealthTimeoutSec = m_HealthTimeoutSec;
    } else if (m_WGSHealthTimeoutSec < 0) {
        // Error
        m_CriticalErrors.push_back(
            "The WGS health timeout is out of range. It must be >= 0. "
            "Received: " + to_string(m_WGSHealthTimeoutSec) + ". Resetting to "
            "default from [HEALTH]/timeout: " +
            to_string(m_HealthTimeoutSec));
        m_WGSHealthTimeoutSec = kDefaultHealthTimeout;
    }
}


void SPubseqGatewaySettings::x_ReadSNPProcessorSection(const CNcbiRegistry &   registry)
{
    m_SNPProcessorsEnabled = registry.GetBool(kSNPProcessorSection,
                                              "enabled",
                                              kDefaultSNPProcessorsEnabled);
    m_SNPProcessorHealthCommand =
            registry.GetString(kSNPProcessorSection, "health_command",
                               kDefaultSNPProcessorHealthCommand);

    m_SNPHealthTimeoutSec =
            registry.GetDouble(kSNPProcessorSection, "health_timeout",
                               kDefaultSNPHealthTimeoutSec);
    if (m_SNPHealthTimeoutSec == 0.0) {
        // Valid configuration: fall back to the default value from
        // the [HEALTH] section
        m_SNPHealthTimeoutSec = m_HealthTimeoutSec;
    } else if (m_SNPHealthTimeoutSec < 0) {
        // Error
        m_CriticalErrors.push_back(
            "The SNP health timeout is out of range. It must be >= 0. "
            "Received: " + to_string(m_SNPHealthTimeoutSec) + ". Resetting to "
            "default from [HEALTH]/timeout: " +
            to_string(m_HealthTimeoutSec));
        m_SNPHealthTimeoutSec = kDefaultHealthTimeout;
    }
}


void SPubseqGatewaySettings::x_ReadMyNCBISection(const CNcbiRegistry &   registry)
{
    m_MyNCBIOKCacheSize = registry.GetInt(kMyNCBISection,
                                          "ok_cache_size",
                                          kDefaultMyNCBIOKCacheSize);
    m_MyNCBINotFoundCacheSize = registry.GetInt(kMyNCBISection,
                                                "not_found_cache_size",
                                                kDefaultMyNCBINotFoundCacheSize);
    m_MyNCBINotFoundCacheExpirationSec = registry.GetInt(
            kMyNCBISection, "not_found_cache_expiration_sec",
            kDefaultMyNCBINotFoundCacheExpirationSec);
    m_MyNCBIErrorCacheSize = registry.GetInt(kMyNCBISection,
                                             "error_cache_size",
                                             kDefaultMyNCBIErrorCacheSize);
    m_MyNCBIErrorCacheBackOffMs = registry.GetInt(kMyNCBISection,
                                                  "error_cache_back_off_ms",
                                                  kDefaultMyNCBIErrorCacheBackOffMs);
    m_MyNCBIURL = registry.GetString(kMyNCBISection,
                                     "url",
                                     kDefaultMyNCBIURL);
    m_MyNCBIHttpProxy = registry.GetString(kMyNCBISection,
                                           "http_proxy",
                                           kDefaultMyNCBIHttpProxy);
    m_MyNCBITimeoutMs = registry.GetInt(kMyNCBISection,
                                        "timeout_ms",
                                        kDefaultMyNCBITimeoutMs);
    m_MyNCBIResolveTimeoutMs = registry.GetInt(kMyNCBISection,
                                               "resolve_timeout_ms",
                                               kDefaultMyNCBIResolveTimeoutMs);
    m_MyNCBIDnsResolveOkPeriodSec = registry.GetInt(kMyNCBISection,
                                                    "dns_resolve_ok_period_sec",
                                                    kDefaultMyNCBIDnsResolveOkPeriodSec);
    m_MyNCBIDnsResolveFailPeriodSec = registry.GetInt(kMyNCBISection,
                                                      "dns_resolve_fail_period_sec",
                                                      kDefaultMyNCBIDnsResolveFailPeriodSec);
    m_MyNCBITestWebCubbyUser = registry.GetString(kMyNCBISection,
                                                  "test_web_cubby_user",
                                                  kDefaultMyNCBITestWebCubbyUser);
    m_MyNCBITestOkPeriodSec = registry.GetInt(kMyNCBISection,
                                              "test_ok_period_sec",
                                              kDefaultMyNCBITestOkPeriodSec);
    m_MyNCBITestFailPeriodSec = registry.GetInt(kMyNCBISection,
                                                "test_fail_period_sec",
                                                kDefaultMyNCBITestFailPeriodSec);
}


void SPubseqGatewaySettings::x_ReadCountersSection(const CNcbiRegistry &   registry)
{
    list<string>            entries;
    registry.EnumerateEntries(kCountersSection, &entries);

    for(const auto &  value_id : entries) {
        string      name_and_description = registry.Get(kCountersSection,
                                                        value_id);
        string      name;
        string      description;
        if (NStr::SplitInTwo(name_and_description, ":::", name, description,
                             NStr::fSplit_ByPattern)) {
            m_IdToNameAndDescription[value_id] = {name, description};
        } else {
            m_CriticalErrors.push_back(
                    "Malformed counter [" + kCountersSection + "]/" +
                    name_and_description +
                    " information. Expected <name>:::<description");
        }
    }
}


void SPubseqGatewaySettings::x_ReadAdminSection(const CNcbiRegistry &   registry)
{
    try {
        m_AuthToken = registry.GetEncryptedString(kAdminSection, "auth_token",
                                                  IRegistry::fPlaintextAllowed);
    } catch (const CRegistryException &  ex) {
        m_CriticalErrors.push_back(
                "Decrypting error detected while reading "
                "[" + kAdminSection + "]/auth_token value: " +
                string(ex.what()));

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    } catch (...) {
        m_CriticalErrors.push_back(
                "Unknown decrypting error detected while reading "
                "[" + kAdminSection + "]/auth_token value");

        // Treat the value as a clear text
        m_AuthToken = registry.GetString("ADMIN", "auth_token",
                                         kDefaultAuthToken);
    }

    string  auth_commands = registry.GetString(kAdminSection, "auth_commands",
                                               kDefaultAuthCommands);
    NStr::Split(auth_commands, " ", m_AuthCommands);
    if (!m_AuthCommands.empty()) {
        for (size_t k = 0; k < m_AuthCommands.size(); ++k) {
            transform(m_AuthCommands[k].begin(), m_AuthCommands[k].end(),
                      m_AuthCommands[k].begin(), ::tolower);
        }
    }
}


void SPubseqGatewaySettings::x_ReadLogSection(const CNcbiRegistry &   registry)
{
    m_LogPeerIPAlways = registry.GetBool(kLogSection, "log_peer_ip_always",
                                         kDefaultLogPeerIPAlways);
}


void SPubseqGatewaySettings::x_ReadH2OSection(const CNcbiRegistry &   registry)
{
    m_IdleTimeoutSec = registry.GetInt(kH2OSection, "idle_timeout",
                                       kDefaultIdleTimeoutSec);
}


void SPubseqGatewaySettings::Validate(CPSGAlerts &  alerts)
{
    x_ValidateServerSection();
    x_ValidateStatisticsSection();
    x_ValidateLmdbCacheSection();
    x_ValidateAutoExcludeSection();
    x_ValidateDebugSection();
    x_ValidateIPGSection();
    x_ValidateAdminSection();
    x_ValidateSSLSection();
    x_ValidateHealthSection();
    x_ValidateCassandraProcessorSection();
    x_ValidateLMDBProcessorSection();
    x_ValidateCDDProcessorSection();
    x_ValidateWGSProcessorSection();
    x_ValidateSNPProcessorSection();
    x_ValidateMyNCBISection();
    x_ValidateCountersSection();
    x_ValidateLogSection();
    x_ValidateH2OSection();

    string      combined_msg;
    if (!m_CriticalErrors.empty()) {
        string      combined_critical_errors;
        for (const string &  item : m_CriticalErrors) {
            if (!combined_critical_errors.empty()) {
                combined_critical_errors += "\n";
            }
            combined_critical_errors += "- " + item;
        }
        combined_msg = "The following critical errors were found "
                       "while reading settings:\n" + combined_critical_errors;
        PSG_CRITICAL(combined_msg);
    }

    if (!m_Errors.empty()) {
        string      combined_errors;
        for (const string &  item : m_Errors) {
            if (!combined_errors.empty()) {
                combined_errors += "\n";
            }
            combined_errors += "- " + item;
        }
        string      errors = "The following errors were found "
                             "while reading settings:\n" + combined_errors;
        PSG_ERROR(errors);

        if (!combined_msg.empty()) {
            combined_msg += "\n";
        }
        combined_msg += errors;
    }

    if (!m_Warnings.empty()) {
        string      combined_warnings;
        for (const string &  item : m_Warnings) {
            if (!combined_warnings.empty()) {
                combined_warnings += "\n";
            }
            combined_warnings += "- " + item;
        }
        string      warns = "The following warnings were found "
                            "while reading settings:\n" + combined_warnings;
        PSG_WARNING(warns);

        if (!combined_msg.empty()) {
            combined_msg += "\n";
        }
        combined_msg += warns;
    }

    if (!combined_msg.empty()) {
        alerts.Register(ePSGS_ConfigProblems, combined_msg);
    }
}


const unsigned short    kHttpPortMin = 1;
const unsigned short    kHttpPortMax = 65534;
const unsigned short    kWorkersMin = 1;
const unsigned short    kWorkersMax = 100;
const unsigned int      kListenerBacklogMin = 5;
const unsigned int      kListenerBacklogMax = 2048;
const unsigned short    kTcpMaxConnMax = 65000;
const unsigned short    kTcpMaxConnMin = 5;
const unsigned int      kTimeoutMsMin = 0;
const unsigned int      kTimeoutMsMax = UINT_MAX;
const unsigned int      kMaxRetriesMin = 0;
const unsigned int      kMaxRetriesMax = UINT_MAX;


void SPubseqGatewaySettings::x_ValidateServerSection(void)
{
    if (m_HttpPort < kHttpPortMin || m_HttpPort > kHttpPortMax) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "[" + kServerSection +
                   "]/port value is out of range. Allowed range: " +
                   to_string(kHttpPortMin) + "..." +
                   to_string(kHttpPortMax) + ". Received: " +
                   to_string(m_HttpPort));
    }

    if (m_HttpWorkers < kWorkersMin || m_HttpWorkers > kWorkersMax) {
        m_CriticalErrors.push_back(
            "The number of HTTP workers is out of range. Allowed "
            "range: " + to_string(kWorkersMin) + "..." +
            to_string(kWorkersMax) + ". Received: " +
            to_string(m_HttpWorkers) + ". Resetting to " +
            to_string(kWorkersDefault));
        m_HttpWorkers = kWorkersDefault;
    }

    if (m_ListenerBacklog < kListenerBacklogMin ||
        m_ListenerBacklog > kListenerBacklogMax) {
        m_CriticalErrors.push_back(
            "The listener backlog is out of range. Allowed "
            "range: " + to_string(kListenerBacklogMin) + "..." +
            to_string(kListenerBacklogMax) + ". Received: " +
            to_string(m_ListenerBacklog) + ". Resetting to " +
            to_string(kListenerBacklogDefault));
        m_ListenerBacklog = kListenerBacklogDefault;
    }

    if (m_ConnThrottleCloseIdleSec < 0) {
        m_CriticalErrors.push_back("Invalid [SERVER]/conn_throttle_close_idle value. "
                                   "It must be >= 0. Resetting to " +
                                   to_string(kDefaultConnThrottleCloseIdleSec));
        m_ConnThrottleCloseIdleSec = kDefaultConnThrottleCloseIdleSec;
    }

    if (m_ConnForceCloseWaitSec < 0) {
        m_CriticalErrors.push_back("Invalid [SERVER]/conn_force_close_wait value. "
                                   "It must be >= 0. Resetting to " +
                                   to_string(kDefaultConnForceCloseWaitSec));
        m_ConnForceCloseWaitSec = kDefaultConnForceCloseWaitSec;
    }

    if (m_ThrottlingDataValidSec <= 0) {
        m_CriticalErrors.push_back("Invalid [SERVER]/conn_throttle_data_valid value. "
                                   "It must be > 0. Resetting to " +
                                   to_string(kDefaultThrottlingDataValidSec));
        m_ThrottlingDataValidSec = kDefaultThrottlingDataValidSec;
    }

    bool    connection_config_good = true;
    if (m_TcpMaxConn < kTcpMaxConnMin || m_TcpMaxConn > kTcpMaxConnMax) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "The max number of connections is out of range. Allowed "
            "range: " + to_string(kTcpMaxConnMin) + "..." +
            to_string(kTcpMaxConnMax) + ". Received: " +
            to_string(m_TcpMaxConn) +
            ". Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_TcpMaxConnSoftLimit == 0) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Invalid [SERVER]/maxconnsoftlimit value. It must be > 0. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_TcpMaxConn < m_TcpMaxConnSoftLimit) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/maxconn value. "
            "It must be >= [SERVER]/maxconnsoftlimit value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_TcpMaxConnAlertLimit == 0) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Invalid [SERVER]/maxconnalertlimit value. "
            "It must be > 0. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_TcpMaxConnSoftLimit < m_TcpMaxConnAlertLimit) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/maxconnalertlimit value. "
            "It must be <= [SERVER]/maxconnsoftlimit value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good &&
        m_ConnThrottleThreshold > static_cast<size_t>(m_TcpMaxConnSoftLimit)) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/conn_throttle_threshold value. It must be "
            "<= [SERVER]/maxconnsoftlimit value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_ConnThrottleByHost > m_ConnThrottleThreshold) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/conn_throttle_by_host value. It must be "
            "<= [SERVER]/conn_throttle_threshold value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_ConnThrottleBySite > m_ConnThrottleThreshold) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/conn_throttle_by_site value. It must be "
            "<= [SERVER]/conn_throttle_threshold value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_ConnThrottleByProcess > m_ConnThrottleThreshold) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/conn_throttle_by_process value. It must be "
            "<= [SERVER]/conn_throttle_threshold value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (connection_config_good && m_ConnThrottleByUserAgent > m_ConnThrottleThreshold) {
        connection_config_good = false;
        m_CriticalErrors.push_back(
            "Inconsistent [SERVER]/conn_throttle_by_user_agent value. It must be "
            "<= [SERVER]/conn_throttle_threshold value. "
            "Resetting all connection and throttling settings to default.");
        x_ResetConnectionSettingsToDefault();
    }

    if (m_TimeoutMs < kTimeoutMsMin || m_TimeoutMs > kTimeoutMsMax) {
        m_CriticalErrors.push_back(
            "The operation timeout is out of range. Allowed "
            "range: " + to_string(kTimeoutMsMin) + "..." +
            to_string(kTimeoutMsMax) + ". Received: " +
            to_string(m_TimeoutMs) + ". Resetting to " +
            to_string(kTimeoutDefault));
        m_TimeoutMs = kTimeoutDefault;
    }

    if (m_MaxRetries < kMaxRetriesMin || m_MaxRetries > kMaxRetriesMax) {
        m_CriticalErrors.push_back(
            "The max retries is out of range. Allowed "
            "range: " + to_string(kMaxRetriesMin) + "..." +
            to_string(kMaxRetriesMax) + ". Received: " +
            to_string(m_MaxRetries) + ". Resetting to " +
            to_string(kMaxRetriesDefault));
        m_MaxRetries = kMaxRetriesDefault;
    }

    if (m_HttpMaxBacklog <= 0) {
        m_CriticalErrors.push_back(
            "Invalid " + kServerSection + "]/http_max_backlog value (" +
            to_string(m_HttpMaxBacklog) + "). "
            "The http max backlog must be > 0. Resetting to " +
            to_string(kDefaultHttpMaxBacklog));
        m_HttpMaxBacklog = kDefaultHttpMaxBacklog;
    }

    if (m_HttpMaxRunning <= 0) {
        m_CriticalErrors.push_back(
            "Invalid " + kServerSection + "]/http_max_running value (" +
            to_string(m_HttpMaxRunning) + "). "
            "The http max running must be > 0. Resetting to " +
            to_string(kDefaultHttpMaxRunning));
        m_HttpMaxRunning = kDefaultHttpMaxRunning;
    }

    if (m_LogSamplingRatio < 0) {
        m_CriticalErrors.push_back(
            "Invalid " + kServerSection + "]/log_sampling_ratio value (" +
            to_string(m_LogSamplingRatio) + "). "
            "The log sampling ratio must be >= 0. Resetting to " +
            to_string(kDefaultLogSamplingRatio));
        m_LogSamplingRatio = kDefaultLogSamplingRatio;
    }

    if (m_LogTimingThreshold < 0) {
        m_CriticalErrors.push_back(
            "Invalid " + kServerSection + "]/log_timing_threshold value (" +
            to_string(m_LogTimingThreshold) + "). "
            "The log timing threshold must be >= 0. Resetting to " +
            to_string(kDefaultLogTimingThreshold));
        m_LogTimingThreshold = kDefaultLogTimingThreshold;
    }

    if (m_MaxHops <= 0) {
        m_CriticalErrors.push_back(
            "Invalid " + kServerSection + "]/max_hops value (" +
            to_string(m_MaxHops) + "). "
            "The max hops must be > 0. Resetting to " +
            to_string(kDefaultMaxHops));
        m_MaxHops = kDefaultMaxHops;
    }

    if (m_ResendTimeoutSec < 0.0) {
        m_CriticalErrors.push_back(
            "Invalid [" + kServerSection + "]/resend_timeout value (" +
            to_string(m_ResendTimeoutSec) + "). "
            "The resend timeout must be >= 0. Resetting to " +
            to_string(kDefaultResendTimeoutSec));
        m_ResendTimeoutSec = kDefaultResendTimeoutSec;
    }

    if (m_RequestTimeoutSec <= 0.0) {
        m_CriticalErrors.push_back(
            "Invalid [" + kServerSection + "]/request_timeout value (" +
            to_string(m_RequestTimeoutSec) + "). "
            "The request timeout must be > 0. Resetting to " +
            to_string(kDefaultRequestTimeoutSec));
        m_RequestTimeoutSec = kDefaultRequestTimeoutSec;
    }

    if (m_ProcessorMaxConcurrency == 0) {
        m_CriticalErrors.push_back(
            "Invalid [" + kServerSection +
            "]/ProcessorMaxConcurrency value (" +
            to_string(m_ProcessorMaxConcurrency) + "). "
            "The processor max concurrency must be > 0. "
            "Resetting to " + to_string(kDefaultProcessorMaxConcurrency));
        m_RequestTimeoutSec = kDefaultProcessorMaxConcurrency;
    }
}


void SPubseqGatewaySettings::x_ValidateStatisticsSection(void)
{
    bool        stat_settings_good = true;
    if (NStr::CompareNocase(m_StatScaleType, "log") != 0 &&
        NStr::CompareNocase(m_StatScaleType, "linear") != 0) {
        m_CriticalErrors.push_back(
            "Invalid [" + kStatisticsSection +
            "]/type value '" + m_StatScaleType +
            "'. Allowed values are: log, linear. "
            "The statistics parameters are reset to default values.");
        stat_settings_good = false;

        m_MinStatValue = kMinStatValue;
        m_MaxStatValue = kMaxStatValue;
        m_NStatBins = kNStatBins;
        m_StatScaleType = kStatScaleType;
    }

    if (stat_settings_good) {
        if (m_MinStatValue > m_MaxStatValue) {
            m_CriticalErrors.push_back(
                "Invalid [" + kStatisticsSection +
                "]/min and max values. The "
                "max cannot be less than min. "
                "The statistics parameters are reset to default values.");
            stat_settings_good = false;

            m_MinStatValue = kMinStatValue;
            m_MaxStatValue = kMaxStatValue;
            m_NStatBins = kNStatBins;
            m_StatScaleType = kStatScaleType;
        }
    }

    if (stat_settings_good) {
        if (m_NStatBins <= 0) {
            m_CriticalErrors.push_back(
                "Invalid [" + kStatisticsSection +
                "]/n_bins value. The "
                "number of bins must be > 0. "
                "The statistics parameters are reset to default values.");

            // Uncomment if there will be more [STATISTICS] section parameters
            // stat_settings_good = false;

            m_MinStatValue = kMinStatValue;
            m_MaxStatValue = kMaxStatValue;
            m_NStatBins = kNStatBins;
            m_StatScaleType = kStatScaleType;
        }
    }

    if (m_TickSpan <= 0) {
        m_CriticalErrors.push_back(
            "Invalid [" + kStatisticsSection + "]/tick_span value (" +
            to_string(m_TickSpan) + "). "
            "The tick span must be > 0. Resetting to " +
            to_string(kTickSpan));
        m_TickSpan = kTickSpan;
    }
}

void SPubseqGatewaySettings::x_ValidateLmdbCacheSection(void)
{
    if (m_Si2csiDbFile.empty()) {
        m_Warnings.push_back(
                "[" + kLmdbCacheSection + "]/dbfile_si2csi is not found "
                "in the ini file. No si2csi cache will be used.");
    }

    if (m_BioseqInfoDbFile.empty()) {
        m_Warnings.push_back(
                "[" + kLmdbCacheSection + "]/dbfile_bioseq_info is not found "
                "in the ini file. No bioseq_info cache will be used.");
    }

    if (m_BlobPropDbFile.empty()) {
        m_Warnings.push_back(
                "[" + kLmdbCacheSection + "]/dbfile_blob_prop is not found "
                "in the ini file. No blob_prop cache will be used.");
    }
}

void SPubseqGatewaySettings::x_ValidateAutoExcludeSection(void)
{
    if (m_ExcludeCacheMaxSize < 0) {
        m_CriticalErrors.push_back(
            "The max exclude cache size must be a positive integer. "
            "Received: " + to_string(m_ExcludeCacheMaxSize) + ". "
            "Resetting to 0 (exclude blobs cache is disabled)");
        m_ExcludeCacheMaxSize = 0;
    }

    if (m_ExcludeCachePurgePercentage < 0 || m_ExcludeCachePurgePercentage > 100) {
        string  msg = "The exclude cache purge percentage is out of range. "
                      "Allowed: 0...100. Received: " +
                      to_string(m_ExcludeCachePurgePercentage) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            m_CriticalErrors.push_back(msg + "Resetting to " +
                               to_string(kDefaultExcludeCachePurgePercentage));
        } else {
            m_Warnings.push_back(msg + "The provided value has no effect "
                                 "because the exclude cache is disabled.");
        }
        m_ExcludeCachePurgePercentage = kDefaultExcludeCachePurgePercentage;
    }

    if (m_ExcludeCacheInactivityPurge <= 0) {
        string  msg = "The exclude cache inactivity purge timeout must be > 0."
                      "Received: " +
                      to_string(m_ExcludeCacheInactivityPurge) + ". ";
        if (m_ExcludeCacheMaxSize > 0) {
            m_CriticalErrors.push_back(msg + "Resetting to " +
                               to_string(kDefaultExcludeCacheInactivityPurge));
        } else {
            m_Warnings.push_back(msg + "The provided value has no effect "
                                 "because the exclude cache is disabled.");
        }
        m_ExcludeCacheInactivityPurge = kDefaultExcludeCacheInactivityPurge;
    }
}

void SPubseqGatewaySettings::x_ValidateDebugSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateIPGSection(void)
{
    if (m_IPGPageSize <= 0) {
        m_CriticalErrors.push_back(
            "The [" + kIPGSection + "]/page_size value must be > 0. "
            "Resetting to " + to_string(kDefaultIPGPageSize));
        m_IPGPageSize = kDefaultIPGPageSize;
    }
}

void SPubseqGatewaySettings::x_ValidateAdminSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateSSLSection(void)
{
    if (m_SSLEnable) {
        if (m_SSLCertFile.empty()) {
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[" + kSSLSection + "]/ssl_cert_file value must be provided "
                       "if [" + kSSLSection + "]/ssl_enable is set to true");
        }
        if (m_SSLKeyFile.empty()) {
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[" + kSSLSection + "]/ssl_key_file value must be provided "
                       "if [" + kSSLSection + "]/ssl_enable is set to true");
        }

        if (!CFile(m_SSLCertFile).Exists()) {
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[" + kSSLSection + "]/ssl_cert_file is not found");
        }
        if (!CFile(m_SSLKeyFile).Exists()) {
            NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                       "[" + kSSLSection + "]/ssl_key_file is not found");
        }

        if (m_SSLCiphers.empty()) {
            m_SSLCiphers = kDefaultSSLCiphers;
        }
    }
}

void SPubseqGatewaySettings::x_ValidateHealthSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateCassandraProcessorSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateLMDBProcessorSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateCDDProcessorSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateWGSProcessorSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateSNPProcessorSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateMyNCBISection(void)
{
    if (m_MyNCBIURL.empty()) {
        m_CriticalErrors.push_back(
            "The [" + kMyNCBISection + "]/url value must be not empty. "
            "Resetting to " + kDefaultMyNCBIURL);
        m_MyNCBIURL = kDefaultMyNCBIURL;
    }

    if (m_MyNCBITimeoutMs <= 0) {
        m_CriticalErrors.push_back(
            "The [" + kMyNCBISection + "]/timeout_ms value must be > 0. "
            "Resetting to " + to_string(kDefaultMyNCBITimeoutMs));
        m_MyNCBITimeoutMs = kDefaultMyNCBITimeoutMs;
    }

    if (m_MyNCBIResolveTimeoutMs <= 0) {
        m_CriticalErrors.push_back(
            "The [" + kMyNCBISection + "]/resolve_timeout_ms value must be > 0. "
            "Resetting to " + to_string(kDefaultMyNCBIResolveTimeoutMs));
        m_MyNCBIResolveTimeoutMs = kDefaultMyNCBIResolveTimeoutMs;
    }
}

void SPubseqGatewaySettings::x_ValidateCountersSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateLogSection(void)
{ /* Nothing to validate so far */ }

void SPubseqGatewaySettings::x_ValidateH2OSection(void)
{ /* Nothing to validate so far */ }


double
SPubseqGatewaySettings::x_GetPercentValue(const CNcbiRegistry &  registry,
                                          const string &  section,
                                          const string &  entry,
                                          double  default_value)
{
    string  reg_value = registry.GetString(section, entry, "");
    reg_value = NStr::TruncateSpaces(reg_value);
    if (reg_value.empty()) {
        return default_value;
    }

    if (reg_value.back() != '%') {
        // The '%' character at the end is a must. Return a not set value
        m_CriticalErrors.push_back(
            "The [" + section + "]/" + entry + " value is malformed. "
            "The '%' character must be at the end. "
            "Resetting to " + to_string(default_value));
        return default_value;
    }

    // Remove the '%' character and truncate again
    reg_value.back() = ' ';
    reg_value = NStr::TruncateSpaces(reg_value);

    if (reg_value.empty()) {
        // There is nothing in front of the '%' character
        m_CriticalErrors.push_back(
            "The [" + section + "]/" + entry + " value is malformed. "
            "There is no actual value in front of the '%' character. "
            "Resetting to " + to_string(default_value));
        return default_value;
    }

    // Now actually conver string to double value
    try {
        double  val = stod(reg_value);
        if (val <= 0) {
            m_CriticalErrors.push_back(
                "The [" + section + "]/" + entry + " value is invalid. It must be > 0. "
                "Resetting to " + to_string(default_value));
            return default_value;
        }
        return val;
    } catch (...) {
        ; // Error converting, see error message below
    }

    m_CriticalErrors.push_back(
        "The [" + section + "]/" + entry + " value is malformed. "
        "Error converting it to double. "
        "Resetting to " + to_string(default_value));
    return default_value;
}


// Detects if the configuration value expressed as a percentage or not
// Default is the percentage.
bool
SPubseqGatewaySettings::x_IsPercentValue(const CNcbiRegistry &  registry,
                                         const string &  section,
                                         const string &  entry)
{
    string  reg_value = registry.GetString(section, entry, "");
    reg_value = NStr::TruncateSpaces(reg_value);
    if (reg_value.empty()) {
        return true;
    }

    return reg_value.back() == '%';
}


void SPubseqGatewaySettings::x_ResetConnectionSettingsToDefault(void)
{
    m_TcpMaxConn = kTcpMaxConnDefault;
    m_TcpMaxConnSoftLimit = round(double(m_TcpMaxConn) * kDefaultTcpMaxConnSoftLimitPercent / 100.0);
    m_TcpMaxConnAlertLimit = round(double(m_TcpMaxConnSoftLimit) * kDefaultTcpMaxConnAlertLimitPercent / 100.0);
    m_ConnThrottleThreshold = round(double(m_TcpMaxConnSoftLimit) * kDefaultConnThrottleThresholdPercent / 100.0);
    m_ConnThrottleByHost = round(double(m_ConnThrottleThreshold) * kDefaultConnThrottleByHostPercent / 100.0);
    m_ConnThrottleBySite = round(double(m_ConnThrottleThreshold) * kDefaultConnThrottleBySitePercent / 100.0);
    m_ConnThrottleByProcess = round(double(m_ConnThrottleThreshold) * kDefaultConnThrottleByProcessPercent / 100.0);
    m_ConnThrottleByUserAgent = round(double(m_ConnThrottleThreshold) * kDefaultConnThrottleByUserAgentPercent / 100.0);
}


unsigned long
SPubseqGatewaySettings::x_GetDataSize(const CNcbiRegistry &  registry,
                                      const string &  section,
                                      const string &  entry,
                                      unsigned long  default_val)
{
    CConfig                         conf(registry);
    const CConfig::TParamTree *     param_tree = conf.GetTree();
    const TPluginManagerParamTree * section_tree =
                                        param_tree->FindSubNode(section);

    if (!section_tree)
        return default_val;

    CConfig     c((CConfig::TParamTree*)section_tree, eNoOwnership);
    return c.GetDataSize("psg", entry, CConfig::eErr_NoThrow,
                         default_val);
}


size_t SPubseqGatewaySettings::GetProcessorMaxConcurrency(
                                            const CNcbiRegistry &   registry,
                                            const string &  processor_id)
{
    string                  section = processor_id + "_PROCESSOR";

    if (registry.HasEntry(section, "ProcessorMaxConcurrency")) {
        size_t      limit = registry.GetInt(section,
                                            "ProcessorMaxConcurrency",
                                            m_ProcessorMaxConcurrency);
        if (limit == 0) {
            m_CriticalErrors.push_back(
                "Invalid [" + section + "]/ProcessorMaxConcurrency value (" +
                to_string(limit) + "). "
                "The processor max concurrency must be > 0. "
                "Resetting to " +
                to_string(m_ProcessorMaxConcurrency));
            limit = m_ProcessorMaxConcurrency;
        }

        return limit;
    }

    // No processor specific value => server wide (or default)
    return m_ProcessorMaxConcurrency;
}


bool SPubseqGatewaySettings::IsAuthProtectedCommand(const string &  cmd) const
{
    for (const auto &  item: m_AuthCommands) {
        if (item == cmd) {
            return true;
        }
    }
    return false;
}

