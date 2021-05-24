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

USING_NCBI_SCOPE;

using namespace std::placeholders;


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
    CPSGS_CassBlobBase(request, reply, GetName()),
    m_BlobStage(false)
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_AnnotRequest>() everywhere
    m_AnnotRequest = & request->GetRequest<SPSGS_AnnotRequest>();
}


CPSGS_AnnotProcessor::~CPSGS_AnnotProcessor()
{}


IPSGS_Processor*
CPSGS_AnnotProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                      shared_ptr<CPSGS_Reply> reply,
                                      TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_AnnotationRequest)
        return nullptr;

    auto    valid_annots = x_FilterNames(request->GetRequest<SPSGS_AnnotRequest>().m_Names);
    if (valid_annots.empty())
        return nullptr;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace("Cannot create " + GetName() +
                             " processor because Cassandra DB "
                             "is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return nullptr;
    }

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


bool CPSGS_AnnotProcessor::x_IsNameValid(const string &  name)
{
    static CRegexp  regexp("^NA\\d+\\.\\d+$", CRegexp::fCompile_ignore_case);
    return regexp.IsMatch(name);
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
        m_Completed = true;
        SignalFinishProcessing();
        return;
    }

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
    SignalFinishProcessing();
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
    vector<pair<string, int32_t>>   bioseq_na_keyspaces =
                CPubseqGatewayApp::GetInstance()->GetBioseqNAKeyspaces();

    for (const auto &  bioseq_na_keyspace : bioseq_na_keyspaces) {
        unique_ptr<CCassNamedAnnotFetch>   details;
        details.reset(new CCassNamedAnnotFetch(*m_AnnotRequest));

        CCassNAnnotTaskFetch *  fetch_task =
                new CCassNAnnotTaskFetch(app->GetCassandraTimeout(),
                                         app->GetCassandraMaxRetries(),
                                         app->GetCassandraConnection(),
                                         bioseq_na_keyspace.first,
                                         bioseq_resolution.m_BioseqInfo.GetAccession(),
                                         bioseq_resolution.m_BioseqInfo.GetVersion(),
                                         bioseq_resolution.m_BioseqInfo.GetSeqIdType(),
                                         m_ValidNames,
                                         nullptr, nullptr);
        details->SetLoader(fetch_task);

        fetch_task->SetConsumeCallback(
            CNamedAnnotationCallback(
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotData,
                     this, _1, _2, _3, _4),
                details.get(), bioseq_na_keyspace.second));
        fetch_task->SetErrorCB(
            CNamedAnnotationErrorCallback(
                bind(&CPSGS_AnnotProcessor::x_OnNamedAnnotError,
                     this, _1, _2, _3, _4, _5),
                details.get()));
        fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
                ToJson(*fetch_task).Repr(CJsonNode::fStandardJson),
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        m_FetchDetails.push_back(move(details));
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
    if (m_Cancelled) {
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

    if (m_Cancelled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->SetReadFinished();
        return false;
    }
    if (IPSGS_Processor::m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                        CPSGSCounters::ePSGS_UnknownError);
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");

        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        m_Completed = true;
        SignalFinishProcessing();
        return false;
    }

    if (last) {
        fetch_details->SetReadFinished();

        // There could be many sat_name(s) requested so the callback is called
        // many times. If all of them finished then the completion should be
        // set to true. Otherwise the process of waiting for the other callback
        // should continue.
        if (AreAllFinishedRead()) {
            m_Completed = true;
            SignalFinishProcessing();
            return false;
        }

        // Not all finished so wait for more callbacks
        x_Peek(false);
        return true;
    }

    auto    other_proc_priority = m_AnnotRequest->RegisterProcessedName(
                    m_Priority, annot_record.GetAnnotName());
    bool    annot_was_sent = false;
    if (other_proc_priority == kUnknownPriority) {
        // Has not been processed yet at all
        IPSGS_Processor::m_Reply->PrepareNamedAnnotationData(
            annot_record.GetAnnotName(), GetName(),
            ToJson(annot_record, sat).Repr(CJsonNode::fStandardJson));
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
            annot_record.GetAnnotName(), GetName(),
            ToJson(annot_record, sat).Repr(CJsonNode::fStandardJson));
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

    x_Peek(false);
    return true;
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

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    auto *  app = CPubseqGatewayApp::GetInstance();
    PSG_ERROR(message);

    if (is_error) {
        if (code == CCassandraException::eQueryTimeout)
            app->GetCounters().Increment(CPSGSCounters::ePSGS_CassQueryTimeoutError);
        else
            app->GetCounters().Increment(CPSGSCounters::ePSGS_UnknownError);
    }

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Named annotation error callback",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(),
            GetName(), message, CRequestStatus::e500_InternalServerError,
            code, severity);
    if (is_error) {
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);

        // There will be no more activity
        fetch_details->SetReadFinished();
        m_Completed = true;
        SignalFinishProcessing();
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

    m_AnnotRequest->m_BlobId = SPSGS_BlobId(to_string(sat) + "." + to_string(sat_key));
    SCass_BlobId    blob_id(m_AnnotRequest->m_BlobId.GetId());

    auto    app = CPubseqGatewayApp::GetInstance();
    if (!blob_id.MapSatToKeyspace()) {
        app->GetCounters().Increment(CPSGSCounters::ePSGS_ServerSatToSatNameError);

        string  err_msg = GetName() + " processor failed to map sat " +
                          to_string(blob_id.m_Sat) +
                          " to a Cassandra keyspace while requesting the blob props";
        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(), GetName(),
                err_msg, CRequestStatus::e404_NotFound,
                ePSGS_UnknownResolvedSatellite, eDiag_Error);
        PSG_WARNING(err_msg);

        // It could be only one annotation and the blob prop retrieval happens
        // after sending the annotation so it is safe to say that the processor
        // is finished
        m_Completed = true;
        SignalFinishProcessing();
        return;
    }

    // Checking only.
    // Here we are requesting blob props only so may be the blob itself will
    // not be requested at all. However if the blob is in cache then its blob
    // properties have already been sent for sure.
    if (!m_AnnotRequest->m_ClientId.empty()) {
        if (m_AnnotRequest->m_AutoBlobSkipping) {
            bool        completed = true;
            if (app->GetExcludeBlobCache()->IsInCache(
                        m_AnnotRequest->m_ClientId,
                        blob_id.m_Sat, blob_id.m_SatKey, completed)) {
                if (completed)
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                            blob_id.ToString(), GetName(), ePSGS_BlobSent);
                else
                    IPSGS_Processor::m_Reply->PrepareBlobExcluded(
                            blob_id.ToString(), GetName(), ePSGS_BlobInProgress);
                m_Completed = true;
                SignalFinishProcessing();
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
                                        blob_id.m_Sat,
                                        blob_id.m_SatKey,
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

            m_Completed = true;
            SignalFinishProcessing();
            return;
        }

        load_task = new CCassBlobTaskLoadBlob(app->GetCassandraTimeout(),
                                              app->GetCassandraMaxRetries(),
                                              app->GetCassandraConnection(),
                                              blob_id.m_Keyspace,
                                              blob_id.m_SatKey,
                                              last_modified,
                                              false, nullptr);
        fetch_details->SetLoader(load_task);
    }

    load_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());
    load_task->SetErrorCB(
        CGetBlobErrorCallback(bind(&CPSGS_AnnotProcessor::OnGetBlobError,
                                   this, _1, _2, _3, _4, _5),
                              fetch_details.get()));
    load_task->SetPropsCallback(
        CBlobPropCallback(bind(&CPSGS_AnnotProcessor::OnAnnotBlobProp,
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


// Triggered for the annotation blob props;
// If the id2 blob props are sent then the other blob prop handler is used
void CPSGS_AnnotProcessor::OnAnnotBlobProp(CCassBlobFetch *  fetch_details,
                                           CBlobRecord const &  blob,
                                           bool is_found)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    if (!is_found) {
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Blob properties are not found",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        m_Completed = true;
        SignalFinishProcessing();
        return;
    }

    auto    app = CPubseqGatewayApp::GetInstance();
    unsigned int    max_to_send = max(app->GetSendBlobIfSmall(),
                                      m_AnnotRequest->m_SendBlobIfSmall);
    if (blob.GetSize() > max_to_send) {
        // Nothing needs to be sent because the blob is too big
        if (IPSGS_Processor::m_Request->NeedTrace()) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Blob size is too large (" + to_string(blob.GetSize()) +
                " > " + to_string(max_to_send) + " max allowed to send)",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        m_Completed = true;
        SignalFinishProcessing();
        return;
    }

    // Send the blob props and send a corresponding blob props
    m_BlobStage = true;
    CPSGS_CassBlobBase::OnGetBlobProp(bind(&CPSGS_AnnotProcessor::OnGetBlobProp,
                                           this, _1, _2, _3),
                                      bind(&CPSGS_AnnotProcessor::OnGetBlobChunk,
                                           this, _1, _2, _3, _4, _5),
                                      bind(&CPSGS_AnnotProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_AnnotProcessor::OnGetBlobProp(CCassBlobFetch *  fetch_details,
                                         CBlobRecord const &  blob,
                                         bool is_found)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    CPSGS_CassBlobBase::OnGetBlobProp(bind(&CPSGS_AnnotProcessor::OnGetBlobProp,
                                           this, _1, _2, _3),
                                      bind(&CPSGS_AnnotProcessor::OnGetBlobChunk,
                                           this, _1, _2, _3, _4, _5),
                                      bind(&CPSGS_AnnotProcessor::OnGetBlobError,
                                           this, _1, _2, _3, _4, _5),
                                      fetch_details, blob, is_found);

    if (IPSGS_Processor::m_Reply->IsOutputReady())
        x_Peek(false);
}


void CPSGS_AnnotProcessor::OnGetBlobError(CCassBlobFetch *  fetch_details,
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


void CPSGS_AnnotProcessor::OnGetBlobChunk(CCassBlobFetch *  fetch_details,
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


void CPSGS_AnnotProcessor::Cancel(void)
{
    m_Cancelled = true;
    CancelLoaders();
}


IPSGS_Processor::EPSGS_Status CPSGS_AnnotProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Cancelled)
        return IPSGS_Processor::ePSGS_Cancelled;

    return status;
}


string CPSGS_AnnotProcessor::GetName(void) const
{
    return "Cassandra-getna";
}


void CPSGS_AnnotProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_AnnotProcessor::x_Peek(bool  need_wait)
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
            if (details)
                overall_final_state |= x_Peek(details, need_wait);
        }
        if (initial_size == m_FetchDetails.size())
            break;
    }

    if (m_BlobStage) {
        // It is a stage of sending the blob. So the chunks need to be sent as
        // soon as they are available
        if (IPSGS_Processor::m_Reply->IsOutputReady())
            IPSGS_Processor::m_Reply->Flush(false);

        if (AreAllFinishedRead()) {
            for (auto &  details: m_FetchDetails) {
                if (details) {
                    // Update the cache records where needed
                    details->SetExcludeBlobCacheCompleted();
                }
            }
        }
    } else {
        // Ready packets needs to be send only once when everything is finished
        if (overall_final_state) {
            if (AreAllFinishedRead()) {
                if (IPSGS_Processor::m_Reply->IsOutputReady()) {
                    IPSGS_Processor::m_Reply->Flush(false);
                }
            }
        }
    }

    m_InPeek = false;
}


bool CPSGS_AnnotProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                  bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return true;

    bool        final_state = false;
    if (need_wait)
        if (!fetch_details->ReadFinished()) {
            final_state = fetch_details->GetLoader()->Wait();
        }

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetCounters().Increment(CPSGSCounters::ePSGS_UnknownError);
        PSG_ERROR(error);

        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(),
                GetName(), error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        SignalFinishProcessing();
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

