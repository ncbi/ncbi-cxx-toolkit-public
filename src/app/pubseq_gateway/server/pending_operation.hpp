#ifndef PENDING_OPERATION__HPP
#define PENDING_OPERATION__HPP

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
#include <memory>
using namespace std;

#include <corelib/request_ctx.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

#include "http_server_transport.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "pending_requests.hpp"
#include "cass_fetch.hpp"
#include "protocol_utils.hpp"
#include "async_seq_id_resolver.hpp"
#include "async_bioseq_query.hpp"
#include "id2info.hpp"



class CPendingOperation
{
private:
    enum EAsyncInterruptPoint {
        eAnnotSeqIdResolution,
        eResolveSeqIdResolution,
        eResolveBioseqDetails,
        eGetSeqIdResolution,
        eGetBioseqDetails
    };

public:
    CPendingOperation(const SBlobRequest &  blob_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const SResolveRequest &  resolve_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const SAnnotRequest &  annot_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);
    CPendingOperation(const STSEChunkRequest &  tse_chunk_request,
                      size_t  initial_reply_chunks,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout,
                      unsigned int  max_retries,
                      CRef<CRequestContext>  request_context);

    ~CPendingOperation();

    void Clear();
    void Start(HST::CHttpReply<CPendingOperation>& resp);
    void Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait);

    void Cancel(void)
    {
        m_Cancelled = true;
    }

    void UpdateOverallStatus(CRequestStatus::ECode  status)
    {
        m_OverallStatus = max(status, m_OverallStatus);
    }

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    // Serving the 'resolve' request
    void x_ProcessResolveRequest(void);
    void x_ProcessResolveRequest(SResolveInputSeqIdError &  err,
                                 SBioseqResolution &  bioseq_resolution);
    void x_CompleteResolveRequest(SBioseqResolution &  bioseq_resolution);
    void x_OnResolveResolutionError(SResolveInputSeqIdError &  err,
                                    const THighResolutionTimePoint &  start_timestamp);
    void x_OnResolveResolutionError(CRequestStatus::ECode  status,
                                    const string &  message,
                                    const THighResolutionTimePoint &  start_timestamp);
    void x_ResolveRequestBioseqInconsistency(const THighResolutionTimePoint &  start_timestamp);
    void x_ResolveRequestBioseqInconsistency(const string &  err_msg,
                                             const THighResolutionTimePoint &  start_timestamp);

public:
    void OnBioseqDetailsRecord(SBioseqResolution &&  async_bioseq_info);
    void OnBioseqDetailsError(CRequestStatus::ECode  status, int  code,
                              EDiagSev  severity, const string &  message,
                              const THighResolutionTimePoint &  start_timestamp);

private:
    void x_ProcessGetRequest(void);
    void x_ProcessGetRequest(SResolveInputSeqIdError &  err,
                             SBioseqResolution &  bioseq_resolution);
    void x_GetRequestBioseqInconsistency(
                            const THighResolutionTimePoint &  start_timestamp);
    void x_GetRequestBioseqInconsistency(
                            const string &  err_msg,
                            const THighResolutionTimePoint &  start_timestamp);
    void x_CompleteGetRequest(SBioseqResolution &  bioseq_resolution);
    void x_StartMainBlobRequest(void);

private:
    void x_ProcessAnnotRequest(void);
    void x_ProcessAnnotRequest(SResolveInputSeqIdError &  err,
                               SBioseqResolution &  bioseq_resolution);
    void x_ProcessTSEChunkRequest(void);
    bool x_AllFinishedRead(void) const;
    void x_SendReplyCompletion(bool  forced = false);
    void x_SetRequestContext(void);
    void x_PrintRequestStop(int  status);
    bool x_SatToSatName(const SBlobRequest &  blob_request,
                        SBlobId &  blob_id);
    bool x_TSEChunkSatToSatName(SBlobId &  blob_id);
    void x_SendBlobPropError(size_t  item_id,
                             const string &  message,
                             int  error_code);
    void x_SendReplyError(const string &  msg,
                          CRequestStatus::ECode  status,
                          int  code);
    void x_SendBioseqInfo(const string &  protobuf_bioseq_info,
                          CBioseqInfoRecord &  bioseq_info,
                          EOutputFormat  output_format);
    void x_Peek(HST::CHttpReply<CPendingOperation>& resp, bool  need_wait,
                unique_ptr<CCassFetch> &  fetch_details);
    void x_PeekIfNeeded(void);

    bool x_UsePsgProtocol(void) const;
    void x_InitUrlIndentification(void);
    bool x_ValidateTSEChunkNumber(int64_t  requested_chunk,
                                  CPSGId2Info::TChunks  total_chunks);

private:
    bool x_ComposeOSLT(CSeq_id &  parsed_seq_id, int16_t &  effective_seq_id_type,
                       list<string> &  secondary_id_list, string &  primary_id);
    ECacheLookupResult x_ResolveAsIsInCache(SBioseqResolution &  bioseq_resolution,
                                            SResolveInputSeqIdError &  err);
    void x_ResolveViaComposeOSLTInCache(CSeq_id &  parsed_seq_id,
                                        int16_t  effective_seq_id_type,
                                        const list<string> &  secondary_id_list,
                                        const string &  primary_id,
                                        SResolveInputSeqIdError &  err,
                                        SBioseqResolution &  bioseq_resolution);
    ECacheLookupResult x_ResolvePrimaryOSLTInCache(const string &  primary_id,
                                                   int16_t  effective_version,
                                                   int16_t  effective_seq_id_type,
                                                   bool  need_to_try_bioseq_info,
                                                   SBioseqResolution &  bioseq_resolution);
    ECacheLookupResult x_ResolveSecondaryOSLTInCache(const string &  secondary_id,
                                                     int16_t  effective_seq_id_type,
                                                     SBioseqResolution &  bioseq_resolution);

private:
    void x_ResolveInputSeqId(SBioseqResolution &  bioseq_resolution,
                             SResolveInputSeqIdError &  err);
    bool x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                 int16_t &  eff_seq_id_type);
    ESeqIdParsingResult x_ParseInputSeqId(CSeq_id &  seq_id, string &  err_msg);
    void x_OnBioseqError(CRequestStatus::ECode  status, const string &  err_msg,
                         const THighResolutionTimePoint &  start_timestamp);
    void x_OnReplyError(CRequestStatus::ECode  status, int  err_code,
                        const string &  err_msg,
                        const THighResolutionTimePoint &  start_timestamp);

public:
    int16_t GetEffectiveVersion(const CTextseq_id *  text_seq_id);
    void OnSeqIdAsyncResolutionFinished(SBioseqResolution &&  async_bioseq_resolution);
    void OnSeqIdAsyncError(CRequestStatus::ECode  status, int  code,
                           EDiagSev  severity, const string &  message,
                           const THighResolutionTimePoint &  start_timestamp);

public:
    // Named annotations callbacks
    bool OnNamedAnnotData(CNAnnotRecord &&  annot_record, bool  last,
                          CCassNamedAnnotFetch *  fetch_details, int32_t  sat);
    void OnNamedAnnotError(CCassNamedAnnotFetch *  fetch_details,
                           CRequestStatus::ECode  status, int  code,
                           EDiagSev  severity, const string &  message);

public:
    // Get blob callbacks
    void OnGetBlobProp(CCassBlobFetch *  fetch_details,
                       CBlobRecord const &  blob, bool is_found);
    void OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                        CBlobRecord const &  blob, const unsigned char *  chunk_data,
                        unsigned int  data_size, int  chunk_no);
    void OnGetBlobError(CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message);

public:
    void OnGetSplitHistory(CCassSplitHistoryFetch *  fetch_details,
                           vector<SSplitHistoryRecord> && result);
    void OnGetSplitHistoryError(CCassSplitHistoryFetch *  fetch_details,
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message);

private:
    void x_OnBlobPropNotFound(CCassBlobFetch *  fetch_details);
    void x_OnBlobPropNoneTSE(CCassBlobFetch *  fetch_details);
    void x_OnBlobPropSlimTSE(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);
    void x_OnBlobPropSmartTSE(CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropWholeTSE(CCassBlobFetch *  fetch_details,
                              CBlobRecord const &  blob);
    void x_OnBlobPropOrigTSE(CCassBlobFetch *  fetch_details,
                             CBlobRecord const &  blob);
    void x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                     CBlobRecord const &  blob);
    void x_RequestID2BlobChunks(CCassBlobFetch *  fetch_details,
                                CBlobRecord const &  blob, bool  info_blob_only);
    void x_RequestId2SplitBlobs(CCassBlobFetch *  fetch_details, const string &  sat_name);
    bool x_ParseId2Info(CCassBlobFetch *  fetch_details, CBlobRecord const &  blob);
    bool x_ParseTSEChunkId2Info(const string &  info,
                                unique_ptr<CPSGId2Info> &  id2_info,
                                const SBlobId &  blob_id);
    void x_RequestTSEChunk(const SSplitHistoryRecord &  split_record,
                           CCassSplitHistoryFetch *  fetch_details);

    void x_RegisterResolveTiming(const SBioseqResolution &  bioseq_resolution);
    void x_RegisterResolveTiming(CRequestStatus::ECode  status,
                                 const THighResolutionTimePoint &  start_timestamp);

    unique_ptr<CPSGId2Info>     m_Id2Info;

public:
    unsigned int GetTimeout(void) const                   { return m_Timeout; }
    unsigned int GetMaxRetries(void) const                { return m_MaxRetries; }
    shared_ptr<CCassConnection> GetConnection(void) const { return m_Conn; }
    int16_t GetUrlSeqIdType(void) const                   { return m_UrlSeqIdType; }
    shared_ptr<CCassDataCallbackReceiver> GetDataReadyCB(void)
    { return m_ProtocolSupport.GetReply()->GetDataReadyCB(); }

    void RegisterFetch(CCassFetch *  fetch)
    { m_FetchDetails.push_back(unique_ptr<CCassFetch>(fetch)); }

private:
    shared_ptr<CCassConnection>             m_Conn;
    unsigned int                            m_Timeout;
    unsigned int                            m_MaxRetries;
    CRequestStatus::ECode                   m_OverallStatus;

    // Incoming request. It could be by sat/sat_key or by seq_id/id_type
    EPendingRequestType                     m_RequestType;
    SBlobRequest                            m_BlobRequest;
    SResolveRequest                         m_ResolveRequest;
    SAnnotRequest                           m_AnnotRequest;
    STSEChunkRequest                        m_TSEChunkRequest;
    CTempString                             m_UrlSeqId;
    int16_t                                 m_UrlSeqIdType;
    ECacheAndCassandraUse                   m_UrlUseCache;

    bool                                    m_Cancelled;
    CRef<CRequestContext>                   m_RequestContext;

    // Cassandra data loaders; there could be many of them
    list<unique_ptr<CCassFetch>>            m_FetchDetails;

    CProtocolUtils                          m_ProtocolSupport;

    // Async DB access support
    unique_ptr<CAsyncSeqIdResolver>         m_AsyncSeqIdResolver;
    unique_ptr<CAsyncBioseqQuery>           m_AsyncBioseqDetailsQuery;
    EAsyncInterruptPoint                    m_AsyncInterruptPoint;
    THighResolutionTimePoint                m_AsyncCassResolutionStart;
};


#endif
