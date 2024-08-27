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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Event loop for PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_evloop.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processor.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <util/thread_pool.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


/////////////////////////////////////////////////////////////////////////////
// Object references held:
//
// CPSGL_Queue:
//     is created by PSG loader
//     is destructed by PSG loader
//   uses CPSGL_RequestTracker(s) for callback while they are registered by CPSGL_QueueGuard
//
// CPSGL_QueueGuard:
//     is created only on stack by some caller method
//     is destructed at the end of the caller
//     does cleanup in destructor:
//       stops unfinished background task(s)
//       deregisters CPSGL_RequestTracker from(s) CPSGL_Queue
//   holds CPSGL_Queue
//   holds CPSGL_RequestTracker(s) until they are processed and results are obtained by the caller
//
// CPSGL_RequestTracker:
//     is created by CPSGL_QueueGuard
//     is destructed by CPSGL_QueueGuard
//   holds CPSG_Request
//   holds CPSG_Reply
//   holds CPSGL_Processor
//   holds background task(s)
//   uses CPSGL_QueueGuard for starting background threads
//   uses CPSGL_QueueGuard to notify end of processing
//
// background task:
//     is created by CPSGL_RequestTracker
//     is destructed by last reference
//   holds CPSGL_RequestTracker until finished
//
// CPSGL_Processor:
//     is created by the caller
//     is destructed by the caller
//   holds results necessary by the caller
//
// CPSGL_ResultGuard:
//     is created by CPSGL_QueueGuard
//     is destructed by the caller
//   holds CPSGL_Processor
//
//
/////////////////////////////////////////////////////////////////////////////
// NOTE:
//
// Runtime circual reference will appear between CPSGL_RequestTracker and its
// background task(s).
// The circle will be broken either:
//   1. when the background task finishes
//   2. by CPSGL_QueueGuard destructor
//     - it will also request to cancel the task(s)
//

/////////////////////////////////////////////////////////////////////////////
// CPSGL_Queue
/////////////////////////////////////////////////////////////////////////////


CPSGL_Queue::CPSGL_Queue(const string& service_name)
    : m_EventLoop(service_name,
                  bind(&CPSGL_Queue::ProcessItemCallback, this, placeholders::_1, placeholders::_2),
                  bind(&CPSGL_Queue::ProcessReplyCallback, this, placeholders::_1, placeholders::_2)),
      m_EventLoopThread(&CPSG_EventLoop::Run, ref(m_EventLoop), CDeadline::eInfinite)
{
}


CPSGL_Queue::~CPSGL_Queue()
{
    m_EventLoop.Reset();
    m_EventLoopThread.join();
}


void CPSGL_Queue::SetRequestContext(const CRef<CRequestContext>& context)
{
    m_RequestContext = context;
}


void CPSGL_Queue::RegisterRequest(CPSGL_RequestTracker* tracker)
{
    CFastMutexGuard guard(m_TrackerMapMutex);
    _VERIFY(m_TrackerMap.insert(make_pair(tracker->GetRequest().get(), tracker)).second);
}


void CPSGL_Queue::DeregisterRequest(const CPSGL_RequestTracker* tracker)
{
    CFastMutexGuard guard(m_TrackerMapMutex);
    m_TrackerMap.erase(tracker->GetRequest().get());
}


CRef<CPSGL_RequestTracker> CPSGL_Queue::GetTracker(const shared_ptr<CPSG_Reply>& reply)
{
    CFastMutexGuard guard(m_TrackerMapMutex);
    auto iter = m_TrackerMap.find(reply->GetRequest().get());
    if ( iter == m_TrackerMap.end() ) {
        return null;
    }
    return Ref(iter->second);
}


inline
CRef<CPSGL_RequestTracker> CPSGL_Queue::GetTracker(const shared_ptr<CPSG_ReplyItem>& item)
{
    return GetTracker(item->GetReply());
}


bool CPSGL_Queue::SendRequest(const CRef<CPSGL_RequestTracker>& tracker)
{
    shared_ptr<CPSG_Request> request = tracker->GetRequest();
    if ( m_RequestContext ) {
        request->SetRequestContext(m_RequestContext);
    }
    return m_EventLoop.SendRequest(request, CDeadline::eInfinite);
}


void CPSGL_Queue::ProcessItemCallback(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item)
{
    if ( auto tracker = GetTracker(item) ) {
        tracker->ProcessItemCallback(status, item);
    }
}    


void CPSGL_Queue::ProcessReplyCallback(EPSG_Status status,
                                       const shared_ptr<CPSG_Reply>& reply)
{
    if ( auto tracker = GetTracker(reply) ) {
        tracker->ProcessReplyCallback(status, reply);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_RequestTracker
/////////////////////////////////////////////////////////////////////////////


CPSGL_RequestTracker::CPSGL_RequestTracker(CPSGL_QueueGuard& queue_guard,
                                           const shared_ptr<CPSG_Request>& request,
                                           const CRef<CPSGL_Processor>& processor,
                                           size_t index)
    : m_QueueGuard(queue_guard),
      m_Request(request),
      m_Processor(processor),
      m_Index(index),
      m_Status(CThreadPool_Task::eExecuting),
      m_ReplyStatus(EPSG_Status::eInProgress),
      m_NeedsFinalization(false),
      m_BackgroundTasksSemaphore(0, kMax_UInt),
      m_BackgroundItemTasks(0)
{
}


CPSGL_RequestTracker::~CPSGL_RequestTracker()
{
    CancelBackgroundTasks();
}


class CPSGL_RequestTracker::CBackgroundTask : public CThreadPool_Task
{
public:
    CBackgroundTask(CPSGL_RequestTracker* tracker,
                    EPSG_Status item_status,
                    const shared_ptr<CPSG_ReplyItem>& item)
        : m_Tracker(tracker),
          m_ItemStatus(item_status),
          m_Item(item)
    {
    }
    explicit
    CBackgroundTask(CPSGL_RequestTracker* tracker) // for reply processing
        : m_Tracker(tracker),
          m_ItemStatus(EPSG_Status::eError)
    {
    }

    static bool s_IsFinished(EStatus status)
    {
        return status >= eCompleted;
    }
    static bool s_IsAborted(EStatus status)
    {
        return status > eCompleted;
    }
    
    EStatus Execute() override
    {
        if ( m_Item ) {
            return m_Tracker->BackgroundProcessItemCallback(this, m_ItemStatus, m_Item);
        }
        else {
            return m_Tracker->BackgroundProcessReplyCallback(this);
        }
    }

    void OnStatusChange(EStatus old_task_status) override
    {
        m_Tracker->OnStatusChange(this, old_task_status);
    }
    
    CRef<CPSGL_RequestTracker> m_Tracker;
    EPSG_Status m_ItemStatus;
    shared_ptr<CPSG_ReplyItem> m_Item;
};


void CPSGL_RequestTracker::Cancel()
{
    MarkAsCanceled();
    CancelBackgroundTasks();
    Reset();
}


void CPSGL_RequestTracker::Reset()
{
    m_Reply.reset();
    m_Processor.Reset();
    m_Request.reset();
}


void CPSGL_RequestTracker::CancelBackgroundTasks()
{
    TBackgroundTasks tasks;
    {{
        CFastMutexGuard guard(m_TrackerMutex);
        tasks = m_BackgroundTasks;
    }}
    for ( auto& task : tasks ) {
        task.GetNCObject().RequestToCancel();
    }
    WaitForBackgroundTasks();
}


void CPSGL_RequestTracker::WaitForBackgroundTasks()
{
    for ( ;; ) {
        {{
            CFastMutexGuard guard(m_TrackerMutex);
            if ( m_BackgroundTasks.empty() ) {
                return;
            }
        }}
        m_BackgroundTasksSemaphore.Wait();
    }
}


inline
void CPSGL_RequestTracker::QueueInBackground(const CRef<CBackgroundTask>& task)
{
    {{
        CFastMutexGuard guard(m_TrackerMutex);
        m_BackgroundTasks.insert(task);
        if ( task-> m_Item ) {
            ++m_BackgroundItemTasks;
        }
    }}
    m_QueueGuard.m_ThreadPool.AddTask(task.GetNCPointer());
}


void CPSGL_RequestTracker::StartProcessItemInBackground(EPSG_Status status,
                                                        const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::StartProcessItemInBackground()");
    _ASSERT(!m_Reply);
    QueueInBackground(Ref(new CBackgroundTask(this, status, item)));
}


void CPSGL_RequestTracker::StartProcessReplyInBackground()
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::StartProcessReplyInBackground()");
    _ASSERT(m_BackgroundItemTasks == 0);
    _ASSERT(m_Reply);
    QueueInBackground(Ref(new CBackgroundTask(this)));
}


void CPSGL_RequestTracker::OnStatusChange(CBackgroundTask* task,
                                          CThreadPool_Task::EStatus old_task_status)
{
    auto new_task_status = task->GetStatus();
    if ( CBackgroundTask::s_IsFinished(new_task_status) ) {
        {{
            CFastMutexGuard guard(m_TrackerMutex);
            m_BackgroundTasks.erase(Ref(task));
        }}
        if ( CBackgroundTask::s_IsAborted(new_task_status) ) {
            // save error status
            m_Status = new_task_status;
            m_QueueGuard.MarkAsFinished(Ref(this));
        }
        m_BackgroundTasksSemaphore.Post();
    }
}


void CPSGL_RequestTracker::MarkAsFinished(CThreadPool_Task::EStatus status)
{
    _ASSERT(CBackgroundTask::s_IsFinished(status));
    {{
        CFastMutexGuard guard(m_TrackerMutex);
        CThreadPool_Task::EStatus old_status = m_Status;
        if ( status > old_status ) {
            m_Status = status;
        }
    }}
    m_QueueGuard.MarkAsFinished(Ref(this));
}


inline
void CPSGL_RequestTracker::MarkAsCompleted()
{
    MarkAsFinished(CThreadPool_Task::eCompleted);
}


inline
void CPSGL_RequestTracker::MarkAsFailed()
{
    MarkAsFinished(CThreadPool_Task::eFailed);
}


inline
void CPSGL_RequestTracker::MarkAsNeedsFinalization()
{
    _ASSERT(!m_NeedsFinalization);
    m_NeedsFinalization = true;
    MarkAsFinished(CThreadPool_Task::eCompleted);
}


inline
void CPSGL_RequestTracker::MarkAsCanceled()
{
    MarkAsFinished(CThreadPool_Task::eCanceled);
}


void CPSGL_RequestTracker::ProcessItemCallback(EPSG_Status status,
                                               const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessItemCallback()");
    try {
        _ASSERT(item);
        auto result = m_Processor->ProcessItemFast(status, item);
        if ( result == CPSGL_Processor::eToNextStage ) {
            // queue background processing
            StartProcessItemInBackground(status, item);
        }
        else if ( result != CPSGL_Processor::eProcessed ) {
            ERR_POST("CPSGDataLoader: failed processing reply item: "<<result);
            MarkAsFailed();
        }
    }
    catch ( exception& exc ) {
        ERR_POST("CPSGDataLoader: exception while processing reply item: "<<exc.what());
        MarkAsFailed();
    }
}


void CPSGL_RequestTracker::ProcessReplyCallback(EPSG_Status status,
                                                const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback()");
    try {
        _ASSERT(reply);
        {{
            // items may be still being processed in background
            // and we cannot yet process reply in this case
            CFastMutexGuard guard(m_TrackerMutex);
            _ASSERT(!m_Reply);
            m_ReplyStatus = status;
            m_Reply = reply;
            if ( m_BackgroundItemTasks > 0 ) {
                // not all items processed
                // the reply will be processed by the last item task
                return;
            }
        }}
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback(): ProcessReplyFast()");
        auto result = m_Processor->ProcessReplyFast(status, reply);
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback(): ProcessReplyFast(): "<<result);
        if ( result == CPSGL_Processor::eToNextStage ) {
            // queue processing
            StartProcessReplyInBackground();
        }
        else if ( result != CPSGL_Processor::eProcessed ) {
            ERR_POST("CPSGDataLoader: failed processing reply: "<<result);
            MarkAsFailed();
        }
        else {
            MarkAsCompleted();
        }
    }
    catch ( exception& exc ) {
        ERR_POST("CPSGDataLoader: exception while processing reply: "<<exc.what());
        MarkAsFailed();
    }
}


CThreadPool_Task::EStatus
CPSGL_RequestTracker::BackgroundProcessItemCallback(CBackgroundTask* task,
                                                    EPSG_Status status,
                                                    const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback()");
    try {
        {{
            // check if canceled
            CFastMutexGuard guard(m_TrackerMutex);
            if ( CBackgroundTask::s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                _ASSERT(m_Status == CThreadPool_Task::eCanceled ||
                        m_Status == CThreadPool_Task::eFailed);
                return m_Status;
            }
        }}
        {{
            // process item
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessItemSlow()");
            auto result = m_Processor->ProcessItemSlow(status, item);
            if ( result != CPSGL_Processor::eProcessed ) {
                ERR_POST("CPSGDataLoader: failed processing reply item: "<<result);
                MarkAsFailed();
                return CThreadPool_Task::eFailed;
            }
        }}
        {{
            // check if result needs to be processed too
            CFastMutexGuard guard(m_TrackerMutex);
            if ( CBackgroundTask::s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                _ASSERT(m_Status == CThreadPool_Task::eCanceled ||
                        m_Status == CThreadPool_Task::eFailed);
                return m_Status;
            }
            if ( --m_BackgroundItemTasks > 0 || !m_Reply ) {
                // either there are other background item tasks
                // or reply wasn't received yet
                return CThreadPool_Task::eCompleted;
            }
            _ASSERT(m_BackgroundTasks.find(Ref(task)) != m_BackgroundTasks.end());
            _ASSERT(m_BackgroundItemTasks == 0);
        }}
        {{
            // process reply, first 'fast' call
            _ASSERT(m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplyFast()");
            auto result = m_Processor->ProcessReplyFast(m_ReplyStatus, m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplyFast(): "<<result);
            if ( result == CPSGL_Processor::eProcessed ) {
                MarkAsCompleted();
                return CThreadPool_Task::eCompleted;
            }
            else if ( result != CPSGL_Processor::eToNextStage ) {
                ERR_POST("CPSGDataLoader: failed processing reply: "<<result);
                MarkAsFailed();
            }
        }}
        {{
            // process reply, regular call
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReply()");
            auto result = m_Processor->ProcessReplySlow(m_ReplyStatus, m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplySlow(): "<<result);
            if ( result == CPSGL_Processor::eProcessed ) {
                MarkAsCompleted();
            }
            else if ( result != CPSGL_Processor::eToNextStage ) {
                ERR_POST("CPSGDataLoader: failed processing reply: "<<result);
                MarkAsFailed();
            }
            else {
                MarkAsNeedsFinalization();
            }
            return CThreadPool_Task::eCompleted;
        }}
    }
    catch ( exception& exc ) {
        ERR_POST("CPSGDataLoader: exception while processing reply item: "<<exc.what());
        MarkAsFailed();
        return CThreadPool_Task::eFailed;
    }
}


CThreadPool_Task::EStatus
CPSGL_RequestTracker::BackgroundProcessReplyCallback(CBackgroundTask* task)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessReplyCallback()");
    try {
        {{
            CFastMutexGuard guard(m_TrackerMutex);
            if ( CBackgroundTask::s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                _ASSERT(m_Status == CThreadPool_Task::eCanceled ||
                        m_Status == CThreadPool_Task::eFailed);
                return m_Status;
            }
            _ASSERT(m_BackgroundTasks.find(Ref(task)) != m_BackgroundTasks.end());
        }}
        _ASSERT(m_BackgroundItemTasks == 0);
        _ASSERT(m_Reply);
        auto result = m_Processor->ProcessReplySlow(m_ReplyStatus, m_Reply);
        if ( result == CPSGL_Processor::eProcessed ) {
            MarkAsCompleted();
            return CThreadPool_Task::eCompleted;
        }
        else if ( result != CPSGL_Processor::eToNextStage ) {
            ERR_POST("CPSGDataLoader: failed processing reply: "<<result);
            MarkAsFailed();
            return CThreadPool_Task::eFailed;
        }
        else {
            MarkAsNeedsFinalization();
            return CThreadPool_Task::eCompleted;
        }
    }
    catch ( exception& exc ) {
        ERR_POST("CPSGDataLoader: exception while processing reply: "<<exc.what());
        MarkAsFailed();
        return CThreadPool_Task::eFailed;
    }
}


CPSGL_ResultGuard CPSGL_RequestTracker::FinalizeResult()
{
    WaitForBackgroundTasks();
    if ( m_NeedsFinalization ) {
        _ASSERT(GetStatus() == CThreadPool_Task::eCompleted);
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::FinalizeResult(): calling");
        try {
            auto result = m_Processor->ProcessReplyFinal();
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::FinalizeResult(): finalized: "<<result);
            if ( result != CPSGL_Processor::eProcessed ) {
                m_Status = CThreadPool_Task::eFailed;
            }
        }
        catch ( exception& exc ) {
            ERR_POST("CPSGDataLoader: exception while processing reply: "<<exc.what());
            m_Status = CThreadPool_Task::eFailed;
        }
    }
    CPSGL_ResultGuard result;
    result.m_Processor = std::move(m_Processor);
    result.m_Index = m_Index;
    result.m_Status = m_Status;
    result.m_ReplyStatus = m_ReplyStatus;
    Reset();
    return result;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_QueueGuard
/////////////////////////////////////////////////////////////////////////////

CPSGL_QueueGuard::CPSGL_QueueGuard(CThreadPool& thread_pool,
                                   CPSGL_Queue& queue)
    : m_ThreadPool(thread_pool),
      m_Queue(&queue),
      m_CompleteSemaphore(0, kMax_UInt)
{
}


CPSGL_QueueGuard::~CPSGL_QueueGuard()
{
    CancelAll();
}


void CPSGL_QueueGuard::CancelAll()
{
    while ( auto tracker = GetQueuedRequest() ) {
        tracker->Cancel();
        _ASSERT(GetQueuedRequest() != tracker);
    }
}


CRef<CPSGL_RequestTracker> CPSGL_QueueGuard::GetQueuedRequest()
{
    CFastMutexGuard guard(m_CompleteMutex);
    return m_QueuedRequests.empty()? null: *m_QueuedRequests.begin();
}


#ifdef _DEBUG
static const char* s_GetRequestTypeName(CPSG_Request::EType type)
{
    switch (type) {
    case CPSG_Request::eBiodata:        return "biodata";
    case CPSG_Request::eResolve:        return "resolve";
    case CPSG_Request::eBlob:           return "blob";
    case CPSG_Request::eNamedAnnotInfo: return "annot";
    case CPSG_Request::eChunk:          return "chunk";
    case CPSG_Request::eIpgResolve:     return "ipg_resolve";
    }

    // Should not happen
    _TROUBLE;
    return "unknown";
}
#endif


void CPSGL_QueueGuard::AddRequest(const shared_ptr<CPSG_Request>& request,
                                  const CRef<CPSGL_Processor>& processor,
                                  size_t index)
{
    CRef<CPSGL_RequestTracker> tracker(new CPSGL_RequestTracker(*this, request, processor, index));
    _TRACE("CPSGL_QueueGuard::AddRequest(): CPSGL_RequestTracker("<<tracker<<", "<<tracker->m_Processor<<") for requst  "<<s_GetRequestTypeName(request->GetType())<<" "<<request->GetId());
    {{
        CFastMutexGuard guard(m_CompleteMutex);
        m_QueuedRequests.insert(tracker);
    }}
    m_Queue->RegisterRequest(tracker);
    if ( !m_Queue->SendRequest(tracker) ) {
        ERR_POST("CPSGDataLoader: cannot send request");
        tracker->MarkAsFailed();
    }
}


void CPSGL_QueueGuard::MarkAsFinished(const CRef<CPSGL_RequestTracker>& tracker)
{
    _TRACE("CPSGL_QueueGuard::MarkAsFinished(): tracker: "<<tracker);
    m_Queue->DeregisterRequest(tracker);
    {{
        CFastMutexGuard guard(m_CompleteMutex);
        if ( m_QueuedRequests.erase(tracker) ) {
            m_CompleteRequests.push_back(tracker);
        }
        m_CompleteSemaphore.Post();
    }}
}


CPSGL_ResultGuard CPSGL_QueueGuard::GetNextResult()
{
    CRef<CPSGL_RequestTracker> tracker;
    for ( ;; ) {
        {{
            CFastMutexGuard guard(m_CompleteMutex);
            if ( !m_CompleteRequests.empty() ) {
                tracker = m_CompleteRequests.front();
                m_CompleteRequests.pop_front();
                break;
            }
            if ( m_QueuedRequests.empty() ) {
                break;
            }
        }}
        m_CompleteSemaphore.Wait();
    }
    if ( tracker ) {
        _TRACE("CPSGL_QueueGuard::GetNextResult(): tracker: "<<tracker);
        return tracker->FinalizeResult();
    }
    return CPSGL_ResultGuard();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_ResultGuard
/////////////////////////////////////////////////////////////////////////////

CPSGL_ResultGuard::CPSGL_ResultGuard()
    : m_Index(0),
      m_Status(CThreadPool_Task::eIdle),
      m_ReplyStatus(EPSG_Status::eError)
{
}


CPSGL_ResultGuard::~CPSGL_ResultGuard()
{
}


CPSGL_ResultGuard::CPSGL_ResultGuard(CPSGL_ResultGuard&&) = default;
CPSGL_ResultGuard& CPSGL_ResultGuard::operator=(CPSGL_ResultGuard&&) = default;


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
