#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_EVLOOP__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_EVLOOP__HPP

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

#include <corelib/ncbistd.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <util/thread_pool.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);

class CPSGL_Processor;
class CPSGL_ProcessorQueue;
class CPSGL_Queue;
class CPSGL_QueueGuard;
class CPSGL_RequestTracker;
class CPSGL_ResultGuard;
class CPSGL_Dispatcher;


class CPSGL_TrackerMap : public CObject
{
public:
    void RegisterRequest(CPSGL_RequestTracker* tracker);
    void DeregisterRequest(const CPSGL_RequestTracker* tracker);
    
    CRef<CPSGL_RequestTracker> GetTracker(const shared_ptr<CPSG_Reply>& reply);
    CRef<CPSGL_RequestTracker> GetTracker(const shared_ptr<CPSG_ReplyItem>& item);
        
private:
    CFastMutex m_TrackerMapMutex;
    unordered_map<const CPSG_Request*, CPSGL_RequestTracker*> m_TrackerMap;
};


class CPSGL_Queue : public CObject
{
public:
    explicit
    CPSGL_Queue(const string& service_name,
                CRef<CPSGL_TrackerMap> tracker_map);
    ~CPSGL_Queue();

    CPSG_EventLoop& GetPSG_Queue()
    {
        return m_EventLoop;
    }

protected:
    friend class CPSGL_QueueGuard;
    friend class CPSGL_Dispatcher;
    
    void SendRequest(CRef<CPSGL_RequestTracker> tracker);
    bool TrySendRequest(CRef<CPSGL_RequestTracker> tracker,
                        CDeadline deadline);

    void ProcessItemCallback(EPSG_Status status,
                             const shared_ptr<CPSG_ReplyItem>& item);
    void ProcessReplyCallback(EPSG_Status status,
                              const shared_ptr<CPSG_Reply>& reply);

    

private:
    friend class CPSGL_QueueGuard;

    CPSG_EventLoop m_EventLoop;
    CRef<CPSGL_TrackerMap> m_TrackerMap;
    thread m_EventLoopThread;
};


class CPSGL_RequestTracker : public CObject
{
public:
    CPSGL_RequestTracker(CPSGL_QueueGuard& queue_guard,
                         const shared_ptr<CPSG_Request>& request,
                         const CRef<CPSGL_Processor>& processor,
                         size_t index = 0);
    ~CPSGL_RequestTracker();

    void Cancel();
    void Reset();
    
    CThreadPool_Task::EStatus GetStatus() const
    {
        return m_Status;
    }
    size_t GetIndex() const
    {
        return m_Index;
    }

    EPSG_Status GetReplyStatus() const
    {
        return m_ReplyStatus;
    }

    const shared_ptr<CPSG_Request>& GetRequest() const
    {
        return m_Request;
    }
    
    const CRef<CPSGL_Processor>& GetProcessor() const
    {
        return m_Processor;
    }
    template<class Processor>
    CRef<Processor> GetProcessor()
    {
        return Ref(dynamic_cast<Processor*>(m_Processor.GetNCPointerOrNull()));
    }
    
    
    void ProcessItemCallback(EPSG_Status status,
                             const shared_ptr<CPSG_ReplyItem>& item);
    void ProcessReplyCallback(EPSG_Status status,
                              const shared_ptr<CPSG_Reply>& reply);
    CPSGL_ResultGuard FinalizeResult();
    
protected:
    friend class CPSGL_Processor;
    friend class CPSGL_Queue;
    friend class CPSGL_QueueGuard;
    
    class CBackgroundTask;
    class CCallbackGuard;

    CFastMutex& GetTrackerMutex()
    {
        return *m_TrackerMutex;
    }
    
    CThreadPool_Task::EStatus
    BackgroundProcessItemCallback(CBackgroundTask* task,
                                  EPSG_Status status,
                                  const shared_ptr<CPSG_ReplyItem>& item);
    CThreadPool_Task::EStatus
    BackgroundProcessReplyCallback(CBackgroundTask* task);
    
    void QueueInBackground(const CRef<CBackgroundTask>& task);
    void StartProcessItemInBackground(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item);
    void StartProcessReplyInBackground();
    void CancelBackgroundTasks(); // cancel background tasks and wait
    void WaitForBackgroundTasks(); // wait until it's safe to destruct
    void MarkAsFinished(CThreadPool_Task::EStatus status);
    void MarkAsCompleted();
    void MarkAsFailed();
    void MarkAsNeedsFinalization();
    void MarkAsCanceled();
    
    CPSGL_QueueGuard& m_QueueGuard;
    shared_ptr<CPSG_Request> m_Request;
    CRef<CPSGL_Processor> m_Processor;
    size_t m_Index;
    atomic<CThreadPool_Task::EStatus> m_Status;
    EPSG_Status m_ReplyStatus;
    bool m_NeedsFinalization;
    shared_ptr<CPSG_Reply> m_Reply;

    CRef<CObjectFor<CFastMutex>> m_TrackerMutex;
    // track background tasks to prevent premature destruction
    CSemaphore m_InCallbackSemaphore; // wait for safe destructtion
    typedef set<CRef<CBackgroundTask>> TBackgroundTasks;
    TBackgroundTasks m_BackgroundTasks; // background tasks for cancellation
    unsigned m_BackgroundItemTaskCount; // background 'item' tasks
    unsigned m_InCallbackCount; // active callbacks - PSG or thread pool
};


class CPSGL_ResultGuard
{
public:
    CPSGL_ResultGuard();
    ~CPSGL_ResultGuard();

    DECLARE_OPERATOR_BOOL_REF(m_Processor);
    
    CThreadPool_Task::EStatus GetStatus() const
    {
        return m_Status;
    }

    EPSG_Status GetReplyStatus() const
    {
        return m_ReplyStatus;
    }
    size_t GetIndex() const
    {
        return m_Index;
    }

    const CRef<CPSGL_Processor>& GetProcessor()
    {
        return m_Processor;
    }
    template<class Processor>
    CRef<Processor> GetProcessor() const
    {
        return Ref(dynamic_cast<Processor*>(m_Processor.GetNCPointerOrNull()));
    }
    
    CPSGL_ResultGuard(CPSGL_ResultGuard&&);
    CPSGL_ResultGuard& operator=(CPSGL_ResultGuard&&);

    CPSGL_ResultGuard(const CPSGL_ResultGuard&) = delete;
    CPSGL_ResultGuard& operator=(const CPSGL_ResultGuard&) = delete;
    
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    
private:
    friend class CPSGL_RequestTracker;
    
    CRef<CPSGL_Processor> m_Processor;
    size_t m_Index;
    CThreadPool_Task::EStatus m_Status;
    EPSG_Status m_ReplyStatus;
};


class CPSGL_Dispatcher : public CObject
{
public:
    CPSGL_Dispatcher(const string& service_name,
                     unsigned max_pool_threads,
                     unsigned io_event_loops);
    ~CPSGL_Dispatcher();

    CThreadPool& GetThreadPool()
    {
        return *m_ThreadPool;
    }
    
    void SetRequestContext(const CRef<CRequestContext>& context);
    void SetRequestFlags(CPSG_Request::TFlags flags);
    void SetUserArgs(const SPSG_UserArgs& user_args);
    void Stop();

    void SendRequest(CRef<CPSGL_RequestTracker> tracker);
    void ForgetRequest(const CRef<CPSGL_RequestTracker>& tracker);
    
private:
    CRef<CRequestContext> m_RequestContext;
    atomic<size_t> m_NextQueue = {0};
    vector<CRef<CPSGL_Queue>> m_QueueSet;
    unique_ptr<CThreadPool> m_ThreadPool;
    CRef<CPSGL_TrackerMap> m_TrackerMap;
};


class CPSGL_QueueGuard
{
public:
    explicit
    CPSGL_QueueGuard(CRef<CPSGL_Dispatcher>& dispatcher);
    ~CPSGL_QueueGuard();

    void AddRequest(const shared_ptr<CPSG_Request>& request,
                    const CRef<CPSGL_Processor>& processor,
                    size_t index = 0);
    CThreadPool& GetThreadPool()
    {
        return m_Dispatcher->GetThreadPool();
    }
    
    void CancelAll();
    
    CPSGL_ResultGuard GetNextResult();

    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    
protected:
    friend class CPSGL_RequestTracker;

    CRef<CPSGL_RequestTracker> GetQueuedRequest();
    
    void MarkAsFinished(const CRef<CPSGL_RequestTracker>& request_processor);

    CRef<CPSGL_Dispatcher> m_Dispatcher;

    CFastMutex m_CompleteMutex;
    CSemaphore m_CompleteSemaphore;
    list<CRef<CPSGL_RequestTracker>> m_RequestsToSend;
    set<CRef<CPSGL_RequestTracker>> m_QueuedRequests;
    list<CRef<CPSGL_RequestTracker>> m_CompleteRequests;
};


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
