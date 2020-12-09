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
 * File Description: get blob processor
 *
 */
#include <ncbi_pch.hpp>

#include "getblob_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "get_blob_callback.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;


CPSGS_GetBlobProcessor::CPSGS_GetBlobProcessor() :
    m_BlobRequest(nullptr)
{}


CPSGS_GetBlobProcessor::CPSGS_GetBlobProcessor(
                                        shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority,
                                        const SCass_BlobId &  blob_id) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_CassBlobBase(request, reply, GetName())
{
    m_BlobId = blob_id;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_BlobBySatSatKeyRequest>() everywhere
    m_BlobRequest = & request->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
}


CPSGS_GetBlobProcessor::~CPSGS_GetBlobProcessor()
{}


IPSGS_Processor*
CPSGS_GetBlobProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_BlobBySatSatKeyRequest)
        return nullptr;

    auto            blob_request = & request->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    SCass_BlobId    blob_id(blob_request->m_BlobId.GetId());
    if (!blob_id.IsValid())
        return nullptr;

    return new CPSGS_GetBlobProcessor(request, reply, priority, blob_id);
}


void CPSGS_GetBlobProcessor::Process(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    if (!m_BlobId.MapSatToKeyspace()) {
        app->GetErrorCounters().IncClientSatToSatName();

        string  err_msg = GetName() + " processor failed to map sat " +
                          to_string(m_BlobId.m_Sat) +
                          " to a Cassandra keyspace";
        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(), GetName(),
                err_msg, CRequestStatus::e404_NotFound,
                ePSGS_UnknownResolvedSatellite, eDiag_Error);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        PSG_WARNING(err_msg);

        m_Completed = true;
        SignalFinishProcessing();

        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    }

    if (m_BlobRequest->m_TSEOption != SPSGS_BlobRequestBase::ePSGS_NoneTSE &&
        m_BlobRequest->m_TSEOption != SPSGS_BlobRequestBase::ePSGS_SlimTSE) {
        if (!m_BlobRequest->m_ClientId.empty()) {
            // Adding to exclude blob cache is unconditional however skipping
            // is only for the blobs identified by seq_id/seq_id_type
            bool        completed = true;
            auto        cache_result =
                app->GetExcludeBlobCache()->AddBlobId(
                        m_BlobRequest->m_ClientId,
                        m_BlobId.m_Sat,
                        m_BlobId.m_SatKey,
                        completed);
            if (cache_result == ePSGS_Added)
                m_BlobRequest->m_ExcludeBlobCacheAdded = true;
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(*m_BlobRequest, m_BlobId));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(IPSGS_Processor::m_Request,
                                      IPSGS_Processor::m_Reply);
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        m_BlobId.m_Sat,
                                        m_BlobId.m_SatKey,
                                        m_BlobRequest->m_LastModified,
                                        *blob_record.get());

    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              m_BlobId.m_Keyspace,
                                              move(blob_record),
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    } else {
        if (m_BlobRequest->m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
            // No data in cache and not going to the DB
            size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
            auto        ret_status = CRequestStatus::e404_NotFound;
            if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit) {
                IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
                    item_id, GetName(),
                    "Blob properties are not found",
                    ret_status, ePSGS_BlobPropsNotFound,
                    eDiag_Error);
            } else {
                ret_status = CRequestStatus::e500_InternalServerError;
                IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
                    item_id, GetName(),
                    "Blob properties are not found due to a cache lookup error",
                    ret_status, ePSGS_BlobPropsNotFound,
                    eDiag_Error);
            }
            IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(item_id,
                                                                GetName(),
                                                                2);

            if (m_BlobRequest->m_ExcludeBlobCacheAdded &&
                !m_BlobRequest->m_ClientId.empty()) {
                app->GetExcludeBlobCache()->Remove(
                        m_BlobRequest->m_ClientId,
                        m_BlobId.m_Sat,
                        m_BlobId.m_SatKey);

                // To prevent SetCompleted() later
                m_BlobRequest->m_ExcludeBlobCacheAdded = false;
            }

            // Finished without reaching cassandra
            UpdateOverallStatus(ret_status);
            m_Completed = true;
            SignalFinishProcessing();
            return;
        }

        if (m_BlobRequest->m_LastModified == INT64_MIN) {
            load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                                  app->GetCassandraMaxRetries(),
                                                  app->GetCassandraConnection(),
                                                  m_BlobId.m_Keyspace,
                                                  m_BlobId.m_SatKey,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        } else {
            load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                                  app->GetCassandraMaxRetries(),
                                                  app->GetCassandraConnection(),
                                                  m_BlobId.m_Keyspace,
                                                  m_BlobId.m_SatKey,
                                                  m_BlobRequest->m_LastModified,
                                                  false, nullptr);
            fetch_details->SetLoader(load_task);
        }
    }

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPSGS_GetBlobProcessor::OnGetBlobError,
                                   this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(bind(&CPSGS_GetBlobProcessor::OnGetBlobProp,
                               this, _1, _2, _3),
                          IPSGS_Processor::m_Request,
                          IPSGS_Processor::m_Reply,
                          fetch_details.get(),
                          blob_prop_cache_lookup_result != ePSGS_CacheHit));

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                            "Cassandra request: " +
                            ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(fetch_details));

    // Initiate cassandra request
    load_task->Wait();
}


void CPSGS_GetBlobProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                           CBlobRecord const &  blob,
                                           bool is_found)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    if (SignalStartProcessing() == EPSGS_StartProcessing::ePSGS_Cancel) {
        m_Completed = true;
        return;
    }

    CPSGS_CassBlobBase::OnGetBlobProp(bind(&CPSGS_GetBlobProcessor::OnGetBlobProp,
                                           this, _1, _2, _3),
                                      bind(&CPSGS_GetBlobProcessor::OnGetBlobChunk,
                                           this, _1, _2, _3, _4, _5),
                                      bind(&CPSGS_GetBlobProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetBlobProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
                                            CRequestStatus::ECode  status,
                                            int  code,
                                            EDiagSev  severity,
                                            const string &  message)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    CPSGS_CassBlobBase::OnGetBlobError(fetch_details, status, code,
                                       severity, message);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetBlobProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob,
                                            const unsigned char *  chunk_data,
                                            unsigned int  data_size,
                                            int  chunk_no)
{
    CPSGS_CassBlobBase::OnGetBlobChunk(m_Cancelled, fetch_details,
                                       chunk_data, data_size, chunk_no);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetBlobProcessor::Cancel(void)
{
    m_Cancelled = true;
    CancelLoaders();
}


IPSGS_Processor::EPSGS_Status CPSGS_GetBlobProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Cancelled)
        return IPSGS_Processor::ePSGS_Cancelled;

    return status;
}


string CPSGS_GetBlobProcessor::GetName(void) const
{
    return "Cassandra-getblob";
}


void CPSGS_GetBlobProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_GetBlobProcessor::x_Peek(bool  need_wait)
{
    if (m_Cancelled)
        return;

    if (m_InPeek)
        return;

    m_InPeek = true;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    bool        overall_final_state = false;
    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto &  details: m_FetchDetails) {
            if (details) {
                overall_final_state |= x_Peek(details, need_wait);
            }
        }

        if (initial_size == m_FetchDetails.size()) {
            break;
        }
    }

    // Blob specific: ready packets need to be sent right away
    if (IPSGS_Processor::m_Reply->IsOutputReady())
        IPSGS_Processor::m_Reply->Flush(false);

    // Blob specific: deal with exclude blob cache
    if (AreAllFinishedRead()) {
        // Get the reference to the blob base request
        auto &      blob_request =
                IPSGS_Processor::m_Request->GetRequest<SPSGS_BlobRequestBase>();

        if (blob_request.m_ExcludeBlobCacheAdded &&
            ! blob_request.m_ExcludeBlobCacheCompleted &&
            ! blob_request.m_ClientId.empty()) {
            auto *  app = CPubseqGatewayApp::GetInstance();
            app->GetExcludeBlobCache()->SetCompleted(
                                            blob_request.m_ClientId,
                                            m_BlobId.m_Sat,
                                            m_BlobId.m_SatKey, true);
            blob_request.m_ExcludeBlobCacheCompleted = true;
        }
    }

    m_InPeek = false;
}


bool CPSGS_GetBlobProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                    bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return true;

    bool    final_state = false;
    if (need_wait)
        if (!fetch_details->ReadFinished())
            final_state = fetch_details->GetLoader()->Wait();

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetErrorCounters().IncUnknownError();
        PSG_ERROR(error);

        CCassBlobFetch *  blob_fetch = static_cast<CCassBlobFetch *>(fetch_details.get());
        PrepareServerErrorMessage(blob_fetch, ePSGS_UnknownError, eDiag_Error, error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        SignalFinishProcessing();
    }

    return final_state;
}

