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

#include "pubseq_gateway.hpp"
#include "pending_operation.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "get_blob_callback.hpp"
#include "insdc_utils.hpp"
#include "get_processor.hpp"
#include "tse_chunk_processor.hpp"
#include "resolve_processor.hpp"
#include "annot_processor.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

using namespace std::placeholders;


CPendingOperation::CPendingOperation(unique_ptr<CPSGS_Request>  user_request,
                                     size_t  initial_reply_chunks) :
    m_UserRequest(move(user_request)),
    m_Reply(new CPSGS_Reply(initial_reply_chunks)),
    m_Cancelled(false)
{
    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: resolution "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }

    for (auto &  details: m_FetchDetails) {
        if (details) {
            PSG_TRACE("CPendingOperation::~CPendingOperation: "
                      "Id2SplibChunks blob loader: " <<
                      details->GetLoader());
        }
    }

    // Just in case if a request ended without a normal request stop,
    // finish it here as the last resort.
    x_PrintRequestStop();
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::Clear(): resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::Clear(): annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId << "." <<
                      m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::Clear(): TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest->GetRequest<SPSGS_TSEChunkRequest>().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }

    for (auto &  details: m_FetchDetails) {
        if (details) {
            PSG_TRACE("CPendingOperation::Clear(): "
                      "Cassandra data async loader: " <<
                      details->GetLoader());
            details->ResetCallbacks();
            details = nullptr;
        }
    }
    m_FetchDetails.clear();

    m_Reply->Clear();
    m_AsyncSeqIdResolver = nullptr;
    m_AsyncBioseqDetailsQuery = nullptr;
    m_Cancelled = false;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_Reply->SetReply(&resp);
    auto    request_type = m_UserRequest->GetRequestType();
    switch (request_type) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_Processor.reset(new CPSGS_ResolveProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_UrlUseCache = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_UseCache;
            x_ProcessGetRequest();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            m_Processor.reset(new CPSGS_GetProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_Processor.reset(new CPSGS_AnnotProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            m_Processor.reset(new CPSGS_TSEChunkProcessor(m_UserRequest, m_Reply));
            m_Processor->Process();
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Unhandeled request type " +
                       to_string(static_cast<int>(request_type)));
    }
}


void CPendingOperation::x_OnBioseqError(
                            CRequestStatus::ECode  status,
                            const string &  err_msg,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    size_t      item_id = m_Reply->GetItemId();

    if (status != CRequestStatus::e404_NotFound)
        m_UserRequest->UpdateOverallStatus(status);

    m_Reply->PrepareBioseqMessage(item_id, err_msg, status,
                                  ePSGS_NoBioseqInfo, eDiag_Error);
    m_Reply->PrepareBioseqCompletion(item_id, 2);
    x_SendReplyCompletion(true);
    m_Reply->Flush();

    x_RegisterResolveTiming(status, start_timestamp);
}


void CPendingOperation::x_OnReplyError(
                            CRequestStatus::ECode  status,
                            int  err_code, const string &  err_msg,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    m_Reply->PrepareReplyMessage(err_msg, status,
                                 err_code, eDiag_Error);
    x_SendReplyCompletion(true);
    m_Reply->Flush();

    x_RegisterResolveTiming(status, start_timestamp);
}


void CPendingOperation::x_ProcessGetRequest(void)
{
    // Get request could be one of the following:
    // - the blob was requested by a sat/sat_key pair
    // - the blob was requested by a seq_id/seq_id_type pair
    if (m_UserRequest->GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        // By seq_id -> search for the bioseq key info in the cache only
        SResolveInputSeqIdError err;
        SBioseqResolution       bioseq_resolution(
                    m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_StartTimestamp);

        x_ResolveInputSeqId(bioseq_resolution, err);
        if (err.HasError()) {
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, ePSGS_UnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
            return;
        }

        // Async resolution may be required
        if (bioseq_resolution.m_ResolutionResult == ePSGS_PostponedForDB) {
            m_AsyncInterruptPoint = eGetSeqIdResolution;
            return;
        }

        x_ProcessGetRequest(err, bioseq_resolution);
    } else {
        x_StartMainBlobRequest();
    }
}


void CPendingOperation::x_ProcessGetRequest(SResolveInputSeqIdError &  err,
                                            SBioseqResolution &  bioseq_resolution)
{
    if (!bioseq_resolution.IsValid()) {
        if (err.HasError()) {
            m_UserRequest->UpdateOverallStatus(err.m_ErrorCode);
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, ePSGS_UnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            x_OnBioseqError(CRequestStatus::e404_NotFound,
                            "Could not resolve seq_id " +
                            m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId,
                            bioseq_resolution.m_RequestStartTimestamp);
        }
        return;
    }

    // A few cases here: comes from cache or DB
    // ePSGS_Si2csiCache, ePSGS_Si2csiDB, ePSGS_BioseqCache, ePSGS_BioseqDB
    if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB) {
        // The only key fields are available, need to pull the full
        // bioseq info
        CPSGCache   psg_cache(m_UserRequest, m_Reply);
        auto        cache_lookup_result =
                            psg_cache.LookupBioseqInfo(bioseq_resolution);
        if (cache_lookup_result != ePSGS_Found) {
            // No cache hit (or not allowed); need to get to DB if allowed
            if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_CacheOnly) {
                // Async DB query
                m_AsyncInterruptPoint = eGetBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                              this, m_UserRequest, m_Reply));
                // true => use seq_id_type
                m_AsyncBioseqDetailsQuery->MakeRequest();
                return;
            }

            // default error message
            x_GetRequestBioseqInconsistency(
                                    bioseq_resolution.m_RequestStartTimestamp);
            return;
        }
    }

    x_CompleteGetRequest(bioseq_resolution);
}


void
CPendingOperation::x_CompleteGetRequest(SBioseqResolution &  bioseq_resolution)
{
    // Send BIOSEQ_INFO
    x_SendBioseqInfo(bioseq_resolution, SPSGS_ResolveRequest::ePSGS_JsonFormat);
    x_RegisterResolveTiming(bioseq_resolution);

    // Translate sat to keyspace
    SPSGS_BlobId    blob_id(bioseq_resolution.m_BioseqInfo.GetSat(),
                            bioseq_resolution.m_BioseqInfo.GetSatKey());

    if (!x_SatToSatName(m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>(), blob_id)) {
        // This is server data inconsistency error, so 500 (not 404)
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        x_SendReplyCompletion(true);
        m_Reply->Flush();
        return;
    }

    // retrieve BLOB_PROP & CHUNKS (if needed)
    m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId = blob_id;

    x_StartMainBlobRequest();
}


void CPendingOperation::x_GetRequestBioseqInconsistency(
                        const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    x_GetRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId +
        " but the bioseq info is not found",
        start_timestamp);
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_GetRequestBioseqInconsistency(
                        const string &  err_msg,
                        const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg,
                    start_timestamp);
}


void CPendingOperation::x_ResolveRequestBioseqInconsistency(
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    // This method is used in case of a resolve and get_na requests
    string      seq_id;
    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            seq_id = m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId;
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            seq_id = m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId;
            break;
        default:
            // Should not happened
            seq_id = "???";
    }

    x_ResolveRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        seq_id + " but the bioseq info is not found",
        start_timestamp);
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_ResolveRequestBioseqInconsistency(
                            const string &  err_msg,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    if (m_UserRequest->UsePsgProtocol()) {
        x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg,
                        start_timestamp);
    } else {
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Reply->Send500(err_msg.c_str());

        x_RegisterResolveTiming(CRequestStatus::e500_InternalServerError,
                                start_timestamp);
        x_PrintRequestStop();
    }
}


void CPendingOperation::OnBioseqDetailsRecord(
                                    SBioseqResolution &&  async_bioseq_info)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eGetBioseqDetails:
            if (async_bioseq_info.IsValid())
                x_CompleteGetRequest(async_bioseq_info);
            else
                // default error message
                x_GetRequestBioseqInconsistency(
                                async_bioseq_info.m_RequestStartTimestamp);
            break;
        default:
            ;
    }
}


void CPendingOperation::OnBioseqDetailsError(
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message,
                                const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eGetBioseqDetails:
            x_GetRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " +
                    m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId + " but the "
                    "bioseq info is not found (database error: " + message +
                    ")", start_timestamp);
            break;
        default:
            ;
    }
}


void CPendingOperation::x_StartMainBlobRequest(void)
{
    // Here: m_BlobRequest has the resolved sat and a sat_key regardless
    //       how the blob was requested

    // In case of the blob request by sat.sat_key it could be in the exclude
    // blob list and thus should not be retrieved at all
    if (m_UserRequest->GetRequestType() ==
                            CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        if (m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().IsExcludedBlob()) {
            m_Reply->PrepareBlobExcluded(
                m_Reply->GetItemId(),
                m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId, ePSGS_Excluded);
            x_SendReplyCompletion(true);
            m_Reply->Flush();
            return;
        }
    }

    CPubseqGatewayApp * app = CPubseqGatewayApp::GetInstance();
    SPSGS_BlobRequestBase::EPSGS_TSEOption
                        tse_option = SPSGS_BlobRequestBase::ePSGS_UnknownTSE;
    string                          client_id;
    SPSGS_BlobId                    blob_id;
    bool *                          exclude_blob_cache_added;
    CBlobRecord::TTimestamp         last_modified;
    TPSGS_HighResolutionTimePoint   start_timestamp;
    if (m_UserRequest->GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        tse_option = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_TSEOption;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ClientId;
        blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheAdded;
        last_modified = INT64_MIN;
        start_timestamp = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_StartTimestamp;
    } else {
        tse_option = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_TSEOption;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ClientId;
        blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheAdded;
        last_modified = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_LastModified;
        start_timestamp = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_StartTimestamp;
    }

    if (tse_option != SPSGS_BlobRequestBase::ePSGS_NoneTSE &&
        tse_option != SPSGS_BlobRequestBase::ePSGS_SlimTSE) {
        if (!client_id.empty()) {
            // Adding to exclude blob cache is unconditional however skipping
            // is only for the blobs identified by seq_id/seq_id_type
            bool        completed = true;
            auto        cache_result =
                app->GetExcludeBlobCache()->AddBlobId(client_id, blob_id.m_Sat,
                                                      blob_id.m_SatKey,
                                                      completed);
            if (m_UserRequest->GetRequestType() ==
                    CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
                if (cache_result == ePSGS_AlreadyInCache) {
                    if (completed)
                        m_Reply->PrepareBlobExcluded(blob_id, ePSGS_Sent);
                    else
                        m_Reply->PrepareBlobExcluded(blob_id, ePSGS_InProgress);
                    x_SendReplyCompletion(true);
                    m_Reply->Flush();
                    return;
                }
            }
            if (cache_result == ePSGS_Added)
                * exclude_blob_cache_added = true;
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    if (m_UserRequest->GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest)
        fetch_details.reset(
                new CCassBlobFetch(m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>()));
    else
        fetch_details.reset(
                new CCassBlobFetch(m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>()));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(m_UserRequest, m_Reply);
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        blob_id.m_Sat,
                                        blob_id.m_SatKey,
                                        last_modified,
                                        *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              blob_id.m_SatName,
                                              std::move(blob_record),
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    } else {
        if (m_UrlUseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
            // No data in cache and not going to the DB
            if (blob_prop_cache_lookup_result == ePSGS_NotFound)
                x_OnReplyError(CRequestStatus::e404_NotFound,
                               ePSGS_BlobPropsNotFound,
                               "Blob properties are not found",
                               start_timestamp);
            else
                x_OnReplyError(CRequestStatus::e500_InternalServerError,
                               ePSGS_BlobPropsNotFound,
                               "Blob properties are not found "
                               "due to a cache lookup error",
                               start_timestamp);

            if (* exclude_blob_cache_added &&
                !client_id.empty()) {
                app->GetExcludeBlobCache()->Remove(client_id, blob_id.m_Sat,
                                                   blob_id.m_SatKey);

                // To prevent SetCompleted() later
                * exclude_blob_cache_added = false;
            }
            return;
        }

        if (last_modified == INT64_MIN) {
            load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                                  app->GetCassandraMaxRetries(),
                                                  app->GetCassandraConnection(),
                                                  blob_id.m_SatName,
                                                  blob_id.m_SatKey,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        } else {
            load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                                  app->GetCassandraMaxRetries(),
                                                  app->GetCassandraConnection(),
                                                  blob_id.m_SatName,
                                                  blob_id.m_SatKey,
                                                  last_modified,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        }
    }

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPendingOperation::OnGetBlobError, this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(bind(&CPendingOperation::OnGetBlobProp, this, _1, _2, _3),
                          m_UserRequest, m_Reply, fetch_details.get(),
                          blob_prop_cache_lookup_result != ePSGS_Found));

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace("Cassandra request: " +
                  ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                  m_UserRequest->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(fetch_details));

    // Initiate cassandra request
    load_task->Wait();
}


void CPendingOperation::Peek(HST::CHttpReply<CPendingOperation>& resp,
                             bool  need_wait)
{
    if (m_Cancelled) {
        if (resp.IsOutputReady() && !resp.IsFinished()) {
            resp.Send(nullptr, 0, true, true);
        }
        return;
    }

    if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest ||
        m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_TSEChunkRequest ||
        m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest ||
        m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_AnnotationRequest) {
        m_Processor->ProcessEvent();
        if (m_Processor->IsFinished() && !resp.IsFinished() && resp.IsOutputReady() ) {
            x_SendReplyCompletion(true);
            m_Reply->Flush(true);
        }
        return;
    }


    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call resp->Send()  to send what we have if it is ready
    for (auto &  details: m_FetchDetails) {
        if (details)
            x_Peek(resp, need_wait, details);
    }

    bool    all_finished_read = x_AllFinishedRead();

    // For resolve and named annotations the packets need to be sent only when
    // all the data are in chunks
    auto    request_type = m_UserRequest->GetRequestType();
    bool    is_blob_request = (request_type == CPSGS_Request::ePSGS_BlobBySeqIdRequest) ||
                              (request_type == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest);
    if (resp.IsOutputReady()) {
       if (is_blob_request) {
           m_Reply->Flush(all_finished_read);
       } else {
            if (all_finished_read) {
                m_Reply->Flush();
            }
       }
    }

    if (is_blob_request && all_finished_read) {
        string              client_id;
        SPSGS_BlobId        blob_id;
        bool *              exclude_blob_cache_added;
        bool *              exclude_blob_cache_completed;
        if (request_type == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
            client_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ClientId;
            blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId;
            exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheAdded;
            exclude_blob_cache_completed = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheCompleted;
        } else {
            client_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ClientId;
            blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId;
            exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheAdded;
            exclude_blob_cache_completed = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheCompleted;
        }

        if (* exclude_blob_cache_added &&
            ! (* exclude_blob_cache_completed) &&
            !client_id.empty()) {
            CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
            app->GetExcludeBlobCache()->SetCompleted(client_id, blob_id.m_Sat,
                                                     blob_id.m_SatKey, true);
            * exclude_blob_cache_completed = true;
        }
    }
}


void CPendingOperation::x_PeekIfNeeded(void)
{
    HST::CHttpReply<CPendingOperation> *  reply = m_Reply->GetReply();

    if (reply->IsOutputReady())
        Peek(*reply, false);
}


void CPendingOperation::x_Peek(HST::CHttpReply<CPendingOperation>& resp,
                               bool  need_wait,
                               unique_ptr<CCassFetch> &  fetch_details)
{
    if (!fetch_details->GetLoader())
        return;

    if (need_wait)
        if (!fetch_details->ReadFinished())
            fetch_details->GetLoader()->Wait();

    if (fetch_details->GetLoader()->HasError() &&
        resp.IsOutputReady() && !resp.IsFinished()) {
        // Send an error
        string               error = fetch_details->GetLoader()->LastError();
        CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
        app->GetErrorCounters().IncUnknownError();

        PSG_ERROR(error);

        EPSGS_DbFetchType   fetch_type = fetch_details->GetFetchType();
        if (fetch_type == ePSGS_BlobBySeqIdFetch ||
            fetch_type == ePSGS_BlobBySatSatKeyFetch ||
            fetch_type == ePSGS_TSEChunkFetch) {
            CCassBlobFetch *  blob_fetch = static_cast<CCassBlobFetch *>(fetch_details.get());
            if (blob_fetch->IsBlobPropStage()) {
                m_Reply->PrepareBlobPropMessage(
                    blob_fetch, error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
                m_Reply->PrepareBlobPropCompletion(blob_fetch);
            } else {
                m_Reply->PrepareBlobMessage(
                    blob_fetch, error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
                m_Reply->PrepareBlobCompletion(blob_fetch);
            }
        } else if (fetch_type == ePSGS_AnnotationFetch ||
                   fetch_type == ePSGS_BioseqInfoFetch) {
            m_Reply->PrepareReplyMessage(
                    error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
        }

        // Mark finished
        fetch_details->SetReadFinished();

        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        x_SendReplyCompletion();
    }
}


bool CPendingOperation::x_AllFinishedRead(void) const
{
    size_t  started_count = 0;

    for (const auto &  details: m_FetchDetails) {
        if (details) {
            ++started_count;
            if (!details->ReadFinished())
                return false;
        }
    }

    return started_count != 0;
}


void CPendingOperation::x_SendReplyCompletion(bool  forced)
{
    // Send the reply completion only if needed
    if (x_AllFinishedRead() || forced) {
        m_Reply->PrepareReplyCompletion();

        // No need to set the context/engage context resetter: they're set outside
        x_PrintRequestStop();
    }
}


void CPendingOperation::x_PrintRequestStop(void)
{
    if (m_UserRequest->GetRequestContext().NotNull()) {
        CDiagContext::SetRequestContext(m_UserRequest->GetRequestContext());
        m_UserRequest->GetRequestContext()->SetReadOnly(false);
        m_UserRequest->GetRequestContext()->SetRequestStatus(m_UserRequest->GetOverallStatus());
        GetDiagContext().PrintRequestStop();
        m_UserRequest->GetRequestContext().Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


EPSGS_SeqIdParsingResult
CPendingOperation::x_ParseInputSeqId(CSeq_id &  seq_id, string &  err_msg)
{
    bool    need_trace = m_UserRequest->NeedTrace();

    try {
        seq_id.Set(m_UrlSeqId);
        if (need_trace)
            m_Reply->SendTrace("Parsing CSeq_id('" + m_UrlSeqId + "') succeeded",
                               m_UserRequest->GetStartTimestamp());

        if (m_UrlSeqIdType <= 0) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id finished OK (#1)",
                                   m_UserRequest->GetStartTimestamp());
            return ePSGS_ParsedOK;
        }

        // Check the parsed type with the given
        int16_t     eff_seq_id_type;
        if (x_GetEffectiveSeqIdType(seq_id, eff_seq_id_type, false)) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id finished OK (#2)",
                                   m_UserRequest->GetStartTimestamp());
            return ePSGS_ParsedOK;
        }

        // seq_id_type from URL and from CSeq_id differ
        CSeq_id_Base::E_Choice  seq_id_type = seq_id.Which();

        if (need_trace)
            m_Reply->SendTrace("CSeq_id provided type " + to_string(seq_id_type) +
                               " and URL provided seq_id_type " +
                               to_string(m_UrlSeqIdType) + " mismatch",
                               m_UserRequest->GetStartTimestamp());

        if (IsINSDCSeqIdType(m_UrlSeqIdType) &&
            IsINSDCSeqIdType(seq_id_type)) {
            // Both seq_id_types belong to INSDC
            if (need_trace) {
                m_Reply->SendTrace("Both types belong to INSDC types.\n"
                                   "Parsing CSeq_id finished OK (#3)",
                                   m_UserRequest->GetStartTimestamp());
            }
            return ePSGS_ParsedOK;
        }

        // Type mismatch: form the error message in case of resolution problems
        err_msg = "Seq_id '" + m_UrlSeqId +
                  "' possible type mismatch: the URL provides " +
                  to_string(m_UrlSeqIdType) +
                  " while the CSeq_Id detects it as " +
                  to_string(static_cast<int>(seq_id_type));
    } catch (...) {
        if (need_trace)
            m_Reply->SendTrace("Parsing CSeq_id('" + m_UrlSeqId + "') failed (exception)",
                               m_UserRequest->GetStartTimestamp());
    }

    // Second variation of Set()
    if (m_UrlSeqIdType > 0) {
        try {
            seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                       (CSeq_id_Base::E_Choice)(m_UrlSeqIdType),
                       m_UrlSeqId);
            if (need_trace) {
                m_Reply->SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                                   to_string(m_UrlSeqIdType) + ", '" + m_UrlSeqId +
                                   "') succeeded.\n"
                                   "Parsing CSeq_id finished OK (#4)",
                                   m_UserRequest->GetStartTimestamp());
            }
            return ePSGS_ParsedOK;
        } catch (...) {
            if (need_trace)
                m_Reply->SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                                   to_string(m_UrlSeqIdType) + ", '" + m_UrlSeqId +
                                   "') failed (exception)",
                                   m_UserRequest->GetStartTimestamp());
        }
    }

    if (need_trace) {
        m_Reply->SendTrace("Parsing CSeq_id finished FAILED",
                           m_UserRequest->GetStartTimestamp());
    }

    return ePSGS_ParseFailed;
}


void CPendingOperation::x_InitUrlIndentification(void)
{
    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_UrlSeqId = m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqId;
            m_UrlSeqIdType = m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType;
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_UrlSeqId = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId;
            m_UrlSeqIdType = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType;
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_UrlSeqId = m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqId;
            m_UrlSeqIdType = m_UserRequest->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType;
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Not handled request type " +
                       to_string(static_cast<int>(m_UserRequest->GetRequestType())));
    }
}


void
CPendingOperation::x_ResolveInputSeqId(SBioseqResolution &  bioseq_resolution,
                                       SResolveInputSeqIdError &  err)
{
    x_InitUrlIndentification();

    auto            app = CPubseqGatewayApp::GetInstance();
    string          parse_err_msg;
    CSeq_id         oslt_seq_id;
    auto            parsing_result = x_ParseInputSeqId(oslt_seq_id,
                                                       parse_err_msg);

    // The results of the ComposeOSLT are used in both cache and DB
    int16_t         effective_seq_id_type;
    list<string>    secondary_id_list;
    string          primary_id;
    bool            composed_ok = false;
    if (parsing_result == ePSGS_ParsedOK) {
        composed_ok = x_ComposeOSLT(oslt_seq_id, effective_seq_id_type,
                                    secondary_id_list, primary_id);
    }

    if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly) {
        // Try cache
        auto    start = chrono::high_resolution_clock::now();

        if (composed_ok)
            x_ResolveViaComposeOSLTInCache(oslt_seq_id, effective_seq_id_type,
                                           secondary_id_list, primary_id,
                                           err, bioseq_resolution);
        else
            x_ResolveAsIsInCache(bioseq_resolution, err);

        if (bioseq_resolution.IsValid()) {
            // Special case for the seq_id like gi|156232
            bool    continue_with_cassandra = false;
            if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
                if (!CanSkipBioseqInfoRetrieval(bioseq_resolution.m_BioseqInfo)) {
                    // This is an optimization. Try to find the record in the
                    // BIOSEQ_INFO only if needed.
                    CPSGCache   psg_cache(true, m_UserRequest, m_Reply);
                    auto        bioseq_cache_lookup_result =
                                    psg_cache.LookupBioseqInfo(bioseq_resolution);

                    if (bioseq_cache_lookup_result != ePSGS_Found) {
                        // Not found or error
                        continue_with_cassandra = true;
                        bioseq_resolution.Reset();
                    } else {
                        bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;

                        auto    adj_result = AdjustBioseqAccession(bioseq_resolution);
                        if (adj_result == ePSGS_LogicError ||
                            adj_result == ePSGS_SeqIdsEmpty) {
                            continue_with_cassandra = true;
                            bioseq_resolution.Reset();
                        }
                    }
                }
            } else {
                // The result is coming from the BIOSEQ_INFO cache. Need to try
                // the adjustment
                auto    adj_result = AdjustBioseqAccession(bioseq_resolution);
                if (adj_result == ePSGS_LogicError ||
                    adj_result == ePSGS_SeqIdsEmpty) {
                    continue_with_cassandra = true;
                    bioseq_resolution.Reset();
                }
            }

            if (!continue_with_cassandra) {
                app->GetTiming().Register(eResolutionLmdb, eOpStatusFound, start);
                return;
            }
        }
        app->GetTiming().Register(eResolutionLmdb, eOpStatusNotFound, start);
    }

    if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_CacheOnly) {
        // Need to initiate async DB resolution

        // Memorize an error if there was one
        if (!parse_err_msg.empty() && !err.HasError()) {
            err.m_ErrorMessage = parse_err_msg;
            err.m_ErrorCode = CRequestStatus::e404_NotFound;
        }

        bioseq_resolution.m_PostponedError = std::move(err);
        err.Reset();

        // Async request
        m_AsyncSeqIdResolver.reset(new CAsyncSeqIdResolver(
                                            oslt_seq_id, effective_seq_id_type,
                                            std::move(secondary_id_list),
                                            std::move(primary_id),
                                            m_UrlSeqId, composed_ok,
                                            std::move(bioseq_resolution),
                                            this,
                                            m_UserRequest,
                                            m_Reply));
        m_AsyncCassResolutionStart = chrono::high_resolution_clock::now();
        m_AsyncSeqIdResolver->Process();


        // The CAsyncSeqIdResolver moved the bioseq_resolution, so it is safe
        // to signal the return here
        bioseq_resolution.Reset();
        bioseq_resolution.m_ResolutionResult = ePSGS_PostponedForDB;
        return;
    }

    // Finished with resolution:
    // - not found
    // - parsing error
    // - LMDB error
    app->GetRequestCounters().IncNotResolved();

    if (!parse_err_msg.empty()) {
        if (!err.HasError()) {
            err.m_ErrorMessage = parse_err_msg;
            err.m_ErrorCode = CRequestStatus::e404_NotFound;
        }
    }
}


EPSGS_CacheLookupResult CPendingOperation::x_ResolvePrimaryOSLTInCache(
                                const string &  primary_id,
                                int16_t  effective_version,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution)
{
    EPSGS_CacheLookupResult     bioseq_cache_lookup_result = ePSGS_NotFound;

    if (!primary_id.empty()) {
        CPSGCache           psg_cache(true, m_UserRequest, m_Reply);

        // Try BIOSEQ_INFO
        bioseq_resolution.m_BioseqInfo.SetAccession(primary_id);
        bioseq_resolution.m_BioseqInfo.SetVersion(effective_version);
        bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

        bioseq_cache_lookup_result = psg_cache.LookupBioseqInfo(
                                        bioseq_resolution);
        if (bioseq_cache_lookup_result == ePSGS_Found) {
            bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;
            return ePSGS_Found;
        }

        bioseq_resolution.Reset();
    }
    return bioseq_cache_lookup_result;
}


EPSGS_CacheLookupResult CPendingOperation::x_ResolveSecondaryOSLTInCache(
                                const string &  secondary_id,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution)
{
    bioseq_resolution.m_BioseqInfo.SetAccession(secondary_id);
    bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

    CPSGCache   psg_cache(true, m_UserRequest, m_Reply);
    auto        si2csi_cache_lookup_result =
                        psg_cache.LookupSi2csi(bioseq_resolution);
    if (si2csi_cache_lookup_result == ePSGS_Found) {
        bioseq_resolution.m_ResolutionResult = ePSGS_Si2csiCache;
        return ePSGS_Found;
    }

    bioseq_resolution.Reset();

    if (si2csi_cache_lookup_result == ePSGS_Failure)
        return ePSGS_Failure;
    return ePSGS_NotFound;
}


bool
CPendingOperation::x_ComposeOSLT(CSeq_id &  parsed_seq_id,
                                 int16_t &  effective_seq_id_type,
                                 list<string> &  secondary_id_list,
                                 string &  primary_id)
{
    bool    need_trace = m_UserRequest->NeedTrace();

    if (!x_GetEffectiveSeqIdType(parsed_seq_id,
                                 effective_seq_id_type, need_trace)) {
        if (need_trace) {
            m_Reply->SendTrace("OSLT has not been tried due to mismatch between the "
                               " parsed CSeq_id seq_id_type and the URL provided one",
                               m_UserRequest->GetStartTimestamp());
        }
        return false;
    }

    try {
        primary_id = parsed_seq_id.ComposeOSLT(&secondary_id_list,
                                               CSeq_id::fGpipeAddSecondary);
    } catch (...) {
        if (need_trace) {
            m_Reply->SendTrace("OSLT call failure (exception)",
                               m_UserRequest->GetStartTimestamp());
        }
        return false;
    }

    if (need_trace) {
        string  trace_msg("OSLT succeeded");
        trace_msg += "\nOSLT primary id: " + primary_id;

        if (secondary_id_list.empty()) {
            trace_msg += "\nOSLT secondary id list is empty";
        } else {
            for (const auto &  item : secondary_id_list) {
                trace_msg += "\nOSLT secondary id: " + item;
            }
        }
        m_Reply->SendTrace(trace_msg, m_UserRequest->GetStartTimestamp());
    }

    return true;
}


void
CPendingOperation::x_ResolveViaComposeOSLTInCache(CSeq_id &  parsed_seq_id,
                                                  int16_t  effective_seq_id_type,
                                                  const list<string> &  secondary_id_list,
                                                  const string &  primary_id,
                                                  SResolveInputSeqIdError &  err,
                                                  SBioseqResolution &  bioseq_resolution)
{
    const CTextseq_id *     text_seq_id = parsed_seq_id.GetTextseq_Id();
    int16_t                 effective_version = GetEffectiveVersion(text_seq_id);
    bool                    cache_failure = false;

    if (!primary_id.empty()) {
        auto    cache_lookup_result =
                    x_ResolvePrimaryOSLTInCache(primary_id, effective_version,
                                                effective_seq_id_type,
                                                bioseq_resolution);
        if (cache_lookup_result == ePSGS_Found)
            return;
        if (cache_lookup_result == ePSGS_Failure)
            cache_failure = true;
    }

    for (const auto &  secondary_id : secondary_id_list) {
        auto    cache_lookup_result =
                    x_ResolveSecondaryOSLTInCache(secondary_id,
                                                  effective_seq_id_type,
                                                  bioseq_resolution);
        if (cache_lookup_result == ePSGS_Found)
            return;
        if (cache_lookup_result == ePSGS_Failure) {
            cache_failure = true;
            break;
        }
    }

    // Try cache as it came from URL
    // The primary id may match the URL given seq_id so it makes sense to
    // exclude trying the very same string in x_ResolveAsIsInCache(). The
    // x_ResolveAsIsInCache() capitalizes the url seq id so the capitalized
    // versions need to be compared
    string      upper_seq_id(m_UrlSeqId.data(), m_UrlSeqId.size());
    NStr::ToUpper(upper_seq_id);
    bool        need_as_is = primary_id != upper_seq_id;
    auto        cache_lookup_result =
                    x_ResolveAsIsInCache(bioseq_resolution, err, need_as_is);
    if (cache_lookup_result == ePSGS_Found)
        return;
    if (cache_lookup_result == ePSGS_Failure)
        cache_failure = true;

    bioseq_resolution.Reset();

    if (cache_failure) {
        err.m_ErrorMessage = "Cache lookup failure";
        err.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }
}


EPSGS_CacheLookupResult
CPendingOperation::x_ResolveAsIsInCache(SBioseqResolution &  bioseq_resolution,
                                        SResolveInputSeqIdError &  err,
                                        bool  need_as_is)
{
    EPSGS_CacheLookupResult     cache_lookup_result = ePSGS_NotFound;

    // Capitalize seq_id
    string      upper_seq_id(m_UrlSeqId.data(), m_UrlSeqId.size());
    NStr::ToUpper(upper_seq_id);

    // 1. As is
    if (need_as_is == true) {
        cache_lookup_result = x_ResolveSecondaryOSLTInCache(upper_seq_id,
                                                            m_UrlSeqIdType,
                                                            bioseq_resolution);
    }

    if (cache_lookup_result == ePSGS_NotFound) {
        // 2. if there are | at the end => strip all trailing bars
        //    else => add one | at the end
        if (upper_seq_id[upper_seq_id.size() - 1] == '|') {
            string  strip_bar_seq_id(upper_seq_id);
            while (strip_bar_seq_id[strip_bar_seq_id.size() - 1] == '|')
                strip_bar_seq_id.erase(strip_bar_seq_id.size() - 1, 1);
            cache_lookup_result = x_ResolveSecondaryOSLTInCache(strip_bar_seq_id,
                                                                m_UrlSeqIdType,
                                                                bioseq_resolution);
        } else {
            string      seq_id_added_bar(upper_seq_id);
            seq_id_added_bar.append(1, '|');
            cache_lookup_result = x_ResolveSecondaryOSLTInCache(seq_id_added_bar,
                                                                m_UrlSeqIdType,
                                                                bioseq_resolution);
        }
    }

    if (cache_lookup_result == ePSGS_Failure) {
        bioseq_resolution.Reset();
        err.m_ErrorMessage = "Cache lookup failure";
        err.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }

    return cache_lookup_result;
}


bool CPendingOperation::x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                                int16_t &  eff_seq_id_type,
                                                bool  need_trace)
{
    CSeq_id_Base::E_Choice  parsed_seq_id_type = parsed_seq_id.Which();
    bool                    parsed_seq_id_type_found = (parsed_seq_id_type !=
                                                        CSeq_id_Base::e_not_set);

    if (!parsed_seq_id_type_found && m_UrlSeqIdType < 0) {
        eff_seq_id_type = -1;
        return true;
    }

    if (!parsed_seq_id_type_found) {
        eff_seq_id_type = m_UrlSeqIdType;
        return true;
    }

    if (m_UrlSeqIdType < 0) {
        eff_seq_id_type = parsed_seq_id_type;
        return true;
    }

    // Both found
    if (parsed_seq_id_type == m_UrlSeqIdType) {
        eff_seq_id_type = m_UrlSeqIdType;
        return true;
    }

    // The parsed and url explicit seq_id_type do not match
    if (IsINSDCSeqIdType(parsed_seq_id_type) &&
        IsINSDCSeqIdType(m_UrlSeqIdType)) {
        if (need_trace) {
            m_Reply->SendTrace(
                "Seq id type mismatch. Parsed CSeq_id reports seq_id_type as " +
                to_string(parsed_seq_id_type) + " while the URL reports " +
                to_string(m_UrlSeqIdType) +". They both belong to INSDC types so "
                "CSeq_id provided type " + to_string(parsed_seq_id_type) +
                " is taken as an effective one",
                m_UserRequest->GetStartTimestamp());
        }
        eff_seq_id_type = parsed_seq_id_type;
        return true;
    }

    return false;
}


int16_t CPendingOperation::GetEffectiveVersion(const CTextseq_id *  text_seq_id)
{
    try {
        if (text_seq_id == nullptr)
            return -1;
        if (text_seq_id->CanGetVersion())
            return text_seq_id->GetVersion();
    } catch (...) {
    }
    return -1;
}


void CPendingOperation::x_SendBioseqInfo(
        SBioseqResolution &  bioseq_resolution,
        SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format)
{
    auto    effective_output_format = output_format;

    if (output_format == SPSGS_ResolveRequest::ePSGS_NativeFormat ||
        output_format == SPSGS_ResolveRequest::ePSGS_UnknownFormat) {
        effective_output_format = SPSGS_ResolveRequest::ePSGS_JsonFormat;
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);

    // Convert to the appropriate format
    string              data_to_send;
    if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat) {
        data_to_send = ToJson(bioseq_resolution.m_BioseqInfo,
                              x_GetBioseqInfoFields()).Repr(CJsonNode::fStandardJson);
    } else {
        data_to_send = ToBioseqProtobuf(bioseq_resolution.m_BioseqInfo);
    }

    if (m_UserRequest->UsePsgProtocol()) {
        // Send it as the PSG protocol
        size_t              item_id = m_Reply->GetItemId();
        m_Reply->PrepareBioseqData(item_id, data_to_send,
                                   effective_output_format);
        m_Reply->PrepareBioseqCompletion(item_id, 2);
    } else {
        // Send it as the HTTP data
        if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat)
            m_Reply->SendData(data_to_send, ePSGS_JsonMime);
        else
            m_Reply->SendData(data_to_send, ePSGS_BinaryMime);
    }
}


bool
CPendingOperation::x_SatToSatName(const SPSGS_BlobBySeqIdRequest &  blob_request,
                                  SPSGS_BlobId &  blob_id)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToSatName(blob_id.m_Sat, blob_id.m_SatName))
        return true;

    app->GetErrorCounters().IncServerSatToSatName();

    // It is a server error - data inconsistency
    x_SendBlobPropError(
            m_Reply->GetItemId(),
            "Unknown satellite number " + to_string(blob_id.m_Sat) +
            " for bioseq info with seq_id '" + blob_request.m_SeqId + "'",
            ePSGS_UnknownResolvedSatellite);
    return false;
}


void CPendingOperation::x_SendBlobPropError(size_t  item_id,
                                            const string &  msg,
                                            int  err_code)
{
    m_Reply->PrepareBlobPropMessage(
        item_id, msg, CRequestStatus::e500_InternalServerError, err_code, eDiag_Error);
    m_Reply->PrepareBlobPropCompletion(item_id, 2);
    m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);
}


void CPendingOperation::x_SendReplyError(const string &  msg,
                                         CRequestStatus::ECode  status,
                                         int  code)
{
    m_Reply->PrepareReplyMessage(msg, status, code, eDiag_Error);
    m_UserRequest->UpdateOverallStatus(status);

    if (status >= CRequestStatus::e400_BadRequest &&
        status < CRequestStatus::e500_InternalServerError) {
        PSG_WARNING(msg);
    } else {
        PSG_ERROR(msg);
    }
}

//
// Blob callbacks
//

void CPendingOperation::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                      CBlobRecord const &  blob, bool is_found)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace("Blob prop callback; found: " + to_string(is_found),
                           m_UserRequest->GetStartTimestamp());
    }

    if (is_found) {
        // Found, send blob props back as JSON
        m_Reply->PrepareBlobPropData(
            fetch_details,  ToJson(blob).Repr(CJsonNode::fStandardJson));

        // Note: initially only blob_props are requested and at that moment the
        //       TSE option is 'known'. So the initial request should be
        //       finished, see m_FinishedRead = true
        //       In the further requests of the blob data regardless with blob
        //       props or not, the TSE option is set to unknown so the request
        //       will be finished at the moment when blob chunks are handled.
        switch (fetch_details->GetTSEOption()) {
            case SPSGS_BlobRequestBase::ePSGS_NoneTSE:
                x_OnBlobPropNoneTSE(fetch_details);
                break;
            case SPSGS_BlobRequestBase::ePSGS_SlimTSE:
                x_OnBlobPropSlimTSE(fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_SmartTSE:
                x_OnBlobPropSmartTSE(fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_WholeTSE:
                x_OnBlobPropWholeTSE(fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_OrigTSE:
                x_OnBlobPropOrigTSE(fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_UnknownTSE:
                // Used when INFO blobs are asked; i.e. chunks have been
                // requested as well, so only the prop completion message needs
                // to be sent.
                m_Reply->PrepareBlobPropCompletion(fetch_details);
                break;
        }
    } else {
        x_OnBlobPropNotFound(fetch_details);
    }

    x_PeekIfNeeded();
}


void CPendingOperation::x_OnBlobPropNotFound(CCassBlobFetch *  fetch_details)
{
    // Not found, report 500 because it is data inconsistency
    // or 404 if it was requested via sat.sat_key
    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
    app->GetErrorCounters().IncBlobPropsNotFoundError();

    SPSGS_BlobId    blob_id = fetch_details->GetBlobId();
    string          message = "Blob " + blob_id.ToString() +
                              " properties are not found";
    if (fetch_details->GetFetchType() == ePSGS_BlobBySatSatKeyFetch) {
        // User requested wrong sat_key, so it is a client error
        PSG_WARNING(message);
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_Reply->PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e404_NotFound,
                                ePSGS_BlobPropsNotFound, eDiag_Error);
    } else {
        // Server error, data inconsistency
        PSG_ERROR(message);
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Reply->PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_BlobPropsNotFound, eDiag_Error);
    }

    SPSGS_BlobId    request_blob_id;
    string          client_id;
    bool *          exclude_blob_cache_added;
    if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheAdded;
    } else {
        request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheAdded;
    }

    if (blob_id == request_blob_id) {
        if ((* exclude_blob_cache_added) &&
            !client_id.empty()) {
            app->GetExcludeBlobCache()->Remove(client_id,
                                               blob_id.m_Sat, blob_id.m_SatKey);
            * exclude_blob_cache_added = false;
        }
    }

    m_Reply->PrepareBlobPropCompletion(fetch_details);

    fetch_details->SetReadFinished();
    x_SendReplyCompletion();
}


void CPendingOperation::x_OnBlobPropNoneTSE(CCassBlobFetch *  fetch_details)
{
    fetch_details->SetReadFinished();
    // Nothing else to be sent;
    m_Reply->PrepareBlobPropCompletion(fetch_details);
    x_SendReplyCompletion();
}


void CPendingOperation::x_OnBlobPropSlimTSE(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob)
{
    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
    SPSGS_BlobId            blob_id = fetch_details->GetBlobId();

    SPSGS_BlobId    request_blob_id;
    string          client_id;
    bool *          exclude_blob_cache_added;
    if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheAdded;
    } else {
        request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId;
        client_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheAdded;
    }

    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        m_Reply->PrepareBlobPropCompletion(fetch_details);

        // An original blob may be required if its size is small
        unsigned int         slim_max_blob_size = app->GetSlimMaxBlobSize();

        if (blob.GetSize() <= slim_max_blob_size) {
            // The blob is small, get it, but first check in the
            // exclude blob cache
            if (blob_id == request_blob_id && !client_id.empty()) {
                bool        completed = true;
                auto        cache_result =
                    app->GetExcludeBlobCache()->AddBlobId(
                        client_id, blob_id.m_Sat, blob_id.m_SatKey, completed);
                if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest &&
                        cache_result == ePSGS_AlreadyInCache) {
                    if (completed)
                        m_Reply->PrepareBlobExcluded(blob_id, ePSGS_Sent);
                    else
                        m_Reply->PrepareBlobExcluded(blob_id, ePSGS_InProgress);
                    x_SendReplyCompletion(true);
                    return;
                }

                if (cache_result == ePSGS_Added)
                    * exclude_blob_cache_added = true;
            }

            x_RequestOriginalBlobChunks(fetch_details, blob);
        } else {
            // Nothing else to be sent, the original blob is big
            x_SendReplyCompletion();
        }
    } else {
        // Check the cache first - only if it is about the main
        // blob request
        if (blob_id == request_blob_id && !client_id.empty()) {
            bool        completed = true;
            auto        cache_result =
                app->GetExcludeBlobCache()->AddBlobId(
                        client_id, blob_id.m_Sat, blob_id.m_SatKey, completed);
            if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest &&
                    cache_result == ePSGS_AlreadyInCache) {
                m_Reply->PrepareBlobPropCompletion(fetch_details);
                if (completed)
                    m_Reply->PrepareBlobExcluded(blob_id, ePSGS_Sent);
                else
                    m_Reply->PrepareBlobExcluded(blob_id, ePSGS_InProgress);
                x_SendReplyCompletion(true);
                return;
            }

            if (cache_result == ePSGS_Added)
                * exclude_blob_cache_added = true;
        }

        // Not in the cache, request the split INFO blob only
        x_RequestID2BlobChunks(fetch_details, blob, true);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_Reply->PrepareBlobPropCompletion(fetch_details);
    }
}


void CPendingOperation::x_OnBlobPropSmartTSE(CCassBlobFetch *  fetch_details,
                                             CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_Reply->PrepareBlobPropCompletion(fetch_details);
        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        // Request the split INFO blob only
        x_RequestID2BlobChunks(fetch_details, blob, true);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_Reply->PrepareBlobPropCompletion(fetch_details);
    }
}


void CPendingOperation::x_OnBlobPropWholeTSE(CCassBlobFetch *  fetch_details,
                                             CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_Reply->PrepareBlobPropCompletion(fetch_details);
        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        // Request the split INFO blob and all split chunks
        x_RequestID2BlobChunks(fetch_details, blob, false);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_Reply->PrepareBlobPropCompletion(fetch_details);
    }
}

void CPendingOperation::x_OnBlobPropOrigTSE(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    // Request original blob chunks
    m_Reply->PrepareBlobPropCompletion(fetch_details);
    x_RequestOriginalBlobChunks(fetch_details, blob);
}


void CPendingOperation::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  /*blob*/,
                                       const unsigned char *  chunk_data,
                                       unsigned int  data_size, int  chunk_no)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->SetReadFinished();
        return;
    }
    if (m_Reply->IsReplyFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        return;
    }

    if (chunk_no >= 0) {
        if (m_UserRequest->NeedTrace()) {
            m_Reply->SendTrace("Blob chunk " + to_string(chunk_no) + " callback",
                               m_UserRequest->GetStartTimestamp());
        }

        // A blob chunk; 0-length chunks are allowed too
        m_Reply->PrepareBlobData(fetch_details,
                                 chunk_data, data_size, chunk_no);
    } else {
        if (m_UserRequest->NeedTrace()) {
            m_Reply->SendTrace("Blob chunk no-more-data callback",
                               m_UserRequest->GetStartTimestamp());
        }

        // End of the blob
        m_Reply->PrepareBlobCompletion(fetch_details);
        fetch_details->SetReadFinished();

        x_SendReplyCompletion();

        // Note: no need to set the blob completed in the exclude blob cache.
        // It will happen in Peek()
    }

    x_PeekIfNeeded();
}


void CPendingOperation::OnGetBlobError(
                        CCassBlobFetch *  fetch_details,
                        CRequestStatus::ECode  status, int  code,
                        EDiagSev  severity, const string &  message)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (status >= CRequestStatus::e400_BadRequest &&
        status < CRequestStatus::e500_InternalServerError) {
        PSG_WARNING(message);
    } else {
        PSG_ERROR(message);
    }

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace("Blob error callback; status " + to_string(status),
                           m_UserRequest->GetStartTimestamp());
    }

    if (status == CRequestStatus::e404_NotFound) {
        app->GetErrorCounters().IncGetBlobNotFound();
    } else {
        if (is_error) {
            if (code == CCassandraException::eQueryTimeout)
                app->GetErrorCounters().IncCassQueryTimeoutError();
            else
                app->GetErrorCounters().IncUnknownError();
        }
    }

    if (is_error) {
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        if (fetch_details->IsBlobPropStage()) {
            m_Reply->PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_Reply->PrepareBlobPropCompletion(fetch_details);
        } else {
            m_Reply->PrepareBlobMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_Reply->PrepareBlobCompletion(fetch_details);
        }

        SPSGS_BlobId    request_blob_id;
        string          client_id;
        bool *          exclude_blob_cache_added;
        if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
            request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_BlobId;
            client_id = m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ClientId;
            exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_ExcludeBlobCacheAdded;
        } else {
            request_blob_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId;
            client_id = m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ClientId;
            exclude_blob_cache_added = & m_UserRequest->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_ExcludeBlobCacheAdded;
        }

        SPSGS_BlobId    blob_id = fetch_details->GetBlobId();
        if (blob_id == request_blob_id) {
            if ((* exclude_blob_cache_added) && !client_id.empty()) {
                app->GetExcludeBlobCache()->Remove(client_id, blob_id.m_Sat,
                                                   blob_id.m_SatKey);

                // To prevent any updates
                * exclude_blob_cache_added = false;
            }
        }

        // If it is an error then regardless what stage it was, props or
        // chunks, there will be no more activity
        fetch_details->SetReadFinished();
    } else {
        if (fetch_details->IsBlobPropStage())
            m_Reply->PrepareBlobPropMessage(fetch_details, message,
                                            status, code, severity);
        else
            m_Reply->PrepareBlobMessage(fetch_details, message,
                                        status, code, severity);
    }

    x_SendReplyCompletion();
    x_PeekIfNeeded();
}


void CPendingOperation::x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                                    CBlobRecord const &  blob)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_UserRequest->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    // eUnknownTSE is safe here; no blob prop call will happen anyway
    // eUnknownUseCache is safe here; no further resolution required
    SPSGS_BlobBySatSatKeyRequest
            orig_blob_request(fetch_details->GetBlobId(),
                              blob.GetModified(),
                              SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                              SPSGS_RequestBase::ePSGS_UnknownUseCache,
                              fetch_details->GetClientId(),
                              trace_flag,
                              chrono::high_resolution_clock::now());

    // Create the cass async loader
    unique_ptr<CBlobRecord>             blob_record(new CBlobRecord(blob));
    CCassBlobTaskLoadBlob *             load_task =
        new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                  app->GetCassandraMaxRetries(),
                                  app->GetCassandraConnection(),
                                  orig_blob_request.m_BlobId.m_SatName,
                                  std::move(blob_record),
                                  true, nullptr);

    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(orig_blob_request));
    cass_blob_fetch->SetLoader(load_task);

    // Blob props have already been rceived
    cass_blob_fetch->SetBlobPropSent();

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPendingOperation::OnGetBlobError, this, _1, _2, _3, _4, _5),
                              cass_blob_fetch.get()));
    load_task->SetPropsCallback(nullptr);
    load_task->SetChunkCallback(
        CBlobChunkCallback(bind(&CPendingOperation::OnGetBlobChunk, this, _1, _2, _3, _4, _5),
                           cass_blob_fetch.get()));

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace(
            "Cassandra request: " + ToJson(*load_task).Repr(CJsonNode::fStandardJson),
            m_UserRequest->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(cass_blob_fetch));

    load_task->Wait();
}


void CPendingOperation::x_RequestID2BlobChunks(CCassBlobFetch *  fetch_details,
                                               CBlobRecord const &  blob,
                                               bool  info_blob_only)
{
    if (!x_ParseId2Info(fetch_details, blob))
        return;

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();

    // Translate sat to keyspace
    SPSGS_BlobId    info_blob_id(m_Id2Info->GetSat(), m_Id2Info->GetInfo());    // mandatory

    if (!app->SatToSatName(m_Id2Info->GetSat(), info_blob_id.m_SatName)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              to_string(m_Id2Info->GetSat()) +
                              ") to a cassandra keyspace for the blob " +
                              fetch_details->GetBlobId().ToString();
        m_Reply->PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_BadID2Info, eDiag_Error);
        app->GetErrorCounters().IncServerSatToSatName();
        m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
        return;
    }

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_UserRequest->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    // Create the Id2Info requests.
    // eUnknownTSE is treated in the blob prop handler as to do nothing (no
    // sending completion message, no requesting other blobs)
    // eUnknownUseCache is safe here; no further resolution
    // empty client_id means that later on there will be no exclude blobs cache ops
    SPSGS_BlobBySatSatKeyRequest
        info_blob_request(info_blob_id, INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          "", trace_flag,
                          chrono::high_resolution_clock::now());

    // Prepare Id2Info retrieval
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(info_blob_request));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_UserRequest, m_Reply);
    auto                        blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        info_blob_request.m_BlobId.m_Sat,
                                        info_blob_request.m_BlobId.m_SatKey,
                                        info_blob_request.m_LastModified,
                                        *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;


    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        load_task = new CCassBlobTaskLoadBlob(
                        app->GetCassandraTimeout(),
                        app->GetCassandraMaxRetries(),
                        app->GetCassandraConnection(),
                        info_blob_request.m_BlobId.m_SatName,
                        std::move(blob_record),
                        true, nullptr);
    } else {
        if (m_UrlUseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
            // No need to continue; it is forbidded to look for blob props in
            // the Cassandra DB
            string      message;

            if (blob_prop_cache_lookup_result == ePSGS_NotFound) {
                message = "Blob properties are not found";
                m_UserRequest->UpdateOverallStatus(CRequestStatus::e404_NotFound);
                m_Reply->PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e404_NotFound,
                                    ePSGS_BlobPropsNotFound, eDiag_Error);
            } else {
                message = "Blob properties are not found due to LMDB cache error";
                m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                m_Reply->PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e500_InternalServerError,
                                    ePSGS_BlobPropsNotFound, eDiag_Error);
            }

            PSG_WARNING(message);
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(
                        app->GetCassandraTimeout(),
                        app->GetCassandraMaxRetries(),
                        app->GetCassandraConnection(),
                        info_blob_request.m_BlobId.m_SatName,
                        info_blob_request.m_BlobId.m_SatKey,
                        true, nullptr);
    }
    cass_blob_fetch->SetLoader(load_task);

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPendingOperation::OnGetBlobError, this, _1, _2, _3, _4, _5),
                              cass_blob_fetch.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(bind(&CPendingOperation::OnGetBlobProp, this, _1, _2, _3),
                          m_UserRequest, m_Reply, cass_blob_fetch.get(),
                          blob_prop_cache_lookup_result != ePSGS_Found));
    load_task->SetChunkCallback(
        CBlobChunkCallback(bind(&CPendingOperation::OnGetBlobChunk, this, _1, _2, _3, _4, _5),
                           cass_blob_fetch.get()));

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace("Cassandra request: " +
                           ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                           m_UserRequest->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(cass_blob_fetch));
    auto    to_init_iter = m_FetchDetails.end();
    --to_init_iter;

    // We may need to request ID2 chunks
    if (!info_blob_only) {
        // Sat name is the same
        x_RequestId2SplitBlobs(fetch_details, info_blob_id.m_SatName);
    }

    // initiate retrieval: only those which were just created
    while (to_init_iter != m_FetchDetails.end()) {
        if (*to_init_iter)
            (*to_init_iter)->GetLoader()->Wait();
        ++to_init_iter;
    }
}


void CPendingOperation::x_RequestId2SplitBlobs(CCassBlobFetch *  fetch_details,
                                               const string &  sat_name)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_UserRequest->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    for (int  chunk_no = 1; chunk_no <= m_Id2Info->GetChunks(); ++chunk_no) {
        SPSGS_BlobId    chunks_blob_id(m_Id2Info->GetSat(),
                                       m_Id2Info->GetInfo() - m_Id2Info->GetChunks() - 1 + chunk_no);
        chunks_blob_id.m_SatName = sat_name;

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        // client_id is "" (empty string) so the split blobs do not participate
        // in the exclude blob cache
        SPSGS_BlobBySatSatKeyRequest
            chunk_request(chunks_blob_id, INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          "", trace_flag,
                          chrono::high_resolution_clock::now());

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_UserRequest, m_Reply);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            chunk_request.m_BlobId.m_Sat,
                                            chunk_request.m_BlobId.m_SatKey,
                                            chunk_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == ePSGS_Found) {
            load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunk_request.m_BlobId.m_SatName,
                            std::move(blob_record),
                            true, nullptr);
            details->SetLoader(load_task);
        } else {
            if (m_UrlUseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
                // No need to create a request because the Cassandra DB access
                // is forbidden
                string      message;
                if (blob_prop_cache_lookup_result == ePSGS_NotFound) {
                    message = "Blob properties are not found";
                    m_UserRequest->UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    m_Reply->PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e404_NotFound,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    m_Reply->PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e500_InternalServerError,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                }
                m_Reply->PrepareBlobPropCompletion(details.get());
                PSG_WARNING(message);
                continue;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunk_request.m_BlobId.m_SatName,
                            chunk_request.m_BlobId.m_SatKey,
                            true, nullptr);
            details->SetLoader(load_task);
        }

        load_task->SetDataReadyCB(GetDataReadyCB());
        load_task->SetErrorCB(
            CGetBlobErrorCallback(bind(&CPendingOperation::OnGetBlobError, this, _1, _2, _3, _4, _5),
                                  details.get()));
        load_task->SetPropsCallback(
            CBlobPropCallback(bind(&CPendingOperation::OnGetBlobProp, this, _1, _2, _3),
                              m_UserRequest, m_Reply, details.get(),
                              blob_prop_cache_lookup_result != ePSGS_Found));
        load_task->SetChunkCallback(
            CBlobChunkCallback(bind(&CPendingOperation::OnGetBlobChunk, this, _1, _2, _3, _4, _5),
                               details.get()));

        m_FetchDetails.push_back(std::move(details));
    }
}


bool CPendingOperation::x_ParseId2Info(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  blob)
{
    string      err_msg;
    try {
        m_Id2Info.reset(new CPSGId2Info(blob.GetId2Info()));
        return true;
    } catch (const exception &  exc) {
        err_msg = "Error extracting id2 info for the blob " +
            fetch_details->GetBlobId().ToString() + ": " + exc.what();
    } catch (...) {
        err_msg = "Unknown error extracting id2 info for the blob " +
            fetch_details->GetBlobId().ToString() + ".";
    }

    CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncInvalidId2InfoError();
    m_Reply->PrepareBlobPropMessage(
        fetch_details, err_msg, CRequestStatus::e500_InternalServerError,
        ePSGS_BadID2Info, eDiag_Error);
    m_UserRequest->UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
    return false;
}


void CPendingOperation::OnSeqIdAsyncResolutionFinished(
                                SBioseqResolution &&  async_bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    auto    app = CPubseqGatewayApp::GetInstance();

    if (!async_bioseq_resolution.IsValid()) {
        app->GetTiming().Register(eResolutionCass, eOpStatusNotFound,
                                  m_AsyncCassResolutionStart);
        app->GetRequestCounters().IncNotResolved();
    } else {
        app->GetTiming().Register(eResolutionCass, eOpStatusFound,
                                  m_AsyncCassResolutionStart);
    }

    // The results are populated by CAsyncSeqIdResolver in
    // m_PostponedSeqIdResolution
    switch (m_AsyncInterruptPoint) {
        case eGetSeqIdResolution:
            x_ProcessGetRequest(async_bioseq_resolution.m_PostponedError,
                                async_bioseq_resolution);
            break;
        default:
            ;
    }
}


void CPendingOperation::OnSeqIdAsyncError(
                            CRequestStatus::ECode  status, int  code,
                            EDiagSev  severity, const string &  message,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eGetSeqIdResolution:
            x_OnReplyError(status, code, message, start_timestamp);
            return;
        default:
            ;
    }
}


void CPendingOperation::x_RegisterResolveTiming(
                                const SBioseqResolution &  bioseq_resolution)
{
    if (bioseq_resolution.m_RequestStartTimestamp.time_since_epoch().count() != 0) {
        auto    app = CPubseqGatewayApp::GetInstance();
        if (bioseq_resolution.m_CassQueryCount > 0)
            app->GetTiming().Register(eResolutionFoundInCassandra,
                                      eOpStatusFound,
                                      bioseq_resolution.m_RequestStartTimestamp,
                                      bioseq_resolution.m_CassQueryCount);
        else
            app->GetTiming().Register(eResolutionFound,
                                      eOpStatusFound,
                                      bioseq_resolution.m_RequestStartTimestamp);
    }
}


void CPendingOperation::x_RegisterResolveTiming(
                            CRequestStatus::ECode  status,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    if (start_timestamp.time_since_epoch().count() != 0) {
        auto    app = CPubseqGatewayApp::GetInstance();
        if (status == CRequestStatus::e404_NotFound)
            app->GetTiming().Register(eResolutionNotFound, eOpStatusNotFound,
                                      start_timestamp);
        else
            app->GetTiming().Register(eResolutionError, eOpStatusNotFound,
                                      start_timestamp);
    }
}


SPSGS_RequestBase::EPSGS_AccSubstitutioOption
CPendingOperation::x_GetAccessionSubstitutionOption(void)
{
    // The substitution makes sense only for resolve/get/annot requests
    switch (m_UserRequest->GetRequestType()) {
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_UserRequest->GetRequest<SPSGS_BlobBySeqIdRequest>().m_AccSubstOption;
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_AccSubstOption;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
        default:
            break;
    }
    return SPSGS_RequestBase::ePSGS_NeverAccSubstitute;
}


EPSGS_AccessionAdjustmentResult
CPendingOperation::AdjustBioseqAccession(SBioseqResolution &  bioseq_resolution)
{
    if (CanSkipBioseqInfoRetrieval(bioseq_resolution.m_BioseqInfo))
        return ePSGS_NotRequired;

    auto    acc_subst_option = x_GetAccessionSubstitutionOption();
    if (acc_subst_option == SPSGS_RequestBase::ePSGS_NeverAccSubstitute)
        return ePSGS_NotRequired;

    if (acc_subst_option == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        bioseq_resolution.m_BioseqInfo.GetSeqIdType() != CSeq_id::e_Gi)
        return ePSGS_NotRequired;

    auto    adj_result = bioseq_resolution.AdjustAccession(m_UserRequest,
                                                           m_Reply);
    if (adj_result == ePSGS_LogicError ||
        adj_result == ePSGS_SeqIdsEmpty) {
        if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
            PSG_WARNING("BIOSEQ_INFO cache error: " +
                        bioseq_resolution.m_AdjustmentError);
        else
            PSG_WARNING("BIOSEQ_INFO Cassandra error: " +
                        bioseq_resolution.m_AdjustmentError);
    }
    return adj_result;
}


SPSGS_ResolveRequest::TPSGS_BioseqIncludeData
CPendingOperation::x_GetBioseqInfoFields(void)
{
    if (m_UserRequest->GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest)
        return m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_IncludeDataFlags;
    return SPSGS_ResolveRequest::fPSGS_AllBioseqFields;
}


bool CPendingOperation::x_NonKeyBioseqInfoFieldsRequested(void)
{
    return (x_GetBioseqInfoFields() &
            ~SPSGS_ResolveRequest::fPSGS_BioseqKeyFields) != 0;
}


// The method tells if the BIOSEQ_INFO record needs to be retrieved.
// It can be skipped under very specific conditions.
// It makes sense if the source of data is SI2CSI, i.e. only key fields are
// available.
bool
CPendingOperation::CanSkipBioseqInfoRetrieval(
                            const CBioseqInfoRecord &  bioseq_info_record)
{
    if (m_UserRequest->GetRequestType() != CPSGS_Request::ePSGS_ResolveRequest)
        return false;   // The get request supposes the full bioseq info

    if (x_NonKeyBioseqInfoFieldsRequested())
        return false;   // In the resolve request more bioseq_info fields are requested


    auto    seq_id_type = bioseq_info_record.GetSeqIdType();
    if (bioseq_info_record.GetVersion() > 0 && seq_id_type != CSeq_id::e_Gi)
        return true;    // This combination in data never requires accession adjustments

    auto    include_flags = m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_IncludeDataFlags;
    if ((include_flags & ~SPSGS_ResolveRequest::fPSGS_Gi) == 0)
        return true;    // Only GI field or no fields are requested so no accession
                        // adjustments are required

    auto    acc_subst = m_UserRequest->GetRequest<SPSGS_ResolveRequest>().m_AccSubstOption;
    if (acc_subst == SPSGS_RequestBase::ePSGS_NeverAccSubstitute)
        return true;    // No accession adjustments anyway so key fields are enough

    if (acc_subst == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        seq_id_type != CSeq_id::e_Gi)
        return true;    // No accession adjustments required

    return false;
}

