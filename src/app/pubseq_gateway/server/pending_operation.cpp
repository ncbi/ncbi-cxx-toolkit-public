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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "pending_operation.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "getblob_processor.hpp"
#include "tse_chunk_processor.hpp"
#include "resolve_processor.hpp"
#include "annot_processor.hpp"
#include "get_processor.hpp"

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
USING_IDBLOB_SCOPE;



CPendingOperation::CPendingOperation(unique_ptr<CPSGS_Request>  user_request,
                                     size_t  initial_reply_chunks) :
    m_UserRequest(move(user_request)),
    m_Reply(new CPSGS_Reply(initial_reply_chunks)),
    m_Cancelled(false)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    PSG_TRACE("CPendingOperation::CPendingOperation() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    PSG_TRACE("CPendingOperation::~CPendingOperation() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);

    // Just in case if a request ended without a normal request stop,
    // finish it here as the last resort.
    x_PrintRequestStop();
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    PSG_TRACE("CPendingOperation::Clear() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);

    m_Reply->Clear();
    m_Cancelled = false;
}


void CPendingOperation::Start(shared_ptr<HST::CHttpReply<CPendingOperation>> resp)
{
    auto *          app = CPubseqGatewayApp::GetInstance();

    m_Reply->SetReply(resp.get());

    m_Processors = app->DispatchRequest(m_UserRequest, m_Reply);
    if (m_Processors.empty()) {
        string  msg = "No processors found to serve the request";
        PSG_TRACE(msg);

        m_FinishStatuses.push_back(IPSGS_Processor::ePSGS_NotFound);
        m_Reply->PrepareReplyMessage(msg, CRequestStatus::e404_NotFound,
                                     ePSGS_NoProcessor, eDiag_Error);
        m_Reply->PrepareReplyCompletion();
        m_Reply->Flush(true);
        x_PrintRequestStop();
        return;
    }

    m_CurrentProcessor = m_Processors.begin();
    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace(
            "Start pending request: " +
            m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) +
            ". Number of processors: " + to_string(m_Processors.size()) +
            ". Running processor: " + (*m_CurrentProcessor)->GetName(),
            m_UserRequest->GetStartTimestamp());
    }
    PSG_TRACE("Running processor: " << (*m_CurrentProcessor)->GetName());

    (*m_CurrentProcessor)->Process();
}


void CPendingOperation::Peek(HST::CHttpReply<CPendingOperation>& resp,
                             bool  need_wait)
{
    if (m_Cancelled) {
        if (resp.IsOutputReady() && !resp.IsFinished()) {
            resp.Send(nullptr, 0, true, true);
        }
        return;
    }

    if (m_CurrentProcessor == m_Processors.end()) {
        // No more processors
        x_FinalizeReply();
        return;
    }


    auto    processor_status = (*m_CurrentProcessor)->GetStatus();
    if (processor_status == IPSGS_Processor::ePSGS_InProgress) {
        // Note: the ProcessEvent() _may_ lead to the situation when a
        // processor has completed its job. In this case the processor
        // _may_ call SignalProcessorFinish() which leads to a recursive call
        // of Peek() and thus can move the current processor iterator forward.
        // To avoid it the current iterator is saved and checked that it is
        // still the same processor in handling.
        auto    current_processor = m_CurrentProcessor;
        (*m_CurrentProcessor)->ProcessEvent();
        if (current_processor != m_CurrentProcessor)
            return;
        processor_status = (*m_CurrentProcessor)->GetStatus();
    }

    if (processor_status != IPSGS_Processor::ePSGS_InProgress) {
        m_FinishStatuses.push_back(processor_status);

        PSG_TRACE("Processor: " << (*m_CurrentProcessor)->GetName() <<
                  " finished with status " << processor_status);
        if (m_UserRequest->NeedTrace()) {
            m_Reply->SendTrace(
                "Processor: " + (*m_CurrentProcessor)->GetName() +
                " finished with status " + to_string(processor_status),
                m_UserRequest->GetStartTimestamp());
        }

        ++m_CurrentProcessor;
        if (m_CurrentProcessor == m_Processors.end()) {
            x_FinalizeReply();
        } else {
            PSG_TRACE("Running next processor: " <<
                      (*m_CurrentProcessor)->GetName());
            if (m_UserRequest->NeedTrace()) {
                m_Reply->SendTrace(
                    "Running next processor: " + (*m_CurrentProcessor)->GetName(),
                    m_UserRequest->GetStartTimestamp());
            }
            (*m_CurrentProcessor)->Process();
        }
    }
}


void CPendingOperation::x_FinalizeReply(void)
{
    if (!m_Reply->IsReplyFinished() && m_Reply->IsOutputReady()) {
        m_Reply->PrepareReplyCompletion();
        m_Reply->Flush(true);

        x_PrintRequestStop();
    }
}


void CPendingOperation::x_PrintRequestStop(void)
{
    if (m_UserRequest->GetRequestContext().NotNull()) {
        CDiagContext::SetRequestContext(m_UserRequest->GetRequestContext());
        m_UserRequest->GetRequestContext()->SetReadOnly(false);
        m_UserRequest->GetRequestContext()->SetRequestStatus(x_GetRequestStopStatus());
        GetDiagContext().PrintRequestStop();
        m_UserRequest->GetRequestContext().Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


CRequestStatus::ECode CPendingOperation::x_GetRequestStopStatus(void) const
{
    IPSGS_Processor::EPSGS_Status   best_status = IPSGS_Processor::ePSGS_Error;
    for (const auto &  status : m_FinishStatuses) {
        best_status = min(best_status, status);
    }

    switch (best_status) {
        case IPSGS_Processor::ePSGS_Found:
            return CRequestStatus::e200_Ok;
        case IPSGS_Processor::ePSGS_NotFound:
            return CRequestStatus::e404_NotFound;
        default:
            break;
    }

    // Do we need to distinguish between a user request error like 400 and
    // a server problem like 500?
    return CRequestStatus::e500_InternalServerError;
}

