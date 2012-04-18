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
*   Pool of threads.
*/

#include <ncbi_pch.hpp>
#include <util/thread_pool.hpp>
#include <util/thread_pool_ctrl.hpp>
#include <util/sync_queue.hpp>
#include <util/error_codes.hpp>

#define NCBI_USE_ERRCODE_X  Util_Thread

BEGIN_NCBI_SCOPE


class CThreadPool_Guard;
class CThreadPool_ServiceThread;


/// Functor to compare tasks by priority
struct SThreadPool_TaskCompare {
    bool operator() (const CRef<CThreadPool_Task>& left,
                     const CRef<CThreadPool_Task>& right)
    {
        return left->GetPriority() < right->GetPriority();
    }
};


/// Real implementation of all ThreadPool functions
class CThreadPool_Impl : public CObject
{
public:
    typedef CThreadPool::TExclusiveFlags  TExclusiveFlags;

    /// Convert pointer to CThreadPool object into pointer to CThreadPool_Impl
    /// object. Can be done only here to avoid excessive friendship to
    /// CThreadPool class.
    static CThreadPool_Impl* s_GetImplPointer(CThreadPool* pool);

    /// Call x_SetTaskStatus() for the given task.
    /// Method introduced to avoid excessive friendship to CThreadPool_Task
    /// class.
    ///
    /// @sa CThreadPool_Task::x_SetTaskStatus()
    static void sx_SetTaskStatus(CThreadPool_Task*          task,
                                 CThreadPool_Task::EStatus  status);

    /// Call x_RequestToCancel() for the given task.
    /// Method introduced to avoid excessive friendship to CThreadPool_Task
    /// class.
    ///
    /// @sa CThreadPool_Task::x_RequestToCancel()
    static void sx_RequestToCancel(CThreadPool_Task* task);


    /// Constructor with default controller
    /// @param pool_intf
    ///   ThreadPool interface object attached to this implementation
    ///
    /// @sa CThreadPool::CThreadPool()
    CThreadPool_Impl(CThreadPool*      pool_intf,
                     unsigned int      queue_size,
                     unsigned int      max_threads,
                     unsigned int      min_threads,
                     CThread::TRunMode threads_mode = CThread::fRunDefault);

    /// Constructor with explicitly given controller
    /// @param pool_intf
    ///   ThreadPool interface object attached to this implementation
    ///
    /// @sa CThreadPool::CThreadPool()
    CThreadPool_Impl(CThreadPool*        pool_intf,
                     unsigned int        queue_size,
                     CThreadPool_Controller* controller,
                     CThread::TRunMode   threads_mode = CThread::fRunDefault);

    /// Get pointer to ThreadPool interface object
    CThreadPool* GetPoolInterface(void) const;

    /// Set destroy timeout for the pool
    ///
    /// @sa CThreadPool::SetDestroyTimeout()
    void SetDestroyTimeout(const CTimeSpan& timeout);

    /// Get destroy timeout for the pool
    ///
    /// @sa CThreadPool::GetDestroyTimeout()
    const CTimeSpan& GetDestroyTimeout(void) const;

    /// Destroy reference to this object
    /// Method is called when CThreadPool object is destroyed which means
    /// that implementation can be destroyed too if there is no references
    /// to it left.
    void DestroyReference(void);

    /// Get main pool mutex
    ///
    /// @sa CThreadPool::GetMainPoolMutex()
    CMutex& GetMainPoolMutex(void);

    /// Add task to the pool
    ///
    /// @sa CThreadPool::AddTask()
    void AddTask(CThreadPool_Task* task, const CTimeSpan* timeout);

    /// Request to cancel the task
    ///
    /// @sa CThreadPool::CancelTask()
    void CancelTask(CThreadPool_Task* task);

    /// Cancel the selected groups of tasks in the pool
    ///
    /// @sa CThreadPool::CancelTasks()
    void CancelTasks(TExclusiveFlags tasks_group);

    /// Add the task for exclusive execution in the pool
    ///
    /// @sa CThreadPool::RequestExclusiveExecution()
    void RequestExclusiveExecution(CThreadPool_Task*  task,
                                   TExclusiveFlags    flags);

    /// Launch new threads in pool
    /// @param count
    ///   Number of threads to launch
    void LaunchThreads(unsigned int count);

    /// Finish threads in pool
    /// Stop first all idle threads then stop busy threads without
    /// cancelation of currently executing tasks.
    /// @param count
    ///   Number of threads to finish
    void FinishThreads(unsigned int count);

    /// Get number of threads running in the pool
    unsigned int GetThreadsCount(void) const;

    /// Mark thread as idle or non-idle
    /// @param thread
    ///   Thread to mark
    /// @param is_idle
    ///   If thread should be marked as idle or not
    void SetThreadIdle(CThreadPool_ThreadImpl* thread, bool is_idle);

    /// Callback from working thread when it finished its Main() method
    void ThreadStopped(CThreadPool_ThreadImpl* thread);

    /// Callback when some thread changed its idleness or finished
    /// (including service thread)
    void ThreadStateChanged(void);

    /// Get next task from queue if there is one
    /// If the queue is empty then return NULL.
    CRef<CThreadPool_Task> TryGetNextTask(void);

    /// Callback from thread when it is starting to execute task
    void TaskStarting(void);

    /// Callback from thread when it has finished to execute task
    void TaskFinished(void);

    /// Get the number of tasks currently waiting in queue
    unsigned int GetQueuedTasksCount(void) const;

    /// Get the number of currently executing tasks
    unsigned int GetExecutingTasksCount(void) const;

    /// Type for storing information about exclusive task launching
    typedef pair< TExclusiveFlags,
                  CRef<CThreadPool_Task> > TExclusiveTaskInfo;

    /// Get information about next exclusive task to execute
    TExclusiveTaskInfo TryGetExclusiveTask(void);

    /// Request suspension of the pool
    /// @param flags
    ///   Parameters for necessary exclusive execution environment
    void RequestSuspend(TExclusiveFlags flags);

    /// Resume the pool operation after exclusive task execution
    void ResumeWork(void);

    /// Check if the pool is suspended for exclusive execution
    bool IsSuspended(void) const;

    /// Check if it is already allowed to execute exclusive task
    bool CanDoExclusiveTask(void) const;

    /// Abort the pool operation
    ///
    /// @sa CThreadPool::Abort()
    void Abort(const CTimeSpan* timeout);

    /// Check if the pool is already aborted
    bool IsAborted(void) const;

    /// Finish all current threads and replace them with new ones
    ///
    /// @sa CThreadPool::FlushThreads()
    void FlushThreads(CThreadPool::EFlushType flush_type);

    /// Call the CThreadPool_Controller::HandleEvent() method of the pool
    /// controller with the given event type. If ThreadPool is already aborted
    /// and controller is reset then do nothing.
    void CallController(CThreadPool_Controller::EEvent event);

    /// Schedule running of CThreadPool_Controller::HandleEvent() with eOther
    /// event type
    void CallControllerOther(void);

    /// Call the CThreadPool_Controller::GetSafeSleepTime() method of the pool
    /// controller. If ThreadPool is already aborted and controller is reset
    /// then return time period of 1 second.
    CTimeSpan GetSafeSleepTime(void) const;

    /// Mark that initialization of the interface was finished
    void SetInterfaceStarted(void);


private:
    /// Type of queue used for storing tasks
    typedef CSyncQueue< CRef<CThreadPool_Task>,
                        CSyncQueue_multiset< CRef<CThreadPool_Task>,
                                             SThreadPool_TaskCompare > >
            TQueue;
    /// Type of queue used for storing information about exclusive tasks
    typedef CSyncQueue<TExclusiveTaskInfo>                 TExclusiveQueue;
    /// Type of list of all poolled threads
    typedef set<CThreadPool_ThreadImpl*> TThreadsList;


    /// Prohibit copying and assigning
    CThreadPool_Impl(const CThreadPool_Impl&);
    CThreadPool_Impl& operator= (const CThreadPool_Impl&);

    /// Transform size of queue given in constructor to the size passed to
    /// CSyncQueue constructor.
    /// Method can be called only from constructor because it initializes
    /// value of m_IsQueueAllowed member variable.
    unsigned int x_GetQueueSize(unsigned int queue_size);

    /// Initialization of all class member variables that can be initialized
    /// outside of constructor
    /// @param pool_intf
    ///   ThreadPool interface object attached to this implementation
    /// @param controller
    ///   Controller for the pool
    void x_Init(CThreadPool*            pool_intf,
                CThreadPool_Controller* controller,
                CThread::TRunMode       threads_mode);

    /// Destructor. Will be called from CRef
    ~CThreadPool_Impl(void);

    /// Delete task from the queue
    /// If task does not exist in queue then does nothing.
    void x_RemoveTaskFromQueue(const CThreadPool_Task* task);

    /// Cancel all tasks waiting in the queue
    void x_CancelQueuedTasks(void);

    /// Cancel all currently executing tasks
    void x_CancelExecutingTasks(void);

    /// Type of some simple predicate
    ///
    /// @sa x_WaitForPredicate
    typedef bool (CThreadPool_Impl::*TWaitPredicate)(void) const;

    /// Check if new task can be added to the pool
    bool x_IsNewTaskAllowed(void) const;

    /// Check if new task can be added to the pool when queuing is disabled
    bool x_CanAddImmediateTask(void) const;

    /// Check if all threads in pool finished their work
    bool x_HasNoThreads(void) const;

    /// Wait for some predicate to be true
    /// @param wait_func
    ///   Predicate to wait for
    /// @param pool_guard
    ///   Guardian that locks main pool mutex at the time of method call and
    ///   that have to be unlocked for the time of waiting
    /// @param wait_sema
    ///   Semaphore which will be posted when predicate become true
    /// @param timeout
    ///   Maximum amount of time to wait
    /// @param timer
    ///   Timer for mesuring elapsed time. Method assumes that timer is
    ///   started at the moment from which timeout should be calculated.
    bool x_WaitForPredicate(TWaitPredicate      wait_func,
                            CThreadPool_Guard*  pool_guard,
                            CSemaphore*         wait_sema,
                            const CTimeSpan*    timeout,
                            const CStopWatch*   timer);


private:
    /// ThreadPool interface object attached to this implementation
    CThreadPool*                     m_Interface;
    /// Reference to this pool to prevent its destroying earlier than we
    /// allow it to
    CRef<CThreadPool_Impl>           m_SelfRef;
    /// Timeout to wait for all threads to finish before the ThreadPool
    /// interface object will be able to destroy
    CTimeSpan                        m_DestroyTimeout;
    /// Queue for storing tasks
    TQueue                           m_Queue;
    /// Mutex for guarding all changes in the pool, its threads and controller
    CMutex                           m_MainPoolMutex;
    /// Semaphore for waiting for available threads to process task when
    /// queuing is disabled.
    CSemaphore                       m_RoomWait;
    /// Controller managing count of threads in pool
    CRef<CThreadPool_Controller>     m_Controller;
    /// List of all idle threads
    TThreadsList                     m_IdleThreads;
    /// List of all threads currently executing some tasks
    TThreadsList                     m_WorkingThreads;
    /// Running mode of all threads
    CThread::TRunMode                m_ThreadsMode;
    /// Total number of threads
    /// Introduced for more adequate and fast reflecting to threads starting
    /// and stopping events
    CAtomicCounter                   m_ThreadsCount;
    /// Number of tasks executing now
    /// Introduced for more adequate and fast reflecting to task executing
    /// start and finish events
    CAtomicCounter                   m_ExecutingTasks;
    /// Total number of tasks acquired by pool
    /// Includes queued tasks and executing tasks. Introduced for
    /// maintaining atomicity of this number changing
    CAtomicCounter                   m_TotalTasks;
    /// Flag about working with special case:
    /// FALSE - queue_size == 0, TRUE - queue_size > 0
    bool                             m_IsQueueAllowed;
    /// If pool is already aborted or not
    volatile bool                    m_Aborted;
    /// Semaphore for waiting for threads finishing in Abort() method
    ///
    /// @sa Abort()
    CSemaphore                       m_AbortWait;
    /// If pool is suspended for exclusive task execution or not.
    /// Thread Checker can complain that access to this variable everywhere is
    /// not guarded by some mutex. But it's okay because special care is taken
    /// to make any race a matter of timing - suspend will happen properly in
    /// any case. Also everything is written with the assumption that there's
    /// no other threads (besides this very thread pool) that could call any
    /// methods here.
    volatile bool                    m_Suspended;
    /// Requested requirements for the exclusive execution environment
    volatile TExclusiveFlags         m_SuspendFlags;
    /// Flag indicating if flush of threads requested after adding exclusive
    /// task but before it is started its execution.
    volatile bool                    m_FlushRequested;
    /// Thread for execution of exclusive tasks and passing of events
    /// to the controller
    CRef<CThreadPool_ServiceThread>  m_ServiceThread;
    /// Queue for information about exclusive tasks
    TExclusiveQueue                  m_ExclusiveQueue;
};



/// Real implementation of all CThreadPool_Thread functions
class CThreadPool_ThreadImpl
{
public:
    /// Convert pointer to CThreadPool_Thread object into pointer
    /// to CThreadPool_ThreadImpl object. Can be done only here to avoid
    /// excessive friendship to CThreadPool_Thread class.
    static CThreadPool_ThreadImpl*
    s_GetImplPointer(CThreadPool_Thread* thread);

    /// Create new CThreadPool_Thread object
    /// Method introduced to avoid excessive friendship to CThreadPool_Thread
    /// class.
    ///
    /// @sa CThreadPool_Thread::CThreadPool_Thread()
    static CThreadPool_Thread* s_CreateThread(CThreadPool* pool);

    /// Constructor
    /// @param thread_intf
    ///   ThreadPool_Thread interface object attached to this implementation
    /// @param pool
    ///   Pool implementation owning this thread
    CThreadPool_ThreadImpl(CThreadPool_Thread* thread_intf,
                           CThreadPool_Impl*   pool);

    /// Destructor
    /// Called directly from CThreadPool destructor
    ~CThreadPool_ThreadImpl(void);

    /// Get ThreadPool interface object owning this thread
    ///
    /// @sa CThreadPool_Thread::GetPool()
    CThreadPool* GetPool(void) const;

    /// Request this thread to finish its operation.
    /// It renders the thread unusable and eventually ready for destruction
    /// (as soon as its current task is finished and there are no CRefs to
    /// this thread left).
    void RequestToFinish(void);

    /// If finishing of this thread is already in progress or not
    bool IsFinishing(void) const;

    /// Wake up the thread from idle state
    ///
    /// @sa x_Idle
    void WakeUp(void);

    /// Get task currently executing in the thread
    /// May be NULL if thread is idle or is in the middle of changing of
    /// current task
    ///
    /// @sa CThreadPool_Thread::GetCurrentTask()
    CRef<CThreadPool_Task> GetCurrentTask(void) const;

    /// Request to cancel current task execution
    void CancelCurrentTask(void);

    /// Implementation of thread Main() method
    ///
    /// @sa CThreadPool_Thread::Main()
    void Main(void);

    /// Implementation of threadOnExit() method
    ///
    /// @sa CThreadPool_Thread::OnExit()
    void OnExit(void);

private:
    /// Prohibit copying and assigning
    CThreadPool_ThreadImpl(const CThreadPool_ThreadImpl&);
    CThreadPool_ThreadImpl& operator= (const CThreadPool_ThreadImpl&);

    /// Suspend until the wake up signal.
    ///
    /// @sa WakeUp()
    void x_Idle(void);

    /// Mark the thread idle or non-idle
    void x_SetIdleState(bool is_idle);

    /// Do finalizing when task finished its execution
    /// @param status
    ///   Status that the task must get
    void x_TaskFinished(CThreadPool_Task::EStatus status);


    /// ThreadPool_Thread interface object attached to this implementation
    CThreadPool_Thread*          m_Interface;
    /// Pool running the thread
    CRef<CThreadPool_Impl>       m_Pool;
    /// If the thread is already asked to finish or not
    volatile bool                m_Finishing;
    /// If cancel of the currently executing task is requested or not
    volatile bool                m_CancelRequested;
    /// Idleness of the thread
    bool                         m_IsIdle;
    /// Task currently executing in the thread
    CRef<CThreadPool_Task>       m_CurrentTask;
    /// Semaphore for waking up from idle waiting
    CSemaphore                   m_IdleTrigger;
};



/// Thread used in pool for different internal needs: execution of exclusive
/// tasks and passing events to controller
class CThreadPool_ServiceThread : public CThread
{
public:
    /// Constructor
    /// @param pool
    ///   ThreadPool owning this thread
    CThreadPool_ServiceThread(CThreadPool_Impl* pool);

    /// Wake up from idle waiting or waiting of pool preparing exclusive
    /// environment
    void WakeUp(void);

    /// Request finishing of the thread
    void RequestToFinish(void);

    /// Check if this thread have already finished or not
    bool IsFinished(void);

    /// Tell the thread that controller should handle eOther event
    ///
    /// @sa CThreadPool_Controller::HandleEvent()
    void NeedCallController(void);

protected:
    /// Destructor. Will be called from CRef
    virtual ~CThreadPool_ServiceThread(void);

private:
    /// Main thread execution
    virtual void* Main(void);

    /// Do "idle" work when thread is not busy executing exclusive tasks
    void x_Idle(void);

    /// Wait until pool is ready for execution of exclusive task
    void x_WaitForPoolStop(CThreadPool_Guard* pool_guard);

    /// Pool owning this thread
    CRef<CThreadPool_Impl>  m_Pool;
    /// Semaphore for idle sleeping
    CSemaphore              m_IdleTrigger;
    /// If finishing of the thread is already requested
    volatile bool           m_Finishing;
    /// If the thread has already finished its Main() method
    volatile bool           m_Finished;
    /// Currently executing exclusive task
    CRef<CThreadPool_Task>  m_CurrentTask;
    /// Flag indicating that thread should pass eOther event to the controller
    CAtomicCounter          m_NeedCallController;
};



/// Guardian for protecting pool by locking its main mutex
class CThreadPool_Guard : private CMutexGuard
{
public:
    /// Constructor
    /// @param pool
    ///   Pool to protect
    /// @param is_active
    ///   If the mutex should be locked in constructor or not
    CThreadPool_Guard(CThreadPool_Impl* pool, bool is_active = true);

    /// Turn this guardian on
    void Guard(void);

    /// Turn this guardian off
    void Release(void);

private:
    /// Pool protected by the guardian
    CThreadPool_Impl* m_Pool;
};



/// Special task which does nothing
/// It's used in FlushThreads to force pool to wait while all old threads
/// finish their operation to start new ones.
///
/// @sa CThreadPool_Impl::FlushThreads()
class CThreadPool_EmptyTask : public CThreadPool_Task
{
public:
    /// Empty main method
    virtual EStatus Execute(void) { return eCompleted; }

    // In the absence of the following constructor, new compilers (as required
    // by the new C++ standard) may fill the object memory with zeros,
    // erasing flags set by CObject::operator new (see CXX-1808)
    CThreadPool_EmptyTask(void) {}
};



/// Check if status returned from CThreadPool_Task::Execute() is allowed
/// and change it to eCompleted value if it is invalid
static inline CThreadPool_Task::EStatus
s_ConvertTaskResult(CThreadPool_Task::EStatus status)
{
    _ASSERT(status == CThreadPool_Task::eCompleted
            ||  status == CThreadPool_Task::eFailed
            ||  status == CThreadPool_Task::eCanceled);

    if (status != CThreadPool_Task::eCompleted
        &&  status != CThreadPool_Task::eFailed
        &&  status != CThreadPool_Task::eCanceled)
    {
        ERR_POST_X(9, Critical
                      << "Wrong status returned from "
                         "CThreadPool_Task::Execute(): "
                      << status);
        status = CThreadPool_Task::eCompleted;
    }

    return status;
}



const CAtomicCounter::TValue kNeedCallController_Shift = 0x0FFFFFFF;


inline void
CThreadPool_ServiceThread::WakeUp(void)
{
    m_IdleTrigger.Post();
}

inline void
CThreadPool_ServiceThread::NeedCallController(void)
{
    if (m_NeedCallController.Add(1) > kNeedCallController_Shift + 1) {
        m_NeedCallController.Add(-1);
    }
    else {
        WakeUp();
    }
}



inline void
CThreadPool_ThreadImpl::WakeUp(void)
{
    m_IdleTrigger.Post();
}



inline CMutex&
CThreadPool_Impl::GetMainPoolMutex(void)
{
    return m_MainPoolMutex;
}



CThreadPool_Guard::CThreadPool_Guard(CThreadPool_Impl* pool, bool is_active)
    : CMutexGuard(eEmptyGuard),
      m_Pool(pool)
{
    _ASSERT(pool);

    if (is_active)
        Guard();
}

void
CThreadPool_Guard::Guard(void)
{
    CMutexGuard::Guard(m_Pool->GetMainPoolMutex());
}

void
CThreadPool_Guard::Release(void)
{
    CMutexGuard::Release();
}



inline void
CThreadPool_Impl::sx_SetTaskStatus(CThreadPool_Task*          task,
                                   CThreadPool_Task::EStatus  status)
{
    task->x_SetStatus(status);
}

inline void
CThreadPool_Impl::sx_RequestToCancel(CThreadPool_Task* task)
{
    task->x_RequestToCancel();
}

inline CThreadPool*
CThreadPool_Impl::GetPoolInterface(void) const
{
    return m_Interface;
}

inline void
CThreadPool_Impl::SetInterfaceStarted(void)
{
    m_ServiceThread->Run(CThread::fRunDetached);
}

inline bool
CThreadPool_Impl::IsAborted(void) const
{
    return m_Aborted;
}

inline bool
CThreadPool_Impl::IsSuspended(void) const
{
    return m_Suspended;
}

inline unsigned int
CThreadPool_Impl::GetThreadsCount(void) const
{
    return (unsigned int)m_ThreadsCount.Get();
}

inline unsigned int
CThreadPool_Impl::GetQueuedTasksCount(void) const
{
    return (unsigned int)m_Queue.GetSize();
}

inline unsigned int
CThreadPool_Impl::GetExecutingTasksCount(void) const
{
    return (unsigned int)m_ExecutingTasks.Get();
}

inline CTimeSpan
CThreadPool_Impl::GetSafeSleepTime(void) const
{
    // m_Controller variable can be uninitialized in only when ThreadPool
    // is already aborted
    CThreadPool_Controller* controller = m_Controller.GetNCPointerOrNull();
    if (controller  &&  ! m_Aborted) {
        return controller->GetSafeSleepTime();
    }
    else {
        return CTimeSpan(0, 0);
    }
}

inline void
CThreadPool_Impl::CallController(CThreadPool_Controller::EEvent event)
{
    CThreadPool_Controller* controller = m_Controller.GetNCPointerOrNull();
    if (controller  &&  ! m_Aborted  &&
        (! m_Suspended  ||  event == CThreadPool_Controller::eSuspend))
    {
        controller->HandleEvent(event);
    }
}

inline void
CThreadPool_Impl::CallControllerOther(void)
{
    CThreadPool_ServiceThread* thread = m_ServiceThread;
    if (thread) {
        thread->NeedCallController();
    }
}

inline void
CThreadPool_Impl::TaskStarting(void)
{
    m_ExecutingTasks.Add(1);
    // In current implementation controller operation doesn't depend on this
    // action. So we will save mutex locks for the sake of performance
    //CallControllerOther();
}

inline void
CThreadPool_Impl::TaskFinished(void)
{
    m_ExecutingTasks.Add(-1);
    m_TotalTasks.Add(-1);
    m_RoomWait.Post();
    CallControllerOther();
}

inline void
CThreadPool_Impl::ThreadStateChanged(void)
{
    if (m_Aborted) {
        if (x_HasNoThreads()) {
            m_AbortWait.Post();
        }
    }
    else if (m_Suspended) {
        if (((m_SuspendFlags & CThreadPool::fFlushThreads)
                 &&  GetThreadsCount() == 0)
            ||  (! (m_SuspendFlags & CThreadPool::fFlushThreads)
                 &&  m_WorkingThreads.size() == 0))
        {
            m_ServiceThread->WakeUp();
        }
    }
}

inline void
CThreadPool_Impl::ThreadStopped(CThreadPool_ThreadImpl* thread)
{
    m_ThreadsCount.Add(-1);

    CThreadPool_Guard guard(this);

    m_IdleThreads.erase(thread);
    m_WorkingThreads.erase(thread);

    CallControllerOther();

    ThreadStateChanged();
}

inline CRef<CThreadPool_Task>
CThreadPool_Impl::TryGetNextTask(void)
{
    if (!m_Suspended  ||  (m_SuspendFlags & CThreadPool::fExecuteQueuedTasks)) {
        TQueue::TAccessGuard guard(m_Queue);

        if (m_Queue.GetSize() != 0) {
            return m_Queue.Pop();
        }
    }

    return CRef<CThreadPool_Task>();
}

inline CThreadPool_Impl::TExclusiveTaskInfo
CThreadPool_Impl::TryGetExclusiveTask(void)
{
    TExclusiveQueue::TAccessGuard guard(m_ExclusiveQueue);

    if (m_ExclusiveQueue.GetSize() != 0) {
        CThreadPool_Impl::TExclusiveTaskInfo info = m_ExclusiveQueue.Pop();
        if (m_FlushRequested) {
            info.first |= CThreadPool::fFlushThreads;
            m_FlushRequested = false;
        }
        return info;
    }

    return TExclusiveTaskInfo(0, CRef<CThreadPool_Task>());
}

inline bool
CThreadPool_Impl::CanDoExclusiveTask(void) const
{
    if ((m_SuspendFlags & CThreadPool::fExecuteQueuedTasks)
        &&  GetQueuedTasksCount() != 0)
    {
        return false;
    }

    if ((m_SuspendFlags & CThreadPool::fFlushThreads)
        &&  GetThreadsCount() != 0)
    {
        return false;
    }

    return m_WorkingThreads.size() == 0;
}

inline void
CThreadPool_Impl::RequestSuspend(TExclusiveFlags flags)
{
    m_SuspendFlags = flags;
    m_Suspended = true;
    if (flags & CThreadPool::fCancelQueuedTasks) {
        x_CancelQueuedTasks();
    }
    if (flags & CThreadPool::fCancelExecutingTasks) {
        x_CancelExecutingTasks();
    }

    if (flags & CThreadPool::fFlushThreads) {
        FinishThreads((unsigned int)m_IdleThreads.size());
    }

    CallController(CThreadPool_Controller::eSuspend);
}

inline void
CThreadPool_Impl::ResumeWork(void)
{
    m_Suspended = false;

    CallController(CThreadPool_Controller::eResume);

    ITERATE(TThreadsList, it, m_IdleThreads) {
        (*it)->WakeUp();
    }
}



inline void
CThreadPool_Controller::x_AttachToPool(CThreadPool_Impl* pool)
{
    if (m_Pool != NULL) {
        NCBI_THROW(CThreadPoolException, eControllerBusy,
                   "Cannot attach Controller to several ThreadPools.");
    }

    m_Pool = pool;
}

inline void
CThreadPool_Controller::x_DetachFromPool(void)
{
    m_Pool = NULL;
}



CThreadPool_Task::CThreadPool_Task(unsigned int priority)
{
    x_Init(priority);
}

CThreadPool_Task::CThreadPool_Task(const CThreadPool_Task& other)
{
    x_Init(other.m_Priority);
}

void
CThreadPool_Task::x_Init(unsigned int priority)
{
    m_Pool = NULL;
    m_Priority = priority;
    // Thread Checker complains here but this code is called only from
    // constructor, so no one else can reference this task yet.
    m_Status = eIdle;
    m_CancelRequested = false;
}

CThreadPool_Task::~CThreadPool_Task(void)
{}

CThreadPool_Task&
CThreadPool_Task::operator= (const CThreadPool_Task& other)
{
    if (m_IsBusy.Get() != 0) {
        NCBI_THROW(CThreadPoolException, eTaskBusy,
                   "Cannot change task when it is already added "
                   "to ThreadPool");
    }

    CObject::operator= (other);
    // There can be race with CThreadPool_Impl::AddTask()
    // If task will be already added to queue and priority will be then
    // changed queue can crush later
    m_Priority = other.m_Priority;
    return *this;
}

void
CThreadPool_Task::OnStatusChange(EStatus /* old */)
{}

void
CThreadPool_Task::OnCancelRequested(void)
{}

inline void
CThreadPool_Task::x_SetOwner(CThreadPool_Impl* pool)
{
    if (m_IsBusy.Add(1) != 1) {
        m_IsBusy.Add(-1);
        NCBI_THROW(CThreadPoolException, eTaskBusy,
                   "Cannot add task in ThreadPool several times");
    }

    // Thread Checker complains that this races with task canceling and
    // resetting m_Pool below. But it's an thread pool usage error if
    // someone tries to call concurrently AddTask and CancelTask. With a proper
    // workflow CancelTask shouldn't be called until AddTask has returned.
    m_Pool = pool;
}

inline void
CThreadPool_Task::x_ResetOwner(void)
{
    m_Pool = NULL;
    m_IsBusy.Add(-1);
}

void
CThreadPool_Task::x_SetStatus(EStatus new_status)
{
    EStatus old_status = m_Status;
    if (old_status != new_status  &&  old_status != eCanceled) {
        // Thread Checker complains here, but all status transitions are
        // properly guarded with different mutexes and they cannot mix with
        // each other.
        m_Status = new_status;
        OnStatusChange(old_status);
    }

    if (IsFinished()) {
        // Thread Checker complains here. See comment in x_SetOwner above for
        // details.
        m_Pool = NULL;
    }
}

inline void
CThreadPool_Task::x_RequestToCancel(void)
{
    m_CancelRequested = true;

    OnCancelRequested();

    if (GetStatus() <= eQueued) {
        // This can race with calling task's Execute() method but it's okay.
        // For details see comment in CThreadPool_ThreadImpl::Main().
        x_SetStatus(eCanceled);
    }
}

void
CThreadPool_Task::RequestToCancel(void)
{
    // Protect from possible reseting of the pool variable during execution
    CThreadPool_Impl* pool = m_Pool;
    if (IsFinished()) {
        return;
    }
    else if (!pool) {
        x_RequestToCancel();
    }
    else {
        pool->CancelTask(this);
    }
}

CThreadPool*
CThreadPool_Task::GetPool(void) const
{
    // Protect from possible reseting of the pool variable during execution
    CThreadPool_Impl* pool_impl = m_Pool;
    return pool_impl? pool_impl->GetPoolInterface(): NULL;
}



CThreadPool_ServiceThread::CThreadPool_ServiceThread(CThreadPool_Impl* pool)
    : m_Pool(pool),
      m_IdleTrigger(0, kMax_Int),
      m_Finishing(false),
      m_Finished(false)
{
    _ASSERT(pool);

    m_NeedCallController.Set(kNeedCallController_Shift);
}

CThreadPool_ServiceThread::~CThreadPool_ServiceThread(void)
{}

inline bool
CThreadPool_ServiceThread::IsFinished(void)
{
    return m_Finished;
}

inline void
CThreadPool_ServiceThread::x_Idle(void)
{
    if (m_NeedCallController.Add(-1) < kNeedCallController_Shift) {
        m_NeedCallController.Add(1);
    }
    m_Pool->CallController(CThreadPool_Controller::eOther);

    CTimeSpan timeout = m_Pool->GetSafeSleepTime();
    m_IdleTrigger.TryWait(timeout.GetCompleteSeconds(),
                          timeout.GetNanoSecondsAfterSecond());
}

inline void
CThreadPool_ServiceThread::x_WaitForPoolStop(CThreadPool_Guard* pool_guard)
{
    while (! m_Pool->IsAborted()  &&  ! m_Pool->CanDoExclusiveTask()) {
        pool_guard->Release();
        m_IdleTrigger.Wait();
        pool_guard->Guard();
    }
}

inline void
CThreadPool_ServiceThread::RequestToFinish(void)
{
    m_Finishing = true;
    WakeUp();

    CThreadPool_Task* task = m_CurrentTask;
    if (task) {
        CThreadPool_Impl::sx_RequestToCancel(task);
    }
}

void*
CThreadPool_ServiceThread::Main(void)
{
    while (! m_Finishing) {
        CThreadPool_Impl::TExclusiveTaskInfo task_info =
                                              m_Pool->TryGetExclusiveTask();
        m_CurrentTask = task_info.second;

        if (m_CurrentTask.IsNull()) {
            x_Idle();
        }
        else {
            CThreadPool_Guard guard(m_Pool);

            if (m_Finishing) {
                if (! m_CurrentTask->IsCancelRequested()) {
                    CThreadPool_Impl::sx_RequestToCancel(m_CurrentTask);
                }
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                                 CThreadPool_Task::eCanceled);
                break;
            }

            m_Pool->RequestSuspend(task_info.first);
            x_WaitForPoolStop(&guard);

            if (m_Finishing) {
                if (!m_CurrentTask->IsCancelRequested()) {
                    CThreadPool_Impl::sx_RequestToCancel(m_CurrentTask);
                }
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                                 CThreadPool_Task::eCanceled);
                break;
            }

            guard.Release();

            CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                               CThreadPool_Task::eExecuting);
            try {
                CThreadPool_Task::EStatus status =
                                s_ConvertTaskResult(m_CurrentTask->Execute());
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask, status);
            }
            catch (exception& e) {
                ERR_POST_X(11, "Exception from exclusive task in ThreadPool: "
                               << e.what());
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                                   CThreadPool_Task::eFailed);
            }
            catch (...) {
                ERR_POST_X(12, "Unknown exception from exclusive task "
                               "in ThreadPool.");
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                                   CThreadPool_Task::eFailed);
            }

            guard.Guard();
            m_Pool->ResumeWork();
        }
    }

    m_Finished = true;
    m_Pool->ThreadStateChanged();

    return NULL;
}



inline CThreadPool_ThreadImpl*
CThreadPool_ThreadImpl::s_GetImplPointer(CThreadPool_Thread* thread)
{
    return thread->m_Impl;
}

inline CThreadPool_Thread*
CThreadPool_ThreadImpl::s_CreateThread(CThreadPool* pool)
{
    return new CThreadPool_Thread(pool);
}

inline
CThreadPool_ThreadImpl::CThreadPool_ThreadImpl
(
    CThreadPool_Thread*  thread_intf,
    CThreadPool_Impl*    pool
)
  : m_Interface(thread_intf),
    m_Pool(pool),
    m_Finishing(false),
    m_CancelRequested(false),
    m_IsIdle(true),
    m_IdleTrigger(0, kMax_Int)
{}

inline
CThreadPool_ThreadImpl::~CThreadPool_ThreadImpl(void)
{}

inline CThreadPool*
CThreadPool_ThreadImpl::GetPool(void) const
{
    return m_Pool->GetPoolInterface();
}

inline bool
CThreadPool_ThreadImpl::IsFinishing(void) const
{
    return m_Finishing;
}

inline CRef<CThreadPool_Task>
CThreadPool_ThreadImpl::GetCurrentTask(void) const
{
    return m_CurrentTask;
}

inline void
CThreadPool_ThreadImpl::x_SetIdleState(bool is_idle)
{
    if (m_IsIdle != is_idle) {
        m_IsIdle = is_idle;
        m_Pool->SetThreadIdle(this, is_idle);
    }
}

inline void
CThreadPool_ThreadImpl::x_TaskFinished(CThreadPool_Task::EStatus status)
{
    if (m_CurrentTask->GetStatus() == CThreadPool_Task::eExecuting) {
        CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask, status);
    }

    m_CurrentTask.Reset();
    m_Pool->TaskFinished();
}

inline void
CThreadPool_ThreadImpl::x_Idle(void)
{
    x_SetIdleState(true);

    m_IdleTrigger.Wait();
}

inline void
CThreadPool_ThreadImpl::RequestToFinish(void)
{
    m_Finishing = true;
    WakeUp();
}

inline void
CThreadPool_ThreadImpl::CancelCurrentTask(void)
{
    // Avoid resetting of the pointer during execution
    // TODO: there's possible race if before we add reference on the task
    // m_CurrentTask will be reset to NULL, last reference will be removed and
    // task will be deleted. But nobody uses CThreadPool in this way, thus
    // this assignment is safe (ThreadPool won't own the last reference to
    // the task).
    CRef<CThreadPool_Task> task = m_CurrentTask;
    if (task.NotNull()) {
        CThreadPool_Impl::sx_RequestToCancel(task);
    }
    else {
        m_CancelRequested = true;
    }
}

inline void
CThreadPool_ThreadImpl::Main(void)
{
    m_Interface->Initialize();

    while (!m_Finishing) {
        // We have to heed call to CancelCurrentTask() only after this point.
        // So we reset value of m_CancelRequested here without any mutexes.
        // If CancelCurrentTask() is called earlier or this assignment races
        // with assignment in CancelCurrentTask() then caller of
        // CancelCurrentTask() will make sure that TryGetNextTask() returns
        // NULL.
        m_CancelRequested = false;
        m_CurrentTask = m_Pool->TryGetNextTask();

        if (m_CurrentTask.IsNull()) {
            x_Idle();
        }
        else {
            if (m_CurrentTask->IsCancelRequested()  ||  m_CancelRequested) {
                // Some race can appear if task is canceled at the time
                // when it's being queued or at the time when it's being
                // unqueued
                if (! m_CurrentTask->IsCancelRequested()) {
                    CThreadPool_Impl::sx_RequestToCancel(m_CurrentTask);
                }
                CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                                 CThreadPool_Task::eCanceled);
                m_CurrentTask = NULL;
                continue;
            }

            x_SetIdleState(false);
            m_Pool->TaskStarting();

            // This can race with canceling of the task. This can result in
            // task's Execute() method called with the state of eCanceled
            // already set or cancellation being totally ignored in the task's
            // status (m_CancelRequested will be still set). Both outcomes are
            // simple timing and cancellation should be checked in the task's
            // Execute() method anyways. The worst outcome here is that task
            // can be marked as eCanceled when it's completely and successfully
            // executed. I don't think it's too bad though.
            CThreadPool_Impl::sx_SetTaskStatus(m_CurrentTask,
                                               CThreadPool_Task::eExecuting);

            try {
                CThreadPool_Task::EStatus status =
                                s_ConvertTaskResult(m_CurrentTask->Execute());
                x_TaskFinished(status);
            }
            catch (exception& e) {
                ERR_POST_X(7, "Exception from task in ThreadPool: "
                              << e.what());
                x_TaskFinished(CThreadPool_Task::eFailed);
            }
            catch (...) {
                x_TaskFinished(CThreadPool_Task::eFailed);
                throw;
            }
        }
    }
}

inline void
CThreadPool_ThreadImpl::OnExit(void)
{
    try {
        m_Interface->Finalize();
    } STD_CATCH_ALL_X(8, "Finalize")

    m_Pool->ThreadStopped(this);
}



inline CThreadPool_Impl*
CThreadPool_Impl::s_GetImplPointer(CThreadPool* pool)
{
    return pool->m_Impl;
}

inline unsigned int
CThreadPool_Impl::x_GetQueueSize(unsigned int queue_size)
{
    if (queue_size == 0) {
        // 10 is just in case, in fact when queue_size == 0 pool will always
        // check for idle threads, so tasks will never crowd in the queue
        queue_size = 10;
        m_IsQueueAllowed = false;
    }
    else {
        m_IsQueueAllowed = true;
    }

    return queue_size;
}

inline
CThreadPool_Impl::CThreadPool_Impl(CThreadPool*      pool_intf,
                                   unsigned int      queue_size,
                                   unsigned int      max_threads,
                                   unsigned int      min_threads,
                                   CThread::TRunMode threads_mode)
    : m_Queue(x_GetQueueSize(queue_size)),
      m_RoomWait(0, kMax_Int),
      m_AbortWait(0, kMax_Int)
{
    x_Init(pool_intf,
           new CThreadPool_Controller_PID(max_threads, min_threads),
           threads_mode);
}

inline
CThreadPool_Impl::CThreadPool_Impl(CThreadPool*            pool_intf,
                                   unsigned int            queue_size,
                                   CThreadPool_Controller* controller,
                                   CThread::TRunMode       threads_mode)
    : m_Queue(x_GetQueueSize(queue_size)),
      m_RoomWait(0, kMax_Int),
      m_AbortWait(0, kMax_Int)
{
    x_Init(pool_intf, controller, threads_mode);
}

void
CThreadPool_Impl::x_Init(CThreadPool*             pool_intf,
                         CThreadPool_Controller*  controller,
                         CThread::TRunMode        threads_mode)
{
    m_Interface = pool_intf;
    m_SelfRef = this;
    m_DestroyTimeout = CTimeSpan(10, 0);
    m_ThreadsCount.Set(0);
    m_ExecutingTasks.Set(0);
    m_TotalTasks.Set(0);
    m_Aborted = false;
    m_Suspended = false;
    m_ThreadsMode = (threads_mode | CThread::fRunDetached)
                     & ~CThread::fRunAllowST;

    controller->x_AttachToPool(this);
    m_Controller = controller;

    m_ServiceThread = new CThreadPool_ServiceThread(this);
}

CThreadPool_Impl::~CThreadPool_Impl(void)
{}

inline void
CThreadPool_Impl::DestroyReference(void)
{
    // Abort even if m_Aborted == true because threads can still be running
    // and we have to wait for their termination
    Abort(&m_DestroyTimeout);

    m_Interface = NULL;
    m_ServiceThread = NULL;
    m_SelfRef = NULL;
}

inline void
CThreadPool_Impl::SetDestroyTimeout(const CTimeSpan& timeout)
{
    m_DestroyTimeout = timeout;
}

inline const CTimeSpan&
CThreadPool_Impl::GetDestroyTimeout(void) const
{
    return m_DestroyTimeout;
}

void
CThreadPool_Impl::LaunchThreads(unsigned int count)
{
    if (count == 0)
        return;

    CThreadPool_Guard guard(this);

    for (unsigned int i = 0; i < count; ++i) {
        CRef<CThreadPool_Thread> thread(m_Interface->CreateThread());
        m_IdleThreads.insert(
                        CThreadPool_ThreadImpl::s_GetImplPointer(thread));
        thread->Run(m_ThreadsMode);
    }

    m_ThreadsCount.Add(count);
    CallControllerOther();
}

void
CThreadPool_Impl::FinishThreads(unsigned int count)
{
    if (count == 0)
        return;

    CThreadPool_Guard guard(this);

    // The cast is theoretically extraneous, but Sun's WorkShop
    // compiler otherwise calls the wrong versions of begin() and
    // end() and refuses to convert the resulting iterators.
    REVERSE_ITERATE(TThreadsList, it,
                    static_cast<const TThreadsList&>(m_IdleThreads))
    {
        // Maybe in case of several quick consecutive calls we should favor
        // the willing to finish several threads.
        //if ((*it)->IsFinishing())
        //    continue;

        (*it)->RequestToFinish();
        --count;
        if (count == 0)
            break;
    }

    REVERSE_ITERATE(TThreadsList, it,
                    static_cast<const TThreadsList&>(m_WorkingThreads))
    {
        if (count == 0)
            break;

        (*it)->RequestToFinish();
        --count;
    }
}

void
CThreadPool_Impl::SetThreadIdle(CThreadPool_ThreadImpl* thread, bool is_idle)
{
    CThreadPool_Guard guard(this);

    if (is_idle  &&  !m_Suspended  &&  m_Queue.GetSize() != 0) {
        thread->WakeUp();
        return;
    }

    TThreadsList* to_del;
    TThreadsList* to_ins;
    if (is_idle) {
        to_del = &m_WorkingThreads;
        to_ins = &m_IdleThreads;
    }
    else {
        to_del = &m_IdleThreads;
        to_ins = &m_WorkingThreads;
    }

    TThreadsList::iterator it = to_del->find(thread);
    if (it != to_del->end()) {
        to_del->erase(it);
    }
    to_ins->insert(thread);

    if (is_idle  &&  m_Suspended
        &&  (m_SuspendFlags & CThreadPool::fFlushThreads))
    {
        thread->RequestToFinish();
    }

    ThreadStateChanged();
}

inline bool
CThreadPool_Impl::x_IsNewTaskAllowed(void) const
{
    return !m_Aborted
            &&  (!m_Suspended
                  ||  !(m_SuspendFlags & CThreadPool::fDoNotAllowNewTasks));
}

bool
CThreadPool_Impl::x_CanAddImmediateTask(void) const
{
    // If pool aborts at some point in waiting it has to stop waiting
    // immediately
    return !x_IsNewTaskAllowed()
           ||  (!m_Suspended  &&  (unsigned int)m_TotalTasks.Get()
                                              < m_Controller->GetMaxThreads());
}

bool
CThreadPool_Impl::x_HasNoThreads(void) const
{
    CThreadPool_ServiceThread* thread = m_ServiceThread.GetNCPointerOrNull();
    return m_IdleThreads.size() + m_WorkingThreads.size() == 0
           &&  (! thread  ||  thread->IsFinished());
}

bool
CThreadPool_Impl::x_WaitForPredicate(TWaitPredicate      wait_func,
                                     CThreadPool_Guard*  pool_guard,
                                     CSemaphore*         wait_sema,
                                     const CTimeSpan*    timeout,
                                     const CStopWatch*   timer)
{
    while (!(this->*wait_func)()) {
        pool_guard->Release();

        if (timeout) {
            CTimeSpan next_tm = CTimeSpan(timeout->GetAsDouble()
                                              - timer->Elapsed());
            if (next_tm.GetSign() == eNegative) {
                return false;
            }

            if (! wait_sema->TryWait(next_tm.GetCompleteSeconds(),
                                     next_tm.GetNanoSecondsAfterSecond()))
            {
                return false;
            }
        }
        else {
            wait_sema->Wait();
        }

        pool_guard->Guard();
    }

    return true;
}

/// Throw an exception with standard message when AddTask() is called
/// but ThreadPool is aborted or do not allow new tasks
NCBI_NORETURN
static inline void
ThrowAddProhibited(void)
{
    NCBI_THROW(CThreadPoolException, eProhibited,
               "Adding of new tasks is prohibited");
}

 inline void
CThreadPool_Impl::AddTask(CThreadPool_Task* task, const CTimeSpan* timeout)
{
    _ASSERT(task);

    // To be sure that if simple new operator was passed as argument the task
    // will still be referenced even if some exception happen in this method
    CRef<CThreadPool_Task> task_ref(task);

    if (!x_IsNewTaskAllowed()) {
        ThrowAddProhibited();
    }

    CThreadPool_Guard guard(this, false);
    auto_ptr<CTimeSpan> real_timeout;

    if (!m_IsQueueAllowed) {
        guard.Guard();

        CStopWatch timer(CStopWatch::eStart);
        if (! x_WaitForPredicate(&CThreadPool_Impl::x_CanAddImmediateTask,
                                 &guard, &m_RoomWait, timeout, &timer))
        {
            NCBI_THROW(CSyncQueueException, eNoRoom,
                       "Cannot add task - all threads are busy");
        }

        if (!x_IsNewTaskAllowed()) {
            ThrowAddProhibited();
        }

        if (timeout) {
            real_timeout.reset(new CTimeSpan(timeout->GetAsDouble()
                                                  - timer.Elapsed()));
        }
    }

    task->x_SetOwner(this);
    task->x_SetStatus(CThreadPool_Task::eQueued);
    try {
        // Pushing to queue must be out of mutex to be able to wait
        // for available space.
        m_Queue.Push(Ref(task), real_timeout.get());
    }
    catch (...) {
        task->x_SetStatus(CThreadPool_Task::eIdle);
        task->x_ResetOwner();
        throw;
    }

    if (m_IsQueueAllowed) {
        guard.Guard();
    }

    // Check if someone aborted the pool or suspended it with cacelation of
    // queued tasks after we added this task to the queue but before we were
    // able to acquire the mutex
    CThreadPool::TExclusiveFlags check_flags
        = CThreadPool::fDoNotAllowNewTasks + CThreadPool::fCancelQueuedTasks;
    if (m_Aborted  ||  (m_Suspended
                        &&  (m_SuspendFlags & check_flags)  == check_flags))
    {
        if (m_Queue.GetSize() != 0) {
            x_CancelQueuedTasks();
        }
        return;
    }

    unsigned int cnt_req = (unsigned int)m_TotalTasks.Add(1);

    if (!m_IsQueueAllowed  &&  cnt_req > GetThreadsCount()) {
        LaunchThreads(cnt_req - GetThreadsCount());
    }

    if (! m_Suspended) {
        int count = GetQueuedTasksCount();
        ITERATE(TThreadsList, it, m_IdleThreads) {
            if (! (*it)->IsFinishing()) {
                (*it)->WakeUp();
                --count;
                if (count == 0)
                    break;
            }
        }
    }

    CallControllerOther();
}

inline void
CThreadPool_Impl::x_RemoveTaskFromQueue(const CThreadPool_Task* task)
{
    TQueue::TAccessGuard q_guard(m_Queue);

    TQueue::TAccessGuard::TIterator it = q_guard.Begin();
    while (it != q_guard.End()  &&  *it != task) {
        ++it;
    }

    if (it != q_guard.End()) {
        q_guard.Erase(it);
    }
}

void
CThreadPool_Impl::RequestExclusiveExecution(CThreadPool_Task*  task,
                                            TExclusiveFlags    flags)
{
    _ASSERT(task);

    // To be sure that if simple new operator was passed as argument the task
    // will still be referenced even if some exception happen in this method
    CRef<CThreadPool_Task> task_ref(task);

    if (m_Aborted) {
        NCBI_THROW(CThreadPoolException, eProhibited,
                   "Cannot add exclusive task when ThreadPool is aborted");
    }

    task->x_SetOwner(this);
    task->x_SetStatus(CThreadPool_Task::eQueued);
    m_ExclusiveQueue.Push(TExclusiveTaskInfo(flags, Ref(task)));

    CThreadPool_ServiceThread* thread = m_ServiceThread;
    if (thread) {
        thread->WakeUp();
    }
}

void
CThreadPool_Impl::CancelTask(CThreadPool_Task* task)
{
    _ASSERT(task);

    if (task->IsFinished()) {
        return;
    }
    // Some race can happen here if the task is being queued now
    if (task->GetStatus() == CThreadPool_Task::eIdle) {
        task->x_RequestToCancel();
        return;
    }

    CThreadPool* task_pool = task->GetPool();
    if (task_pool != m_Interface) {
        if (!task_pool) {
            // Task have just finished - we can do nothing
            return;
        }

        NCBI_THROW(CThreadPoolException, eInvalid,
                   "Cannot cancel task execution "
                   "if it is inserted in another ThreadPool");
    }

    task->x_RequestToCancel();
    x_RemoveTaskFromQueue(task);

    CallControllerOther();
}

inline void
CThreadPool_Impl::CancelTasks(TExclusiveFlags tasks_group)
{
    _ASSERT( (tasks_group & (CThreadPool::fCancelExecutingTasks
                             + CThreadPool::fCancelQueuedTasks))
                  == tasks_group
             &&  tasks_group != 0);

    if (tasks_group & CThreadPool::fCancelQueuedTasks) {
        x_CancelQueuedTasks();
    }
    if (tasks_group & CThreadPool::fCancelExecutingTasks) {
        x_CancelExecutingTasks();
    }

    CallControllerOther();
}

void
CThreadPool_Impl::x_CancelExecutingTasks(void)
{
    CThreadPool_Guard guard(this);

    ITERATE(TThreadsList, it, m_WorkingThreads) {
        (*it)->CancelCurrentTask();
    }

    // CThreadPool_ThreadImpl::Main() acts not under guard, so we cannot be
    // sure that it doesn't have already task to execute before it marked
    // itself as working
    ITERATE(TThreadsList, it, m_IdleThreads) {
        (*it)->CancelCurrentTask();
    }
}

void
CThreadPool_Impl::x_CancelQueuedTasks(void)
{
    TQueue::TAccessGuard q_guard(m_Queue);

    for (TQueue::TAccessGuard::TIterator it = q_guard.Begin();
                                         it != q_guard.End(); ++it)
    {
        it->GetNCPointer()->x_RequestToCancel();
    }

    m_Queue.Clear();
}

inline void
CThreadPool_Impl::FlushThreads(CThreadPool::EFlushType flush_type)
{
    CThreadPool_Guard guard(this);

    if (m_Aborted) {
        NCBI_THROW(CThreadPoolException, eProhibited,
                   "Cannot flush threads when ThreadPool aborted");
    }

    if (flush_type == CThreadPool::eStartImmediately
        ||  (flush_type == CThreadPool::eWaitToFinish  &&  m_Suspended))
    {
        FinishThreads(GetThreadsCount());
    }
    else if (flush_type == CThreadPool::eWaitToFinish) {
        bool need_add = true;

        {{
            // To avoid races with TryGetExclusiveTask() we need to put
            // guard here
            TExclusiveQueue::TAccessGuard q_guard(m_ExclusiveQueue);

            if (m_ExclusiveQueue.GetSize() != 0) {
                m_FlushRequested = true;
                need_add = false;
            }
        }}

        if (need_add) {
            RequestExclusiveExecution(new CThreadPool_EmptyTask(),
                                      CThreadPool::fFlushThreads);
        }
    }
}

inline void
CThreadPool_Impl::Abort(const CTimeSpan* timeout)
{
    CThreadPool_Guard guard(this);

    // Method can be called several times in a row and every time we need
    // to wait for threads to finish operation
    m_Aborted = true;

    x_CancelQueuedTasks();
    x_CancelExecutingTasks();

    {{
        TExclusiveQueue::TAccessGuard q_guard(m_ExclusiveQueue);

        for (TExclusiveQueue::TAccessGuard::TIterator it = q_guard.Begin();
                                                it != q_guard.End(); ++it)
        {
            it->second->x_RequestToCancel();
        }

        m_ExclusiveQueue.Clear();
    }}

    if (m_ServiceThread.NotNull()) {
        m_ServiceThread->RequestToFinish();
    }

    FinishThreads(GetThreadsCount());

    if (m_Controller.NotNull()) {
        m_Controller->x_DetachFromPool();
    }

    CStopWatch timer(CStopWatch::eStart);
    x_WaitForPredicate(&CThreadPool_Impl::x_HasNoThreads,
                       &guard, &m_AbortWait, timeout, &timer);
    m_AbortWait.Post();

    // This assigning can destroy the controller. If some threads are not
    // finished yet and at this very moment will call controller it can crash.
    //m_Controller = NULL;
}



CThreadPool_Controller::CThreadPool_Controller(unsigned int max_threads,
                                               unsigned int min_threads)
    : m_Pool(NULL),
      m_MinThreads(min_threads),
      m_MaxThreads(max_threads),
      m_InHandleEvent(false)
{
    if (max_threads < min_threads  ||  max_threads == 0) {
        NCBI_THROW_FMT(CThreadPoolException, eInvalid,
                       "Invalid numbers of min and max number of threads:"
                       " min=" << min_threads << ", max=" << max_threads);
    }
}

CThreadPool_Controller::~CThreadPool_Controller(void)
{}

CThreadPool*
CThreadPool_Controller::GetPool(void) const
{
    // Avoid changing of pointer during method execution
    CThreadPool_Impl* pool = m_Pool;
    return pool? pool->GetPoolInterface(): NULL;
}

CMutex&
CThreadPool_Controller::GetMainPoolMutex(CThreadPool* pool) const
{
    CThreadPool_Impl* impl = CThreadPool_Impl::s_GetImplPointer(pool);
    if (!impl) {
        NCBI_THROW(CThreadPoolException, eInactive,
                   "Cannot do active work when not attached "
                   "to some ThreadPool");
    }
    return impl->GetMainPoolMutex();
}

void
CThreadPool_Controller::EnsureLimits(void)
{
    CThreadPool_Impl* pool = m_Pool;

    if (! pool)
        return;

    Uint4 count = pool->GetThreadsCount();
    if (count > m_MaxThreads) {
        pool->FinishThreads(count - m_MaxThreads);
    }
    if (count < m_MinThreads) {
        pool->LaunchThreads(m_MinThreads - count);
    }
}

void
CThreadPool_Controller::SetMinThreads(unsigned int min_threads)
{
    CThreadPool_Guard guard(m_Pool, false);
    if (m_Pool)
        guard.Guard();

    m_MinThreads = min_threads;

    EnsureLimits();
}

void
CThreadPool_Controller::SetMaxThreads(unsigned int max_threads)
{
    CThreadPool_Guard guard(m_Pool, false);
    if (m_Pool)
        guard.Guard();

    m_MaxThreads = max_threads;

    EnsureLimits();
}

void
CThreadPool_Controller::SetThreadsCount(unsigned int count)
{
    if (count > GetMaxThreads())
        count = GetMaxThreads();
    if (count < GetMinThreads())
        count = GetMinThreads();

    CThreadPool_Impl* pool = m_Pool;

    unsigned int now_cnt = pool->GetThreadsCount();
    if (count > now_cnt) {
        pool->LaunchThreads(count - now_cnt);
    }
    else if (count < now_cnt) {
        pool->FinishThreads(now_cnt - count);
    }
}

void
CThreadPool_Controller::HandleEvent(EEvent event)
{
    CThreadPool_Impl* pool = m_Pool;
    if (! pool)
        return;

    CThreadPool_Guard guard(pool);

    if (m_InHandleEvent  ||  pool->IsAborted()  ||  pool->IsSuspended())
        return;

    m_InHandleEvent = true;

    try {
        OnEvent(event);
        m_InHandleEvent = false;
    }
    catch (...) {
        m_InHandleEvent = false;
        throw;
    }
}

CTimeSpan
CThreadPool_Controller::GetSafeSleepTime(void) const
{
    if (m_Pool) {
        return CTimeSpan(1, 0);
    }
    else {
        return CTimeSpan(0, 0);
    }
}



CThreadPool_Thread::CThreadPool_Thread(CThreadPool* pool)
{
    _ASSERT(pool);

    m_Impl = new CThreadPool_ThreadImpl(this,
                                    CThreadPool_Impl::s_GetImplPointer(pool));
}

CThreadPool_Thread::~CThreadPool_Thread(void)
{
    delete m_Impl;
}

void
CThreadPool_Thread::Initialize(void)
{}

void
CThreadPool_Thread::Finalize(void)
{}

CThreadPool*
CThreadPool_Thread::GetPool(void) const
{
    return m_Impl->GetPool();
}

CRef<CThreadPool_Task>
CThreadPool_Thread::GetCurrentTask(void) const
{
    return m_Impl->GetCurrentTask();
}

void*
CThreadPool_Thread::Main(void)
{
    m_Impl->Main();
    return NULL;
}

void
CThreadPool_Thread::OnExit(void)
{
    m_Impl->OnExit();
}



CThreadPool::CThreadPool(unsigned int      queue_size,
                         unsigned int      max_threads,
                         unsigned int      min_threads,
                         CThread::TRunMode threads_mode)
{
    m_Impl = new CThreadPool_Impl(this, queue_size, max_threads, min_threads,
                                  threads_mode);
    m_Impl->SetInterfaceStarted();
}

CThreadPool::CThreadPool(unsigned int            queue_size,
                         CThreadPool_Controller* controller,
                         CThread::TRunMode       threads_mode)
{
    m_Impl = new CThreadPool_Impl(this, queue_size, controller, threads_mode);
    m_Impl->SetInterfaceStarted();
}

CThreadPool::~CThreadPool(void)
{
    m_Impl->DestroyReference();
}

CMutex&
CThreadPool::GetMainPoolMutex(void)
{
    return m_Impl->GetMainPoolMutex();
}

CThreadPool_Thread*
CThreadPool::CreateThread(void)
{
    return CThreadPool_ThreadImpl::s_CreateThread(this);
}

void
CThreadPool::AddTask(CThreadPool_Task* task, const CTimeSpan* timeout)
{
    m_Impl->AddTask(task, timeout);
}

void
CThreadPool::CancelTask(CThreadPool_Task* task)
{
    m_Impl->CancelTask(task);
}

void
CThreadPool::Abort(const CTimeSpan* timeout)
{
    m_Impl->Abort(timeout);
}

bool
CThreadPool::IsAborted(void) const
{
    return m_Impl->IsAborted();
}

void
CThreadPool::SetDestroyTimeout(const CTimeSpan& timeout)
{
    m_Impl->SetDestroyTimeout(timeout);
}

const CTimeSpan&
CThreadPool::GetDestroyTimeout(void) const
{
    return m_Impl->GetDestroyTimeout();
}

void
CThreadPool::RequestExclusiveExecution(CThreadPool_Task*  task,
                                       TExclusiveFlags    flags)
{
    m_Impl->RequestExclusiveExecution(task, flags);
}

void
CThreadPool::CancelTasks(TExclusiveFlags tasks_group)
{
    m_Impl->CancelTasks(tasks_group);
}

void
CThreadPool::FlushThreads(EFlushType flush_type)
{
    m_Impl->FlushThreads(flush_type);
}

unsigned int
CThreadPool::GetThreadsCount(void) const
{
    return m_Impl->GetThreadsCount();
}

unsigned int
CThreadPool::GetQueuedTasksCount(void) const
{
    return m_Impl->GetQueuedTasksCount();
}

unsigned int
CThreadPool::GetExecutingTasksCount(void) const
{
    return m_Impl->GetExecutingTasksCount();
}



END_NCBI_SCOPE
