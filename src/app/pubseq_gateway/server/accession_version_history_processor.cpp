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
 * File Description: accession version history processor
 *
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/regexp.hpp>

#include "accession_version_history_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "acc_ver_hist_callback.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;


CPSGS_AccessionVersionHistoryProcessor::CPSGS_AccessionVersionHistoryProcessor() :
    m_RecordCount(0),
    m_AccVerHistoryRequest(nullptr)
{}


CPSGS_AccessionVersionHistoryProcessor::CPSGS_AccessionVersionHistoryProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(
            request, reply,
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveFinished,
                 this, _1),
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveError,
                 this, _1, _2, _3, _4),
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnResolutionGoodData,
                 this)),
    m_RecordCount(0)
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_AccessionVersionHistoryRequest>() everywhere
    m_AccVerHistoryRequest = & request->GetRequest<SPSGS_AccessionVersionHistoryRequest>();
}


CPSGS_AccessionVersionHistoryProcessor::~CPSGS_AccessionVersionHistoryProcessor()
{}


IPSGS_Processor*
CPSGS_AccessionVersionHistoryProcessor::CreateProcessor(
                                        shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_AccessionVersionHistoryRequest)
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

    return new CPSGS_AccessionVersionHistoryProcessor(request, reply, priority);
}


void CPSGS_AccessionVersionHistoryProcessor::Process(void)
{
    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveError(
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
CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveFinished(
                                        SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    // Note: there is no need to translate the keyspace form the bioseq info.
    // The keyspace is the same where si2csi/bioseq_info tables are

    // Initiate the accession history request
    auto *                              app = CPubseqGatewayApp::GetInstance();
    unique_ptr<CCassAccVerHistoryFetch> details;
    details.reset(new CCassAccVerHistoryFetch(*m_AccVerHistoryRequest));

    // Note: the part of the resolution process is a accession substitution
    // However the request must be done using the original accession and
    // seq_id_type
    CCassAccVerHistoryTaskFetch *  fetch_task =
        new CCassAccVerHistoryTaskFetch(app->GetCassandraTimeout(),
                                        app->GetCassandraMaxRetries(),
                                        app->GetCassandraConnection(),
                                        app->GetBioseqKeyspace(),
                                        bioseq_resolution.GetOriginalAccession(),
                                        nullptr, nullptr,
                                        0,      // version is not used here
                                        bioseq_resolution.GetOriginalSeqIdType());
    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(
        CAccVerHistCallback(
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistData,
                 this, _1, _2, _3),
            details.get()));
    fetch_task->SetErrorCB(
        CAccVerHistErrorCallback(
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistError,
                 this, _1, _2, _3, _4, _5),
            details.get()));
    fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
            ToJson(*fetch_task).Repr(CJsonNode::fStandardJson),
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(details));
    fetch_task->Wait();
}


void
CPSGS_AccessionVersionHistoryProcessor::x_SendBioseqInfo(
                                        SBioseqResolution &  bioseq_resolution)
{
    if (m_Cancelled) {
        return;
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);

    size_t  item_id = IPSGS_Processor::m_Reply->GetItemId();
    auto    data_to_send = ToJson(bioseq_resolution.GetBioseqInfo(),
                                  SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                                        Repr(CJsonNode::fStandardJson);

    IPSGS_Processor::m_Reply->PrepareBioseqData(
            item_id, GetName(), data_to_send,
            SPSGS_ResolveRequest::ePSGS_JsonFormat);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, GetName(), 2);
}


bool
CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistData(
                            SAccVerHistRec &&  acc_ver_hist_record,
                            bool  last,
                            CCassAccVerHistoryFetch *  fetch_details)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        if (last) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Accession version history no-more-data callback",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        } else {
            IPSGS_Processor::m_Reply->SendTrace(
                "Accession version history data received",
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

        if (m_RecordCount == 0)
            UpdateOverallStatus(CRequestStatus::e404_NotFound);

        m_Completed = true;
        SignalFinishProcessing();
        return false;
    }

    ++m_RecordCount;
    IPSGS_Processor::m_Reply->PrepareAccVerHistoryData(
        GetName(), ToJson(acc_ver_hist_record).Repr(CJsonNode::fStandardJson));

    x_Peek(false);
    return true;
}


void
CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistError(
                                    CCassAccVerHistoryFetch *  fetch_details,
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
            "Accession version history error callback",
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


void CPSGS_AccessionVersionHistoryProcessor::Cancel(void)
{
    m_Cancelled = true;
    CancelLoaders();
}


IPSGS_Processor::EPSGS_Status CPSGS_AccessionVersionHistoryProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Cancelled)
        return IPSGS_Processor::ePSGS_Cancelled;

    return status;
}


string CPSGS_AccessionVersionHistoryProcessor::GetName(void) const
{
    return "Cassandra-accession-version-history";
}


void CPSGS_AccessionVersionHistoryProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_AccessionVersionHistoryProcessor::x_Peek(bool  need_wait)
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

    // Ready packets needs to be send only once when everything is finished
    if (overall_final_state) {
        if (AreAllFinishedRead()) {
            if (IPSGS_Processor::m_Reply->IsOutputReady()) {
                IPSGS_Processor::m_Reply->Flush(false);
            }
        }
    }

    m_InPeek = false;
}


bool CPSGS_AccessionVersionHistoryProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
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


void CPSGS_AccessionVersionHistoryProcessor::x_OnResolutionGoodData(void)
{
    // The resolution process started to receive data which look good so
    // the dispatcher should be notified that the other processors can be
    // stopped
    if (m_Cancelled || m_Completed)
        return;

    if (SignalStartProcessing() == EPSGS_StartProcessing::ePSGS_Cancel) {
        m_Completed = true;
    }
}

