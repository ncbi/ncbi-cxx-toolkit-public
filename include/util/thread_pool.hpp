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

#include <deque>


/** @addtogroup ThreadedPools
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  TEMPLATES:
//
//     CBlockingQueue<>  -- queue of requests, with efficiently blocking Get()
//     CThreadInPool<>   -- abstract request-handling thread
//     CPoolOfThreads<>  -- abstract pool of threads sharing a request queue
//

template <typename TRequest>
class CBlockingQueue
{
public:
    CBlockingQueue(unsigned int max_size = kMax_UInt)
        : m_GetSem(0,1), m_PutSem(1,1), m_MaxSize(max_size) {}

    void         Put(const TRequest& data); // Throws exception if full
    void         WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                             unsigned int timeout_nsec = 0) const;
    // Blocks politely if empty
    TRequest     Get(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0);
    unsigned int GetSize(void) const;
    unsigned int GetMaxSize(void) const { return m_MaxSize; }
    bool         IsEmpty(void) const    { return GetSize() == 0; }
    bool         IsFull(void) const     { return GetSize() == GetMaxSize(); }

protected:
    // Derived classes should take care to use these members properly.
    volatile deque<TRequest> m_Queue;
    CSemaphore               m_GetSem; // Raised iff the queue contains data
    mutable CSemaphore       m_PutSem; // Raised iff the queue has room
    mutable CMutex           m_Mutex;  // Guards access to queue

private:
    unsigned int             m_MaxSize;
};


// Forward declaration
template <typename TRequest> class CPoolOfThreads;


template <typename TRequest>
/* abstract */ class CThreadInPool : public CThread
{
public:
    typedef CPoolOfThreads<TRequest> TPool;

    CThreadInPool(TPool* pool) : m_Pool(pool) {}

protected:
    virtual ~CThreadInPool(void) {}

    virtual void Init(void) {} // called at beginning of Main()

    // Called from Main() for each request this thread handles
    virtual void ProcessRequest(const TRequest& req) = 0;

    virtual void x_OnExit(void) {} // called by OnExit()

private:
    // to prevent overriding; inherited from CThread
    virtual void* Main(void);
    virtual void OnExit(void);

    TPool* m_Pool;
};


template <typename TRequest>
/* abstract */ class CPoolOfThreads
{
public:
    friend class CThreadInPool<TRequest>;
    typedef CThreadInPool<TRequest> TThread;

    CPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                   int spawn_threshold = 1)
        : m_MaxThreads(max_threads), m_Threshold(spawn_threshold),
          m_Queue(queue_size)
        { m_ThreadCount.Set(0);  m_Delta.Set(0); }
    virtual ~CPoolOfThreads(void);

    void Spawn(unsigned int num_threads);
    void AcceptRequest(const TRequest& req);
    void WaitForRoom(unsigned int timeout_sec  = kMax_UInt,
                     unsigned int timeout_nsec = 0) 
        { m_Queue.WaitForRoom(timeout_sec, timeout_nsec); }
    bool IsFull(void) const { return m_Queue.IsFull(); }

protected:
    virtual TThread* NewThread(void) = 0;

    CAtomicCounter           m_ThreadCount;
    volatile unsigned int    m_MaxThreads;
    CAtomicCounter           m_Delta;     // # unfinished requests - # threads
    int                      m_Threshold; // for delta
    CMutex                   m_Mutex;     // for m_MaxThreads
    CBlockingQueue<TRequest> m_Queue;
};

/////////////////////////////////////////////////////////////////////////////
//
//  SPECIALIZATIONS:
//
//     CStdRequest       -- abstract request type
//     CStdThreadInPool  -- thread handling CStdRequest
//     CStdPoolOfThreads -- pool of threads handling CStdRequest
//

/* abstract */ class CStdRequest : public CObject
{
public:
    virtual ~CStdRequest(void) {}

    // Called by whichever thread handles this request.
    virtual void Process(void) = 0;
};


class NCBI_XUTIL_EXPORT CStdThreadInPool
    : public CThreadInPool< CRef< CStdRequest > >
{
public:
    typedef CThreadInPool< CRef< CStdRequest > > TParent;

    CStdThreadInPool(TPool* pool) : TParent(pool) {}

protected:
    virtual void ProcessRequest(const CRef<CStdRequest>& req)
        { const_cast<CStdRequest&>(*req).Process(); }

    // virtual void Init(void); // called before processing any requests
    // virtual void x_OnExit(void); // called just before exiting
};


class NCBI_XUTIL_EXPORT CStdPoolOfThreads
    : public CPoolOfThreads< CRef< CStdRequest > >
{
public:
    typedef CPoolOfThreads< CRef< CStdRequest > > TParent;
    CStdPoolOfThreads(unsigned int max_threads, unsigned int queue_size,
                      int spawn_threshold = 1)
        : TParent(max_threads, queue_size, spawn_threshold) {}

    // void Spawn(unsigned int num_threads);
    // void AcceptRequest(const TRequest& req);

    // Causes all threads in the pool to exit cleanly after finishing
    // all pending requests, optionally waiting for them to die.
    virtual void KillAllThreads(bool wait);

protected:
    virtual TThread* NewThread(void)
        { return new CStdThreadInPool(this); }
};


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//   CBlockingQueue<>::
//

template <typename TRequest>
void CBlockingQueue<TRequest>::Put(const TRequest& data)
{
    CMutexGuard guard(m_Mutex);
    // Having the mutex, we can safely drop "volatile"
    deque<TRequest>& q = const_cast<deque<TRequest>&>(m_Queue);
    if (q.size() == m_MaxSize) {
        m_PutSem.TryWait();
        NCBI_THROW(CBlockingQueueException, eFull, "CBlockingQueue<>::Put: "
                   "attempt to insert into a full queue");
    } else if (q.empty()) {
        m_GetSem.Post();
    }
    q.push_back(data);
}


template <typename TRequest>
void CBlockingQueue<TRequest>::WaitForRoom(unsigned int timeout_sec,
                                           unsigned int timeout_nsec) const
{
    // Make sure there's room, but don't actually consume anything
    if (m_PutSem.TryWait(timeout_sec, timeout_nsec)) {
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
    deque<TRequest>& q = const_cast<deque<TRequest>&>(m_Queue);
    TRequest result = q.front();
    q.pop_front();
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
    return const_cast<const deque<TRequest>&>(m_Queue).size();
}


/////////////////////////////////////////////////////////////////////////////
//   CThreadInPool<>::
//

template <typename TRequest>
void* CThreadInPool<TRequest>::Main(void)
{
    Detach();
    Init();

    for (;;) {
        m_Pool->m_Delta.Add(-1);
        ProcessRequest(m_Pool->m_Queue.Get());
    }

    return 0; // Unreachable, but necessary for WorkShop build
}


template <typename TRequest>
void CThreadInPool<TRequest>::OnExit(void)
{
    try {
        x_OnExit();
    } catch (...) {
        // Ignore exceptions; there's nothing useful we can do anyway
    }
    m_Pool->m_ThreadCount.Add(-1);
}


/////////////////////////////////////////////////////////////////////////////
//   CPoolOfThreads<>::
//

template <typename TRequest>
CPoolOfThreads<TRequest>::~CPoolOfThreads(void)
{
    CAtomicCounter::TValue n = m_ThreadCount.Get();
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
        NewThread()->Run();
    }
}


template <typename TRequest>
void CPoolOfThreads<TRequest>::AcceptRequest(const TRequest& req)
{
    bool new_thread = false;
    {{
        CMutexGuard guard(m_Mutex);
        m_Queue.Put(req);
        if (static_cast<int>(m_Delta.Add(1)) >= m_Threshold
            &&  m_ThreadCount.Get() < m_MaxThreads) {
            // Add another thread to the pool because they're all busy.
            m_ThreadCount.Add(1);
            new_thread = true;
        }
    }}

    if (new_thread) {
        NewThread()->Run();
    }
}

END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
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
