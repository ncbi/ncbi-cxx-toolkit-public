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
 * File Description: get TSE chunk processor
 *
 */

#include <ncbi_pch.hpp>

#include "tse_chunk_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "get_blob_callback.hpp"
#include "split_history_callback.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;

CPSGS_TSEChunkProcessor::CPSGS_TSEChunkProcessor() :
    m_TSEChunkRequest(nullptr),
    m_Cancelled(false)
{}


CPSGS_TSEChunkProcessor::CPSGS_TSEChunkProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                const SCass_BlobId &  blob_id) :
    CPSGS_CassProcessorBase(request, reply),
    CPSGS_CassBlobBase(request, reply),
    m_Cancelled(false)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;

    m_BlobId = blob_id;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_TSEChunkRequest>() everywhere
    m_TSEChunkRequest = & request->GetRequest<SPSGS_TSEChunkRequest>();
}


CPSGS_TSEChunkProcessor::~CPSGS_TSEChunkProcessor()
{}


IPSGS_Processor*
CPSGS_TSEChunkProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                         shared_ptr<CPSGS_Reply> reply) const
{
    if (request->GetRequestType() != CPSGS_Request::ePSGS_TSEChunkRequest)
        return nullptr;

    auto            blob_request = & request->GetRequest<SPSGS_TSEChunkRequest>();
    SCass_BlobId    blob_id(blob_request->m_TSEId.GetId());
    if (!blob_id.IsValid())
        return nullptr;

    return new CPSGS_TSEChunkProcessor(request, reply, blob_id);
}


void CPSGS_TSEChunkProcessor::Process(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    string  err_msg;

    if (!m_BlobId.MapSatToKeyspace()) {
        app->GetErrorCounters().IncClientSatToSatName();

        err_msg = GetName() + " failed to map sat " +
                  to_string(m_BlobId.m_Sat) + " to a Cassandra keyspace";
        x_SendProcessorError(err_msg, CRequestStatus::e404_NotFound,
                             ePSGS_UnknownResolvedSatellite);
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();

        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    }

    // First, check the blob prop cache, may be the requested version matches
    // the requested one
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(IPSGS_Processor::m_Request,
                                          IPSGS_Processor::m_Reply);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    auto                        blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(m_BlobId.m_Sat,
                                 m_BlobId.m_SatKey,
                                 last_modified, *blob_record.get());
    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        do {
            // Step 1: check the id2info presense
            if (blob_record->GetId2Info().empty()) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + m_BlobId.ToString() +
                            " properties id2info is empty in cache");
                break;  // Continue with cassandra
            }

            // Step 2: check that the id2info is parsable
            unique_ptr<CPSGId2Info>     id2_info;
            // false -> do not finish the request
            if (!x_ParseTSEChunkId2Info(blob_record->GetId2Info(),
                                        id2_info, m_BlobId,
                                        false)) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                break;  // Continue with cassandra
            }

            // Step 3: check the split version in cache
            if (id2_info->GetSplitVersion() != m_TSEChunkRequest->m_SplitVersion) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + m_BlobId.ToString() +
                            " split version in cache does not match the requested one");
                break;  // Continue with cassandra
            }

            app->GetRequestCounters().IncTSEChunkSplitVersionCacheMatched();

            // Step 4: validate the chunk number
            if (!x_ValidateTSEChunkNumber(m_TSEChunkRequest->m_Chunk,
                                          id2_info->GetChunks(), false)) {
                break; // Continue with cassandra
            }

            // Step 5: For the target chunk - convert sat to sat name
            // Chunk's blob id
            int64_t         sat_key = id2_info->GetInfo() -
                                      id2_info->GetChunks() - 1 + m_TSEChunkRequest->m_Chunk;
            SCass_BlobId    chunk_blob_id(id2_info->GetSat(), sat_key);
            if (!x_TSEChunkSatToKeyspace(chunk_blob_id, false)) {
                break;  // Continue with cassandra
            }

            // Step 6: search in cache the TSE chunk properties
            last_modified = INT64_MIN;
            auto  tse_blob_prop_cache_lookup_result = psg_cache.LookupBlobProp(
                            chunk_blob_id.m_Sat, chunk_blob_id.m_SatKey,
                            last_modified, *blob_record.get());
            if (tse_blob_prop_cache_lookup_result != ePSGS_CacheHit) {
                err_msg = "TSE chunk blob " + chunk_blob_id.ToString() +
                          " properties are not found in cache";
                if (tse_blob_prop_cache_lookup_result == ePSGS_CacheFailure)
                    err_msg += " due to LMDB error";
                PSG_WARNING(err_msg);
                break;  // Continue with cassandra
            }

            // Step 7: initiate the chunk request
            SPSGS_RequestBase::EPSGS_Trace  trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
            if (IPSGS_Processor::m_Request->NeedTrace())
                trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;
            SPSGS_BlobBySatSatKeyRequest
                        chunk_request(SPSGS_BlobId(chunk_blob_id.ToString()),
                                      INT64_MIN,
                                      SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                                      SPSGS_RequestBase::ePSGS_UnknownUseCache,
                                      "", 0, trace_flag,
                                      chrono::high_resolution_clock::now());

            unique_ptr<CCassBlobFetch>  fetch_details;
            fetch_details.reset(new CCassBlobFetch(chunk_request, chunk_blob_id));
            CCassBlobTaskLoadBlob *         load_task =
                new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                          app->GetCassandraMaxRetries(),
                                          app->GetCassandraConnection(),
                                          chunk_blob_id.m_Keyspace,
                                          move(blob_record),
                                          true, nullptr);
            fetch_details->SetLoader(load_task);
            load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
            load_task->SetErrorCB(
                CGetBlobErrorCallback(bind(&CPSGS_TSEChunkProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details.get()));
            load_task->SetPropsCallback(
                CBlobPropCallback(bind(&CPSGS_TSEChunkProcessor::OnGetBlobProp,
                                        this, _1, _2, _3),
                                  IPSGS_Processor::m_Request,
                                  IPSGS_Processor::m_Reply,
                                  fetch_details.get(), false));
            load_task->SetChunkCallback(
                CBlobChunkCallback(bind(&CPSGS_TSEChunkProcessor::OnGetBlobChunk,
                                        this, _1, _2, _3, _4, _5),
                                   fetch_details.get()));

            if (IPSGS_Processor::m_Request->NeedTrace()) {
                IPSGS_Processor::m_Reply->SendTrace(
                            "Cassandra request: " +
                            ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                            IPSGS_Processor::m_Request->GetStartTimestamp());
            }

            m_FetchDetails.push_back(move(fetch_details));
            load_task->Wait();  // Initiate cassandra request
            return;
        } while (false);
    } else {
        err_msg = "Blob " + m_BlobId.ToString() +
                  " properties are not found in cache";
        if (blob_prop_cache_lookup_result == ePSGS_CacheFailure)
            err_msg += " due to LMDB error";
        PSG_WARNING(err_msg);
    }

    // Here:
    // - fallback to cassandra
    // - cache is not allowed
    // - not found in cache

    // Initiate async the history request
    unique_ptr<CCassSplitHistoryFetch>      fetch_details;
    fetch_details.reset(new CCassSplitHistoryFetch(*m_TSEChunkRequest, m_BlobId));
    CCassBlobTaskFetchSplitHistory *   load_task =
        new  CCassBlobTaskFetchSplitHistory(app->GetCassandraTimeout(),
                                            app->GetCassandraMaxRetries(),
                                            app->GetCassandraConnection(),
                                            m_BlobId.m_Keyspace,
                                            m_BlobId.m_SatKey,
                                            m_TSEChunkRequest->m_SplitVersion,
                                            nullptr, nullptr);
    fetch_details->SetLoader(load_task);
    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CSplitHistoryErrorCallback(
            bind(&CPSGS_TSEChunkProcessor::OnGetSplitHistoryError,
                 this, _1, _2, _3, _4, _5),
            fetch_details.get()));
    load_task->SetConsumeCallback(
        CSplitHistoryConsumeCallback(
            bind(&CPSGS_TSEChunkProcessor::OnGetSplitHistory,
                 this, _1, _2),
            fetch_details.get()));

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                    "Cassandra request: " +
                    ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                    IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(fetch_details));
    load_task->Wait();  // Initiate cassandra request
}


void CPSGS_TSEChunkProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob,
                                            bool is_found)
{
    CPSGS_CassBlobBase::OnGetBlobProp(bind(&CPSGS_TSEChunkProcessor::OnGetBlobProp,
                                           this, _1, _2, _3),
                                      bind(&CPSGS_TSEChunkProcessor::OnGetBlobChunk,
                                           this, _1, _2, _3, _4, _5),
                                      bind(&CPSGS_TSEChunkProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_TSEChunkProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
                                             CRequestStatus::ECode  status,
                                             int  code,
                                             EDiagSev  severity,
                                             const string &  message)
{
    CPSGS_CassBlobBase::OnGetBlobError(fetch_details, status, code,
                                       severity, message);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_TSEChunkProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
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


void
CPSGS_TSEChunkProcessor::OnGetSplitHistory(
                                    CCassSplitHistoryFetch *  fetch_details,
                                    vector<SSplitHistoryRecord> && result)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    fetch_details->SetReadFinished();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        return;
    }

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                "Split history callback; found: " + to_string(result.empty()),
                IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    auto *      app = CPubseqGatewayApp::GetInstance();
    if (result.empty()) {
        // Split history is not found
        app->GetErrorCounters().IncSplitHistoryNotFoundError();

        string      message = "Split history version " +
                              to_string(fetch_details->GetSplitVersion()) +
                              " is not found for the TSE id " +
                              fetch_details->GetTSEId().ToString();
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(),
                GetName(), message, CRequestStatus::e404_NotFound,
                ePSGS_SplitHistoryNotFound, eDiag_Error);
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        // Split history found.
        // Note: the request was issued so that there could be exactly one
        // split history record or none at all. So it is not checked that
        // there are more than one record.
        x_RequestTSEChunk(result[0], fetch_details);
    }

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void
CPSGS_TSEChunkProcessor::OnGetSplitHistoryError(
                                    CCassSplitHistoryFetch *  fetch_details,
                                    CRequestStatus::ECode  status,
                                    int  code,
                                    EDiagSev  severity,
                                    const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

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

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                "Split history error callback; status: " + to_string(status),
                IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(),
            GetName(), message, status, code, severity);

    if (is_error) {
        if (code == CCassandraException::eQueryTimeout)
            app->GetErrorCounters().IncCassQueryTimeoutError();
        else
            app->GetErrorCounters().IncUnknownError();

        // If it is an error then there will be no more activity
        fetch_details->SetReadFinished();
    }

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


bool
CPSGS_TSEChunkProcessor::x_ParseTSEChunkId2Info(
                                        const string &  info,
                                        unique_ptr<CPSGId2Info> &  id2_info,
                                        const SCass_BlobId &  blob_id,
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

    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetErrorCounters().IncInvalidId2InfoError();
    if (need_finish) {
        x_SendProcessorError(err_msg, CRequestStatus::e500_InternalServerError,
                             ePSGS_InvalidId2Info);
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        PSG_WARNING(err_msg);
    }
    return false;
}


void
CPSGS_TSEChunkProcessor::x_SendProcessorError(const string &  msg,
                                              CRequestStatus::ECode  status,
                                              int  code)
{
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(),
                GetName(), msg, status, code, eDiag_Error);
    UpdateOverallStatus(status);

    if (status >= CRequestStatus::e400_BadRequest &&
        status < CRequestStatus::e500_InternalServerError) {
        PSG_WARNING(msg);
    } else {
        PSG_ERROR(msg);
    }
}


bool
CPSGS_TSEChunkProcessor::x_ValidateTSEChunkNumber(
                                        int64_t  requested_chunk,
                                        CPSGId2Info::TChunks  total_chunks,
                                        bool  need_finish)
{
    if (requested_chunk > total_chunks) {
        string      msg = "Invalid chunk requested. "
                          "The number of available chunks: " +
                          to_string(total_chunks) + ", requested number: " +
                          to_string(requested_chunk);
        if (need_finish) {
            auto *  app = CPubseqGatewayApp::GetInstance();
            app->GetErrorCounters().IncMalformedArguments();
            x_SendProcessorError(msg, CRequestStatus::e400_BadRequest,
                                 ePSGS_MalformedParameter);
            m_Completed = true;
            IPSGS_Processor::m_Reply->SignalProcessorFinished();
        } else {
            PSG_WARNING(msg);
        }
        return false;
    }
    return true;
}


bool
CPSGS_TSEChunkProcessor::x_TSEChunkSatToKeyspace(SCass_BlobId &  blob_id,
                                                 bool  need_finish)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToKeyspace(blob_id.m_Sat, blob_id.m_Keyspace))
        return true;

    app->GetErrorCounters().IncServerSatToSatName();

    string  msg = "Unknown TSE chunk satellite number " +
                  to_string(blob_id.m_Sat) +
                  " for the blob " + blob_id.ToString();
    if (need_finish) {
        x_SendProcessorError(msg, CRequestStatus::e500_InternalServerError,
                             ePSGS_UnknownResolvedSatellite);

        // This method is used only in case of the TSE chunk requests.
        // So in case of errors - synchronous or asynchronous - it is
        // necessary to finish the reply anyway.
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        PSG_WARNING(msg);
    }
    return false;
}


void
CPSGS_TSEChunkProcessor::x_RequestTSEChunk(
                                    const SSplitHistoryRecord &  split_record,
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
    SCass_BlobId    chunk_blob_id(id2_info->GetSat(), sat_key);
    if (!x_TSEChunkSatToKeyspace(chunk_blob_id, true))
        return;

    // Look for the blob props
    // Form the chunk request with/without blob props
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    CPSGCache                   psg_cache(
            fetch_details->GetUseCache() != SPSGS_RequestBase::ePSGS_DbOnly,
            IPSGS_Processor::m_Request,
            IPSGS_Processor::m_Reply);
    int64_t                     last_modified = INT64_MIN;  // last modified is unknown
    auto                        blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(chunk_blob_id.m_Sat,
                                 chunk_blob_id.m_SatKey,
                                 last_modified, *blob_record.get());
    if (blob_prop_cache_lookup_result != ePSGS_CacheHit &&
        fetch_details->GetUseCache() == SPSGS_RequestBase::ePSGS_CacheOnly) {
        // Cassandra is forbidden for the blob prop
        string  err_msg = "TSE chunk blob " + chunk_blob_id.ToString() +
                          " properties are not found in cache";
        if (blob_prop_cache_lookup_result == ePSGS_CacheFailure)
            err_msg += " due to LMDB error";
        x_SendProcessorError(err_msg, CRequestStatus::e404_NotFound,
                             ePSGS_BlobPropsNotFound);
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
        return;
    }

    SPSGS_RequestBase::EPSGS_Trace  trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (IPSGS_Processor::m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;
    SPSGS_BlobBySatSatKeyRequest
        chunk_request(SPSGS_BlobId(chunk_blob_id.ToString()), INT64_MIN,
                      SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                      SPSGS_RequestBase::ePSGS_UnknownUseCache,
                      "", 0, trace_flag, chrono::high_resolution_clock::now());
    unique_ptr<CCassBlobFetch>  cass_blob_fetch;
    cass_blob_fetch.reset(new CCassBlobFetch(chunk_request, chunk_blob_id));

    auto    app = CPubseqGatewayApp::GetInstance();
    CCassBlobTaskLoadBlob *     load_task = nullptr;

    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunk_blob_id.m_Keyspace,
                            move(blob_record),
                            true, nullptr);
    } else {
        load_task = new CCassBlobTaskLoadBlob(
                            app->GetCassandraTimeout(),
                            app->GetCassandraMaxRetries(),
                            app->GetCassandraConnection(),
                            chunk_blob_id.m_Keyspace,
                            chunk_blob_id.m_SatKey,
                            true, nullptr);
    }
    cass_blob_fetch->SetLoader(load_task);

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(
            bind(&CPSGS_TSEChunkProcessor::OnGetBlobError,
                 this, _1, _2, _3, _4, _5),
            cass_blob_fetch.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(
            bind(&CPSGS_TSEChunkProcessor::OnGetBlobProp,
                 this, _1, _2, _3),
            IPSGS_Processor::m_Request,
            IPSGS_Processor::m_Reply,
            cass_blob_fetch.get(),
            blob_prop_cache_lookup_result != ePSGS_CacheHit));
    load_task->SetChunkCallback(
        CBlobChunkCallback(
            bind(&CPSGS_TSEChunkProcessor::OnGetBlobChunk,
                 this, _1, _2, _3, _4, _5),
            cass_blob_fetch.get()));

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                    "Cassandra request: " +
                    ToJson(*load_task).Repr(CJsonNode::fStandardJson),
                    IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(cass_blob_fetch));
    load_task->Wait();
}


void CPSGS_TSEChunkProcessor::Cancel(void)
{
    m_Cancelled = true;
}


IPSGS_Processor::EPSGS_Status CPSGS_TSEChunkProcessor::GetStatus(void)
{
    return CPSGS_CassProcessorBase::GetStatus();
}


string CPSGS_TSEChunkProcessor::GetName(void) const
{
    return "LMBD cache/Cassandra get TSE chunk processor";
}


void CPSGS_TSEChunkProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_TSEChunkProcessor::x_Peek(bool  need_wait)
{
    if (m_Cancelled)
        return;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    for (auto &  details: m_FetchDetails) {
        if (details)
            x_Peek(details, need_wait);
    }

    // TSE chunk: ready packets need to be sent right away
    if (IPSGS_Processor::m_Reply->IsOutputReady())
        IPSGS_Processor::m_Reply->Flush(false);
}


void CPSGS_TSEChunkProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                     bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return;

    if (need_wait)
        if (!fetch_details->ReadFinished())
            fetch_details->GetLoader()->Wait();

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetErrorCounters().IncUnknownError();
        PSG_ERROR(error);

        CCassBlobFetch *  blob_fetch = static_cast<CCassBlobFetch *>(fetch_details.get());
        if (blob_fetch->IsBlobPropStage()) {
            IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
                blob_fetch, error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);
            IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(blob_fetch);
        } else {
            IPSGS_Processor::m_Reply->PrepareBlobMessage(
                blob_fetch, error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);
            IPSGS_Processor::m_Reply->PrepareBlobCompletion(blob_fetch);
        }

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    }
}

