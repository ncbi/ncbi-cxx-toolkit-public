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

#include <deque>

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
    class CException : public runtime_error
    {
    public:
        CException(const string& s) : runtime_error(s) {}
    };

    CBlockingQueue(unsigned int max_size = kMax_UInt)
        : m_GetSem(0,1), m_PutSem(1,1), m_MaxSize(max_size) {}

    void         Put(const TRequest& data); // Throws exception if full
    void         WaitForRoom(void) const;
    TRequest     Get(void);                 // Blocks politely if empty
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
        : m_ThreadCount(0), m_MaxThreads(max_threads),
          m_Delta(0), m_Threshold(spawn_threshold), m_Queue(queue_size)
        {}
    virtual ~CPoolOfThreads(void) {}

    void Spawn(unsigned int num_threads);
    void AcceptRequest(const TRequest& req);
    void WaitForRoom(void)  { m_Queue.WaitForRoom(); }
    bool IsFull(void) const { return m_Queue.IsFull(); }

protected:
    virtual TThread* NewThread(void) = 0;

    volatile unsigned int    m_ThreadCount;
    volatile unsigned int    m_MaxThreads;
    volatile int             m_Delta;     // # unfinished requests - # threads
    int                      m_Threshold; // for delta
    CMutex                   m_Mutex;     // for volatile counters
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


class CStdThreadInPool : public CThreadInPool< CRef< CStdRequest > >
{
public:
    typedef CThreadInPool< CRef< CStdRequest > > TParent;

    CStdThreadInPool(TPool* pool) : TParent(pool) {}

protected:
    virtual void ProcessRequest(const CRef<CStdRequest>& req)
        { req->Process(); }

    // virtual void Init(void); // called before processing any requests
    // virtual void x_OnExit(void); // called just before exiting
};


class CStdPoolOfThreads : public CPoolOfThreads< CRef< CStdRequest > >
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
        throw CException("Attempt to insert into a full queue");
    } else if (q.empty()) {
        m_GetSem.Post();
    }
    q.push_back(data);
}


template <typename TRequest>
void CBlockingQueue<TRequest>::WaitForRoom(void) const
{
    // Make sure there's room, but don't actually consume anything
    m_PutSem.Wait();
    m_PutSem.Post();
}


template <typename TRequest>
TRequest CBlockingQueue<TRequest>::Get(void)
{
    m_GetSem.Wait();

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
        {{
            CMutexGuard guard(m_Pool->m_Mutex);
            m_Pool->m_Delta--;
        }}
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
    CMutexGuard guard(m_Pool->m_Mutex);
    m_Pool->m_ThreadCount--;
}


/////////////////////////////////////////////////////////////////////////////
//   CPoolOfThreads<>::
//

template <typename TRequest>
void CPoolOfThreads<TRequest>::Spawn(unsigned int num_threads)
{
    for (unsigned int i = 0; i < num_threads; i++)
    {
        {{
            CMutexGuard guard(m_Mutex);
            m_ThreadCount++;
        }}
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
        if (++m_Delta >= m_Threshold  &&  m_ThreadCount < m_MaxThreads) {
            // Add another thread to the pool because they're all busy.
            m_ThreadCount++;
            new_thread = true;
        }
    }}

    if (new_thread) {
        NewThread()->Run();
    }
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
