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
 * File Description: resolve processor
 *
 */

#include <ncbi_pch.hpp>

#include "resolve_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;


CPSGS_ResolveProcessor::CPSGS_ResolveProcessor() :
    m_ResolveRequest(nullptr)
{}


CPSGS_ResolveProcessor::CPSGS_ResolveProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4),
                      bind(&CPSGS_ResolveProcessor::x_OnResolutionGoodData,
                           this))
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_ResolveRequest>() everywhere
    m_ResolveRequest = & request->GetRequest<SPSGS_ResolveRequest>();
}


CPSGS_ResolveProcessor::~CPSGS_ResolveProcessor()
{}


IPSGS_Processor*
CPSGS_ResolveProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority) const
{
    if (!IsCassandraProcessorEnabled(request))
        return nullptr;

    if (request->GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest) {
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

        return new CPSGS_ResolveProcessor(request, reply, priority);
    }
    return nullptr;
}


void CPSGS_ResolveProcessor::Process(void)
{
    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_ResolveProcessor::x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    UpdateOverallStatus(status);

    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    if (status == CRequestStatus::e404_NotFound) {
        IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, GetName(),
                                                       message, status,
                                                       ePSGS_NoBioseqInfo,
                                                       eDiag_Error);
    } else {
        PSG_WARNING(message);
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
CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    if (m_Cancelled) {
        m_Completed = true;
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    m_Completed = true;
    SignalFinishProcessing();
}


void
CPSGS_ResolveProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    auto    effective_output_format = m_ResolveRequest->m_OutputFormat;
    if (effective_output_format == SPSGS_ResolveRequest::ePSGS_NativeFormat ||
        effective_output_format == SPSGS_ResolveRequest::ePSGS_UnknownFormat) {
        effective_output_format = SPSGS_ResolveRequest::ePSGS_JsonFormat;
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
        AdjustBioseqAccession(bioseq_resolution);

    string              data_to_send;
    if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat) {
        data_to_send = ToJson(bioseq_resolution.m_BioseqInfo,
                              m_ResolveRequest->m_IncludeDataFlags).
                                                Repr(CJsonNode::fStandardJson);
    } else {
        data_to_send = ToBioseqProtobuf(bioseq_resolution.m_BioseqInfo);
    }

    size_t              item_id = IPSGS_Processor::m_Reply->GetItemId();
    IPSGS_Processor::m_Reply->PrepareBioseqData(item_id, GetName(),
                                                data_to_send,
                                                effective_output_format);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, GetName(), 2);
}


void CPSGS_ResolveProcessor::Cancel(void)
{
    m_Cancelled = true;
    CancelLoaders();
}


IPSGS_Processor::EPSGS_Status CPSGS_ResolveProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress) {
        return status;
    }

    if (m_Cancelled) {
        return IPSGS_Processor::ePSGS_Cancelled;
    }

    return status;
}


string CPSGS_ResolveProcessor::GetName(void) const
{
    return "Cassandra-resolve";
}


void CPSGS_ResolveProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_ResolveProcessor::x_Peek(bool  need_wait)
{
    if (m_Cancelled) {
        return;
    }

    if (m_InPeek) {
        return;
    }

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


bool CPSGS_ResolveProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
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


void CPSGS_ResolveProcessor::x_OnResolutionGoodData(void)
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

