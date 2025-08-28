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
 * File Description: named annotation processor
 *
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/regexp.hpp>

#include "annot_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "named_annot_callback.hpp"
#include "psgs_seq_id_utils.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;

static const string   kAnnotProcessorName = "Cassandra-getna";


CPSGS_AnnotProcessor::CPSGS_AnnotProcessor() :
    m_AnnotRequest(nullptr), m_BlobStage(false)
{}


CPSGS_AnnotProcessor::CPSGS_AnnotProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_AnnotProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_AnnotProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4),
                      bind(&CPSGS_AnnotProcessor::x_OnResolutionGoodData,
                           this)),
    CPSGS_CassBlobBase(request, reply, kAnnotProcessorName,
                       bind(&CPSGS_AnnotProcessor::OnGetBlobProp,
                            this, _1, _2, _3),
                       bind(&CPSGS_AnnotProcessor::OnGetBlobChunk,
                            this, _1, _2, _3, _4, _5),
                       bind(&CPSGS_AnnotProcessor::OnGetBlobError,
                            this, _1, _2, _3, _4, _5)),
    m_BlobStage(false)
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_AnnotRequest>() everywhere
    m_AnnotRequest = & request->GetRequest<SPSGS_AnnotRequest>();
}


CPSGS_AnnotProcessor::~CPSGS_AnnotProcessor()
{}


bool
CPSGS_AnnotProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                 shared_ptr<CPSGS_Reply> reply) const
{
    if (!IsCassandraProcessorEnabled(request))
        return false;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_AnnotationRequest)
        return false;

    auto    valid_annots = x_FilterNames(request->GetRequest<SPSGS_AnnotRequest>().m_Names);
    if (valid_annots.empty())
        return false;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace(kAnnotProcessorName + " processor cannot process "
                             " request because Cassandra DB is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return false;
    }

    return true;
}


vector<string>
CPSGS_AnnotProcessor::WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply) const
{
    return x_FilterNames(request->GetRequest<SPSGS_AnnotRequest>().m_Names);
}


IPSGS_Processor*
CPSGS_AnnotProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                      shared_ptr<CPSGS_Reply> reply,
                                      TProcessorPriority  priority) const
{
    if (!CanProcess(request, reply))
        return nullptr;

    return new CPSGS_AnnotProcessor(request, reply, priority);
}


vector<string>
CPSGS_AnnotProcessor::x_FilterNames(const vector<string> &  names)
{
    vector<string>  valid_annots;
    for (const auto &  name : names) {
        if (x_IsNameValid(name))
            valid_annots.push_back(name);
    }
    return valid_annots;
}


static CRegexp  kAnnotNameRegexp("^NA\\d+\\.\\d+$", CRegexp::fCompile_ignore_case);
bool CPSGS_AnnotProcessor::x_IsNameValid(const string &  name)
{
    return kAnnotNameRegexp.IsMatch(name);
}


void CPSGS_AnnotProcessor::Process(void)
{
    // The other processors may have been triggered before this one.
    // So check if there are still not processed yet names which can be
    // processed by this processor
    auto    not_processed_names = m_AnnotRequest->GetNotProcessedName(m_Priority);
    m_ValidNames = x_FilterNames(not_processed_names);
    if (m_ValidNames.empty()) {
        UpdateOverallStatus(CRequestStatus::e200_Ok);
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    // Lock the request for all the cassandra processors so that the other
    // processors may wait on the event
    IPSGS_Processor::m_Request->Lock(kCassandraProcessorEvent);

    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_AnnotProcessor::x_OnSeqIdResolveError(
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

    // Report all the 'accepted' annotations with the corresponding result code
    SPSGS_AnnotRequest::EPSGS_ResultStatus  rs = SPSGS_AnnotRequest::ePSGS_RS_NotFound;
    switch (m_Status) {
        case CRequestStatus::e504_GatewayTimeout:
            rs = SPSGS_AnnotRequest::ePSGS_RS_Timeout;
            break;
        case CRequestStatus::e404_NotFound:
            rs = SPSGS_AnnotRequest::ePSGS_RS_NotFound;
            break;
        default:
            rs = SPSGS_AnnotRequest::ePSGS_RS_Error;
            break;
    }
    for (const auto &  name : m_ValidNames) {
        m_AnnotRequest->ReportResultStatus(name, rs);
    }

    // Send a chunk to the client
    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, kAnnotProcessorName,
                                                   message, status, code,
                                                   severity);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(
                                            item_id, kAnnotProcessorName, 2);

    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


// This callback is called only in case of a valid resolution
void
CPSGS_AnnotProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    // Initiate annotation request
    auto *                          app = CPubseqGatewayApp::GetInstance();
    vector<SSatInfoEntry>           bioseq_na_keyspaces = app->GetBioseqNAKeyspaces();

    for (const auto &  bioseq_na_keyspace : bioseq_na_keyspaces) {
        unique_ptr<CCassNamedAnnotFetch>   details;
        details.reset(new CCassNamedAnnotFetch(*m_AnnotRequest));

        // Note: the accession and seq_id_type may be adjusted in the bioseq
        // info record. However the request must be done using the original
        // accession and seq_id_type
        string  accession = StripTrailingVerticalBars(bioseq_resolution.GetOriginalAccession());
        CCassNAnnotTaskFetch *  fetch_task =
                new CCassNAnnotTaskFetch(bioseq_na_keyspace.connection,
                                         bioseq_na_keyspace.keyspace,
                                         accession,
                                         bioseq_resolution.GetBioseqInfo().GetVersion(),
                                         bioseq_resolution.GetOriginalSeqIdType(),
                                         m_ValidNames,
                                         nullptr, nullptr);
        details->SetLoader(fetch_task);

        fetch_task->SetConsumeCallback(
            CNamedAnnotationCallback(
                this,
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotData,
                     this, _1, _2, _3, _4),
                details.get(), bioseq_na_keyspace.sat));
        fetch_task->SetErrorCB(
            CNamedAnnotationErrorCallback(
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotError,
                     this, _1, _2, _3, _4, _5),
                details.get()));
        fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
                ToJsonString(*fetch_task),
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        m_FetchDetails.push_back(std::move(details));
    }

    // Initiate the retrieval loop
    for (auto &  fetch_details: m_FetchDetails) {
        if (fetch_details)
            if (!fetch_details->ReadFinished()) {
                fetch_details->GetLoader()->Wait();
            }
    }
}


void
CPSGS_AnnotProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    auto    other_proc_priority = m_AnnotRequest->RegisterBioseqInfo(m_Priority);
    if (other_proc_priority == kUnknownPriority) {
        // Has not been processed yet at all
        // Fall through
    } else if (other_proc_priority < m_Priority) {
        // Was processed by a lower priority processor so it needs to be
        // send anyway
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Bioseq info has already been sent by the other processor. "
                "The data are to be sent because the other processor priority (" +
                to_string(other_proc_priority) + ") is lower than mine (" +
                to_string(m_Priority) + ")",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    } else {
        // The other processor has already processed it
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Skip sending bioseq info because the other processor with priority " +
                to_string(other_proc_priority) + " has already sent it "
                "(my priority is " + to_string(m_Priority) + ")",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
        return;
    }


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
            item_id, kAnnotProcessorName, data_to_send,
            SPSGS_ResolveRequest::ePSGS_JsonFormat, 1);
}


bool
CPSGS_AnnotProcessor::x_OnNamedAnnotData(CNAnnotRecord &&  annot_record,
                                         bool  last,
                                         CCassNamedAnnotFetch *  fetch_details,
                                         int32_t  sat)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        if (last) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Named annotation no-more-data callback",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        } else {
            IPSGS_Processor::m_Reply->SendTrace(
                "Named annotation data received",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    }

    if (m_Canceled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        return false;
    }
    if (IPSGS_Processor::m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                        this,
                                        CPSGSCounters::ePSGS_ProcUnknownError);
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");

        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return false;
    }

    // A responce has been succesfully received. Memorize it so that it can be
    // considered if some other keyspaces reply finish with errors or timeouts
    m_AnnotFetchCompletions.push_back(CRequestStatus::e200_Ok);

    if (last) {
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();

        // There could be many sat_name(s) requested so the callback is called
        // many times. If all of them finished then the completion should be
        // set to true. Otherwise the process of waiting for the other callback
        // should continue.
        if (AreAllFinishedRead()) {
            // Detect not found annotations
            for (const auto &  name: m_ValidNames) {
                if (m_Success.find(name) == m_Success.end()) {
                    m_AnnotRequest->ReportResultStatus(
                            name, SPSGS_AnnotRequest::ePSGS_RS_NotFound);
                }
            }

            CPSGS_CassProcessorBase::SignalFinishProcessing();
            return false;
        }

        // Not all finished so wait for more callbacks
        x_Peek(false);
        return true;
    }

    // Memorize a good annotation
    m_Success.insert(annot_record.GetAnnotName());

    x_SendAnnotDataToClient(std::move(annot_record), sat);
    x_Peek(false);
    return true;
}

void
CPSGS_AnnotProcessor::x_SendAnnotDataToClient(CNAnnotRecord &&  annot_record, int32_t  sat)
{
    auto    other_proc_priority = m_AnnotRequest->RegisterProcessedName(
                    m_Priority, annot_record.GetAnnotName());
    bool    annot_was_sent = false;
    if (other_proc_priority == kUnknownPriority ||
        other_proc_priority == m_Priority) {
        // 1. Has not been processed yet at all
        // 2. It was me who send the other one, i.e. it is multiple data items
        //    for the same annotation
        IPSGS_Processor::m_Reply->PrepareNamedAnnotationData(
            annot_record.GetAnnotName(), kAnnotProcessorName,
            ToJsonString(annot_record, sat));
        annot_was_sent = true;
    } else if (other_proc_priority < m_Priority) {
        // Was processed by a lower priority processor so it needs to be
        // send anyway
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "The NA name " + annot_record.GetAnnotName() + " has already "
                "been processed by the other processor. The data are to be sent"
                " because the other processor priority (" +
                to_string(other_proc_priority) + ") is lower than mine (" +
                to_string(m_Priority) + ")",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
        IPSGS_Processor::m_Reply->PrepareNamedAnnotationData(
            annot_record.GetAnnotName(), kAnnotProcessorName,
            ToJsonString(annot_record, sat));
        annot_was_sent = true;
    } else {
        // The other processor has already processed it
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Skip sending NA name " + annot_record.GetAnnotName() +
                " because the other processor with priority " +
                to_string(other_proc_priority) + " has already processed it "
                "(my priority is " + to_string(m_Priority) + ")",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    }

    if (annot_was_sent) {
        // May be the blob needs to be requested as well...
        if (x_NeedToRequestBlobProp())
            x_RequestBlobProp(sat, annot_record.GetSatKey(),
                              annot_record.GetModified());
    }
}


void
CPSGS_AnnotProcessor::x_OnNamedAnnotError(CCassNamedAnnotFetch *  fetch_details,
                                          CRequestStatus::ECode  status,
                                          int  code,
                                          EDiagSev  severity,
                                          const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    // It could be a message or an error
    CRequestStatus::ECode   fetch_completion =
            CountError(fetch_details, status, code, severity, message,
                       ePSGS_NeedLogging, ePSGS_SkipStatusUpdate);
    bool    is_error = IsError(severity);
    m_AnnotFetchCompletions.push_back(fetch_completion);

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(),
            kAnnotProcessorName, message, status, code, severity);

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    if (is_error) {
        // Report error/timeout to the request
        SPSGS_AnnotRequest::EPSGS_ResultStatus  result_status = SPSGS_AnnotRequest::ePSGS_RS_Error;
        if (fetch_completion == CRequestStatus::e504_GatewayTimeout)
            result_status = SPSGS_AnnotRequest::ePSGS_RS_Timeout;
        for (const auto &  name: m_ValidNames) {
            if (m_Success.find(name) == m_Success.end()) {
                m_AnnotRequest->ReportResultStatus(name, result_status);
            }
        }

        // There will be no more activity
        fetch_details->SetReadFinished();

        if (AreAllFinishedRead()) {
            // At least one completion has just been added to the vector
            CRequestStatus::ECode   best_status = m_AnnotFetchCompletions[0];
            for (auto  fetch_completion_status : m_AnnotFetchCompletions) {
                if (fetch_completion_status < best_status) {
                    best_status = fetch_completion_status;
                }
            }

            UpdateOverallStatus(best_status);
            CPSGS_CassProcessorBase::SignalFinishProcessing();
        }
    } else {
        x_Peek(false);
    }
}


bool CPSGS_AnnotProcessor::x_NeedToRequestBlobProp(void)
{
    // Check the tse option
    if (m_AnnotRequest->m_TSEOption != SPSGS_BlobRequestBase::ePSGS_SlimTSE &&
        m_AnnotRequest->m_TSEOption != SPSGS_BlobRequestBase::ePSGS_SmartTSE) {
        return false;
    }

    // Check the number of requested annotations.
    // The feature of retrieving the blob should work only if it is the only
    // one annotation
    if (m_AnnotRequest->m_Names.size() != 1) {
        return false;
    }

    // This is exactly one annotation requested;
    // it conforms to cassandra processor;
    // the tse option is slim or smart so the blob needs to be sent if the size
    // is small enough.
    // So, to know the size, let's request the blob properties
    return true;
}


void CPSGS_AnnotProcessor::x_RequestBlobProp(int32_t  sat, int32_t  sat_key,
                                             int64_t  last_modified)
{
    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Retrieve blob props for " + to_string(sat) + "." + to_string(sat_key) +
            " to check if the blob size is small (if so to send it right away).",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_AnnotRequest->m_BlobId = SPSGS_BlobId(sat, sat_key);
    SCass_BlobId    blob_id(m_AnnotRequest->m_BlobId.GetId());

    auto    app = CPubseqGatewayApp::GetInstance();
    if (!blob_id.MapSatToKeyspace()) {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        app->GetCounters().Increment(this,
                                     CPSGSCounters::ePSGS_ServerSatToSatNameError);

        string  err_msg = kAnnotProcessorName + " processor failed to map sat " +
                          to_string(blob_id.m_Sat) +
                          " to a Cassandra keyspace while requesting the blob props";
        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(), kAnnotProcessorName,
                err_msg, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownResolvedSatellite, eDiag_Error);
        PSG_WARNING(err_msg);

        // Annotation is considered as an errored because the promised data are
        // not supplied
        m_AnnotRequest->ReportBlobError(m_Priority,
                                        SPSGS_AnnotRequest::ePSGS_RS_Error);

        // It could be only one annotation and the blob prop retrieval happens
        // after sending the annotation so it is safe to say that the processor
        // is finished
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    bool    need_to_check_blob_exclude_cache =
        !m_AnnotRequest->m_ClientId.empty() &&   // cache is per client
        m_AnnotRequest->m_ResendTimeoutMks > 0;  // resend_timeout == 0 is switching off blob skipping

    // Checking only.
    // Here we are requesting blob props only so may be the blob itself will
    // not be requested at all. However if the blob is in cache then its blob
    // properties have already been sent for sure.
    if (need_to_check_blob_exclude_cache) {
        bool                completed = true;
        psg_time_point_t    completed_time;  // no need to check for the
                                             // annotation processor
        if (app->GetExcludeBlobCache()->IsInCache(
                    m_AnnotRequest->m_ClientId,
                    SExcludeBlobId(blob_id.m_Sat, blob_id.m_SatKey),
                    completed, completed_time)) {

            bool    finish_processing = true;

            if (completed) {
                // It depends how long ago it was sent; if too long then
                // send anyway; otherwise send a reply specifying how long
                // ago it was sent
                // Special case: if the effective resend timeout == 0.0
                // then the blob needs to be sent right away
                unsigned long  sent_mks_ago = GetTimespanToNowMks(completed_time);
                if (m_AnnotRequest->m_ResendTimeoutMks > 0 &&
                    sent_mks_ago < m_AnnotRequest->m_ResendTimeoutMks) {
                    // No sending; the blob was send recent enough
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                            blob_id.ToString(), kAnnotProcessorName,
                            sent_mks_ago,
                            m_AnnotRequest->m_ResendTimeoutMks - sent_mks_ago);
                } else {
                    // Sending anyway; it was longer than the resend
                    // timeout
                    // The easiest way to achieve that is to remove the
                    // blob from the exclude cache. So the code in the base
                    // class will add this blob to the cache again as new
                    app->GetExcludeBlobCache()->Remove(
                                            m_AnnotRequest->m_ClientId,
                                            SExcludeBlobId(blob_id.m_Sat, blob_id.m_SatKey));
                    finish_processing = false;
                }
            } else {
                IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                        blob_id.ToString(), kAnnotProcessorName,
                        ePSGS_BlobInProgress);
            }
            if (finish_processing) {
                CPSGS_CassProcessorBase::SignalFinishProcessing();
                return;
            }
        }
    }

    unique_ptr<CCassBlobFetch>  fetch_details;
    fetch_details.reset(new CCassBlobFetch(*m_AnnotRequest, blob_id));

    unique_ptr<CBlobRecord> blob_record(new CBlobRecord);
    CPSGCache               psg_cache(IPSGS_Processor::m_Request,
                                      IPSGS_Processor::m_Reply);
    auto                    blob_prop_cache_lookup_result =
                                    psg_cache.LookupBlobProp(
                                        this,
                                        blob_id.m_Sat,
                                        blob_id.m_SatKey,
                                        last_modified,
                                        *blob_record.get());

    CCassBlobTaskLoadBlob *     load_task = nullptr;
    if (blob_prop_cache_lookup_result == ePSGS_CacheHit) {
        load_task = new CCassBlobTaskLoadBlob(blob_id.m_Keyspace->GetConnection(),
                                              blob_id.m_Keyspace->keyspace,
                                              std::move(blob_record),
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    } else {
        if (m_AnnotRequest->m_UseCache == SPSGS_RequestBase::ePSGS_CacheOnly) {
            // No data in cache and not going to the DB
            if (IPSGS_Processor::m_Request->NeedTrace()) {
                string      trace_msg;
                if (blob_prop_cache_lookup_result == ePSGS_CacheNotHit)
                    trace_msg = "Blob properties are not found";
                else
                    trace_msg = "Blob properties are not found (cache lookup error)";

                IPSGS_Processor::m_Reply->SendTrace(
                    trace_msg, IPSGS_Processor::m_Request->GetStartTimestamp());
            }

            fetch_details->RemoveFromExcludeBlobCache();

            CPSGS_CassProcessorBase::SignalFinishProcessing();
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(blob_id.m_Keyspace->GetConnection(),
                                              blob_id.m_Keyspace->keyspace,
                                              blob_id.m_SatKey,
                                              last_modified,
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    }

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(this,
                              bind(&CPSGS_AnnotProcessor::OnGetBlobError,
                                   this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(this,
                          bind(&CPSGS_AnnotProcessor::OnAnnotBlobProp,
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


// Triggered for the annotation blob props;
// If the id2 blob props are sent then the other blob prop handler is used
void CPSGS_AnnotProcessor::OnAnnotBlobProp(CCassBlobFetch *  fetch_details,
                                           CBlobRecord const &  blob,
                                           bool is_found)
{
    // Annotation blob properties may come only once
    fetch_details->GetLoader()->ClearError();
    fetch_details->SetReadFinished();

    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    if (!is_found) {
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Blob properties are not found",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        if (AreAllFinishedRead()) {
            CPSGS_CassProcessorBase::SignalFinishProcessing();
        } else {
            x_Peek(false);
        }
        return;
    }

    auto    app = CPubseqGatewayApp::GetInstance();
    unsigned int    max_to_send = max(app->Settings().m_SendBlobIfSmall,
                                      m_AnnotRequest->m_SendBlobIfSmall);
    if (blob.GetSize() > max_to_send) {
        // Nothing needs to be sent because the blob is too big
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Blob size is too large (" + to_string(blob.GetSize()) +
                " > " + to_string(max_to_send) + " max allowed to send)",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        if (AreAllFinishedRead()) {
            CPSGS_CassProcessorBase::SignalFinishProcessing();
        } else {
            x_Peek(false);
        }
        return;
    }

    // Send the blob props and send a corresponding blob props
    m_BlobStage = true;
    CPSGS_CassBlobBase::OnGetBlobProp(fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_AnnotProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
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


void CPSGS_AnnotProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
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

    // Move the annotation to the error list because the promised data are not
    // provided (OnGetBlobError(...) called CountError(...) so the m_Status is
    // set correctly
    SPSGS_AnnotRequest::EPSGS_ResultStatus  result_status = SPSGS_AnnotRequest::ePSGS_RS_Error;
    if (m_Status == CRequestStatus::e504_GatewayTimeout)
        result_status = SPSGS_AnnotRequest::ePSGS_RS_Timeout;
    m_AnnotRequest->ReportBlobError(m_Priority, result_status);


    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_AnnotProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
                                          CBlobRecord const &  blob,
                                          const unsigned char *  chunk_data,
                                          unsigned int  data_size,
                                          int  chunk_no)
{
    CPSGS_CassBlobBase::OnGetBlobChunk(m_Canceled, fetch_details,
                                       chunk_data, data_size, chunk_no);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


IPSGS_Processor::EPSGS_Status CPSGS_AnnotProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Canceled)
        return IPSGS_Processor::ePSGS_Canceled;

    return status;
}


string CPSGS_AnnotProcessor::GetName(void) const
{
    return kAnnotProcessorName;
}


string CPSGS_AnnotProcessor::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}


void CPSGS_AnnotProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_AnnotProcessor::x_Peek(bool  need_wait)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    bool        overall_final_state = false;

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto &  details: m_FetchDetails) {
            if (details) {
                if (details->InPeek()) {
                    continue;
                }
                details->SetInPeek(true);
                overall_final_state |= x_Peek(details, need_wait);
                details->SetInPeek(false);
            }
        }
        if (initial_size == m_FetchDetails.size())
            break;
    }

    if (m_BlobStage) {
        // It is a stage of sending the blob. So the chunks need to be sent as
        // soon as they are available
        IPSGS_Processor::m_Reply->Flush(CPSGS_Reply::ePSGS_SendAccumulated);

        if (AreAllFinishedRead()) {
            for (auto &  details: m_FetchDetails) {
                if (details) {
                    // Update the cache records where needed
                    details->SetExcludeBlobCacheCompleted();
                }
            }
            CPSGS_CassProcessorBase::SignalFinishProcessing();
        }
    } else {
        // Ready packets needs to be send only once when everything is finished
        if (overall_final_state) {
            if (AreAllFinishedRead()) {
                IPSGS_Processor::m_Reply->Flush(CPSGS_Reply::ePSGS_SendAccumulated);
                CPSGS_CassProcessorBase::SignalFinishProcessing();
            }
        }
    }
}


bool CPSGS_AnnotProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                  bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return true;

    bool        final_state = false;
    if (need_wait) {
        if (!fetch_details->ReadFinished()) {
            final_state = fetch_details->GetLoader()->Wait();
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

        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(),
                kAnnotProcessorName, error,
                CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    }

    return final_state;
}


void CPSGS_AnnotProcessor::x_OnResolutionGoodData(void)
{
    // The resolution process started to receive data which look good
    // however the annotations processor should not do anything.
    // The processor uses the an API to check bioseq info and named annotations
    // before sending them.
}

