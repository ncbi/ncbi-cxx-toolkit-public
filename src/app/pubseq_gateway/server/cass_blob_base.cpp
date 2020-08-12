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

using namespace std::placeholders;


CPSGS_CassBlobBase::CPSGS_CassBlobBase()
{}


CPSGS_CassBlobBase::CPSGS_CassBlobBase(shared_ptr<CPSGS_Request>  request,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const string &  processor_id) :
    CPSGS_CassProcessorBase(request, reply),
    m_ProcessorId(processor_id)
{}


CPSGS_CassBlobBase::~CPSGS_CassBlobBase()
{}


void
CPSGS_CassBlobBase::OnGetBlobProp(TBlobPropsCB  blob_props_cb,
                                  TBlobChunkCB  blob_chunk_cb,
                                  TBlobErrorCB  blob_error_cb,
                                  CCassBlobFetch *  fetch_details,
                                  CBlobRecord const &  blob,
                                  bool is_found)
{
    CRequestContextResetter     context_resetter;
    m_Request->SetRequestContext();

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Blob prop callback; found: " + to_string(is_found),
                           m_Request->GetStartTimestamp());
    }

    if (is_found) {
        // Found, send blob props back as JSON
        m_Reply->PrepareBlobPropData(
            fetch_details, m_ProcessorId,
            ToJson(blob).Repr(CJsonNode::fStandardJson));

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
                x_OnBlobPropSlimTSE(blob_props_cb, blob_chunk_cb, blob_error_cb,
                                    fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_SmartTSE:
                x_OnBlobPropSmartTSE(blob_props_cb, blob_chunk_cb, blob_error_cb,
                                     fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_WholeTSE:
                x_OnBlobPropWholeTSE(blob_props_cb, blob_chunk_cb, blob_error_cb,
                                     fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_OrigTSE:
                x_OnBlobPropOrigTSE(blob_chunk_cb, blob_error_cb,
                                    fetch_details, blob);
                break;
            case SPSGS_BlobRequestBase::ePSGS_UnknownTSE:
                // Used when INFO blobs are asked; i.e. chunks have been
                // requested as well, so only the prop completion message needs
                // to be sent.
                m_Reply->PrepareBlobPropCompletion(fetch_details,
                                                   m_ProcessorId);
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
    m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    SetFinished(fetch_details);
}


void
CPSGS_CassBlobBase::x_OnBlobPropSlimTSE(TBlobPropsCB  blob_props_cb,
                                        TBlobChunkCB  blob_chunk_cb,
                                        TBlobErrorCB  blob_error_cb,
                                        CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob)
{
    auto        fetch_blob = fetch_details->GetBlobId();

    // The handler deals with both kind of blob requests:
    // - by sat/sat_key
    // - by seq_id/seq_id_type
    // So get the reference to the blob base request
    auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);

        // An original blob may be required if its size is small
        auto *          app = CPubseqGatewayApp::GetInstance();
        unsigned int    slim_max_blob_size = app->GetSlimMaxBlobSize();

        if (blob.GetSize() <= slim_max_blob_size) {
            // The blob is small, get it, but first check in the
            // exclude blob cache
            if (x_CheckExcludeBlobCache(fetch_details,
                                        blob_request) == ePSGS_InCache) {
                m_Reply->SignalProcessorFinished();
                return;
            }

            x_RequestOriginalBlobChunks(blob_chunk_cb, blob_error_cb,
                                        fetch_details, blob);
        } else {
            // Nothing else to be sent, the original blob is big
        }
        m_Reply->SignalProcessorFinished();
        return;
    }

    // Check the cache first - only if it is about the main
    // blob request
    if (x_CheckExcludeBlobCache(fetch_details,
                                blob_request) == ePSGS_InCache) {
        m_Reply->SignalProcessorFinished();
        return;
    }

    // Not in the cache, request the split INFO blob only
    x_RequestID2BlobChunks(blob_props_cb, blob_chunk_cb, blob_error_cb,
                           fetch_details, blob, true);

    // It is important to send completion after: there could be
    // an error of converting/translating ID2 info
    m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    m_Reply->SignalProcessorFinished();
}


void
CPSGS_CassBlobBase::x_OnBlobPropSmartTSE(TBlobPropsCB  blob_props_cb,
                                         TBlobChunkCB  blob_chunk_cb,
                                         TBlobErrorCB  blob_error_cb,
                                         CCassBlobFetch *  fetch_details,
                                         CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
        x_RequestOriginalBlobChunks(blob_chunk_cb, blob_error_cb,
                                    fetch_details, blob);
    } else {
        // Request the split INFO blob only
        x_RequestID2BlobChunks(blob_props_cb, blob_chunk_cb, blob_error_cb,
                               fetch_details, blob, true);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    }
    m_Reply->SignalProcessorFinished();
}


void
CPSGS_CassBlobBase::x_OnBlobPropWholeTSE(TBlobPropsCB  blob_props_cb,
                                         TBlobChunkCB  blob_chunk_cb,
                                         TBlobErrorCB  blob_error_cb,
                                         CCassBlobFetch *  fetch_details,
                                         CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    if (blob.GetId2Info().empty()) {
        // Request original blob chunks
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
        x_RequestOriginalBlobChunks(blob_chunk_cb, blob_error_cb,
                                    fetch_details, blob);
    } else {
        // Request the split INFO blob and all split chunks
        x_RequestID2BlobChunks(blob_props_cb, blob_chunk_cb, blob_error_cb,
                               fetch_details, blob, false);

        // It is important to send completion after: there could be
        // an error of converting/translating ID2 info
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    }
    m_Reply->SignalProcessorFinished();
}


void
CPSGS_CassBlobBase::x_OnBlobPropOrigTSE(TBlobChunkCB  blob_chunk_cb,
                                        TBlobErrorCB  blob_error_cb,
                                        CCassBlobFetch *  fetch_details,
                                        CBlobRecord const &  blob)
{
    fetch_details->SetReadFinished();
    // Request original blob chunks
    m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    x_RequestOriginalBlobChunks(blob_chunk_cb,
                                blob_error_cb,
                                fetch_details, blob);
    m_Reply->SignalProcessorFinished();
}


void
CPSGS_CassBlobBase::x_RequestOriginalBlobChunks(TBlobChunkCB  blob_chunk_cb,
                                                TBlobErrorCB  blob_error_cb,
                                                CCassBlobFetch *  fetch_details,
                                                CBlobRecord const &  blob)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    // eUnknownTSE is safe here; no blob prop call will happen anyway
    // eUnknownUseCache is safe here; no further resolution required
    auto    cass_blob_id = fetch_details->GetBlobId();
    SPSGS_BlobBySatSatKeyRequest
            orig_blob_request(SPSGS_BlobId(cass_blob_id.ToString()),
                              blob.GetModified(),
                              SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                              SPSGS_RequestBase::ePSGS_UnknownUseCache,
                              fetch_details->GetClientId(),
                              0, trace_flag,
                              chrono::high_resolution_clock::now());

    // Create the cass async loader
    unique_ptr<CBlobRecord>             blob_record(new CBlobRecord(blob));
    CCassBlobTaskLoadBlob *             load_task =
        new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                  app->GetCassandraMaxRetries(),
                                  app->GetCassandraConnection(),
                                  cass_blob_id.m_Keyspace,
                                  move(blob_record),
                                  true, nullptr);

    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(orig_blob_request, cass_blob_id));
    cass_blob_fetch->SetLoader(load_task);

    // Blob props have already been rceived
    cass_blob_fetch->SetBlobPropSent();

    load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(blob_error_cb, cass_blob_fetch.get()));
    load_task->SetPropsCallback(nullptr);
    load_task->SetChunkCallback(
        CBlobChunkCallback(blob_chunk_cb, cass_blob_fetch.get()));

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(
            "Cassandra request: " + ToJson(*load_task).Repr(CJsonNode::fStandardJson),
            m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(cass_blob_fetch));

    load_task->Wait();
}


void
CPSGS_CassBlobBase::x_RequestID2BlobChunks(TBlobPropsCB  blob_props_cb,
                                           TBlobChunkCB  blob_chunk_cb,
                                           TBlobErrorCB  blob_error_cb,
                                           CCassBlobFetch *  fetch_details,
                                           CBlobRecord const &  blob,
                                           bool  info_blob_only)
{
    if (!x_ParseId2Info(fetch_details, blob))
        return;

    auto *      app = CPubseqGatewayApp::GetInstance();

    // Translate sat to keyspace
    SCass_BlobId    info_blob_id(m_Id2Info->GetSat(), m_Id2Info->GetInfo());    // mandatory

    if (!app->SatToKeyspace(m_Id2Info->GetSat(), info_blob_id.m_Keyspace)) {
        // Error: send it in the context of the blob props
        string      message = "Error mapping id2 info sat (" +
                              to_string(m_Id2Info->GetSat()) +
                              ") to a cassandra keyspace for the blob " +
                              fetch_details->GetBlobId().ToString();
        m_Reply->PrepareBlobPropMessage(
                                fetch_details, m_ProcessorId, message,
                                CRequestStatus::e500_InternalServerError,
                                ePSGS_BadID2Info, eDiag_Error);
        app->GetErrorCounters().IncServerSatToSatName();
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        PSG_ERROR(message);
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
    SPSGS_BlobBySatSatKeyRequest
        info_blob_request(SPSGS_BlobId(info_blob_id.ToString()),
                          INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          "", 0, trace_flag,
                          chrono::high_resolution_clock::now());

    // Prepare Id2Info retrieval
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(info_blob_request, info_blob_id));

    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(m_Request, m_Reply);
    auto                        blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        info_blob_id.m_Sat,
                                        info_blob_id.m_SatKey,
                                        info_blob_request.m_LastModified,
                                        *blob_record.get());
    CCassBlobTaskLoadBlob *     load_task = nullptr;


    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        load_task = new CCassBlobTaskLoadBlob(
                        app->GetCassandraTimeout(),
                        app->GetCassandraMaxRetries(),
                        app->GetCassandraConnection(),
                        info_blob_id.m_Keyspace,
                        move(blob_record),
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
                m_Reply->PrepareBlobPropMessage(
                                    fetch_details, m_ProcessorId, message,
                                    CRequestStatus::e404_NotFound,
                                    ePSGS_BlobPropsNotFound, eDiag_Error);
            } else {
                message = "Blob properties are not found due to LMDB cache error";
                UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                m_Reply->PrepareBlobPropMessage(
                                    fetch_details, m_ProcessorId, message,
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
                        info_blob_id.m_Keyspace,
                        info_blob_id.m_SatKey,
                        true, nullptr);
    }
    cass_blob_fetch->SetLoader(load_task);

    load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(blob_error_cb, cass_blob_fetch.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(blob_props_cb,
                          m_Request, m_Reply, cass_blob_fetch.get(),
                          blob_prop_cache_lookup_result != ePSGS_CacheHit));
    load_task->SetChunkCallback(
        CBlobChunkCallback(blob_chunk_cb, cass_blob_fetch.get()));

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Cassandra request: " +
                           ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                           m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(cass_blob_fetch));
    auto    to_init_iter = m_FetchDetails.end();
    --to_init_iter;

    // We may need to request ID2 chunks
    if (!info_blob_only) {
        // Sat name is the same
        x_RequestId2SplitBlobs(blob_props_cb, blob_chunk_cb, blob_error_cb,
                               fetch_details, info_blob_id.m_Keyspace);
    }

    // initiate retrieval: only those which were just created
    while (to_init_iter != m_FetchDetails.end()) {
        if (*to_init_iter)
            (*to_init_iter)->GetLoader()->Wait();
        ++to_init_iter;
    }
}


void
CPSGS_CassBlobBase::x_RequestId2SplitBlobs(TBlobPropsCB  blob_props_cb,
                                           TBlobChunkCB  blob_chunk_cb,
                                           TBlobErrorCB  blob_error_cb,
                                           CCassBlobFetch *  fetch_details,
                                           const string &  keyspace)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    auto    trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;

    for (int  chunk_no = 1; chunk_no <= m_Id2Info->GetChunks(); ++chunk_no) {
        SCass_BlobId    chunks_blob_id(m_Id2Info->GetSat(),
                                       m_Id2Info->GetInfo() - m_Id2Info->GetChunks() - 1 + chunk_no);
        chunks_blob_id.m_Keyspace = keyspace;

        // eUnknownTSE is treated in the blob prop handler as to do nothing (no
        // sending completion message, no requesting other blobs)
        // eUnknownUseCache is safe here; no further resolution required
        // client_id is "" (empty string) so the split blobs do not participate
        // in the exclude blob cache
        SPSGS_BlobBySatSatKeyRequest
            chunk_request(SPSGS_BlobId(chunks_blob_id.ToString()),
                          INT64_MIN,
                          SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                          SPSGS_RequestBase::ePSGS_UnknownUseCache,
                          "", 0, trace_flag,
                          chrono::high_resolution_clock::now());

        unique_ptr<CCassBlobFetch>   details;
        details.reset(new CCassBlobFetch(chunk_request, chunks_blob_id));

        unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
        CPSGCache                   psg_cache(m_Request, m_Reply);
        auto                        blob_prop_cache_lookup_result =
                                        psg_cache.LookupBlobProp(
                                            chunks_blob_id.m_Sat,
                                            chunks_blob_id.m_SatKey,
                                            chunk_request.m_LastModified,
                                            *blob_record.get());
        CCassBlobTaskLoadBlob *     load_task = nullptr;

        if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
            load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunks_blob_id.m_Keyspace,
                            move(blob_record),
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
                    m_Reply->PrepareBlobPropMessage(
                                        details.get(), m_ProcessorId, message,
                                        CRequestStatus::e404_NotFound,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                } else {
                    message = "Blob properties are not found "
                              "due to a blob proc cache lookup error";
                    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
                    m_Reply->PrepareBlobPropMessage(
                                        details.get(), m_ProcessorId, message,
                                        CRequestStatus::e500_InternalServerError,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
                }
                m_Reply->PrepareBlobPropCompletion(details.get(), m_ProcessorId);
                PSG_WARNING(message);
                continue;
            }

            load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunks_blob_id.m_Keyspace,
                            chunks_blob_id.m_SatKey,
                            true, nullptr);
            details->SetLoader(load_task);
        }

        load_task->SetDataReadyCB(m_Reply->GetDataReadyCB());
        load_task->SetErrorCB(
            CGetBlobErrorCallback(blob_error_cb, details.get()));
        load_task->SetPropsCallback(
            CBlobPropCallback(blob_props_cb,
                              m_Request, m_Reply, details.get(),
                              blob_prop_cache_lookup_result != ePSGS_CacheHit));
        load_task->SetChunkCallback(
            CBlobChunkCallback(blob_chunk_cb, details.get()));

        m_FetchDetails.push_back(move(details));
    }
}



CPSGS_CassBlobBase::EPSGS_BlobCacheCheckResult
CPSGS_CassBlobBase::x_CheckExcludeBlobCache(CCassBlobFetch *  fetch_details,
                                            SPSGS_BlobRequestBase &  blob_request)
{
    if (blob_request.m_ClientId.empty())
        return ePSGS_NotInCache;

    auto        fetch_blob = fetch_details->GetBlobId();
    if (fetch_blob != m_BlobId)
        return ePSGS_NotInCache;

    auto *      app = CPubseqGatewayApp::GetInstance();
    bool        completed = true;
    auto        cache_result = app->GetExcludeBlobCache()->AddBlobId(
                                            blob_request.m_ClientId,
                                            m_BlobId.m_Sat,
                                            m_BlobId.m_SatKey,
                                            completed);
    if (m_Request->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest &&
        cache_result == ePSGS_AlreadyInCache) {
        m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
        if (completed)
            m_Reply->PrepareBlobExcluded(m_BlobId.ToString(), m_ProcessorId,
                                         ePSGS_BlobSent);
        else
            m_Reply->PrepareBlobExcluded(m_BlobId.ToString(), m_ProcessorId,
                                         ePSGS_BlobInProgress);
        return ePSGS_InCache;
    }

    if (cache_result == ePSGS_Added)
        blob_request.m_ExcludeBlobCacheAdded = true;
    return ePSGS_NotInCache;
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

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    auto *  app = CPubseqGatewayApp::GetInstance();
    if (status >= CRequestStatus::e400_BadRequest &&
        status < CRequestStatus::e500_InternalServerError) {
        PSG_WARNING(message);
    } else {
        PSG_ERROR(message);
    }

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Blob error callback; status " + to_string(status),
                           m_Request->GetStartTimestamp());
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
            m_Reply->PrepareBlobPropMessage(
                                fetch_details, m_ProcessorId, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
        } else {
            m_Reply->PrepareBlobMessage(
                                fetch_details, m_ProcessorId, message,
                                CRequestStatus::e500_InternalServerError,
                                code, severity);
            m_Reply->PrepareBlobCompletion(fetch_details, m_ProcessorId);
        }

        // The handler deals with both kind of blob requests:
        // - by sat/sat_key
        // - by seq_id/seq_id_type
        // So get the reference to the blob base request
        auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

        if (fetch_details->GetBlobId() == m_BlobId) {
            if (blob_request.m_ExcludeBlobCacheAdded &&
                ! blob_request.m_ClientId.empty()) {
                app->GetExcludeBlobCache()->Remove(blob_request.m_ClientId,
                                                   m_BlobId.m_Sat,
                                                   m_BlobId.m_SatKey);

                // To prevent any updates
                blob_request.m_ExcludeBlobCacheAdded = false;
            }
        }

        // If it is an error then regardless what stage it was, props or
        // chunks, there will be no more activity
        fetch_details->SetReadFinished();
    } else {
        if (fetch_details->IsBlobPropStage())
            m_Reply->PrepareBlobPropMessage(fetch_details, m_ProcessorId,
                                            message, status, code, severity);
        else
            m_Reply->PrepareBlobMessage(fetch_details, m_ProcessorId, message,
                                        status, code, severity);
    }

    SetFinished(fetch_details);
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
        SetFinished(fetch_details);
        return;
    }
    if (m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
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
        m_Reply->PrepareBlobData(fetch_details, m_ProcessorId,
                                 chunk_data, data_size, chunk_no);
    } else {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Blob chunk no-more-data callback",
                               m_Request->GetStartTimestamp());
        }

        // End of the blob
        m_Reply->PrepareBlobCompletion(fetch_details, m_ProcessorId);
        SetFinished(fetch_details);

        // Note: no need to set the blob completed in the exclude blob cache.
        // It will happen in Peek()
    }
}


void
CPSGS_CassBlobBase::x_OnBlobPropNotFound(CCassBlobFetch *  fetch_details)
{
    // Not found, report 500 because it is data inconsistency
    // or 404 if it was requested via sat.sat_key
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetErrorCounters().IncBlobPropsNotFoundError();

    auto    blob_id = fetch_details->GetBlobId();
    string  message = "Blob " + blob_id.ToString() + " properties are not found";
    if (fetch_details->GetFetchType() == ePSGS_BlobBySatSatKeyFetch) {
        // User requested wrong sat_key, so it is a client error
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        m_Reply->PrepareBlobPropMessage(fetch_details, m_ProcessorId, message,
                                        CRequestStatus::e404_NotFound,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
    } else {
        // Server error, data inconsistency
        PSG_ERROR(message);
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Reply->PrepareBlobPropMessage(fetch_details, m_ProcessorId, message,
                                        CRequestStatus::e500_InternalServerError,
                                        ePSGS_BlobPropsNotFound, eDiag_Error);
    }

    // The handler deals with both kind of blob requests:
    // - by sat/sat_key
    // - by seq_id/seq_id_type
    // So get the reference to the blob base request
    auto &      blob_request = m_Request->GetRequest<SPSGS_BlobRequestBase>();

    if (blob_id == m_BlobId) {
        if (blob_request.m_ExcludeBlobCacheAdded && !blob_request.m_ClientId.empty()) {
            app->GetExcludeBlobCache()->Remove(blob_request.m_ClientId,
                                               m_BlobId.m_Sat,
                                               m_BlobId.m_SatKey);
            blob_request.m_ExcludeBlobCacheAdded = false;
        }
    }

    m_Reply->PrepareBlobPropCompletion(fetch_details, m_ProcessorId);
    SetFinished(fetch_details);
}


bool
CPSGS_CassBlobBase::x_ParseId2Info(CCassBlobFetch *  fetch_details,
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
        fetch_details, m_ProcessorId,
        err_msg, CRequestStatus::e500_InternalServerError,
        ePSGS_BadID2Info, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
    return false;
}


void
CPSGS_CassBlobBase::SetFinished(CCassBlobFetch *  fetch_details)
{
    fetch_details->SetReadFinished();
    m_Reply->SignalProcessorFinished();
}

