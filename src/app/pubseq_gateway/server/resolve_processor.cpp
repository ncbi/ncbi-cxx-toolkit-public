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
    m_ResolveRequest(nullptr),
    m_Cancelled(false)
{}


CPSGS_ResolveProcessor::CPSGS_ResolveProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply) :
    CPSGS_CassProcessorBase(request, reply),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4)),
    m_Cancelled(false)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;

    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_ResolveRequest>() everywhere
    m_ResolveRequest = & request->GetRequest<SPSGS_ResolveRequest>();
}


CPSGS_ResolveProcessor::~CPSGS_ResolveProcessor()
{}


IPSGS_Processor*
CPSGS_ResolveProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply) const
{
    if (request->GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest)
        return new CPSGS_ResolveProcessor(request, reply);
    return nullptr;
}


void CPSGS_ResolveProcessor::Process(void)
{
    SResolveInputSeqIdError     err;
    SBioseqResolution           bioseq_resolution(
                                        m_ResolveRequest->m_StartTimestamp);

    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId(bioseq_resolution, err);
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
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (status != CRequestStatus::e404_NotFound)
        IPSGS_Processor::m_Request->UpdateOverallStatus(status);
    PSG_WARNING(message);

    if (m_ResolveRequest->m_UsePsgProtocol) {
        if (status == CRequestStatus::e404_NotFound) {
            size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
            IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, message,
                                                           status,
                                                           ePSGS_NoBioseqInfo,
                                                           eDiag_Error);
            IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, 2);
        } else {
            IPSGS_Processor::m_Reply->PrepareReplyMessage(message, status,
                                                          code, severity);
        }
    } else {
        switch (status) {
            case CRequestStatus::e400_BadRequest:
                IPSGS_Processor::m_Reply->Send400(message.c_str());
                break;
            case CRequestStatus::e404_NotFound:
                IPSGS_Processor::m_Reply->Send404(message.c_str());
                break;
            case CRequestStatus::e500_InternalServerError:
                IPSGS_Processor::m_Reply->Send500(message.c_str());
                break;
            case CRequestStatus::e502_BadGateway:
                IPSGS_Processor::m_Reply->Send502(message.c_str());
                break;
            case CRequestStatus::e503_ServiceUnavailable:
                IPSGS_Processor::m_Reply->Send503(message.c_str());
                break;
            default:
                IPSGS_Processor::m_Reply->Send400(message.c_str());
        }
    }

    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
}


// This callback is called only in case of a valid resolution
void
CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    m_Completed = true;
    IPSGS_Processor::m_Reply->SignalProcessorFinished();
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

    if (m_ResolveRequest->m_UsePsgProtocol) {
        size_t              item_id = IPSGS_Processor::m_Reply->GetItemId();
        IPSGS_Processor::m_Reply->PrepareBioseqData(item_id, data_to_send,
                                                    effective_output_format);
        IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, 2);
    } else {
        if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat)
            IPSGS_Processor::m_Reply->SendData(data_to_send, ePSGS_JsonMime);
        else
            IPSGS_Processor::m_Reply->SendData(data_to_send, ePSGS_BinaryMime);
    }
}


void CPSGS_ResolveProcessor::Cancel(void)
{
    m_Cancelled = true;
}


bool CPSGS_ResolveProcessor::IsFinished(void)
{
    return CPSGS_CassProcessorBase::IsFinished();
}


void CPSGS_ResolveProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_ResolveProcessor::x_Peek(bool  need_wait)
{
    if (m_Cancelled)
        return;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call resp->Send()  to send what we have if it is ready
    for (auto &  details: m_FetchDetails) {
        if (details)
            x_Peek(details, need_wait);
    }

    // Ready packets needs to be send only once when everything is finished
    if (IPSGS_Processor::m_Reply->GetReply()->IsOutputReady())
        if (AreAllFinishedRead())
            IPSGS_Processor::m_Reply->Flush(false);
}


void CPSGS_ResolveProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                    bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return;

    if (need_wait)
        if (!fetch_details->ReadFinished())
            fetch_details->GetLoader()->Wait();

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->GetReply()->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->GetReply()->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        app->GetErrorCounters().IncUnknownError();
        PSG_ERROR(error);

        IPSGS_Processor::m_Reply->PrepareReplyMessage(
                error, CRequestStatus::e500_InternalServerError,
                ePSGS_UnknownError, eDiag_Error);

        // Mark finished
        IPSGS_Processor::m_Request->UpdateOverallStatus(
                                    CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        IPSGS_Processor::m_Reply->SignalProcessorFinished();
    }
}

