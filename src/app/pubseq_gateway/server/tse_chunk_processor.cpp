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
            TProcessorPriority  priority,
            shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> sat_info_chunk_ver_id2info,
            shared_ptr<CPSGS_IdModifiedVerFlavorId2Info>    id_mod_ver_id2info) :
    CPSGS_CassProcessorBase(request, reply),
    CPSGS_CassBlobBase(request, reply, GetName()),
    m_Cancelled(false),
    m_SatInfoChunkVerId2Info(sat_info_chunk_ver_id2info),
    m_IdModVerId2Info(id_mod_ver_id2info)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;
    IPSGS_Processor::m_Priority = priority;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_TSEChunkRequest>() everywhere
    m_TSEChunkRequest = & request->GetRequest<SPSGS_TSEChunkRequest>();
}


CPSGS_TSEChunkProcessor::~CPSGS_TSEChunkProcessor()
{}


IPSGS_Processor*
CPSGS_TSEChunkProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                         shared_ptr<CPSGS_Reply> reply,
                                         TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_TSEChunkRequest)
        return nullptr;

    auto            tse_chunk_request = & request->GetRequest<SPSGS_TSEChunkRequest>();

    // CXX-11478: some VDB chunks start with 0 but in Cassandra they always
    // start with 1. Non negative condition is checked at the time when the
    // request is received.
    if (tse_chunk_request->m_Id2Chunk == 0)
        return nullptr;

    // Check parseability of the id2_info parameter
    shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>  sat_info_chunk_ver_id2info;
    shared_ptr<CPSGS_IdModifiedVerFlavorId2Info>     id_mod_ver_id2info;
    if (x_DetectId2InfoFlavor(tse_chunk_request->m_Id2Info,
                              sat_info_chunk_ver_id2info,
                              id_mod_ver_id2info) !=
                                            ePSGS_UnknownId2InfoFlavor) {
        return new CPSGS_TSEChunkProcessor(request, reply, priority,
                                           sat_info_chunk_ver_id2info,
                                           id_mod_ver_id2info);
    }

    return nullptr;
}


EPSGSId2InfoFlavor
CPSGS_TSEChunkProcessor::x_DetectId2InfoFlavor(
            const string &                                    id2_info,
            shared_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> & sat_info_chunk_ver_id2info,
            shared_ptr<CPSGS_IdModifiedVerFlavorId2Info> &    id_mod_ver_id2info) const
{
    try {
        // false -> do not count errors
        sat_info_chunk_ver_id2info.reset(
            new CPSGS_SatInfoChunksVerFlavorId2Info(id2_info, false));
        return ePSGS_SatInfoChunksVerId2InfoFlavor;
    } catch (...) {
        // Parsing error: may be it is another id2_info format
    }

    try {
        id_mod_ver_id2info.reset(
            new CPSGS_IdModifiedVerFlavorId2Info(id2_info));
        return ePSGS_IdModifiedVerId2InfoFlavor;
    } catch (...) {
        // Parsing error: may be it is for another processor
    }
    return ePSGS_UnknownId2InfoFlavor;
}


bool CPSGS_TSEChunkProcessor::x_ParseTSEChunkId2Info(
            const string &                                     info,
            unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info> &  id2_info,
            const SCass_BlobId &                               blob_id,
            bool                                               need_finish)
{
    string      err_msg;
    try {
        id2_info.reset(new CPSGS_SatInfoChunksVerFlavorId2Info(info));
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
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        PSG_WARNING(err_msg);
    }
    return false;
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
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    } else {
        PSG_WARNING(msg);
    }
    return false;
}


void CPSGS_TSEChunkProcessor::Process(void)
{
    if (m_SatInfoChunkVerId2Info.get() != nullptr) {
        x_ProcessSatInfoChunkVerId2Info();
        return;
    }
    if (m_IdModVerId2Info.get() != nullptr) {
        x_ProcessIdModVerId2Info();
        return;
    }

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Logic error: none of the id2_info options were initialized");
}


void CPSGS_TSEChunkProcessor::x_ProcessIdModVerId2Info(void)
{
    // This option is when id2info came in a shape of
    // tse_id~~last_modified~~split_version

    auto    app = CPubseqGatewayApp::GetInstance();
    string  err_msg;

    if (!m_IdModVerId2Info->GetTSEId().MapSatToKeyspace()) {
        app->GetErrorCounters().IncClientSatToSatName();

        err_msg = GetName() + " failed to map sat " +
                  to_string(m_IdModVerId2Info->GetTSEId().m_Sat) +
                  " to a Cassandra keyspace";
        x_SendProcessorError(err_msg, CRequestStatus::e404_NotFound,
                             ePSGS_UnknownResolvedSatellite);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
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

    // CXX-11478: last modified is to be ignored for now
    int64_t     last_modified = INT64_MIN;  // last modified is unknown
    auto        blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(m_IdModVerId2Info->GetTSEId().m_Sat,
                                 m_IdModVerId2Info->GetTSEId().m_SatKey,
                                 last_modified, *blob_record.get());
    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        do {
            // Step 1: check the id2info presense
            if (blob_record->GetId2Info().empty()) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + m_IdModVerId2Info->GetTSEId().ToString() +
                            " properties id2info is empty in cache");
                break;  // Continue with cassandra
            }

            // Step 2: check that the id2info is parsable
            unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     cache_id2_info;
            // false -> do not finish the request
            if (!x_ParseTSEChunkId2Info(blob_record->GetId2Info(),
                                        cache_id2_info,
                                        m_IdModVerId2Info->GetTSEId(),
                                        false)) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                break;  // Continue with cassandra
            }

            // Step 3: check the split version in cache
            if (cache_id2_info->GetSplitVersion() != m_IdModVerId2Info->GetSplitVersion()) {
                app->GetRequestCounters().IncTSEChunkSplitVersionCacheNotMatched();
                PSG_WARNING("Blob " + m_IdModVerId2Info->GetTSEId().ToString() +
                            " split version in cache does not match the requested one");
                break;  // Continue with cassandra
            }

            app->GetRequestCounters().IncTSEChunkSplitVersionCacheMatched();

            // Step 4: validate the chunk number
            if (!x_ValidateTSEChunkNumber(m_TSEChunkRequest->m_Id2Chunk,
                                          cache_id2_info->GetChunks(), false)) {
                break; // Continue with cassandra
            }

            // Step 5: For the target chunk - convert sat to sat name
            // Chunk's blob id
            int64_t         sat_key;
            if (m_TSEChunkRequest->m_Id2Chunk == kSplitInfoChunk) {
                // Special case
                sat_key = cache_id2_info->GetInfo();
            } else {
                // For the target chunk - convert sat to sat name chunk's blob id
                sat_key = cache_id2_info->GetInfo() -
                          cache_id2_info->GetChunks() - 1 +
                          m_TSEChunkRequest->m_Id2Chunk;
            }
            SCass_BlobId    chunk_blob_id(cache_id2_info->GetSat(), sat_key);
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
                                      vector<string>(), vector<string>(),
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
        err_msg = "Blob " + m_IdModVerId2Info->GetTSEId().ToString() +
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
    fetch_details.reset(new CCassSplitHistoryFetch(*m_TSEChunkRequest,
                                                   m_IdModVerId2Info->GetTSEId(),
                                                   m_IdModVerId2Info->GetSplitVersion()));
    CCassBlobTaskFetchSplitHistory *   load_task =
        new  CCassBlobTaskFetchSplitHistory(app->GetCassandraTimeout(),
                                            app->GetCassandraMaxRetries(),
                                            app->GetCassandraConnection(),
                                            m_IdModVerId2Info->GetTSEId().m_Keyspace,
                                            m_IdModVerId2Info->GetTSEId().m_SatKey,
                                            m_IdModVerId2Info->GetSplitVersion(),
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


void CPSGS_TSEChunkProcessor::x_ProcessSatInfoChunkVerId2Info(void)
{
    // This option is when id2info came in a shape of sat.info.chunks[.ver]

    auto    app = CPubseqGatewayApp::GetInstance();
    string  err_msg;

    // Note: the TSE id (blob id) is not used in the chunk retrieval.
    //       So there is no need to map its sat to keyspace. The TSE id
    //       will be sent to the client as as.
    // The user provided id2_info is used to calculate the chunk blob_id
    // so the sat from id2_info will be mapped to the cassandra keyspace

    // Validate the chunk number
    // true -> finish if failed
    if (!x_ValidateTSEChunkNumber(m_TSEChunkRequest->m_Id2Chunk,
                                  m_SatInfoChunkVerId2Info->GetChunks(),
                                  true))
        return;

    int64_t         sat_key;
    if (m_TSEChunkRequest->m_Id2Chunk == kSplitInfoChunk) {
        // Special case
        sat_key = m_SatInfoChunkVerId2Info->GetInfo();
    } else {
        // For the target chunk - convert sat to sat name chunk's blob id
        sat_key = m_SatInfoChunkVerId2Info->GetInfo() -
                  m_SatInfoChunkVerId2Info->GetChunks() - 1 + m_TSEChunkRequest->m_Id2Chunk;
    }
    SCass_BlobId    chunk_blob_id(m_SatInfoChunkVerId2Info->GetSat(), sat_key);
    if (!x_TSEChunkSatToKeyspace(chunk_blob_id))
        return;

    // Search in cache the TSE chunk properties
    CPSGCache                   psg_cache(IPSGS_Processor::m_Request,
                                          IPSGS_Processor::m_Reply);
    int64_t                     last_modified = INT64_MIN;
    unique_ptr<CBlobRecord>     blob_record(new CBlobRecord);
    auto                        tse_blob_prop_cache_lookup_result =
        psg_cache.LookupBlobProp(chunk_blob_id.m_Sat, chunk_blob_id.m_SatKey,
                                 last_modified, *blob_record.get());


    // Initiate the chunk request
    SPSGS_RequestBase::EPSGS_Trace  trace_flag = SPSGS_RequestBase::ePSGS_NoTracing;
    if (IPSGS_Processor::m_Request->NeedTrace())
        trace_flag = SPSGS_RequestBase::ePSGS_WithTracing;
    SPSGS_BlobBySatSatKeyRequest
                        chunk_request(SPSGS_BlobId(chunk_blob_id.ToString()),
                                      INT64_MIN,
                                      SPSGS_BlobRequestBase::ePSGS_UnknownTSE,
                                      SPSGS_RequestBase::ePSGS_UnknownUseCache,
                                      "", 0, trace_flag,
                                      vector<string>(), vector<string>(),
                                      chrono::high_resolution_clock::now());

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(chunk_request, chunk_blob_id));

    CCassBlobTaskLoadBlob *         load_task = nullptr;
    if (tse_blob_prop_cache_lookup_result != ePSGS_CacheHit) {
        // Cassandra should look for blob props as well
        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              chunk_blob_id.m_Keyspace,
                                              chunk_blob_id.m_SatKey,
                                              true, nullptr);
    } else {
        // Blob props are already here
        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              chunk_blob_id.m_Keyspace,
                                              move(blob_record),
                                              true, nullptr);
    }

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
}


void CPSGS_TSEChunkProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                            CBlobRecord const &  blob,
                                            bool is_found)
{
    // Note: cannot use CPSGS_CassBlobBase::OnGetBlobChunk() anymore because
    // the reply has to have a few more fields for ID/get_tse_chunk request
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                "Blob prop callback; found: " + to_string(is_found),
                IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    if (is_found) {
        IPSGS_Processor::m_Reply->PrepareTSEBlobPropData(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk,
                m_TSEChunkRequest->m_Id2Info,
                ToJson(blob).Repr(CJsonNode::fStandardJson));
        IPSGS_Processor::m_Reply->PrepareTSEBlobPropCompletion(
                fetch_details, GetName());
    } else {
        // Not found; it is the user error, not the data inconsistency
        auto *  app = CPubseqGatewayApp::GetInstance();
        app->GetErrorCounters().IncBlobPropsNotFoundError();

        auto    blob_id = fetch_details->GetBlobId();
        string  message = "Blob " + blob_id.ToString() + " properties are not found";
        PSG_WARNING(message);
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        IPSGS_Processor::m_Reply->PrepareTSEBlobPropMessage(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                message, CRequestStatus::e404_NotFound,
                ePSGS_BlobPropsNotFound, eDiag_Error);
        IPSGS_Processor::m_Reply->PrepareTSEBlobPropCompletion(
                fetch_details, GetName());
        SetFinished(fetch_details);
    }

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_TSEChunkProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
                                             CRequestStatus::ECode  status,
                                             int  code,
                                             EDiagSev  severity,
                                             const string &  message)
{
    // Cannot use CPSGS_CassBlobBase::OnGetBlobError() anymore because
    // the TSE messages have different parameters

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = CountError(status, code, severity, message);

    if (is_error) {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        if (fetch_details->IsBlobPropStage()) {
            IPSGS_Processor::m_Reply->PrepareTSEBlobPropMessage(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                message, CRequestStatus::e500_InternalServerError, code, severity);
            IPSGS_Processor::m_Reply->PrepareTSEBlobPropCompletion(
                fetch_details, GetName());
        } else {
            IPSGS_Processor::m_Reply->PrepareTSEBlobMessage(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                message, CRequestStatus::e500_InternalServerError, code, severity);
            IPSGS_Processor::m_Reply->PrepareTSEBlobCompletion(
                fetch_details, GetName());
        }

        // If it is an error then regardless what stage it was, props or
        // chunks, there will be no more activity
        fetch_details->SetReadFinished();
    } else {
        if (fetch_details->IsBlobPropStage())
            IPSGS_Processor::m_Reply->PrepareTSEBlobPropMessage(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                message, status, code, severity);
        else
            IPSGS_Processor::m_Reply->PrepareTSEBlobMessage(
                fetch_details, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                message, status, code, severity);
    }

    SetFinished(fetch_details);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_TSEChunkProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                                             CBlobRecord const &  blob,
                                             const unsigned char *  chunk_data,
                                             unsigned int  data_size,
                                             int  chunk_no)
{
    // Note: cannot use CPSGS_CassBlobBase::OnGetBlobChunk() anymore because
    // the reply has to have a few more fields for TSE chunks
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        SetFinished(fetch_details);
        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    }
    if (IPSGS_Processor::m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().
                                                     IncUnknownError();
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");
        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    }

    if (chunk_no >= 0) {
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                    "Blob chunk " + to_string(chunk_no) + " callback",
                    IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        // A blob chunk; 0-length chunks are allowed too
        IPSGS_Processor::m_Reply->PrepareTSEBlobData(
                fetch_details, GetName(),
                chunk_data, data_size, chunk_no,
                m_TSEChunkRequest->m_Id2Chunk,
                m_TSEChunkRequest->m_Id2Info);
    } else {
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                    "Blob chunk no-more-data callback",
                    IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        // End of the blob
        IPSGS_Processor::m_Reply->PrepareTSEBlobCompletion(
                fetch_details, GetName());
        SetFinished(fetch_details);

        // Note: no need to set the blob completed in the exclude blob cache.
        // It will happen in Peek()
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
CPSGS_TSEChunkProcessor::x_RequestTSEChunk(
                                    const SSplitHistoryRecord &  split_record,
                                    CCassSplitHistoryFetch *  fetch_details)
{
    // Parse id2info
    unique_ptr<CPSGS_SatInfoChunksVerFlavorId2Info>     id2_info;
    if (!x_ParseTSEChunkId2Info(split_record.id2_info,
                                id2_info, fetch_details->GetTSEId(), true))
        return;

    // Check the requested chunk
    // true -> finish the request if failed
    if (!x_ValidateTSEChunkNumber(fetch_details->GetChunk(),
                                  id2_info->GetChunks(), true))
        return;

    // Resolve sat to satkey
    int64_t         sat_key;
    if (fetch_details->GetChunk() == kSplitInfoChunk) {
        // Special case
        sat_key = id2_info->GetInfo();
    } else {
        sat_key = id2_info->GetInfo() - id2_info->GetChunks() - 1 +
                  fetch_details->GetChunk();
    }
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
        UpdateOverallStatus(CRequestStatus::e404_NotFound);
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
                      "", 0, trace_flag,
                      vector<string>(), vector<string>(),
                      chrono::high_resolution_clock::now());
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
                                CPSGS_SatInfoChunksVerFlavorId2Info::TChunks  total_chunks,
                                bool  need_finish)
{
    if (requested_chunk == kSplitInfoChunk) {
        // Special value: the info chunk must be provided
        return true;
    }

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
            UpdateOverallStatus(CRequestStatus::e400_BadRequest);
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
CPSGS_TSEChunkProcessor::x_TSEChunkSatToKeyspace(SCass_BlobId &  blob_id)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    if (app->SatToKeyspace(blob_id.m_Sat, blob_id.m_Keyspace))
        return true;

    app->GetErrorCounters().IncClientSatToSatName();

    string  msg = "Unknown TSE chunk satellite number " +
                  to_string(blob_id.m_Sat) +
                  " for the blob " + blob_id.ToString();
    x_SendProcessorError(msg, CRequestStatus::e500_InternalServerError,
                         ePSGS_UnknownResolvedSatellite);

    // This method is used only in case of the TSE chunk requests.
    // So in case of errors - synchronous or asynchronous - it is
    // necessary to finish the reply anyway.
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
    return false;
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
    return "Cassandra-gettsechunk";
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
            IPSGS_Processor::m_Reply->PrepareTSEBlobPropMessage(
                blob_fetch, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);
            IPSGS_Processor::m_Reply->PrepareTSEBlobPropCompletion(
                    blob_fetch, GetName());
        } else {
            IPSGS_Processor::m_Reply->PrepareTSEBlobMessage(
                blob_fetch, GetName(),
                m_TSEChunkRequest->m_Id2Chunk, m_TSEChunkRequest->m_Id2Info,
                error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);
            IPSGS_Processor::m_Reply->PrepareTSEBlobCompletion(
                blob_fetch, GetName());
        }

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    }
}

