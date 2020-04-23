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
#include "named_annot_callback.hpp"
#include "get_blob_callback.hpp"
#include "split_history_callback.hpp"
#include "insdc_utils.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);


CPendingOperation::CPendingOperation(const CPSGS_Request &  user_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_UserRequest(user_request),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_ProtocolSupport(initial_reply_chunks),
    m_CreateTimestamp(chrono::high_resolution_clock::now())
{
    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: resolution "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest.GetResolveRequest().m_SeqId << "." <<
                      m_UserRequest.GetResolveRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqId << "." <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest.GetAnnotRequest().m_SeqId << "." <<
                      m_UserRequest.GetAnnotRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest.GetTSEChunkRequest().m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest.GetResolveRequest().m_SeqId << "." <<
                      m_UserRequest.GetResolveRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqId << "." <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest.GetAnnotRequest().m_SeqId << "." <<
                      m_UserRequest.GetAnnotRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest.GetTSEChunkRequest().m_TSEId.ToString() <<
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
    // finish it here as the last resort. (is it Cancel() case?)
    x_PrintRequestStop(m_OverallStatus);
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            PSG_TRACE("CPendingOperation::Clear(): resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest.GetResolveRequest().m_SeqId << "." <<
                      m_UserRequest.GetResolveRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by seq_id/seq_id_type: " <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqId << "." <<
                      m_UserRequest.GetBlobBySeqIdRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            PSG_TRACE("CPendingOperation::Clear(): blob "
                      "requested by sat/sat_key: " <<
                      m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId.ToString() <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            PSG_TRACE("CPendingOperation::Clear(): annotation "
                      "request by seq_id/seq_id_type: " <<
                      m_UserRequest.GetAnnotRequest().m_SeqId << "." <<
                      m_UserRequest.GetAnnotRequest().m_SeqIdType <<
                      ", this: " << this);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            PSG_TRACE("CPendingOperation::Clear(): TSE chunk "
                      "request by sat/sat_key: " <<
                      m_UserRequest.GetTSEChunkRequest().m_TSEId.ToString() <<
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

    m_ProtocolSupport.Clear();
    m_AsyncSeqIdResolver = nullptr;
    m_AsyncBioseqDetailsQuery = nullptr;
    m_Cancelled = false;
}


void CPendingOperation::Start(HST::CHttpReply<CPendingOperation>& resp)
{
    m_ProtocolSupport.SetReply(&resp);
    auto    request_type = m_UserRequest.GetRequestType();
    switch (request_type) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_UrlUseCache = m_UserRequest.GetResolveRequest().m_UseCache;
            x_ProcessResolveRequest();
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_UrlUseCache = m_UserRequest.GetBlobBySeqIdRequest().m_UseCache;
            x_ProcessGetRequest();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            m_UrlUseCache = m_UserRequest.GetBlobBySatSatKeyRequest().m_UseCache;
            x_ProcessGetRequest();
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_UrlUseCache = m_UserRequest.GetAnnotRequest().m_UseCache;
            x_ProcessAnnotRequest();
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            m_UrlUseCache = m_UserRequest.GetTSEChunkRequest().m_UseCache;
            x_ProcessTSEChunkRequest();
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
    size_t      item_id = m_ProtocolSupport.GetItemId();

    if (status != CRequestStatus::e404_NotFound)
        UpdateOverallStatus(status);

    m_ProtocolSupport.PrepareBioseqMessage(item_id, err_msg, status,
                                           ePSGS_NoBioseqInfo, eDiag_Error);
    m_ProtocolSupport.PrepareBioseqCompletion(item_id, 2);
    x_SendReplyCompletion(true);
    m_ProtocolSupport.Flush();

    x_RegisterResolveTiming(status, start_timestamp);
}


void CPendingOperation::x_OnReplyError(
                            CRequestStatus::ECode  status,
                            int  err_code, const string &  err_msg,
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    m_ProtocolSupport.PrepareReplyMessage(err_msg, status,
                                          err_code, eDiag_Error);
    x_SendReplyCompletion(true);
    m_ProtocolSupport.Flush();

    x_RegisterResolveTiming(status, start_timestamp);
}


void CPendingOperation::x_ProcessGetRequest(void)
{
    // Get request could be one of the following:
    // - the blob was requested by a sat/sat_key pair
    // - the blob was requested by a seq_id/seq_id_type pair
    if (m_UserRequest.GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        // By seq_id -> search for the bioseq key info in the cache only
        SResolveInputSeqIdError err;
        SBioseqResolution       bioseq_resolution(
                    m_UserRequest.GetBlobBySeqIdRequest().m_StartTimestamp);

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
            UpdateOverallStatus(err.m_ErrorCode);
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, ePSGS_UnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            x_OnBioseqError(CRequestStatus::e404_NotFound,
                            "Could not resolve seq_id " +
                            m_UserRequest.GetBlobBySeqIdRequest().m_SeqId,
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
        CPSGCache   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
        auto        cache_lookup_result =
                            psg_cache.LookupBioseqInfo(bioseq_resolution, this);
        if (cache_lookup_result != ePSGS_Found) {
            // No cache hit (or not allowed); need to get to DB if allowed
            if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_CacheOnly) {
                // Async DB query
                m_AsyncInterruptPoint = eGetBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                              this));
                // true => use seq_id_type
                m_AsyncBioseqDetailsQuery->MakeRequest(true);
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

    if (!x_SatToSatName(m_UserRequest.GetBlobBySeqIdRequest(), blob_id)) {
        // This is server data inconsistency error, so 500 (not 404)
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        return;
    }

    // retrieve BLOB_PROP & CHUNKS (if needed)
    m_UserRequest.GetBlobBySeqIdRequest().m_BlobId = blob_id;

    x_StartMainBlobRequest();
}


void CPendingOperation::x_GetRequestBioseqInconsistency(
                        const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    x_GetRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_UserRequest.GetBlobBySeqIdRequest().m_SeqId +
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


void CPendingOperation::x_OnResolveResolutionError(
                        SResolveInputSeqIdError &  err,
                        const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    x_OnResolveResolutionError(err.m_ErrorCode, err.m_ErrorMessage,
                               start_timestamp);
}


void CPendingOperation::x_OnResolveResolutionError(
                        CRequestStatus::ECode  status,
                        const string &  message,
                        const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    UpdateOverallStatus(status);
    PSG_WARNING(message);
    if (x_UsePsgProtocol()) {
        x_OnReplyError(status, ePSGS_UnresolvedSeqId, message, start_timestamp);
    } else {
        switch (status) {
            case CRequestStatus::e400_BadRequest:
                m_ProtocolSupport.Send400(message.c_str());
                break;
            case CRequestStatus::e404_NotFound:
                m_ProtocolSupport.Send404(message.c_str());
                break;
            case CRequestStatus::e500_InternalServerError:
                m_ProtocolSupport.Send500(message.c_str());
                break;
            case CRequestStatus::e502_BadGateway:
                m_ProtocolSupport.Send502(message.c_str());
                break;
            case CRequestStatus::e503_ServiceUnavailable:
                m_ProtocolSupport.Send503(message.c_str());
                break;
            default:
                m_ProtocolSupport.Send400(message.c_str());
        }
        x_RegisterResolveTiming(status, start_timestamp);
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::x_ProcessResolveRequest(void)
{
    SResolveInputSeqIdError     err;
    auto                        start_timestamp =
                                    m_UserRequest.GetResolveRequest().m_StartTimestamp;
    SBioseqResolution           bioseq_resolution(start_timestamp);

    x_ResolveInputSeqId(bioseq_resolution, err);
    if (err.HasError()) {
        x_OnResolveResolutionError(err, start_timestamp);
        return;
    }

    // Async resolution may be required
    if (bioseq_resolution.m_ResolutionResult == ePSGS_PostponedForDB) {
        m_AsyncInterruptPoint = eResolveSeqIdResolution;
        return;
    }

    x_ProcessResolveRequest(err, bioseq_resolution);
}


// Could be called by:
// - sync processing (resolved via cache)
// - async processing (resolved via DB)
void CPendingOperation::x_ProcessResolveRequest(
                                    SResolveInputSeqIdError &  err,
                                    SBioseqResolution &  bioseq_resolution)
{
    if (!bioseq_resolution.IsValid()) {
        if (err.HasError()) {
            x_OnResolveResolutionError(err,
                                       bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            string  err_msg = "Could not resolve seq_id " +
                              m_UserRequest.GetResolveRequest().m_SeqId;

            if (x_UsePsgProtocol()) {
                x_OnBioseqError(CRequestStatus::e404_NotFound, err_msg,
                                bioseq_resolution.m_RequestStartTimestamp);
            } else {
                m_ProtocolSupport.Send404(err_msg.c_str());
                x_RegisterResolveTiming(CRequestStatus::e404_NotFound,
                                        bioseq_resolution.m_RequestStartTimestamp);
                x_PrintRequestStop(m_OverallStatus);
            }
        }
        return;
    }

    // A few cases here: comes from cache or DB
    // ePSGS_Si2csiCache, ePSGS_Si2csiDB, ePSGS_BioseqCache, ePSGS_BioseqDB
    if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
        // We have the following fields at hand:
        // - accession, version, seq_id_type, gi
        // May be it is what the user asked for
        if (!CanSkipBioseqInfoRetrieval(bioseq_resolution.m_BioseqInfo)) {
            // Need to pull the full bioseq info
            CPSGCache   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
            auto        cache_lookup_result =
                                psg_cache.LookupBioseqInfo(bioseq_resolution,
                                                           this);
            if (cache_lookup_result != ePSGS_Found) {
                // No cache hit (or not allowed); need to get to DB if allowed
                if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_CacheOnly) {
                    // Async DB query
                    m_AsyncInterruptPoint = eResolveBioseqDetails;
                    m_AsyncBioseqDetailsQuery.reset(
                            new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                                  this));
                    // true => use seq_id_type
                    m_AsyncBioseqDetailsQuery->MakeRequest(true);
                    return;
                }

                // default error message
                x_ResolveRequestBioseqInconsistency(
                                        bioseq_resolution.m_RequestStartTimestamp);
                return;
            } else {
                bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;
            }
        }
    }

    x_CompleteResolveRequest(bioseq_resolution);
}


void CPendingOperation::x_ResolveRequestBioseqInconsistency(
                            const TPSGS_HighResolutionTimePoint &  start_timestamp)
{
    // This method is used in case of a resolve and get_na requests
    string      seq_id;
    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            seq_id = m_UserRequest.GetResolveRequest().m_SeqId;
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            seq_id = m_UserRequest.GetAnnotRequest().m_SeqId;
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
    if (x_UsePsgProtocol()) {
        x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg,
                        start_timestamp);
    } else {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_ProtocolSupport.Send500(err_msg.c_str());

        x_RegisterResolveTiming(CRequestStatus::e500_InternalServerError,
                                start_timestamp);
        x_PrintRequestStop(m_OverallStatus);
    }
}


// Could be called:
// - synchronously
// - asynchronously if additional request to bioseq info is required
void CPendingOperation::x_CompleteResolveRequest(
                                        SBioseqResolution &  bioseq_resolution)
{
    // Bioseq info is found, send it to the client
    x_SendBioseqInfo(bioseq_resolution,
                     m_UserRequest.GetResolveRequest().m_OutputFormat);
    if (x_UsePsgProtocol()) {
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        x_RegisterResolveTiming(bioseq_resolution);
    } else {
        x_RegisterResolveTiming(bioseq_resolution);
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::OnBioseqDetailsRecord(
                                    SBioseqResolution &&  async_bioseq_info)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eResolveBioseqDetails:
            if (async_bioseq_info.IsValid())
                x_CompleteResolveRequest(async_bioseq_info);
            else
                // default error message
                x_ResolveRequestBioseqInconsistency(
                                async_bioseq_info.m_RequestStartTimestamp);
            break;
        case eGetBioseqDetails:
            if (async_bioseq_info.IsValid())
                x_CompleteGetRequest(async_bioseq_info);
            else
                // default error message
                x_GetRequestBioseqInconsistency(
                                async_bioseq_info.m_RequestStartTimestamp);
            break;
        case eAnnotBioseqDetails:
            if (async_bioseq_info.IsValid())
                x_CompleteAnnotRequest(async_bioseq_info);
            else
                x_ResolveRequestBioseqInconsistency(
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
    x_SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eResolveBioseqDetails:
            x_ResolveRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " +
                    m_UserRequest.GetResolveRequest().m_SeqId + " but the "
                    "bioseq info is not found (database error: " +
                    message + ")", start_timestamp);
            break;
        case eGetBioseqDetails:
            x_GetRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " +
                    m_UserRequest.GetBlobBySeqIdRequest().m_SeqId + " but the "
                    "bioseq info is not found (database error: " + message +
                    ")", start_timestamp);
            break;
        case eAnnotBioseqDetails:
            x_ResolveRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " +
                    m_UserRequest.GetAnnotRequest().m_SeqId + " but the "
                    "bioseq info is not found (database error: " +
                    message + ")", start_timestamp);
            break;
        default:
            ;
    }
}


void CPendingOperation::x_ProcessAnnotRequest(void)
{
    SResolveInputSeqIdError     err;
    SBioseqResolution           bioseq_resolution(
                m_UserRequest.GetAnnotRequest().m_StartTimestamp);

    x_ResolveInputSeqId(bioseq_resolution, err);
    if (err.HasError()) {
        PSG_WARNING(err.m_ErrorMessage);
        x_OnReplyError(err.m_ErrorCode, ePSGS_UnresolvedSeqId, err.m_ErrorMessage,
                       bioseq_resolution.m_RequestStartTimestamp);
        return;
    }

    // Async resolution may be required
    if (bioseq_resolution.m_ResolutionResult == ePSGS_PostponedForDB) {
        m_AsyncInterruptPoint = eAnnotSeqIdResolution;
        return;
    }

    x_ProcessAnnotRequest(err, bioseq_resolution);
}


void CPendingOperation::x_ProcessTSEChunkRequest(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    string  err_msg;

    // First, check the blob prop cache, may be the requested version matches
    // the requested one
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    SPSGS_TSEChunkRequest &     request = m_UserRequest.GetTSEChunkRequest();
    auto                        blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(request.m_TSEId.m_Sat,
                                 request.m_TSEId.m_SatKey,
                                 last_modified, this, *blob_record.get());
    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        do {
            // Step 1: check the id2info presense
            if (blob_record->GetId2Info().empty()) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + request.m_TSEId.ToString() +
                            " properties id2info is empty in cache");
                break;  // Continue with cassandra
            }

            // Step 2: check that the id2info is parsable
            unique_ptr<CPSGId2Info>     id2_info;
            // false -> do not finish the request
            if (!x_ParseTSEChunkId2Info(blob_record->GetId2Info(),
                                        id2_info, request.m_TSEId,
                                        false)) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                break;  // Continue with cassandra
            }

            // Step 3: check the split version in cache
            if (id2_info->GetSplitVersion() != request.m_SplitVersion) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + request.m_TSEId.ToString() +
                            " split version in cache does not match the requested one");
                break;  // Continue with cassandra
            }

            app->GetRequestCounters().IncTSEChunkSplitVersionCacheMatched();

            // Step 4: validate the chunk number
            if (!x_ValidateTSEChunkNumber(request.m_Chunk,
                                          id2_info->GetChunks(), false)) {
                break; // Continue with cassandra
            }

            // Step 5: For the target chunk - convert sat to sat name
            // Chunk's blob id
            int64_t         sat_key = id2_info->GetInfo() -
                                      id2_info->GetChunks() - 1 + request.m_Chunk;
            SPSGS_BlobId    chunk_blob_id(id2_info->GetSat(), sat_key);
            if (!x_TSEChunkSatToSatName(chunk_blob_id, false)) {
                break;  // Continue with cassandra
            }

            // Step 6: search in cache the TSE chunk properties
            last_modified = INT64_MIN;
            auto  tse_blob_prop_cache_lookup_result = psg_cache.LookupBlobProp(
                            chunk_blob_id.m_Sat, chunk_blob_id.m_SatKey,
                            last_modified, this, *blob_record.get());
            if (tse_blob_prop_cache_lookup_result != ePSGS_Found) {
                err_msg = "TSE chunk blob " + chunk_blob_id.ToString() +
                          " properties are not found in cache";
                if (tse_blob_prop_cache_lookup_result == ePSGS_Failure)
                    err_msg += " due to LMDB error";
                PSG_WARNING(err_msg);
                break;  // Continue with cassandra
            }

            // Step 7: initiate the chunk request
            SPSGS_BlobBySatSatKeyRequest
                        chunk_request(chunk_blob_id, INT64_MIN,
                                      SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                                      SPSGS_RequestBase::ePSGS_UnknownUseCache,
                                      "", NeedTrace(),
                                      chrono::high_resolution_clock::now());

            unique_ptr<CCassBlobFetch>  fetch_details;
            fetch_details.reset(new CCassBlobFetch(chunk_request));
            CCassBlobTaskLoadBlob *         load_task =
                new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                          chunk_blob_id.m_SatName,
                                          std::move(blob_record),
                                          true, nullptr);
            fetch_details->SetLoader(load_task);
            load_task->SetDataReadyCB(GetDataReadyCB());
            load_task->SetErrorCB(CGetBlobErrorCallback(this, fetch_details.get()));
            load_task->SetPropsCallback(CBlobPropCallback(this, fetch_details.get(), false));
            load_task->SetChunkCallback(CBlobChunkCallback(this, fetch_details.get()));

            if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
                SendTrace("Cassandra request: " +
                          ToJson(*load_task).Repr(CJsonNode::fStandardJson));
            }

            m_FetchDetails.push_back(std::move(fetch_details));
            load_task->Wait();  // Initiate cassandra request
            return;
        } while (false);
    } else {
        err_msg = "Blob " + request.m_TSEId.ToString() +
                  " properties are not found in cache";
        if (blob_prop_cache_lookup_result == ePSGS_Failure)
            err_msg += " due to LMDB error";
        PSG_WARNING(err_msg);
    }

    // Here:
    // - fallback to cassandra
    // - cache is not allowed
    // - not found in cache

    // Initiate async the history request
    unique_ptr<CCassSplitHistoryFetch>      fetch_details;
    fetch_details.reset(new CCassSplitHistoryFetch(request));
    CCassBlobTaskFetchSplitHistory *   load_task =
        new  CCassBlobTaskFetchSplitHistory(m_Timeout, m_MaxRetries, m_Conn,
                                            request.m_TSEId.m_SatName,
                                            request.m_TSEId.m_SatKey,
                                            request.m_SplitVersion,
                                            nullptr, nullptr);
    fetch_details->SetLoader(load_task);
    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CSplitHistoryErrorCallback(this, fetch_details.get()));
    load_task->SetConsumeCallback(CSplitHistoryConsumeCallback(this, fetch_details.get()));

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Cassandra request: " +
            ToJson(*load_task).Repr(CJsonNode::fStandardJson));
    }

    m_FetchDetails.push_back(std::move(fetch_details));
    load_task->Wait();  // Initiate cassandra request
}


// Could be called by:
// - sync processing (resolved via cache)
// - async processing (resolved via DB)
void CPendingOperation::x_ProcessAnnotRequest(
                                SResolveInputSeqIdError &  err,
                                SBioseqResolution &  bioseq_resolution)
{
    if (!bioseq_resolution.IsValid()) {
        if (err.HasError()) {
            UpdateOverallStatus(err.m_ErrorCode);
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, ePSGS_UnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            x_OnReplyError(CRequestStatus::e404_NotFound, ePSGS_NoBioseqInfo,
                           "Could not resolve seq_id " +
                           m_UserRequest.GetAnnotRequest().m_SeqId,
                           bioseq_resolution.m_RequestStartTimestamp);
        }
        return;
    }

    // A few cases here: comes from cache or DB
    // ePSGS_Si2csiCache, ePSGS_Si2csiDB, ePSGS_BioseqCache, ePSGS_BioseqDB
    if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
        // We have the following fields at hand:
        // - accession, version, seq_id_type, gi
        // However the get_na requires to send back the whole bioseq_info
        CPSGCache   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
        auto        cache_lookup_result =
                                psg_cache.LookupBioseqInfo(bioseq_resolution,
                                                           this);
        if (cache_lookup_result != ePSGS_Found) {
            // No cache hit (or not allowed); need to get to DB if allowed
            if (m_UrlUseCache != SPSGS_RequestBase::ePSGS_CacheOnly) {
                // Async DB query
                m_AsyncInterruptPoint = eAnnotBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                              this));
                // true => use seq_id_type
                m_AsyncBioseqDetailsQuery->MakeRequest(true);
                return;
            }

            // Error message
            x_ResolveRequestBioseqInconsistency(
                    bioseq_resolution.m_RequestStartTimestamp);
            return;
        } else {
            bioseq_resolution.m_ResolutionResult = ePSGS_BioseqCache;
        }
    }

    x_CompleteAnnotRequest(bioseq_resolution);
}


// Could be called:
// - synchronously
// - asynchronously if additional request to bioseq info is required
void CPendingOperation::x_CompleteAnnotRequest(
                                        SBioseqResolution &  bioseq_resolution)
{
    // At the moment the annotations request supports only json
    x_SendBioseqInfo(bioseq_resolution, SPSGS_ResolveRequest::ePSGS_JsonFormat);
    x_RegisterResolveTiming(bioseq_resolution);

    vector<pair<string, int32_t>>  bioseq_na_keyspaces =
                CPubseqGatewayApp::GetInstance()->GetBioseqNAKeyspaces();

    for (const auto &  bioseq_na_keyspace : bioseq_na_keyspaces) {
        unique_ptr<CCassNamedAnnotFetch>   details;
        details.reset(new CCassNamedAnnotFetch(
                            m_UserRequest.GetAnnotRequest()));

        CCassNAnnotTaskFetch *  fetch_task =
                new CCassNAnnotTaskFetch(m_Timeout, m_MaxRetries, m_Conn,
                                         bioseq_na_keyspace.first,
                                         bioseq_resolution.m_BioseqInfo.GetAccession(),
                                         bioseq_resolution.m_BioseqInfo.GetVersion(),
                                         bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                         m_UserRequest.GetAnnotRequest().m_Names,
                                         nullptr, nullptr);
        details->SetLoader(fetch_task);

        fetch_task->SetConsumeCallback(
            CNamedAnnotationCallback(this, details.get(), bioseq_na_keyspace.second));
        fetch_task->SetErrorCB(CNamedAnnotationErrorCallback(this, details.get()));
        fetch_task->SetDataReadyCB(GetDataReadyCB());

        if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
            SendTrace("Cassandra request: " +
                ToJson(*fetch_task).Repr(CJsonNode::fStandardJson));
        }

        m_FetchDetails.push_back(std::move(details));
    }

    // Initiate the retrieval loop
    for (auto &  fetch_details: m_FetchDetails) {
        if (fetch_details)
            if (!fetch_details->ReadFinished())
                fetch_details->GetLoader()->Wait();
    }
}


void CPendingOperation::x_StartMainBlobRequest(void)
{
    // Here: m_BlobRequest has the resolved sat and a sat_key regardless
    //       how the blob was requested

    // In case of the blob request by sat.sat_key it could be in the exclude
    // blob list and thus should not be retrieved at all
    if (m_UserRequest.GetRequestType() ==
                            CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        if (m_UserRequest.GetBlobBySeqIdRequest().IsExcludedBlob()) {
            m_ProtocolSupport.PrepareBlobExcluded(
                m_ProtocolSupport.GetItemId(),
                m_UserRequest.GetBlobBySeqIdRequest().m_BlobId, ePSGS_Excluded);
            x_SendReplyCompletion(true);
            m_ProtocolSupport.Flush();
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
    if (m_UserRequest.GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        tse_option = m_UserRequest.GetBlobBySeqIdRequest().m_TSEOption;
        client_id = m_UserRequest.GetBlobBySeqIdRequest().m_ClientId;
        blob_id = m_UserRequest.GetBlobBySeqIdRequest().m_BlobId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheAdded;
        last_modified = INT64_MIN;
        start_timestamp = m_UserRequest.GetBlobBySeqIdRequest().m_StartTimestamp;
    } else {
        tse_option = m_UserRequest.GetBlobBySatSatKeyRequest().m_TSEOption;
        client_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_ClientId;
        blob_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheAdded;
        last_modified = m_UserRequest.GetBlobBySatSatKeyRequest().m_LastModified;
        start_timestamp = m_UserRequest.GetBlobBySatSatKeyRequest().m_StartTimestamp;
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
            if (m_UserRequest.GetRequestType() ==
                    CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
                if (cache_result == ePSGS_AlreadyInCache) {
                    if (completed)
                        m_ProtocolSupport.PrepareBlobExcluded(
                            blob_id, ePSGS_Sent);
                    else
                        m_ProtocolSupport.PrepareBlobExcluded(
                            blob_id, ePSGS_InProgress);
                    x_SendReplyCompletion(true);
                    m_ProtocolSupport.Flush();
                    return;
                }
            }
            if (cache_result == ePSGS_Added)
                * exclude_blob_cache_added = true;
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    if (m_UserRequest.GetRequestType() ==
                        CPSGS_Request::ePSGS_BlobBySeqIdRequest)
        fetch_details.reset(
                new CCassBlobFetch(m_UserRequest.GetBlobBySeqIdRequest()));
    else
        fetch_details.reset(
                new CCassBlobFetch(m_UserRequest.GetBlobBySatSatKeyRequest()));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        blob_id.m_Sat,
                                        blob_id.m_SatKey,
                                        last_modified,
                                        this, *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
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
            load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                                  blob_id.m_SatName,
                                                  blob_id.m_SatKey,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        } else {
            load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                                  blob_id.m_SatName,
                                                  blob_id.m_SatKey,
                                                  last_modified,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        }
    }

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, fetch_details.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, fetch_details.get(),
                                                  blob_prop_cache_lookup_result != ePSGS_Found));

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Cassandra request: " +
                  ToJson(*load_task).Repr(CJsonNode::fStandardJson));
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
    auto    request_type = m_UserRequest.GetRequestType();
    bool    is_blob_request = (request_type == CPSGS_Request::ePSGS_BlobBySeqIdRequest) ||
                              (request_type == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest);
    if (resp.IsOutputReady()) {
       if (is_blob_request) {
           m_ProtocolSupport.Flush(all_finished_read);
       } else {
            if (all_finished_read) {
                m_ProtocolSupport.Flush();
            }
       }
    }

    if (is_blob_request && all_finished_read) {
        string              client_id;
        SPSGS_BlobId        blob_id;
        bool *              exclude_blob_cache_added;
        bool *              exclude_blob_cache_completed;
        if (request_type == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
            client_id = m_UserRequest.GetBlobBySeqIdRequest().m_ClientId;
            blob_id = m_UserRequest.GetBlobBySeqIdRequest().m_BlobId;
            exclude_blob_cache_added = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheAdded;
            exclude_blob_cache_completed = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheCompleted;
        } else {
            client_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_ClientId;
            blob_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId;
            exclude_blob_cache_added = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheAdded;
            exclude_blob_cache_completed = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheCompleted;
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
    HST::CHttpReply<CPendingOperation> *  reply = m_ProtocolSupport.GetReply();

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
                m_ProtocolSupport.PrepareBlobPropMessage(
                    blob_fetch, error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
                m_ProtocolSupport.PrepareBlobPropCompletion(blob_fetch);
            } else {
                m_ProtocolSupport.PrepareBlobMessage(
                    blob_fetch, error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
                m_ProtocolSupport.PrepareBlobCompletion(blob_fetch);
            }
        } else if (fetch_type == ePSGS_AnnotationFetch ||
                   fetch_type == ePSGS_BioseqInfoFetch) {
            m_ProtocolSupport.PrepareReplyMessage(
                    error, CRequestStatus::e500_InternalServerError,
                    ePSGS_UnknownError, eDiag_Error);
        }

        // Mark finished
        fetch_details->SetReadFinished();

        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
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
        m_ProtocolSupport.PrepareReplyCompletion();

        // No need to set the context/engage context resetter: they're set outside
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::x_SetRequestContext(void)
{
    if (m_RequestContext.NotNull())
        CDiagContext::SetRequestContext(m_RequestContext);
}


void CPendingOperation::x_PrintRequestStop(int  status)
{
    if (m_RequestContext.NotNull()) {
        CDiagContext::SetRequestContext(m_RequestContext);
        m_RequestContext->SetReadOnly(false);
        m_RequestContext->SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        m_RequestContext.Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


EPSGS_SeqIdParsingResult
CPendingOperation::x_ParseInputSeqId(CSeq_id &  seq_id, string &  err_msg)
{
    bool    need_trace = NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing;

    try {
        seq_id.Set(m_UrlSeqId);
        if (need_trace)
            SendTrace("Parsing CSeq_id('" + m_UrlSeqId + "') succeeded");

        if (m_UrlSeqIdType <= 0) {
            if (need_trace)
                SendTrace("Parsing CSeq_id finished OK (#1)");
            return ePSGS_ParsedOK;
        }

        // Check the parsed type with the given
        int16_t     eff_seq_id_type;
        if (x_GetEffectiveSeqIdType(seq_id, eff_seq_id_type, false)) {
            if (need_trace)
                SendTrace("Parsing CSeq_id finished OK (#2)");
            return ePSGS_ParsedOK;
        }

        // seq_id_type from URL and from CSeq_id differ
        CSeq_id_Base::E_Choice  seq_id_type = seq_id.Which();

        if (need_trace)
            SendTrace("CSeq_id provided type " + to_string(seq_id_type) +
                      " and URL provided seq_id_type " +
                      to_string(m_UrlSeqIdType) + " mismatch");

        if (IsINSDCSeqIdType(m_UrlSeqIdType) &&
            IsINSDCSeqIdType(seq_id_type)) {
            // Both seq_id_types belong to INSDC
            if (need_trace) {
                SendTrace("Both types belong to INSDC types.\n"
                          "Parsing CSeq_id finished OK (#3)");
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
            SendTrace("Parsing CSeq_id('" + m_UrlSeqId + "') failed (exception)");
    }

    // Second variation of Set()
    if (m_UrlSeqIdType > 0) {
        try {
            seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                       (CSeq_id_Base::E_Choice)(m_UrlSeqIdType),
                       m_UrlSeqId);
            if (need_trace) {
                SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                          to_string(m_UrlSeqIdType) + ", '" + m_UrlSeqId +
                          "') succeeded.\n"
                          "Parsing CSeq_id finished OK (#4)");
            }
            return ePSGS_ParsedOK;
        } catch (...) {
            if (need_trace)
                SendTrace("Parsing CSeq_id(eFasta_AsTypeAndContent, " +
                          to_string(m_UrlSeqIdType) + ", '" + m_UrlSeqId +
                          "') failed (exception)");
        }
    }

    if (need_trace) {
        SendTrace("Parsing CSeq_id finished FAILED");
    }

    return ePSGS_ParseFailed;
}


void CPendingOperation::x_InitUrlIndentification(void)
{
    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_UrlSeqId = m_UserRequest.GetResolveRequest().m_SeqId;
            m_UrlSeqIdType = m_UserRequest.GetResolveRequest().m_SeqIdType;
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_UrlSeqId = m_UserRequest.GetBlobBySeqIdRequest().m_SeqId;
            m_UrlSeqIdType = m_UserRequest.GetBlobBySeqIdRequest().m_SeqIdType;
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_UrlSeqId = m_UserRequest.GetAnnotRequest().m_SeqId;
            m_UrlSeqIdType = m_UserRequest.GetAnnotRequest().m_SeqIdType;
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Not handled request type " +
                       to_string(static_cast<int>(m_UserRequest.GetRequestType())));
    }
}


bool CPendingOperation::x_ValidateTSEChunkNumber(int64_t  requested_chunk,
                                                 CPSGId2Info::TChunks  total_chunks,
                                                 bool  need_finish)
{
    if (requested_chunk > total_chunks) {
        string      msg = "Invalid chunk requested. The number of available chunks: " +
                          to_string(total_chunks) + ", requested number: " +
                          to_string(requested_chunk);
        if (need_finish) {
            CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncMalformedArguments();
            x_SendReplyError(msg, CRequestStatus::e400_BadRequest, ePSGS_MalformedParameter);
            x_SendReplyCompletion(true);
            m_ProtocolSupport.Flush();
        } else {
            PSG_WARNING(msg);
        }
        return false;
    }
    return true;
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
                    CPSGCache   psg_cache(true);
                    auto        bioseq_cache_lookup_result =
                                    psg_cache.LookupBioseqInfo(bioseq_resolution,
                                                               this);

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
                                            std::move(bioseq_resolution), this));
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
        CPSGCache           psg_cache(true);

        // Try BIOSEQ_INFO
        bioseq_resolution.m_BioseqInfo.SetAccession(primary_id);
        bioseq_resolution.m_BioseqInfo.SetVersion(effective_version);
        bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

        bioseq_cache_lookup_result = psg_cache.LookupBioseqInfo(
                                        bioseq_resolution, this);
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

    CPSGCache   psg_cache(true);
    auto        si2csi_cache_lookup_result =
                        psg_cache.LookupSi2csi(bioseq_resolution, this);
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
    bool    need_trace = NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing;
    if (!x_GetEffectiveSeqIdType(parsed_seq_id,
                                 effective_seq_id_type, need_trace)) {
        if (need_trace) {
            SendTrace("OSLT has not been tried due to mismatch between the "
                      " parsed CSeq_id seq_id_type and the URL provided one");
        }
        return false;
    }

    try {
        primary_id = parsed_seq_id.ComposeOSLT(&secondary_id_list,
                                               CSeq_id::fGpipeAddSecondary);
    } catch (...) {
        if (need_trace) {
            SendTrace("OSLT call failure (exception)");
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
        SendTrace(trace_msg);
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
            SendTrace("Seq id type mismatch. Parsed CSeq_id reports seq_id_type as " +
                      to_string(parsed_seq_id_type) + " while the URL reports " +
                      to_string(m_UrlSeqIdType) +". They both belong to INSDC types so "
                      "CSeq_id provided type " + to_string(parsed_seq_id_type) +
                      " is taken as an effective one");
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

    if (x_UsePsgProtocol()) {
        // Send it as the PSG protocol
        size_t              item_id = m_ProtocolSupport.GetItemId();
        m_ProtocolSupport.PrepareBioseqData(item_id, data_to_send,
                                            effective_output_format);
        m_ProtocolSupport.PrepareBioseqCompletion(item_id, 2);
    } else {
        // Send it as the HTTP data
        if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat)
            m_ProtocolSupport.SendData(data_to_send, ePSGS_JsonMime);
        else
            m_ProtocolSupport.SendData(data_to_send, ePSGS_BinaryMime);
    }
}


bool CPendingOperation::x_UsePsgProtocol(void)
{
    if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest)
        return m_UserRequest.GetResolveRequest().m_UsePsgProtocol;
    return true;
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
            m_ProtocolSupport.GetItemId(),
            "Unknown satellite number " + to_string(blob_id.m_Sat) +
            " for bioseq info with seq_id '" + blob_request.m_SeqId + "'",
            ePSGS_UnknownResolvedSatellite);
    return false;
}


bool CPendingOperation::x_TSEChunkSatToSatName(SPSGS_BlobId &  blob_id,
                                               bool  need_finish)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToSatName(blob_id.m_Sat, blob_id.m_SatName))
        return true;

    app->GetErrorCounters().IncServerSatToSatName();

    string  msg = "Unknown TSE chunk satellite number " +
                  to_string(blob_id.m_Sat) +
                  " for the blob " + blob_id.ToString();
    if (need_finish) {
        x_SendReplyError(msg, CRequestStatus::e500_InternalServerError,
                         ePSGS_UnknownResolvedSatellite);

        // This method is used only in case of the TSE chunk requests.
        // So in case of errors - synchronous or asynchronous - it is necessary to
        // finish the reply anyway.
        // In case of the sync part there are no initiated requests at all. So the
        // call of completion is done with the force flag.
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
    } else {
        PSG_WARNING(msg);
    }
    return false;
}


void CPendingOperation::x_SendBlobPropError(size_t  item_id,
                                            const string &  msg,
                                            int  err_code)
{
    m_ProtocolSupport.PrepareBlobPropMessage(
        item_id, msg, CRequestStatus::e500_InternalServerError, err_code, eDiag_Error);
    m_ProtocolSupport.PrepareBlobPropCompletion(item_id, 2);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);
}


void CPendingOperation::x_SendReplyError(const string &  msg,
                                         CRequestStatus::ECode  status,
                                         int  code)
{
    m_ProtocolSupport.PrepareReplyMessage(msg, status, code, eDiag_Error);
    UpdateOverallStatus(status);

    if (status >= CRequestStatus::e400_BadRequest &&
        status < CRequestStatus::e500_InternalServerError) {
        PSG_WARNING(msg);
    } else {
        PSG_ERROR(msg);
    }
}


//
// Named annotations callbacks
//

bool CPendingOperation::OnNamedAnnotData(CNAnnotRecord &&        annot_record,
                                         bool                    last,
                                         CCassNamedAnnotFetch *  fetch_details,
                                         int32_t                 sat)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->SetReadFinished();
        return false;
    }
    if (m_ProtocolSupport.IsReplyFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        return false;
    }

    if (last) {
        if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
            SendTrace("Named annotation no-more-data callback");
        }
        fetch_details->SetReadFinished();
        x_SendReplyCompletion();
    } else {
        if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
            SendTrace("Named annotation data received");
        }
        m_ProtocolSupport.PrepareNamedAnnotationData(
                annot_record.GetAnnotName(),
                ToJson(annot_record, sat).Repr(CJsonNode::fStandardJson));
    }

    x_PeekIfNeeded();
    return true;
}


void CPendingOperation::OnNamedAnnotError(
                                CCassNamedAnnotFetch *  fetch_details,
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    PSG_ERROR(message);

    if (is_error) {
        if (code == CCassandraException::eQueryTimeout)
            app->GetErrorCounters().IncCassQueryTimeoutError();
        else
            app->GetErrorCounters().IncUnknownError();
    }

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Named annotation error callback");
    }

    m_ProtocolSupport.PrepareReplyMessage(message,
                                          CRequestStatus::e500_InternalServerError,
                                          code, severity);
    if (is_error) {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        // There will be no more activity
        fetch_details->SetReadFinished();
    }

    x_SendReplyCompletion();
    x_PeekIfNeeded();
}


//
// Blob callbacks
//

void CPendingOperation::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                      CBlobRecord const &  blob, bool is_found)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Blob prop callback; found: " + to_string(is_found));
    }

    if (is_found) {
        // Found, send blob props back as JSON
        m_ProtocolSupport.PrepareBlobPropData(
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
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
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
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e404_NotFound,
                                ePSGS_BlobPropsNotFound, eDiag_Error);
    } else {
        // Server error, data inconsistency
        PSG_ERROR(message);
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_BlobPropsNotFound, eDiag_Error);
    }

    SPSGS_BlobId    request_blob_id;
    string          client_id;
    bool *          exclude_blob_cache_added;
    if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        request_blob_id = m_UserRequest.GetBlobBySeqIdRequest().m_BlobId;
        client_id = m_UserRequest.GetBlobBySeqIdRequest().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheAdded;
    } else {
        request_blob_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId;
        client_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheAdded;
    }

    if (blob_id == request_blob_id) {
        if ((* exclude_blob_cache_added) &&
            !client_id.empty()) {
            app->GetExcludeBlobCache()->Remove(client_id,
                                               blob_id.m_Sat, blob_id.m_SatKey);
            * exclude_blob_cache_added = false;
        }
    }

    m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);

    fetch_details->SetReadFinished();
    x_SendReplyCompletion();
}


void CPendingOperation::x_OnBlobPropNoneTSE(CCassBlobFetch *  fetch_details)
{
    fetch_details->SetReadFinished();
    // Nothing else to be sent;
    m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
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
    if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        request_blob_id = m_UserRequest.GetBlobBySeqIdRequest().m_BlobId;
        client_id = m_UserRequest.GetBlobBySeqIdRequest().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheAdded;
    } else {
        request_blob_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId;
        client_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_ClientId;
        exclude_blob_cache_added = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheAdded;
    }

    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);

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
                if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest &&
                        cache_result == ePSGS_AlreadyInCache) {
                    if (completed)
                        m_ProtocolSupport.PrepareBlobExcluded(blob_id, ePSGS_Sent);
                    else
                        m_ProtocolSupport.PrepareBlobExcluded(blob_id, ePSGS_InProgress);
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
            if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest &&
                    cache_result == ePSGS_AlreadyInCache) {
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                if (completed)
                    m_ProtocolSupport.PrepareBlobExcluded(blob_id, ePSGS_Sent);
                else
                    m_ProtocolSupport.PrepareBlobExcluded(blob_id, ePSGS_InProgress);
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
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
    }
}


void CPendingOperation::x_OnBlobPropSmartTSE(CCassBlobFetch *  fetch_details,
                                             CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        // Request the split INFO blob only
        x_RequestID2BlobChunks(fetch_details, blob, true);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
    }
}


void CPendingOperation::x_OnBlobPropWholeTSE(CCassBlobFetch *  fetch_details,
                                             CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        // Request the split INFO blob and all split chunks
        x_RequestID2BlobChunks(fetch_details, blob, false);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
    }
}

void CPendingOperation::x_OnBlobPropOrigTSE(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    // Request original blob chunks
    m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
    x_RequestOriginalBlobChunks(fetch_details, blob);
}


void CPendingOperation::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  /*blob*/,
                                       const unsigned char *  chunk_data,
                                       unsigned int  data_size, int  chunk_no)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->SetReadFinished();
        return;
    }
    if (m_ProtocolSupport.IsReplyFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        return;
    }

    if (chunk_no >= 0) {
        if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
            SendTrace("Blob chunk " + to_string(chunk_no) + " callback");
        }

        // A blob chunk; 0-length chunks are allowed too
        m_ProtocolSupport.PrepareBlobData(fetch_details,
                                          chunk_data, data_size, chunk_no);
    } else {
        if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
            SendTrace("Blob chunk no-more-data callback");
        }

        // End of the blob
        m_ProtocolSupport.PrepareBlobCompletion(fetch_details);
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
    x_SetRequestContext();

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

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Blob error callback; status " + to_string(status));
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
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        if (fetch_details->IsBlobPropStage()) {
            m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
        } else {
            m_ProtocolSupport.PrepareBlobMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_ProtocolSupport.PrepareBlobCompletion(fetch_details);
        }

        SPSGS_BlobId    request_blob_id;
        string          client_id;
        bool *          exclude_blob_cache_added;
        if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
            request_blob_id = m_UserRequest.GetBlobBySeqIdRequest().m_BlobId;
            client_id = m_UserRequest.GetBlobBySeqIdRequest().m_ClientId;
            exclude_blob_cache_added = & m_UserRequest.GetBlobBySeqIdRequest().m_ExcludeBlobCacheAdded;
        } else {
            request_blob_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_BlobId;
            client_id = m_UserRequest.GetBlobBySatSatKeyRequest().m_ClientId;
            exclude_blob_cache_added = & m_UserRequest.GetBlobBySatSatKeyRequest().m_ExcludeBlobCacheAdded;
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
            m_ProtocolSupport.PrepareBlobPropMessage(fetch_details, message,
                                                     status, code, severity);
        else
            m_ProtocolSupport.PrepareBlobMessage(fetch_details, message,
                                                 status, code, severity);
    }

    x_SendReplyCompletion();
    x_PeekIfNeeded();
}


void CPendingOperation::OnGetSplitHistory(CCassSplitHistoryFetch *  fetch_details,
                                          vector<SSplitHistoryRecord> && result)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    fetch_details->SetReadFinished();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        return;
    }

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Split history callback; found: " +
                  to_string(result.empty()));
    }

    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
    if (result.empty()) {
        // Split history is not found
        app->GetErrorCounters().IncSplitHistoryNotFoundError();

        string      message = "Split history version " +
                              to_string(fetch_details->GetSplitVersion()) +
                              " is not found for the TSE id " +
                              fetch_details->GetTSEId().ToString();
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_ProtocolSupport.PrepareReplyMessage(message,
                                              CRequestStatus::e404_NotFound,
                                              ePSGS_SplitHistoryNotFound,
                                              eDiag_Error);
        x_SendReplyCompletion();
    } else {
        // Split history found.
        // Note: the request was issued so that there could be exactly one
        // split history record or none at all. So it is not checked that
        // there are more than one record.
        x_RequestTSEChunk(result[0], fetch_details);
    }

    x_PeekIfNeeded();
}


void CPendingOperation::x_RequestTSEChunk(const SSplitHistoryRecord &  split_record,
                                          CCassSplitHistoryFetch *  fetch_details)
{
    // Parse id2info
    unique_ptr<CPSGId2Info>     id2_info;
    if (!x_ParseTSEChunkId2Info(split_record.id2_info,
                                id2_info, fetch_details->GetTSEId(), true))
        return;

    // Check the requested chunk
    // true -> finish the request if failed
    if (!x_ValidateTSEChunkNumber(fetch_details->GetChunk(),
                                  id2_info->GetChunks(), true))
        return;

    // Resolve sat to satkey
    int64_t         sat_key = id2_info->GetInfo() - id2_info->GetChunks() - 1 +
                              fetch_details->GetChunk();
    SPSGS_BlobId    chunk_blob_id(id2_info->GetSat(), sat_key);
    if (!x_TSEChunkSatToSatName(chunk_blob_id, true))
        return;

    // Look for the blob props
    // Form the chunk request with/without blob props
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(fetch_details->GetUseCache() != SPSGS_RequestBase::ePSGS_DbOnly);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    auto                        blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(chunk_blob_id.m_Sat,
                                 chunk_blob_id.m_SatKey,
                                 last_modified, this, *blob_record.get());
    if (blob_prop_cache_lookup_result != ePSGS_Found &&
        fetch_details->GetUseCache() == SPSGS_RequestBase::ePSGS_CacheOnly) {
        // Cassandra is forbidden for the blob prop
        string  err_msg = "TSE chunk blob " + chunk_blob_id.ToString() +
                          " properties are not found in cache";
        if (blob_prop_cache_lookup_result == ePSGS_Failure)
            err_msg += " due to LMDB error";
        x_SendReplyError(err_msg, CRequestStatus::e404_NotFound,
                         ePSGS_BlobPropsNotFound);
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        return;
    }

    SPSGS_BlobBySatSatKeyRequest
        chunk_request(chunk_blob_id, INT64_MIN,
                      SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                      SPSGS_RequestBase::ePSGS_UnknownUseCache,
                      "", NeedTrace(), chrono::high_resolution_clock::now());
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(chunk_request));

    CCassBlobTaskLoadBlob *     load_task = nullptr;

    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        load_task = new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            chunk_request.m_BlobId.m_SatName,
                            std::move(blob_record),
                            true, nullptr);
    } else {
        load_task = new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            chunk_request.m_BlobId.m_SatName,
                            chunk_request.m_BlobId.m_SatKey,
                            true, nullptr);
    }
    cass_blob_fetch->SetLoader(load_task);

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, cass_blob_fetch.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, cass_blob_fetch.get(),
                                                  blob_prop_cache_lookup_result != ePSGS_Found));
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Cassandra request: " +
                  ToJson(*load_task).Repr(CJsonNode::fStandardJson));
    }

    m_FetchDetails.push_back(std::move(cass_blob_fetch));
    load_task->Wait();
}


void CPendingOperation::OnGetSplitHistoryError(CCassSplitHistoryFetch *  fetch_details,
                                               CRequestStatus::ECode  status, int  code,
                                               EDiagSev  severity, const string &  message)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

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

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Split history error callback; status: " + to_string(status));
    }

    m_ProtocolSupport.PrepareReplyMessage(message, status, code, severity);

    if (is_error) {
        if (code == CCassandraException::eQueryTimeout)
            app->GetErrorCounters().IncCassQueryTimeoutError();
        else
            app->GetErrorCounters().IncUnknownError();

        // If it is an error then there will be no more activity
        fetch_details->SetReadFinished();
    }

    x_SendReplyCompletion();
    x_PeekIfNeeded();
}


void CPendingOperation::x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                                    CBlobRecord const &  blob)
{
    // eUnknownTSE is safe here; no blob prop call will happen anyway
    // eUnknownUseCache is safe here; no further resolution required
    SPSGS_BlobBySatSatKeyRequest
            orig_blob_request(fetch_details->GetBlobId(),
                              blob.GetModified(),
                              SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                              SPSGS_RequestBase::ePSGS_UnknownUseCache,
                              fetch_details->GetClientId(),
                              NeedTrace(),
                              chrono::high_resolution_clock::now());

    // Create the cass async loader
    unique_ptr<CBlobRecord>             blob_record(new CBlobRecord(blob));
    CCassBlobTaskLoadBlob *             load_task =
        new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                  orig_blob_request.m_BlobId.m_SatName,
                                  std::move(blob_record),
                                  true, nullptr);

    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(orig_blob_request));
    cass_blob_fetch->SetLoader(load_task);

    // Blob props have already been rceived
    cass_blob_fetch->SetBlobPropSent();

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, cass_blob_fetch.get()));
    load_task->SetPropsCallback(nullptr);
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Cassandra request: " +
                  ToJson(*load_task).Repr(CJsonNode::fStandardJson));
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
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_BadID2Info, eDiag_Error);
        app->GetErrorCounters().IncServerSatToSatName();
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
        return;
    }

    // Create the Id2Info requests.
    // eUnknownTSE is treated in the blob prop handler as to do nothing (no
    // sending completion message, no requesting other blobs)
    // eUnknownUseCache is safe here; no further resolution
    // empty client_id means that later on there will be no exclude blobs cache ops
    SPSGS_BlobBySatSatKeyRequest
        info_blob_request(info_blob_id, INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          "", NeedTrace(),
                          chrono::high_resolution_clock::now());

    // Prepare Id2Info retrieval
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(info_blob_request));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
    auto                        blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        info_blob_request.m_BlobId.m_Sat,
                                        info_blob_request.m_BlobId.m_SatKey,
                                        info_blob_request.m_LastModified,
                                        this, *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;


    if (blob_prop_cache_lookup_result == ePSGS_Found) {
        load_task = new CCassBlobTaskLoadBlob(
                        m_Timeout, m_MaxRetries, m_Conn,
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
                UpdateOverallStatus(CRequestStatus::e404_NotFound);
                m_ProtocolSupport.PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e404_NotFound,
                                    ePSGS_BlobPropsNotFound, eDiag_Error);
            } else {
                message = "Blob properties are not found due to LMDB cache error";
                UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                m_ProtocolSupport.PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e500_InternalServerError,
                                    ePSGS_BlobPropsNotFound, eDiag_Error);
            }

            PSG_WARNING(message);
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(
                        m_Timeout, m_MaxRetries, m_Conn,
                        info_blob_request.m_BlobId.m_SatName,
                        info_blob_request.m_BlobId.m_SatKey,
                        true, nullptr);
    }
    cass_blob_fetch->SetLoader(load_task);

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, cass_blob_fetch.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, cass_blob_fetch.get(),
                                                  blob_prop_cache_lookup_result != ePSGS_Found));
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

    if (NeedTrace() == SPSGS_RequestBase::ePSGS_WithTracing) {
        SendTrace("Cassandra request: " +
                  ToJson(*load_task).Repr(CJsonNode::fStandardJson));
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
                          "", NeedTrace(),
                          chrono::high_resolution_clock::now());

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_UrlUseCache != SPSGS_RequestBase::ePSGS_DbOnly);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            chunk_request.m_BlobId.m_Sat,
                                            chunk_request.m_BlobId.m_SatKey,
                                            chunk_request.m_LastModified,
                                            this, *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == ePSGS_Found) {
            load_task = new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
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
                    UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    m_ProtocolSupport.PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e404_NotFound,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    m_ProtocolSupport.PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e500_InternalServerError,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                }
                m_ProtocolSupport.PrepareBlobPropCompletion(details.get());
                PSG_WARNING(message);
                continue;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            chunk_request.m_BlobId.m_SatName,
                            chunk_request.m_BlobId.m_SatKey,
                            true, nullptr);
            details->SetLoader(load_task);
        }

        load_task->SetDataReadyCB(GetDataReadyCB());
        load_task->SetErrorCB(CGetBlobErrorCallback(this, details.get()));
        load_task->SetPropsCallback(CBlobPropCallback(this, details.get(),
                                                      blob_prop_cache_lookup_result != ePSGS_Found));
        load_task->SetChunkCallback(CBlobChunkCallback(this, details.get()));

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
    m_ProtocolSupport.PrepareBlobPropMessage(
        fetch_details, err_msg, CRequestStatus::e500_InternalServerError,
        ePSGS_BadID2Info, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
    return false;
}


bool CPendingOperation::x_ParseTSEChunkId2Info(const string &  info,
                                               unique_ptr<CPSGId2Info> &  id2_info,
                                               const SPSGS_BlobId &  blob_id,
                                               bool  need_finish)
{
    string      err_msg;
    try {
        id2_info.reset(new CPSGId2Info(info));
        return true;
    } catch (const exception &  exc) {
        err_msg = "Error extracting id2 info for blob " +
            blob_id.ToString() + ": " + exc.what();
    } catch (...) {
        err_msg = "Unknown error while extracting id2 info for blob " +
            blob_id.ToString();
    }

    CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncInvalidId2InfoError();
    if (need_finish) {
        x_SendReplyError(err_msg, CRequestStatus::e500_InternalServerError,
                         ePSGS_InvalidId2Info);
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
    } else {
        PSG_WARNING(err_msg);
    }
    return false;
}


void CPendingOperation::OnSeqIdAsyncResolutionFinished(
                                SBioseqResolution &&  async_bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

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
        case eAnnotSeqIdResolution:
            x_ProcessAnnotRequest(async_bioseq_resolution.m_PostponedError,
                                  async_bioseq_resolution);
            break;
        case eResolveSeqIdResolution:
            x_ProcessResolveRequest(async_bioseq_resolution.m_PostponedError,
                                    async_bioseq_resolution);
            break;
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
    x_SetRequestContext();

    switch (m_AsyncInterruptPoint) {
        case eAnnotSeqIdResolution:
            x_OnReplyError(status, code, message, start_timestamp);
            return;
        case eResolveSeqIdResolution:
            x_OnResolveResolutionError(status, message, start_timestamp);
            return;
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
            app->GetTiming().Register(eResolutionFoundInCache,
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
    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_UserRequest.GetBlobBySeqIdRequest().m_AccSubstOption;
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_UserRequest.GetResolveRequest().m_AccSubstOption;
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

    auto    adj_result = bioseq_resolution.AdjustAccession(this);
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
    if (m_UserRequest.GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest)
        return m_UserRequest.GetResolveRequest().m_IncludeDataFlags;
    return SPSGS_ResolveRequest::fPSGS_AllBioseqFields;
}


SPSGS_RequestBase::EPSGS_Trace CPendingOperation::NeedTrace(void)
{
    // Tracing is compatible only with PSG protocol.
    // The PSG protocol may be not used in case of resolve request.
    switch (m_UserRequest.GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            if (m_UserRequest.GetResolveRequest().m_Trace == SPSGS_RequestBase::ePSGS_WithTracing &&
                m_UserRequest.GetResolveRequest().m_UsePsgProtocol)
                return SPSGS_RequestBase::ePSGS_WithTracing;
            return SPSGS_RequestBase::ePSGS_NoTracing;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_UserRequest.GetBlobBySeqIdRequest().m_Trace;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            return m_UserRequest.GetBlobBySatSatKeyRequest().m_Trace;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_UserRequest.GetAnnotRequest().m_Trace;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            return m_UserRequest.GetTSEChunkRequest().m_Trace;
        default:
            break;
    }
    return SPSGS_RequestBase::ePSGS_NoTracing;
}


void CPendingOperation::SendTrace(const string &  msg)
{
    auto            now = chrono::high_resolution_clock::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>(now - m_CreateTimestamp).count();
    string          timestamp = "Timestamp (mks): " + to_string(mks) + "\n";
    m_ProtocolSupport.PrepareReplyMessage(timestamp + msg,
                                          CRequestStatus::e200_Ok,
                                          0, eDiag_Trace);
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
    if (m_UserRequest.GetRequestType() != CPSGS_Request::ePSGS_ResolveRequest)
        return false;   // The get request supposes the full bioseq info

    if (x_NonKeyBioseqInfoFieldsRequested())
        return false;   // In the resolve request more bioseq_info fields are requested


    auto    seq_id_type = bioseq_info_record.GetSeqIdType();
    if (bioseq_info_record.GetVersion() > 0 && seq_id_type != CSeq_id::e_Gi)
        return true;    // This combination in data never requires accession adjustments

    auto    include_flags = m_UserRequest.GetResolveRequest().m_IncludeDataFlags;
    if ((include_flags & ~SPSGS_ResolveRequest::fPSGS_Gi) == 0)
        return true;    // Only GI field or no fields are requested so no accession
                        // adjustments are required

    auto    acc_subst = m_UserRequest.GetResolveRequest().m_AccSubstOption;
    if (acc_subst == SPSGS_RequestBase::ePSGS_NeverAccSubstitute)
        return true;    // No accession adjustments anyway so key fields are enough

    if (acc_subst == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        seq_id_type != CSeq_id::e_Gi)
        return true;    // No accession adjustments required

    return false;
}

