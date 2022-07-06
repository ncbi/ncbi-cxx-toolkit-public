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


void request_timer_close_cb(uv_handle_t *   handle)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      request_id = (size_t)(handle->data);

    app->GetProcessorDispatcher()->OnRequestTimerClose(request_id);
}


void erase_processor_group_cb(void *  user_data)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      request_id = (size_t)(user_data);
    app->GetProcessorDispatcher()->EraseProcessorGroup(request_id);
}


void CPSGS_Dispatcher::AddProcessor(unique_ptr<IPSGS_Processor> processor)
{
    if (m_RegisteredProcessors.size() >= MAX_PROCESSOR_GROUPS) {
        // This check is only for the development time when new processors are
        // added. It will never happened in deployment.
        ERR_POST(Critical << "Max number of processor groups to be registered "
                 "has been reached. Please increase the MAX_PROCESSOR_GROUPS "
                 "value. Exiting.");
        exit(0);
    }

    auto *      app = CPubseqGatewayApp::GetInstance();
    string      processor_group_name = processor->GetGroupName();

    auto        it = find(m_RegisteredProcessorGroups.begin(),
                          m_RegisteredProcessorGroups.end(),
                          processor_group_name);
    if (it != m_RegisteredProcessorGroups.end()) {
        // This check is only for the development time when new processors are
        // added. It will never happened in deployment.
        ERR_POST(Critical << "Each processor group must be registered once. "
                 "The group '" << processor_group_name << "' is tried to be "
                 "registered more than once. Exiting.");
        exit(0);
    }

    size_t      index = m_RegisteredProcessors.size();
    size_t      limit = app->GetProcessorMaxConcurrency(processor_group_name);

    m_ProcessorConcurrency[index].m_Limit = limit;

    m_RegisteredProcessorGroups.push_back(processor_group_name);
    m_RegisteredProcessors.push_back(move(processor));
}


list<shared_ptr<IPSGS_Processor>>
CPSGS_Dispatcher::DispatchRequest(shared_ptr<CPSGS_Request> request,
                                  shared_ptr<CPSGS_Reply> reply)
{
    list<shared_ptr<IPSGS_Processor>>   ret;
    size_t                              proc_count = m_RegisteredProcessors.size();
    TProcessorPriority                  priority = proc_count;
    auto                                request_id = request->GetRequestId();
    unique_ptr<SProcessorGroup>         procs(new SProcessorGroup(request_id));

    for (auto const &  proc : m_RegisteredProcessors) {
        // First check the limit for the number of processors
        size_t      proc_index = proc_count - priority;
        size_t      limit = m_ProcessorConcurrency[proc_index].m_Limit;

        size_t      current_count = m_ProcessorConcurrency[proc_index].GetCurrentCount();

        if (current_count >= limit) {
            m_ProcessorConcurrency[proc_index].IncrementLimitReachedCount();

            if (request->NeedTrace()) {
                // false: no need to update the last activity
                reply->SendTrace("Processor: " + proc->GetName() +
                                 " will not be tried to create because"
                                 " the processor group limit has been exceeded."
                                 " Limit: " + to_string(limit) +
                                 " Current count: " + to_string(current_count),
                                 request->GetStartTimestamp(), false);
            }
            --priority;
            continue;
        }

        if (request->NeedTrace()) {
            // false: no need to update the last activity
            reply->SendTrace("Try to create processor: " + proc->GetName(),
                             request->GetStartTimestamp(), false);
        }
        shared_ptr<IPSGS_Processor> p(proc->CreateProcessor(request, reply,
                                                            priority));
        if (p) {
            m_ProcessorConcurrency[proc_index].IncrementCurrentCount();

            ret.push_back(p);

            procs->m_Processors.emplace_back(p, ePSGS_Up,
                                             IPSGS_Processor::ePSGS_InProgress);

            if (request->NeedTrace()) {
                // false: no need to update the last activity
                reply->SendTrace("Processor " + p->GetName() +
                                 " has been created successfully (priority: " +
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
        string  msg = "No matching processors found or processor limits "
                      "exceeded to serve the request";
        PSG_TRACE(msg);

        reply->PrepareReplyMessage(msg, CRequestStatus::e404_NotFound,
                                   ePSGS_NoProcessor, eDiag_Error);
        reply->PrepareReplyCompletion(request->GetStartTimestamp());

        reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
        reply->SetCompleted();
        x_PrintRequestStop(request, CRequestStatus::e404_NotFound);
    } else {
        auto *  grp = procs.release();
        pair<size_t,
             unique_ptr<SProcessorGroup>>   req_grp = make_pair(request_id,
                                                                unique_ptr<SProcessorGroup>(grp));

        size_t      bucket_index = x_GetBucketIndex(request_id);
        m_GroupsLock[bucket_index].lock();
        m_ProcessorGroups[bucket_index].insert(m_ProcessorGroups[bucket_index].end(),
                                               move(req_grp));
        m_GroupsLock[bucket_index].unlock();
    }

    return ret;
}


// Start of the timer is done at http_server_transport.cpp PostponedStart
// This guarantees that the timer is created in the same uv loop (i.e. working
// thread) as the processors will use regardless of:
// - the request started processed right away
// - the request was postponed and started later from a postopned list
void CPSGS_Dispatcher::StartRequestTimer(size_t  request_id)
{
    size_t                      bucket_index = x_GetBucketIndex(request_id);
    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs != m_ProcessorGroups[bucket_index].end()) {
        auto *      app = CPubseqGatewayApp::GetInstance();
        procs->second->StartRequestTimer(app->GetUVLoop(), m_RequestTimeoutMillisec);
    }

    m_GroupsLock[bucket_index].unlock();
}


IPSGS_Processor::EPSGS_StartProcessing
CPSGS_Dispatcher::SignalStartProcessing(IPSGS_Processor *  processor)
{
    // Basically the Cancel() call needs to be invoked for each of the other
    // processors.
    bool                        need_trace = processor->GetRequest()->NeedTrace();
    size_t                      request_id = processor->GetRequest()->GetRequestId();
    size_t                      bucket_index = x_GetBucketIndex(request_id);
    IPSGS_Processor *           current_proc = nullptr;
    vector<IPSGS_Processor *>   to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    // NOTE: there is no need to do anything with the request timer.
    //       The reply object tracks the activity from the processors so if the
    //       request timer is triggered the last activity will be checked.
    //       Basing on the check results the request timer may be restarted.

    m_GroupsLock[bucket_index].lock();

    if (need_trace) {
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

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs == m_ProcessorGroups[bucket_index].end()) {
        // The processors group does not exist anymore
        m_GroupsLock[bucket_index].unlock();
        return IPSGS_Processor::ePSGS_Cancel;
    }

    // The group found; check that the processor has not been canceled yet
    for (const auto &  proc: procs->second->m_Processors) {
        if (proc.m_Processor.get() == processor) {
            if (proc.m_DispatchStatus == ePSGS_Canceled) {
                // The other processor has already called Cancel() for this one
                m_GroupsLock[bucket_index].unlock();
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
        current_proc = proc.m_Processor.get();

        if (current_proc != processor) {
            if (proc.m_DispatchStatus == ePSGS_Up) {
                proc.m_DispatchStatus = ePSGS_Canceled;
                to_be_canceled.push_back(current_proc);
            }
        }
    }

    // Memorize which processor succeeded with starting supplying data
    procs->second->m_StartedProcessing = processor;

    m_GroupsLock[bucket_index].unlock();

    // Call the other processor's Cancel() out of the lock
    for (auto & proc: to_be_canceled) {

        if (need_trace) {
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

    if (uv_thread_self() != processor->GetUVThreadId()) {
        if (processor->IsUVThreadAssigned()) {
            PSG_ERROR("SignalFinishProcessing() is called not from an assigned "
                      "thread (and libuv loop). "
                      "Current thread: " << uv_thread_self() <<
                      " Assigned thread: " << processor->GetUVThreadId() <<
                      " Processor: " << processor->GetName() <<
                      " Call source: " << CPSGS_Dispatcher::SignalSourceToString(source));
        } else {
            // The worker thread has not been assigned yet. This is the case
            // when a processor signals finishing before it was started. That
            // possible in a scenario like this:
            // - some requests have been backlogged
            // - a connection has been abruptly dropped
            // - Cancel() is called for backlogged (not started yet) processors
            // - processors signal finishing
        }
    }

    auto                            reply = processor->GetReply();
    auto                            request = processor->GetRequest();
    size_t                          request_id = request->GetRequestId();
    size_t                          bucket_index = x_GetBucketIndex(request_id);
    IPSGS_Processor::EPSGS_Status   best_status = processor->GetStatus();
    IPSGS_Processor::EPSGS_Status   processor_status = processor->GetStatus();
    bool                            need_trace = request->NeedTrace();
    bool                            started_processor_finished = false;

    size_t                          finishing_count = 0;
    size_t                          finished_count = 0;

    if (processor_status == IPSGS_Processor::ePSGS_InProgress) {
        // Call by mistake? The processor reports status as in progress and
        // there is a call to report that it finished
        if (need_trace) {
            x_SendTrace(
                "Dispatcher received signal (from " +
                CPSGS_Dispatcher::SignalSourceToString(source) +
                ") that the processor " + processor->GetName() +
                " (priority: " + to_string(processor->GetPriority()) +
                ") finished while the reported status is " +
                IPSGS_Processor::StatusToString(processor_status) +
                ". Ignore this call and continue.",
                request, reply);
        }
        return;
    }

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs == m_ProcessorGroups[bucket_index].end()) {
        // The processors group does not exist any more
        // Basically this should never happened
        m_GroupsLock[bucket_index].unlock();
        return;
    }

    for (auto &  proc: procs->second->m_Processors) {

        if (proc.m_Processor.get() == processor) {

            if (source == ePSGS_Fromework) {
                // This call is when the framework notices that the processor
                // reports something not InProgress (like error, cancel,
                // timeout or found).

                if (need_trace) {
                    x_SendTrace(
                        "Dispatcher received signal (from framework)"
                        " that the processor " + processor->GetName() +
                        " (priority: " + to_string(processor->GetPriority()) +
                        ") has changed the status to " +
                        IPSGS_Processor::StatusToString(processor_status),
                        request, reply);
                }

                // No changes in the dispatch status to ePSGS_Finished
                // It can be changed only to ePSGS_Canceled
                switch (proc.m_DispatchStatus) {
                    case ePSGS_Finished:
                        // Most probably some kind of timing: dispatcher know
                        // the processor has already finished and there is one
                        // more report
                        ++finished_count;
                        break;
                    case ePSGS_Up:
                        if (processor_status != IPSGS_Processor::ePSGS_InProgress)
                            ++finishing_count;
                        break;
                    case ePSGS_Canceled:
                        ++finishing_count;
                        break;
                }
            } else {
                // This is source == ePSGS_Processor, so the dispatch status
                // may need to be changed
                ++finished_count;

                switch (proc.m_DispatchStatus) {
                    case ePSGS_Finished:
                        // The processor signals finishing second time. It is OK,
                        // just ignore that
                        if (need_trace) {
                            x_SendTrace(
                                "Dispatcher received signal (from processor)"
                                " that the processor " + processor->GetName() +
                                " (priority: " + to_string(processor->GetPriority()) +
                                ") finished with status " +
                                IPSGS_Processor::StatusToString(processor_status) +
                                " when the dispatcher already knows that the "
                                "processor finished (registered status: " +
                                IPSGS_Processor::StatusToString(proc.m_FinishStatus) + ")",
                                request, reply);
                        }

                        // Note: there is no update of the dispatcher processor
                        // status. The only first report from the processor is
                        // taken into account
                        break;
                    case ePSGS_Up:
                    case ePSGS_Canceled:
                        proc.m_FinishStatus = processor_status;

                        if (proc.m_DispatchStatus == ePSGS_Up) {
                            if (need_trace) {
                                x_SendTrace(
                                    "Dispatcher received signal (from processor)"
                                    " that the processor " + processor->GetName() +
                                    " (priority: " + to_string(processor->GetPriority()) +
                                    ") finished with status " +
                                    IPSGS_Processor::StatusToString(processor_status),
                                    request, reply);
                            }
                        } else {
                            // This is a canceled processor final report
                            if (need_trace) {
                                x_SendTrace(
                                    "Dispatcher received signal (from processor)"
                                    " that the previously canceled processor " +
                                    processor->GetName() +
                                    " (priority: " + to_string(processor->GetPriority()) +
                                    ") finished with status " +
                                    IPSGS_Processor::StatusToString(processor_status),
                                    request, reply);
                            }
                        }

                        proc.m_DispatchStatus = ePSGS_Finished;
                        x_DecrementConcurrencyCounter(processor);
                        x_SendProgressMessage(processor_status, processor,
                                              request, reply);
                        break;
                } // End of (proc.m_DispatchStatus) switch

                best_status = min(best_status, proc.m_FinishStatus);

            } // End of report source condition handling

            if (processor == procs->second->m_StartedProcessing) {
                if (proc.m_DispatchStatus == ePSGS_Finished) {
                    started_processor_finished = true;
                }
            }

            continue;
        } // End of (proc.m_Processor.get() == processor) condition

        switch (proc.m_DispatchStatus) {
            case ePSGS_Finished:
                best_status = min(best_status, proc.m_FinishStatus);
                ++finished_count;
                break;
            case ePSGS_Up:
                if (proc.m_Processor->GetStatus() != IPSGS_Processor::ePSGS_InProgress)
                    ++finishing_count;
                break;
            case ePSGS_Canceled:
                // The canceled processors still have to call
                // SignalFinishProcessing() on their own so their dispatch
                // status is updated to ePSGS_Finished
                ++finishing_count;
                break;
        }
    }


    // Here: there are still going, finished and canceled processors.
    //       The reply flush must be done if all finished or canceled.
    //       The processors group removal if all finished.

    size_t  total_count = procs->second->m_Processors.size();
    bool    pre_finished = (finished_count + finishing_count) == total_count;
    bool    fully_finished = finished_count == total_count;
    bool    need_unlock = true;
    bool    safe_to_delete = false;

    if (fully_finished) {
        procs->second->m_AllProcessorsFinished = true;
        safe_to_delete = procs->second->IsSafeToDelete();
    }

    if (pre_finished) {
        // Timer is not needed anymore
        // Note: it is safe to have this call even is the processor has not
        // started yet. If so then the timer is not created thus the call leads
        // to an immediate return
        procs->second->StopRequestTimer();


        if (procs->second->m_StartedProcessing == nullptr) {
            // 1. There is no start data notification in case of annotation
            // requests. All the processors can supply data
            // 2. None of the processors started to supply data; it's 404
            started_processor_finished = true;
        }

        if (!reply->IsFinished() && reply->IsOutputReady() && started_processor_finished) {
            // Map the processor finish to the request status
            CRequestStatus::ECode   request_status = x_MapProcessorFinishToStatus(best_status);

            if (need_trace) {
                x_SendTrace(
                    "Dispatcher: request processing finished; "
                    "final status: " + to_string(request_status) +
                    ". The processors group will be deleted later.",
                    request, reply);
            }

            reply->PrepareReplyCompletion(request->GetStartTimestamp());
            procs->second->m_FinallyFlushed = true;

            // To avoid flushing and stopping request under a lock
            m_GroupsLock[bucket_index].unlock();
            need_unlock = false;

            // This will switch the stream to the finished state, i.e.
            // IsFinished() will return true
            reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
            reply->SetCompleted();
            x_PrintRequestStop(request, request_status);
        }
    }

    if (need_unlock) {
        m_GroupsLock[bucket_index].unlock();
    }

    if (safe_to_delete) {
        // Note: erasing the group right away is not good because the call
        // comes from a processor and it might be not the last thing the
        // processor does. So the destruction needs to be delayed till the
        // next libuv loop iteration
        if (processor->IsUVThreadAssigned()) {
            // The processor has been started (Process() was called)
            auto *  app = CPubseqGatewayApp::GetInstance();
            app->GetUvLoopBinder(processor->GetUVThreadId()).PostponeInvoke(
                    erase_processor_group_cb, (void*)(request_id));
        } else {
            // The processor was canceled before it started
            // The group can be removed right away
            EraseProcessorGroup(request_id);
        }
    }
}


void CPSGS_Dispatcher::SignalConnectionCanceled(size_t      request_id)
{
    // When a connection is canceled there will be no possibility to
    // send anything over the connection. So basically what is needed to do is
    // to cancel processors which have not been canceled yet.

    size_t                          bucket_index = x_GetBucketIndex(request_id);
    vector<IPSGS_Processor *>       to_be_canceled;     // To avoid calling Cancel()
                                                        // under the lock

    m_GroupsLock[bucket_index].lock();
    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs == m_ProcessorGroups[bucket_index].end()) {
        // The processors group does not exist any more
        m_GroupsLock[bucket_index].unlock();
        return;
    }

    for (auto &  proc: procs->second->m_Processors) {
        if (proc.m_DispatchStatus == ePSGS_Up) {
            proc.m_DispatchStatus = ePSGS_Canceled;
            to_be_canceled.push_back(proc.m_Processor.get());
        }
    }
    m_GroupsLock[bucket_index].unlock();

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


void
CPSGS_Dispatcher::x_SendTrace(const string &  msg,
                              shared_ptr<CPSGS_Request> request,
                              shared_ptr<CPSGS_Reply> reply)
{
    // false: no need to update the last activity
    reply->SendTrace(msg, request->GetStartTimestamp(), false);
}


void
CPSGS_Dispatcher::x_SendProgressMessage(IPSGS_Processor::EPSGS_Status  finish_status,
                                        IPSGS_Processor *  processor,
                                        shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply)
{
    if (finish_status == IPSGS_Processor::ePSGS_Timeout ||
        finish_status == IPSGS_Processor::ePSGS_Error ||
        request->NeedProcessorEvents()) {
        reply->PrepareProcessorProgressMessage(
            processor->GetName(),
            IPSGS_Processor::StatusToProgressMessage(finish_status));
    }
}


void CPSGS_Dispatcher::NotifyRequestFinished(size_t  request_id)
{
    // Low level destroyed the pending request which is associated with the
    // provided request id. So check if the processors group is still held.


    size_t                      bucket_index = x_GetBucketIndex(request_id);
    vector<IPSGS_Processor *>   to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock
    IPSGS_Processor *           first_proc = nullptr;

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs != m_ProcessorGroups[bucket_index].end()) {
        // Remove the group because the low level request structures have
        // already been dismissed

        // m_FinallyFlushed => that was a normal finish for the group of
        // processors. They have reported finish finish with data and the reply
        // was flushed and closed.
        if (!procs->second->m_FinallyFlushed) {

            first_proc = procs->second->m_Processors[0].m_Processor.get();

            // Note: it is possible that a processor is on a wait for another
            // processor. This should be taken care of. A Cancel() call will make
            // the locking processor to unlock the waiter
            for (auto &  proc: procs->second->m_Processors) {
                if (proc.m_DispatchStatus == ePSGS_Up) {
                    if (proc.m_Processor->GetStatus() == IPSGS_Processor::ePSGS_InProgress) {
                        to_be_canceled.push_back(proc.m_Processor.get());
                    }
                }
            }

            // The low level destruction also means that there will be no
            // libh2o signal that the response generator memory can be
            // disposed. Set another flag that it could be safe to destrow the
            // group.
            procs->second->m_LowLevelClose = true;
        }
    }

    m_GroupsLock[bucket_index].unlock();

    for (auto & proc: to_be_canceled) {
        proc->Cancel();
    }

    if (first_proc != nullptr) {
        // It is safe to schedule group erase. There will be a check if it is
        // safe to erase a group.
        // The scheduling is needed in case the processors report finishing
        // quicker than the low level reports closing activity.
        if (first_proc->IsUVThreadAssigned()) {
            // The processor has been started (Process() was called)
            auto *  app = CPubseqGatewayApp::GetInstance();
            app->GetUvLoopBinder(first_proc->GetUVThreadId()).PostponeInvoke(
                    erase_processor_group_cb, (void*)(request_id));
        } else {
            // The processor was canceled before it started
            // The group can be removed right away
            EraseProcessorGroup(request_id);
        }
    }
}


// Used when shutdown is timed out
void CPSGS_Dispatcher::CancelAll(void)
{
    vector<IPSGS_Processor *>   to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    for (size_t  bucket_index = 0; bucket_index < PROC_BUCKETS; ++bucket_index) {
        m_GroupsLock[bucket_index].lock();

        for (auto & procs :  m_ProcessorGroups[bucket_index]) {
            for (auto &  proc: procs.second->m_Processors) {
                if (proc.m_DispatchStatus == ePSGS_Up) {
                    to_be_canceled.push_back(proc.m_Processor.get());
                }
            }
        }

        m_GroupsLock[bucket_index].unlock();

        for (auto & proc: to_be_canceled) {
            proc->Cancel();
        }
        to_be_canceled.clear();
    }
}


void CPSGS_Dispatcher::OnRequestTimer(size_t  request_id)
{
    size_t                      bucket_index = x_GetBucketIndex(request_id);
    vector<IPSGS_Processor *>   to_be_canceled;     // To avoid calling Cancel()
                                                    // under the lock

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs == m_ProcessorGroups[bucket_index].end()) {
        m_GroupsLock[bucket_index].unlock();
        return;
    }

    // Check the last activity on the reply object
    vector<SProcessorData> &    processors = procs->second->m_Processors;
    auto *                      first_processor = processors.front().m_Processor.get();
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

        m_GroupsLock[bucket_index].unlock();
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
                to_be_canceled.push_back(proc.m_Processor.get());
            }
        }

        m_GroupsLock[bucket_index].unlock();

        for (auto & proc: to_be_canceled) {
            proc->Cancel();
        }
    }
}


void CPSGS_Dispatcher::OnLibh2oFinished(size_t  request_id)
{
    size_t              bucket_index = x_GetBucketIndex(request_id);

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs != m_ProcessorGroups[bucket_index].end()) {
        procs->second->m_Libh2oFinished = true;

        if (procs->second->IsSafeToDelete()) {
            auto    processor = procs->second->m_Processors[0].m_Processor;

            if (processor->IsUVThreadAssigned()) {
                // The processor has been started (Process() was called)
                auto *  app = CPubseqGatewayApp::GetInstance();
                app->GetUvLoopBinder(processor->GetUVThreadId()).PostponeInvoke(
                            erase_processor_group_cb, (void*)(request_id));
            } else {
                // The processor was canceled before it started
                // The group can be removed right away
                EraseProcessorGroup(request_id);
            }
        }
    }
    m_GroupsLock[bucket_index].unlock();
}


void CPSGS_Dispatcher::EraseProcessorGroup(size_t  request_id)
{
    size_t              bucket_index = x_GetBucketIndex(request_id);
    SProcessorGroup *   group = nullptr;

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs != m_ProcessorGroups[bucket_index].end()) {

        if (!procs->second->IsSafeToDelete()) {
            m_GroupsLock[bucket_index].unlock();
            return;
        }

        group = procs->second.release();

        // Without releasing the pointer it may lead to a processor
        // destruction under the lock. This causes a contention on the mutex
        // and performance degrades.
        m_ProcessorGroups[bucket_index].erase(procs);
    }
    m_GroupsLock[bucket_index].unlock();

    if (group != nullptr) {
        delete group;
    }
}


void CPSGS_Dispatcher::OnRequestTimerClose(size_t  request_id)
{
    size_t              bucket_index = x_GetBucketIndex(request_id);

    m_GroupsLock[bucket_index].lock();

    auto    procs = m_ProcessorGroups[bucket_index].find(request_id);
    if (procs != m_ProcessorGroups[bucket_index].end()) {
        // It could be that the timer is closed first or
        // the processors finished first.
        // Set the flag first
        procs->second->m_TimerClosed = true;

        if (procs->second->IsSafeToDelete()) {
            // Can erase group right away
            m_GroupsLock[bucket_index].unlock();
            EraseProcessorGroup(request_id);
            return;
        }
    }
    m_GroupsLock[bucket_index].unlock();
}


void CPSGS_Dispatcher::x_DecrementConcurrencyCounter(IPSGS_Processor *  processor)
{
    size_t      proc_count = m_RegisteredProcessors.size();
    size_t      proc_priority = processor->GetPriority();
    size_t      proc_index = proc_count - proc_priority;

    m_ProcessorConcurrency[proc_index].DecrementCurrentCount();
}


map<string, size_t>  CPSGS_Dispatcher::GetConcurrentCounters(void)
{
    map<string, size_t>     ret;    // name -> current counter

    for (size_t  index = 0; index < m_RegisteredProcessorGroups.size(); ++index) {
        m_ProcessorConcurrency[index].Lock();

        ret[m_RegisteredProcessorGroups[index]] = m_ProcessorConcurrency[index].m_CurrentCount;
        ret[m_RegisteredProcessorGroups[index] + "-Limit"] = m_ProcessorConcurrency[index].m_LimitReachedCount;

        m_ProcessorConcurrency[index].Unlock();
    }

    return ret;
}


size_t CPSGS_Dispatcher::GetActiveProcessorGroups(void)
{
    size_t      cnt=0;

    for (size_t  index=0; index < PROC_BUCKETS; ++index) {
        m_GroupsLock[index].lock();
        cnt += m_ProcessorGroups[index].size();
        m_GroupsLock[index].unlock();
    }
    return cnt;
}


bool CPSGS_Dispatcher::IsGroupAlive(size_t  request_id)
{
    size_t              bucket_index = x_GetBucketIndex(request_id);
    bool                found = false;

    m_GroupsLock[bucket_index].lock();
    found = (m_ProcessorGroups[bucket_index].find(request_id) != m_ProcessorGroups[bucket_index].end());
    m_GroupsLock[bucket_index].unlock();
    return found;
}

