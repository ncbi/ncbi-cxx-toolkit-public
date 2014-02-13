#ifndef UTIL__THREAD_POOL_OLD__HPP
#define UTIL__THREAD_POOL_OLD__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Pools of generic request-handling threads.
*
*   TEMPLATES:
*      CBlockingQueue<>  -- queue of requests, with efficiently blocking Get()
*      CThreadInPool<>   -- abstract request-handling thread
*      CPoolOfThreads<>  -- abstract pool of threads sharing a request queue
*
*   SPECIALIZATIONS:
*      CStdRequest       -- abstract request type
*      CStdThreadInPool  -- thread handling CStdRequest
*      CStdPoolOfThreads -- pool of threads handling CStdRequest
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_param.hpp>
#include <util/util_exception.hpp>
#include <util/error_codes.hpp>

#include <set>


/** @addtogroup ThreadedPools
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
///     CQueueItemBase -- skeleton blocking-queue item, sans actual request

class CQueueItemBase : public CObject {
public:
    enum EStatus {
        ePending,       ///< still in the queue
        eActive,        ///< extracted but not yet released
        eComplete,      ///< extracted and released
        eWithdrawn,     ///< dropped by submitter's request
        eForciblyCaught ///< let an exception escape
    };

    /// Every request has an associated 32-bit priority field, but
    /// only the top eight bits are under direct user control.  (The
    /// rest are a counter.)
    typedef Uint4 TPriority;
    typedef Uint1 TUserPriority;

    CQueueItemBase(TPriority priority) 
        : m_Priority(priority), m_Status(ePending)
        { }
    
    bool operator> (const CQueueItemBase& item) const 
        { return m_Priority > item.m_Priority; }

    const TPriority& GetPriority(void) const     { return m_Priority; }
    const EStatus&   GetStatus(void) const       { return m_Status; }
    TUserPriority    GetUserPriority(void) const { return m_Priority >> 24; }

    void MarkAsComplete(void)        { x_SetStatus(eComplete); }
    void MarkAsForciblyCaught(void)  { x_SetStatus(eForciblyCaught); }

protected:
    TPriority m_Priority;
    EStatus   m_Status;

    virtual void x_SetStatus(EStatus new_status)
        { m_Status = new_status; }
};


/////////////////////////////////////////////////////////////////////////////
///
///     CBlockingQueue<>  -- queue of requests, with efficiently blocking Get()

template <typename TRequest>
class CBlockingQueue
{
public:
    typedef CQueueItemBase::TPriority     TPriority;
    typedef CQueueItemBase::TUserPriority TUserPriority;

    class CQueueItem;
    typedef CRef<CQueueItem> TItemHandle;

    /// It may be desirable to store handles obtained from GetHandle() in
    /// instances of CCompletingHandle to ensure that they are marked as
    /// complete when all is said and done, even in the face of exceptions.
    class CCompletingHandle : public TItemHandle
    {
    public:
        CCompletingHandle(const TItemHandle& h)
            : TItemHandle(h)
            { }

        ~CCompletingHandle() {
            if (this->NotEmpty()) {
                this->GetObject().MarkAsComplete();
            }
        }
    };

    /// Constructor
    ///
    /// @param max_size
    ///   The maximum size of the queue (may not be zero!)
    CBlockingQueue(size_t max_size = kMax_UInt)
        : m_GetSem(0,1), m_PutSem(1,1), m_HungerSem(0,1), m_HungerCnt(0),
          m_MaxSize(min(max_size, size_t(0xFFFFFF))),
          m_RequestCounter(0xFFFFFF)
        { _ASSERT(max_size > 0); }

    /// Put a request into the queue.  If the queue remains full for
    /// the duration of the (optional) timeout, throw an exception.
    ///
    /// @param request
    ///   Request
    /// @param priority
    ///   The priority of the request. The higher the priority 
    ///   the sooner the request will be processed.
    /// @param timeout_sec
    ///   Number of whole seconds in timeout
    /// @param timeout_nsec
    ///   Number of additional nanoseconds in timeout
    TItemHandle  Put(const TRequest& request, TUserPriority priority = 0,
                     unsigned int timeout_sec = 0,
                     unsigned int timeout_nsec = 0);

    /// Wait for room in the queue for up to
    /// timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nanoseconds
    void         WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                             unsigned int timeout_nsec = 0) const;

    /// Wait for the queue to have waiting readers, for up to
    /// timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nanoseconds
    void         WaitForHunger(unsigned int timeout_sec  = kMax_UInt,
                               unsigned int timeout_nsec = 0) const;

    /// Get the first available request from the queue, and return a
    /// handle to it.
    /// Blocks politely if empty.
    /// Waits up to timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nanoseconds
    TItemHandle  GetHandle(unsigned int timeout_sec  = kMax_UInt,
                           unsigned int timeout_nsec = 0);

    /// Get the first available request from the queue, and return
    /// just the request.
    /// Blocks politely if empty.
    /// Waits up to timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nanoseconds
    NCBI_DEPRECATED
    TRequest     Get(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0);

    /// Get the number of requests in the queue
    size_t       GetSize    (void) const;

    /// Get the maximun number of requests that can be put into the queue
    size_t       GetMaxSize (void) const { return m_MaxSize; }

    /// Check if the queue is empty
    bool         IsEmpty    (void) const { return GetSize() == 0; }

    /// Check if the queue is full
    bool         IsFull     (void) const { return GetSize() == GetMaxSize(); }

    /// Adjust a pending request's priority.
    void         SetUserPriority(TItemHandle handle, TUserPriority priority);

    /// Withdraw a pending request from consideration.
    void         Withdraw(TItemHandle handle);

    /// Get the number of threads waiting for requests, for debugging
    /// purposes only.
    size_t       GetHunger(void) const { return m_HungerCnt; }

    class CQueueItem : public CQueueItemBase
    {
    public:
        // typedef CBlockingQueue<TRequest> TQueue;
        CQueueItem(Uint4 priority, TRequest request)
            : CQueueItemBase(priority), m_Request(request)
            { }

        const TRequest& GetRequest(void) const { return m_Request; } 
        TRequest&       SetRequest(void)       { return m_Request; }
        // void SetUserPriority(TUserPriority p);
        // void Withdraw(void);

    protected:
        // Specialized for CRef<CStdRequest> in thread_pool.cpp
        void x_SetStatus(EStatus new_status)
            { CQueueItemBase::x_SetStatus(new_status); }
        
    private:
        friend class CBlockingQueue<TRequest>;

        // TQueue&   m_Queue;
        TRequest  m_Request;
    };
    
protected:
    struct SItemHandleGreater {
        bool operator()(const TItemHandle& i1, const TItemHandle& i2) const
            { return static_cast<CQueueItemBase>(*i1)
                    > static_cast<CQueueItemBase>(*i2); }
    };
    
    /// The type of the queue
    typedef set<TItemHandle, SItemHandleGreater> TRealQueue;

    // Derived classes should take care to use these members properly.
    volatile TRealQueue m_Queue;     ///< The queue
    CSemaphore          m_GetSem;    ///< Raised if the queue contains data
    mutable CSemaphore  m_PutSem;    ///< Raised if the queue has room
    mutable CSemaphore  m_HungerSem; ///< Raised if Get[Handle] has to wait
    mutable CMutex      m_Mutex;     ///< Guards access to queue
    size_t              m_HungerCnt; ///< Number of threads waiting for data

private:
    size_t              m_MaxSize;        ///< The maximum size of the queue
    Uint4               m_RequestCounter; ///

    typedef bool (CBlockingQueue::*TQueuePredicate)(const TRealQueue& q) const;

    bool x_GetSemPred(const TRealQueue& q) const
        { return !q.empty(); }
    bool x_PutSemPred(const TRealQueue& q) const
        { return q.size() < m_MaxSize; }
    bool x_HungerSemPred(const TRealQueue& q) const
        { return m_HungerCnt > q.size(); }

    bool x_WaitForPredicate(TQueuePredicate pred, CSemaphore& sem,
                            CMutexGuard& guard, unsigned int timeout_sec,
                            unsigned int timeout_nsec) const;

private:
    /// forbidden
    CBlockingQueue(const CBlockingQueue&);
    CBlockingQueue& operator=(const CBlockingQueue&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CThreadInPool<>   -- abstract request-handling thread

template <typename TRequest> class CPoolOfThreads;

template <typename TRequest>
class CThreadInPool : public CThread
{
public:
    typedef CPoolOfThreads<TRequest> TPool;
    typedef typename CBlockingQueue<TRequest>::TItemHandle TItemHandle;
    typedef typename CBlockingQueue<TRequest>::CCompletingHandle
        TCompletingHandle;

    /// Thread run mode 
    enum ERunMode {
        eNormal,   ///< Process request and stay in the pool
        eRunOnce   ///< Process request and die
    };

    /// Constructor
    ///
    /// @param pool
    ///   A pool where this thead is placed
    /// @param mode
    ///   A running mode of this thread
    CThreadInPool(TPool* pool, ERunMode mode = eNormal) 
        : m_Pool(pool), m_RunMode(mode), m_Counter(NULL) {}

    void CountSelf(CAtomicCounter* counter);

protected:
    /// Destructor
    virtual ~CThreadInPool(void);

    /// Intit this thread. It is called at beginning of Main()
    virtual void Init(void) {}

    /// Process a request.
    /// It is called from Main() for each request this thread handles
    ///
    /// @param
    ///   A request for processing
    virtual void ProcessRequest(TItemHandle handle);

    /// Older interface (still delegated to by default)
    virtual void ProcessRequest(const TRequest& req) = 0;

    /// Clean up. It is called by OnExit()
    virtual void x_OnExit(void) {}

    /// Get run mode
    ERunMode GetRunMode(void) const { return m_RunMode; }

private:
    // to prevent overriding; inherited from CThread
    virtual void* Main(void);
    virtual void OnExit(void);

    void x_HandleOneRequest(bool catch_all);
    void x_UnregisterThread(void);

    class CAutoUnregGuard
    {
    public:
        typedef CThreadInPool<TRequest> TThread;
        CAutoUnregGuard(TThread* thr);
        ~CAutoUnregGuard(void);

    private:
        TThread* m_Thread;
    };

    friend class CAutoUnregGuard;


    TPool*          m_Pool;     ///< The pool that holds this thread
    ERunMode        m_RunMode;  ///< How long to keep running
    CAtomicCounter* m_Counter;
};


/////////////////////////////////////////////////////////////////////////////
///
///     CPoolOfThreads<>  -- abstract pool of threads sharing a request queue

template <typename TRequest>
class CPoolOfThreads
{
public:
    typedef CThreadInPool<TRequest> TThread;
    typedef typename TThread::ERunMode ERunMode;

    typedef CBlockingQueue<TRequest> TQueue;
    typedef typename TQueue::TUserPriority TUserPriority;
    typedef typename TQueue::TItemHandle   TItemHandle;

    /// Constructor
    ///
    /// @param max_threads
    ///   The maximum number of threads that this pool can run
    /// @param queue_size
    ///   The maximum number of requests in the queue
    /// @param spawn_threashold
    ///   The number of requests in the queue after which 
    ///   a new thread is started
    /// @param max_urgent_threads
    ///   The maximum number of urgent threads running simultaneously
    CPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                   unsigned int spawn_threshold = 1, 
                   unsigned int max_urgent_threads = kMax_UInt);

    /// Destructor
    virtual ~CPoolOfThreads(void);

    /// Start processing threads
    ///
    /// @param num_threads
    ///    The number of threads to start
    void Spawn(unsigned int num_threads);

    /// Put a request in the queue with a given priority
    ///
    /// @param request
    ///   A request
    /// @param priority
    ///   The priority of the request. The higher the priority 
    ///   the sooner the request will be processed.   
    TItemHandle AcceptRequest(const TRequest& request,
                              TUserPriority priority = 0,
                              unsigned int timeout_sec = 0,
                              unsigned int timeout_nsec = 0);

    /// Puts a request in the queue with the highest priority
    /// It will run a new thread even if the maximum of allowed threads 
    /// has been already reached
    ///
    /// @param request
    ///   A request
    TItemHandle AcceptUrgentRequest(const TRequest& request,
                                    unsigned int timeout_sec = 0,
                                    unsigned int timeout_nsec = 0);

    /// Wait for the room in the queue up to
    /// timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nanoseconds
    void WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0);
  
    /// Check if the queue is full
    bool IsFull(void) const { return m_Queue.IsFull(); }

    /// Check if the queue is empty
    bool IsEmpty(void) const { return m_Queue.IsEmpty(); }

    /// Check whether a new request could be immediately processed
    ///
    /// @param urgent
    ///  Whether the request would be urgent.
    bool HasImmediateRoom(bool urgent = false) const;

    /// Adjust a pending request's priority.
    void         SetUserPriority(TItemHandle handle, TUserPriority priority);

    /// Withdraw a pending request from consideration.
    void         Withdraw(TItemHandle handle)
        { m_Queue.Withdraw(handle); }

    /// Get the number of requests in the queue
    size_t       GetQueueSize(void) const
        { return m_Queue.GetSize(); }


protected:

    /// Create a new thread
    ///
    /// @param mode
    ///   How long the thread should stay around
    virtual TThread* NewThread(ERunMode mode) = 0;

    /// Register a thread. It is called by TThread::Main.
    /// It should detach a thread if not tracking
    ///
    /// @param thread
    ///   A thread to register
    virtual void Register(TThread& thread) { thread.Detach(); }

    /// Unregister a thread
    ///
    /// @param thread
    ///   A thread to unregister
    virtual void UnRegister(TThread&) {}


    typedef CAtomicCounter::TValue TACValue;

    /// The maximum number of threads the pool can hold
    volatile TACValue        m_MaxThreads;
    /// The maximum number of urgent threads running simultaneously
    volatile TACValue        m_MaxUrgentThreads;
    int                      m_Threshold; ///< for delta
    /// The current number of threads in the pool
    CAtomicCounter           m_ThreadCount;
    /// The current number of urgent threads running now
    CAtomicCounter           m_UrgentThreadCount;
    /// The difference between the number of unfinished requests and
    /// the total number of threads in the pool.
    volatile int             m_Delta;
    /// The guard for m_MaxThreads, m_MaxUrgentThreads, and m_Delta.
    mutable CMutex           m_Mutex;
    /// The request queue
    TQueue                   m_Queue;
    bool                     m_QueuingForbidden;

private:
    friend class CThreadInPool<TRequest>;
    TItemHandle x_AcceptRequest(const TRequest& req, 
                                TUserPriority priority,
                                bool urgent,
                                unsigned int timeout_sec = 0,
                                unsigned int timeout_nsec = 0);

    void x_RunNewThread(ERunMode mode, CAtomicCounter* counter);
};

/////////////////////////////////////////////////////////////////////////////
//
//  SPECIALIZATIONS:
//

/////////////////////////////////////////////////////////////////////////////
//
//     CStdRequest       -- abstract request type

class CStdRequest : public CObject
{
public:
    ///Destructor
    virtual ~CStdRequest(void) {}

    /// Do the actual job
    /// Called by whichever thread handles this request.
    virtual void Process(void) = 0;

    typedef CQueueItemBase::EStatus EStatus;

    /// Callback for status changes
    virtual void OnStatusChange(EStatus /* old */, EStatus /* new */) {}
};


EMPTY_TEMPLATE
inline
void CBlockingQueue<CRef<CStdRequest> >::CQueueItem::x_SetStatus
(EStatus new_status)
{
    EStatus old_status = GetStatus();
    CQueueItemBase::x_SetStatus(new_status);
    m_Request->OnStatusChange(old_status, new_status);
}



/////////////////////////////////////////////////////////////////////////////
//
//     CStdThreadInPool  -- thread handling CStdRequest

class NCBI_XUTIL_EXPORT CStdThreadInPool
    : public CThreadInPool< CRef< CStdRequest > >
{
public:
    typedef CThreadInPool< CRef< CStdRequest > > TParent;

    /// Constructor
    ///
    /// @param pool
    ///   A pool where this thead is placed
    /// @param mode
    ///   A running mode of this thread
    CStdThreadInPool(TPool* pool, ERunMode mode = eNormal) 
        : TParent(pool, mode) {}

protected:
    /// Process a request.
    ///
    /// @param
    ///   A request for processing
    virtual void ProcessRequest(const CRef<CStdRequest>& req)
    { const_cast<CStdRequest&>(*req).Process(); }

    // Avoid shadowing the handle-based version.
    virtual void ProcessRequest(TItemHandle handle)
    { TParent::ProcessRequest(handle); }
};

/////////////////////////////////////////////////////////////////////////////
//
//     CStdPoolOfThreads -- pool of threads handling CStdRequest

class NCBI_XUTIL_EXPORT CStdPoolOfThreads
    : public CPoolOfThreads< CRef< CStdRequest > >
{
public:
    typedef CPoolOfThreads< CRef< CStdRequest > > TParent;

    /// Constructor
    ///
    /// @param max_threads
    ///   The maximum number of threads that this pool can run
    /// @param queue_size
    ///   The maximum number of requests in the queue
    /// @param spawn_threshold
    ///   The number of requests in the queue after which 
    ///   a new thread is started
    /// @param max_urgent_threads
    ///   The maximum number of urgent threads running simultaneously
    CStdPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                      unsigned int spawn_threshold = 1,
                      unsigned int max_urgent_threads = kMax_UInt)
        : TParent(max_threads, queue_size, spawn_threshold, max_urgent_threads)
        {}

    virtual ~CStdPoolOfThreads();

    enum EKillFlags {
        fKill_Wait   = 0x1, ///< Wait for all threads in the pool to finish.
        fKill_Reopen = 0x2  ///< Allow a fresh batch of worker threads.
    };
    typedef int TKillFlags; ///< binary OR of EKillFlags

    /// Causes all threads in the pool to exit cleanly after finishing
    /// all pending requests, optionally waiting for them to die.
    ///
    /// @param flags
    ///    Governs optional behavior
    virtual void KillAllThreads(TKillFlags flags);

    /// Causes all threads in the pool to exit cleanly after finishing
    /// all pending requests, optionally waiting for them to die.
    ///
    /// @param wait
    ///    If true will wait until all thread in the pool finish their job
    virtual void KillAllThreads(bool wait)
        { KillAllThreads(wait ? (fKill_Wait | fKill_Reopen) : fKill_Reopen); }

    /// Register a thread.
    ///
    /// @param thread
    ///   A thread to register
    virtual void Register(TThread& thread);

    /// Unregister a thread
    ///
    /// @param thread
    ///   A thread to unregister
    virtual void UnRegister(TThread& thread);

protected:
    /// Create a new thread
    ///
    /// @param mode
    ///   A thread's running mode
    virtual TThread* NewThread(TThread::ERunMode mode)
        { return new CStdThreadInPool(this, mode); }

private:
    typedef list<CRef<TThread> > TThreads;
    TThreads                     m_Threads;
};


NCBI_PARAM_DECL(bool, ThreadPool, Catch_Unhandled_Exceptions);
typedef NCBI_PARAM_TYPE(ThreadPool, Catch_Unhandled_Exceptions) TParamThreadPoolCatchExceptions;



/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//   CBlockingQueue<>::
//

template <typename TRequest>
typename CBlockingQueue<TRequest>::TItemHandle
CBlockingQueue<TRequest>::Put(const TRequest& data, TUserPriority priority,
                              unsigned int timeout_sec,
                              unsigned int timeout_nsec)
{
    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    TRealQueue& q = const_cast<TRealQueue&>(m_Queue);
    if ( !x_WaitForPredicate(&CBlockingQueue::x_PutSemPred, m_PutSem, guard,
                             timeout_sec, timeout_nsec) ) {
        NCBI_THROW(CBlockingQueueException, eFull,
                   "CBlockingQueue<>::Put: "
                   "attempt to insert into a full queue");
    }
    if (m_RequestCounter == 0) {
        m_RequestCounter = 0xFFFFFF;
        NON_CONST_ITERATE (typename TRealQueue, it, q) {
            CQueueItem& val = const_cast<CQueueItem&>(**it);
            val.m_Priority = (val.m_Priority & 0xFF000000) | m_RequestCounter--;
        }
    }
    /// Structure of the internal priority:
    /// The highest byte is a user specified priority;
    /// the other three bytes are a counter which ensures that 
    /// requests with the same user-specified priority are processed 
    /// in FIFO order
    TPriority real_priority = (priority << 24) | m_RequestCounter--;
    TItemHandle handle(new CQueueItem(real_priority, data));
    q.insert(handle);
    m_GetSem.TryWait();
    m_GetSem.Post();
    if (q.size() == m_MaxSize) {
        m_PutSem.TryWait();
    }
    return handle;
}


template <typename TRequest>
void CBlockingQueue<TRequest>::WaitForRoom(unsigned int timeout_sec,
                                           unsigned int timeout_nsec) const
{
    // Make sure there's room, but don't actually change any state
    CMutexGuard guard(m_Mutex);
    if (x_WaitForPredicate(&CBlockingQueue::x_PutSemPred, m_PutSem, guard,
                           timeout_sec, timeout_nsec)) {
        m_PutSem.Post(); // signal that the room still exists
    } else {
        NCBI_THROW(CBlockingQueueException, eTimedOut,
                   "CBlockingQueue<>::WaitForRoom: timed out");
    }
}

template <typename TRequest>
void CBlockingQueue<TRequest>::WaitForHunger(unsigned int timeout_sec,
                                             unsigned int timeout_nsec) const
{
    CMutexGuard guard(m_Mutex);
    if (x_WaitForPredicate(&CBlockingQueue::x_HungerSemPred, m_HungerSem, guard,
                           timeout_sec, timeout_nsec)) {
        m_HungerSem.Post();
    } else {
        NCBI_THROW(CBlockingQueueException, eTimedOut,
                   "CBlockingQueue<>::WaitForHunger: timed out");
    }
}


template <typename TRequest>
typename CBlockingQueue<TRequest>::TItemHandle
CBlockingQueue<TRequest>::GetHandle(unsigned int timeout_sec,
                                    unsigned int timeout_nsec)
{
    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    TRealQueue& q = const_cast<TRealQueue&>(m_Queue);

    if (q.empty()) {
        _VERIFY(++m_HungerCnt);
        m_HungerSem.TryWait();
        m_HungerSem.Post();

        bool ok = x_WaitForPredicate(&CBlockingQueue::x_GetSemPred, m_GetSem,
                                     guard, timeout_sec, timeout_nsec);

        if (--m_HungerCnt <= q.size()) {
            m_HungerSem.TryWait();
        }

        if ( !ok ) {
            NCBI_THROW(CBlockingQueueException, eTimedOut,
                       "CBlockingQueue<>::Get[Handle]: timed out");
        }
    }

    TItemHandle handle(*q.begin());
    q.erase(q.begin());
    if (m_HungerCnt > q.size()) {
        m_HungerSem.TryWait();
        m_HungerSem.Post();
    }
    if ( ! q.empty() ) {
        m_GetSem.TryWait();
        m_GetSem.Post();
    }

    // Get the attention of WaitForRoom() or the like; do this
    // regardless of queue size because derived classes may want
    // to insert multiple objects atomically.
    m_PutSem.TryWait();
    m_PutSem.Post();

    guard.Release(); // avoid possible deadlocks from x_SetStatus
    handle->x_SetStatus(CQueueItem::eActive);
    return handle;
}

template <typename TRequest>
TRequest CBlockingQueue<TRequest>::Get(unsigned int timeout_sec,
                                       unsigned int timeout_nsec)
{
    TItemHandle handle = GetHandle(timeout_sec, timeout_nsec);
    handle->MarkAsComplete(); // almost certainly premature, but our last chance
    return handle->GetRequest();
}


template <typename TRequest>
size_t CBlockingQueue<TRequest>::GetSize(void) const
{
    CMutexGuard guard(m_Mutex);
    return const_cast<const TRealQueue&>(m_Queue).size();
}


template <typename TRequest>
void CBlockingQueue<TRequest>::SetUserPriority(TItemHandle handle,
                                               TUserPriority priority)
{
    if (handle->GetUserPriority() == priority
        ||  handle->GetStatus() != CQueueItem::ePending) {
        return;
    }
    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    TRealQueue& q = const_cast<TRealQueue&>(m_Queue);
    typename TRealQueue::iterator it = q.find(handle);
    // These sanity checks protect against race conditions and
    // accidental use of handles from other queues.
    if (it != q.end()  &&  *it == handle) {
        q.erase(it);
        TPriority counter = handle->m_Priority & 0xFFFFFF;
        handle->m_Priority = (priority << 24) | counter;
        q.insert(handle);
    }
}


template <typename TRequest>
void CBlockingQueue<TRequest>::Withdraw(TItemHandle handle)
{
    if (handle->GetStatus() != CQueueItem::ePending) {
        return;
    }
    {{
        CMutexGuard guard(m_Mutex);
        // Having the mutex, we can safely drop "volatile"
        TRealQueue& q = const_cast<TRealQueue&>(m_Queue);
        typename TRealQueue::iterator it = q.find(handle);
        // These sanity checks protect against race conditions and
        // accidental use of handles from other queues.
        if (it != q.end()  &&  *it == handle) {
            q.erase(it);   
            
            if(q.empty())   {
                // m_GetSem may be signaled - clear it
                m_GetSem.TryWait();
            }
        } else {
            return;
        }
    }}
    // run outside the guard to avoid possible deadlocks from x_SetStatus
    handle->x_SetStatus(CQueueItem::eWithdrawn);
}

template <typename TRequest>
bool CBlockingQueue<TRequest>::x_WaitForPredicate(TQueuePredicate pred,
                                                  CSemaphore& sem,
                                                  CMutexGuard& guard,
                                                  unsigned int timeout_sec,
                                                  unsigned int timeout_nsec)
    const
{
    const TRealQueue& q = const_cast<const TRealQueue&>(m_Queue);
    if ( !(this->*pred)(q) ) {
#if SIZEOF_INT == SIZEOF_LONG
        // If long is larger, converting from unsigned int to (signed)
        // long for CTimeSpan will automatically be safe.
        unsigned int extra_sec = timeout_nsec / kNanoSecondsPerSecond;
        timeout_nsec %= kNanoSecondsPerSecond;
        // Do the comparison this way to avoid overflow.
        if (timeout_sec >= kMax_Int - extra_sec) {
            timeout_sec = kMax_Int; // clamp
        } else {
            timeout_sec += extra_sec;
        }
#endif
        // _ASSERT(timeout_nsec <= (unsigned long)kMax_Long);
        CTimeSpan span(timeout_sec, timeout_nsec);
        while (span.GetSign() == ePositive  &&  !(this->*pred)(q) ) {
            CTime start(CTime::eCurrent, CTime::eGmt);
            // Temporarily release the mutex while waiting, to avoid deadlock.
            guard.Release();
            sem.TryWait(span.GetCompleteSeconds(),
                        span.GetNanoSecondsAfterSecond());
            guard.Guard(m_Mutex);
            span -= CurrentTime(CTime::eGmt) - start;
        }
    }
    sem.TryWait();
    return (this->*pred)(q);
}

/////////////////////////////////////////////////////////////////////////////
//   CThreadInPool<>::
//

template <typename TRequest>
void CThreadInPool<TRequest>::CountSelf(CAtomicCounter* counter)
{
    _ASSERT(m_Counter == NULL);
    counter->Add(1);
    m_Counter = counter;
}

template <typename TRequest>
CThreadInPool<TRequest>::~CThreadInPool()
{
    if (m_Counter != NULL) {
        m_Counter->Add(-1);
    }
}

template <typename TRequest>
CThreadInPool<TRequest>::CAutoUnregGuard::CAutoUnregGuard(TThread* thr)
    : m_Thread(thr)
{}

template <typename TRequest>
CThreadInPool<TRequest>::CAutoUnregGuard::~CAutoUnregGuard(void)
{
    m_Thread->x_UnregisterThread();
}


template <typename TRequest>
void CThreadInPool<TRequest>::x_UnregisterThread(void)
{
    if (m_Counter != NULL) {
        m_Counter->Add(-1);
        m_Counter = NULL;
    }
    m_Pool->UnRegister(*this);
}

template <typename TRequest>
void CThreadInPool<TRequest>::x_HandleOneRequest(bool catch_all)
{
    TItemHandle handle;
    {{
        CMutexGuard guard(m_Pool->m_Mutex);
        --m_Pool->m_Delta;
    }}
    try {
        handle.Reset(m_Pool->m_Queue.GetHandle());
    } catch (CBlockingQueueException& e) {
        // work around "impossible" timeouts
        NCBI_REPORT_EXCEPTION_XX(Util_Thread, 1, "Unexpected timeout", e);
        CMutexGuard guard(m_Pool->m_Mutex);
        ++m_Pool->m_Delta;
        return;
    }
    if (catch_all) {
        try {
            ProcessRequest(handle);
        } catch (std::exception& e) {
            handle->MarkAsForciblyCaught();
            NCBI_REPORT_EXCEPTION_XX(Util_Thread, 2,
                                     "Exception from thread in pool: ", e);
            // throw;
        } catch (...) {
            handle->MarkAsForciblyCaught();
            // silently propagate non-standard exceptions because they're
            // likely to be CExitThreadException.
            // ERR_POST_XX(Util_Thread, 3,
            //             "Thread in pool threw non-standard exception.");
            throw;
        }
    }
    else {
        ProcessRequest(handle);
    }
}

template <typename TRequest>
void* CThreadInPool<TRequest>::Main(void)
{
    try {
        m_Pool->Register(*this);
    } catch (CThreadException&) {
        ERR_POST(Warning << "New worker thread blocked at the last minute.");
        return 0;
    }
    CAutoUnregGuard guard(this);

    Init();
    bool catch_all = TParamThreadPoolCatchExceptions::GetDefault();

    for (;;) {
        x_HandleOneRequest(catch_all);
        if (m_RunMode == eRunOnce)
            break;
    }

    return 0;
}


template <typename TRequest>
void CThreadInPool<TRequest>::OnExit(void)
{
    try {
        x_OnExit();
    } STD_CATCH_ALL_XX(Util_Thread, 6, "x_OnExit")
}

template <typename TRequest>
void CThreadInPool<TRequest>::ProcessRequest(TItemHandle handle)
{
    TCompletingHandle completer = handle;
    ProcessRequest(completer->GetRequest());
}


/////////////////////////////////////////////////////////////////////////////
//   CPoolOfThreads<>::
//

template <typename TRequest>
CPoolOfThreads<TRequest>::CPoolOfThreads(unsigned int max_threads,
                                         unsigned int queue_size,
                                         unsigned int spawn_threshold, 
                                         unsigned int max_urgent_threads)
    : m_MaxThreads(max_threads), m_MaxUrgentThreads(max_urgent_threads),
      m_Threshold(spawn_threshold), m_Delta(0),
      m_Queue(queue_size > 0 ? queue_size : max_threads),
      m_QueuingForbidden(queue_size == 0)
{
    m_ThreadCount.Set(0);
    m_UrgentThreadCount.Set(0);
}


template <typename TRequest>
CPoolOfThreads<TRequest>::~CPoolOfThreads(void)
{
    CAtomicCounter::TValue n = m_ThreadCount.Get() + m_UrgentThreadCount.Get();
    if (n) {
        ERR_POST_XX(Util_Thread, 4,
                    Warning << "CPoolOfThreads<>::~CPoolOfThreads: "
                            << n << " thread(s) still active");
    }
}

template <typename TRequest>
void CPoolOfThreads<TRequest>::Spawn(unsigned int num_threads)
{
    for (unsigned int i = 0; i < num_threads; i++)
    {
        x_RunNewThread(TThread::eNormal, &m_ThreadCount);
    }
}


template <typename TRequest>
inline
typename CPoolOfThreads<TRequest>::TItemHandle
CPoolOfThreads<TRequest>::AcceptRequest(const TRequest& req, 
                                        TUserPriority priority,
                                        unsigned int timeout_sec,
                                        unsigned int timeout_nsec)
{
    return x_AcceptRequest(req, priority, false, timeout_sec, timeout_nsec);
}

template <typename TRequest>
inline
typename CPoolOfThreads<TRequest>::TItemHandle
CPoolOfThreads<TRequest>::AcceptUrgentRequest(const TRequest& req,
                                              unsigned int timeout_sec,
                                              unsigned int timeout_nsec)
{
    return x_AcceptRequest(req, 0xFF, true, timeout_sec, timeout_nsec);
}

template <typename TRequest>
inline
bool CPoolOfThreads<TRequest>::HasImmediateRoom(bool urgent) const
{
    CMutexGuard guard(m_Mutex);

    if (m_Queue.IsFull()) {
        return false; // temporary blockage
    } else if (m_Delta < 0) {
        return true;
    } else if (m_ThreadCount.Get() < m_MaxThreads) {
        return true;
    } else if (urgent  &&  m_UrgentThreadCount.Get() < m_MaxUrgentThreads) {
        return true;
    } else {
        try {
            m_Queue.WaitForHunger(0);
            // This should be redundant with the m_Delta < 0 case, now that
            // m_Mutex guards it.
            ERR_POST_XX(Util_Thread, 5,
                        "Possible thread pool bug.  delta: "
                          << const_cast<int&>(m_Delta)
                          << "; hunger: " << m_Queue.GetHunger());
            return true;
        } catch (...) {
        }
        return false;
    }
}

template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::WaitForRoom(unsigned int timeout_sec,
                                           unsigned int timeout_nsec) 
{
    if (HasImmediateRoom()) {
        return;
    } else if (m_QueuingForbidden) {
        m_Queue.WaitForHunger(timeout_sec, timeout_nsec);
    } else {
        m_Queue.WaitForRoom(timeout_sec, timeout_nsec);
    }
}

template <typename TRequest>
inline
typename CPoolOfThreads<TRequest>::TItemHandle
CPoolOfThreads<TRequest>::x_AcceptRequest(const TRequest& req, 
                                          TUserPriority priority,
                                          bool urgent,
                                          unsigned int timeout_sec,
                                          unsigned int timeout_nsec)
{
    bool new_thread = false;
    TItemHandle handle;
    {{
        CMutexGuard guard(m_Mutex);
        // we reserved 0xFF priority for urgent requests
        if( priority == 0xFF && !urgent ) 
            --priority;
        if (m_QueuingForbidden  &&  !HasImmediateRoom(urgent) ) {
            NCBI_THROW(CBlockingQueueException, eFull,
                       "CPoolOfThreads<>::x_AcceptRequest: "
                       "attempt to insert into a full queue");
        }
        handle = m_Queue.Put(req, priority, timeout_sec, timeout_nsec);
        if (++m_Delta >= m_Threshold
            &&  m_ThreadCount.Get() < m_MaxThreads) {
            // Add another thread to the pool because they're all busy.
            new_thread = true;
        } else if (urgent && m_UrgentThreadCount.Get() >= m_MaxUrgentThreads) {
            // Prevent from running a new urgent thread if we have reached
            // the maximum number of urgent threads
            urgent = false;
        }
    }}

    if (new_thread) {
        x_RunNewThread(TThread::eNormal, &m_ThreadCount);
    } else if (urgent) {
        x_RunNewThread(TThread::eRunOnce, &m_UrgentThreadCount);
    }

    return handle;
}

template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::x_RunNewThread(ERunMode mode,
                                              CAtomicCounter* counter)
{
    try {
        CRef<TThread> thr(NewThread(mode));
        thr->CountSelf(counter);
        thr->Run();
    }
    catch (CThreadException& ex) {
        ERR_POST_XX(Util_Thread, 13,
                    Critical << "Ignoring error while starting new thread: "
                    << ex);
    }
}

template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::SetUserPriority(TItemHandle handle,
                                               TUserPriority priority)
{
    // Maintain segregation between urgent and non-urgent requests
    if (handle->GetUserPriority() == 0xFF) {
        return;
    } else if (priority == 0xFF) {
        priority = 0xFE;
    }
    m_Queue.SetUserPriority(handle, priority);
}

END_NCBI_SCOPE


/* @} */

#endif  /* UTIL__THREAD_POOL_OLD__HPP */
