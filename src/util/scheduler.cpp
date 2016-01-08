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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *   Implementation of Scheduler-related classes.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/scheduler.hpp>
#include <util/sync_queue.hpp>
#include <util/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Util_Scheduler


BEGIN_NCBI_SCOPE


/// Class storing full information about scheduled event for task execution
class CScheduler_QueueEvent : public CObject
{
public:
    /// Id of the series
    TScheduler_SeriesID     id;
    /// Task itself
    CIRef<IScheduler_Task>  task;
    /// Time when this event will be executed
    CTime                   exec_time;
    /// Period of repetitive execution of the task
    CTimeSpan               period;

    /// How to run repetitive tasks including not repeating at all
    /// @sa IScheduler::ERepeatPattern
    enum ERepeatPattern
    {
        /// Execute tasks in the specified period of time after the *START*
        /// of previous task's execution
        eWithRate = IScheduler::eWithRate,
        /// Execute tasks in the specified period of time after the *END*
        /// of previous task's execution
        eWithDelay = IScheduler::eWithDelay,
        /// Execute the task only once
        eNoRepeat
    };

    /// Repeating pattern of the task
    ERepeatPattern          repeat_pattern;


    /// Check if this event matches given series id
    bool IsMatch(TScheduler_SeriesID id) const
    {
        return this->id == id;
    }

    /// Check if this event matches given task
    bool IsMatch(IScheduler_Task* task) const
    {
        return &*this->task == task;
    }

    /// Dummy function to support code templates and avoid duplication of code
    bool IsMatch(bool dummy_val) const
    {
        return dummy_val;
    }

    // In the absence of the following constructor, new compilers (as required
    // by the new C++ standard) may fill the object memory with zeros,
    // erasing flags set by CObject::operator new (see CXX-1808)
    CScheduler_QueueEvent() {}
};



/// Class for comparing references to CSchedQueueTask by its execution time
struct PScheduler_QueueEvent_Compare
{
    bool operator() (const CRef<CScheduler_QueueEvent>&  left,
                     const CRef<CScheduler_QueueEvent>&  right)
    {
        return left->exec_time < right->exec_time;
    }
};



/// Thread-safe implementation of IScheduler interface
class CScheduler_MT
    : public CObject,
      public IScheduler
{
public:
    /// Schedule task for one-time execution
    virtual
    TScheduler_SeriesID AddTask(IScheduler_Task* task,
                                const CTime&     exec_time);

    /// Schedule task for repetitive execution
    virtual
    TScheduler_SeriesID AddRepetitiveTask(IScheduler_Task*  task,
                                          const CTime&      start_time,
                                          const CTimeSpan&  period,
                                          ERepeatPattern    repeat_pattern);

    /// Remove series from scheduler queue
    virtual
    void RemoveSeries(TScheduler_SeriesID series_id);

    /// Remove task from scheduler queue
    virtual
    void RemoveTask(IScheduler_Task* task);

    /// Unschedule all series waiting in scheduler queue
    virtual
    void RemoveAllSeries(void);

    /// Get full scheduler series list
    virtual
    void GetScheduledSeries(vector<SScheduler_SeriesInfo>* series) const;

    /// Add listener which will be notified about changing in time
    /// of availability of next scheduled task
    virtual
    void RegisterListener(IScheduler_Listener* listener);

    /// Remove scheduler listener
    virtual
    void UnregisterListener(IScheduler_Listener* listener);

    /// Get next time point when scheduler will be ready to execute some task
    /// If there are already tasks to execute then return current time.
    virtual
    CTime GetNextExecutionTime(void) const;

    /// Check if there are tasks in scheduler queue (if it is not empty)
    virtual
    bool IsEmpty(void) const;

    /// Check if there are tasks ready to execute
    virtual
    bool HasTasksToExecute(const CTime& now) const;

    /// Get information about next task that is ready to execute
    /// If there are no tasks to execute then return id = 0 and task = NULL
    virtual
    SScheduler_SeriesInfo GetNextTaskToExecute(const CTime& now);

    /// Be aware that task was just finished its execution
    virtual
    void TaskExecuted(TScheduler_SeriesID series_id, const CTime& now);

    /// Constructor
    CScheduler_MT(void);

protected:
    /// Destructor. To be called from CRef.
    virtual ~CScheduler_MT(void);

private:
    /// Prohibit copying and assigning
    CScheduler_MT(const CScheduler_MT&);
    CScheduler_MT& operator= (const CScheduler_MT&);

    /// Schedule task execution
    /// @param id
    ///   id of the scheduler series. if 0 then it is assigned automatically
    /// @param task
    ///   Task to execute
    /// @param exec_time
    ///   Time when task will be executed
    /// @param num_repeats
    ///   Total number of task executions
    /// @param period
    ///   Period between task executions
    /// @param isDelay
    ///   Whether period is executed from the beginning oor from the ending
    ///   of the task execution
    /// @param guard
    ///   Guard for the main mutex which will be released at the end of method
    TScheduler_SeriesID x_AddQueueTask
    (
        TScheduler_SeriesID                    id,
        IScheduler_Task*                       task,
        const CTime&                           exec_time,
        const CTimeSpan&                       period,
        CScheduler_QueueEvent::ERepeatPattern  repeat_pattern,
        CMutexGuard*                           guard
    );

    /// Change next execution time when queue of scheduled tasks is changed.
    /// Notify all listeners about change if needed.
    /// @param guard
    ///   Guardian locking main scheduler mutex which must be unlocked before
    ///   notification of listeners. NB: after method execution mutex is not
    ///   locked anymore.
    void x_SchedQueueChanged(CMutexGuard* guard);

    /// Implementation of removing task from queue.
    /// The task is searched by criteria given as a parameter. Parameter
    /// can be of any type that is accepted by
    /// CScheduler_QueueEvent::IsMatch().
    ///
    /// @sa CScheduler_QueueEvent::IsMatch(), RemoveSeries(), RemoveTask()
    template <class T>
    void x_RemoveTaskImpl(T task);


    /// Type of queue for information about scheduled tasks
    typedef CSyncQueue_multiset<CRef<CScheduler_QueueEvent>,
                                PScheduler_QueueEvent_Compare> TSchedQueue;
    /// Type of list of information about currently executing tasks
    typedef deque< CRef<CScheduler_QueueEvent> >               TExecList;
    /// Type of list of all scheduler listeners
    typedef vector<IScheduler_Listener*>                       TListenersList;

    /// Queue of scheduled tasks
    TSchedQueue     m_ScheduledTasks;
    /// List of executing tasks
    TExecList       m_ExecutingTasks;
    /// Counter for generating task id
    CAtomicCounter  m_IDCounter;
    /// Main mutex for protection of changes in scheduler
    mutable CMutex  m_MainMutex;
    /// List of all scheduler listeners
    TListenersList  m_Listeners;
    /// Time of execution of nearest task
    CTime           m_NextExecTime;
};



// Max time_t minus a couple days to avoid any possible problems related to
// conversions to local time etc.
static const time_t kInfinityTimeT = 0x7FFB0000;


CScheduler_MT::CScheduler_MT(void)
{
    m_NextExecTime.SetTimeT(kInfinityTimeT);
    m_IDCounter.Set(0);
}

CScheduler_MT::~CScheduler_MT(void)
{
}

TScheduler_SeriesID
CScheduler_MT::x_AddQueueTask
(
    TScheduler_SeriesID                    id,
    IScheduler_Task*                       task,
    const CTime&                           exec_time,
    const CTimeSpan&                       period,
    CScheduler_QueueEvent::ERepeatPattern  repeat_pattern,
    CMutexGuard*                           guard
)
{
    // Be sure that task is referenced and will be destroyed in case
    // of any exception
    CIRef<IScheduler_Task> task_ref(task);

    CRef<CScheduler_QueueEvent> event_info(new CScheduler_QueueEvent());

    if (id == 0) {
        id = (TScheduler_SeriesID)m_IDCounter.Add(1);
    }

    event_info->id = id;
    event_info->task = task;
    event_info->exec_time = exec_time;
    event_info->period = period;
    event_info->repeat_pattern = repeat_pattern;

    m_ScheduledTasks.push_back(event_info);

    x_SchedQueueChanged(guard);
    // Mutex unlocked!!!

    return id;
}

void
CScheduler_MT::x_SchedQueueChanged(CMutexGuard* guard)
{
    TListenersList listeners;

    {{
        // This part will be guarded by the mutex

        CTime next_time;

        if (m_ScheduledTasks.size() == 0) {
            next_time.SetTimeT(kInfinityTimeT);
        }
        else {
            next_time = (*m_ScheduledTasks.begin())->exec_time;
        }

        if (next_time != m_NextExecTime) {
            m_NextExecTime = next_time;
            listeners = m_Listeners;
        }

        guard->Release();
    }}

    NON_CONST_ITERATE(TListenersList, it, listeners) {
        (*it)->OnNextExecutionTimeChange(this);
    }
}

TScheduler_SeriesID
CScheduler_MT::AddTask(IScheduler_Task* task, const CTime& exec_time)
{
    CMutexGuard guard(m_MainMutex);

    return x_AddQueueTask(0, task, exec_time, CTimeSpan(),
                          CScheduler_QueueEvent::eNoRepeat, &guard);
}

TScheduler_SeriesID
CScheduler_MT::AddRepetitiveTask(IScheduler_Task*  task,
                                 const CTime&      start_time,
                                 const CTimeSpan&  period,
                                 ERepeatPattern    repeat_pattern)
{
    CMutexGuard guard(m_MainMutex);

    return x_AddQueueTask(0, task, start_time, period,
                       CScheduler_QueueEvent::ERepeatPattern(repeat_pattern),
                       &guard);
}

template <class T>
inline void
CScheduler_MT::x_RemoveTaskImpl(T task)
{
    CMutexGuard guard(m_MainMutex);

    bool is_begin_removed = false;

    for (TSchedQueue::iterator it = m_ScheduledTasks.begin();
                                it != m_ScheduledTasks.end(); )
    {
        if ((*it)->IsMatch(task)) {
            if (it == m_ScheduledTasks.begin()) {
                is_begin_removed = true;
            }
            it = m_ScheduledTasks.erase(it);
        }
        else {
            ++it;
        }
    }

    ITERATE(TExecList, it, m_ExecutingTasks) {
        if ((*it)->IsMatch(task)) {
            it->GetNCPointer()->repeat_pattern = CScheduler_QueueEvent::eNoRepeat;
        }
    }

    if (is_begin_removed) {
        x_SchedQueueChanged(&guard);
        // Mutex unlocked!!!
    }
}

void
CScheduler_MT::RemoveSeries(TScheduler_SeriesID series_id)
{
    x_RemoveTaskImpl(series_id);
}

void
CScheduler_MT::RemoveTask(IScheduler_Task* task)
{
    x_RemoveTaskImpl(task);
}

void
CScheduler_MT::RemoveAllSeries(void)
{
    x_RemoveTaskImpl(true);
}

void
CScheduler_MT::GetScheduledSeries(vector<SScheduler_SeriesInfo>* series) const
{
    series->clear();

    {{
        CMutexGuard guard(m_MainMutex);

        series->resize(m_ScheduledTasks.size());
        size_t ind = 0;
        ITERATE (TSchedQueue, it, m_ScheduledTasks) {
            (*series)[ind].id   = (*it)->id;
            (*series)[ind].task = (*it)->task;
            ++ind;
        }

        ITERATE(TExecList, it, m_ExecutingTasks) {
            if ((*it)->repeat_pattern != CScheduler_QueueEvent::eNoRepeat) {
                series->resize(ind + 1);
                (*series)[ind].id   = (*it)->id;
                (*series)[ind].task = (*it)->task;
                ++ind;
            }
        }
    }}
}

void
CScheduler_MT::RegisterListener(IScheduler_Listener* listener)
{
    CMutexGuard guard(m_MainMutex);

    m_Listeners.push_back(listener);
}

void
CScheduler_MT::UnregisterListener(IScheduler_Listener* listener)
{
    CMutexGuard guard(m_MainMutex);

    TListenersList::iterator it = find(m_Listeners.begin(), m_Listeners.end(),
                                       listener);

    if (it != m_Listeners.end()) {
        m_Listeners.erase(it);
    }
}

CTime
CScheduler_MT::GetNextExecutionTime(void) const
{
    CMutexGuard guard(m_MainMutex);

    return m_NextExecTime;
}

bool
CScheduler_MT::IsEmpty(void) const
{
    CMutexGuard guard(m_MainMutex);

    bool result = m_ScheduledTasks.empty();

    if (result) {
        ITERATE(TExecList, it, m_ExecutingTasks) {
            if ((*it)->repeat_pattern != CScheduler_QueueEvent::eNoRepeat)
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

bool
CScheduler_MT::HasTasksToExecute(const CTime& now) const
{
    CMutexGuard guard(m_MainMutex);

    return m_NextExecTime <= now;
}

SScheduler_SeriesInfo
CScheduler_MT::GetNextTaskToExecute(const CTime& now)
{
    SScheduler_SeriesInfo res_info;
    res_info.id = 0;
    CRef<CScheduler_QueueEvent> event_info;

    {{
        CMutexGuard guard(m_MainMutex);

        if (m_ScheduledTasks.size() == 0
            ||  (*m_ScheduledTasks.begin())->exec_time > now)
        {
            return res_info;
        }

        event_info = m_ScheduledTasks.front();
        m_ScheduledTasks.pop_front();
        m_ExecutingTasks.push_back(event_info);

        res_info.id   = event_info->id;
        res_info.task = event_info->task;

        if (event_info->repeat_pattern == CScheduler_QueueEvent::eWithRate) {
            x_AddQueueTask(event_info->id,
                           event_info->task,
                           event_info->exec_time + event_info->period,
                           event_info->period,
                           event_info->repeat_pattern,
                           &guard);
            // Mutex unlocked!!!
            // x_SchedQueueChanged() is called inside x_AddQueueTask()
        }
        else {
            // x_SchedQueueChanged() should be called anyway because we've changed
            // the beginning of the queue
            x_SchedQueueChanged(&guard);
            // Mutex unlocked!!!
        }
    }}

    return res_info;
}

void
CScheduler_MT::TaskExecuted(TScheduler_SeriesID series_id, const CTime& now)
{
    CMutexGuard guard(m_MainMutex);

    CRef<CScheduler_QueueEvent> event_info;

    NON_CONST_ITERATE(TExecList, it, m_ExecutingTasks) {
        if ((*it)->IsMatch(series_id)) {
            event_info = *it;
            m_ExecutingTasks.erase(it);
            break;
        }
    }

    if (event_info.IsNull()) {
        return;
    }

    if (event_info->repeat_pattern == CScheduler_QueueEvent::eWithDelay) {
        x_AddQueueTask(event_info->id,
                       event_info->task,
                       now + event_info->period,
                       event_info->period,
                       event_info->repeat_pattern,
                       &guard);
        // Mutex unlocked!!!
    }
}



CIRef<IScheduler>
IScheduler::Create(void)
{
    return CIRef<IScheduler>(new CScheduler_MT());
}



/// Standalone thread to execute scheduled tasks - implementation
class CScheduler_ExecThread_Impl
    : public IScheduler_Listener,
      public CThread
{
public:
    /// Constructor
    /// @param scheduler
    ///   Scheduler which tasks will be executed
    CScheduler_ExecThread_Impl(IScheduler* scheduler);

    /// Callback from the scheduler -- about changes in the execution timeline
    virtual void OnNextExecutionTimeChange(IScheduler*);

    /// Stop executing the tasks, and finish the thread.
    /// This method should be called to force executor to finish its work
    /// and destroy. Without it destruction will be impossible.
    void Stop(void);

protected:
    /// Destructor, to be called from CRef
    virtual ~CScheduler_ExecThread_Impl();

    /// Main thread function
    virtual void* Main(void);

private:
    /// Prohibit copying and assignment
    CScheduler_ExecThread_Impl(const CScheduler_ExecThread_Impl&);
    CScheduler_ExecThread_Impl& operator= (const CScheduler_ExecThread_Impl&);

    /// Scheduler controlled by the executor
    CIRef<IScheduler>                 m_Scheduler;
    /// Reference to self to avoid destruction earlier than needed
    CRef<CScheduler_ExecThread_Impl>  m_SelfRef;
    /// Semaphore for handling idle waiting
    CSemaphore                        m_WaitTrigger;
    /// If the thread has been requested to stop
    volatile bool                     m_Stopped;
};



CScheduler_ExecThread_Impl::CScheduler_ExecThread_Impl(IScheduler*  scheduler)
    : m_Scheduler(scheduler),
      m_WaitTrigger(0, kMax_Int),
      m_Stopped(false)
{
    m_SelfRef = this;
    m_Scheduler->RegisterListener(this);
    Run(CThread::fRunDetached);
}

CScheduler_ExecThread_Impl::~CScheduler_ExecThread_Impl(void)
{}

void
CScheduler_ExecThread_Impl::OnNextExecutionTimeChange(IScheduler*)
{
    m_WaitTrigger.Post();
}

void
CScheduler_ExecThread_Impl::Stop(void)
{
    m_Stopped = true;
    m_WaitTrigger.Post();
    m_SelfRef = NULL;
}

void*
CScheduler_ExecThread_Impl::Main(void)
{
    CTime cur_time(CTime::eCurrent);
    while ( !m_Stopped ) {
        CTimeSpan timeout = m_Scheduler->GetNextExecutionTime() - cur_time;
        m_WaitTrigger.TryWait(CTimeout(timeout));

        // If we are already stopped we will not do unnecessary work
        if ( !m_Stopped ) {
            cur_time.SetCurrent();
            for (;;) {
                SScheduler_SeriesInfo task_info =
                    m_Scheduler->GetNextTaskToExecute(cur_time);

                if (task_info.task.IsNull())
                    break;

                try {
                    task_info.task->Execute();
                }
                catch (exception& e) {
                    ERR_POST_X(1, "Exception in scheduler task execution: "
                                  << e.what());
                }

                if (m_Stopped)
                    break;

                cur_time.SetCurrent();
                m_Scheduler->TaskExecuted(task_info.id, cur_time);
            }
        }
    }

    return NULL;
}



CScheduler_ExecutionThread::CScheduler_ExecutionThread(IScheduler* scheduler)
    : m_Impl(new CScheduler_ExecThread_Impl(scheduler))
{
}

CScheduler_ExecutionThread::~CScheduler_ExecutionThread(void)
{
    m_Impl->Stop();
}

END_NCBI_SCOPE
