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


CPendingOperation::CPendingOperation(shared_ptr<CPSGS_Request>  user_request,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     shared_ptr<IPSGS_Processor>  processor) :
    m_UserRequest(user_request),
    m_Reply(reply),
    m_Started(false),
    m_ConnectionCanceled(false),
    m_ConnectionCanceledHandled(false),
    m_Processor(processor),
    m_InProcess(false)
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    /*
    PSG_TRACE("CPendingOperation::CPendingOperation() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);
    */
}


CPendingOperation::~CPendingOperation()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    /*
    PSG_TRACE("CPendingOperation::~CPendingOperation() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);
    */
}


void CPendingOperation::Clear()
{
    CRequestContextResetter     context_resetter;
    m_UserRequest->SetRequestContext();

    /*
    PSG_TRACE("CPendingOperation::Clear() request: " <<
              m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) <<
              ", this: " << this);
    */

    m_Reply->Clear();
    m_Started = false;
    m_ConnectionCanceled = false;
    m_ConnectionCanceledHandled = false;
    m_InProcess = false;
}


void CPendingOperation::Start(void)
{
    if (m_Started)
        return;
    m_Started = true;

    if (m_UserRequest->NeedTrace()) {
        m_Reply->SendTrace(
            "Start pending request: " +
            m_UserRequest->Serialize().Repr(CJsonNode::fStandardJson) +
            "\nRunning processor: " + m_Processor->GetName() +
            " (priority: " + to_string(m_Processor->GetPriority()) + ")",
            m_UserRequest->GetStartTimestamp());
    }

    try {
        m_Processor->OnBeforeProcess();
        m_Processor->Process();
    } catch (const exception &  exc) {
        string  msg = "Failure to start processor " + m_Processor->GetName() +
                      ". The processor generated an exception: " +
                      string(exc.what()) +
                      "\nThe processor will be cancelled.";
        if (m_UserRequest->NeedTrace()) {
            m_Reply->SendTrace(msg, m_UserRequest->GetStartTimestamp());
        }
        PSG_ERROR(msg);
        m_Processor->Cancel();
    } catch (...) {
        string  msg = "Failure to start processor " + m_Processor->GetName() +
                      ". The processor generated an unknown exception. "
                      "The processor will be cancelled.";
        if (m_UserRequest->NeedTrace()) {
            m_Reply->SendTrace(msg, m_UserRequest->GetStartTimestamp());
        }
        PSG_ERROR(msg);
        m_Processor->Cancel();
    }
}


void CPendingOperation::SendProcessorStartMessage(void)
{
    if (m_UserRequest->NeedProcessorEvents())
        m_Reply->PrepareProcessorProgressMessage(m_Processor->GetName(),
                                                 "start");
}


void call_process_event_cb(void *  user_data)
{
    IPSGS_Processor *       proc = (IPSGS_Processor*)(user_data);
    proc->ProcessEvent();
}


void CPendingOperation::Peek(bool  need_wait)
{
    if (m_InProcess) {
        // Prevent recursion due to SignalProcessorFinished() call from
        // processor->ProcessEvent()
        return;
    }

    if (m_ConnectionCanceled) {
        // The ProcessEvent() needs to be called because it is the processor
        // responsibility to report the finish even in case of a dropped
        // connection.
        // The ProcessEvent() call however should be called from the processor
        // loop, not from the framework loop.
        if (!m_ConnectionCanceledHandled) {
            m_ConnectionCanceledHandled = true;
            m_Processor->PostponeInvoke(call_process_event_cb,
                                        (void*)(m_Processor.get()));
        }
        return;
    }

    // Previously the ProcessEvent() call was conditional. The invoke was done
    // only for InProgress and Canceled status. Later it turned out that a
    // processor can get a timeout for one fetch and still need an event for a
    // not finished other fetch like public comment. Potentially it is possible
    // to have similar cases with unauthorized or error states. So it is safer
    // to deliver an event regardless of the processor status.
    m_InProcess = true;
    m_Processor->ProcessEvent();
    auto    processor_status = m_Processor->GetStatus();
    m_InProcess = false;

    if (processor_status != IPSGS_Processor::ePSGS_InProgress) {
        // The SignalFinishProcessing() call needs to be made even if it has
        // already been called.
        // The processor may think it has finished but the output is not ready
        // so the final Flush() cannot be called. The processors group may not
        // be deleted before the output is finally flushed.
        // So one of the consequent SignalFinishProcessing() calls will detect
        // again that all the members of the group finished and that the output
        // is ready then the dispatcher will:
        // - prepare the final PSG chunk
        // - call the last flush for the output
        // - do request stop
        // - destroy the processors group which will free all the
        //   allocated memory
        CPubseqGatewayApp::GetInstance()->SignalFinishProcessing(
                        m_Processor.get(), CPSGS_Dispatcher::ePSGS_Fromework);
    }
}


void CPendingOperation::ConnectionCancel(void)
{
    m_ConnectionCanceled = true;

    m_Reply->ConnectionCancel();

    CPubseqGatewayApp::GetInstance()->SignalConnectionCanceled(
        m_UserRequest->GetRequestId());
}

