#ifndef CONNECT__IMPL__THREAD_POOL_FOR_SERVER__HPP
#define CONNECT__IMPL__THREAD_POOL_FOR_SERVER__HPP

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
* Author:  Pavel Ivanov
*
* File Description:
*/


#include <util/thread_pool_old.hpp>


/** @addtogroup ThreadedPools
 *
 * @{
 */

BEGIN_NCBI_SCOPE


class CQueueItemBase_ForServer : public CObject {
public:
    typedef CQueueItemBase::EStatus EStatus;

    CQueueItemBase_ForServer(void) 
        : m_Status(CQueueItemBase::ePending)
    {}
    
    const EStatus&   GetStatus(void) const       { return m_Status; }

    void MarkAsComplete(void)        { x_SetStatus(CQueueItemBase::eComplete); }
    void MarkAsForciblyCaught(void)  { x_SetStatus(CQueueItemBase::eForciblyCaught); }

protected:
    EStatus   m_Status;

    virtual void x_SetStatus(EStatus new_status)
    { m_Status = new_status; }
};


class CBlockingQueue_ForServer
{
public:
    class CQueueItem;
    typedef CRef<CQueueItem> TItemHandle;
    typedef CRef<CStdRequest> TRequest;

    /// It may be desirable to store handles obtained from GetHandle() in
    /// instances of CCompletingHandle to ensure that they are marked as
    /// complete when all is said and done, even in the face of exceptions.
    class CCompletingHandle : public TItemHandle
    {
    public:
        CCompletingHandle(const TItemHandle& h)
        : TItemHandle(h)
        {}

        ~CCompletingHandle()
        {
            if (this->NotEmpty()) {
                this->GetObject().MarkAsComplete();
            }
        }
    };

    /// Constructor
    CBlockingQueue_ForServer(void)
#ifndef NCBI_HAVE_CONDITIONAL_VARIABLE
        : m_GetSem(0,1)
#endif
    {}

    /// Put a request into the queue.  If the queue remains full for
    /// the duration of the (optional) timeout, throw an exception.
    ///
    /// @param request
    ///   Request
    TItemHandle Put(const TRequest& request);

    /// Get the first available request from the queue, and return a
    /// handle to it.
    /// Blocks politely if empty.
    TItemHandle  GetHandle(void);

    class CQueueItem : public CQueueItemBase_ForServer
    {
    public:
        // typedef CBlockingQueue<TRequest> TQueue;
        CQueueItem(TRequest request)
            : m_Request(request)
        {}

        const TRequest& GetRequest(void) const { return m_Request; } 

    protected:
        // Specialized for CRef<CStdRequest> in thread_pool.cpp
        void x_SetStatus(EStatus new_status)
        {
            EStatus old_status = GetStatus();
            CQueueItemBase_ForServer::x_SetStatus(new_status);
            m_Request->OnStatusChange(old_status, new_status);
        }
        
    private:
        friend class CBlockingQueue_ForServer;

        TRequest  m_Request;
    };
    
protected:
    /// The type of the queue
    typedef deque<TItemHandle> TRealQueue;

    // Derived classes should take care to use these members properly.
    TRealQueue m_Queue;     ///< The queue
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
    CConditionVariable  m_GetCond;
#else
    CSemaphore          m_GetSem;    ///< Raised if the queue contains data
#endif
    mutable CMutex      m_Mutex;     ///< Guards access to queue

private:
    /// forbidden
    CBlockingQueue_ForServer(const CBlockingQueue_ForServer&);
    CBlockingQueue_ForServer& operator=(const CBlockingQueue_ForServer&);
};


class CPoolOfThreads_ForServer;

class CThreadInPool_ForServer : public CThread
{
public:
    typedef CPoolOfThreads_ForServer TPool;
    typedef CBlockingQueue_ForServer::TItemHandle TItemHandle;
    typedef CBlockingQueue_ForServer::CCompletingHandle TCompletingHandle;
    typedef CBlockingQueue_ForServer::TRequest TRequest;

    /// Constructor
    ///
    /// @param pool
    ///   A pool where this thead is placed
    /// @param mode
    ///   A running mode of this thread
    CThreadInPool_ForServer(TPool* pool) 
        : m_Pool(pool)
    {}

protected:
    /// Destructor
    virtual ~CThreadInPool_ForServer(void)
    {}

    /// Process a request.
    /// It is called from Main() for each request this thread handles
    ///
    /// @param
    ///   A request for processing
    void ProcessRequest(TItemHandle handle);

    /// Older interface (still delegated to by default)
    void ProcessRequest(const TRequest& req)
    { req.GetNCPointerOrNull()->Process(); }

private:
    // to prevent overriding; inherited from CThread
    virtual void* Main(void);
    virtual void OnExit(void);

    void x_HandleOneRequest(bool catch_all);
    void x_UnregisterThread(void);

    class CAutoUnregGuard
    {
    public:
        typedef CThreadInPool_ForServer TThread;
        CAutoUnregGuard(TThread* thr);
        ~CAutoUnregGuard(void);

    private:
        TThread* m_Thread;
    };

    friend class CAutoUnregGuard;


    TPool*   m_Pool;     ///< The pool that holds this thread
};


class CPoolOfThreads_ForServer
{
public:
    typedef CThreadInPool_ForServer TThread;

    typedef CBlockingQueue_ForServer TQueue;
    typedef TQueue::TItemHandle TItemHandle;
    typedef TQueue::TRequest TRequest;

    /// Constructor
    ///
    /// @param max_threads
    ///   The maximum number of threads that this pool can run
    CPoolOfThreads_ForServer(unsigned int max_threads, const string& thr_suffix);

    /// Destructor
    virtual ~CPoolOfThreads_ForServer(void);

    /// Start processing threads
    ///
    /// @param num_threads
    ///    The number of threads to start
    void Spawn(unsigned int num_threads);

    /// Put a request in the queue with a given priority
    ///
    /// @param request
    ///   A request
    void AcceptRequest(const TRequest& request);
    TItemHandle GetHandle(void);

    /// Causes all threads in the pool to exit cleanly after finishing
    /// all pending requests, optionally waiting for them to die.
    ///
    /// @param wait
    ///    If true will wait until all thread in the pool finish their job
    void KillAllThreads(bool wait);

private:
    friend class CThreadInPool_ForServer;

    /// Create a new thread
    TThread* NewThread(void)
    { return new CThreadInPool_ForServer(this); }

    /// Register a thread. It is called by TThread::Main.
    /// It should detach a thread if not tracking
    ///
    /// @param thread
    ///   A thread to register
    void Register(TThread& thread);

    /// Unregister a thread
    ///
    /// @param thread
    ///   A thread to unregister
    void UnRegister(TThread&);


    typedef CAtomicCounter::TValue TACValue;

    /// The maximum number of threads the pool can hold
    volatile TACValue       m_MaxThreads;
    /// The current number of threads in the pool
    CAtomicCounter          m_ThreadCount;
    /// The guard for m_MaxThreads and m_MaxUrgentThreads
    CMutex                  m_Mutex;
    CAtomicCounter          m_PutQueueNum;
    CAtomicCounter          m_GetQueueNum;
    TQueue**                m_Queues;
    string                  m_ThrSuffix;

    typedef list<CRef<TThread> > TThreads;
    TThreads                m_Threads;
    bool                    m_KilledAll;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT__IMPL__THREAD_POOL_FOR_SERVER__HPP */
