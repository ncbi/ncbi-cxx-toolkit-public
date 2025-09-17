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

static const string   kGetProcessorName = "Cassandra-get";


CPSGS_GetProcessor::CPSGS_GetProcessor() :
    m_BlobRequest(nullptr)
{}


CPSGS_GetProcessor::CPSGS_GetProcessor(shared_ptr<CPSGS_Request> request,
                                       shared_ptr<CPSGS_Reply> reply,
                                       TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_GetProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_GetProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4),
                      bind(&CPSGS_GetProcessor::x_OnResolutionGoodData,
                           this)),
    CPSGS_CassBlobBase(request, reply, kGetProcessorName,
                       bind(&CPSGS_GetProcessor::OnGetBlobProp,
                            this, _1, _2, _3),
                       bind(&CPSGS_GetProcessor::OnGetBlobChunk,
                            this, _1, _2, _3, _4, _5),
                       bind(&CPSGS_GetProcessor::OnGetBlobError,
                            this, _1, _2, _3, _4, _5))
{
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
{
    CleanupMyNCBICache();
}


bool
CPSGS_GetProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                               shared_ptr<CPSGS_Reply> reply) const
{
    if (!IsCassandraProcessorEnabled(request))
        return false;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_BlobBySeqIdRequest)
        return false;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace(kGetProcessorName + " processor cannot process "
                             "request because Cassandra DB is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return false;
    }

    return true;
}


IPSGS_Processor*
CPSGS_GetProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority  priority) const
{
    if (!CanProcess(request, reply))
        return nullptr;

    return new CPSGS_GetProcessor(request, reply, priority);
}


void CPSGS_GetProcessor::Process(void)
{
    // Lock the request for all the cassandra processors so that the other
    // processors may wait on the event
    IPSGS_Processor::m_Request->Lock(kCassandraProcessorEvent);

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
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    EPSGS_LoggingFlag           logging_flag = ePSGS_NeedLogging;
    if (status == CRequestStatus::e404_NotFound)
        logging_flag = ePSGS_SkipLogging;
    CountError(nullptr, status, code, severity, message,
               logging_flag, ePSGS_NeedStatusUpdate);

    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, kGetProcessorName,
                                                   message, status,
                                                   code, severity);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id,
                                                      kGetProcessorName, 2);

    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


// This callback is called only in case of a valid resolution
void
CPSGS_GetProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    try {
        // Base class may need the resolved seq id so memorize it
        auto &  bioseq_info_record = bioseq_resolution.GetBioseqInfo();
        m_ResolvedSeqID.Set(CSeq_id_Base::E_Choice(bioseq_info_record.GetSeqIdType()),
                            bioseq_info_record.GetAccession(),
                            bioseq_info_record.GetName(),
                            bioseq_info_record.GetVersion());
    } catch (const exception &  exc) {
        PSG_WARNING("Cannot build CSeq_id out of the resolved input seq_id "
                    "due to: " + string(exc.what()));
    } catch (...) {
        PSG_WARNING("Cannot build CSeq_id out of the resolved input seq_id "
                    "due to unknown reason");
    }

    x_SendBioseqInfo(bioseq_resolution);

    // Translate sat to keyspace
    // In case of errors it:
    // - sends a message to the client
    // - sets the processor return code
    // - signals that the processor finished
    m_BlobId = TranslateSatToKeyspace(bioseq_resolution.GetBioseqInfo().GetSat(),
                                      bioseq_resolution.GetBioseqInfo().GetSatKey(),
                                      m_BlobRequest->m_SeqId);
    if (m_BlobId.IsValid()) {
        x_GetBlob();
    }
}


void
CPSGS_GetProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache) {
        AdjustBioseqAccession(bioseq_resolution);
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
        bioseq_resolution.AdjustName(IPSGS_Processor::m_Request,
                                     IPSGS_Processor::m_Reply);
    }

    size_t  item_id = IPSGS_Processor::m_Reply->GetItemId();
    auto    data_to_send = ToJsonString(bioseq_resolution.GetBioseqInfo(),
                                        SPSGS_ResolveRequest::fPSGS_AllBioseqFields);

    // Data chunk and meta chunk are merged into one
    IPSGS_Processor::m_Reply->PrepareBioseqDataAndCompletion(
            item_id, kGetProcessorName, data_to_send,
            SPSGS_ResolveRequest::ePSGS_JsonFormat, 1);
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
    if (x_IsExcludedBlob()) {
        IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                IPSGS_Processor::m_Reply->GetItemId(), kGetProcessorName,
                m_BlobId.ToString(), ePSGS_BlobExcluded);
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    auto *  app = CPubseqGatewayApp::GetInstance();

    bool    need_to_check_blob_exclude_cache =
        !m_BlobRequest->m_ClientId.empty() &&   // cache is per client
        m_BlobRequest->m_ResendTimeoutMks > 0;  // resend_timeout == 0 is switching off blob skipping

    // Note: checking only if the blob is in cache. The cache insert is done in
    // a common code for the blob retrieval later (see
    // CPSGS_CassBlobBase::x_CheckExcludeBlobCache)

    if (need_to_check_blob_exclude_cache) {
        bool                completed = true;
        psg_time_point_t    completed_time;
        if (app->GetExcludeBlobCache()->IsInCache(
                    m_BlobRequest->m_ClientId,
                    SExcludeBlobId(m_BlobId.m_Sat, m_BlobId.m_SatKey),
                    completed, completed_time)) {

            bool    finish_processing = true;

            if (completed) {
                // It depends how long ago it was sent; if too long then
                // send anyway; otherwise send a reply specifying how long
                // ago it was sent
                // Special case: if the effective resend timeout == 0.0
                // then the blob needs to be sent right away
                unsigned long  sent_mks_ago = GetTimespanToNowMks(completed_time);
                if (m_BlobRequest->m_ResendTimeoutMks > 0 &&
                    sent_mks_ago < m_BlobRequest->m_ResendTimeoutMks) {
                    // No sending; the blob was send recent enough
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                            m_BlobId.ToString(), kGetProcessorName,
                            sent_mks_ago,
                            m_BlobRequest->m_ResendTimeoutMks - sent_mks_ago);
                } else {
                    // Sending anyway; it was longer than the resend
                    // timeout
                    // The easiest way to achieve that is to remove the
                    // blob from the exclude cache. So the code in the base
                    // class will add this blob to the cache again as new
                    app->GetExcludeBlobCache()->Remove(
                                            m_BlobRequest->m_ClientId,
                                            SExcludeBlobId(m_BlobId.m_Sat, m_BlobId.m_SatKey));
                    finish_processing = false;
                }
            } else {
                IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                        m_BlobId.ToString(), kGetProcessorName,
                        ePSGS_BlobInProgress);
            }

            if (finish_processing) {
                CPSGS_CassProcessorBase::SignalFinishProcessing();
                return;
            }
        }
    }

    if (m_BlobId.m_IsSecureKeyspace.value()) {
        auto    populate_result = PopulateMyNCBIUser(
                                  bind(&CPSGS_GetProcessor::x_OnMyNCBIData,
                                       this, _1, _2),
                                  bind(&CPSGS_GetProcessor::x_OnMyNCBIError,
                                       this, _1, _2, _3, _4, _5));
        switch (populate_result) {
            case CPSGS_CassBlobBase::ePSGS_FoundInOKCache:
                // The user name has been populated so just continue
                break;
            case CPSGS_CassBlobBase::ePSGS_IncludeHUPSetToNo:
            case CPSGS_CassBlobBase::ePSGS_CookieNotPresent:
                CPSGS_CassProcessorBase::SignalFinishProcessing();
                return;
            case CPSGS_CassBlobBase::ePSGS_FoundInErrorCache:
            case CPSGS_CassBlobBase::ePSGS_FoundInNotFoundCache:
                // The error handlers have been called while checking the caches.
                // The error handlers called SignalFinishProcessing() so just
                // return.
                return;
            case CPSGS_CassBlobBase::ePSGS_AddedToWaitlist:
            case CPSGS_CassBlobBase::ePSGS_RequestInitiated:
                // Wait for a callback which comes from cache or from the my
                // ncbi access wrapper asynchronously
                return;
            default:
                break;  // Cannot happened
        }
    }

    x_GetBlobFinalStage();
}



void CPSGS_GetProcessor::x_GetBlobFinalStage(void)
{
    // The user name check is done when a connection is requested
    shared_ptr<CCassConnection>     cass_connection;
    try {
        if (m_BlobId.m_IsSecureKeyspace.value()) {
            cass_connection = m_BlobId.m_Keyspace->GetSecureConnection(
                                                m_UserName.value());
            if (!cass_connection) {
                ReportSecureSatUnauthorized(m_UserName.value());
                CPSGS_CassProcessorBase::SignalFinishProcessing();

                if (IPSGS_Processor::m_Reply->IsOutputReady())
                    x_Peek(false);
                return;
            }
        } else {
            cass_connection = m_BlobId.m_Keyspace->GetConnection();
        }
    } catch (const exception &  exc) {
        ReportFailureToGetCassConnection(exc.what());
        CPSGS_CassProcessorBase::SignalFinishProcessing();

        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    } catch (...) {
        ReportFailureToGetCassConnection();
        CPSGS_CassProcessorBase::SignalFinishProcessing();

        if (IPSGS_Processor::m_Reply->IsOutputReady())
            x_Peek(false);
        return;
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(*m_BlobRequest, m_BlobId));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(IPSGS_Processor::m_Request,
                                      IPSGS_Processor::m_Reply);
    int64_t                 last_modified = INT64_MIN;
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        this,
                                        m_BlobId.m_Sat,
                                        m_BlobId.m_SatKey,
                                        last_modified,
                                        *blob_record.get());

    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        load_task = new CCassBlobTaskLoadBlob(cass_connection,
                                              m_BlobId.m_Keyspace->keyspace,
                                              std::move(blob_record),
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    } else {
        if (m_BlobRequest->m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
            // No data in cache and not going to the DB
            size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
            auto        ret_status = CRequestStatus::e404_NotFound;
            if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit) {
                IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
                    item_id, kGetProcessorName,
                    "Blob properties are not found",
                    ret_status, ePSGS_NoBlobPropsError,
                    eDiag_Error);
            } else {
                ret_status = CRequestStatus::e500_InternalServerError;
                IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
                    item_id, kGetProcessorName,
                    "Blob properties are not found due to a cache lookup error",
                    ret_status, ePSGS_NoBlobPropsError,
                    eDiag_Error);
            }
            IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(item_id,
                                                                kGetProcessorName,
                                                                2);
            fetch_details->RemoveFromExcludeBlobCache();

            // Finished without reaching cassandra
            UpdateOverallStatus(ret_status);
            CPSGS_CassProcessorBase::SignalFinishProcessing();
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(cass_connection,
                                              m_BlobId.m_Keyspace->keyspace,
                                              m_BlobId.m_SatKey,
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    }

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(this,
                              bind(&CPSGS_GetProcessor::OnGetBlobError,
                                   this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    load_task->SetPropsCallback(
        CBlobPropCallback(this,
                          bind(&CPSGS_GetProcessor::OnGetBlobProp,
                               this, _1, _2, _3),
                          IPSGS_Processor::m_Request,
                          IPSGS_Processor::m_Reply,
                          fetch_details.get(),
                          blob_prop_cache_lookup_result != ePSGS_CacheHit));

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
                            "Cassandra request: " +
                            ToJsonString(*load_task),
                            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(fetch_details));

    // Initiate cassandra request
    load_task->Wait();
}


void CPSGS_GetProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                       CBlobRecord const &  blob,
                                       bool is_found)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CPSGS_CassBlobBase::OnGetBlobProp(fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
                                        CRequestStatus::ECode  status,
                                        int  code,
                                        EDiagSev  severity,
                                        const string &  message)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

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
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CPSGS_CassBlobBase::OnGetBlobChunk(m_Canceled, fetch_details,
                                       chunk_data, data_size, chunk_no);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


IPSGS_Processor::EPSGS_Status CPSGS_GetProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Canceled)
        return IPSGS_Processor::ePSGS_Canceled;

    return status;
}


string CPSGS_GetProcessor::GetName(void) const
{
    return kGetProcessorName;
}


string CPSGS_GetProcessor::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}


void CPSGS_GetProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_GetProcessor::x_Peek(bool  need_wait)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    /* bool        overall_final_state = false; */

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto &  details: m_FetchDetails) {
            if (details) {
                if (details->InPeek()) {
                    continue;
                }
                details->SetInPeek(true);
                /* overall_final_state |= */ x_Peek(details, need_wait);
                details->SetInPeek(false);
            }
        }

        if (initial_size == m_FetchDetails.size()) {
            break;
        }
    }

    // Blob specific: ready packets need to be sent right away
    IPSGS_Processor::m_Reply->Flush(CPSGS_Reply::ePSGS_SendAccumulated);

    // Blob specific: deal with exclude blob cache
    if (AreAllFinishedRead() && IsMyNCBIFinished()) {
        for (auto &  details: m_FetchDetails) {
            if (details) {
                // Update the cache records where needed
                details->SetExcludeBlobCacheCompleted();
            }
        }
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    }
}


bool CPSGS_GetProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return true;

    bool    final_state = false;
    if (need_wait)
        if (!fetch_details->ReadFinished()) {
            final_state = fetch_details->GetLoader()->Wait();
            if (final_state) {
                fetch_details->SetReadFinished();
            }
        }

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetCounters().Increment(this, CPSGSCounters::ePSGS_ProcUnknownError);
        PSG_ERROR(error);

        CCassBlobFetch *  blob_fetch = static_cast<CCassBlobFetch *>(fetch_details.get());
        PrepareServerErrorMessage(blob_fetch, ePSGS_UnknownError, eDiag_Error, error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    }

    return final_state;
}


void CPSGS_GetProcessor::x_OnResolutionGoodData(void)
{
    // The resolution process started to receive data which look good so
    // the dispatcher should be notified that the other processors can be
    // stopped
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    if (SignalStartProcessing() == EPSGS_StartProcessing::ePSGS_Cancel) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    }

    // If the other processor waits then let it go but after sending the signal
    // of the good data (it may cancel the other processors)
    UnlockWaitingProcessor();
}


void CPSGS_GetProcessor::x_OnMyNCBIError(const string &  cookie,
                                         CRequestStatus::ECode  status,
                                         int  code,
                                         EDiagSev  severity,
                                         const string &  message)
{
    if (status == CRequestStatus::e404_NotFound) {
        ReportMyNCBINotFound();
    } else {
        ReportMyNCBIError(status, message);
    }

    CPSGS_CassProcessorBase::SignalFinishProcessing();

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_GetProcessor::x_OnMyNCBIData(const string &  cookie,
                                        CPSG_MyNCBIRequest_WhoAmI::SUserInfo user_info)
{
    // All good, can proceed
    m_UserName = user_info.username;
    x_GetBlobFinalStage();
}

