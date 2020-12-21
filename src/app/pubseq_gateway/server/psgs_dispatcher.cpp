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
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include "psgs_dispatcher.hpp"
#include "pubseq_gateway_logging.hpp"

USING_NCBI_SCOPE;


void CPSGS_Dispatcher::AddProcessor(unique_ptr<IPSGS_Processor> processor)
{
    m_RegisteredProcessors.push_back(move(processor));
}


list<IPSGS_Processor *>
CPSGS_Dispatcher::DispatchRequest(shared_ptr<CPSGS_Request> request,
                                  shared_ptr<CPSGS_Reply> reply)
{
    list<IPSGS_Processor *>         ret;
    list<SProcessorData>            proc_group;
    TProcessorPriority              priority = m_RegisteredProcessors.size();
    auto                            request_id = request->GetRequestId();

    for (auto const &  proc : m_RegisteredProcessors) {
        if (request->NeedTrace()) {
            reply->SendTrace("Try to create processor: " + proc->GetName(),
                             request->GetStartTimestamp());
        }
        IPSGS_Processor *   p = proc->CreateProcessor(request, reply, priority);
        if (p) {
            ret.push_back(p);

            m_GroupsLock.lock();

            auto    it = m_ProcessorGroups.find(request_id);
            auto    new_proc = SProcessorData(p, ePSGS_Up,
                                              IPSGS_Processor::ePSGS_InProgress);
            if (it == m_ProcessorGroups.end()) {
                // First processor in the group so create the list
                list<SProcessorData>    procs;
                procs.emplace_back(new_proc);
                m_ProcessorGroups[request_id] = move(procs);
            } else {
                // Additional processor in the group so use existing list
                it->second.emplace_back(new_proc);
            }

            m_GroupsLock.unlock();

            if (request->NeedTrace()) {
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has been created sucessfully (priority: " +
                                 to_string(priority) + ")",
                                 request->GetStartTimestamp());
            }
        } else {
            if (request->NeedTrace()) {
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has not been created",
                                 request->GetStartTimestamp());
            }
        }
        --priority;
    }

    if (ret.empty()) {
        string  msg = "No matching processors found to serve the request";
        PSG_TRACE(msg);

        reply->PrepareReplyMessage(msg, CRequestStatus::e404_NotFound,
                                   ePSGS_NoProcessor, eDiag_Error);
        reply->PrepareReplyCompletion();
        reply->Flush(true);
        x_PrintRequestStop(request, CRequestStatus::e404_NotFound);
    }

    return ret;
}


IPSGS_Processor::EPSGS_StartProcessing
CPSGS_Dispatcher::SignalStartProcessing(IPSGS_Processor *  processor)
{
    // Basically the Cancel() call needs to be invoked for each of the other
    // processors.
    size_t                      request_id = processor->GetRequest()->GetRequestId();
    list<IPSGS_Processor *>     to_be_cancelled;    // To avoid calling Cancel()
                                                    // under the lock


    m_GroupsLock.lock();

    if (processor->GetRequest()->NeedTrace()) {
        // Trace sending is under a lock intentionally: to avoid changes in the
        // sequence of processing the signal due to a race when tracing is
        // switched on.
        // In a normal operation the tracing is switched off anyway.
        processor->GetReply()->SendTrace(
            "Processor: " + processor->GetName() + " (priority: " +
            to_string(processor->GetPriority()) +
            ") signalled start dealing with data for the client",
            processor->GetRequest()->GetStartTimestamp());
    }

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist anymore
        m_GroupsLock.unlock();
        return IPSGS_Processor::ePSGS_Cancel;
    }

    // The group found; check that the processor has not been cancelled yet
    for (const auto &  proc: procs->second) {
        if (proc.m_Processor == processor) {
            if (proc.m_DispatchStatus == ePSGS_Cancelled) {
                // The other processor has already called Cancel() for this one
                m_GroupsLock.unlock();
                return IPSGS_Processor::ePSGS_Cancel;
            }
            if (proc.m_DispatchStatus != ePSGS_Up) {
                PSG_ERROR("Logic error: the processor dispatcher received "
                          "'start processing' when its dispatch status is not UP (i.e. FINISHED)");
            }
            break;
        }
    }

    // Everything looks OK; this is the first processor who started to send
    // data to the client so the other processors should be cancelled
    for (auto &  proc: procs->second) {
        if (proc.m_Processor == processor)
            continue;
        if (proc.m_DispatchStatus == ePSGS_Up) {
            proc.m_DispatchStatus = ePSGS_Cancelled;
            to_be_cancelled.push_back(proc.m_Processor);
        }
    }

    m_GroupsLock.unlock();

    // Call the other processor's Cancel() out of the lock
    for (auto & proc: to_be_cancelled) {

        if (processor->GetRequest()->NeedTrace()) {
            processor->GetReply()->SendTrace(
                "Invoking Cancel() for the processor: " + proc->GetName() +
                " (priority: " + to_string(proc->GetPriority()) + ")",
                processor->GetRequest()->GetStartTimestamp());
        }

        proc->Cancel();
    }

    return IPSGS_Processor::ePSGS_Proceed;
}


void CPSGS_Dispatcher::SignalFinishProcessing(IPSGS_Processor *  processor)
{
    // It needs to check if all the processors finished.
    // If so then the best finish status is used, sent to the client and the
    // group is deleted

    bool                            all_procs_finished = true;
    size_t                          request_id = processor->GetRequest()->GetRequestId();
    IPSGS_Processor::EPSGS_Status   best_status = processor->GetStatus();

    m_GroupsLock.lock();

    if (processor->GetRequest()->NeedTrace()) {
        // Trace sending is under a lock intentionally: to avoid changes in the
        // sequence of processing the signal due to a race when tracing is
        // switched on.
        // In a normal operation the tracing is switched off anyway.
        processor->GetReply()->SendTrace(
            "Processor: " + processor->GetName() + " (priority: " +
            to_string(processor->GetPriority()) +
            ") signalled finish with status " +
            IPSGS_Processor::StatusToString(processor->GetStatus()),
            processor->GetRequest()->GetStartTimestamp());
    }

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist any more
        m_GroupsLock.unlock();
        return;
    }

    for (auto &  proc: procs->second) {
        if (proc.m_Processor == processor) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                proc.m_DispatchStatus = ePSGS_Finished;
                proc.m_FinishStatus = processor->GetStatus();
                continue;
            }

            // The status is already cancelled or finished which may mean that
            // it is not the first time call. However it is possible that
            // during the first time the output was not ready so the final PSG
            // chunk has not been sent. Thus we should continue as usual.
        }

        switch (proc.m_DispatchStatus) {
            case ePSGS_Finished:
                best_status = min(best_status, proc.m_FinishStatus);
                break;
            case ePSGS_Up:
                all_procs_finished = false;
                break;
            case ePSGS_Cancelled:
                // Finished but the cancelled processor do not participate in
                // the overall reply status
                break;
        }
    }

    if (all_procs_finished) {
        // - Send the best status to the client
        // - Clear the group
        CRequestStatus::ECode   request_status;
        switch (best_status) {
            case IPSGS_Processor::ePSGS_Found:
                request_status = CRequestStatus::e200_Ok;
                break;
            case IPSGS_Processor::ePSGS_NotFound:
                request_status = CRequestStatus::e404_NotFound;
                break;
            default:
                request_status = CRequestStatus::e500_InternalServerError;
        }

        auto    reply = processor->GetReply();
        if (!reply->IsFinished() && reply->IsOutputReady()) {
            if (processor->GetRequest()->NeedTrace()) {
                reply->SendTrace(
                    "Request processing finished; final status: " +
                    to_string(request_status),
                    processor->GetRequest()->GetStartTimestamp());
            }

            reply->PrepareReplyCompletion();
            reply->Flush(true);
            x_PrintRequestStop(processor->GetRequest(),
                               request_status);

            // Clear the group after the final chunk is sent
            m_ProcessorGroups.erase(procs);
        }
    }

    m_GroupsLock.unlock();
}


void CPSGS_Dispatcher::SignalConnectionCancelled(IPSGS_Processor *  processor)
{
    // When a connection is cancelled there will be no possibility to
    // send anything over the connection. So basically what is needed to do is
    // to print request stop and delete the processors group.
    auto        request = processor->GetRequest();
    size_t      request_id = request->GetRequestId();

    m_GroupsLock.lock();

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist any more
        m_GroupsLock.unlock();
        return;
    }

    if (request->GetRequestContext().NotNull()) {
        request->SetRequestContext();
        PSG_MESSAGE("HTTP connection has been cancelled");
        CDiagContext::SetRequestContext(NULL);
    }

    x_PrintRequestStop(processor->GetRequest(), CRequestStatus::e200_Ok);
    m_ProcessorGroups.erase(procs);
    m_GroupsLock.unlock();
}


void CPSGS_Dispatcher::x_PrintRequestStop(shared_ptr<CPSGS_Request> request,
                                          CRequestStatus::ECode  status)
{
    if (request->GetRequestContext().NotNull()) {
        request->SetRequestContext();
        CDiagContext::GetRequestContext().SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        CDiagContext::GetRequestContext().Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}

