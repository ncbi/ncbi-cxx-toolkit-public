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
 * File Description: base class for processors which retrieve blobs
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway.hpp"
#include "cass_blob_base.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "public_comment_callback.hpp"

using namespace std::placeholders;


CPSGS_CassBlobBase::CPSGS_CassBlobBase() :
    m_LastModified(-1),
    m_CollectSplitInfo(false),
    m_SplitInfoGzipFlag(false)
{}


CPSGS_CassBlobBase::CPSGS_CassBlobBase(shared_ptr<CPSGS_Request>  request,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const string &  processor_id,
                                       TBlobPropsCB  blob_props_cb,
                                       TBlobChunkCB  blob_chunk_cb,
                                       TBlobErrorCB  blob_error_cb) :
    m_NeedToParseId2Info(true),
    m_ProcessorId(processor_id),
    m_LastModified(-1),
    m_CollectSplitInfo(false),
    m_SplitInfoGzipFlag(false),
    m_BlobPropsCB(blob_props_cb),
    m_BlobChunkCB(blob_chunk_cb),
    m_BlobErrorCB(blob_error_cb),
    m_NeedFallbackBlob(false),
    m_FallbackBlobRequested(false)
{
    // Detect if a fallback to the original blob is required if there were
    // problems
    SPSGS_BlobRequestBase::EPSGS_TSEOption      request_tse_option = SPSGS_BlobRequestBase::ePSGS_UnknownTSE;
    switch (request->GetRequestType()) {
        case CPSGS_Request::ePSGS_AnnotationRequest:
            request_tse_option = request->GetRequest<SPSGS_AnnotRequest>().m_TSEOption;
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            request_tse_option = request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_TSEOption;
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            request_tse_option = request->GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_TSEOption;
            break;
        default:
            // The m_RequestTSEOption is relevant only for the 3 requests above
            break;
    }

    if (request_tse_option == SPSGS_BlobRequestBase::ePSGS_SlimTSE ||
        request_tse_option == SPSGS_BlobRequestBase::ePSGS_SmartTSE ||
        request_tse_option == SPSGS_BlobRequestBase::ePSGS_WholeTSE) {
        m_NeedFallbackBlob = true;
    }
}


CPSGS_CassBlobBase::~CPSGS_CassBlobBase()
{}


void
CPSGS_CassBlobBase::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                  CBlobRecord const &  in_blob_prop,
                                  bool is_found)
{
    // Need a copy because the broken id2 info needs to be overwritten
    CBlobRecord     blob_prop = in_blob_prop;

    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Blob prop callback; found: " + to_string(is_found),
                           m_Request->GetStartTimestamp());
    }

    if (is_found) {
        // The method could be called multiple times. First time it is called
        // for the blob in the request (ID/getblob and ID/get). At this moment
        // the blob id2info field should be parsed and memorized.
        // Later the original blob id2info is used to decide if id2_chunk
        // and id2_info should be present in the reply.

        if (m_LastModified == -1)
            m_LastModified = blob_prop.GetModified();

        // If the split info chunks need to be collected then the zip flag
        // needs to be memorized for the further deserialization when all the
        // chunks have been received
        if (m_CollectSplitInfo) {
            if (fetch_details->GetBlobId() == m_InfoBlobId) {
                m_SplitInfoGzipFlag = blob_prop.GetFlag(EBlobFlags::eGzip);
            }
        }

        unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>
                    parsed_id2_info = x_CheckId2Info(fetch_details, blob_prop);
        x_PrepareBlobPropData(fetch_details, blob_prop);

        // Blob may be withdrawn or confidential
        bool    is_authorized = x_IsAuthorized(ePSGS_RetrieveBlobData,
                                               fetch_details->GetBlobId(),
                                               blob_prop, "");
        if (!is_authorized) {
            x_PrepareBlobPropCompletion(fetch_details);

            // Need to send 403 - forbidden
            x_PrepareBlobMessage(fetch_details, "Blob retrieval is not authorized",
                                 CRequestStatus::e403_Forbidden,
                                 ePSGS_BlobRetrievalIsNotAuthorized,
                                 eDiag_Error);

            fetch_details->GetLoader()->ClearError();
            fetch_details->SetReadFinished();
            return;
        }

        if (m_NeedToParseId2Info) {
            m_NeedToParseId2Info = false;

            // Note: here the id2_info field is taken from the original
            // incoming blob pros. This is because during checking the id2_info
            // field above (x_CheckId2Info()) it could be reset if it is broken
            if (!in_blob_prop.GetId2Info().empty()) {
                m_Id2Info.reset(parsed_id2_info.release());

                if (m_Id2Info.get() == nullptr) {
                    if (m_NeedFallbackBlob) {
                        m_NeedFallbackBlob = false;

                        string  err_msg = "Falling back to retrieve "
                                          "the original blob due to broken id2 info.";

                        m_Reply->PrepareProcessorMessage(
                            m_Reply->GetItemId(), m_ProcessorId, err_msg,
                            CRequestStatus::e500_InternalServerError,
                            ePSGS_BadID2Info, eDiag_Warning);

                        PSG_ERROR(err_msg);

                        x_OnBlobPropOrigTSE(fetch_details, blob_prop);
                        return;
                    }

                    // Here: basically it is a case of tse=orig or tse=none
                    // In both cases we just continue.
                }
            }
        }

        if (fetch_details->GetTSEOption() != SPSGS_BlobRequestBase::ePSGS_UnknownTSE) {
            // Memorize the fetch details and blob props for the case
            // if a fallback to the original blob is requested. Only the
            // initial blob props need to be saved and this is when a TSE
            // option is known.
            m_InitialBlobPropFetch = fetch_details;
            m_InitialBlobProps = blob_prop;
        }

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
                x_OnBlobPropSlimTSE(fetch_details, blob_prop);
                break;
            case SPSGS_BlobRequestBase::ePSGS_SmartTSE:
                x_OnBlobPropSmartTSE(fetch_details, blob_prop);
                break;
            case SPSGS_BlobRequestBase::ePSGS_WholeTSE:
                x_OnBlobPropWholeTSE(fetch_details, blob_prop);
                break;
            case SPSGS_BlobRequestBase::ePSGS_OrigTSE:
                x_OnBlobPropOrigTSE(fetch_details, blob_prop);
                break;
            case SPSGS_BlobRequestBase::ePSGS_UnknownTSE:
                // Used when INFO blobs are asked; i.e. chunks have been
                // requested as well, so only the prop completion message needs
                // to be sent.
                x_PrepareBlobPropCompletion(fetch_details);
                break;
        }
    } else {
        x_OnBlobPropNotFound(fetch_details);
    }
}


void
CPSGS_CassBlobBase::x_OnBlobPropNoneTSE(CCassBlobFetch *  fetch_details)
{
    // Nothing else to be sent
    x_PrepareBlobPropCompletion(fetch_details);
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
}


void
CPSGS_CassBlobBase::x_OnBlobPropSlimTSE(CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob)
{
    auto        fetch_blob = fetch_details->GetBlobId();

    // The handler deals with all kind of blob requests:
    // - by sat/sat_key
    // - by seq_id/seq_id_type
    // - by sat/sat_key after named annotation resolution
    // So get the reference to the blob base request
    auto &          blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();
    auto *          app = CPubseqGatewayApp::GetInstance();

    unsigned int    max_to_send = max(app->Settings().m_SendBlobIfSmall,
                                      blob_request.m_SendBlobIfSmall);

    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        x_PrepareBlobPropCompletion(fetch_details);

        // An original blob may be required if its size is small
        if (blob.GetSize() <= max_to_send) {
            // The blob is small so get it
            x_RequestOriginalBlobChunks(fetch_details, blob);
        } else {
            // Nothing else to be sent, the original blob is big
        }
        return;
    }

    // Not in the cache
    if (blob.GetSize() <= max_to_send) {
        // Request the split INFO blob and all split chunks
        x_RequestID2BlobChunks(fetch_details, blob, false);
    } else {
        // Request the split INFO blob only
        x_RequestID2BlobChunks(fetch_details, blob, true);
    }

    // It is important to send completion after: there could be
    // an error of converting/translating ID2 info
    x_PrepareBlobPropCompletion(fetch_details);
}


void
CPSGS_CassBlobBase::x_OnBlobPropSmartTSE(CCassBlobFetch *  fetch_details,
                                         CBlobRecord const &  blob)
{
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        x_PrepareBlobPropCompletion(fetch_details);

        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        auto &          blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();
        auto *          app = CPubseqGatewayApp::GetInstance();

        unsigned int    max_to_send = max(app->Settings().m_SendBlobIfSmall,
                                          blob_request.m_SendBlobIfSmall);

        if (blob.GetSize() <= max_to_send) {
            // Request the split INFO blob and all split chunks
            x_RequestID2BlobChunks(fetch_details, blob, false);
        } else {
            // Request the split INFO blob only
            x_RequestID2BlobChunks(fetch_details, blob, true);
        }

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        x_PrepareBlobPropCompletion(fetch_details);
    }
}


void
CPSGS_CassBlobBase::x_OnBlobPropWholeTSE(CCassBlobFetch *  fetch_details,
                                         CBlobRecord const &  blob)
{
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        x_PrepareBlobPropCompletion(fetch_details);
        x_RequestOriginalBlobChunks(fetch_details, blob);
    } else {
        // Request the split INFO blob and all split chunks
        x_RequestID2BlobChunks(fetch_details, blob, false);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        x_PrepareBlobPropCompletion(fetch_details);
    }
}


void
CPSGS_CassBlobBase::x_OnBlobPropOrigTSE(CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob)
{
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
    // Request original blob chunks
    x_PrepareBlobPropCompletion(fetch_details);
    x_RequestOriginalBlobChunks(fetch_details, blob);
}


void
CPSGS_CassBlobBase::x_RequestOriginalBlobChunks(CCassBlobFetch *  fetch_details,
                                                CBlobRecord const &  blob)
{
    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    auto                            cass_blob_id = fetch_details->GetBlobId();

    shared_ptr<CCassConnection>     cass_connection;
    try {
        if (cass_blob_id.m_IsSecureKeyspace.value()) {
            cass_connection = cass_blob_id.m_Keyspace->GetSecureConnection(
                                                m_UserName.value());
            if (!cass_connection) {
                ReportSecureSatUnauthorized(m_UserName.value());
                CPSGS_CassProcessorBase::SignalFinishProcessing();
                return;
            }
        } else {
            cass_connection = cass_blob_id.m_Keyspace->GetConnection();
        }
    } catch (const exception &  exc) {
        ReportFailureToGetCassConnection(exc.what());
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    } catch (...) {
        ReportFailureToGetCassConnection();
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }


    // eUnknownTSE is safe here; no blob prop call will happen anyway
    // eUnknownUseCache is safe here; no further resolution required
    SPSGS_BlobBySatSatKeyRequest
            orig_blob_request(SPSGS_BlobId(cass_blob_id.ToString()),
                              blob.GetModified(),
                              SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                              SPSGS_RequestBase::ePSGS_UnknownUseCache,
                              fetch_details->GetClientId(),
                              0, 0, m_Request->GetIncludeHUP(), trace_flag,
                              m_Request->NeedProcessorEvents(),
                              vector<string>(), vector<string>(),
                              psg_clock_t::now());

    // Create the cass async loader
    unique_ptr<CBlobRecord>             blob_record(new CBlobRecord(blob));
    CCassBlobTaskLoadBlob *             load_task =
        new CCassBlobTaskLoadBlob(cass_connection,
                                  cass_blob_id.m_Keyspace->keyspace,
                                  std::move(blob_record),
                                  true, nullptr);

    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(orig_blob_request, cass_blob_id));
    cass_blob_fetch->SetLoader(load_task);

    // Blob props have already been received
    cass_blob_fetch->SetBlobPropSent();

    if (x_CheckExcludeBlobCache(cass_blob_fetch.get(),
                                false) == ePSGS_SkipRetrieving)
        return;

    load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(this, m_BlobErrorCB, cass_blob_fetch.get()));
    load_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    load_task->SetPropsCallback(nullptr);
    load_task->SetChunkCallback(
        CBlobChunkCallback(this, m_BlobChunkCB, cass_blob_fetch.get()));

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(
            "Cassandra request: " + ToJsonString(*load_task),
            m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(cass_blob_fetch));

    load_task->Wait();
}


void
CPSGS_CassBlobBase::x_RequestID2BlobChunks(CCassBlobFetch *  fetch_details,
                                           CBlobRecord const &  blob,
                                           bool  info_blob_only)
{
    auto *      app = CPubseqGatewayApp::GetInstance();

    // Translate sat to keyspace
    SCass_BlobId    info_blob_id(m_Id2Info->GetSat(), m_Id2Info->GetInfo());    // mandatory

    if (!info_blob_id.MapSatToKeyspace()) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              to_string(m_Id2Info->GetSat()) +
                              ") to a cassandra keyspace for the blob " +
                              fetch_details->GetBlobId().ToString();
        x_PrepareBlobPropMessage(fetch_details, message,
                                 CRequestStatus::e502_BadGateway,
                                 ePSGS_BadID2Info, eDiag_Error);
        app->GetCounters().Increment(this,
                                     CPSGSCounters::ePSGS_ServerSatToSatNameError);
        UpdateOverallStatus(CRequestStatus::e502_BadGateway);
        PSG_ERROR(message);
        return;
    }

    // Note: the blobs from a public keyspace may only point to the blobs in a
    // public keyspace. A similar rule is about a secure keyspace: HUP blobs
    // point only to HUP blobs. Thus if a keyspace is secure then the MyNCBI
    // resultion has already happened and can be used here.

    shared_ptr<CCassConnection>     cass_connection;

    try {
        if (info_blob_id.m_IsSecureKeyspace.value()) {
            cass_connection = info_blob_id.m_Keyspace->GetSecureConnection(
                                                m_UserName.value());
            if (!cass_connection) {
                x_PrepareBlobPropMessage(fetch_details,
                                         "id2 info sat connection unauthorized "
                                         "for the blob " + fetch_details->GetBlobId().ToString(),
                                         CRequestStatus::e401_Unauthorized,
                                         ePSGS_SecureSatUnauthorized, eDiag_Error);
                ReportSecureSatUnauthorized(m_UserName.value());
                return;
            }
        } else {
            cass_connection = info_blob_id.m_Keyspace->GetConnection();
        }
    } catch (const exception &  exc) {
        x_PrepareBlobPropMessage(fetch_details,
                                 "id2 info sat connection authorization error: " +
                                 string(exc.what()),
                                 CRequestStatus::e500_InternalServerError,
                                 ePSGS_CassConnectionError, eDiag_Error);
        ReportFailureToGetCassConnection(exc.what());
        return;
    } catch (...) {
        x_PrepareBlobPropMessage(fetch_details,
                                 "id2 info sat connection authorization unknown error",
                                 CRequestStatus::e500_InternalServerError,
                                 ePSGS_CassConnectionError, eDiag_Error);
        ReportFailureToGetCassConnection();
        return;
    }

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    // Create the Id2Info requests.
    // eUnknownTSE is treated in the blob prop handler as to do nothing (no
    // sending completion message, no requesting other blobs)
    // eUnknownUseCache is safe here; no further resolution
    // empty client_id means that later on there will be no exclude blobs cache ops
    string      info_blob_id_as_str = info_blob_id.ToString();
    SPSGS_BlobBySatSatKeyRequest
        info_blob_request(SPSGS_BlobId(info_blob_id_as_str),
                          INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          fetch_details->GetClientId(), 0, 0,
                          m_Request->GetIncludeHUP(), trace_flag,
                          m_Request->NeedProcessorEvents(),
                          vector<string>(), vector<string>(),
                          psg_clock_t::now());

    // Prepare Id2Info retrieval
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(info_blob_request, info_blob_id));
    bool                        info_blob_requested = false;

    if (x_CheckExcludeBlobCache(cass_blob_fetch.get(),
                                true) == ePSGS_ProceedRetrieving) {
        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_Request, m_Reply);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            this,
                                            info_blob_id.m_Sat,
                                            info_blob_id.m_SatKey,
                                            info_blob_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;


        if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            info_blob_id.m_Keyspace->keyspace,
                            std::move(blob_record),
                            true, nullptr);
        } else {
            // The handler deals with both kind of blob requests:
            // - by sat/sat_key
            // - by seq_id/seq_id_type
            // So get the reference to the blob base request
            auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

            if (blob_request.m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
                // No need to continue; it is forbidded to look for blob props in
                // the Cassandra DB
                string      message;

                if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit) {
                    message = "Blob properties are not found";
                    UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    x_PrepareBlobPropMessage(fetch_details, message,
                                             CRequestStatus::e404_NotFound,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                } else {
                    message = "Blob properties are not found due to LMDB cache error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    x_PrepareBlobPropMessage(fetch_details, message,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                }

                PSG_WARNING(message);
                return;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            info_blob_id.m_Keyspace->keyspace,
                            info_blob_id.m_SatKey,
                            true, nullptr);
        }
        cass_blob_fetch->SetLoader(load_task);

        load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
        load_task->SetErrorCB(
            CGetBlobErrorCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobErrorCallback,
                     this, _1, _2, _3, _4, _5),
                cass_blob_fetch.get()));
        load_task->SetLoggingCB(
                bind(&CPSGS_CassProcessorBase::LoggingCallback,
                     this, _1, _2));
        load_task->SetPropsCallback(
            CBlobPropCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobPropsCallback,
                     this, _1, _2, _3),
                m_Request, m_Reply, cass_blob_fetch.get(),
                blob_prop_cache_lookup_result != ePSGS_CacheHit));
        load_task->SetChunkCallback(
            CBlobChunkCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobChunkCallback,
                     this, _1, _2, _3, _4, _5),
                cass_blob_fetch.get()));

        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Cassandra request: " +
                               ToJsonString(*load_task),
                               m_Request->GetStartTimestamp());
        }

        m_RequestedID2BlobChunks.push_back(info_blob_id_as_str);
        info_blob_requested = true;
        cass_blob_fetch->SetNeedAddId2ChunkId2Info(true);
        m_FetchDetails.push_back(std::move(cass_blob_fetch));
    }

    auto    to_init_iter = m_FetchDetails.end();
    --to_init_iter;

    // We may need to request ID2 chunks
    if (!info_blob_only) {
        // Sat name is the same
        x_RequestId2SplitBlobs(fetch_details);
    } else {
        // This is the case when only split INFO has been requested.
        // So may be it is necessary to request some more chunks
        x_DecideToRequestMoreChunksForSmartTSE(fetch_details, info_blob_id);
    }

    // initiate retrieval: only those which were just created
    if (!info_blob_requested) {
        // If the info blob was not retrieved then the to_init_iter points to
        // the previous fetch for which Wait() has been invoked before
        ++to_init_iter;
    }

    while (to_init_iter != m_FetchDetails.end()) {
        if (*to_init_iter)
            (*to_init_iter)->GetLoader()->Wait();
        ++to_init_iter;
    }
}


void
CPSGS_CassBlobBase::x_RequestId2SplitBlobs(CCassBlobFetch *  fetch_details)
{
    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    for (int  chunk_no = 1; chunk_no <= m_Id2Info->GetChunks(); ++chunk_no) {
        SCass_BlobId    chunks_blob_id(m_Id2Info->GetSat(),
                                       m_Id2Info->GetInfo() - m_Id2Info->GetChunks() - 1 + chunk_no);
        // Note: the mapping must be good becuse the same mapping was done
        // before
        chunks_blob_id.MapSatToKeyspace();

        // Note: the blobs from a public keyspace may only point to the blobs in a
        // public keyspace. A similar rule is about a secure keyspace: HUP blobs
        // point only to HUP blobs. Thus if a keyspace is secure then the MyNCBI
        // resultion has already happened and can be used here.
        shared_ptr<CCassConnection>     cass_connection;

        try {
            if (chunks_blob_id.m_IsSecureKeyspace.value()) {
                cass_connection = chunks_blob_id.m_Keyspace->GetSecureConnection(
                                                    m_UserName.value());
                if (!cass_connection) {
                    x_PrepareBlobPropMessage(fetch_details,
                                             "id2 split chunk sat connection unauthorized "
                                             "for the blob " + fetch_details->GetBlobId().ToString(),
                                             CRequestStatus::e401_Unauthorized,
                                             ePSGS_SecureSatUnauthorized, eDiag_Error);
                    ReportSecureSatUnauthorized(m_UserName.value());
                    return;
                }
            } else {
                cass_connection = chunks_blob_id.m_Keyspace->GetConnection();
            }
        } catch (const exception &  exc) {
            x_PrepareBlobPropMessage(fetch_details,
                                     "id2 split chunk sat connection authorization error: " +
                                     string(exc.what()),
                                     CRequestStatus::e500_InternalServerError,
                                     ePSGS_CassConnectionError, eDiag_Error);
            ReportFailureToGetCassConnection(exc.what());
            return;
        } catch (...) {
            x_PrepareBlobPropMessage(fetch_details,
                                     "id2 split chunk sat connection authorization unknown error",
                                     CRequestStatus::e500_InternalServerError,
                                     ePSGS_CassConnectionError, eDiag_Error);
            ReportFailureToGetCassConnection();
            return;
        }

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        string          chunks_blob_id_as_str = chunks_blob_id.ToString();
        SPSGS_BlobBySatSatKeyRequest
            chunk_request(SPSGS_BlobId(chunks_blob_id_as_str),
                          INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          fetch_details->GetClientId(), 0, 0,
                          m_Request->GetIncludeHUP(),
                          trace_flag,
                          m_Request->NeedProcessorEvents(),
                          vector<string>(), vector<string>(),
                          psg_clock_t::now());

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request, chunks_blob_id));

        // Check the already sent cache
        if (x_CheckExcludeBlobCache(details.get(),
                                    true) == ePSGS_SkipRetrieving) {
            continue;
        }


        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_Request, m_Reply);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            this,
                                            chunks_blob_id.m_Sat,
                                            chunks_blob_id.m_SatKey,
                                            chunk_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            chunks_blob_id.m_Keyspace->keyspace,
                            std::move(blob_record),
                            true, nullptr);
            details->SetLoader(load_task);
        } else {
            // The handler deals with both kind of blob requests:
            // - by sat/sat_key
            // - by seq_id/seq_id_type
            // So get the reference to the blob base request
            auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

            if (blob_request.m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
                // No need to create a request because the Cassandra DB access
                // is forbidden
                string      message;
                if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit) {
                    message = "Blob properties are not found";
                    UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    x_PrepareBlobPropMessage(details.get(), message,
                                             CRequestStatus::e404_NotFound,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    x_PrepareBlobPropMessage(details.get(), message,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                }
                PSG_WARNING(message);
                continue;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            chunks_blob_id.m_Keyspace->keyspace,
                            chunks_blob_id.m_SatKey,
                            true, nullptr);
            details->SetLoader(load_task);
        }

        load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
        load_task->SetErrorCB(
            CGetBlobErrorCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobErrorCallback,
                    this, _1, _2, _3, _4, _5),
                details.get()));
        load_task->SetLoggingCB(
                bind(&CPSGS_CassProcessorBase::LoggingCallback,
                     this, _1, _2));
        load_task->SetPropsCallback(
            CBlobPropCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobPropsCallback,
                     this, _1, _2, _3),
                m_Request, m_Reply, details.get(),
                blob_prop_cache_lookup_result != ePSGS_CacheHit));
        load_task->SetChunkCallback(
            CBlobChunkCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobChunkCallback,
                     this, _1, _2, _3, _4, _5),
                details.get()));

        m_RequestedID2BlobChunks.push_back(chunks_blob_id_as_str);
        details->SetNeedAddId2ChunkId2Info(true);
        m_FetchDetails.push_back(std::move(details));
    }
}


void
CPSGS_CassBlobBase::x_DecideToRequestMoreChunksForSmartTSE(
                                            CCassBlobFetch *  fetch_details,
                                            SCass_BlobId const &  info_blob_id)
{
    // More chunks should be requested for a specific case:
    // - ID/get request
    // - the 'tse; request option is set to 'smart'
    // - blob is splitted
    // - blob has a large size so the only split INFO has been requested

    // The last two criterias have been checked before

    auto        request_type = m_Request->GetRequestType();
    if (request_type != CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Decision: split info of " + info_blob_id.ToString() +
                               " will not be collected for further analysis "
                               "because it is not the ID/get request",
                               m_Request->GetStartTimestamp());
        }
        return;
    }

    auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();
    if (blob_request.m_TSEOption != SPSGS_BlobRequestBase::ePSGS_SmartTSE) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Decision: split info of " + info_blob_id.ToString() +
                               " will not be collected for further analysis "
                               "because the ID/get request is not "
                               "with 'tse' option set to 'smart'",
                               m_Request->GetStartTimestamp());
        }
        return;
    }

    if (m_ResolvedSeqID.Which() == CSeq_id_Base::e_not_set) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Decision: split info of " + info_blob_id.ToString() +
                               " will not be collected for further analysis "
                               "because of the failure to construct CSeq_id from the "
                               "resolved input seq_id (see an applog warning)",
                               m_Request->GetStartTimestamp());
        }
        return;
    }

    // Here: the split INFO chunks must be fed to function which may tell that
    // some more chunks need to be sent
    m_InfoBlobId = info_blob_id;

    // Check the cached info first
    auto *  app = CPubseqGatewayApp::GetInstance();
    auto    cache_search_result = app->GetSplitInfoCache()->GetBlob(info_blob_id);
    if (cache_search_result.has_value()) {
        // The chunks are in cache
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Extra split info blob for " + info_blob_id.ToString() +
                               " is already in cache. Using the blob to request extra chunks",
                               m_Request->GetStartTimestamp());
        }
        vector<int>     extra_chunks;
        try {
            extra_chunks = psg::GetBioseqChunks(m_ResolvedSeqID,
                                                *cache_search_result.value());
        } catch (const exception &  exc) {
            PSG_ERROR("Error getting bioseq chunks from split info: " +
                      string(exc.what()));
            return;
        } catch (...) {
            PSG_ERROR("Unknown error of getting bioseq chunks from split info");
            return;
        }
        x_RequestMoreChunksForSmartTSE(fetch_details, extra_chunks, false);
        return;
    }

    // Not in cache; collect the chunks
    m_CollectSplitInfo = true;

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Decision: split info of " + info_blob_id.ToString() +
                           " will be collected for further analysis",
                           m_Request->GetStartTimestamp());
    }
}


void CPSGS_CassBlobBase::x_DeserializeSplitInfo(CCassBlobFetch *  fetch_details)
{
    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Deserializing collected split info from the buffer "
                           "for further analysis",
                           m_Request->GetStartTimestamp());
    }

    // The Gzip flag has been memorized when the split info blob props were
    // received
    m_CollectedSplitInfo.SetGzip(m_SplitInfoGzipFlag);

    // Add to cache
    auto *  app = CPubseqGatewayApp::GetInstance();
    CRef<CID2S_Split_Info>  blob;

    try {
        blob = m_CollectedSplitInfo.DeserializeSplitInfo();
    } catch (const exception &  exc) {
        PSG_ERROR("Error deserializing split info: " + string(exc.what()));
        return;
    } catch (...) {
        PSG_ERROR("Unknown error of deserializing split info");
        return;
    }

    app->GetSplitInfoCache()->AddBlob(m_InfoBlobId, blob);

    // Calculate the extra chunks
    vector<int>     extra_chunks;
    try {
       extra_chunks = psg::GetBioseqChunks(m_ResolvedSeqID, *blob);
    } catch (const exception &  exc) {
        PSG_ERROR("Error getting bioseq chunks from split info: " +
                  string(exc.what()));
        return;
    } catch (...) {
        PSG_ERROR("Unknown error of getting bioseq chunks from split info");
        return;
    }

    x_RequestMoreChunksForSmartTSE(fetch_details, extra_chunks, true);
}


void CPSGS_CassBlobBase::x_RequestMoreChunksForSmartTSE(CCassBlobFetch *  fetch_details,
                                                        const vector<int> &  extra_chunks,
                                                        bool  need_wait)
{
    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    for (auto chunk_no : extra_chunks) {
        SCass_BlobId    chunks_blob_id(m_Id2Info->GetSat(),
                                       m_Id2Info->GetInfo() - m_Id2Info->GetChunks() - 1 + chunk_no);
        chunks_blob_id.m_Keyspace = m_InfoBlobId.m_Keyspace;
        chunks_blob_id.m_IsSecureKeyspace = m_InfoBlobId.m_IsSecureKeyspace;
        string          chunks_blob_id_as_str = chunks_blob_id.ToString();

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        // client_id is "" (empty string) so the split blobs do not participate
        // in the exclude blob cache
        SPSGS_BlobBySatSatKeyRequest
            chunk_request(SPSGS_BlobId(chunks_blob_id_as_str),
                          INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          fetch_details->GetClientId(), 0, 0,
                          m_Request->GetIncludeHUP(),
                          trace_flag,
                          m_Request->NeedProcessorEvents(),
                          vector<string>(), vector<string>(),
                          psg_clock_t::now());

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request, chunks_blob_id));

        // Check the already sent cache
        if (x_CheckExcludeBlobCache(details.get(),
                                    true) == ePSGS_SkipRetrieving) {
            continue;
        }

        // Note: the blobs from a public keyspace may only point to the blobs in a
        // public keyspace. A similar rule is about a secure keyspace: HUP blobs
        // point only to HUP blobs. Thus if a keyspace is secure then the MyNCBI
        // resultion has already happened and can be used here.
        shared_ptr<CCassConnection>     cass_connection;

        try {
            if (chunks_blob_id.m_IsSecureKeyspace.value()) {
                cass_connection = chunks_blob_id.m_Keyspace->GetSecureConnection(
                                                    m_UserName.value());
                if (!cass_connection) {
                    x_PrepareBlobPropMessage(fetch_details,
                                             "id2 split chunk sat connection unauthorized "
                                             "for the blob " + fetch_details->GetBlobId().ToString(),
                                             CRequestStatus::e401_Unauthorized,
                                             ePSGS_SecureSatUnauthorized, eDiag_Error);
                    ReportSecureSatUnauthorized(m_UserName.value());
                    return;
                }
            } else {
                cass_connection = chunks_blob_id.m_Keyspace->GetConnection();
            }
        } catch (const exception &  exc) {
            x_PrepareBlobPropMessage(fetch_details,
                                     "id2 split chunk sat connection authorization error: " +
                                     string(exc.what()),
                                     CRequestStatus::e500_InternalServerError,
                                     ePSGS_CassConnectionError, eDiag_Error);
            ReportFailureToGetCassConnection(exc.what());
            return;
        } catch (...) {
            x_PrepareBlobPropMessage(fetch_details,
                                     "id2 split chunk sat connection authorization unknown error",
                                     CRequestStatus::e500_InternalServerError,
                                     ePSGS_CassConnectionError, eDiag_Error);
            ReportFailureToGetCassConnection();
            return;
        }

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_Request, m_Reply);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            this,
                                            chunks_blob_id.m_Sat,
                                            chunks_blob_id.m_SatKey,
                                            chunk_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            chunks_blob_id.m_Keyspace->keyspace,
                            std::move(blob_record),
                            true, nullptr);
            details->SetLoader(load_task);
        } else {
            // The handler is only for the ID/get i.e. blob by seq_id/seq_id_type
            // Get the reference to the blob base request
            auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

            if (blob_request.m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
                // No need to create a request because the Cassandra DB access
                // is forbidden
                string      message;
                if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit) {
                    message = "Blob properties are not found";
                    UpdateOverallStatus(CRequestStatus::e404_NotFound);
                    x_PrepareBlobPropMessage(details.get(), message,
                                             CRequestStatus::e404_NotFound,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    x_PrepareBlobPropMessage(details.get(), message,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_NoBlobPropsError, eDiag_Error);
                }
                PSG_WARNING(message);
                continue;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            cass_connection,
                            chunks_blob_id.m_Keyspace->keyspace,
                            chunks_blob_id.m_SatKey,
                            true, nullptr);
            details->SetLoader(load_task);
        }

        load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
        load_task->SetErrorCB(
            CGetBlobErrorCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobErrorCallback,
                     this, _1, _2, _3, _4, _5),
                details.get()));
        load_task->SetLoggingCB(
                bind(&CPSGS_CassProcessorBase::LoggingCallback,
                     this, _1, _2));
        load_task->SetPropsCallback(
            CBlobPropCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobPropsCallback,
                     this, _1, _2, _3),
                m_Request, m_Reply, details.get(),
                blob_prop_cache_lookup_result != ePSGS_CacheHit));
        load_task->SetChunkCallback(
            CBlobChunkCallback(this,
                bind(&CPSGS_CassBlobBase::x_BlobChunkCallback,
                     this, _1, _2, _3, _4, _5),
                details.get()));

        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Requesting extra chunk from INFO for the 'smart' tse option: " +
                               chunks_blob_id.ToString(),
                               m_Request->GetStartTimestamp());
        }

        m_RequestedID2BlobChunks.push_back(chunks_blob_id_as_str);
        details->SetNeedAddId2ChunkId2Info(true);
        m_FetchDetails.push_back(std::move(details));

        if (need_wait) {
            // Needed only when the method is called from the last chunk
            // receive context
            load_task->Wait();
        }
    }
}


CPSGS_CassBlobBase::EPSGS_BlobCacheCheckResult
CPSGS_CassBlobBase::x_CheckExcludeBlobCache(CCassBlobFetch *  fetch_details,
                                            bool  need_add_id2_chunk_id2_info)
{
    if (!fetch_details->IsBlobFetch())
        return ePSGS_ProceedRetrieving;
    if (fetch_details->GetClientId().empty())
        return ePSGS_ProceedRetrieving;

    bool                completed = true;
    psg_time_point_t    completed_time;
    auto                cache_result = fetch_details->AddToExcludeBlobCache(
                                            completed, completed_time);

    auto    request_type = m_Request->GetRequestType();
    if (request_type != CPSGS_Request::ePSGS_AnnotationRequest &&
        request_type != CPSGS_Request::ePSGS_BlobBySeqIdRequest) {
        // Only ID/get and ID/get_na may need to skip a blob
        return ePSGS_ProceedRetrieving;
    }

    if (cache_result == ePSGS_Added)
        return ePSGS_ProceedRetrieving;

    unsigned long       resend_timeout_mks;
    if (request_type == CPSGS_Request::ePSGS_AnnotationRequest) {
        auto &  annot_request = m_Request->GetRequest<SPSGS_AnnotRequest>();
        resend_timeout_mks = annot_request.m_ResendTimeoutMks;
    } else {
        // This is CPSGS_Request::ePSGS_BlobBySeqIdRequest request
        auto &  blob_request = m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>();
        resend_timeout_mks = blob_request.m_ResendTimeoutMks;
    }

    if (resend_timeout_mks == 0) {
        // Essentially it is the same as auto blob skipping is set to off
        return ePSGS_ProceedRetrieving;
    }

    // In case the blob is in process of sending the reply is the same for
    // ID/get and ID/get_na requests
    if (!completed) {
        x_PrepareBlobExcluded(fetch_details, ePSGS_BlobInProgress,
                              need_add_id2_chunk_id2_info);
        return ePSGS_SkipRetrieving;
    }

    // Here: the blob is in case and has already been sent so the
    // resend_timeout needs to be respected when a decision send it or not is
    // made
    unsigned long       sent_mks_ago = GetTimespanToNowMks(completed_time);
    if (sent_mks_ago < resend_timeout_mks) {
        // No sending the blob; it was sent recent enough
        x_PrepareBlobExcluded(fetch_details, sent_mks_ago,
                              resend_timeout_mks - sent_mks_ago,
                              need_add_id2_chunk_id2_info);
        return ePSGS_SkipRetrieving;
    }

    // Sending the blob anyway; it was longer than the resend
    // timeout or resend_timeout is 0.
    // Also need to do two more things:
    // - mark the blob in cache as in-progress again
    // - make a note in the fetch details that it needs to update
    //   the cache as completed once blob is finished
    auto *      app = CPubseqGatewayApp::GetInstance();

    // 'false' means not-completed, i.e. in-progress
    app->GetExcludeBlobCache()->SetCompleted(
                                    fetch_details->GetClientId(),
                                    SExcludeBlobId(fetch_details->GetBlobId().m_Sat,
                                        fetch_details->GetBlobId().m_SatKey),
                                    false);
    fetch_details->SetExcludeBlobCacheUpdated(true);
    return ePSGS_ProceedRetrieving;
}


void
CPSGS_CassBlobBase::PrepareServerErrorMessage(CCassBlobFetch *  fetch_details,
                                              int  code,
                                              EDiagSev  severity,
                                              const string &  message)
{
    if (fetch_details->IsBlobPropStage()) {
        x_PrepareBlobPropMessage(fetch_details, message,
                                 CRequestStatus::e500_InternalServerError,
                                 code, severity);
    } else {
        x_PrepareBlobMessage(fetch_details, message,
                             CRequestStatus::e500_InternalServerError,
                             code, severity);
    }
}


void
CPSGS_CassBlobBase::OnGetBlobError(CCassBlobFetch *  fetch_details,
                                   CRequestStatus::ECode  status,
                                   int  code,
                                   EDiagSev  severity,
                                   const string &  message)
{
    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    // It could be a message or an error
    CountError(fetch_details, status, code, severity, message,
               ePSGS_NeedLogging, ePSGS_NeedStatusUpdate);
    bool    is_error = IsError(severity);

    if (is_error) {
        PrepareServerErrorMessage(fetch_details, code, severity, message);

        // Remove from the already-sent cache if necessary
        fetch_details->RemoveFromExcludeBlobCache();

        // If it is an error then regardless what stage it was, props or
        // chunks, there will be no more activity
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
    } else {
        if (fetch_details->IsBlobPropStage())
            x_PrepareBlobPropMessage(fetch_details, message, status,
                                     code, severity);
        else
            x_PrepareBlobMessage(fetch_details, message, status,
                                 code, severity);

        // To avoid sending an error in Peek()
        fetch_details->GetLoader()->ClearError();
    }
}


void
CPSGS_CassBlobBase::OnGetBlobChunk(bool  cancelled,
                                   CCassBlobFetch *  fetch_details,
                                   const unsigned char *  chunk_data,
                                   unsigned int  data_size,
                                   int  chunk_no)
{
    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    if (cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        return;
    }
    if (m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                            this,
                                            CPSGSCounters::ePSGS_ProcUnknownError);
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        return;
    }

    if (chunk_no >= 0) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Blob chunk " + to_string(chunk_no) + " callback",
                               m_Request->GetStartTimestamp());
        }

        // A blob chunk; 0-length chunks are allowed too
        x_PrepareBlobData(fetch_details, chunk_data, data_size, chunk_no);

        // Collect split info for further analisys if needed
        if (m_CollectSplitInfo) {
            if (fetch_details->GetBlobId() == m_InfoBlobId) {
                if (m_Request->NeedTrace()) {
                    m_Reply->SendTrace("Collecting split info in the buffer "
                                       "for further analysis. Chunk number: " +
                                       to_string(chunk_no) + " of size " +
                                       to_string(data_size),
                                       m_Request->GetStartTimestamp());
                }
                m_CollectedSplitInfo.AddDataChunk(chunk_data, data_size, chunk_no);
            }
        }
    } else {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Blob chunk no-more-data callback",
                               m_Request->GetStartTimestamp());
        }

        // End of the blob
        x_PrepareBlobCompletion(fetch_details);
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();

        // Note: no need to set the blob completed in the exclude blob cache.
        // It will happen in Peek()

        // Analyse split info if needed
        if (m_CollectSplitInfo) {
            if (fetch_details->GetBlobId() == m_InfoBlobId) {
                x_DeserializeSplitInfo(fetch_details);
            }
        }
    }
}


void
CPSGS_CassBlobBase::x_OnBlobPropNotFound(CCassBlobFetch *  fetch_details)
{
    // Not found, report 502 because it is data inconsistency
    // or 404 if it was requested via sat.sat_key
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_BlobPropsNotFound);

    auto    blob_id = fetch_details->GetBlobId();
    string  message = "Blob " + blob_id.ToString() +
                      " properties are not found (last modified: " +
                      to_string(fetch_details->GetLoader()->GetModified()) + ")";
    if (fetch_details->GetFetchType() == ePSGS_BlobBySatSatKeyFetch) {
        // User requested wrong sat_key, so it is a client error
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        x_PrepareBlobPropMessage(fetch_details, message,
                                 CRequestStatus::e404_NotFound,
                                 ePSGS_NoBlobPropsError, eDiag_Error);
    } else {
        // Server error, data inconsistency
        PSG_ERROR(message);
        UpdateOverallStatus(CRequestStatus::e502_BadGateway);
        x_PrepareBlobPropMessage(fetch_details, message,
                                 CRequestStatus::e502_BadGateway,
                                 ePSGS_NoBlobPropsError, eDiag_Error);
    }

    // Remove from the already-sent cache if necessary
    fetch_details->RemoveFromExcludeBlobCache();

    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
}


unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>
CPSGS_CassBlobBase::x_CheckId2Info(CCassBlobFetch *  fetch_details,
                                        CBlobRecord &  blob_prop)
{
    string                                              id2_info = blob_prop.GetId2Info();
    unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     parsed_id2_info;

    if (id2_info.empty())
        return parsed_id2_info;

    string                                              err_msg;
    try {
        // Note: in case of an error it is already counted in the constructor
        // which parses id2 info field
        parsed_id2_info.reset(new CPSGS_SatInfoChunksVerFlavorId2Info(blob_prop.GetId2Info()));
        return parsed_id2_info;
    } catch (const exception &  exc) {
        err_msg = "Error extracting id2 info for the blob " +
            fetch_details->GetBlobId().ToString() + ": " + exc.what();
    } catch (...) {
        err_msg = "Unknown error extracting id2 info for the blob " +
            fetch_details->GetBlobId().ToString() + ".";
    }

    err_msg += "\nThe broken id2 info field content will be discarded "
               "before sending to the client.";
    blob_prop.SetId2Info("");

    m_Reply->PrepareProcessorMessage(
        m_Reply->GetItemId(),
        m_ProcessorId, err_msg, CRequestStatus::e500_InternalServerError,
        ePSGS_BadID2Info, eDiag_Warning);

    PSG_ERROR(err_msg);
    return parsed_id2_info;
}


bool
CPSGS_CassBlobBase::NeedToAddId2CunkId2Info(void) const
{
    auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();
    if (blob_request.m_TSEOption == SPSGS_BlobRequestBase::ePSGS_OrigTSE)
        return false;

    return m_Id2Info.get() != nullptr && m_NeedToParseId2Info == false;
}


int64_t
CPSGS_CassBlobBase::x_GetId2ChunkNumber(CCassBlobFetch *  fetch_details)
{
    // Note: this member is called only when m_Id2Info is parsed successfully

    auto        blob_key = fetch_details->GetBlobId().m_SatKey;
    auto        orig_blob_info = m_Id2Info->GetInfo();
    if (orig_blob_info == blob_key) {
        // It is a split info chunk so use a special value
        return kSplitInfoChunk;
    }

    // Calculate the id2_chunk
    return blob_key - orig_blob_info + m_Id2Info->GetChunks() + 1;
}


bool
CPSGS_CassBlobBase::x_IsAuthorized(EPSGS_BlobOp  blob_op,
                                   const SCass_BlobId &  blob_id,
                                   const CBlobRecord &  blob,
                                   const TAuthToken &  auth_token)
{
    // Future extension: at the moment there is only one blob operation and
    // there are no authorization tokens. Later they may come to play

    // By some reasons the function deals not only with the authorization but
    // with withdrawal and blob publication date (confidentiality)

    // Note: the IsConfidential() needs to be checked only if it is not a
    // secure keyspace. For the secure keyspace the confidential blobs need to
    // be supplied anyway.
    bool    is_secure_keyspace = false;
    if (blob_id.m_IsSecureKeyspace.has_value()) {
        is_secure_keyspace = blob_id.m_IsSecureKeyspace.value();
    }

    if (!is_secure_keyspace) {
        if (blob.IsConfidential()) {
            if (m_Request->NeedTrace()) {
                m_Reply->SendTrace(
                    "Blob " + blob_id.ToString() + " is not authorized "
                    "because it is confidential", m_Request->GetStartTimestamp());
            }
            return false;
        }
    }

    if (blob.GetFlag(EBlobFlags::eWithdrawn)) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace(
                "Blob " + blob_id.ToString() + " is not authorized "
                "because it is withdrawn", m_Request->GetStartTimestamp());
        }
        return false;
    }

    return true;
}


void
CPSGS_CassBlobBase::x_PrepareBlobPropData(CCassBlobFetch *  blob_fetch_details,
                                          CBlobRecord const &  blob)
{
    bool    need_id2_identification = blob_fetch_details->GetNeedAddId2ChunkId2Info();

    // CXX-11547: may be public comments request is needed as well
    if (blob.GetFlag(EBlobFlags::eSuppress) ||
        blob.GetFlag(EBlobFlags::eWithdrawn)) {
        // Request public comment
        auto                                    app = CPubseqGatewayApp::GetInstance();
        unique_ptr<CCassPublicCommentFetch>     comment_fetch_details;
        comment_fetch_details.reset(new CCassPublicCommentFetch());
        // Memorize the identification which will be used at the moment of
        // sending the comment to the client
        if (need_id2_identification) {
            comment_fetch_details->SetId2Identification(
                x_GetId2ChunkNumber(blob_fetch_details),
                m_Id2Info->Serialize());
        } else {
            comment_fetch_details->SetCassBlobIdentification(
                blob_fetch_details->GetBlobId(),
                m_LastModified);
        }

        // Note: the blobs from a public keyspace may only point to the blobs in a
        // public keyspace. A similar rule is about a secure keyspace: HUP blobs
        // point only to HUP blobs. Thus if a keyspace is secure then the MyNCBI
        // resultion has already happened and can be used here.

        shared_ptr<CCassConnection>     cass_connection;

        try {
            if (blob_fetch_details->GetBlobId().m_IsSecureKeyspace.value()) {
                cass_connection = blob_fetch_details->GetBlobId().m_Keyspace->GetSecureConnection(
                                                    m_UserName.value());
                if (!cass_connection) {
                    ReportSecureSatUnauthorized(m_UserName.value());
                    return;
                }
            } else {
                cass_connection = blob_fetch_details->GetBlobId().m_Keyspace->GetConnection();
            }
        } catch (const exception &  exc) {
            ReportFailureToGetCassConnection(exc.what());
            return;
        } catch (...) {
            ReportFailureToGetCassConnection();
            return;
        }

        CCassStatusHistoryTaskGetPublicComment *    load_task =
            new CCassStatusHistoryTaskGetPublicComment(
                    cass_connection,
                    blob_fetch_details->GetBlobId().m_Keyspace->keyspace,
                    blob, nullptr);
        comment_fetch_details->SetLoader(load_task);
        load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
        load_task->SetErrorCB(
            CPublicCommentErrorCallback(
                this,
                bind(&CPSGS_CassBlobBase::OnPublicCommentError,
                     this, _1, _2, _3, _4, _5),
                comment_fetch_details.get()));
        load_task->SetLoggingCB(
                bind(&CPSGS_CassProcessorBase::LoggingCallback,
                     this, _1, _2));
        load_task->SetCommentCallback(
            CPublicCommentConsumeCallback(
                this,
                bind(&CPSGS_CassBlobBase::OnPublicComment,
                     this, _1, _2, _3),
                comment_fetch_details.get()));
        load_task->SetMessages(app->GetPublicCommentsMapping());

        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace(
                "Cassandra request: " +
                ToJsonString(*load_task),
                m_Request->GetStartTimestamp());
        }

        m_FetchDetails.push_back(std::move(comment_fetch_details));
        load_task->Wait();  // Initiate cassandra request
    }


    if (need_id2_identification) {
        m_Reply->PrepareTSEBlobPropData(
            blob_fetch_details, m_ProcessorId,
            x_GetId2ChunkNumber(blob_fetch_details), m_Id2Info->Serialize(),
            ToJsonString(blob));
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob props without id2_chunk/id2_info
        m_Reply->PrepareBlobPropData(
            blob_fetch_details, m_ProcessorId,
            ToJsonString(blob), m_LastModified);
    }
}


void
CPSGS_CassBlobBase::x_PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details)
{
    if (fetch_details->GetNeedAddId2ChunkId2Info()) {
        m_Reply->PrepareTSEBlobPropCompletion(fetch_details, m_ProcessorId);
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob prop completion without id2_chunk/id2_info
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    }
}


void
CPSGS_CassBlobBase::x_PrepareBlobData(CCassBlobFetch *  fetch_details,
                                      const unsigned char *  chunk_data,
                                      unsigned int  data_size,
                                      int  chunk_no)
{
    if (fetch_details->GetNeedAddId2ChunkId2Info()) {
        m_Reply->PrepareTSEBlobData(
            fetch_details, m_ProcessorId,
            chunk_data, data_size, chunk_no,
            x_GetId2ChunkNumber(fetch_details), m_Id2Info->Serialize());
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob prop completion without id2_chunk/id2_info
        m_Reply->PrepareBlobData(fetch_details, m_ProcessorId,
                                 chunk_data, data_size, chunk_no,
                                 m_LastModified);
    }
}


void
CPSGS_CassBlobBase::x_PrepareBlobCompletion(CCassBlobFetch *  fetch_details)
{
    if (fetch_details->GetNeedAddId2ChunkId2Info()) {
        m_Reply->PrepareTSEBlobCompletion(fetch_details, m_ProcessorId);
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob prop completion without id2_chunk/id2_info
        m_Reply->PrepareBlobCompletion(fetch_details, m_ProcessorId);
    }
}


void
CPSGS_CassBlobBase::x_PrepareBlobPropMessage(CCassBlobFetch *  fetch_details,
                                             const string &  message,
                                             CRequestStatus::ECode  status,
                                             int  err_code,
                                             EDiagSev  severity)
{
    if (fetch_details->GetNeedAddId2ChunkId2Info()) {
        m_Reply->PrepareTSEBlobPropMessage(
            fetch_details, m_ProcessorId,
            x_GetId2ChunkNumber(fetch_details), m_Id2Info->Serialize(),
            message, status, err_code, severity);
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob prop completion without id2_chunk/id2_info
        m_Reply->PrepareBlobPropMessage(
            fetch_details, m_ProcessorId,
            message, status, err_code, severity);
    }

    x_PrepareBlobPropCompletion(fetch_details);
}


void
CPSGS_CassBlobBase::x_PrepareBlobMessage(CCassBlobFetch *  fetch_details,
                                         const string &  message,
                                         CRequestStatus::ECode  status,
                                         int  err_code,
                                         EDiagSev  severity)
{
    if (fetch_details->GetNeedAddId2ChunkId2Info()) {
        m_Reply->PrepareTSEBlobMessage(
            fetch_details, m_ProcessorId,
            x_GetId2ChunkNumber(fetch_details), m_Id2Info->Serialize(),
            message, status, err_code, severity);
    } else {
        // There is no id2info in the originally requested blob
        // so just send blob prop completion without id2_chunk/id2_info
        m_Reply->PrepareBlobMessage(
            fetch_details, m_ProcessorId,
            message, status, err_code, severity, m_LastModified);
    }

    x_PrepareBlobCompletion(fetch_details);
}


void
CPSGS_CassBlobBase::x_PrepareBlobExcluded(CCassBlobFetch *  fetch_details,
                                          EPSGS_BlobSkipReason  skip_reason,
                                          bool  need_add_id2_chunk_id2_info)
{
    if (skip_reason == ePSGS_BlobExcluded || !need_add_id2_chunk_id2_info) {
        m_Reply->PrepareBlobExcluded(fetch_details->GetBlobId().ToString(),
                                     m_ProcessorId, skip_reason,
                                     m_LastModified);
        return;
    }

    // It is sent or in progress and need to add more info
    // NOTE: the blob id argument is temporary to satisfy the older clients
    m_Reply->PrepareTSEBlobExcluded(m_ProcessorId, skip_reason,
                                    fetch_details->GetBlobId().ToString(),
                                    x_GetId2ChunkNumber(fetch_details),
                                    m_Id2Info->Serialize());
}


void
CPSGS_CassBlobBase::x_PrepareBlobExcluded(CCassBlobFetch *  fetch_details,
                                          unsigned long  sent_mks_ago,
                                          unsigned long  until_resend_mks,
                                          bool  need_add_id2_chunk_id2_info)
{
    // Note: this version of the method is used only for the ID/get requests so
    // the additional resend related fields need to be supplied

    if (need_add_id2_chunk_id2_info) {
        // NOTE: the blob id argument is temporary to satisfy the older clients
        m_Reply->PrepareTSEBlobExcluded(fetch_details->GetBlobId().ToString(),
                                        x_GetId2ChunkNumber(fetch_details),
                                        m_Id2Info->Serialize(),
                                        m_ProcessorId, sent_mks_ago,
                                        until_resend_mks);
    } else {
        m_Reply->PrepareBlobExcluded(fetch_details->GetBlobId().ToString(),
                                     m_ProcessorId, sent_mks_ago, until_resend_mks,
                                     m_LastModified);
    }
}


void
CPSGS_CassBlobBase::OnPublicCommentError(
                            CCassPublicCommentFetch *  fetch_details,
                            CRequestStatus::ECode  status,
                            int  code,
                            EDiagSev  severity,
                            const string &  message)
{
    if (m_Canceled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        return;
    }

    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    // It could be a message or an error
    CountError(fetch_details, status, code, severity, message,
               ePSGS_NeedLogging, ePSGS_NeedStatusUpdate);
    bool    is_error = IsError(severity);

    m_Reply->PrepareProcessorMessage(
        m_Reply->GetItemId(),
        m_ProcessorId, message, status, code, severity);

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    if (is_error) {
        // If it is an error then there will be no more activity
        fetch_details->SetReadFinished();
    }

    // Note: is it necessary to call something like x_Peek() of the actual
    //       processor class to send this immediately? It should work without
    //       this call and at the moment x_Peek() is not available here
    // if (m_Reply->IsOutputReady())
    //     x_Peek(false);
}


void
CPSGS_CassBlobBase::OnPublicComment(
                            CCassPublicCommentFetch *  fetch_details,
                            string  comment,
                            bool  is_found)
{
    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();

    if (m_Canceled) {
        fetch_details->GetLoader()->Cancel();
        return;
    }

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(
            "Public comment callback; found: " + to_string(is_found),
            m_Request->GetStartTimestamp());
    }

    if (is_found) {
        if (fetch_details->GetIdentification() ==
                        CCassPublicCommentFetch::ePSGS_ById2) {
            m_Reply->PreparePublicComment(
                        m_ProcessorId, comment,
                        fetch_details->GetId2Chunk(),
                        fetch_details->GetId2Info());
        } else {
            // There is no id2info in the originally requested blob
            // so just send blob prop completion without id2_chunk/id2_info
            m_Reply->PreparePublicComment(
                        m_ProcessorId, comment,
                        fetch_details->GetBlobId().ToString(),
                        fetch_details->GetLastModified());
        }
    }

    // Note: is it necessary to call something like x_Peek() of the actual
    //       processor class to send this immediately? It should work without
    //       this call and at the moment x_Peek() is not available here
    // if (m_Reply->IsOutputReady())
    //     x_Peek(false);
}


void
CPSGS_CassBlobBase::x_BlobChunkCallback(CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob,
                                        const unsigned char *  chunk_data,
                                        unsigned int  data_size,
                                        int  chunk_no)
{
    if (!m_NeedFallbackBlob) {
        // As usual; no need to request anything extra or skip
        m_BlobChunkCB(fetch_details, blob, chunk_data, data_size, chunk_no);
        return;
    }

    if (!m_FallbackBlobRequested) {
        // Chunk came when there were no previous problems; so as usual
        m_BlobChunkCB(fetch_details, blob, chunk_data, data_size, chunk_no);
        return;
    }

    // Here: chunk came when there was an error before and fallback blob has
    // been already requested. Check if it is an ID2 chunk.
    string      blob_id_as_str = fetch_details->GetBlobId().ToString();
    if (find(m_RequestedID2BlobChunks.begin(),
             m_RequestedID2BlobChunks.end(), blob_id_as_str) ==
        m_RequestedID2BlobChunks.end()) {
        // It is not an ID2 chunk; proceed as usual
        m_BlobChunkCB(fetch_details, blob, chunk_data, data_size, chunk_no);
        return;
    }

    if (fetch_details->GetTotalSentBlobChunks() > 0) {
        // Some chunks of that blob have already been sent. To avoid breaking
        // the chunks consistency, e.g. the final closing PSG-Chunk, let's let
        // it be finished as usual
        m_BlobChunkCB(fetch_details, blob, chunk_data, data_size, chunk_no);
        return;
    }

    if (m_Request->NeedTrace()) {
        string  msg = "Receiving an ID2 blob " + blob_id_as_str +
                      " chunk " + to_string(chunk_no) +
                      " when a fallback original blob has been requested. "
                      "Ignore and continue.";
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    // Discarding the chunk.
    fetch_details->RemoveFromExcludeBlobCache();
    fetch_details->GetLoader()->Cancel();
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
}


void
CPSGS_CassBlobBase::x_BlobPropsCallback(CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob,
                                        bool  is_found)
{
    if (!m_NeedFallbackBlob) {
        // As usual
        m_BlobPropsCB(fetch_details, blob, is_found);
        return;
    }

    string      blob_id_as_str = fetch_details->GetBlobId().ToString();
    if (find(m_RequestedID2BlobChunks.begin(),
             m_RequestedID2BlobChunks.end(), blob_id_as_str) ==
        m_RequestedID2BlobChunks.end()) {
        // It is not an ID2 chunk; proceed as usual
        m_BlobPropsCB(fetch_details, blob, is_found);
        return;
    }

    string      msg;

    if (!m_FallbackBlobRequested) {
        if (is_found) {
            // As usual; blob props are found and everything is going well
            m_BlobPropsCB(fetch_details, blob, is_found);
            return;
        }

        // Here: blob prop not found and falback has not been requested yet.
        m_FallbackBlobRequested = true;
        fetch_details->GetLoader()->Cancel();
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();

        msg = "Blob " + blob_id_as_str + " properties are not found. "
              "Falling back to retrieve the original blob.";

        CRequestContextResetter     context_resetter;
        m_Request->SetRequestContext();

        PSG_WARNING(msg);

        // Request original blob as a fallback
        x_RequestOriginalBlobChunks(m_InitialBlobPropFetch, m_InitialBlobProps);

        // Send a processor message
        m_Reply->PrepareProcessorMessage(
            m_Reply->GetItemId(),
            m_ProcessorId, msg, CRequestStatus::e500_InternalServerError,
            ePSGS_NotFoundID2BlobPropWithFallback,
            eDiag_Warning);

        return;
    }

    // Here: fallback has already been requested. The blob props are found or
    // not.

    if (m_Request->NeedTrace()) {
        if (is_found) {
            msg = "Blob " + blob_id_as_str + " properties are received when "
                  "a fallback to request the original blob "
                  "has already been initiated before.";
        } else {
            msg = "Blob " + blob_id_as_str + " properties are not found. "
                  "Fallback to request the original blob "
                  "has already been initiated before.";
        }

        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    // To prevent chunks coming, let's cancel the fetch
    fetch_details->GetLoader()->Cancel();
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();
}


void
CPSGS_CassBlobBase::x_BlobErrorCallback(CCassBlobFetch *  fetch_details,
                                        CRequestStatus::ECode  status,
                                        int  code,
                                        EDiagSev  severity,
                                        const string &  message)
{
    if (!m_NeedFallbackBlob) {
        // As usual
        m_BlobErrorCB(fetch_details, status, code, severity, message);
        return;
    }

    string      blob_id_as_str = fetch_details->GetBlobId().ToString();
    if (find(m_RequestedID2BlobChunks.begin(),
             m_RequestedID2BlobChunks.end(), blob_id_as_str) ==
        m_RequestedID2BlobChunks.end()) {
        // It is not an ID2 chunk; proceed as usual
        m_BlobErrorCB(fetch_details, status, code, severity, message);
        return;
    }

    // Not found chunk is an error here regardless of the reported severity
    bool    is_error = IsError(severity) || (status == CRequestStatus::e404_NotFound);
    if (!is_error) {
        // It is not an error; could be a warning or just information.
        // Proceed as usual.
        m_BlobErrorCB(fetch_details, status, code, severity, message);
        return;
    }

    // Remove from the already-sent cache if necessary
    fetch_details->RemoveFromExcludeBlobCache();

    // If it is an error then regardless what stage it was, props or
    // chunks, there will be no more activity
    fetch_details->GetLoader()->Cancel();
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();

    string      msg;
    if (!m_FallbackBlobRequested) {
        // It is an error and fallback has not been requested yet.
        // Let's initiate it.

        m_FallbackBlobRequested = true;

        msg = "Blob " + blob_id_as_str + " retrieval error. "
              "Fallback to request the original blob is initiated.\n"
              "Callback error message: " + message;

        CRequestContextResetter     context_resetter;
        m_Request->SetRequestContext();

        PSG_WARNING(msg);

        // Request original blob as a fallback
        x_RequestOriginalBlobChunks(m_InitialBlobPropFetch, m_InitialBlobProps);

        if (fetch_details->GetTotalSentBlobChunks() == 0) {
            m_Reply->PrepareProcessorMessage(
                m_Reply->GetItemId(), m_ProcessorId, msg,
                CRequestStatus::e500_InternalServerError,
                ePSGS_ID2ChunkErrorWithFallback, eDiag_Warning);
        } else {
            x_PrepareBlobMessage(fetch_details,
                                 msg, CRequestStatus::e500_InternalServerError,
                                 ePSGS_ID2ChunkErrorWithFallback,
                                 eDiag_Warning);
        }
        return;
    }

    // It's an error but a fallback has already been requested because of a
    // different event
    msg = "Blob " + blob_id_as_str + " retrieval error. "
          "Fallback to request the original blob has already been initiated before.\n"
          "Callback error message: " + message;

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    if (fetch_details->GetTotalSentBlobChunks() == 0) {
        m_Reply->PrepareProcessorMessage(
            m_Reply->GetItemId(), m_ProcessorId, msg,
            CRequestStatus::e500_InternalServerError,
            ePSGS_ID2ChunkErrorWithFallback, eDiag_Warning);
    } else {
        x_PrepareBlobMessage(fetch_details,
                             msg, CRequestStatus::e500_InternalServerError,
                             ePSGS_ID2ChunkErrorAfterFallbackRequested,
                             eDiag_Warning);
    }
}

