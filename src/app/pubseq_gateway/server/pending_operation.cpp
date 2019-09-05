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

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);


CPendingOperation::CPendingOperation(const SBlobRequest &  blob_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_RequestType(eBlobRequest),
    m_BlobRequest(blob_request),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_ProtocolSupport(initial_reply_chunks)
{
    if (m_BlobRequest.m_BlobIdType == eBySeqId) {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by seq_id/seq_id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_SeqIdType <<
                 ", this: " << this);
    } else {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.ToString() <<
                 ", this: " << this);
    }
}


CPendingOperation::CPendingOperation(const SResolveRequest &  resolve_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_RequestType(eResolveRequest),
    m_ResolveRequest(resolve_request),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_ProtocolSupport(initial_reply_chunks)
{
    PSG_TRACE("CPendingOperation::CPendingOperation: resolution "
              "request by seq_id/seq_id_type: " <<
              m_ResolveRequest.m_SeqId << "." << m_ResolveRequest.m_SeqIdType <<
              ", this: " << this);
}


CPendingOperation::CPendingOperation(const SAnnotRequest &  annot_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_RequestType(eAnnotationRequest),
    m_AnnotRequest(annot_request),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_ProtocolSupport(initial_reply_chunks)
{
    PSG_TRACE("CPendingOperation::CPendingOperation: annotation "
              "request by seq_id/seq_id_type: " <<
              m_AnnotRequest.m_SeqId << "." << m_AnnotRequest.m_SeqIdType <<
              ", this: " << this);
}


CPendingOperation::CPendingOperation(const STSEChunkRequest &  tse_chunk_request,
                                     size_t  initial_reply_chunks,
                                     shared_ptr<CCassConnection>  conn,
                                     unsigned int  timeout,
                                     unsigned int  max_retries,
                                     CRef<CRequestContext>  request_context) :
    m_Conn(conn),
    m_Timeout(timeout),
    m_MaxRetries(max_retries),
    m_OverallStatus(CRequestStatus::e200_Ok),
    m_RequestType(eTSEChunkRequest),
    m_TSEChunkRequest(tse_chunk_request),
    m_Cancelled(false),
    m_RequestContext(request_context),
    m_ProtocolSupport(initial_reply_chunks)
{
    PSG_TRACE("CPendingOperation::CPendingOperation: TSE chunk "
              "request by sat/sat_key: " <<
              m_TSEChunkRequest.m_TSEId.ToString() <<
              ", this: " << this);
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (m_RequestType) {
        case eBlobRequest:
            if (m_BlobRequest.m_BlobIdType == eBySeqId) {
                PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                          "requested by seq_id/seq_id_type: " <<
                          m_BlobRequest.m_SeqId << "." <<
                          m_BlobRequest.m_SeqIdType <<
                          ", this: " << this);
            } else {
                PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                          "requested by sat/sat_key: " <<
                          m_BlobRequest.m_BlobId.ToString() <<
                          ", this: " << this);
            }
            break;
        case eResolveRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_ResolveRequest.m_SeqId << "." <<
                      m_ResolveRequest.m_SeqIdType <<
                      ", this: " << this);

            break;
        case eAnnotationRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: annotation "
                      "requested by seq_id/seq_id_type: " <<
                      m_AnnotRequest.m_SeqId << "." <<
                      m_AnnotRequest.m_SeqIdType <<
                      ", this: " << this);
            break;
        case eTSEChunkRequest:
            PSG_TRACE("CPendingOperation::~CPendingOperation: TSE chunk "
                      " requested by sat/sat_key: " <<
                      m_TSEChunkRequest.m_TSEId.ToString() <<
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

    switch (m_RequestType) {
        case eBlobRequest:
            if (m_BlobRequest.m_BlobIdType == eBySeqId) {
                PSG_TRACE("CPendingOperation::Clear: blob "
                          "requested by seq_id/seq_id_type: " <<
                          m_BlobRequest.m_SeqId << "." <<
                          m_BlobRequest.m_SeqIdType <<
                          ", this: " << this);
            } else {
                PSG_TRACE("CPendingOperation::Clear: blob "
                          "requested by sat/sat_key: " <<
                          m_BlobRequest.m_BlobId.ToString() <<
                          ", this: " << this);
            }
            break;
        case eResolveRequest:
            PSG_TRACE("CPendingOperation::Clear: resolve "
                      "requested by seq_id/seq_id_type: " <<
                      m_ResolveRequest.m_SeqId << "." <<
                      m_ResolveRequest.m_SeqIdType <<
                      ", this: " << this);

            break;
        case eAnnotationRequest:
            PSG_TRACE("CPendingOperation::Clear: annotation "
                      "requested by seq_id/seq_id_type: " <<
                      m_AnnotRequest.m_SeqId << "." <<
                      m_AnnotRequest.m_SeqIdType <<
                      ", this: " << this);
            break;
        case eTSEChunkRequest:
            PSG_TRACE("CPendingOperation::Clear: TSE chunk "
                      " requested by sat/sat_key: " <<
                      m_TSEChunkRequest.m_TSEId.ToString() <<
                      ", this: " << this);
            break;
        default:
            ;
    }

    for (auto &  details: m_FetchDetails) {
        if (details) {
            PSG_TRACE("CPendingOperation::Clear: "
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
    switch (m_RequestType) {
        case eResolveRequest:
            m_UrlUseCache = m_ResolveRequest.m_UseCache;
            x_ProcessResolveRequest();
            break;
        case eBlobRequest:
            m_UrlUseCache = m_BlobRequest.m_UseCache;
            x_ProcessGetRequest();
            break;
        case eAnnotationRequest:
            m_UrlUseCache = m_AnnotRequest.m_UseCache;
            x_ProcessAnnotRequest();
            break;
        case eTSEChunkRequest:
            m_UrlUseCache = m_TSEChunkRequest.m_UseCache;
            x_ProcessTSEChunkRequest();
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Unhandeled request type " +
                       NStr::NumericToString(static_cast<int>(m_RequestType)));
    }
}


void CPendingOperation::x_OnBioseqError(
                            CRequestStatus::ECode  status,
                            const string &  err_msg,
                            const THighResolutionTimePoint &  start_timestamp)
{
    size_t      item_id = m_ProtocolSupport.GetItemId();

    if (status != CRequestStatus::e404_NotFound)
        UpdateOverallStatus(status);

    m_ProtocolSupport.PrepareBioseqMessage(item_id, err_msg, status,
                                          eNoBioseqInfo, eDiag_Error);
    m_ProtocolSupport.PrepareBioseqCompletion(item_id, 2);
    x_SendReplyCompletion(true);
    m_ProtocolSupport.Flush();

    x_RegisterResolveTiming(status, start_timestamp);
}


void CPendingOperation::x_OnReplyError(
                            CRequestStatus::ECode  status,
                            int  err_code, const string &  err_msg,
                            const THighResolutionTimePoint &  start_timestamp)
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
    if (m_BlobRequest.m_BlobIdType == eBySeqId) {
        // By seq_id -> search for the bioseq key info in the cache only
        SResolveInputSeqIdError err;
        SBioseqResolution       bioseq_resolution(m_BlobRequest.m_StartTimestamp);

        x_ResolveInputSeqId(bioseq_resolution, err);
        if (err.HasError()) {
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
            return;
        }

        // Async resolution may be required
        if (bioseq_resolution.m_ResolutionResult == ePostponedForDB) {
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
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            x_OnBioseqError(CRequestStatus::e404_NotFound,
                            "Could not resolve seq_id " + m_BlobRequest.m_SeqId,
                            bioseq_resolution.m_RequestStartTimestamp);
        }
        return;
    }

    // A few cases here: comes from cache or DB
    // eFromSi2csiCache, eFromSi2csiDB, eFromBioseqCache, eFromBioseqDB

    if (bioseq_resolution.m_ResolutionResult == eFromSi2csiCache) {
        // Need to convert binary to a structured representation
        ConvertSi2csiToBioseqResolution(bioseq_resolution.m_CacheInfo,
                                        bioseq_resolution);
    }

    if (bioseq_resolution.m_ResolutionResult == eFromSi2csiCache ||
        bioseq_resolution.m_ResolutionResult == eFromSi2csiDB) {
        // The only key fields are available, need to pull the full
        // bioseq info
        CPSGCache           psg_cache(m_UrlUseCache != eCassandraOnly);
        ECacheLookupResult  cache_lookup_result =
                                psg_cache.LookupBioseqInfo(
                                        bioseq_resolution.m_BioseqInfo,
                                        bioseq_resolution.m_CacheInfo);
        if (cache_lookup_result == eFound) {
            ConvertBioseqProtobufToBioseqInfo(
                        bioseq_resolution.m_CacheInfo,
                        bioseq_resolution.m_BioseqInfo);
        } else {
            // No cache hit (or not allowed); need to get to DB if allowed
            if (m_UrlUseCache != eCacheOnly) {
                // Async DB query
                bioseq_resolution.m_CacheInfo.clear();
                m_AsyncInterruptPoint = eGetBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                              this));
                m_AsyncBioseqDetailsQuery->MakeRequest();
                return;
            }

            // default error message
            x_GetRequestBioseqInconsistency(
                                    bioseq_resolution.m_RequestStartTimestamp);
            return;
        }
    } else if (bioseq_resolution.m_ResolutionResult == eFromBioseqCache) {
            // Need to unpack the cache string to a structure
            ConvertBioseqProtobufToBioseqInfo(
                    bioseq_resolution.m_CacheInfo,
                    bioseq_resolution.m_BioseqInfo);
    }

    x_CompleteGetRequest(bioseq_resolution);
}


void CPendingOperation::x_CompleteGetRequest(
                                        SBioseqResolution &  bioseq_resolution)
{
    // Send BIOSEQ_INFO
    x_SendBioseqInfo(kEmptyStr, bioseq_resolution.m_BioseqInfo, eJsonFormat);
    x_RegisterResolveTiming(bioseq_resolution);

    // Translate sat to keyspace
    SBlobId     blob_id(bioseq_resolution.m_BioseqInfo.GetSat(),
                        bioseq_resolution.m_BioseqInfo.GetSatKey());

    if (!x_SatToSatName(m_BlobRequest, blob_id)) {
        // This is server data inconsistency error, so 500 (not 404)
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        return;
    }

    // retrieve BLOB_PROP & CHUNKS (if needed)
    m_BlobRequest.m_BlobId = blob_id;

    x_StartMainBlobRequest();
}


void CPendingOperation::x_GetRequestBioseqInconsistency(
                            const THighResolutionTimePoint &  start_timestamp)
{
    x_GetRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_BlobRequest.m_SeqId + " but the bioseq info is not found",
        start_timestamp);
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_GetRequestBioseqInconsistency(
                            const string &  err_msg,
                            const THighResolutionTimePoint &  start_timestamp)
{
    x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg,
                    start_timestamp);
}


void CPendingOperation::x_OnResolveResolutionError(
                            SResolveInputSeqIdError &  err,
                            const THighResolutionTimePoint &  start_timestamp)
{
    x_OnResolveResolutionError(err.m_ErrorCode, err.m_ErrorMessage,
                               start_timestamp);
}


void CPendingOperation::x_OnResolveResolutionError(
                            CRequestStatus::ECode  status,
                            const string &  message,
                            const THighResolutionTimePoint &  start_timestamp)
{
    PSG_WARNING(message);
    if (x_UsePsgProtocol()) {
        x_OnReplyError(status, eUnresolvedSeqId, message, start_timestamp);
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
    SBioseqResolution           bioseq_resolution(
                                            m_ResolveRequest.m_StartTimestamp);

    x_ResolveInputSeqId(bioseq_resolution, err);
    if (err.HasError()) {
        x_OnResolveResolutionError(err, m_ResolveRequest.m_StartTimestamp);
        return;
    }

    // Async resolution may be required
    if (bioseq_resolution.m_ResolutionResult == ePostponedForDB) {
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
                              m_ResolveRequest.m_SeqId;

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
    // eFromSi2csiCache, eFromSi2csiDB, eFromBioseqCache, eFromBioseqDB

    if (bioseq_resolution.m_ResolutionResult == eFromSi2csiCache) {
        // Need to convert binary to a structured representation
        ConvertSi2csiToBioseqResolution(
                bioseq_resolution.m_CacheInfo, bioseq_resolution);
    }

    if (bioseq_resolution.m_ResolutionResult == eFromSi2csiCache ||
        bioseq_resolution.m_ResolutionResult == eFromSi2csiDB) {
        // The only key fields are available, need to pull the full
        // bioseq info
        CPSGCache           psg_cache(m_UrlUseCache != eCassandraOnly);
        ECacheLookupResult  cache_lookup_result =
                                psg_cache.LookupBioseqInfo(
                                        bioseq_resolution.m_BioseqInfo,
                                        bioseq_resolution.m_CacheInfo);
        if (cache_lookup_result != eFound) {
            // No cache hit (or not allowed); need to get to DB if allowed
            if (m_UrlUseCache != eCacheOnly) {
                // Async DB query
                bioseq_resolution.m_CacheInfo.clear();
                m_AsyncInterruptPoint = eResolveBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(std::move(bioseq_resolution),
                                              this));
                m_AsyncBioseqDetailsQuery->MakeRequest();
                return;
            }

            // default error message
            x_ResolveRequestBioseqInconsistency(
                                    bioseq_resolution.m_RequestStartTimestamp);
            return;
        }
    } else if (bioseq_resolution.m_ResolutionResult == eFromBioseqDB) {
        // just in case
        bioseq_resolution.m_CacheInfo.clear();
    }

    x_CompleteResolveRequest(bioseq_resolution);
}


void CPendingOperation::x_ResolveRequestBioseqInconsistency(
                            const THighResolutionTimePoint &  start_timestamp)
{
    x_ResolveRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_ResolveRequest.m_SeqId + " but the bioseq info is not found",
        start_timestamp);
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_ResolveRequestBioseqInconsistency(
                            const string &  err_msg,
                            const THighResolutionTimePoint &  start_timestamp)
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
    x_SendBioseqInfo(bioseq_resolution.m_CacheInfo,
                     bioseq_resolution.m_BioseqInfo,
                     m_ResolveRequest.m_OutputFormat);
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
        default:
            ;
    }
}


void CPendingOperation::OnBioseqDetailsError(
                                CRequestStatus::ECode  status, int  code,
                                EDiagSev  severity, const string &  message,
                                const THighResolutionTimePoint &  start_timestamp)
{
    switch (m_AsyncInterruptPoint) {
        case eResolveBioseqDetails:
            x_ResolveRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " + m_ResolveRequest.m_SeqId + " but the "
                    "bioseq info is not found (database error: " +
                    message + ")", start_timestamp);
            break;
        case eGetBioseqDetails:
            x_GetRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for "
                    "seq_id " + m_BlobRequest.m_SeqId + " but the "
                    "bioseq info is not found (database error: " + message +
                    ")", start_timestamp);
            break;
        default:
            ;
    }
}


void CPendingOperation::x_ProcessAnnotRequest(void)
{
    SResolveInputSeqIdError     err;
    SBioseqResolution           bioseq_resolution(m_AnnotRequest.m_StartTimestamp);

    x_ResolveInputSeqId(bioseq_resolution, err);
    if (err.HasError()) {
        PSG_WARNING(err.m_ErrorMessage);
        x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId, err.m_ErrorMessage,
                       bioseq_resolution.m_RequestStartTimestamp);
        return;
    }

    // Async resolution may be required
    if (bioseq_resolution.m_ResolutionResult == ePostponedForDB) {
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
    CPSGCache                   psg_cache(m_UrlUseCache != eCassandraOnly);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    ECacheLookupResult          blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(m_TSEChunkRequest.m_TSEId.m_Sat,
                                 m_TSEChunkRequest.m_TSEId.m_SatKey,
                                 last_modified, *blob_record.get());
    if (blob_prop_cache_lookup_result == eFound) {
        // Compare the version
        unique_ptr<CPSGId2Info>     id2_info;
        if (!x_ParseTSEChunkId2Info(blob_record->GetId2Info(),
                                    id2_info, m_TSEChunkRequest.m_TSEId))
            return;

        if (id2_info->GetSplitVersion() == m_TSEChunkRequest.m_SplitVersion) {
            // Form the new blob id for the chunk and look for it in the
            // blob_prop cache. But first, sanity check for the chunk number
            if (!x_ValidateTSEChunkNumber(m_TSEChunkRequest.m_Chunk,
                                          id2_info->GetChunks()))
                return;

            // Chunk's blob id
            int64_t     sat_key = id2_info->GetInfo() - id2_info->GetChunks() - 1 + m_TSEChunkRequest.m_Chunk;
            SBlobId     chunk_blob_id(id2_info->GetSat(), sat_key);
            if (!x_TSEChunkSatToSatName(chunk_blob_id))
                return;

            last_modified = INT64_MIN;
            blob_prop_cache_lookup_result = psg_cache.LookupBlobProp(
                    chunk_blob_id.m_Sat, chunk_blob_id.m_SatKey,
                    last_modified, *blob_record.get());
            if (blob_prop_cache_lookup_result != eFound) {
                err_msg = "Blob " + chunk_blob_id.ToString() +
                          " properties are not found";
                x_SendReplyError(err_msg, CRequestStatus::e404_NotFound,
                                 eBlobPropsNotFound);
                x_SendReplyCompletion();
                return;
            }

            // Initiate the chunk request
            app->GetRequestCounters().IncTSEChunkSplitVersionCacheMatched();

            SBlobRequest        chunk_request(chunk_blob_id, INT64_MIN,
                                              eUnknownTSE, eUnknownUseCache, "");

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

            m_FetchDetails.push_back(std::move(fetch_details));
            load_task->Wait();  // Initiate cassandra request
            return;
        }

        app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
    } else {
        // Not found in the blob_prop cache
        if (m_UrlUseCache == eCacheOnly) {
            // No need to continue; it is forbidded to look for blob props in
            // the Cassandra DB
            if (blob_prop_cache_lookup_result == eNotFound) {
                err_msg = "Blob properties are not found";
            } else {
                err_msg = "Blob properties are not found due to LMDB cache error";
            }
            x_SendReplyError(err_msg, CRequestStatus::e404_NotFound, eBlobPropsNotFound);
            x_SendReplyCompletion();
            return;
        }
    }

    // Here:
    // - not found in the cache
    // - cache is not allowed
    // - found in cache but the version is not the last

    // Initiate async the history request
    unique_ptr<CCassSplitHistoryFetch>      fetch_details;
    fetch_details.reset(new CCassSplitHistoryFetch(m_TSEChunkRequest));
    CCassBlobTaskFetchSplitHistory *   load_task =
        new  CCassBlobTaskFetchSplitHistory(m_Timeout, m_MaxRetries, m_Conn,
                                            m_TSEChunkRequest.m_TSEId.m_SatName,
                                            m_TSEChunkRequest.m_TSEId.m_SatKey,
                                            m_TSEChunkRequest.m_SplitVersion,
                                            nullptr, nullptr);
    fetch_details->SetLoader(load_task);
    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CSplitHistoryErrorCallback(this, fetch_details.get()));
    load_task->SetConsumeCallback(CSplitHistoryConsumeCallback(this, fetch_details.get()));

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
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId,
                           err.m_ErrorMessage,
                           bioseq_resolution.m_RequestStartTimestamp);
        } else {
            // Could not resolve, send 404
            x_OnReplyError(CRequestStatus::e404_NotFound, eNoBioseqInfo,
                           "Could not resolve seq_id " + m_AnnotRequest.m_SeqId,
                           bioseq_resolution.m_RequestStartTimestamp);
        }
        return;
    }

    switch (bioseq_resolution.m_ResolutionResult) {
        case eFromSi2csiCache:
            ConvertSi2csiToBioseqResolution(
                    bioseq_resolution.m_CacheInfo, bioseq_resolution);
            break;
        case eFromBioseqCache:
            ConvertBioseqProtobufToBioseqInfo(
                    bioseq_resolution.m_CacheInfo,
                    bioseq_resolution.m_BioseqInfo);
            break;
        case eFromBioseqDB:
        case eFromSi2csiDB:
        default:
            break;
    }

    x_RegisterResolveTiming(bioseq_resolution);

    vector<pair<string, int32_t>>  bioseq_na_keyspaces =
                CPubseqGatewayApp::GetInstance()->GetBioseqNAKeyspaces();

    for (const auto &  bioseq_na_keyspace : bioseq_na_keyspaces) {
        unique_ptr<CCassNamedAnnotFetch>   details;
        details.reset(new CCassNamedAnnotFetch(m_AnnotRequest));

        CCassNAnnotTaskFetch *  fetch_task =
                new CCassNAnnotTaskFetch(m_Timeout, m_MaxRetries, m_Conn,
                                         bioseq_na_keyspace.first,
                                         bioseq_resolution.m_BioseqInfo.GetAccession(),
                                         bioseq_resolution.m_BioseqInfo.GetVersion(),
                                         bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                         m_AnnotRequest.m_Names,
                                         nullptr, nullptr);
        details->SetLoader(fetch_task);

        fetch_task->SetConsumeCallback(
            CNamedAnnotationCallback(this, details.get(), bioseq_na_keyspace.second));
        fetch_task->SetErrorCB(CNamedAnnotationErrorCallback(this, details.get()));
        fetch_task->SetDataReadyCB(GetDataReadyCB());

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
    if (m_BlobRequest.IsExcludedBlob()) {
        m_ProtocolSupport.PrepareBlobExcluded(
            m_ProtocolSupport.GetItemId(), m_BlobRequest.m_BlobId, eExcluded);
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        return;
    }

    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();

    if (m_BlobRequest.m_TSEOption != eNoneTSE &&
        m_BlobRequest.m_TSEOption != eSlimTSE) {
        if (!m_BlobRequest.m_ClientId.empty()) {
            // Adding to exclude blob cache is unconditional however skipping
            // is only for the blobs identified by seq_id/seq_id_type
            bool                 completed = true;
            ECacheAddResult      cache_result =
                app->GetExcludeBlobCache()->AddBlobId(
                        m_BlobRequest.m_ClientId,
                        m_BlobRequest.m_BlobId.m_Sat,
                        m_BlobRequest.m_BlobId.m_SatKey,
                        completed);
            if (m_BlobRequest.m_BlobIdType == eBySeqId) {
                if (cache_result == eAlreadyInCache) {
                    if (completed)
                        m_ProtocolSupport.PrepareBlobExcluded(
                            m_BlobRequest.m_BlobId, eSent);
                    else
                        m_ProtocolSupport.PrepareBlobExcluded(
                            m_BlobRequest.m_BlobId, eInProgress);
                    x_SendReplyCompletion(true);
                    m_ProtocolSupport.Flush();
                    return;
                }
            }
            if (cache_result == eAdded)
                m_BlobRequest.m_ExcludeBlobCacheAdded = true;
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(m_BlobRequest));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_UrlUseCache != eCassandraOnly);
    ECacheLookupResult          blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        m_BlobRequest.m_BlobId.m_Sat,
                                        m_BlobRequest.m_BlobId.m_SatKey,
                                        m_BlobRequest.m_LastModified,
                                        *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == eFound) {
        load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                              m_BlobRequest.m_BlobId.m_SatName,
                                              std::move(blob_record),
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    } else {
        if (m_UrlUseCache == eCacheOnly) {
            // No data in cache and not going to the DB
            if (blob_prop_cache_lookup_result == eNotFound)
                x_OnReplyError(CRequestStatus::e404_NotFound, eBlobPropsNotFound,
                               "Blob properties are not found",
                               m_BlobRequest.m_StartTimestamp);
            else
                x_OnReplyError(CRequestStatus::e500_InternalServerError,
                               eBlobPropsNotFound,
                               "Blob properties are not found "
                               "due to a cache lookup error",
                               m_BlobRequest.m_StartTimestamp);

            if (m_BlobRequest.m_ExcludeBlobCacheAdded &&
                !m_BlobRequest.m_ClientId.empty()) {
                app->GetExcludeBlobCache()->Remove(
                    m_BlobRequest.m_ClientId,
                    m_BlobRequest.m_BlobId.m_Sat,
                    m_BlobRequest.m_BlobId.m_SatKey);

                // To prevent SetCompleted() later
                m_BlobRequest.m_ExcludeBlobCacheAdded = false;
            }
            return;
        }

        if (m_BlobRequest.m_LastModified == INT64_MIN) {
            load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                                  m_BlobRequest.m_BlobId.m_SatName,
                                                  m_BlobRequest.m_BlobId.m_SatKey,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        } else {
            load_task = new CCassBlobTaskLoadBlob(m_Timeout, m_MaxRetries, m_Conn,
                                                  m_BlobRequest.m_BlobId.m_SatName,
                                                  m_BlobRequest.m_BlobId.m_SatKey,
                                                  m_BlobRequest.m_LastModified,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        }
    }

    load_task->SetDataReadyCB(GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, fetch_details.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, fetch_details.get(),
                                                  blob_prop_cache_lookup_result != eFound));

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
    if (resp.IsOutputReady()) {
       if (m_RequestType == eBlobRequest) {
           m_ProtocolSupport.Flush(all_finished_read);
       } else {
            if (all_finished_read) {
                m_ProtocolSupport.Flush();
            }
       }
    }

    if (m_RequestType == eBlobRequest &&
        all_finished_read &&
        m_BlobRequest.m_ExcludeBlobCacheAdded &&
        !m_BlobRequest.m_ExcludeBlobCacheCompleted &&
        !m_BlobRequest.m_ClientId.empty()) {
        CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
        app->GetExcludeBlobCache()->SetCompleted(
                m_BlobRequest.m_ClientId,
                m_BlobRequest.m_BlobId.m_Sat,
                m_BlobRequest.m_BlobId.m_SatKey, true);
        m_BlobRequest.m_ExcludeBlobCacheCompleted = true;
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

        EPendingRequestType     request_type = fetch_details->GetRequestType();
        if (request_type == eBlobRequest || request_type == eTSEChunkRequest) {
            CCassBlobFetch *  blob_fetch = static_cast<CCassBlobFetch *>(fetch_details.get());
            if (blob_fetch->IsBlobPropStage()) {
                m_ProtocolSupport.PrepareBlobPropMessage(blob_fetch, error,
                                                         CRequestStatus::e500_InternalServerError,
                                                         eUnknownError, eDiag_Error);
                m_ProtocolSupport.PrepareBlobPropCompletion(blob_fetch);
            } else {
                m_ProtocolSupport.PrepareBlobMessage(blob_fetch, error,
                                                     CRequestStatus::e500_InternalServerError,
                                                     eUnknownError, eDiag_Error);
                m_ProtocolSupport.PrepareBlobCompletion(blob_fetch);
            }
        } else if (request_type == eAnnotationRequest || request_type == eBioseqInfoRequest) {
            m_ProtocolSupport.PrepareReplyMessage(error,
                                                  CRequestStatus::e500_InternalServerError,
                                                  eUnknownError, eDiag_Error);
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
        // +1 for the reply completion itself
        m_ProtocolSupport.PrepareReplyCompletion();

        // No need to set the context/engage context resetter: they set outside
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


ESeqIdParsingResult CPendingOperation::x_ParseInputSeqId(CSeq_id &  seq_id,
                                                         string &  err_msg)
{
    try {
        seq_id.Set(m_UrlSeqId);

        if (m_UrlSeqIdType < 0)
            return eParsedOK;

        // Check the parsed type with the given
        int16_t     eff_seq_id_type;
        if (x_GetEffectiveSeqIdType(seq_id, eff_seq_id_type))
            return eParsedOK;

        // Type mismatch: form the error message in case of resolution problems
        CSeq_id_Base::E_Choice  seq_id_type = seq_id.Which();
        err_msg = "Seq_id " + m_UrlSeqId +
                  " possible type mismatch: the URL provides " +
                  NStr::NumericToString(m_UrlSeqIdType) +
                  " while the CSeq_Id detects it as " +
                  NStr::NumericToString(static_cast<int>(seq_id_type));

        // Here: fall through; maybe another variation of Set() will work

    } catch (...) {
        // Try another Set() variation below
    }

    // Second variation of Set()
    if (m_UrlSeqIdType >= 0) {
        try {
            seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                       (CSeq_id_Base::E_Choice)(m_UrlSeqIdType),
                       m_UrlSeqId);
            return eParsedOK;
        } catch (...) {
        }
    }
    return eParseFailed;
}


void CPendingOperation::x_InitUrlIndentification(void)
{
    switch (m_RequestType) {
        case eBlobRequest:
            m_UrlSeqId = m_BlobRequest.m_SeqId;
            m_UrlSeqIdType = m_BlobRequest.m_SeqIdType;
            break;
        case eResolveRequest:
            m_UrlSeqId = m_ResolveRequest.m_SeqId;
            m_UrlSeqIdType = m_ResolveRequest.m_SeqIdType;
            break;
        case eAnnotationRequest:
            m_UrlSeqId = m_AnnotRequest.m_SeqId;
            m_UrlSeqIdType = m_AnnotRequest.m_SeqIdType;
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Not handled request type " +
                       NStr::NumericToString(static_cast<int>(m_RequestType)));
    }
}


bool CPendingOperation::x_ValidateTSEChunkNumber(int64_t  requested_chunk,
                                                 CPSGId2Info::TChunks  total_chunks)
{
    if (requested_chunk > total_chunks) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncMalformedArguments();
        x_SendReplyError(
                "Invalid chunk requested. The number of available chunks: " +
                NStr::NumericToString(total_chunks) + ", requested number: " +
                NStr::NumericToString(requested_chunk),
                CRequestStatus::e400_BadRequest, eMalformedParameter);
        x_SendReplyCompletion();
        return false;
    }
    return true;
}


void
CPendingOperation::x_ResolveInputSeqId(SBioseqResolution &  bioseq_resolution,
                                       SResolveInputSeqIdError &  err)
{
    x_InitUrlIndentification();

    auto                    app = CPubseqGatewayApp::GetInstance();
    string                  parse_err_msg;
    CSeq_id                 oslt_seq_id;
    ESeqIdParsingResult     parsing_result = x_ParseInputSeqId(oslt_seq_id,
                                                               parse_err_msg);

    // The results of the ComposeOSLT are used in both cache and DB
    int16_t                 effective_seq_id_type;
    list<string>            secondary_id_list;
    string                  primary_id;
    bool                    composed_ok = false;
    if (parsing_result == eParsedOK) {
        composed_ok = x_ComposeOSLT(oslt_seq_id, effective_seq_id_type,
                                    secondary_id_list, primary_id);
    }

    if (m_UrlUseCache != eCassandraOnly) {
        // Try cache
        auto    start = chrono::high_resolution_clock::now();

        if (composed_ok)
            x_ResolveViaComposeOSLTInCache(oslt_seq_id, effective_seq_id_type,
                                           secondary_id_list, primary_id,
                                           err, bioseq_resolution);
        else
            x_ResolveAsIsInCache(bioseq_resolution, err);

        if (bioseq_resolution.IsValid()) {
            app->GetTiming().Register(eResolutionLmdb, eOpStatusFound, start);
            return;
        }
        app->GetTiming().Register(eResolutionLmdb, eOpStatusNotFound, start);
    }

    if (m_UrlUseCache != eCacheOnly) {
        // Need to initiate async DB resolution

        // Memorize an error if there was one
        if (!parse_err_msg.empty() && !err.HasError()) {
            err.m_ErrorMessage = parse_err_msg;
            err.m_ErrorCode = CRequestStatus::e400_BadRequest;
        }

        bioseq_resolution.m_PostponedError = std::move(err);
        err.Reset();

        // Async request
        m_AsyncSeqIdResolver.reset(new CAsyncSeqIdResolver(
                                            oslt_seq_id, effective_seq_id_type,
                                            secondary_id_list, primary_id,
                                            m_UrlSeqId, composed_ok,
                                            std::move(bioseq_resolution), this));
        m_AsyncCassResolutionStart = chrono::high_resolution_clock::now();
        m_AsyncSeqIdResolver->Process();


        // The CAsyncSeqIdResolver moved the bioseq_resolution, so it is safe
        // to signal the return here
        bioseq_resolution.Reset();
        bioseq_resolution.m_ResolutionResult = ePostponedForDB;
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
            err.m_ErrorCode = CRequestStatus::e400_BadRequest;
        }
    }
}


ECacheLookupResult CPendingOperation::x_ResolvePrimaryOSLTInCache(
                                const string &  primary_id,
                                int16_t  effective_version,
                                int16_t  effective_seq_id_type,
                                bool  need_to_try_bioseq_info,
                                SBioseqResolution &  bioseq_resolution)
{
    ECacheLookupResult  bioseq_cache_lookup_result = eNotFound;

    if (!primary_id.empty()) {
        CPSGCache           psg_cache(true);

        if (need_to_try_bioseq_info) {
            // Try BIOSEQ_INFO
            bioseq_resolution.m_BioseqInfo.SetAccession(primary_id);
            bioseq_resolution.m_BioseqInfo.SetVersion(effective_version);
            bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

            bioseq_cache_lookup_result =
                    psg_cache.LookupBioseqInfo(bioseq_resolution.m_BioseqInfo,
                                               bioseq_resolution.m_CacheInfo);
            if (bioseq_cache_lookup_result == eFound) {
                bioseq_resolution.m_ResolutionResult = eFromBioseqCache;
                return eFound;
            }
        }

        // Second try: SI2CSI
        bioseq_resolution.m_BioseqInfo.SetAccession(primary_id);
        bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

        ECacheLookupResult  si2csi_cache_lookup_result = psg_cache.LookupSi2csi(
                                    bioseq_resolution.m_BioseqInfo,
                                    bioseq_resolution.m_CacheInfo);

        if (si2csi_cache_lookup_result == eFound) {
            bioseq_resolution.m_ResolutionResult = eFromSi2csiCache;
            return eFound;
        }

        bioseq_resolution.Reset();

        if (si2csi_cache_lookup_result == eFailure)
            return eFailure;
    }

    if (bioseq_cache_lookup_result == eFailure)
        return eFailure;
    return eNotFound;
}


ECacheLookupResult CPendingOperation::x_ResolveSecondaryOSLTInCache(
                                const string &  secondary_id,
                                int16_t  effective_seq_id_type,
                                SBioseqResolution &  bioseq_resolution)
{
    bioseq_resolution.m_BioseqInfo.SetAccession(secondary_id);
    bioseq_resolution.m_BioseqInfo.SetSeqIdType(effective_seq_id_type);

    CPSGCache           psg_cache(true);
    ECacheLookupResult  si2csi_cache_lookup_result =
                psg_cache.LookupSi2csi(bioseq_resolution.m_BioseqInfo,
                                       bioseq_resolution.m_CacheInfo);
    if (si2csi_cache_lookup_result == eFound) {
        bioseq_resolution.m_ResolutionResult = eFromSi2csiCache;
        return eFound;
    }

    bioseq_resolution.Reset();

    if (si2csi_cache_lookup_result == eFailure)
        return eFailure;
    return eNotFound;
}


bool
CPendingOperation::x_ComposeOSLT(CSeq_id &  parsed_seq_id,
                                 int16_t &  effective_seq_id_type,
                                 list<string> &  secondary_id_list,
                                 string &  primary_id)
{
    if (!x_GetEffectiveSeqIdType(parsed_seq_id, effective_seq_id_type))
        return false;

    try {
        primary_id = parsed_seq_id.ComposeOSLT(&secondary_id_list);
    } catch (...) {
        return false;
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

    // Optimization (premature?) to avoid trying bioseq_info in some cases
    bool                    need_to_try_bioseq_info = true;
    if (text_seq_id == nullptr || !text_seq_id->CanGetAccession()) {
        need_to_try_bioseq_info = false;
    }

    bool                    cache_failure = false;

    if (!primary_id.empty()) {
        ECacheLookupResult  cache_lookup_result =
                x_ResolvePrimaryOSLTInCache(primary_id, effective_version,
                                            effective_seq_id_type,
                                            need_to_try_bioseq_info,
                                            bioseq_resolution);
        if (cache_lookup_result == eFound)
            return;
        if (cache_lookup_result == eFailure)
            cache_failure = true;
    }

    for (const auto &  secondary_id : secondary_id_list) {
        ECacheLookupResult  cache_lookup_result =
                x_ResolveSecondaryOSLTInCache(secondary_id, effective_seq_id_type,
                                              bioseq_resolution);
        if (cache_lookup_result == eFound)
            return;
        if (cache_lookup_result == eFailure) {
            cache_failure = true;
            break;
        }
    }

    // Try cache as it came from URL
    ECacheLookupResult  cache_lookup_result =
            x_ResolveAsIsInCache(bioseq_resolution, err);
    if (cache_lookup_result == eFound)
        return;
    if (cache_lookup_result == eFailure)
        cache_failure = true;

    bioseq_resolution.Reset();

    if (cache_failure) {
        err.m_ErrorMessage = "Cache lookup failure";
        err.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }
}


ECacheLookupResult
CPendingOperation::x_ResolveAsIsInCache(SBioseqResolution &  bioseq_resolution,
                                        SResolveInputSeqIdError &  err)
{
    ECacheLookupResult  cache_lookup_result = eNotFound;

    // Need to capitalize the seq_id before going to the tables.
    // Capitalizing in place suites because the other tries are done via
    // copies provided by OSLT
    char *      current = (char *)(m_UrlSeqId.data());
    for (size_t  index = 0; index < m_UrlSeqId.size(); ++index, ++current)
        *current = (char)toupper((unsigned char)(*current));

    // 1. As is
    cache_lookup_result = x_ResolveSecondaryOSLTInCache(m_UrlSeqId, m_UrlSeqIdType,
                                                        bioseq_resolution);
    if (cache_lookup_result == eNotFound) {
        // 2. if there are | at the end => strip all trailing bars
        //    else => add one | at the end
        if (m_UrlSeqId[m_UrlSeqId.size() - 1] == '|') {
            CTempString     strip_bar_seq_id(m_UrlSeqId);
            while (strip_bar_seq_id[strip_bar_seq_id.size() - 1] == '|')
                strip_bar_seq_id.erase(strip_bar_seq_id.size() - 1);

            cache_lookup_result = x_ResolveSecondaryOSLTInCache(strip_bar_seq_id,
                                                                m_UrlSeqIdType,
                                                                bioseq_resolution);
        } else {
            string      seq_id_added_bar(m_UrlSeqId, m_UrlSeqId.size());
            seq_id_added_bar.append(1, '|');
            cache_lookup_result = x_ResolveSecondaryOSLTInCache(seq_id_added_bar,
                                                                m_UrlSeqIdType,
                                                                bioseq_resolution);
        }
    }

    if (cache_lookup_result == eFailure) {
        bioseq_resolution.Reset();
        err.m_ErrorMessage = "Cache lookup failure";
        err.m_ErrorCode = CRequestStatus::e500_InternalServerError;
    }

    return cache_lookup_result;
}


bool CPendingOperation::x_GetEffectiveSeqIdType(const CSeq_id &  parsed_seq_id,
                                                int16_t &  eff_seq_id_type)
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


void CPendingOperation::x_SendBioseqInfo(const string &  protobuf_bioseq_info,
                                         CBioseqInfoRecord &  bioseq_info,
                                         EOutputFormat  output_format)
{
    EOutputFormat       effective_output_format = output_format;

    if (output_format == eNativeFormat || output_format == eUnknownFormat) {
        if (protobuf_bioseq_info.empty())
            effective_output_format = eJsonFormat;
        else
            effective_output_format = eProtobufFormat;
    }

    string              data_to_send;
    const string *      data_ptr = &data_to_send;   // To avoid copying in case
                                                    // of protobuf

    if (effective_output_format == eJsonFormat) {
        if (!protobuf_bioseq_info.empty())
            ConvertBioseqProtobufToBioseqInfo(protobuf_bioseq_info,
                                              bioseq_info);

        ConvertBioseqInfoToJson(
                bioseq_info,
                m_RequestType == eResolveRequest ? m_ResolveRequest.m_IncludeDataFlags :
                                                   fServAllBioseqFields,
                data_to_send);
    } else {
        if (protobuf_bioseq_info.empty()) {
            ConvertBioseqInfoToBioseqProtobuf(bioseq_info, data_to_send);
        } else {
            data_ptr = &protobuf_bioseq_info;
        }
    }

    if (x_UsePsgProtocol()) {
        // Send it as the PSG protocol
        size_t              item_id = m_ProtocolSupport.GetItemId();
        m_ProtocolSupport.PrepareBioseqData(item_id, *data_ptr,
                                            effective_output_format);
        m_ProtocolSupport.PrepareBioseqCompletion(item_id, 2);
    } else {
        // Send it as the HTTP data
        if (effective_output_format == eJsonFormat)
            m_ProtocolSupport.SendData(data_ptr, eJsonMime);
        else
            m_ProtocolSupport.SendData(data_ptr, eBinaryMime);
    }
}


bool CPendingOperation::x_UsePsgProtocol(void) const
{
    if (m_RequestType == eResolveRequest)
        return m_ResolveRequest.m_UsePsgProtocol;
    return true;
}


bool CPendingOperation::x_SatToSatName(const SBlobRequest &  blob_request,
                                       SBlobId &  blob_id)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToSatName(blob_id.m_Sat, blob_id.m_SatName))
        return true;

    app->GetErrorCounters().IncServerSatToSatName();

    string      msg = "Unknown satellite number " +
                      NStr::NumericToString(blob_id.m_Sat);
    if (blob_request.m_BlobIdType == eBySeqId)
        msg += " for bioseq info with seq_id '" +
               blob_request.m_SeqId + "'";
    else
        msg += " for the blob " + blob_id.ToString();

    // It is a server error - data inconsistency
    x_SendBlobPropError(m_ProtocolSupport.GetItemId(), msg,
                        eUnknownResolvedSatellite);
    return false;
}


bool CPendingOperation::x_TSEChunkSatToSatName(SBlobId &  blob_id)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToSatName(blob_id.m_Sat, blob_id.m_SatName))
        return true;

    app->GetErrorCounters().IncServerSatToSatName();

    string  msg = "Unknown satellite number " +
                  NStr::NumericToString(blob_id.m_Sat) +
                  " for the blob " + blob_id.ToString();
    x_SendReplyError(msg, CRequestStatus::e500_InternalServerError,
                     eUnknownResolvedSatellite);
    x_SendReplyCompletion();
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
    PSG_ERROR(msg);
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
        fetch_details->SetReadFinished();
        x_SendReplyCompletion();
    } else {
        CJsonNode   json = ConvertBioseqNAToJson(annot_record, sat);
        m_ProtocolSupport.PrepareNamedAnnotationData(annot_record.GetAccession(),
                                                     annot_record.GetVersion(),
                                                     annot_record.GetSeqIdType(),
                                                     annot_record.GetAnnotName(),
                                                     json.Repr(CJsonNode::fStandardJson));
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

    if (is_found) {
        // Found, send blob props back as JSON
        CJsonNode   json = ConvertBlobPropToJson(blob);
        m_ProtocolSupport.PrepareBlobPropData(fetch_details,
                                              json.Repr(CJsonNode::fStandardJson));

        // Note: initially only blob_props are requested and at that moment the
        //       TSE option is 'known'. So the initial request should be
        //       finished, see m_FinishedRead = true
        //       In the further requests of the blob data regardless with blob
        //       props or not, the TSE option is set to unknown so the request
        //       will be finished at the moment when blob chunks are handled.
        switch (fetch_details->GetTSEOption()) {
            case eNoneTSE:
                x_OnBlobPropNoneTSE(fetch_details);
                break;
            case eSlimTSE:
                x_OnBlobPropSlimTSE(fetch_details, blob);
                break;
            case eSmartTSE:
                x_OnBlobPropSmartTSE(fetch_details, blob);
                break;
            case eWholeTSE:
                x_OnBlobPropWholeTSE(fetch_details, blob);
                break;
            case eOrigTSE:
                x_OnBlobPropOrigTSE(fetch_details, blob);
                break;
            case eUnknownTSE:
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

    string      message = "Blob properties are not found";
    string      blob_prop_msg;
    if (fetch_details->GetBlobIdType() == eBySatAndSatKey) {
        // User requested wrong sat_key, so it is a client error
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e404_NotFound,
                                eBlobPropsNotFound, eDiag_Error);
    } else {
        // Server error, data inconsistency
        PSG_ERROR(message);
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                eBlobPropsNotFound, eDiag_Error);
    }

    SBlobId     blob_id = fetch_details->GetBlobId();
    if (blob_id == m_BlobRequest.m_BlobId) {
        if (m_BlobRequest.m_ExcludeBlobCacheAdded &&
            !m_BlobRequest.m_ClientId.empty()) {
            app->GetExcludeBlobCache()->Remove(m_BlobRequest.m_ClientId,
                                               blob_id.m_Sat, blob_id.m_SatKey);
            m_BlobRequest.m_ExcludeBlobCacheAdded = false;
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
    SBlobId                 blob_id = fetch_details->GetBlobId();

    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);

        // An original blob may be required if its size is small
        unsigned int         slim_max_blob_size = app->GetSlimMaxBlobSize();

        if (blob.GetSize() <= slim_max_blob_size) {
            // The blob is small, get it, but first check in the
            // exclude blob cache
            if (blob_id == m_BlobRequest.m_BlobId && !m_BlobRequest.m_ClientId.empty()) {
                bool                 completed = true;
                ECacheAddResult      cache_result =
                    app->GetExcludeBlobCache()->AddBlobId(
                            m_BlobRequest.m_ClientId,
                            blob_id.m_Sat, blob_id.m_SatKey, completed);
                if (m_BlobRequest.m_BlobIdType == eBySeqId && cache_result == eAlreadyInCache) {
                    if (completed)
                        m_ProtocolSupport.PrepareBlobExcluded(blob_id, eSent);
                    else
                        m_ProtocolSupport.PrepareBlobExcluded(blob_id, eInProgress);
                    x_SendReplyCompletion(true);
                    return;
                }

                if (cache_result == eAdded)
                    m_BlobRequest.m_ExcludeBlobCacheAdded = true;
            }

            x_RequestOriginalBlobChunks(fetch_details, blob);
        } else {
            // Nothing else to be sent, the original blob is big
            x_SendReplyCompletion();
        }
    } else {
        // Check the cache first - only if it is about the main
        // blob request
        if (blob_id == m_BlobRequest.m_BlobId && !m_BlobRequest.m_ClientId.empty()) {
            bool                 completed = true;
            ECacheAddResult      cache_result =
                app->GetExcludeBlobCache()->AddBlobId(
                        m_BlobRequest.m_ClientId,
                        blob_id.m_Sat, blob_id.m_SatKey, completed);
            if (m_BlobRequest.m_BlobIdType == eBySeqId && cache_result == eAlreadyInCache) {
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                if (completed)
                    m_ProtocolSupport.PrepareBlobExcluded(blob_id, eSent);
                else
                    m_ProtocolSupport.PrepareBlobExcluded(blob_id, eInProgress);
                x_SendReplyCompletion(true);
                return;
            }

            if (cache_result == eAdded)
                m_BlobRequest.m_ExcludeBlobCacheAdded = true;
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
        // A blob chunk; 0-length chunks are allowed too
        m_ProtocolSupport.PrepareBlobData(fetch_details,
                                          chunk_data, data_size, chunk_no);
    } else {
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
    PSG_ERROR(message);

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

        SBlobId     blob_id = fetch_details->GetBlobId();
        if (blob_id == m_BlobRequest.m_BlobId) {
            if (m_BlobRequest.m_ExcludeBlobCacheAdded &&
                !m_BlobRequest.m_ClientId.empty()) {
                app->GetExcludeBlobCache()->Remove(m_BlobRequest.m_ClientId,
                                                   blob_id.m_Sat, blob_id.m_SatKey);

                // To prevent any updates
                m_BlobRequest.m_ExcludeBlobCacheAdded = false;
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

    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
    if (result.empty()) {
        // Split history is not found
        app->GetErrorCounters().IncSplitHistoryNotFoundError();

        string      message = "Split history version " +
                              NStr::NumericToString(fetch_details->GetSplitVersion()) +
                              " is not found for the TSE id " +
                              fetch_details->GetTSEId().ToString();
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_ProtocolSupport.PrepareReplyMessage(message,
                                              CRequestStatus::e404_NotFound,
                                              eSplitHistoryNotFound, eDiag_Error);
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
                                id2_info, fetch_details->GetTSEId()))
        return;

    // Check the requested chunk
    if (!x_ValidateTSEChunkNumber(fetch_details->GetChunk(),
                                  id2_info->GetChunks()))
        return;

    // Resolve sat to satkey
    int64_t     sat_key = id2_info->GetInfo() - id2_info->GetChunks() - 1 + fetch_details->GetChunk();
    SBlobId     chunk_blob_id(id2_info->GetSat(), sat_key);
    if (!x_TSEChunkSatToSatName(chunk_blob_id))
        return;

    // Look for the blob props
    // Form the chunk request with/without blob props
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(fetch_details->GetUseCache() != eCassandraOnly);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    ECacheLookupResult          blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(chunk_blob_id.m_Sat,
                                 chunk_blob_id.m_SatKey,
                                 last_modified, *blob_record.get());

    SBlobRequest                chunk_request(chunk_blob_id, INT64_MIN,
                                              eUnknownTSE, eUnknownUseCache, "");
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(chunk_request));

    CCassBlobTaskLoadBlob *     load_task = nullptr;

    if (blob_prop_cache_lookup_result == eFound) {
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
                                                  blob_prop_cache_lookup_result != eFound));
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

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
    PSG_ERROR(message);

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
    SBlobRequest    orig_blob_request(fetch_details->GetBlobId(),
                                      blob.GetModified(), eUnknownTSE,
                                      eUnknownUseCache,
                                      fetch_details->GetClientId());

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
    SBlobId     info_blob_id(m_Id2Info->GetSat(), m_Id2Info->GetInfo());    // mandatory

    if (!app->SatToSatName(m_Id2Info->GetSat(), info_blob_id.m_SatName)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              NStr::NumericToString(m_Id2Info->GetSat()) +
                              ") to a cassandra keyspace for the blob " +
                              fetch_details->GetBlobId().ToString();
        m_ProtocolSupport.PrepareBlobPropMessage(
                                fetch_details, message,
                                CRequestStatus::e500_InternalServerError,
                                eBadID2Info, eDiag_Error);
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
    SBlobRequest    info_blob_request(info_blob_id, INT64_MIN, eUnknownTSE,
                                      eUnknownUseCache, "");

    // Prepare Id2Info retrieval
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(info_blob_request));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_UrlUseCache != eCassandraOnly);
    ECacheLookupResult          blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        info_blob_request.m_BlobId.m_Sat,
                                        info_blob_request.m_BlobId.m_SatKey,
                                        info_blob_request.m_LastModified,
                                        *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;


    if (blob_prop_cache_lookup_result == eFound) {
        load_task = new CCassBlobTaskLoadBlob(
                        m_Timeout, m_MaxRetries, m_Conn,
                        info_blob_request.m_BlobId.m_SatName,
                        std::move(blob_record),
                        true, nullptr);
    } else {
        if (m_UrlUseCache == eCacheOnly) {
            // No need to continue; it is forbidded to look for blob props in
            // the Cassandra DB
            string      message;

            if (blob_prop_cache_lookup_result == eNotFound) {
                message = "Blob properties are not found";
                UpdateOverallStatus(CRequestStatus::e404_NotFound);
                m_ProtocolSupport.PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e404_NotFound,
                                    eBlobPropsNotFound, eDiag_Error);
            } else {
                message = "Blob properties are not found due to LMDB cache error";
                UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                m_ProtocolSupport.PrepareBlobPropMessage(
                                    fetch_details, message,
                                    CRequestStatus::e500_InternalServerError,
                                    eBlobPropsNotFound, eDiag_Error);
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
                                                  blob_prop_cache_lookup_result != eFound));
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

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
        SBlobId     chunks_blob_id(m_Id2Info->GetSat(),
                                   m_Id2Info->GetInfo() - m_Id2Info->GetChunks() - 1 + chunk_no);
        chunks_blob_id.m_SatName = sat_name;

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        // client_id is "" (empty string) so the split blobs do not participate
        // in the exclude blob cache
        SBlobRequest    chunk_request(chunks_blob_id, INT64_MIN, eUnknownTSE,
                                      eUnknownUseCache, "");

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_UrlUseCache != eCassandraOnly);
        ECacheLookupResult          blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            chunk_request.m_BlobId.m_Sat,
                                            chunk_request.m_BlobId.m_SatKey,
                                            chunk_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == eFound) {
            load_task = new CCassBlobTaskLoadBlob(
                            m_Timeout, m_MaxRetries, m_Conn,
                            chunk_request.m_BlobId.m_SatName,
                            std::move(blob_record),
                            true, nullptr);
            details->SetLoader(load_task);
        } else {
            if (m_UrlUseCache == eCacheOnly) {
                // No need to create a request because the Cassandra DB access
                // is forbidden
                string      message;
                if (blob_prop_cache_lookup_result == eNotFound) {
                    message = "Blob properties are not found";
                    UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    m_ProtocolSupport.PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e404_NotFound,
                                        eBlobPropsNotFound, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    m_ProtocolSupport.PrepareBlobPropMessage(
                                        details.get(), message,
                                        CRequestStatus::e500_InternalServerError,
                                        eBlobPropsNotFound, eDiag_Error);
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
                                                      blob_prop_cache_lookup_result != eFound));
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
        eBadID2Info, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
    return false;
}


bool CPendingOperation::x_ParseTSEChunkId2Info(const string &  info,
                                               unique_ptr<CPSGId2Info> &  id2_info,
                                               const SBlobId &  blob_id)
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
    x_SendReplyError(err_msg, CRequestStatus::e500_InternalServerError,
                     eInvalidId2Info);
    x_SendReplyCompletion();
    return false;
}


void CPendingOperation::OnSeqIdAsyncResolutionFinished(
                                SBioseqResolution &&  async_bioseq_resolution)
{
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
                            const THighResolutionTimePoint &  start_timestamp)
{
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
                            const THighResolutionTimePoint &  start_timestamp)
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



