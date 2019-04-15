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
    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by seq_id/seq_id_type: " <<
                 m_BlobRequest.m_SeqId << "." << m_BlobRequest.m_SeqIdType <<
                 ", this: " << this);
    } else {
        PSG_TRACE("CPendingOperation::CPendingOperation: blob "
                 "requested by sat/sat_key: " <<
                 m_BlobRequest.m_BlobId.m_Sat << "." <<
                 m_BlobRequest.m_BlobId.m_SatKey <<
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


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    x_SetRequestContext();

    switch (m_RequestType) {
        case eBlobRequest:
            if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
                PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                          "requested by seq_id/seq_id_type: " <<
                          m_BlobRequest.m_SeqId << "." <<
                          m_BlobRequest.m_SeqIdType <<
                          ", this: " << this);
            } else {
                PSG_TRACE("CPendingOperation::~CPendingOperation: blob "
                          "requested by sat/sat_key: " <<
                          m_BlobRequest.m_BlobId.m_Sat << "." <<
                          m_BlobRequest.m_BlobId.m_SatKey <<
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
            if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
                PSG_TRACE("CPendingOperation::Clear: blob "
                          "requested by seq_id/seq_id_type: " <<
                          m_BlobRequest.m_SeqId << "." <<
                          m_BlobRequest.m_SeqIdType <<
                          ", this: " << this);
            } else {
                PSG_TRACE("CPendingOperation::Clear: blob "
                          "requested by sat/sat_key: " <<
                          m_BlobRequest.m_BlobId.m_Sat << "." <<
                          m_BlobRequest.m_BlobId.m_SatKey <<
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
        default:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Unhandeled request type " +
                       NStr::NumericToString(static_cast<int>(m_RequestType)));
    }
}


void CPendingOperation::x_OnBioseqError(CRequestStatus::ECode  status,
                                        const string &  err_msg)
{
    size_t      item_id = m_ProtocolSupport.GetItemId();

    if (status != CRequestStatus::e404_NotFound)
        UpdateOverallStatus(status);

    m_ProtocolSupport.PrepareBioseqMessage(item_id, err_msg, status,
                                          eNoBioseqInfo, eDiag_Error);
    m_ProtocolSupport.PrepareBioseqCompletion(item_id, 2);
    x_SendReplyCompletion(true);
    m_ProtocolSupport.Flush();
}


void CPendingOperation::x_OnReplyError(CRequestStatus::ECode  status,
                                       int  err_code, const string &  err_msg)
{
    m_ProtocolSupport.PrepareReplyMessage(err_msg, status, err_code, eDiag_Error);
    x_SendReplyCompletion(true);
    m_ProtocolSupport.Flush();
}


void CPendingOperation::x_ProcessGetRequest(void)
{
    // Get request could be one of the following:
    // - the blob was requested by a sat/sat_key pair
    // - the blob was requested by a seq_id/seq_id_type pair
    if (m_BlobRequest.GetBlobIdentificationType() == eBySeqId) {
        // By seq_id -> search for the bioseq key info in the cache only
        SResolveInputSeqIdError err;
        SBioseqResolution       bioseq_resolution = x_ResolveInputSeqId(err);

        if (err.HasError()) {
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId, err.m_ErrorMessage);
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
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId, err.m_ErrorMessage);
        } else {
            // Could not resolve, send 404
            x_OnBioseqError(CRequestStatus::e404_NotFound,
                            "Could not resolve seq_id " + m_BlobRequest.m_SeqId);
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
                m_AsyncInterruptPoint = eGetBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(bioseq_resolution.m_BioseqInfo.GetAccession(),
                                              bioseq_resolution.m_BioseqInfo.GetVersion(),
                                              bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                              m_PostponedSeqIdResolution,
                                              this));
                m_AsyncBioseqDetailsQuery->MakeRequest();
                return;
            }

            // default error message
            x_GetRequestBioseqInconsistency();
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


void CPendingOperation::x_CompleteGetRequest(SBioseqResolution &  bioseq_resolution)
{
    // Send BIOSEQ_INFO
    x_SendBioseqInfo("", bioseq_resolution.m_BioseqInfo, eJsonFormat);

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


void CPendingOperation::x_GetRequestBioseqInconsistency(void)
{
    x_GetRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_BlobRequest.m_SeqId + " but the bioseq info is not found");
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_GetRequestBioseqInconsistency(const string &  err_msg)
{
    x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg);
}


void CPendingOperation::x_OnResolveResolutionError(SResolveInputSeqIdError &  err)
{
    x_OnResolveResolutionError(err.m_ErrorCode, err.m_ErrorMessage);
}


void CPendingOperation::x_OnResolveResolutionError(CRequestStatus::ECode  status,
                                                   const string &  message)
{
    PSG_WARNING(message);
    if (x_UsePsgProtocol()) {
        x_OnReplyError(status, eUnresolvedSeqId, message);
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
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::x_ProcessResolveRequest(void)
{
    SResolveInputSeqIdError     err;
    SBioseqResolution           bioseq_resolution = x_ResolveInputSeqId(err);

    if (err.HasError()) {
        x_OnResolveResolutionError(err);
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
void CPendingOperation::x_ProcessResolveRequest(SResolveInputSeqIdError &  err,
                                                SBioseqResolution &  bioseq_resolution)
{
    if (!bioseq_resolution.IsValid()) {
        if (err.HasError()) {
            x_OnResolveResolutionError(err);
        } else {
            // Could not resolve, send 404
            string  err_msg = "Could not resolve seq_id " + m_ResolveRequest.m_SeqId;

            if (x_UsePsgProtocol()) {
                x_OnBioseqError(CRequestStatus::e404_NotFound, err_msg);
            } else {
                m_ProtocolSupport.Send404(err_msg.c_str());
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
                m_AsyncInterruptPoint = eResolveBioseqDetails;
                m_AsyncBioseqDetailsQuery.reset(
                        new CAsyncBioseqQuery(bioseq_resolution.m_BioseqInfo.GetAccession(),
                                              bioseq_resolution.m_BioseqInfo.GetVersion(),
                                              bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                              m_PostponedSeqIdResolution,
                                              this));
                m_AsyncBioseqDetailsQuery->MakeRequest();
                return;
            }

            // default error message
            x_ResolveRequestBioseqInconsistency();
            return;
        }
    } else if (bioseq_resolution.m_ResolutionResult == eFromBioseqDB) {
        // just in case
        bioseq_resolution.m_CacheInfo.clear();
    }

    x_CompleteResolveRequest(bioseq_resolution);
}


void CPendingOperation::x_ResolveRequestBioseqInconsistency(void)
{
    x_ResolveRequestBioseqInconsistency(
        "Data inconsistency: the bioseq key info was resolved for seq_id " +
        m_ResolveRequest.m_SeqId + " but the bioseq info is not found");
}


// If there is no bioseq info found then it is a server error 500
// A few cases here:
// - cache only; no bioseq info in cache
// - db only; no bioseq info in db
// - db and cache; no bioseq neither in cache nor in db
void CPendingOperation::x_ResolveRequestBioseqInconsistency(const string &  err_msg)
{
    if (x_UsePsgProtocol()) {
        x_OnBioseqError(CRequestStatus::e500_InternalServerError, err_msg);
    } else {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_ProtocolSupport.Send500(err_msg.c_str());
        x_PrintRequestStop(m_OverallStatus);
    }
}


// Could be called:
// - synchronously
// - asynchronously if additional request to bioseq info is required
void CPendingOperation::x_CompleteResolveRequest(SBioseqResolution &  bioseq_resolution)
{
    // Bioseq info is found, send it to the client
    x_SendBioseqInfo(bioseq_resolution.m_CacheInfo,
                     bioseq_resolution.m_BioseqInfo,
                     m_ResolveRequest.m_OutputFormat);
    if (x_UsePsgProtocol()) {
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
    } else {
        x_PrintRequestStop(m_OverallStatus);
    }
}


void CPendingOperation::OnBioseqDetailsRecord(void)
{
    switch (m_AsyncInterruptPoint) {
        case eResolveBioseqDetails:
            if (m_PostponedSeqIdResolution.IsValid())
                x_CompleteResolveRequest(m_PostponedSeqIdResolution);
            else
                // default error message
                x_ResolveRequestBioseqInconsistency();
            break;
        case eGetBioseqDetails:
            if (m_PostponedSeqIdResolution.IsValid())
                x_CompleteGetRequest(m_PostponedSeqIdResolution);
            else
                // default error message
                x_GetRequestBioseqInconsistency();
            break;
        default:
            ;
    }
}


void CPendingOperation::OnBioseqDetailsError(CRequestStatus::ECode  status, int  code,
                                             EDiagSev  severity, const string &  message)
{
    switch (m_AsyncInterruptPoint) {
        case eResolveBioseqDetails:
            x_ResolveRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for seq_id " +
                    m_ResolveRequest.m_SeqId + " but the bioseq info is not found "
                    "(database error: " + message + ")");
            break;
        case eGetBioseqDetails:
            x_GetRequestBioseqInconsistency(
                    "Data inconsistency: the bioseq key info was resolved for seq_id " +
                    m_BlobRequest.m_SeqId + " but the bioseq info is not found "
                    "(database error: " + message + ")");
            break;
        default:
            ;
    }
}


void CPendingOperation::x_ProcessAnnotRequest(void)
{
    SResolveInputSeqIdError     err;
    SBioseqResolution           bioseq_resolution = x_ResolveInputSeqId(err);

    if (err.HasError()) {
        PSG_WARNING(err.m_ErrorMessage);
        x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId, err.m_ErrorMessage);
        return;
    }

    // Async resolution may be required
    if (bioseq_resolution.m_ResolutionResult == ePostponedForDB) {
        m_AsyncInterruptPoint = eAnnotSeqIdResolution;
        return;
    }

    x_ProcessAnnotRequest(err, bioseq_resolution);
}


// Could be called by:
// - sync processing (resolved via cache)
// - async processing (resolved via DB)
void CPendingOperation::x_ProcessAnnotRequest(SResolveInputSeqIdError &  err,
                                              SBioseqResolution &  bioseq_resolution)
{
    if (!bioseq_resolution.IsValid()) {
        if (err.HasError()) {
            PSG_WARNING(err.m_ErrorMessage);
            x_OnReplyError(err.m_ErrorCode, eUnresolvedSeqId, err.m_ErrorMessage);
        } else {
            // Could not resolve, send 404
            x_OnReplyError(CRequestStatus::e404_NotFound, eNoBioseqInfo,
                           "Could not resolve seq_id " + m_AnnotRequest.m_SeqId);
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
        fetch_task->SetDataReadyCB(m_ProtocolSupport.GetReply()->GetDataReadyCB());

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
        x_SendReplyCompletion(true);
        m_ProtocolSupport.Flush();
        return;
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
                               "Blob properties are not found");
            else
                x_OnReplyError(CRequestStatus::e500_InternalServerError,
                               eBlobPropsNotFound,
                               "Blob properties are not found "
                               "due to a cache lookup error");
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

    load_task->SetDataReadyCB(m_ProtocolSupport.GetReply()->GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, fetch_details.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, fetch_details.get()));

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
        if (request_type == eBlobRequest) {
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


SBioseqResolution
CPendingOperation::x_ResolveInputSeqId(SResolveInputSeqIdError &  err)
{
    x_InitUrlIndentification();

    SBioseqResolution       bioseq_resolution;
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
        if (composed_ok)
            x_ResolveViaComposeOSLTInCache(oslt_seq_id, effective_seq_id_type,
                                           secondary_id_list, primary_id,
                                           err, bioseq_resolution);
        else
            x_ResolveAsIsInCache(bioseq_resolution, err);

        if (bioseq_resolution.IsValid())
            return bioseq_resolution;
    }

    if (m_UrlUseCache != eCacheOnly) {
        // Need to initiate async DB resolution

        // Memorize an error if there was one
        if (!parse_err_msg.empty() && !err.HasError()) {
            err.m_ErrorMessage = parse_err_msg;
            err.m_ErrorCode = CRequestStatus::e400_BadRequest;
        }

        m_PostponedSeqIdResolveError = err;
        err.Reset();

        bioseq_resolution.Reset();
        bioseq_resolution.m_ResolutionResult = ePostponedForDB;

        // Async request
        m_AsyncSeqIdResolver.reset(new CAsyncSeqIdResolver(
                                            oslt_seq_id, effective_seq_id_type,
                                            secondary_id_list, primary_id,
                                            m_UrlSeqId, composed_ok,
                                            m_PostponedSeqIdResolution, this));
        m_AsyncSeqIdResolver->Process();
        return bioseq_resolution;
    }

    if (!bioseq_resolution.IsValid()) {
        CPubseqGatewayApp::GetInstance()->GetRequestCounters().
                                                IncNotResolved();

        if (!parse_err_msg.empty()) {
            if (!err.HasError()) {
                err.m_ErrorMessage = parse_err_msg;
                err.m_ErrorCode = CRequestStatus::e400_BadRequest;
            }
        }
    }
    return bioseq_resolution;
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

    // Cannot resolve
    string      msg = string("Unknown satellite number ") +
                      NStr::NumericToString(blob_id.m_Sat);
    if (blob_request.GetBlobIdentificationType() == eBySeqId)
        msg += " for bioseq info with seq_id '" +
               blob_request.m_SeqId + "'";
    else
        msg += " for the blob " + GetBlobId(blob_id);

    // It is a server error - data inconsistency
    PSG_ERROR(msg);

    x_SendUnknownServerSatelliteError(m_ProtocolSupport.GetItemId(), msg,
                                      eUnknownResolvedSatellite);

    app->GetErrorCounters().IncServerSatToSatName();
    return false;
}


void CPendingOperation::x_SendUnknownServerSatelliteError(
                                size_t  item_id,
                                const string &  msg,
                                int  err_code)
{
    m_ProtocolSupport.PrepareBlobPropMessage(item_id, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             err_code, eDiag_Error);
    m_ProtocolSupport.PrepareBlobPropCompletion(item_id, 2);
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
                                                     json.Repr());
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
        // Found, send back as JSON
        CJsonNode   json = ConvertBlobPropToJson(blob);
        m_ProtocolSupport.PrepareBlobPropData(fetch_details, json.Repr());

        // Note: initially only blob_props are requested and at that moment the
        //       TSE option is 'known'. So the initial request should be
        //       finished, see m_FinishedRead = true
        //       In the further requests of the blob data regardless with blob
        //       props or not, the TSE option is set to unknown so the request
        //       will be finished at the moment when blob chunks are handled.
        switch (fetch_details->GetTSEOption()) {
            case eNoneTSE:
                fetch_details->SetReadFinished();
                // Nothing else to be sent;
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                x_SendReplyCompletion();
                break;
            case eSlimTSE:
                fetch_details->SetReadFinished();
                if (blob.GetId2Info().empty()) {
                    // Nothing else to be sent
                    m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                    x_SendReplyCompletion();
                } else {
                    // Request the split INFO blob only
                    x_RequestID2BlobChunks(fetch_details, blob, true);

                    // It is important to send completion after: there could be
                    // an error of converting/translating ID2 info
                    m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                }
                break;
            case eSmartTSE:
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
                break;
            case eWholeTSE:
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
                break;
            case eOrigTSE:
                fetch_details->SetReadFinished();
                // Request original blob chunks
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                x_RequestOriginalBlobChunks(fetch_details, blob);
                break;
            case eUnknownTSE:
                // Used when INFO blobs are asked; i.e. chunks have been
                // requeted as well, so only the prop completion message needs
                // to be sent.
                m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);
                break;
        }
    } else {
        // Not found, report 500 because it is data inconsistency
        // or 404 if it was requested via sat.sat_key
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                IncBlobPropsNotFoundError();

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

        m_ProtocolSupport.PrepareBlobPropCompletion(fetch_details);

        fetch_details->SetReadFinished();
        x_SendReplyCompletion();
    }

    x_PeekIfNeeded();
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


void CPendingOperation::x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                                    CBlobRecord const &  blob)
{
    // eUnknownTSE is safe here; no blob prop call will happen anyway
    // eUnknownUseCache is safe here; no further resolution required
    SBlobRequest    orig_blob_request(fetch_details->GetBlobId(),
                                      blob.GetModified(), eUnknownTSE,
                                      eUnknownUseCache);

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

    load_task->SetDataReadyCB(m_ProtocolSupport.GetReply()->GetDataReadyCB());
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
    SBlobId     info_blob_id(m_Id2InfoSat, m_Id2InfoInfo);    // mandatory

    if (!app->SatToSatName(m_Id2InfoSat, info_blob_id.m_SatName)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              NStr::NumericToString(m_Id2InfoSat) +
                              ") to a cassandra keyspace for the blob " +
                              GetBlobId(fetch_details->GetBlobId());
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
    SBlobRequest    info_blob_request(info_blob_id, INT64_MIN, eUnknownTSE,
                                      eUnknownUseCache);

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

    load_task->SetDataReadyCB(m_ProtocolSupport.GetReply()->GetDataReadyCB());
    load_task->SetErrorCB(CGetBlobErrorCallback(this, cass_blob_fetch.get()));
    load_task->SetPropsCallback(CBlobPropCallback(this, cass_blob_fetch.get()));
    load_task->SetChunkCallback(CBlobChunkCallback(this, cass_blob_fetch.get()));

    m_FetchDetails.push_back(std::move(cass_blob_fetch));

    // We may need to request ID2 chunks
    if (!info_blob_only) {
        // Sat name is the same
        x_RequestId2SplitBlobs(info_blob_id.m_SatName);
    }

    // initiate retrieval
    for (auto &  fetch_details: m_FetchDetails) {
        if (fetch_details)
            fetch_details->GetLoader()->Wait();
    }
}


void CPendingOperation::x_RequestId2SplitBlobs(const string &  sat_name)
{
    for (int  chunk_no = 1; chunk_no <= m_Id2InfoChunks; ++chunk_no) {
        SBlobId     chunks_blob_id(m_Id2InfoSat,
                                   m_Id2InfoInfo - m_Id2InfoChunks - 1 + chunk_no);
        chunks_blob_id.m_SatName = sat_name;

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        SBlobRequest    chunk_request(chunks_blob_id, INT64_MIN, eUnknownTSE,
                                      eUnknownUseCache);

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

        load_task->SetDataReadyCB(m_ProtocolSupport.GetReply()->GetDataReadyCB());
        load_task->SetErrorCB(CGetBlobErrorCallback(this, details.get()));
        load_task->SetPropsCallback(CBlobPropCallback(this, details.get()));
        load_task->SetChunkCallback(CBlobChunkCallback(this, details.get()));

        m_FetchDetails.push_back(std::move(details));
    }
}


bool CPendingOperation::x_ParseId2Info(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  blob)
{
    // id2_info: "sat.info[.chunks]"
    vector<string>          parts;
    NStr::Split(blob.GetId2Info(), ".", parts);

    if (parts.size() < 3) {
        // Error: send it in the context of the blob props
        x_OnBadId2Info(fetch_details,
                       "Error extracting id2 info for the blob " +
                       GetBlobId(fetch_details->GetBlobId()) +
                       ". Expected 'sat.info.nchunks', found " +
                       NStr::NumericToString(parts.size()) + " parts.");
        return false;
    }

    try {
        m_Id2InfoSat = NStr::StringToInt(parts[0]);
        m_Id2InfoInfo = NStr::StringToInt(parts[1]);
        m_Id2InfoChunks = NStr::StringToInt(parts[2]);
    } catch (...) {
        // Error: send it in the context of the blob props
        x_OnBadId2Info(fetch_details,
                       "Error converting id2 info into integers for the blob " +
                       GetBlobId(fetch_details->GetBlobId()));
        return false;
    }

    // Validate the values
    string      validate_message;
    if (m_Id2InfoSat <= 0)
        validate_message = "Invalid id2_info SAT value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoSat) + ".";
    if (m_Id2InfoInfo <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2_info INFO value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoInfo) + ".";
    }
    if (m_Id2InfoChunks <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2_info NCHUNKS value. Expected to be > 0. Received: " +
            NStr::NumericToString(m_Id2InfoChunks) + ".";
    }


    if (!validate_message.empty()) {
        // Error: send it in the context of the blob props
        validate_message += " Blob: " + GetBlobId(fetch_details->GetBlobId());
        x_OnBadId2Info(fetch_details, validate_message);
        return false;
    }

    return true;
}


void CPendingOperation::x_OnBadId2Info(CCassBlobFetch *  fetch_details,
                                       const string &  msg)
{
    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();

    m_ProtocolSupport.PrepareBlobPropMessage(fetch_details, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             eBadID2Info, eDiag_Error);
    app->GetErrorCounters().IncBioseqID2Info();
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);
}


void CPendingOperation::OnSeqIdAsyncResolutionFinished(void)
{
    if (!m_PostponedSeqIdResolution.IsValid())
        CPubseqGatewayApp::GetInstance()->GetRequestCounters().IncNotResolved();

    // The results are populated by CAsyncSeqIdResolver in
    // m_PostponedSeqIdResolution
    switch (m_AsyncInterruptPoint) {
        case eAnnotSeqIdResolution:
            x_ProcessAnnotRequest(m_PostponedSeqIdResolveError,
                                  m_PostponedSeqIdResolution);
            break;
        case eResolveSeqIdResolution:
            x_ProcessResolveRequest(m_PostponedSeqIdResolveError,
                                    m_PostponedSeqIdResolution);
            break;
        case eGetSeqIdResolution:
            x_ProcessGetRequest(m_PostponedSeqIdResolveError,
                                m_PostponedSeqIdResolution);
            break;
        default:
            ;
    }
}


void CPendingOperation::OnSeqIdAsyncError(CRequestStatus::ECode  status,
                                          int  code,
                                          EDiagSev  severity,
                                          const string &  message)
{
    switch (m_AsyncInterruptPoint) {
        case eAnnotSeqIdResolution:
            PSG_WARNING(message);
            x_OnReplyError(status, code, message);
            return;
        case eResolveSeqIdResolution:
            PSG_WARNING(message);
            x_OnResolveResolutionError(status, message);
            return;
        case eGetSeqIdResolution:
            PSG_WARNING(message);
            x_OnReplyError(status, code, message);
            return;
        default:
            ;
    }
}

