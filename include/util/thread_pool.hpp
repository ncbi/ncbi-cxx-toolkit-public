#ifndef THREAD_POOL__HPP
#define THREAD_POOL__HPP

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
#include <corelib/ncbi_limits.hpp>
#include <util/util_exception.hpp>

#include <set>


/** @addtogroup ThreadedPools
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
///     CBlockingQueue<>  -- queue of requests, with efficiently blocking Get()

template <typename TRequest>
class CBlockingQueue
{
public:
    /// Constructor
    ///
    /// @param max_size
    ///   The maximum size of tht queue
    CBlockingQueue(unsigned int max_size = kMax_UInt)
        : m_GetSem(0,1), m_PutSem(1,1), 
          m_MaxSize(min<unsigned int>(max_size,0xFFFFFF)),
          m_RequestCounter(0xFFFFFF)  {}

    /// Put a request into the queue. Throws exception if full.
    ///
    /// @param request
    ///   Request
    /// @param priority
    ///   A priority of the request. The higher the priority 
    ///   the sooner the request will be processed.   
    void         Put(const TRequest& request, Uint1 priority = 0); 

    /// Wait for the room in the queue up to
    /// timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nonoseconds
    void         WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                             unsigned int timeout_nsec = 0) const;

    /// Get the first available requst from the queue. 
    /// Blocks politely if empty. 
    /// Waits up to timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nonoseconds
    TRequest     Get(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0);

    /// Get a number of requests in the queue
    unsigned int GetSize    (void) const;

    /// Get the maximun number of requests that can be put into the queue
    unsigned int GetMaxSize (void) const { return m_MaxSize; }

    /// Check if the queue is empty
    bool         IsEmpty    (void) const { return GetSize() == 0; }

    /// Check if the queue is full
    bool         IsFull     (void) const { return GetSize() == GetMaxSize(); }

protected:
    class CQueueItem
    {
    public:
        CQueueItem(Uint4 priority, TRequest request) 
            : m_Priority(priority), m_Request(request) {}
    
        bool operator> (const CQueueItem& item) const 
        { return m_Priority > item.m_Priority; }

        Uint4& GetPriority(void) { return m_Priority; }
        TRequest& GetRequest(void)  { return m_Request; } 
        
    private:
        Uint4 m_Priority;
        TRequest m_Request;
    };
    /// The type of the queue
    typedef set<CQueueItem, greater<CQueueItem> > TQueue;

    // Derived classes should take care to use these members properly.
    volatile TQueue     m_Queue;  /// The Queue
    CSemaphore          m_GetSem; /// Raised if the queue contains data
    mutable CSemaphore  m_PutSem; /// Raised if the queue has room
    mutable CMutex      m_Mutex;  /// Guards access to queue

private:
    unsigned int        m_MaxSize;        /// The maximum size of the queue
    Uint4               m_RequestCounter; ///
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

    /// Thread run mode 
    enum ERunMode {
        eNormal,   /// Process request and stay in the pool
        eRunOnce   /// Process request and die
    };

    /// Constructor
    ///
    /// @param pool
    ///   A pool where this thead is placed
    /// @param mode
    ///   A running mode of this thread
    CThreadInPool(TPool* pool, ERunMode mode = eNormal) 
        : m_Pool(pool), m_RunMode(mode) {}

protected:
    /// Destructor
    virtual ~CThreadInPool(void) {}

    /// Intit this thread. It is called at beginning of Main()
    virtual void Init(void) {}

    /// Process a request.
    /// It is called from Main() for each request this thread handles
    ///
    /// @param
    ///   A request for processing
    virtual void ProcessRequest(const TRequest& req) = 0;

    /// Clean up. It is called by OnExit()
    virtual void x_OnExit(void) {}

    /// Get run mode
    ERunMode GetRunMode(void) const { return m_RunMode; }

private:
    // to prevent overriding; inherited from CThread
    virtual void* Main(void);
    virtual void OnExit(void);

    TPool*   m_Pool;     /// A pool that holds this thread
    ERunMode m_RunMode;  /// A running mode

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

    /// Constructor
    ///
    /// @param max_threads
    ///   The maximum number of threads that this pool can run
    /// @param queue_size
    ///   The maximum number of requsets in the queue
    /// @param spawn_threashold
    ///   The number of requests in the queue after which 
    ///   a new thread is statred
    /// @param max_urgent_threads
    ///   The maximum number of urgent threads running simultaneously
    CPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                   int spawn_threshold = 1, 
                   unsigned int max_urgent_threads = kMax_UInt)
        : m_MaxThreads(max_threads), m_MaxUrgentThreads(max_urgent_threads),
          m_Threshold(spawn_threshold), m_Queue(queue_size)
        { m_ThreadCount.Set(0); m_UrgentThreadCount.Set(0); m_Delta.Set(0);}

    /// Destructor
    virtual ~CPoolOfThreads(void);

    /// Start processing threads
    ///
    /// @param num_threads
    ///    A number of threads to start
    void Spawn(unsigned int num_threads);

    /// Put a request in the queue with a given priority
    ///
    /// @param request
    ///   A request
    /// @param priority
    ///   A priority of the request. The higher the priority 
    ///   the sooner the request will be processed.   
    void AcceptRequest(const TRequest& request, Uint1 priority = 0);

    /// Puts a request in the queue with the highest priority
    /// It will run a new thread even if the maximum of allowed threads 
    /// has been already reached
    ///
    /// @param request
    ///   A request
    void AcceptUrgentRequest(const TRequest& request);

    /// Wait for the room in the queue up to
    /// timeout_sec + timeout_nsec/1E9 seconds.
    ///
    /// @param timeout_sec
    ///   Number of seconds
    /// @param timeout_nsec
    ///   Number of nonoseconds
    void WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0) 
        { m_Queue.WaitForRoom(timeout_sec, timeout_nsec); }
  
    /// Check if the queue is full
    bool IsFull(void) const { return m_Queue.IsFull(); }

protected:

    /// Create a new thread
    ///
    /// @param mode
    ///   A thread's running mode
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
    TACValue                 m_Threshold; // for delta
    /// The current number of threads the pool
    CAtomicCounter           m_ThreadCount;
    /// The current number of urgent threads running now
    CAtomicCounter           m_UrgentThreadCount;
    /// numbet unfinished requests - number threads in the pool
    CAtomicCounter           m_Delta;     
    /// The guard for m_MaxThreads and m_MaxUrgentThreads
    CMutex                   m_Mutex;
    /// The requests queue
    CBlockingQueue<TRequest> m_Queue;

private:
    friend class CThreadInPool<TRequest>;
    void x_AcceptRequest(const TRequest& req, 
                         Uint1 priority,
                         bool urgent);

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

    /// Do the actial job
    /// Called by whichever thread handles this request.
    virtual void Process(void) = 0;
};


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
    ///   The maximum number of requsets in the queue
    /// @param spawn_threashold
    ///   The number of requests in the queue after which 
    ///   a new thread is statred
    /// @param max_urgent_threads
    ///   The maximum number of urgent threads running simultaneously
    CStdPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                      int spawn_threshold = 1,
                      unsigned int max_urgent_threads = kMax_UInt)
        : TParent(max_threads, queue_size, 
                  spawn_threshold,max_urgent_threads) {}

    /// Causes all threads in the pool to exit cleanly after finishing
    /// all pending requests, optionally waiting for them to die.
    ///
    /// @param wait
    ///    If true will wait until all thread in the pool finish their job
    virtual void KillAllThreads(bool wait);

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


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//   CBlockingQueue<>::
//

template <typename TRequest>
void CBlockingQueue<TRequest>::Put(const TRequest& data, Uint1 priority)
{
    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    TQueue& q = const_cast<TQueue&>(m_Queue);
    if (q.size() == m_MaxSize) {
        NCBI_THROW(CBlockingQueueException, eFull, "CBlockingQueue<>::Put: "
                   "attempt to insert into a full queue");
    } else if (q.empty()) {
        m_GetSem.Post();
    }
    if (m_RequestCounter == 0) {
        m_RequestCounter = 0xFFFFFF;
        for (typename TQueue::iterator it = q.begin(); it != q.end(); ++it) {
            CQueueItem& val = const_cast<CQueueItem&>(*it);
            val.GetPriority() = (val.GetPriority() & 0xFF000000) 
                                    + m_RequestCounter--;
        }
    }
    /// Structure of the internal priority
    /// The highest byte is a user specified priory,
    /// the next 3 bytes are a counter which insures that 
    /// requests with the same user's priority are processed 
    /// in FIFO order
    Uint4 real_priority = (priority << 24) + m_RequestCounter--;
    q.insert( CQueueItem(real_priority,data) );
    if (q.size() == m_MaxSize) {
        m_PutSem.TryWait();
    }
}


template <typename TRequest>
void CBlockingQueue<TRequest>::WaitForRoom(unsigned int timeout_sec,
                                           unsigned int timeout_nsec) const
{
    // Make sure there's room, but don't actually consume anything
    if (m_PutSem.TryWait(timeout_sec, timeout_nsec)) {
        // We couldn't acquire the mutex previously without risk of
        // deadlock, so we acquire it now and ensure that the
        // semaphore's still down before attempting to raise it, to
        // prevent races with Get().
        CMutexGuard guard(m_Mutex);
        m_PutSem.TryWait();
        m_PutSem.Post();
    } else {
        NCBI_THROW(CBlockingQueueException, eTimedOut,
                   "CBlockingQueue<>::WaitForRoom: timed out");        
    }
}


template <typename TRequest>
TRequest CBlockingQueue<TRequest>::Get(unsigned int timeout_sec,
                                       unsigned int timeout_nsec)
{
    if ( !m_GetSem.TryWait(timeout_sec, timeout_nsec) ) {
        NCBI_THROW(CBlockingQueueException, eTimedOut,
                   "CBlockingQueue<>::Get: timed out");        
    }

    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    TQueue& q = const_cast<TQueue&>(m_Queue);
    CQueueItem& item = const_cast<CQueueItem&>(*q.begin());
    TRequest result = item.GetRequest();
    q.erase(q.begin());
    if ( ! q.empty() ) {
        m_GetSem.Post();
    }

    // Get the attention of WaitForRoom() or the like; do this
    // regardless of queue size because derived classes may want
    // to insert multiple objects atomically.
    m_PutSem.TryWait();
    m_PutSem.Post();
    return result;
}


template <typename TRequest>
unsigned int CBlockingQueue<TRequest>::GetSize(void) const
{
    CMutexGuard guard(m_Mutex);
    return const_cast<const TQueue&>(m_Queue).size();
}


/////////////////////////////////////////////////////////////////////////////
//   CThreadInPool<>::
//

template <typename TRequest>
void* CThreadInPool<TRequest>::Main(void)
{
    m_Pool->Register(*this);
    Init();

    for (;;) {
        m_Pool->m_Delta.Add(-1);
        ProcessRequest(m_Pool->m_Queue.Get());
        if (m_RunMode == eRunOnce) {
            m_Pool->UnRegister(*this);
            break;
        }
    }

    return 0; // Unreachable, but necessary for WorkShop build
}


template <typename TRequest>
void CThreadInPool<TRequest>::OnExit(void)
{
    try {
        x_OnExit();
    } STD_CATCH_ALL("x_OnExit")
    m_Pool->m_Delta.Add(-1);
    if (m_RunMode != eRunOnce)
        m_Pool->m_ThreadCount.Add(-1);
    else 
        m_Pool->m_UrgentThreadCount.Add(-1);
}


/////////////////////////////////////////////////////////////////////////////
//   CPoolOfThreads<>::
//

template <typename TRequest>
CPoolOfThreads<TRequest>::~CPoolOfThreads(void)
{
    CAtomicCounter::TValue n = m_ThreadCount.Get() + m_UrgentThreadCount.Get();
    if (n) {
        ERR_POST(Warning << "CPoolOfThreads<>::~CPoolOfThreads: "
                 << n << " thread(s) still active");
    }
}

template <typename TRequest>
void CPoolOfThreads<TRequest>::Spawn(unsigned int num_threads)
{
    for (unsigned int i = 0; i < num_threads; i++)
    {
        m_ThreadCount.Add(1);
        NewThread(TThread::eNormal)->Run();
    }
}


template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::AcceptRequest(const TRequest& req, 
                                             Uint1 priority)
{
    x_AcceptRequest(req, priority, false);
}
template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::AcceptUrgentRequest(const TRequest& req)
{
    x_AcceptRequest(req, 0xFF, true);
}

template <typename TRequest>
inline
void CPoolOfThreads<TRequest>::x_AcceptRequest(const TRequest& req, 
                                               Uint1 priority,
                                               bool urgent)
{
    bool new_thread = false;
    {{
        CMutexGuard guard(m_Mutex);
        // we reserved 0xFF priority for urgent requests
        if( priority == 0xFF && !urgent ) 
            --priority;
        m_Queue.Put(req, priority);
        if (m_Delta.Add(1) >= m_Threshold
            &&  m_ThreadCount.Get() < m_MaxThreads) {
            // Add another thread to the pool because they're all busy.
            m_ThreadCount.Add(1);
            new_thread = true;
        } else if (urgent && m_UrgentThreadCount.Get() < m_MaxUrgentThreads) {
            m_UrgentThreadCount.Add(1);
        } else  // Prevent from running a new urgent thread if we have reached
                // the maximum number of urgent threads
            urgent = false;
    }}

    if (urgent || new_thread) {
        ERunMode mode = urgent && !new_thread ? 
                             TThread::eRunOnce :
                             TThread::eNormal;
        NewThread(mode)->Run();
    }
}

END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
* Revision 1.19  2005/03/23 20:37:44  didenko
* Moved checking the queue size and raising the semaphore after a request insertion in CBlockingQueue<>::Put method
*
* Revision 1.18  2005/03/14 17:10:30  didenko
* + request priority to CBlockingQueue and CPoolOfThreads
* + DoxyGen
*
* Revision 1.17  2004/10/19 15:32:00  ucko
* CBlockingQueue<>::WaitForRoom: guard against possible races with Get().
* CThreadInPool<>::OnExit: report exceptions from x_OnExit rather than
* discarding them altogether.
*
* Revision 1.16  2004/10/18 14:43:19  ucko
* Add a little more documentation.
*
* Revision 1.15  2004/09/08 14:21:04  ucko
* Rework again to eliminate races in KillAllThreads.
*
* Revision 1.14  2004/08/24 15:01:38  ucko
* Tweak to avoid signed/unsigned comparisons.
*
* Revision 1.13  2004/07/15 18:51:21  ucko
* Make CBlockingQueue's default timeout arguments saner, and let
* CPoolOfThreads::WaitForRoom take an optional timeout too.
*
* Revision 1.12  2004/06/02 17:49:08  ucko
* CPoolOfThreads: change type of m_Delta and m_ThreadCount to
* CAtomicCounter to reduce need for m_Mutex; warn if any threads are
* still active when the destructor runs.
*
* Revision 1.11  2003/04/17 17:50:37  siyan
* Added doxygen support
*
* Revision 1.10  2003/02/26 21:34:06  gouriano
* modify C++ exceptions thrown by this library
*
* Revision 1.9  2002/12/19 14:51:00  dicuccio
* Added export specifier for Win32 DLL builds.
*
* Revision 1.8  2002/11/04 21:29:00  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.7  2002/09/13 15:16:03  ucko
* Give CBlockingQueue<>::{WaitForRoom,Get} optional timeouts (infinite
* by default); change exceptions to use new setup.
*
* Revision 1.6  2002/04/18 15:38:19  ucko
* Use "deque" instead of "queue" -- more general, and less likely to
* yield any name conflicts.
* Make most of CBlockingQueue<>'s data protected for the benefit of
* derived classes.
* Move CVS log to end.
*
* Revision 1.5  2002/04/11 15:12:52  ucko
* Added GetSize and GetMaxSize methods to CBlockingQueue and rewrote
* Is{Empty,Full} in terms of them.
*
* Revision 1.4  2002/01/25 15:46:06  ucko
* Add more methods needed by new threaded-server code.
* Minor cleanups.
*
* Revision 1.3  2002/01/24 20:17:49  ucko
* Introduce new exception class for full queues
* Allow waiting for a full queue to have room again
*
* Revision 1.2  2002/01/07 20:15:06  ucko
* Fully initialize thread-pool state.
*
* Revision 1.1  2001/12/11 19:54:44  ucko
* Introduce thread pools.
*
* ===========================================================================
*/

#endif  /* THREAD_POOL__HPP */
