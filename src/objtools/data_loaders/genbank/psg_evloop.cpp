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
#include <cstdlib>
#include <format>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


//#define COLLECT_PROFILE

#ifdef COLLECT_PROFILE
struct SProfiler
{
    atomic<const char*> name = { 0 };
    atomic<size_t> count = { 0 };
    atomic<double> time = { 0 };
    ~SProfiler() {
        if ( name ) {
            cerr << format("{} calls: {} time: {}\n", name.load(), count.load(), time.load());
        }
    }
};
struct SProfilerGuard
{
    SProfiler& p;
    CStopWatch sw;
    SProfilerGuard(SProfiler& p, const char* name)
        : p(p),
          sw(CStopWatch::eStart)
        {
            if ( !p.name ) {
                p.name = name;
            }
        }
    ~SProfilerGuard()
        {
            p.count += 1;
            p.time += sw.Elapsed();
        }
};

static SProfiler sp_AddRequest;
static SProfiler sp_SendRequest;
static SProfiler sp_ProcessItem;
static SProfiler sp_ProcessItemCallback;
static SProfiler sp_ProcessItemCallback1;
static SProfiler sp_ProcessItemCallback2;
static SProfiler sp_ProcessItemCallback3;
static SProfiler sp_ProcessItemCallbackF;
static SProfiler sp_ProcessItemCallbackE;
static SProfiler sp_ProcessReply;
static SProfiler sp_GetTracker;

# define PROFILE(var) SProfilerGuard profile_guard##var(var, #var)
#else
# define PROFILE(var)
#endif

static const int kDestructionDelay = 0; // max delay in milliseconds
static const int kFailureRate = 0; // zero or probability of failure = 1/kFailureRate
static bool kUseBackgroundTasks = false;

static inline
void s_SimulateDelay()
{
    if ( kDestructionDelay ) {
        SleepMilliSec(rand()%(kDestructionDelay?kDestructionDelay+1:1));
    }
}


static inline
void s_SimulateFailure(CPSGL_Processor* processor, const char* message)
{
    if ( kFailureRate ) {
        if ( rand() % (kFailureRate?kFailureRate:1) == 0 ) {
            string full_message = string("simulated exception: ")+message;
            if ( processor ) {
                processor->AddError(full_message);
            }
            NCBI_THROW(CLoaderException, eLoaderFailed, full_message);
        }
    }
}


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
// CPSGL_TrackerMap
/////////////////////////////////////////////////////////////////////////////


CRef<CPSGL_RequestTracker> CPSGL_TrackerMap::GetTracker(const shared_ptr<CPSG_Reply>& reply)
{
    PROFILE(sp_GetTracker);
    CFastMutexGuard guard(m_TrackerMapMutex);
    auto iter = m_TrackerMap.find(reply->GetRequest().get());
    if ( iter == m_TrackerMap.end() ) {
        return null;
    }
    return Ref(iter->second);
}


inline
CRef<CPSGL_RequestTracker> CPSGL_TrackerMap::GetTracker(const shared_ptr<CPSG_ReplyItem>& item)
{
    return GetTracker(item->GetReply());
}


void CPSGL_TrackerMap::RegisterRequest(CPSGL_RequestTracker* tracker)
{
    CFastMutexGuard guard(m_TrackerMapMutex);
    _VERIFY(m_TrackerMap.insert(make_pair(tracker->GetRequest().get(), tracker)).second);
}


void CPSGL_TrackerMap::DeregisterRequest(const CPSGL_RequestTracker* tracker)
{
    CFastMutexGuard guard(m_TrackerMapMutex);
    m_TrackerMap.erase(tracker->GetRequest().get());
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_RequestQueue
/////////////////////////////////////////////////////////////////////////////


void CPSGL_RequestQueue::Put(value_type v)
{
    {{
        lock_guard lock(m_Mutex);
        m_Queue.push(std::move(v));
    }}
    m_CV.notify_one();
}


CPSGL_RequestQueue::value_type CPSGL_RequestQueue::Get()
{
    unique_lock lock(m_Mutex);
    m_CV.wait(lock, [&] { return m_Stopped || !m_Queue.empty(); });
    if ( m_Queue.empty() ) {
        // stopped
        return null;
    }
    auto v = std::move(m_Queue.front());
    m_Queue.pop();
    return v;
}


void CPSGL_RequestQueue::Stop()
{
    {{
        lock_guard lock(m_Mutex);
        m_Stopped = true;
    }}
    m_CV.notify_all();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Queue
/////////////////////////////////////////////////////////////////////////////


CPSGL_Queue::CPSGL_Queue(const string& service_name,
                         CRef<CPSGL_TrackerMap> tracker_map,
                         CRef<CPSGL_RequestQueue> request_queue)
    : m_EventLoop(service_name,
                  bind(&CPSGL_Queue::ProcessItemCallback, this, placeholders::_1, placeholders::_2),
                  bind(&CPSGL_Queue::ProcessReplyCallback, this, placeholders::_1, placeholders::_2)),
      m_TrackerMap(tracker_map),
      m_RequestQueue(request_queue),
      m_EventLoopThread(&CPSG_EventLoop::Run, ref(m_EventLoop), CDeadline::eInfinite),
      m_SendThread(&CPSGL_Queue::SenderRun, ref(*this))
{
}


CPSGL_Queue::~CPSGL_Queue()
{
    m_EventLoop.Reset();
    m_SendThread.join();
    m_EventLoopThread.join();
}


void CPSGL_Queue::SetRequestContext(const CRef<CRequestContext>& context)
{
    m_RequestContext = context;
}


void CPSGL_Queue::SenderRun()
{
    while ( auto request = m_RequestQueue->Get() ) {
        SendRequest(std::move(request));
    }
}


void CPSGL_Queue::SendRequest(CRef<CPSGL_RequestTracker> tracker)
{
    PROFILE(sp_SendRequest);
    shared_ptr<CPSG_Request> request = tracker->GetRequest();
    if ( m_RequestContext ) {
        request->SetRequestContext(m_RequestContext);
    }
    if ( !m_EventLoop.SendRequest(request, CDeadline::eInfinite) ) {
        ERR_POST("CPSGDataLoader: cannot send request");
        tracker.GetNCPointer()->MarkAsFailed();
    }
}


void CPSGL_Queue::ProcessItemCallback(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item)
{
    PROFILE(sp_ProcessItem);
    if ( auto tracker = m_TrackerMap->GetTracker(item) ) {
        tracker->ProcessItemCallback(status, item);
    }
}    


void CPSGL_Queue::ProcessReplyCallback(EPSG_Status status,
                                       const shared_ptr<CPSG_Reply>& reply)
{
    PROFILE(sp_ProcessReply);
    if ( auto tracker = m_TrackerMap->GetTracker(reply) ) {
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
      m_TrackerMutex(new CObjectFor<CFastMutex>()),
      m_InCallbackSemaphore(0, kMax_UInt),
      m_BackgroundItemTaskCount(0),
      m_InCallbackCount(0)
{
}


CPSGL_RequestTracker::~CPSGL_RequestTracker()
{
    CancelBackgroundTasks();
}


static inline bool s_IsFinished(CThreadPool_Task::EStatus status)
{
    return status >= CThreadPool_Task::eCompleted;
}


static inline bool s_IsAborted(CThreadPool_Task::EStatus status)
{
    return status > CThreadPool_Task::eCompleted;
}


class CPSGL_RequestTracker::CCallbackGuard
{
public:
    CCallbackGuard()
        : m_Tracker(nullptr)
    {
    }
    
    explicit
    CCallbackGuard(CPSGL_RequestTracker* tracker)
        : m_Tracker(nullptr)
    {
        Set(tracker);
    }

    ~CCallbackGuard()
    {
        Reset();
    }

    operator bool() const {
        return m_Tracker;
    }
    
    CCallbackGuard(const CCallbackGuard&) = delete;
    CCallbackGuard& operator=(const CCallbackGuard&) = delete;
    
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    
    void Reset()
    {
        if ( auto tracker = m_Tracker ) {
            CFastMutexGuard guard(tracker->GetTrackerMutex());
            if ( --tracker->m_InCallbackCount == 0 ) {
                tracker->m_InCallbackSemaphore.Post();
            }
            m_Tracker = 0; 
        }
    }
    
    void Set(CPSGL_RequestTracker* tracker)
    {
        if ( tracker ) {
            CFastMutexGuard guard(tracker->GetTrackerMutex());
            SetNoLock(tracker);
        }
    }

    void SetNoLock(CPSGL_RequestTracker* tracker)
    {
        if ( !s_IsFinished(tracker->m_Status) ) {
            ++tracker->m_InCallbackCount;
            m_Tracker = tracker;
        }
    }

private:
    CPSGL_RequestTracker* m_Tracker;
};


class CPSGL_RequestTracker::CBackgroundTask : public CThreadPool_Task
{
public:
    CBackgroundTask(CPSGL_RequestTracker* tracker,
                    EPSG_Status item_status,
                    const shared_ptr<CPSG_ReplyItem>& item)
        : m_Tracker(tracker),
          m_TrackerMutex(tracker->m_TrackerMutex),
          m_ItemStatus(item_status),
          m_Item(item)
    {
    }
    explicit
    CBackgroundTask(CPSGL_RequestTracker* tracker) // for reply processing
        : m_Tracker(tracker),
          m_TrackerMutex(tracker->m_TrackerMutex),
          m_ItemStatus(EPSG_Status::eError)
    {
    }
    ~CBackgroundTask()
    {
        s_SimulateDelay();
    }

    CFastMutex& GetTrackerMutex()
    {
        return *m_TrackerMutex;
    }

    EStatus Execute() override
    {
        CCallbackGuard callback_guard;
        CRef<CPSGL_RequestTracker> tracker;
        {{
            CFastMutexGuard guard(GetTrackerMutex());
            tracker = m_Tracker;
            if ( tracker ) {
                callback_guard.SetNoLock(tracker);
            }
        }}
        if ( !callback_guard ) {
            return eCanceled;
        }
        if ( m_Item ) {
            return tracker->BackgroundProcessItemCallback(this, m_ItemStatus, m_Item);
        }
        else {
            return tracker->BackgroundProcessReplyCallback(this);
        }
    }

    void Cancel()
    {
        RequestToCancel();
        DisconnectFromTracker();
    }
    
    void OnStatusChange(EStatus old_task_status) override
    {
        if ( s_IsFinished(GetStatus()) ) {
            DisconnectFromTracker();
            s_SimulateDelay();
        }
    }

    void DisconnectFromTracker()
    {
        CRef<CPSGL_RequestTracker> tracker;
        {{
            CFastMutexGuard guard(GetTrackerMutex());
            if ( m_Tracker ) {
                m_Tracker->m_BackgroundTasks.erase(Ref(this));
                swap(tracker, m_Tracker); // destruct out of guard
            }
        }}
    }
    
    CRef<CPSGL_RequestTracker> m_Tracker;
    CRef<CObjectFor<CFastMutex>> m_TrackerMutex;
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
    CRef<CBackgroundTask> task;
    for ( ;; ) {
        {{
            CFastMutexGuard guard(GetTrackerMutex());
            if ( m_BackgroundTasks.empty() ) {
                break;
            }
            _ASSERT(*m_BackgroundTasks.begin() != task);
            task = *m_BackgroundTasks.begin();
        }}
        task->Cancel();
    }
    WaitForBackgroundTasks();
}


void CPSGL_RequestTracker::WaitForBackgroundTasks()
{
    for ( ;; ) {
        {{
            CFastMutexGuard guard(GetTrackerMutex());
            if ( m_InCallbackCount == 0 ) {
                return;
            }
        }}
        m_InCallbackSemaphore.Wait();
    }
}


inline
void CPSGL_RequestTracker::QueueInBackground(const CRef<CBackgroundTask>& task)
{
    {{
        CFastMutexGuard guard(GetTrackerMutex());
        m_BackgroundTasks.insert(task);
        if ( task->m_Item ) {
            ++m_BackgroundItemTaskCount;
        }
    }}
    m_QueueGuard.GetThreadPool().AddTask(task.GetNCPointer());
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
    _ASSERT(m_BackgroundItemTaskCount == 0);
    _ASSERT(m_Reply);
    QueueInBackground(Ref(new CBackgroundTask(this)));
}


void CPSGL_RequestTracker::MarkAsFinished(CThreadPool_Task::EStatus status)
{
    _ASSERT(s_IsFinished(status));
    {{
        CFastMutexGuard guard(GetTrackerMutex());
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
    PROFILE(sp_ProcessItemCallback);
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessItemCallback()");
    CCallbackGuard guard(this);
    if ( !guard ) {
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessItemCallback() - canceled");
        return;
    }
    PROFILE(sp_ProcessItemCallback1);
    try {
        PROFILE(sp_ProcessItemCallback2);
        s_SimulateFailure(m_Processor, "ProcessItemCallback item fast");
        _ASSERT(item);
        auto result = m_Processor->ProcessItemFast(status, item);
        if ( result == CPSGL_Processor::eToNextStage ) {
            PROFILE(sp_ProcessItemCallback3);
            if ( kUseBackgroundTasks ) {
                // queue background processing
                StartProcessItemInBackground(status, item);
                return;
            }
            result = m_Processor->ProcessItemSlow(status, item);
            _ASSERT(result != CPSGL_Processor::eToNextStage);
        }
        if ( result != CPSGL_Processor::eProcessed ) {
            PROFILE(sp_ProcessItemCallbackE);
            _TRACE("CPSGDataLoader: failed processing reply item: "<<result);
            MarkAsFailed();
        }
    }
    catch ( exception& exc ) {
        PROFILE(sp_ProcessItemCallbackF);
        _TRACE("CPSGDataLoader: exception while processing reply item: "<<exc.what());
        m_Processor->AddError(exc.what());
        MarkAsFailed();
    }
}


void CPSGL_RequestTracker::ProcessReplyCallback(EPSG_Status status,
                                                const shared_ptr<CPSG_Reply>& reply)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback()");
    CCallbackGuard guard(this);
    if ( !guard ) {
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback() - canceled");
        return;
    }
    try {
        _ASSERT(reply);
        {{
            // items may be still being processed in background
            // and we cannot yet process reply in this case
            CFastMutexGuard guard(GetTrackerMutex());
            _ASSERT(!m_Reply);
            m_ReplyStatus = status;
            m_Reply = reply;
            if ( m_BackgroundItemTaskCount > 0 ) {
                // not all items processed
                // the reply will be processed by the last item task
                return;
            }
        }}
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback(): ProcessReplyFast()");
        s_SimulateFailure(m_Processor, "ProcessReplyCallback reply fast");
        auto result = m_Processor->ProcessReplyFast(status, reply);
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::ProcessReplyCallback(): ProcessReplyFast(): "<<result);
        if ( result == CPSGL_Processor::eToNextStage ) {
            if ( kUseBackgroundTasks ) {
                // queue processing
                StartProcessReplyInBackground();
                return;
            }
            result = m_Processor->ProcessReplySlow(status, reply);
            if ( result == CPSGL_Processor::eToNextStage ) {
                MarkAsNeedsFinalization();
                return;
            }
        }
        if ( result != CPSGL_Processor::eProcessed ) {
            _TRACE("CPSGDataLoader: failed processing reply: "<<result);
            MarkAsFailed();
        }
        else {
            MarkAsCompleted();
        }
    }
    catch ( exception& exc ) {
        _TRACE("CPSGDataLoader: exception while processing reply: "<<exc.what());
        m_Processor->AddError(exc.what());
        MarkAsFailed();
    }
}


CThreadPool_Task::EStatus
CPSGL_RequestTracker::BackgroundProcessItemCallback(CBackgroundTask* task,
                                                    EPSG_Status status,
                                                    const shared_ptr<CPSG_ReplyItem>& item)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback()");
    CCallbackGuard guard(this);
    if ( !guard ) {
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback() - finished");
        return m_Status;
    }
    try {
        {{
            // check if canceled
            CFastMutexGuard guard(GetTrackerMutex());
            if ( s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                return m_Status;
            }
        }}
        {{
            // process item
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessItemSlow()");
            s_SimulateFailure(m_Processor, "BackgroundProcessItemCallback item slow");
            auto result = m_Processor->ProcessItemSlow(status, item);
            if ( result != CPSGL_Processor::eProcessed ) {
                _TRACE("CPSGDataLoader: failed processing reply item: "<<result);
                MarkAsFailed();
                return CThreadPool_Task::eFailed;
            }
        }}
        {{
            // check if result needs to be processed too
            CFastMutexGuard guard(GetTrackerMutex());
            if ( s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                return m_Status;
            }
            if ( --m_BackgroundItemTaskCount > 0 || !m_Reply ) {
                // either there are other background item tasks
                // or reply wasn't received yet
                return CThreadPool_Task::eCompleted;
            }
            _ASSERT(m_BackgroundTasks.find(Ref(task)) != m_BackgroundTasks.end());
            _ASSERT(m_BackgroundItemTaskCount == 0);
        }}
        {{
            // process reply, first 'fast' call
            _ASSERT(m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplyFast()");
            s_SimulateFailure(m_Processor, "BackgroundProcessItemCallback reply fast");
            auto result = m_Processor->ProcessReplyFast(m_ReplyStatus, m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplyFast(): "<<result);
            if ( result == CPSGL_Processor::eProcessed ) {
                MarkAsCompleted();
                return CThreadPool_Task::eCompleted;
            }
            else if ( result != CPSGL_Processor::eToNextStage ) {
                _TRACE("CPSGDataLoader: failed processing reply: "<<result);
                MarkAsFailed();
                return CThreadPool_Task::eFailed;
            }
        }}
        {{
            // process reply, regular call
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReply()");
            s_SimulateFailure(m_Processor, "BackgroundProcessItemCallback reply slow");
            auto result = m_Processor->ProcessReplySlow(m_ReplyStatus, m_Reply);
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessItemCallback(): ProcessReplySlow(): "<<result);
            if ( result == CPSGL_Processor::eProcessed ) {
                MarkAsCompleted();
                return CThreadPool_Task::eCompleted;
            }
            else if ( result != CPSGL_Processor::eToNextStage ) {
                _TRACE("CPSGDataLoader: failed processing reply: "<<result);
                MarkAsFailed();
                return CThreadPool_Task::eFailed;
            }
            else {
                MarkAsNeedsFinalization();
                return CThreadPool_Task::eCompleted;
            }
        }}
    }
    catch ( exception& exc ) {
        _TRACE("CPSGDataLoader: exception while processing reply item: "<<exc.what());
        m_Processor->AddError(exc.what());
        MarkAsFailed();
        return CThreadPool_Task::eFailed;
    }
}


CThreadPool_Task::EStatus
CPSGL_RequestTracker::BackgroundProcessReplyCallback(CBackgroundTask* task)
{
    _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessReplyCallback()");
    CCallbackGuard guard(this);
    if ( !guard ) {
        _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::BackgroundProcessReplyCallback() - finished");
        return m_Status;
    }
    try {
        {{
            CFastMutexGuard guard(GetTrackerMutex());
            if ( s_IsAborted(m_Status) ) {
                // failed or canceled, no need to process
                return m_Status;
            }
            _ASSERT(m_BackgroundTasks.find(Ref(task)) != m_BackgroundTasks.end());
        }}
        _ASSERT(m_BackgroundItemTaskCount == 0);
        _ASSERT(m_Reply);
        s_SimulateFailure(m_Processor, "BackgroundProcessReplyCallback reply slow");
        auto result = m_Processor->ProcessReplySlow(m_ReplyStatus, m_Reply);
        if ( result == CPSGL_Processor::eProcessed ) {
            MarkAsCompleted();
            return CThreadPool_Task::eCompleted;
        }
        else if ( result != CPSGL_Processor::eToNextStage ) {
            _TRACE("CPSGDataLoader: failed processing reply: "<<result);
            MarkAsFailed();
            return CThreadPool_Task::eFailed;
        }
        else {
            MarkAsNeedsFinalization();
            return CThreadPool_Task::eCompleted;
        }
    }
    catch ( exception& exc ) {
        _TRACE("CPSGDataLoader: exception while processing reply: "<<exc.what());
        m_Processor->AddError(exc.what());
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
            s_SimulateFailure(m_Processor, "FinalizeResult final");
            auto result = m_Processor->ProcessReplyFinal();
            _TRACE("CPSGL_RequestTracker("<<this<<", "<<m_Processor<<")::FinalizeResult(): finalized: "<<result);
            if ( result != CPSGL_Processor::eProcessed ) {
                m_Status = CThreadPool_Task::eFailed;
            }
        }
        catch ( exception& exc ) {
            _TRACE("CPSGDataLoader: exception while processing reply: "<<exc.what());
            m_Processor->AddError(exc.what());
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
// CPSGL_Dispatcher
/////////////////////////////////////////////////////////////////////////////

CPSGL_Dispatcher::CPSGL_Dispatcher(const string& service_name,
                                   unsigned max_pool_threads,
                                   unsigned io_event_loops)
    : m_TrackerMap(new CPSGL_TrackerMap()),
      m_RequestQueue(new CPSGL_RequestQueue())
{
    if ( kUseBackgroundTasks ) {
        static const unsigned kMinPoolThreads = 1;
        static const unsigned kMaxPoolThreads = 32;
        max_pool_threads = max(max_pool_threads, kMinPoolThreads);
        max_pool_threads = min(max_pool_threads, kMaxPoolThreads);
        m_ThreadPool = make_unique<CThreadPool>(kMax_UInt, max_pool_threads);
    }
    
    static const unsigned kMinIoEventLoops = 1;
    static const unsigned kMaxIoEventLoops = 32;
    io_event_loops = max(io_event_loops, kMinIoEventLoops);
    io_event_loops = min(io_event_loops, kMaxIoEventLoops);
    for ( unsigned i = 0; i < io_event_loops; ++i ) {
        m_QueueSet.push_back(Ref(new CPSGL_Queue(service_name, m_TrackerMap, m_RequestQueue)));
    }
}


void CPSGL_Dispatcher::SetRequestContext(const CRef<CRequestContext>& context)
{
    for ( auto& q : m_QueueSet ) {
        q->SetRequestContext(context);
    }
}


void CPSGL_Dispatcher::SetRequestFlags(CPSG_Request::TFlags flags)
{
    for ( auto& q : m_QueueSet ) {
        q->GetPSG_Queue().SetRequestFlags(flags);
    }
}


void CPSGL_Dispatcher::SetUserArgs(const SPSG_UserArgs& user_args)
{
    for ( auto& q : m_QueueSet ) {
        q->GetPSG_Queue().SetUserArgs(user_args);
    }
}


void CPSGL_Dispatcher::Stop()
{
    // Make sure thread pool is destroyed before any tasks (e.g. CDD prefetch)
    // and stops them all before the loader is destroyed.
    m_ThreadPool.reset();
    m_RequestQueue->Stop();
}


CPSGL_Dispatcher::~CPSGL_Dispatcher()
{
}


void CPSGL_Dispatcher::SendRequest(CRef<CPSGL_RequestTracker> tracker)
{
    m_TrackerMap->RegisterRequest(tracker);
    m_RequestQueue->Put(std::move(tracker));
}


void CPSGL_Dispatcher::ForgetRequest(const CRef<CPSGL_RequestTracker>& tracker)
{
    m_TrackerMap->DeregisterRequest(tracker);
}


/////////////////////////////////////////////////////////////////////////////
// CPSGL_QueueGuard
/////////////////////////////////////////////////////////////////////////////

CPSGL_QueueGuard::CPSGL_QueueGuard(CRef<CPSGL_Dispatcher>& dispatcher)
    : m_Dispatcher(dispatcher),
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
    case CPSG_Request::eAccVerHistory:  return "acc_ver_history";
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
    PROFILE(sp_AddRequest);
    CRef<CPSGL_RequestTracker> tracker(new CPSGL_RequestTracker(*this, request, processor, index));
    _TRACE("CPSGL_QueueGuard::AddRequest(): CPSGL_RequestTracker("<<tracker<<", "<<tracker->m_Processor<<") for requst  "<<s_GetRequestTypeName(request->GetType())<<" "<<request->GetId());
    {{
        CFastMutexGuard guard(m_CompleteMutex);
        m_QueuedRequests.insert(tracker);
    }}
    m_Dispatcher->SendRequest(std::move(tracker));
}


void CPSGL_QueueGuard::MarkAsFinished(const CRef<CPSGL_RequestTracker>& tracker)
{
    _TRACE("CPSGL_QueueGuard::MarkAsFinished(): tracker: "<<tracker);
    m_Dispatcher->ForgetRequest(tracker);
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
