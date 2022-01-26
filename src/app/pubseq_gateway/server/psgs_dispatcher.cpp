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
#include "http_server_transport.hpp"
#include "pubseq_gateway.hpp"


USING_NCBI_SCOPE;

// libuv timer callback
void request_timer_cb(uv_timer_t *  handle)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      request_id = (size_t)(handle->data);
    app->GetProcessorDispatcher()->OnRequestTimer(request_id);
}

// the libuv handle memory can only be freed in the close_cb or after it was
// called. See the libuv documentation
void request_timer_close_cb(uv_handle_t *  handle)
{
    uv_timer_t *    uv_timer = reinterpret_cast<uv_timer_t *>(handle);
    delete uv_timer;
}


void CPSGS_Dispatcher::AddProcessor(unique_ptr<IPSGS_Processor> processor)
{
    m_RegisteredProcessors.push_back(move(processor));
}


list<IPSGS_Processor *>
CPSGS_Dispatcher::DispatchRequest(shared_ptr<CPSGS_Request> request,
                                  shared_ptr<CPSGS_Reply> reply)
{
    list<IPSGS_Processor *>         ret;
    TProcessorPriority              priority = m_RegisteredProcessors.size();
    auto                            request_id = request->GetRequestId();
    unique_ptr<SProcessorGroup>     procs(new SProcessorGroup());

    for (auto const &  proc : m_RegisteredProcessors) {
        if (request->NeedTrace()) {
            // false: no need to update the last activity
            reply->SendTrace("Try to create processor: " + proc->GetName(),
                             request->GetStartTimestamp(), false);
        }
        IPSGS_Processor *   p = proc->CreateProcessor(request, reply, priority);
        if (p) {
            ret.push_back(p);

            auto    new_proc = SProcessorData(p, ePSGS_Up,
                                              IPSGS_Processor::ePSGS_InProgress);
            procs->m_Processors.emplace_back(new_proc);

            if (request->NeedTrace()) {
                // false: no need to update the last activity
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has been created sucessfully (priority: " +
                                 to_string(priority) + ")",
                                 request->GetStartTimestamp(), false);
            }
        } else {
            if (request->NeedTrace()) {
                // false: no need to update the last activity
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has not been created",
                                 request->GetStartTimestamp(), false);
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
        reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
        reply->SetCompleted();
        x_PrintRequestStop(request, CRequestStatus::e404_NotFound);
    } else {
        auto *      app = CPubseqGatewayApp::GetInstance();
        procs->StartRequestTimer(app->GetUVLoop(), m_RequestTimeoutMillisec,
                                 request_id);

        m_GroupsLock.lock();
        m_ProcessorGroups[request_id] = unique_ptr<SProcessorGroup>(procs.release());
        m_GroupsLock.unlock();
    }

    return ret;
}


IPSGS_Processor::EPSGS_StartProcessing
CPSGS_Dispatcher::SignalStartProcessing(IPSGS_Processor *  processor)
{
    // Basically the Cancel() call needs to be invoked for each of the other
    // processors.
    size_t                      request_id = processor->GetRequest()->GetRequestId();
    list<IPSGS_Processor *>     to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    // NOTE: there is no need to do anything with the request timer.
    //       The reply object tracks the activity from the processors so if the
    //       request timer is triggered the last activity will be checked.
    //       Basing on the check results the request timer may be restarted.

    m_GroupsLock.lock();

    if (processor->GetRequest()->NeedTrace()) {
        // Trace sending is under a lock intentionally: to avoid changes in the
        // sequence of processing the signal due to a race when tracing is
        // switched on.
        // In a normal operation the tracing is switched off anyway.
        // false: no need to update the last activity
        processor->GetReply()->SendTrace(
            "Processor: " + processor->GetName() + " (priority: " +
            to_string(processor->GetPriority()) +
            ") signalled start dealing with data for the client",
            processor->GetRequest()->GetStartTimestamp(),
            false);
    }

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist anymore
        m_GroupsLock.unlock();
        return IPSGS_Processor::ePSGS_Cancel;
    }

    // The group found; check that the processor has not been canceled yet
    for (const auto &  proc: procs->second->m_Processors) {
        if (proc.m_Processor == processor) {
            if (proc.m_DispatchStatus == ePSGS_Canceled) {
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
    // data to the client so the other processors should be canceled
    for (auto &  proc: procs->second->m_Processors) {
        if (proc.m_Processor == processor)
            continue;
        if (proc.m_DispatchStatus == ePSGS_Up) {
            proc.m_DispatchStatus = ePSGS_Canceled;
            to_be_canceled.push_back(proc.m_Processor);
        }
    }

    m_GroupsLock.unlock();

    // Call the other processor's Cancel() out of the lock
    for (auto & proc: to_be_canceled) {

        if (processor->GetRequest()->NeedTrace()) {
            // false: no need to update the last activity
            processor->GetReply()->SendTrace(
                "Invoking Cancel() for the processor: " + proc->GetName() +
                " (priority: " + to_string(proc->GetPriority()) + ")",
                processor->GetRequest()->GetStartTimestamp(), false);
        }

        proc->Cancel();
    }

    return IPSGS_Processor::ePSGS_Proceed;
}


void CPSGS_Dispatcher::SignalFinishProcessing(IPSGS_Processor *  processor,
                                              EPSGS_SignalSource  source)
{
    // It needs to check if all the processors finished.
    // If so then the best finish status is used, sent to the client and the
    // group is deleted

    size_t                          request_id = processor->GetRequest()->GetRequestId();
    IPSGS_Processor::EPSGS_Status   best_status = processor->GetStatus();
    bool                            need_trace = processor->GetRequest()->NeedTrace();

    size_t                          canceled_count = 0;
    size_t                          finished_count = 0;
    size_t                          total_count = 0;
    auto                            reply = processor->GetReply();

    m_GroupsLock.lock();

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist any more
        // Basically this should never happened
        m_GroupsLock.unlock();
        return;
    }

    if (best_status == IPSGS_Processor::ePSGS_InProgress) {
        // Call by mistake? The processor reports status as in progress and
        // there is a call to report that it finished
        if (need_trace) {
            // false: no need to update the last activity
            reply->SendTrace(
                "Dispatcher received signal (from " +
                CPSGS_Dispatcher::SignalSourceToString(source) +
                ") that the processor " + processor->GetName() +
                " (priority: " + to_string(processor->GetPriority()) +
                ") finished while the reported status is " +
                IPSGS_Processor::StatusToString(processor->GetStatus()) +
                ". Ignore this call and continue.",
                processor->GetRequest()->GetStartTimestamp(), false);
        }

        m_GroupsLock.unlock();
        return;
    }

    for (auto &  proc: procs->second->m_Processors) {

        ++total_count;

        if (proc.m_Processor == processor) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                proc.m_DispatchStatus = ePSGS_Finished;
                proc.m_FinishStatus = processor->GetStatus();

                ++finished_count;

                reply->PrepareProcessorProgressMessage(
                    processor->GetName(),
                    IPSGS_Processor::StatusToProgressMessage(processor->GetStatus()));

                if (need_trace) {
                    // false: no need to update the last activity
                    reply->SendTrace(
                        "Dispatcher received signal (from " +
                        CPSGS_Dispatcher::SignalSourceToString(source) +
                        ") that the processor " + processor->GetName() +
                        " (priority: " + to_string(processor->GetPriority()) +
                        ") finished with status status " +
                        IPSGS_Processor::StatusToString(processor->GetStatus()),
                        processor->GetRequest()->GetStartTimestamp(),
                        false);
                }

                continue;
            }

            if (proc.m_DispatchStatus == ePSGS_Canceled) {
                proc.m_DispatchStatus = ePSGS_Finished;
                proc.m_FinishStatus = processor->GetStatus();

                reply->PrepareProcessorProgressMessage(
                    processor->GetName(),
                    IPSGS_Processor::StatusToProgressMessage(processor->GetStatus()));

                ++finished_count;

                if (need_trace) {
                    // false: no need to update the last activity
                    reply->SendTrace(
                        "Dispatcher received signal (from " +
                        CPSGS_Dispatcher::SignalSourceToString(source) +
                        ") that the previously canceled processor " +
                        processor->GetName() +
                        " (priority: " + to_string(processor->GetPriority()) +
                        ") finished with status status " +
                        IPSGS_Processor::StatusToString(processor->GetStatus()),
                        processor->GetRequest()->GetStartTimestamp(),
                        false);
                }

                continue;
            }

            // The status is already finished which may mean that
            // it is not the first time call. However it is possible that
            // during the first time the output was not ready so the final PSG
            // chunk has not been sent. Thus we should continue as usual.

            if (need_trace && source == CPSGS_Dispatcher::ePSGS_Processor) {
                // false: no need to update the last activity
                reply->SendTrace(
                    "Dispatcher received signal (from " +
                    CPSGS_Dispatcher::SignalSourceToString(source) +
                    ") that the processor " + processor->GetName() +
                    " (priority: " + to_string(processor->GetPriority()) +
                    ") finished with status status " +
                    IPSGS_Processor::StatusToString(processor->GetStatus()) +
                    " when the dispatcher already knows that the "
                    "processor finished (registered status: " +
                    IPSGS_Processor::StatusToString(proc.m_FinishStatus) + ")",
                    processor->GetRequest()->GetStartTimestamp(),
                    false);
            }
        }

        switch (proc.m_DispatchStatus) {
            case ePSGS_Finished:
                best_status = min(best_status, proc.m_FinishStatus);
                ++finished_count;
                break;
            case ePSGS_Up:
                break;
            case ePSGS_Canceled:
                // The canceled processors still have to call
                // SignalFinishProcessing() on their own so their dispatch
                // status is updated to ePSGS_Finished
                ++canceled_count;
                break;
        }
    }


    // Here: there are still going, finished and canceled processors.
    //       The reply flush must be done if all finished or canceled.
    //       The processors group removal if all finished.

    bool    pre_finished = (finished_count + canceled_count) == total_count;
    bool    fully_finished = finished_count == total_count;

    if (pre_finished) {
        // The reply needs to be flushed if it has not been done yet

        if (!reply->IsFinished() && reply->IsOutputReady()) {
            // Map the processor finish to the request status
            CRequestStatus::ECode   request_status = x_MapProcessorFinishToStatus(best_status);

            if (need_trace) {
                // false: no need to update the last activity
                reply->SendTrace(
                    "Dispatcher: request processing finished; final status: " +
                    to_string(request_status) +
                    ". The processors group will be deleted.",
                    processor->GetRequest()->GetStartTimestamp(), false);
            }

            reply->PrepareReplyCompletion();
            // This will switch the stream to the finished state, i.e.
            // IsFinished() will return true
            reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
            x_PrintRequestStop(processor->GetRequest(), request_status);
        }
    }

    if (fully_finished) {
        // Clear the group after the final chunk is sent
        if (!reply->IsCompleted() && reply->IsFinished()) {
            reply->SetCompleted();
            m_ProcessorGroups.erase(procs);
        }
    }

    m_GroupsLock.unlock();
}


void CPSGS_Dispatcher::SignalConnectionCanceled(size_t      request_id)
{
    // When a connection is canceled there will be no possibility to
    // send anything over the connection. So basically what is needed to do is
    // to cancel processors which have not been canceled yet.

    list<IPSGS_Processor *>     to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    m_GroupsLock.lock();
    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist any more
        m_GroupsLock.unlock();
        return;
    }

    for (auto &  proc: procs->second->m_Processors) {
        if (proc.m_DispatchStatus == ePSGS_Up) {
            proc.m_DispatchStatus = ePSGS_Canceled;
            to_be_canceled.push_back(proc.m_Processor);
        }
    }
    m_GroupsLock.unlock();

    for (auto & proc: to_be_canceled) {
        proc->Cancel();
    }
}


void CPSGS_Dispatcher::x_PrintRequestStop(shared_ptr<CPSGS_Request> request,
                                          CRequestStatus::ECode  status)
{
    CPubseqGatewayApp::GetInstance()->GetCounters().IncrementRequestStopCounter(status);

    if (request->GetRequestContext().NotNull()) {
        request->SetRequestContext();
        CDiagContext::GetRequestContext().SetRequestStatus(status);
        GetDiagContext().PrintRequestStop();
        CDiagContext::GetRequestContext().Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


CRequestStatus::ECode
CPSGS_Dispatcher::x_MapProcessorFinishToStatus(IPSGS_Processor::EPSGS_Status  status) const
{
    switch (status) {
        case IPSGS_Processor::ePSGS_Done:
            return CRequestStatus::e200_Ok;
        case IPSGS_Processor::ePSGS_NotFound:
        case IPSGS_Processor::ePSGS_Canceled:   // not found because it was not let to finish
            return CRequestStatus::e404_NotFound;
        default:
            break;
    }
    // Should not happened
    return CRequestStatus::e500_InternalServerError;
}


void CPSGS_Dispatcher::NotifyRequestFinished(size_t  request_id)
{
    // Low level destroyed the pending request which is associated with the
    // provided request id. So check if the processors group is still held.

    m_GroupsLock.lock();

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        // The processors group does not exist any more
    } else {
        // Remove the group because the low level request structures have
        // already been dismissed

        // Note: it is possible that a processor is on a wait for another
        // processor. This should be taken care of. A Cancel() call will make
        // the locking processor to unlock the waiter
        for (auto &  proc: procs->second->m_Processors) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                proc.m_DispatchStatus = ePSGS_Canceled;
                proc.m_Processor->Cancel();
            }
        }

        m_ProcessorGroups.erase(procs);
    }

    m_GroupsLock.unlock();
}


// Used when shutdown is timed out
void CPSGS_Dispatcher::CancelAll(void)
{
    list<IPSGS_Processor *>     to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    m_GroupsLock.lock();

    for (auto & procs :  m_ProcessorGroups) {
        for (auto &  proc: procs.second->m_Processors) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                to_be_canceled.push_back(proc.m_Processor);
            }
        }
    }

    m_GroupsLock.unlock();

    for (auto & proc: to_be_canceled) {
        proc->Cancel();
    }
}


void CPSGS_Dispatcher::OnRequestTimer(size_t  request_id)
{
    list<IPSGS_Processor *>     to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock
    m_GroupsLock.lock();

    auto    procs = m_ProcessorGroups.find(request_id);
    if (procs == m_ProcessorGroups.end()) {
        m_GroupsLock.unlock();
        return;
    }

    // Check the last activity on the reply object
    list<SProcessorData> &      processors = procs->second->m_Processors;
    auto *                      first_processor = processors.front().m_Processor;
    auto                        reply = first_processor->GetReply();
    auto                        request = first_processor->GetRequest();
    unsigned long               from_last_activity_to_now_ms =
                                    reply->GetTimespanFromLastActivityToNowMks() / 1000;

    if (from_last_activity_to_now_ms < m_RequestTimeoutMillisec) {
        // The request timer is not over since the last activity.
        // The timer needs to be restarted for the rest of the time since the
        // last activity

        if (request->NeedTrace()) {
            // false: no need to update the last activity
            reply->SendTrace("The request timer of " +
                             to_string(m_RequestTimeoutMillisec) +
                             " ms triggered however the last activity with the "
                             "reply was " + to_string(from_last_activity_to_now_ms) +
                             " ms ago. The request timer will be restarted.",
                             request->GetStartTimestamp(), false);
        }

        uint64_t    timeout = m_RequestTimeoutMillisec - from_last_activity_to_now_ms;
        procs->second->RestartTimer(timeout);

        m_GroupsLock.unlock();
    } else {
        // The request timer is over
        if (request->NeedTrace()) {
            // false: no need to update the last activity
            reply->SendTrace("The request timer of " +
                             to_string(m_RequestTimeoutMillisec) +
                             " ms is over. All the not canceled yet processors "
                             "will receive the Cancel() call",
                             request->GetStartTimestamp(), false);
        }

        reply->PrepareRequestTimeoutMessage(
            "Timed out due to prolonged backend(s) inactivity. No response for " +
            to_string(float(m_RequestTimeoutMillisec) / 1000.0) +
            " seconds.");

        // Cancel all active processors in the group
        for (auto &  proc: procs->second->m_Processors) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                to_be_canceled.push_back(proc.m_Processor);
            }
        }

        m_GroupsLock.unlock();

        for (auto & proc: to_be_canceled) {
            proc->Cancel();
        }
    }
}

