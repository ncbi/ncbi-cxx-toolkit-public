#ifndef UTIL__THREAD_POOL__HPP
#define UTIL__THREAD_POOL__HPP

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
 * Author: Denis Vakatov, Pavel Ivanov
 *
 */


/// @file thread_pool.hpp
/// Pool of generic task-executing threads.
///
///  CThreadPool         -- base implementation of pool of threads
///  CThreadPool_Thread  -- base implementation of thread in pool of threads
///  CThreadPool_Task    -- abstract task for executing in thread pool
///  CThreadPool_Controller
///                      -- abstract class to control the number of threads
///                         in pool of threads


// Old interfaces and classes
#include <util/thread_pool_old.hpp>



/** @addtogroup ThreadedPools
 *
 * @{
 */

BEGIN_NCBI_SCOPE


class CThreadPool;
class CThreadPool_Impl;
class CThreadPool_Task;
class CThreadPool_Controller;
class CThreadPool_Thread;
class CThreadPool_ThreadImpl;
class CThreadPoolException;



/// Abstract class for representing single task executing in pool of threads
/// To use this class in application you should inherit your own class from it
/// and define method Execute() - the main method where all task logic
/// executes.
/// Every single task can be executed (or canceled before execution)
/// only once and only in one pool.

class NCBI_XUTIL_EXPORT CThreadPool_Task : public CObject
{
public:
    /// Status of the task
    enum EStatus {
        eIdle,          ///< has not been placed in queue yet
        eQueued,        ///< in the queue, awaiting execution
        eExecuting,     ///< being executed
        eCompleted,     ///< executed successfully
        eFailed,        ///< failure during execution
        eCanceled       ///< canceled - possible only if canceled before
                        ///< processing was started or if method Execute()
                        ///< returns result eCanceled
    };

    /// Constructor
    /// @param priority
    ///   Priority of the task - the smaller the priority,
    ///   the sooner the task will execute
    CThreadPool_Task(unsigned int priority = 0);

    /// Do the actual job. Called by whichever thread handles this task.
    /// @return
    ///   Result of task execution (the status will be set accordingly)
    /// @note
    ///   Only 3 values are allowed:  eCompleted, eFailed, eCanceled.
    virtual EStatus Execute(void) = 0;


    /// Cancel the task.
    /// Equivalent to calling CThreadPool::CancelTask(task).
    /// @note
    ///   If the task is executing it may not be canceled right away. It is
    ///   responsibility of method Execute() implementation to check
    ///   value of IsCancelRequested() periodically and finish its execution
    ///   when this value is TRUE.
    /// @note
    ///   If the task has already finished its execution then do nothing.
    void RequestToCancel(void);

    /// Check if cancellation of the task was requested
    bool IsCancelRequested(void) const;

    /// Get status of the task
    EStatus GetStatus(void) const;

    /// Check if task execution has been already finished
    /// (successfully or not)
    bool IsFinished(void) const;

    /// Get priority of the task
    unsigned int GetPriority(void) const;

protected:
    /// Callback to notify on changes in the task status
    /// @param old
    ///   Task status before the change. Current value can be obtained from
    ///   GetStatus().
    ///
    /// @note
    ///   Status eQueued is set before task is actually pushed to the queue.
    ///   After eQueued status eIdle can appear if
    ///   insertion into the queue failed because of timeout.
    ///   Status eCanceled will be set only in 2 cases:
    ///   - if task is not executing yet and RequestToCancel() called, or
    ///   - if method Execute() returned eCanceled.
    ///   To check if task cancellation is requested during its execution
    ///   use methods OnCancelRequested() or IsCancelRequested().
    /// @sa OnCancelRequested(), IsCancelRequested(), GetStatus()
    virtual void OnStatusChange(EStatus old);

    /// Callback to notify when cancellation of the task is requested
    /// @sa  OnStatusChange()
    virtual void OnCancelRequested(void);

    /// Copy ctor
    CThreadPool_Task(const CThreadPool_Task& other);

    /// Assignment
    /// @note
    ///   There is a possible race condition if request is assigned
    ///   and added to the pool at the same time by different threads.
    ///   It is a responsibility of the derived class to avoid this race.
    CThreadPool_Task& operator= (const CThreadPool_Task& other);

    /// The thread pool which accepted this task for execution
    /// @sa CThreadPool::AddTask()
    CThreadPool* GetPool(void) const;

    /// Destructor. Will be called from CRef.
    virtual ~CThreadPool_Task(void);

private:
    friend class CThreadPool_Impl;

    /// Init all members in constructor
    /// @param priority
    ///   Priority of the task
    void x_Init(unsigned int priority);

    /// Set pool as owner of this task.
    void x_SetOwner(CThreadPool_Impl* pool);

    /// Detach task from the pool (if insertion into the pool has failed).
    void x_ResetOwner(void);

    /// Set task status
    void x_SetStatus(EStatus new_status);

    /// Internal canceling of the task
    void x_RequestToCancel(void);

    /// Flag indicating that the task is already added to some pool
    CAtomicCounter_WithAutoInit m_IsBusy;
    /// Pool owning this task
    CThreadPool_Impl*  m_Pool;
    /// Priority of the task
    unsigned int       m_Priority;
    /// Status of the task
    EStatus            m_Status;
    /// Flag indicating if cancellation of the task was already requested
    volatile bool      m_CancelRequested;
};



/// Main class implementing functionality of pool of threads.
///
/// This class can be safely used as a member of some other class or as
/// a scoped variable. In the destructor it will wait for all its threads
/// to finish with the timeout set by CThreadPool::SetDestroyTimeout().
/// If this timeout is not enough for threads to terminate CThreadPool
/// will be destroyed but all threads will finish later without any
/// "segmentation fault" errors because CThreadPool_Impl object will remain
/// in memory until last thread is finished. So if this CThreadPool object
/// is destroyed at the end of the application and it will fail to finish
/// all threads in destructor then all memory allocated by CThreadPool_Impl
/// can be shown as leakage in different tools like valgrind. To avoid these
/// leakages or for some other reasons to make sure that ThreadPool finished
/// all its operations before the destructor you can call method Abort() at
/// any place in your application.

class NCBI_XUTIL_EXPORT CThreadPool
{
public:
    /// Constructor
    /// @param queue_size
    ///   Maximum number of tasks waiting in the queue. If 0 then tasks
    ///   cannot be queued and are added only when there are threads
    ///   to process them. If greater than 0 and there will be attempt to add
    ///   new task over this maximum then method AddTask() will wait for the
    ///   given timeout for some empty space in the queue.
    /// @param max_threads
    ///   Maximum number of threads allowed to be launched in the pool.
    ///   Value cannot be less than min_threads or equal to 0.
    /// @param min_threads
    ///   Minimum number of threads that have to be launched even
    ///   if there are no tasks added. Value cannot be greater
    ///   than max_threads.
    /// @param threads_mode
    ///   Running mode of all threads in thread pool. Values fRunDetached and
    ///   fRunAllowST are ignored.
    ///
    /// @sa AddTask()
    CThreadPool(unsigned int      queue_size,
                unsigned int      max_threads,
                unsigned int      min_threads = 2,
                CThread::TRunMode threads_mode = CThread::fRunDefault);

    /// Destructor
    virtual ~CThreadPool(void);

    /// Add task to the pool for execution.
    /// @note
    ///   The pool will acquire a CRef ownership to the task which it will
    ///   hold until the task goes out of the pool (when finished)
    /// @param task
    ///   Task to add
    /// @param timeout
    ///   Time to wait if the tasks queue has reached its maximum length.
    ///   If NULL, then wait infinitely.
    void AddTask(CThreadPool_Task* task, const CTimeSpan* timeout = NULL);

    /// Request to cancel the task and remove it from queue if it is there
    ///
    /// @sa CThreadPool_Task::RequestToCancel() 
    void CancelTask(CThreadPool_Task* task);

    /// Abort all functions of the pool.
    /// @note
    ///   This call renders the pool unusable in the sense that you must not
    ///   call any of its methods after that!
    /// @param timeout
    ///   Maximum time to wait for the termination of the pooled threads.
    ///   If this time is not enough for all threads to terminate, the Abort()
    ///   method returns, and all threads are terminated in the background.
    void Abort(const CTimeSpan* timeout = NULL);



    /// Constructor with custom controller
    /// @param queue_size
    ///   Maximum number of tasks waiting in the queue. If 0 then tasks
    ///   cannot be queued and are added only when there are threads
    ///   to process them. If greater than 0 and there will be attempt to add
    ///   new task over this maximum then method AddTask() will wait for the
    ///   given timeout for some empty space in the queue.
    /// @param controller
    ///   Custom controller object that will be responsible for number
    ///   of threads in the pool, when new threads have to be launched and
    ///   old and unused threads have to be finished. Default controller
    ///   implementation (set for the pool in case of using other
    ///   constructor) is CThreadPool_Controller_PID class.
    /// @param threads_mode
    ///   Running mode of all threads in thread pool. Values fRunDetached and
    ///   fRunAllowST are ignored.
    CThreadPool(unsigned int            queue_size,
                CThreadPool_Controller* controller,
                CThread::TRunMode       threads_mode = CThread::fRunDefault);

    /// Set timeout to wait for all threads to finish before the pool
    /// should be able to destroy.
    /// Default value is 10 seconds
    /// @note
    ///   This method is meant to be called very rarely. Because of that it is
    ///   implemented in non-threadsafe manner. While this method is working
    ///   it is not allowed to call itself or GetDestroyTimeout() in other
    ///   threads.
    void SetDestroyTimeout(const CTimeSpan& timeout);

    /// Get timeout to wait for all threads to finish before the pool
    /// will be able to destroy.
    /// @note
    ///   This method is meant to be called very rarely. Because of that it is
    ///   implemented in non-threadsafe manner. While this method is working
    ///   (and after that if timeout is stored in some variable as reference)
    ///   it is not allowed to call SetDestroyTimeout() in other threads.
    const CTimeSpan& GetDestroyTimeout(void) const;

    /// Binary flags indicating different possible options in what environment
    /// the pool will execute exclusive task
    ///
    /// @sa TExclusiveFlags, RequestExclusiveExecution()
    enum EExclusiveFlags {
        /// Do not allow to add new tasks to the pool during
        /// exclusive task execution
        fDoNotAllowNewTasks   = (1 << 0),
        /// Finish all threads currently running in the pool
        fFlushThreads         = (1 << 1),
        /// Cancel all currently executing tasks
        fCancelExecutingTasks = (1 << 2),
        /// Cancel all tasks waiting in the queue and not yet executing
        fCancelQueuedTasks    = (1 << 3),
        /// Execute all tasks waiting in the queue before execution
        /// of exclusive task
        fExecuteQueuedTasks   = (1 << 4)
    };
    /// Type of bit-masked combination of several values from EExclusiveFlags
    ///
    /// @sa EExclusiveFlags, RequestExclusiveExecution()
    typedef unsigned int TExclusiveFlags;

    /// Add the task for exclusive execution in the pool
    /// By default the pool suspends all new and queued tasks processing,
    /// finishes execution of all currently executing tasks and then executes
    /// exclusive task in special thread devoted to this work. The environment
    /// in which exclusive task executes can be modified by flags parameter.
    /// This method does not wait for exclusive execution, it is just adds
    /// the task to exclusive queue and starts the process of exclusive
    /// environment preparation. If next exclusive task will be added before
    /// preveous finishes (or even starts) its execution then they will be
    /// executed consequently each in its own exclusive environment (if flags
    /// parameter for them is different).
    ///
    /// @param task
    ///   Task to execute exclusively
    /// @param flags
    ///   Parameters of the exclusive environment
    void RequestExclusiveExecution(CThreadPool_Task*  task,
                                   TExclusiveFlags    flags = 0);

    /// Cancel the selected groups of tasks in the pool
    /// 
    /// @param tasks_group
    ///   Must be a combination of fCancelQueuedTasks and/or
    ///   fCancelExecutingTasks. Cannot be zero.
    void CancelTasks(TExclusiveFlags tasks_group);

    /// When to start new threads after flushing old ones
    ///
    /// @sa FlushThreads()
    enum EFlushType {
        eStartImmediately,  ///< New threads can be started immediately
        eWaitToFinish       ///< New threads can be started only when all old
                            ///< threads finished their execution
    };

    /// Finish all current threads and replace them with new ones
    /// @param flush_type
    ///   If new threads can be launched immediately after call to this
    ///   method or only after all "old" threads have been finished.
    void FlushThreads(EFlushType flush_type);

    /// Get total number of threads currently running in pool
    unsigned int GetThreadsCount(void) const;

    /// Get the number of tasks currently waiting in queue
    unsigned int GetQueuedTasksCount(void) const;

    /// Get the number of currently executing tasks
    unsigned int GetExecutingTasksCount(void) const;

    /// Does method Abort() was already called for this ThreadPool
    bool IsAborted(void) const;

protected:
    /// Create new thread for the pool
    virtual CThreadPool_Thread* CreateThread(void);

    /// Get the mutex that protects all changes in the pool
    CMutex& GetMainPoolMutex(void);

private:
    friend class CThreadPool_Impl;

    /// Prohibit copying and assigning
    CThreadPool(const CThreadPool&);
    CThreadPool& operator= (const CThreadPool&);

    /// Actual implementation of the pool
    CThreadPool_Impl* m_Impl;
};



/// Base class for a thread running inside CThreadPool and executing tasks.
///
/// Class can be inherited if it doesn't fit your specific needs. But to use
/// inherited class you also will need to inherit CThreadPool and override
/// its method CreateThread().

class NCBI_XUTIL_EXPORT CThreadPool_Thread : public CThread
{
public:
    /// Get the task currently executing in the thread
    CRef<CThreadPool_Task> GetCurrentTask(void) const;

protected:
    /// Construct and attach to the pool
    CThreadPool_Thread(CThreadPool* pool);

    /// Destructor
    virtual ~CThreadPool_Thread(void);

    /// Init this thread. It is called at beginning of Main()
    virtual void Initialize(void);

    /// Clean up. It is called by OnExit()
    virtual void Finalize(void);

    /// Get the thread pool in which this thread is running
    CThreadPool* GetPool(void) const;

private:
    friend class CThreadPool_ThreadImpl;

    /// Prohibit copying and assigning
    CThreadPool_Thread(const CThreadPool_Thread&);
    CThreadPool_Thread& operator= (const CThreadPool_Thread&);

    /// To prevent overriding - main thread function
    virtual void* Main(void);

    /// To prevent overriding - do cleanup after exiting from thread
    virtual void OnExit(void);

    /// Actual implementation of the thread
    CThreadPool_ThreadImpl* m_Impl;
};




/// Abstract class for controlling the number of threads in pool.
/// Every time when something happens in the pool (new task accepted, task has
/// started processing or has been processed, new threads started or some
/// threads killed) method HandleEvent() of this class is called. It makes
/// some common stuff and then calls OnEvent(). The algorithm in OnEvent()
/// has to decide how many threads should be in the pool and call method
/// SetThreadsCount() accordingly. For making your own algorithm
/// you should inherit this class. You then can pass an instance of the class
/// to the ThreadPool's constructor.
/// Controller is strictly attached to the pool in the pool's constructor.
/// One controller cannot track several ThreadPools and one ThreadPool cannot
/// be tracked by several controllers.
/// Implementation of this class is threadsafe, so all its parameters can be
/// changed during ThreadPool operation.


class NCBI_XUTIL_EXPORT CThreadPool_Controller : public CObject
{
public:
    /// Constructor
    /// @param max_threads
    ///   Maximum number of threads in pool
    /// @param min_threads
    ///   Minimum number of threads in pool
    CThreadPool_Controller(unsigned int  max_threads,
                           unsigned int  min_threads);

    /// Set the minimum number of threads in pool
    void SetMinThreads(unsigned int min_threads);

    /// Get the minimum number of threads in pool
    unsigned int GetMinThreads(void) const;

    /// Set the maximum number of threads in pool
    void SetMaxThreads(unsigned int max_threads);

    /// Get the maximum number of threads in pool
    unsigned int GetMaxThreads(void) const;

    /// Events that can happen with ThreadPool
    enum EEvent {
        eSuspend,  ///< ThreadPool is suspended for exclusive task execution
        eResume,   ///< ThreadPool is resumed after exclusive task execution
        eOther     ///< All other events (happen asynchronously, so cannot be
                   ///< further distinguished)
    };

    /// This method is called every time something happens in a pool,
    /// such as: new task added, task is started or finished execution,
    /// new threads started or some threads finished.
    /// It does the hardcoded must-do processing of the event, and also
    /// calls OnEvent() callback to run the controlling algorithm.
    /// Method ensures that OnEvent() always called protected with ThreadPool
    /// main mutex and that ThreadPool itself is not aborted or in suspended
    /// for exclusive execution state (except the eSuspend event).
    ///
    /// @sa OnEvent()
    void HandleEvent(EEvent event);

    /// Get maximum timeout for which calls to method HandleEvent() can be
    /// missing. Method HandleEvent() will be called after this timeout
    /// for sure if ThreadPool will not be aborted or in suspended state
    /// at this moment.
    virtual CTimeSpan GetSafeSleepTime(void) const;


protected:
    /// Destructor. Have to be called only from CRef
    virtual ~CThreadPool_Controller(void);

    /// Main method for the implementation of controlling algorithm.
    /// Method should not implement any excessive calculations because it
    /// will be called guarded with main pool mutex and because of that
    /// it will block several important pool operations.
    /// @note
    ///   Method will never be called recursively or concurrently in different
    ///   threads (HandleEvent() will take care of this).
    ///
    /// @sa HandleEvent()
    virtual void OnEvent(EEvent event) = 0;

    /// Get pool to which this class is attached
    CThreadPool* GetPool(void) const;

    /// Get mutex which guards access to pool
    /// All work in controller should be based on the same mutex as in pool.
    /// So every time when you need to guard access to some members of derived
    /// class it is recommended to use this very mutex. But NB: it's assumed
    /// everywhere that this mutex is locked on the small periods of time. So
    /// be careful and implement the same pattern.
    CMutex& GetMainPoolMutex(CThreadPool* pool) const;

    /// Ensure that constraints of minimum and maximum count of threads in pool
    /// are met. Start new threads or finish overflow threads if needed.
    void EnsureLimits(void);

    /// Set number of threads in pool
    /// Adjust given number to conform to minimum and maximum threads count
    /// constraints if needed.
    void SetThreadsCount(unsigned int count);


private:
    friend class CThreadPool_Impl;

    /// Prohibit copying and assigning
    CThreadPool_Controller(const CThreadPool_Controller&);
    CThreadPool_Controller& operator= (const CThreadPool_Controller&);

    /// Attach the controller to ThreadPool
    void x_AttachToPool(CThreadPool_Impl* pool);

    /// Detach the controller from pool when pool is aborted
    void x_DetachFromPool(void);

    /// ThreadPool to which this controller is attached
    CThreadPool_Impl*  m_Pool;
    /// Minimum number of threads in pool
    unsigned int       m_MinThreads;
    /// Maximum number of threads in pool
    unsigned int       m_MaxThreads;
    /// If controller is already inside HandleEvent() processing
    bool               m_InHandleEvent;
};




/// Exception class for all ThreadPool-related classes

class NCBI_XUTIL_EXPORT CThreadPoolException : public CException
{
public:
    enum EErrCode {
        eControllerBusy, ///< attempt to create several ThreadPools with
                         ///< the same controller
        eTaskBusy,       ///< attempt to change task when it's already placed
                         ///< into ThreadPool or to put task in ThreadPool
                         ///< several times
        eProhibited,     ///< attempt to do something when ThreadPool was
                         ///< already aborted or to add task when it is
                         ///< prohibited by flags of exclusive execution
        eInactive,       ///< attempt to call active methods in
                         ///< ThreadPool_Controller when it is not attached
                         ///< to any ThreadPool
        eInvalid         ///< attempt to operate task added in one ThreadPool
                         ///< by means of methods of another ThreadPool or
                         ///< invalid parameters in the constructor
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CThreadPoolException, CException);
};



//////////////////////////////////////////////////////////////////////////
//  All inline methods
//////////////////////////////////////////////////////////////////////////

inline bool
CThreadPool_Task::IsCancelRequested(void) const
{
    return m_CancelRequested;
}

inline CThreadPool_Task::EStatus
CThreadPool_Task::GetStatus(void) const
{
    return m_Status;
}

inline bool
CThreadPool_Task::IsFinished(void) const
{
    return m_Status >= eCompleted;
}

inline unsigned int
CThreadPool_Task::GetPriority(void) const
{
    return m_Priority;
}


inline unsigned int
CThreadPool_Controller::GetMinThreads(void) const
{
    return m_MinThreads;
}

inline unsigned int
CThreadPool_Controller::GetMaxThreads(void) const
{
    return m_MaxThreads;
}


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL__THREAD_POOL__HPP */
