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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include <corelib/ncbireg.hpp>

#include "scheduler.hpp"
#include "threads_man.hpp"
#include "timers.hpp"
#include "srv_stat.hpp"


BEGIN_NCBI_SCOPE;

struct SPrtyExecMap_tag;
typedef intr::set_base_hook<intr::tag<SPrtyExecMap_tag>,
                            intr::optimize_size<true> >     TPrtyExecMapHook;

struct SPrtyExecQueue : public TPrtyExecMapHook
{
    Uint1 priority;
    Uint4 exec_time;
    TSrvTaskList tasks;

    SPrtyExecQueue(void)
        : priority(0), exec_time(0)
    {
    }
};

struct SPrtyExecCompare
{
    bool operator() (const SPrtyExecQueue& left, const SPrtyExecQueue& right) const
    {
        return left.priority < right.priority;
    }
    bool operator() (Uint1 left, const SPrtyExecQueue& right) const
    {
        return left < right.priority;
    }
    bool operator() (const SPrtyExecQueue& left, Uint1 right) const
    {
        return left.priority < right;
    }
};

typedef intr::set<SPrtyExecQueue,
                  intr::base_hook<TPrtyExecMapHook>,
                  intr::constant_time_size<false>,
                  intr::compare<SPrtyExecCompare> >     TPrtyExecMap;


struct SSchedInfo
{
    TPrtyExecMap tasks_map;
    CMiniMutex tasks_lock;
    /// This futex has total number of tasks in the tasks_map as a value.
    /// SchedExecuteTask() also uses it to wait for any tasks to be queued in
    /// this thread (it waits for futex's value to change to something other
    /// than 0).
    CFutex cnt_signal;
    Uint4 max_tasks;
    Uint4 done_tasks;
    Uint8 done_time;
    Uint8 wait_time;
    Uint8 max_slice;
    CSrvTime jfy_start_time;
    CSrvTime last_exec_time;
    TSrvThreadNum prefer_thr_num;

    SSchedInfo(void)
        : max_tasks(0), done_tasks(0), done_time(0), wait_time(0), max_slice(0), prefer_thr_num(0)
    {
    }
};


static Uint4 s_MaxTasksCoef = 1;
static Uint4 s_MaxTaskLatency = 500;
static int s_IdleStopTimeout = 300;


extern SSrvThread** s_Threads;
extern Uint8 s_CurJiffies;
extern CSrvTime s_JiffyTime;



static SPrtyExecQueue*
s_GetExecQueue(TPrtyExecMap& prty_map, Uint1 priority)
{
    TPrtyExecMap::iterator it = prty_map.find(priority, SPrtyExecCompare());
    if (it != prty_map.end())
        return &*it;

    SPrtyExecQueue* exec_queue = new SPrtyExecQueue();
    exec_queue->priority = priority;
    prty_map.insert(*exec_queue);
    return exec_queue;
}

static void
s_AddTaskToQueue(SSrvThread* thr, CSrvTask* task)
{
    SSchedInfo* sched = thr->sched;
    sched->tasks_lock.Lock();
    // In the rare event that thread thr started the process of stopping itself
    // before we were able to queue new task into it we change thr to point either
    // to current thread (if it's a worker thread) or to first worker thread.
    if (!IsThreadRunning(thr)) {
        sched->tasks_lock.Unlock();
        thr = GetCurThread();
        if (!IsThreadRunning(thr))
            thr = s_Threads[1];
        sched = thr->sched;
        sched->tasks_lock.Lock();
    }

    SPrtyExecQueue* exec_queue = s_GetExecQueue(sched->tasks_map,
                                                task->GetPriority());
    exec_queue->tasks.push_back(*task);
    int signal_val = sched->cnt_signal.GetValue();
    sched->cnt_signal.SetValueNonAtomic(signal_val + 1);
    sched->tasks_lock.Unlock();

    if (signal_val == 0)
        sched->cnt_signal.WakeUpWaiters(1);
}

static CSrvTask*
s_UnqueueTask(SSchedInfo* sched)
{
    sched->tasks_lock.Lock();
    TPrtyExecMap& tasks_map = sched->tasks_map;
    TPrtyExecMap::iterator it_first = tasks_map.begin();
    while (it_first != tasks_map.end()  &&  it_first->tasks.empty())
        ++it_first;
    if (it_first == tasks_map.end()) {
        // This is a rare event when we saw that this thread has some tasks in
        // the queue but by the time we were able to obtain the lock some other
        // thread took all task from this one.
        sched->tasks_lock.Unlock();
        return NULL;
    }

    // Check if enough tasks with this priority have been already executed and
    // it's time to execute task with a lower priority (if there is any).
    SPrtyExecQueue* q_exec = &*it_first;
    TPrtyExecMap::iterator it_next = it_first;
    ++it_next;
    while (it_next != tasks_map.end()) {
        SPrtyExecQueue* q_next = &*it_next;
        if (!q_next->tasks.empty()
            &&  q_next->exec_time + q_next->priority <= q_exec->exec_time)
        {
            break;
        }
        ++it_next;
    }
    // If we found task with lower priority that needs to be executed then
    // change our pointers for execution.
    if (it_next != tasks_map.end()) {
        it_first = it_next;
        q_exec = &*it_first;
    }

    CSrvTask* task = &q_exec->tasks.front();
    q_exec->tasks.pop_front();
    // Take a note that task with this priority is going to be executed.
    q_exec->exec_time += q_exec->priority;
    sched->cnt_signal.SetValueNonAtomic(sched->cnt_signal.GetValue() - 1);
    sched->tasks_lock.Unlock();

    return task;
}

static inline bool
s_IsThreadOverloaded(SSrvThread* thr, Uint4 max_coef)
{
    return Uint4(thr->sched->cnt_signal.GetValue()) >= thr->sched->max_tasks * max_coef
           ||  s_CurJiffies - thr->seen_jiffy > 1;
}

static SSrvThread*
s_FindQueueThread(TSrvThreadNum prefer_num, SSrvThread* cur_thr)
{
    // First of all if current thread is not a worker thread or is in the process
    // of stopping then it shouldn't be mentioned anywhere in the function.
    if (cur_thr  &&  !IsThreadRunning(cur_thr))
        cur_thr = NULL;
    // We will prefer either thread number given, or current thread, or 1st
    // worker thread.
    SSrvThread* prefer_thr = s_Threads[prefer_num];
    if (!IsThreadRunning(prefer_thr))
        prefer_thr = cur_thr? cur_thr: s_Threads[1];
    Uint4 max_coef = ACCESS_ONCE(s_MaxTasksCoef);

try_all_checks_again:
    // Start checking with preferred thread. Later we'll check others.
    SSrvThread* queue_thr = prefer_thr;
    Uint1 pref_chain_tries = 2;

check_thr_overload:
    if (!s_IsThreadOverloaded(queue_thr, max_coef)) {
        // If thread is not overloaded then just queue the task here and return.
select_queue_thr:
        if (queue_thr != prefer_thr) {
            // If we walked some preference chains or worse yet checked each thread
            // one-by-one then remember what we chose eventually so that later calls
            // to s_FindQueueThread with the same prefer_num would complete faster.
            prefer_thr->sched->prefer_thr_num = queue_thr->thread_num;
        }
        return queue_thr;
    }

    if (pref_chain_tries != 0) {
        // If preferred thread is overloaded then we'll try to walk preference
        // chain first (if there is any).
        --pref_chain_tries;
        TSrvThreadNum pref_chain_num = ACCESS_ONCE(queue_thr->sched->prefer_thr_num);
        queue_thr = s_Threads[pref_chain_num];
        if (IsThreadRunning(queue_thr))
            goto check_thr_overload;
    }

    // If preferred thread is overloaded and there's no preference chain or we
    // walked it all through, then we'll just check each thread one-by-one and
    // put the task to the first thread that have some available capacity in the
    // queue. Because of this loop and because of the preference given to 1st
    // thread in several other places (search for s_Threads[1] to find them) we
    // effectively implement the rule "all our tasks should use as few threads
    // as possible and all these threads should be compacted into lower thread
    // numbers, i.e. if we use n threads their numbers should be 1..n".
    TSrvThreadNum thr_num = 1;
    for (; thr_num <= s_MaxRunningThreads; ++thr_num) {
        queue_thr = s_Threads[thr_num];
        if (!IsThreadRunning(queue_thr)) {
            // As we meet first inactive thread we can be sure that there's no
            // more active worker threads with bigger numbers.
            break;
        }
        if (!s_IsThreadOverloaded(queue_thr, max_coef))
            goto select_queue_thr;
    }
    if (thr_num <= s_MaxRunningThreads) {
        // If we didn't find suitable thread for queuing and there's still some
        // capacity for new threads then we should start one. While it will be
        // starting we'll queue the task to the current or preferred thread,
        // even though we have found earlier that it's overloaded. When new thread
        // starts it will check all threads and if it finds overloaded one it
        // will take tasks from it.
        RequestThreadStart(s_Threads[thr_num]);
        return cur_thr? cur_thr: prefer_thr;
    }
    if (max_coef >= numeric_limits<Uint2>::max()) {
        // This if should never be entered but for protection against infinite
        // loop in this function we should put it here.
        return cur_thr? cur_thr: prefer_thr;
    }

    // So we checked all available threads -- they all are overloaded and we can't
    // start new thread. Now we'll pretend that all our per-thread limits on
    // amount of tasks allowed to be queued before overloading became twice as big
    // and repeat the whole calculation procedure once again. This is done to
    // make task distribution more even between threads even in face of very slow
    // processing of those tasks (or in face of huge amounts of tasks).
    // TODO: s_MaxTasksCoef should be lowered back to 1 somewhere.
    AtomicCAS(s_MaxTasksCoef, max_coef, max_coef * 2);
    max_coef *= 2;
    goto try_all_checks_again;
}

static void
s_BalanceTasks(SSchedInfo* sched, SSrvThread* cur_thr)
{
    TSrvTaskList task_lst;

    // Task all the tasks from given sched structure.
    sched->tasks_lock.Lock();
    NON_CONST_ITERATE(TPrtyExecMap, it, sched->tasks_map) {
        SPrtyExecQueue* exec_q = &*it;
        task_lst.splice_after(task_lst.end(), exec_q->tasks);
    }
    sched->cnt_signal.SetValueNonAtomic(0);
    sched->tasks_lock.Unlock();

    // Now distribute these tasks using standard distribution rules. This function
    // is called on sched either from overloaded (or unresponsive) thread, or from
    // stopped thread. Either way standard distribution rules will prevent tasks
    // from going back to the same sched queue (unless that thread became
    // responsive again, without any tasks and thus without any overload).
    while (!task_lst.empty()) {
        CSrvTask* task = &task_lst.front();
        task_lst.pop_front();
        SSrvThread* queue_thr = s_FindQueueThread(cur_thr->thread_num, cur_thr);
        s_AddTaskToQueue(queue_thr, task);
    }
}

static void
s_FindRebalanceTasks(SSrvThread* cur_thr)
{
    // Find thread with maximum number that has any tasks queued and redistribute
    // all the tasks preferring current thread as a queuing destination.
    for (TSrvThreadNum i = s_MaxRunningThreads; i > cur_thr->thread_num; --i) {
        SSrvThread* src_thr = s_Threads[i];
        if (src_thr->sched->cnt_signal.GetValue() != 0) {
            s_BalanceTasks(src_thr->sched, cur_thr);
            return;
        }
    }
}

void
SchedCheckOverloads(void)
{
    for (TSrvThreadNum i = 1; i <= s_MaxRunningThreads; ++i) {
        SSrvThread* thr = s_Threads[i];
        if (!IsThreadRunning(thr))
            return;
        if (s_IsThreadOverloaded(thr, s_MaxTasksCoef))
            s_BalanceTasks(thr->sched, GetCurThread());
    }
}

static void
s_ResetExecTime(SSchedInfo* sched)
{
    sched->tasks_lock.Lock();
    TPrtyExecMap& prty_map = sched->tasks_map;
    NON_CONST_ITERATE(TPrtyExecMap, it, prty_map) {
        it->exec_time = 0;
    }
    sched->tasks_lock.Unlock();
}

static void
s_ResetMaxTasks(SSchedInfo* sched)
{
    sched->max_tasks = Uint4(-1);;
    if (sched->done_tasks != 0  &&  sched->done_time != 0) {
        Uint8 max_tasks = Uint8(sched->done_tasks) * s_MaxTaskLatency / sched->done_time;
        if (max_tasks > 2  &&  max_tasks < Uint4(-1))
            sched->max_tasks = Uint4(max_tasks);
    }
    sched->done_tasks = 0;
    sched->done_time = 0;
    sched->wait_time = 0;
    sched->max_slice = 0;
}

void
SchedStartJiffy(SSrvThread* thr)
{
    SSchedInfo* sched = thr->sched;
    CSrvTime cur_time = CSrvTime::Current();
    CSrvTime jiffy_time = cur_time;
    jiffy_time -= sched->jfy_start_time;
    if (sched->jfy_start_time.Sec() != 0) {
        thr->stat->SchedJiffyStat(jiffy_time.AsUSec(), sched->max_slice,
                                  sched->done_time, sched->done_tasks,
                                  sched->wait_time);
    }

    s_ResetExecTime(sched);
    s_ResetMaxTasks(sched);
    if (s_IsThreadOverloaded(thr, s_MaxTasksCoef))
        s_BalanceTasks(sched, thr);

    sched->jfy_start_time = cur_time;
    if (jiffy_time.Sec() != 0) {
        // Jiffy time can be longer than a second only in 2 cases:
        //  - if some task was executing for too long. In this case updating
        //    last_exec_time won't hurt and idle state calculation below are not
        //    needed anyway.
        //  - if this thread was stopped and now started again. In this case
        //    trying to do any idle state related calculations will hurt and
        //    update to last_exec_time is necessary, because this thread have been
        //    just started and we need to start counting for s_IdleStopTimeout
        //    starting from this very moment.
        sched->last_exec_time = cur_time;
    }
    else if (sched->cnt_signal.GetValue() == 0) {
        CSrvTime idle_time = cur_time;
        idle_time -= sched->last_exec_time;
        if (idle_time.Sec() >= s_IdleStopTimeout) {
            // If this thread didn't execute any tasks for a significant amount
            // of time then this thread should be either stopped or (if it's not
            // running worker thread with maximum number) all tasks from thread
            // with maximum number should be redistributed preferrably to this
            // thread.
            if (IsThreadRunning(s_Threads[thr->thread_num + 1]))
                s_FindRebalanceTasks(thr);
            else if (thr->thread_num != 1)
                RequestThreadStop(thr);
        }
    }
}

bool
SchedIsAllIdle(void)
{
    CSrvTime cur_time = CSrvTime::Current();
    for (TSrvThreadNum i = 1; i <= s_MaxRunningThreads; ++i) {
        SSrvThread* thr = s_Threads[i];
        SSchedInfo* sched = thr->sched;
        sched->tasks_lock.Lock();
        if (sched->cnt_signal.GetValue() != 0  ||  thr->cur_task) {
            sched->tasks_lock.Unlock();
            return false;
        }
        sched->tasks_lock.Unlock();

        CSrvTime idle_time = cur_time;
        idle_time -= sched->last_exec_time;
        if (idle_time <= s_JiffyTime)
            return false;
    }
    return true;
}

static inline void
s_DoTermination(CSrvTask* task)
{
    if (!CTaskServer::IsInShutdown())
        task->m_Terminator.CallRCU();
}

void
MarkTaskTerminated(CSrvTask* task, bool immediate /* = true */)
{
    ESrvTaskFlags term_flag = (immediate? fTaskTerminated: fTaskNeedTermination);

retry:
    TSrvTaskFlags old_flags = ACCESS_ONCE(task->m_TaskFlags);
    TSrvTaskFlags new_flags = old_flags | term_flag;
    if (!AtomicCAS(task->m_TaskFlags, old_flags, new_flags))
        goto retry;

    if (term_flag == fTaskTerminated
        &&  (new_flags & (fTaskRunning + fTaskQueued)) == 0)
    {
        s_DoTermination(task);
    }
}

static inline void
s_MarkTaskRunning(CSrvTask* task)
{
retry:
    TSrvTaskFlags old_flags = ACCESS_ONCE(task->m_TaskFlags);
    if ((old_flags & fTaskQueued) == 0  ||  (old_flags & fTaskRunning) != 0) {
        SRV_FATAL("Invalid task flags: " << old_flags);
    }
    TSrvTaskFlags new_flags = old_flags + fTaskRunning - fTaskQueued;
    if (!AtomicCAS(task->m_TaskFlags, old_flags, new_flags))
        goto retry;
}

static void
s_MarkTaskExecuted(CSrvTask* task, SSrvThread* thr)
{
retry:
    TSrvTaskFlags old_flags = ACCESS_ONCE(task->m_TaskFlags);
    if ((old_flags & fTaskQueued) != 0  ||  (old_flags & fTaskRunning) == 0) {
        SRV_FATAL("Invalid task flags: " << old_flags);
    }
    TSrvTaskFlags new_flags = old_flags - fTaskRunning;
    if (new_flags & (fTaskNeedTermination + fTaskTerminated))
        new_flags &= ~fTaskRunnable;
    else if (new_flags & fTaskRunnable)
        new_flags = new_flags - fTaskRunnable + fTaskQueued;
    if (!AtomicCAS(task->m_TaskFlags, old_flags, new_flags))
        goto retry;

    if (new_flags & fTaskQueued) {
        SSrvThread* queue_thr = s_FindQueueThread(thr->thread_num, thr);
        s_AddTaskToQueue(queue_thr, task);
    }
    else if (new_flags & fTaskTerminated)
        s_DoTermination(task);
}

void
SchedExecuteTask(SSrvThread* thr)
{
    SSchedInfo* sched = thr->sched;
    if (sched->cnt_signal.GetValue() == 0) {
        // If there's no tasks to execute thread will sleep for up to s_JiffyTime.
        // If thread will sleep for the whole s_JiffyTime then we'll return to
        // caller who then will execute per-jiffy tasks. If this thread will be
        // woken up by somebody else because new tasks are available we'll return
        // to the caller, check if jiffy number was changed (if yes then per-jiffy
        // tasks will be executed) and then re-enter this function to actually
        // execute tasks that were queued in this thread.
        CSrvTime start_time = CSrvTime::Current();
        sched->cnt_signal.WaitValueChange(0, s_JiffyTime);
        CSrvTime end_time = CSrvTime::Current();
        end_time -= start_time;
        sched->wait_time += end_time.AsUSec();
        return;
    }
    CSrvTask* task = s_UnqueueTask(sched);
    if (!task) {
        // This can happen in a rare case when somebody else took all tasks
        // from us and redistributed it somewhere else.
        return;
    }

    if (!IsThreadRunning(thr))
        RequestThreadRevive(thr);
    s_MarkTaskRunning(task);
    thr->cur_task = task;
    task->m_LastThread = thr->thread_num;
    CSrvTime start_time = CSrvTime::Current();
    task->InternalRunSlice(thr->thread_num);
    CSrvTime end_time = CSrvTime::Current();
    thr->cur_task = NULL;
    sched->last_exec_time = end_time;
    end_time -= start_time;
    Uint8 exec_time = end_time.AsUSec();
    if (exec_time > sched->max_slice)
        sched->max_slice = exec_time;
    sched->done_time += exec_time;
    ++sched->done_tasks;
    s_MarkTaskExecuted(task, thr);
}

void
ConfigureScheduler(CNcbiRegistry* reg, CTempString section)
{
    s_MaxTaskLatency = Uint4(reg->GetInt(section, "max_task_delay", 500));
    s_IdleStopTimeout = reg->GetInt(section, "idle_thread_stop_timeout", 300);
}

void
AssignThreadSched(SSrvThread* thr)
{
    SSchedInfo* sched = new SSchedInfo();
    sched->max_tasks = 1;
    sched->cnt_signal.SetValueNonAtomic(0);
    thr->sched = sched;
}

void
ReleaseThreadSched(SSrvThread* thr)
{
    SSchedInfo* sched = thr->sched;
    sched->jfy_start_time.Sec() = 0;
    s_BalanceTasks(sched, GetCurThread());
}



void
CSrvTask::Terminate(void)
{
    MarkTaskTerminated(this);
}

void
CSrvTask::SetRunnable(void)
{
retry:
    TSrvTaskFlags old_flags = ACCESS_ONCE(m_TaskFlags);
    // If task has been already terminated then do nothing.
    if (old_flags & (fTaskNeedTermination + fTaskTerminated))
        return;
    // If task is currently running then we need it to mark as runnable so that
    // it will get queued again when it finishes its execution.
    if (old_flags & fTaskRunning) {
        // If task is already marked as runnable then are job is done already.
        if (old_flags & fTaskRunnable)
            return;
        TSrvTaskFlags new_flags = old_flags + fTaskRunnable;
        if (!AtomicCAS(m_TaskFlags, old_flags, new_flags))
            goto retry;
        // Don't forget to cancel the timer if there was any.
        RemoveTaskFromTimer(this, new_flags);
        return;
    }
    // If task is already queued then we don't need to do anything.
    if (old_flags & fTaskQueued)
        return;

    // Now we need to mark task as queued and actually put into queue of some
    // thread.
    TSrvTaskFlags new_flags = old_flags + fTaskQueued;
    if (!AtomicCAS(m_TaskFlags, old_flags, new_flags))
        goto retry;

    // Don't forget to cancel the timer if there was any.
    RemoveTaskFromTimer(this, new_flags);
    SSrvThread* cur_thr = GetCurThread();
    TSrvThreadNum prefer_num = m_LastThread? m_LastThread: 1;
    SSrvThread* queue_thr = s_FindQueueThread(prefer_num, cur_thr);
    s_AddTaskToQueue(queue_thr, this);
}


CSrvTaskTerminator::CSrvTaskTerminator(void)
{}

CSrvTaskTerminator::~CSrvTaskTerminator(void)
{}

void
CSrvTaskTerminator::ExecuteRCU(void)
{
    CSrvTask* task = intr::detail::parent_from_member(this, &CSrvTask::m_Terminator);
    delete task;
}

END_NCBI_SCOPE;
