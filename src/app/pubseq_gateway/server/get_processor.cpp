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

#include "get_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "get_blob_callback.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;


CPSGS_GetProcessor::CPSGS_GetProcessor() :
    m_BlobRequest(nullptr),
    m_Cancelled(false)
{}


CPSGS_GetProcessor::CPSGS_GetProcessor(shared_ptr<CPSGS_Request> request,
                                       shared_ptr<CPSGS_Reply> reply,
                                       TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_GetProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_GetProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4)),
    CPSGS_CassBlobBase(request, reply, GetName()),
    m_Cancelled(false)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;
    IPSGS_Processor::m_Priority = priority;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>() everywhere
    m_BlobRequest = & request->GetRequest<SPSGS_BlobBySeqIdRequest>();

    // Convert generic excluded blobs into cassandra blob ids
    for (const auto &  blob_id_as_str : m_BlobRequest->m_ExcludeBlobs) {
        SCass_BlobId    cass_blob_id(blob_id_as_str);
        if (cass_blob_id.IsValid())
            m_ExcludeBlobs.push_back(cass_blob_id);
    }
}


CPSGS_GetProcessor::~CPSGS_GetProcessor()
{}


IPSGS_Processor*
CPSGS_GetProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() == CPSGS_Request::ePSGS_BlobBySeqIdRequest)
        return new CPSGS_GetProcessor(request, reply, priority);
    return nullptr;
}


void CPSGS_GetProcessor::Process(void)
{
    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_GetProcessor::x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    UpdateOverallStatus(status);
    PSG_WARNING(message);

    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    if (status == CRequestStatus::e404_NotFound) {
        IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, GetName(),
                                                       message, status,
                                                       ePSGS_NoBioseqInfo,
                                                       eDiag_Error);
    } else {
        IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, GetName(),
                                                       message, status,
                                                       ePSGS_BioseqInfoError,
                                                       severity);
    }
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, GetName(), 2);

    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
}


// This callback is called only in case of a valid resolution
void
CPSGS_GetProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    // Translate sat to keyspace
    auto *          app = CPubseqGatewayApp::GetInstance();
    SCass_BlobId    blob_id(bioseq_resolution.m_BioseqInfo.GetSat(),
                            bioseq_resolution.m_BioseqInfo.GetSatKey());
    if (app->SatToKeyspace(blob_id.m_Sat, blob_id.m_Keyspace)) {
        m_BlobId = blob_id;
        x_GetBlob();
        return;
    }

    // It is an error of the sat to keyspace translation
    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    string      msg = "Unknown satellite number " + to_string(blob_id.m_Sat) +
                      " for bioseq info with seq_id '" +
                      m_BlobRequest->m_SeqId + "'";
    app->GetErrorCounters().IncServerSatToSatName();

    IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
        item_id, GetName(), msg, CRequestStatus::e500_InternalServerError,
        ePSGS_UnknownResolvedSatellite, eDiag_Error);
    IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(item_id, GetName(), 2);

    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);

    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
}


void
CPSGS_GetProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);

    size_t  item_id = IPSGS_Processor::m_Reply->GetItemId();
    auto    data_to_send = ToJson(bioseq_resolution.m_BioseqInfo,
                                  SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                                        Repr(CJsonNode::fStandardJson);

    IPSGS_Processor::m_Reply->PrepareBioseqData(
            item_id, GetName(), data_to_send,
            SPSGS_ResolveRequest::ePSGS_JsonFormat);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, GetName(), 2);
}


bool CPSGS_GetProcessor::x_IsExcludedBlob(void) const
{
    for (const auto & item : m_ExcludeBlobs) {
        if (item == m_BlobId)
            return true;
    }
    return false;
}


void CPSGS_GetProcessor::x_GetBlob(void)
{
    auto * app = CPubseqGatewayApp::GetInstance();

    if (x_IsExcludedBlob()) {
        IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                IPSGS_Processor::m_Reply->GetItemId(), GetName(),
                m_BlobId.ToString(), ePSGS_BlobExcluded);
        m_Completed = true;
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
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
            if (cache_result == ePSGS_AlreadyInCache &&
                m_BlobRequest->m_AutoBlobSkipping) {
                if (completed)
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                                    m_BlobId.ToString(), GetName(),
                                    ePSGS_BlobSent);
                else
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                                    m_BlobId.ToString(), GetName(),
                                    ePSGS_BlobInProgress);
                m_Completed = true;
                IPSGS_Processor::m_Reply->SignalProcessorFinished();
                return;
            }

            if (cache_result == ePSGS_Added)
                m_BlobRequest->m_ExcludeBlobCacheAdded = true;
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(*m_BlobRequest, m_BlobId));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(IPSGS_Processor::m_Request,
                                      IPSGS_Processor::m_Reply);
    int64_t                 last_modified = INT64_MIN;
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        m_BlobId.m_Sat,
                                        m_BlobId.m_SatKey,
                                        last_modified,
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
            IPSGS_Processor::m_Reply->SignalProcessorFinished();
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              m_BlobId.m_Keyspace,
                                              m_BlobId.m_SatKey,
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    }

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPSGS_GetProcessor::OnGetBlobError,
                                   this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(bind(&CPSGS_GetProcessor::OnGetBlobProp,
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


void CPSGS_GetProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  blob,
                                       bool is_found)
{
    CPSGS_CassBlobBase::OnGetBlobProp(bind(&CPSGS_GetProcessor::OnGetBlobProp,
                                           this, _1, _2, _3),
                                      bind(&CPSGS_GetProcessor::OnGetBlobChunk,
                                           this, _1, _2, _3, _4, _5),
                                      bind(&CPSGS_GetProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
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


void CPSGS_GetProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
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


void CPSGS_GetProcessor::Cancel(void)
{
    m_Cancelled = true;
}


IPSGS_Processor::EPSGS_Status CPSGS_GetProcessor::GetStatus(void)
{
    return CPSGS_CassProcessorBase::GetStatus();
}


string CPSGS_GetProcessor::GetName(void) const
{
    return "Cassandra-get";
}


void CPSGS_GetProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_GetProcessor::x_Peek(bool  need_wait)
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

    // Blob specific: ready packets need to be sent right away
    if (IPSGS_Processor::m_Reply->IsOutputReady())
        IPSGS_Processor::m_Reply->Flush(false);

    // Blob specific: deal with exclude blob cache
    if (AreAllFinishedRead()) {
        // The handler deals with both kind of blob requests:
        // - by sat/sat_key
        // - by seq_id/seq_id_type
        // So get the reference to the blob base request
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
}


void CPSGS_GetProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
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
        PrepareServerErrorMessage(blob_fetch, ePSGS_UnknownError, eDiag_Error, error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    }
}

