#ifndef PUBSEQ_GATEWAY__HPP
#define PUBSEQ_GATEWAY__HPP

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
#include <string>
#include <functional>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/checksum.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/messages.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_factory.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#include "pending_operation.hpp"
#include "http_daemon.hpp"
#include "pubseq_gateway_version.hpp"
#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "exclude_blob_cache.hpp"
#include "split_info_cache.hpp"
#include "my_ncbi_cache.hpp"
#include "alerts.hpp"
#include "timing.hpp"
#include "psgs_dispatcher.hpp"
#include "psgs_uv_loop_binder.hpp"
#include "settings.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);


const long  kMaxTestIOSize = 1000000000;


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp();
    ~CPubseqGatewayApp();

    virtual void Init(void);
    void ParseArgs(void);
    void OpenCache(void);
    bool OpenCass(void);
    void CreateMyNCBIFactory(void);
    void DoMyNCBIDnsResolve(void);
    void TestMyNCBI(uv_loop_t *  loop);

    bool PopulateCassandraMapping(bool  initialization);
    void CheckCassMapping(void);
    void CloseCass(void);
    optional<SSatInfoEntry> SatToKeyspace(int32_t  sat)
    { return m_CassSchemaProvider->GetBlobKeyspace(sat); }

    void MaintainSplitInfoBlobCache(void) {
        if (m_SplitInfoCache)
            m_SplitInfoCache->Maintain();
    }
    void MaintainMyNCBICaches(void) {
        if (m_MyNCBIOKCache)
            m_MyNCBIOKCache->Maintain();
        if (m_MyNCBINotFoundCache)
            m_MyNCBINotFoundCache->Maintain();
        if (m_MyNCBIErrorCache)
            m_MyNCBIErrorCache->Maintain();
    }

    SSatInfoEntry GetBioseqKeyspace(void) const
    { return m_CassSchemaProvider->GetResolverKeyspace(); }

    optional<SSatInfoEntry> GetIPGKeyspace(void) const
    { return m_CassSchemaProvider->GetIPGKeyspace(); }

    vector<SSatInfoEntry> GetBioseqNAKeyspaces(void) const
    { return m_CassSchemaProvider->GetNAKeyspaces(); }

    CPubseqGatewayCache *  GetLookupCache(void)
    { return m_LookupCache.get(); }

    CExcludeBlobCache *  GetExcludeBlobCache(void)
    { return m_ExcludeBlobCache.get(); }

    CSplitInfoCache *  GetSplitInfoCache(void)
    { return m_SplitInfoCache.get(); }

    CMyNCBIOKCache *  GetMyNCBIOKCache(void)
    { return m_MyNCBIOKCache.get(); }

    CMyNCBINotFoundCache *  GetMyNCBINotFoundCache(void)
    { return m_MyNCBINotFoundCache.get(); }

    CMyNCBIErrorCache *  GetMyNCBIErrorCache(void)
    { return m_MyNCBIErrorCache.get(); }

    shared_ptr<CPSGMessages>  GetPublicCommentsMapping(void)
    { return m_CassSchemaProvider->GetMessages(); }

    unsigned long GetSendBlobIfSmall(void) const
    { return m_Settings.m_SendBlobIfSmall; }

    int OnBadURL(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGet(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetBlob(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnResolve(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetTSEChunk(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetNA(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnAccessionVersionHistory(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnIPGResolve(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyz(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzCassandra(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzConnections(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzLMDB(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzWGS(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzCDD(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnReadyzSNP(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnLivez(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnHealthz(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnHealth(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnDeepHealth(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnConfig(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnInfo(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnStatus(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnShutdown(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetAlerts(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnAckAlert(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnStatistics(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnDispatcherStatus(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnTestIO(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetSatMapping(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnConnectionsStatus(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnHello(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);

    virtual int Run(void);

    static CPubseqGatewayApp *  GetInstance(void);

    void NotifyRequestFinished(size_t  request_id)
    { m_RequestDispatcher->NotifyRequestFinished(request_id); }

    COperationTiming & GetTiming(void)
    { return *m_Timing.get(); }

    EPSGS_StartupDataState GetStartupDataState(void) const
    { return m_StartupDataState; }

    map<string, tuple<string, string>>  GetIdToNameAndDescriptionMap(void) const
    { return m_Settings.m_IdToNameAndDescription; }

    unsigned int GetCassandraTimeout(void) const
    { return m_Settings.m_TimeoutMs; }

    unsigned int GetCassandraMaxRetries(void) const
    { return m_Settings.m_MaxRetries; }

    bool GetCDDProcessorsEnabled() const
    { return m_Settings.m_CDDProcessorsEnabled; }

    bool GetWGSProcessorsEnabled() const
    { return m_Settings.m_WGSProcessorsEnabled; }

    bool GetSNPProcessorsEnabled() const
    { return m_Settings.m_SNPProcessorsEnabled; }

    bool GetCassandraProcessorsEnabled(void) const
    { return m_Settings.m_CassandraProcessorsEnabled; }

    IPSGS_Processor::EPSGS_StartProcessing
    SignalStartProcessing(IPSGS_Processor *  processor)
    { return m_RequestDispatcher->SignalStartProcessing(processor); }

    size_t GetProcessorMaxConcurrency(const string &  processor_id)
    { return m_Settings.GetProcessorMaxConcurrency(GetConfig(), processor_id); }

    void SignalFinishProcessing(IPSGS_Processor *  processor,
                                CPSGS_Dispatcher::EPSGS_SignalSource  signal_source)
    { m_RequestDispatcher->SignalFinishProcessing(processor, signal_source); }

    void SignalConnectionCanceled(size_t  request_id)
    { m_RequestDispatcher->SignalConnectionCanceled(request_id); }

    bool GetSSLEnable(void) const
    { return m_Settings.m_SSLEnable; }

    string GetSSLCertFile(void) const
    { return m_Settings.m_SSLCertFile; }

    string GetSSLKeyFile(void) const
    { return m_Settings.m_SSLKeyFile; }

    string GetSSLCiphers(void) const
    { return m_Settings.m_SSLCiphers; }

    size_t GetShutdownIfTooManyOpenFD(void) const
    { return m_Settings.m_ShutdownIfTooManyOpenFD; }

    int GetPageSize(void) const
    { return m_Settings.m_IPGPageSize; }

    size_t GetHttpMaxBacklog(void) const
    { return m_Settings.m_HttpMaxBacklog; }

    size_t GetHttpMaxRunning(void) const
    { return m_Settings.m_HttpMaxRunning; }

    bool GetSeqIdResolveAlways(void) const
    { return m_Settings.m_SeqIdResolveAlways; }

    CPSGAlerts &  GetAlerts(void)
    { return m_Alerts; }

    CPSGSCounters &  GetCounters(void)
    { return * m_Counters.get(); }

    void RegisterUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);

        m_ThreadToUVLoop[uv_thread] = uv_loop;
        m_ThreadToBinder[uv_thread] =
                unique_ptr<CPSGS_UvLoopBinder>(new CPSGS_UvLoopBinder(uv_loop));
    }

    void UnregisterUVLoop(uv_thread_t  uv_thread)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);
        m_ThreadToBinder[uv_thread]->x_Unregister();
    }

    void RegisterDaemonUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);

        m_ThreadToUVLoop[uv_thread] = uv_loop;
    }

    uv_loop_t *  GetUVLoop(void)
    {
        auto    it = m_ThreadToUVLoop.find(uv_thread_self());
        if (it == m_ThreadToUVLoop.end()) {
            return nullptr;
        }
        return it->second;
    }

    void CancelAllProcessors(void)
    { m_RequestDispatcher->CancelAll(); }

    CPSGS_Dispatcher *  GetProcessorDispatcher(void)
    { return m_RequestDispatcher.get(); }

    CPSGS_UvLoopBinder &  GetUvLoopBinder(uv_thread_t  uv_thread_id);

    shared_ptr<CPSG_MyNCBIFactory> GetMyNCBIFactory(void)
    { return m_MyNCBIFactory; }

private:
    struct SRequestParameter
    {
        bool            m_Found;
        CTempString     m_Value;

        SRequestParameter() : m_Found(false)
        {}
    };

    void x_SendMessageAndCompletionChunks(
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  now,
        const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity);

    bool x_ProcessCommonGetAndResolveParams(
        CHttpRequest &  req,
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  now,
        CTempString &  seq_id, int &  seq_id_type,
        SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache,
        bool  seq_id_is_optional=false);

    // Common URL parameters for all ../ID/.. requests
    bool x_GetCommonIDRequestParams(
            CHttpRequest &  req,
            shared_ptr<CPSGS_Reply>  reply,
            const psg_time_point_t &  now,
            SPSGS_RequestBase::EPSGS_Trace &  trace,
            int &  hops,
            vector<string> &  enabled_processors,
            vector<string> &  disabled_processors,
            bool &  processor_events);

private:
    string  x_GetCmdLineArguments(void) const;
    CRef<CRequestContext>  x_CreateRequestContext(CHttpRequest &  req,
                                                  shared_ptr<CPSGS_Reply>  reply);
    void x_PrintRequestStop(CRef<CRequestContext>  context,
                            CPSGS_Request::EPSGS_Type  request_type,
                            CRequestStatus::ECode  status,
                            size_t  bytes_sent);

    SRequestParameter  x_GetParam(CHttpRequest &  req,
                                  const string &  name) const;
    bool  x_GetSendBlobIfSmallParameter(CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        int &  send_blob_if_small);
    bool x_IsBoolParamValid(const string &  param_name,
                            const CTempString &  param_value,
                            string &  err_msg) const;
    bool x_ConvertIntParameter(const string &  param_name,
                               const CTempString &  param_value,
                               int &  converted,
                               string &  err_msg) const;
    bool x_ConvertIntParameter(const string &  param_name,
                               const CTempString &  param_value,
                               int64_t &  converted,
                               string &  err_msg) const;
    bool x_ConvertDoubleParameter(const string &  param_name,
                                  const CTempString &  param_value,
                                  double &  converted,
                                  string &  err_msg) const;
    bool x_GetUseCacheParameter(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache);
    bool x_GetOutputFormat(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           SPSGS_ResolveRequest::EPSGS_OutputFormat &  output_format);
    bool x_GetIntrospectionFormat(CHttpRequest &  req,
                                  string &  fmt,
                                  string &  err_msg);
    bool x_GetTSEOption(CHttpRequest &  req,
                        shared_ptr<CPSGS_Reply>  reply,
                        const psg_time_point_t &  now,
                        SPSGS_BlobRequestBase::EPSGS_TSEOption &  tse_option);
    bool x_GetLastModified(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           int64_t &  last_modified);
    bool x_GetBlobId(CHttpRequest &  req,
                     shared_ptr<CPSGS_Reply>  reply,
                     const psg_time_point_t &  now,
                     SPSGS_BlobId &  blob_id);
    bool x_GetResolveFlags(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           SPSGS_ResolveRequest::TPSGS_BioseqIncludeData &  include_data_flags);
    bool x_GetId2Chunk(CHttpRequest &  req,
                       shared_ptr<CPSGS_Reply>  reply,
                       const psg_time_point_t &  now,
                       int64_t &  id2_chunk);
    bool x_GetId2Info(CHttpRequest &  req,
                      shared_ptr<CPSGS_Reply>  reply,
                      const psg_time_point_t &  now,
                      string &  id2_info);
    vector<string> x_GetExcludeBlobs(CHttpRequest &  req) const;
    bool x_GetAccessionSubstitutionOption(CHttpRequest &  req,
                                          shared_ptr<CPSGS_Reply>  reply,
                                          const psg_time_point_t &  now,
                                          SPSGS_RequestBase::EPSGS_AccSubstitutioOption & acc_subst_option);
    bool x_GetTraceParameter(CHttpRequest &  req,
                             shared_ptr<CPSGS_Reply>  reply,
                             const psg_time_point_t &  now,
                             SPSGS_RequestBase::EPSGS_Trace &  trace);
    bool x_GetProcessorEventsParameter(CHttpRequest &  req,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const psg_time_point_t &  now,
                                       bool &  processor_events);
    bool x_GetIncludeHUPParameter(CHttpRequest &  req,
                                  shared_ptr<CPSGS_Reply>  reply,
                                  const psg_time_point_t &  now,
                                  optional<bool> &  include_hup);
    bool x_GetHops(CHttpRequest &  req,
                   shared_ptr<CPSGS_Reply>  reply,
                   const psg_time_point_t &  now,
                   int &  hops);
    bool x_GetResendTimeout(CHttpRequest &  req,
                            shared_ptr<CPSGS_Reply>  reply,
                            const psg_time_point_t &  now,
                            double &  resend_timeout);
    bool x_GetSeqIdResolveParameter(CHttpRequest &  req,
                                    shared_ptr<CPSGS_Reply>  reply,
                                    const psg_time_point_t &  now,
                                    bool &  auto_blob_skipping);
    bool x_GetEnabledAndDisabledProcessors(CHttpRequest &  req,
                                           shared_ptr<CPSGS_Reply>  reply,
                                           const psg_time_point_t &  now,
                                           vector<string> &  enabled_processors,
                                           vector<string> &  disabled_processors);
    bool x_GetNames(CHttpRequest &  req,
                    shared_ptr<CPSGS_Reply>  reply,
                    const psg_time_point_t &  now,
                    vector<string> &  names);
    bool x_GetProtein(CHttpRequest &  req,
                      shared_ptr<CPSGS_Reply>  reply,
                      const psg_time_point_t &  now,
                      optional<string> &  protein);
    bool x_GetIPG(CHttpRequest &  req,
                  shared_ptr<CPSGS_Reply>  reply,
                  const psg_time_point_t &  now,
                  int64_t &  ipg);
    bool x_GetNucleotide(CHttpRequest &  req,
                         shared_ptr<CPSGS_Reply>  reply,
                         const psg_time_point_t &  now,
                         optional<string> &  nucleotide);
    bool x_GetTimeSeries(CHttpRequest &  req,
                         shared_ptr<CPSGS_Reply>  reply,
                         const psg_time_point_t &  now,
                         vector<pair<int, int>> &  time_series);
    bool x_GetSNPScaleLimit(CHttpRequest &  req,
                            shared_ptr<CPSGS_Reply>  reply,
                            const psg_time_point_t &  now,
                            optional<CSeq_id::ESNPScaleLimit> &  snp_scale_limit);
    bool x_GetVerboseParameter(CHttpRequest &  req,
                               shared_ptr<CPSGS_Reply>  reply,
                               const psg_time_point_t &  now,
                               bool &  verbose);
    bool x_GetExcludeChecks(CHttpRequest &  req,
                            shared_ptr<CPSGS_Reply>  reply,
                            const psg_time_point_t &  now,
                            vector<string> &  exclude_checks);
    bool x_GetPeerUserAgentParameter(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     const psg_time_point_t &  now,
                                     string &  peer_user_agent);
    bool x_GetPeerIdParameter(CHttpRequest &  req,
                              shared_ptr<CPSGS_Reply>  reply,
                              const psg_time_point_t &  now,
                              string &  peer_id);


private:
    void x_FixIntrospectionVersion(void);
    void x_FixIntrospectionBuildDate(void);
    string x_PrepareConfigJson(void);

    void x_InsufficientArguments(shared_ptr<CPSGS_Reply>  reply,
                                 const psg_time_point_t &  now,
                                 const string &  err_msg);
    void x_MalformedArguments(shared_ptr<CPSGS_Reply>  reply,
                              const psg_time_point_t &  now,
                              const string &  err_msg);
    void x_Finish500(shared_ptr<CPSGS_Reply>  reply,
                     const psg_time_point_t &  now,
                     EPSGS_PubseqGatewayErrorCode  code,
                     const string &  err_msg);
    bool x_IsShuttingDown(shared_ptr<CPSGS_Reply>  reply,
                          const psg_time_point_t &  now);
    bool x_IsShuttingDownForZEndPoints(shared_ptr<CPSGS_Reply>  reply,
                                       bool  verbose);
    bool x_IsConnectionAboveSoftLimit(shared_ptr<CPSGS_Reply>  reply,
                                      const psg_time_point_t &  create_timestamp);
    bool x_IsConnectionAboveSoftLimitForZEndPoints(shared_ptr<CPSGS_Reply>  reply,
                                         bool  verbose);
    void x_RegisterProcessors(void);
    bool x_DispatchRequest(CRef<CRequestContext>   context,
                           shared_ptr<CPSGS_Request>  request,
                           shared_ptr<CPSGS_Reply>  reply);
    void x_InitSSL(void);
    bool x_CheckAuthorization(const string &  request_name,
                              CRef<CRequestContext>   context,
                              CHttpRequest &  http_req,
                              shared_ptr<CPSGS_Reply>  reply,
                              const psg_time_point_t &  now);

private:
    // z end point support

    // Pubseq gateway API lock to make it cheap to create CPSG_Queue instances.
    // It is taken at the initialization and kept till the end of the app life.
    CPSG_Queue::TApiLock    m_PSGAPILock;

    void x_InitialzeZEndPointData(void);

    // At the moment /readyz and healthz match. The only difference is what
    // request counter to increment
    int x_ReadyzHealthzImplementation(CHttpRequest &  req,
                                      shared_ptr<CPSGS_Reply>  reply);

    CRequestStatus::ECode
    x_SelfZEndPointCheckImpl(CRef<CRequestContext>  context,
                             const string &  check_id,
                             const string &  check_name,
                             const string &  check_description,
                             bool  verbose,
                             const string &  health_command,
                             const CTimeout &  health_timeout,
                             CJsonNode &  node);
    bool x_NeedReadyZCheckPerform(const string &  check_name,
                                  bool  verbose,
                                  const vector<string> &  exclude_checks,
                                  bool &  is_critical);
    void x_SelfZEndPointCheck(CHttpRequest &  req,
                              shared_ptr<CPSGS_Reply>  reply,
                              const string &  health_command);
    void x_SendZEndPointReply(CRequestStatus::ECode  http_status,
                              shared_ptr<CPSGS_Reply>  reply,
                              const string *  payload);

private:
    SPubseqGatewaySettings              m_Settings;

    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    shared_ptr<CSatInfoSchemaProvider>  m_CassSchemaProvider;
    shared_ptr<CPSG_MyNCBIFactory>      m_MyNCBIFactory;

    CTime                               m_StartTime;

    unique_ptr<char []>                 m_IOTestBuffer;

    unique_ptr<CPubseqGatewayCache>     m_LookupCache;
    unique_ptr<CHttpDaemon>             m_HttpDaemon;

    unique_ptr<CExcludeBlobCache>       m_ExcludeBlobCache;
    unique_ptr<CSplitInfoCache>         m_SplitInfoCache;
    unique_ptr<CMyNCBIOKCache>          m_MyNCBIOKCache;
    unique_ptr<CMyNCBINotFoundCache>    m_MyNCBINotFoundCache;
    unique_ptr<CMyNCBIErrorCache>       m_MyNCBIErrorCache;

    CPSGAlerts                          m_Alerts;
    unique_ptr<COperationTiming>        m_Timing;
    unique_ptr<CPSGSCounters>           m_Counters;

    EPSGS_StartupDataState              m_StartupDataState;
    CNcbiLogFields                      m_LogFields;

    // Serialized JSON introspection message
    string                              m_HelpMessageJson;
    string                              m_HelpMessageHtml;

    // Serialized JSON for the config request
    string                              m_ConfigReplyJson;

    // Requests dispatcher
    unique_ptr<CPSGS_Dispatcher>        m_RequestDispatcher;

    // Mapping between the libuv thread id and the binder associated with the
    // libuv worker thread loop
    map<uv_thread_t,
        unique_ptr<CPSGS_UvLoopBinder>> m_ThreadToBinder;
    mutex                               m_ThreadToBinderGuard;
    map<uv_thread_t, uv_loop_t *>       m_ThreadToUVLoop;

    // My NCBI periodic check support
    bool                                m_LastMyNCBIResolveOK;
    bool                                m_LastMyNCBITestOk;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};


#endif
