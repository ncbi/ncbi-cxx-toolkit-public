#ifndef NETCACHE__SRV_TASKS__HPP
#define NETCACHE__SRV_TASKS__HPP
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
 */


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


class CRequestContext;
struct STimerTicket;


struct SSrvTaskList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvTaskList_tag> >   TSrvTaskListHook;


/// This class is for TaskServer's internal use only.
/// This class executes deletion of CSrvTask object. By design it is possible
/// that when CSrvTask::Terminate() is called (or more exactly when
/// ExecuteSlice() which probably executes at this time has finished) other
/// threads still have pointer to this task (including probably inside
/// TaskServer's code). But it's assumed (and upper code have to maintain this
/// assumption) that once Terminate() is called any other thread will forget
/// about this pointer soon and won't recall it from anywhere ever. With this
/// assumption in mind CSrvTaskTerminator just need to wait for RCU grace
/// period to pass and then it will be safe to delete CSrvTask object.
class CSrvTaskTerminator : public CSrvRCUUser
{
public:
    CSrvTaskTerminator(void);
    virtual ~CSrvTaskTerminator(void);

private:
    virtual void ExecuteRCU(void);
};


/// Main working entity in TaskServer. It's called "task" because it should
/// work like task/process in OS with cooperative multi-tasking. Everything
/// that task want to do should be divided into relatively small pieces --
/// slices. OS (TaskServer in this case) will provide time slices to each task,
/// they should execute a portion of their work during this slice and then
/// return control back to OS to give other tasks a chance to have their time
/// slice of the processor (thread in case of TaskServer). Each slice of work
/// done by the task should be less than jiffy (0.01 second by default),
/// preferably one or two orders of magnitude less than jiffy. This way
/// TaskServer will be able to schedule all tasks more effectively and spread
/// the load evenly between threads.
class CSrvTask
{
public:
    CSrvTask(void);
    /// Set this task "runnable", i.e. available for execution, having some
    /// pending work requiring some processing power. Call to this method
    /// guarantees that ExecuteSlice() will be called some time later.
    /// If another execution of ExecuteSlice() is in progress at the time
    /// of call to SetRunnable() then TaskServer will call ExecuteSlice()
    /// again some time after current ExecuteSlice() finishes. This behavior
    /// guarantees that if ExecuteSlice() somewhere checks for more available
    /// work, doesn't find it, decides to return and some other thread calls
    /// SetRunnable() at this moment task will have a chance to check for
    /// available work in next call to ExecuteSlice().
    /// This method is thread-safe and can be called from any thread and any
    /// context.
    void SetRunnable(void);
    /// This call is basically equivalent to SetRunnable() but with guarantee
    /// that task will be scheduled for execution no earlier than delay_sec
    /// later (approximately, can be up to a second less if measured in
    /// absolute wall time). If SetRunnable() is called while task is waiting
    /// for its delay timer then timer is canceled and task should set this
    /// timer inside ExecuteSlice() again if it still needs it. Timers are
    /// never set if TaskServer is shutting down (CTaskServer::IsInShutdown()
    /// returns TRUE). In this case ExecuteSlice() will be called immediately
    /// after call to RunAfter() and task should recognize state of shutting
    /// down and behave accordingly.
    /// This method is not thread-safe and should be called only from inside
    /// ExecuteSlice() method.
    void RunAfter(Uint4 delay_sec);
    /// Stops task's execution and deletes it. This method should be used for
    /// any object derived from CSrvTask instead of direct destruction,
    /// otherwise in some situations TaskServer could dereference pointer to
    /// this task after its deletion. The only exception from this rule could
    /// be when task has been just created and SetRunnable() was never called
    /// for it. See more comments on this in the description of
    /// CSrvTaskTerminator.
    /// This method is thread-safe although it's preferred to call it from
    /// inside ExecuteSlice() method.
    /// This method is virtual for internal use only - CSrvSocketTask should
    /// use different termination sequence. Do not ever override this method
    /// in any other class.
    virtual void Terminate(void);

    /// Set and retrieve task's priority. TaskServer has simple notion of
    /// priority: all tasks by default have priority 1. For each slice of task
    /// with priority 2 TaskServer executes 2 slices of priority 1. For each
    /// slice of task with priority 3 TaskServer executes 3 tasks with
    /// priority 1 and 2 tasks with priority 2 (rounding goes up, error doesn't
    /// accumulate, i.e. for 2 tasks with priority 3 it executes 3 tasks with
    /// priority 2). And so on. But this rule works only if there are tasks
    /// with higher priorities in the queue. If only tasks with low priority
    /// are executing at the moment they will take all available time slices.
    /// Counting of already executed tasks and their priorities is done on a
    /// per-jiffy basis, i.e. if there were only high priority tasks for some
    /// time and then task with low priority appeared it probably will be
    /// executed within 0.01 second.
    /// That said priority system is not quite usable now because it needs
    /// things like priority inheritance from one task to the one created
    /// by it, priority combination in CSrvTransitionTask and probably some
    /// other mechanisms to work exactly as expected.
    void SetPriority(Uint1 prty);
    Uint1 GetPriority(void);

    /// Create new diagnostic context for this task to work in. See comments
    /// to SetDiagCtx() on the effect of that.
    void CreateNewDiagCtx(void);
    /// Set diagnostic context for this task to work in. From this moment on
    /// all log messages made from inside ExecuteSlice() of this task will be
    /// assigned to the provided request context. Diagnostic contexts can be
    /// nested -- second call to SetDiagCtx() sets new context of this task
    /// and next call to ReleaseDiagCtx() restores first context.
    void SetDiagCtx(CRequestContext* ctx);
    /// Get current diagnostic context for the task.
    CRequestContext* GetDiagCtx(void);
    /// Releases current diagnostic context of the task. Method must be called
    /// for all contexts created via CreateNewDiagCtx() or set via
    /// SetDiagCtx(). You cannot make more calls and you must not make less
    /// calls than CreateNewDiagCtx() and SetDiagCtx() combined. Both
    /// erroneous situations will result in program abort.
    void ReleaseDiagCtx(void);

// Consider this section protected as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    /// This is the main method to do all work this task should do. This
    /// method will be called periodically while somebody calls SetRunnable()
    /// or RunAfter() on this task. But number of calls to this method won't
    /// match number of calls to SetRunnable() -- if several calls to
    /// SetRunnable() are made before ExecuteSlice() had a chance to execute
    /// it will be called only once. Task's code inside ExecuteSlice() should
    /// understand itself how much work it needs to do and call SetRunnable()
    /// again if needed (or do all the work before returning from
    /// ExecuteSlice()).
    /// Parameter thr_num is number of thread on which ExecuteSlice() is
    /// called.
    virtual void ExecuteSlice(TSrvThreadNum thr_num) = 0;

private:
    CSrvTask(const CSrvTask&);
    CSrvTask& operator= (const CSrvTask&);

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    virtual ~CSrvTask(void);

    /// This is the real time slice execution method called from TaskServer.
    /// By default it just calls ExecuteSlice(). But CSrvSocketTask needs to
    /// do some work before calling into user's code (or even instead of it),
    /// thus this method was added and made virtual.
    virtual void InternalRunSlice(TSrvThreadNum thr_num);


    /// Hook to put this task into TSrvTaskList.
    TSrvTaskListHook m_TaskListHook;
    /// Thread number where this task was executed last time.
    TSrvThreadNum m_LastThread;
    /// Bit-OR of flags for this task. For possible flag values see
    /// scheduler.hpp.
    TSrvTaskFlags m_TaskFlags;
    /// Task's priority.
    Uint1 m_Priority;
    /// Current diagnostic context for this task.
    CRequestContext* m_DiagCtx;
    /// Nested diagnostic contexts of this task. This variable is not NULL
    /// if this task have ever used nested contexts and contains array of
    /// context pointers with deepest context (the one assigned to task first)
    /// in the element 0, context set later in the element 1 etc.
    CRequestContext** m_DiagChain;
    /// Timer ticket assigned to this task when it calls RunAfter().
    STimerTicket* m_Timer;
    /// Object that will delete this task after call to Terminate().
    CSrvTaskTerminator m_Terminator;
};


// Helper types to be used only inside TaskServer. It's placed here only to
// keep all relevant pieces in one place.
typedef intr::member_hook<CSrvTask,
                          TSrvTaskListHook,
                          &CSrvTask::m_TaskListHook>    TSrvTaskListOpt;
typedef intr::slist<CSrvTask,
                    TSrvTaskListOpt,
                    intr::cache_last<true>,
                    intr::constant_time_size<true>,
                    intr::size_type<Uint4> >            TSrvTaskList;


/// Special task which executes as finite state machine.
/// To implement a task that will be executed using FSM you need to declare
/// its class like this:
///
/// class CMyClass : public CSrvStatesTask<CMyClass>
///
/// All machine states are referenced via pointer to function implementing
/// this state. To make state referencing more convenient there's a typedef
/// Me pointing to your class (CMyClass in above example). So you can
/// reference to some state as &Me::x_StateFunctionName (all state
/// implementing methods should be private as they shouldn't be used anywhere
/// outside this state machine). Methods implementing states shouldn't take
/// any arguments and should return next state for this machine to go to
/// (for convenience there's typedef State to use as return type of methods).
/// When state-implementing method returns some non-NULL pointer to next state
/// machine will go to next state immediately. If state-implementing method
/// returns NULL it means that state of the machine shouldn't change and
/// the same state-implementing method should be called next time TaskServer
/// calls ExecuteSlice() of this task. If you need to change machine's state
/// but not execute it immediately or yield thread's time to other tasks
/// waiting for execution you can use a sequence like this:
///
/// SetState(&Me::x_NextState);
/// SetRunnable();  // use RunAfter() here if you need to execute later
/// return NULL;
///
/// Initial state of the machine should be set using SetState() in your class
/// constructor.
///
/// Virtual inheritance from CSrvTask is used to allow derived classes to
/// combine several task types together (such as CSrvSocketTask,
/// CSrvStatesTask etc.) and still have one implementation of CSrvTask.
template <class Derived>
class CSrvStatesTask : public virtual CSrvTask
{
protected:
    /// Convenient typedef to use in state pointers.
    typedef Derived Me;
    struct State;
    /// State-implementing method typedef.
    typedef State (Me::*FStateFunc)(void);
    /// Structure behaving identically to pointer to state-implementing method.
    /// It's necessary because without it declaration of FStateFunc is
    /// impossible.
    struct State {
        FStateFunc func;

        State(void) : func(NULL) {}
        State(FStateFunc func) : func(func) {}
        State& operator= (FStateFunc func_) {func = func_; return *this;}
        bool operator== (FStateFunc func_) {return func == func_;}
        bool operator!= (FStateFunc func_) {return func != func_;}
    };


    CSrvStatesTask(void)
        : m_CurState(NULL)
    {}
    virtual ~CSrvStatesTask(void)
    {}
    /// Sets current state of the machine.
    void SetState(State state)
    {
        m_CurState = state.func;
    }

protected:
    /// Time slice execution for the state machine. Executes all
    /// state-implementing functions until no state change is requested.
    virtual void ExecuteSlice(TSrvThreadNum /* thr_num */)
    {
        for (;;) {
            FStateFunc next_state = (((Me*)this)->*m_CurState)().func;
            if (next_state == NULL)
                return;
            m_CurState = next_state;
#if 0
            SetRunnable();
            return;
#endif
        }
    }

private:
    /// Current state of the machine.
    FStateFunc m_CurState;
};


class CSrvTransConsumer;

struct SSrvConsList_Tag;
typedef intr::list_base_hook<intr::tag<SSrvConsList_Tag> >  TSrvConsListHook;
typedef intr::list<CSrvTransConsumer,
                   intr::base_hook<TSrvConsListHook>,
                   intr::constant_time_size<false> >        TSrvConsList;

/// Special task that has some information to give to its consumers but first
/// this information should be obtained from somewhere. After construction
/// task is in "Initial" state. When first consumer asks to perform transition
/// task becomes runnable and consumer is put in the queue for later
/// notification. If any more consumers ask for transition they are put in the
/// queue for notification without disrupting transition process. When
/// ExecuteSlice() has finished doing transition state of the task becomes
/// "Final" and all consumers waiting in the queue are notified (become
/// runnable). Consumers asking for transition after that get notified right
/// away and state of the task never changes and remains "Final".
///
/// Virtual inheritance from CSrvTask is used to allow derived classes to
/// combine several task types together (such as CSrvSocketTask,
/// CSrvStatesTask etc.) and still have one implementation of CSrvTask.
class CSrvTransitionTask : public virtual CSrvTask
{
public:
    CSrvTransitionTask(void);
    virtual ~CSrvTransitionTask(void);

    /// Requests task's state transition with the provided consumer to be
    /// notified when transition is finished.
    void RequestTransition(CSrvTransConsumer* consumer);
    /// Cancel transition request from the provided consumer.
    void CancelTransRequest(CSrvTransConsumer* consumer);

protected:
    /// Finish the transition process and change task's state from "Transition"
    /// to "Final" with notification sent to all consumers. Method must be
    /// called from inside ExecuteSlice() of this task.
    void FinishTransition(void);
    /// Checks if "Final" state was already reached.
    bool IsTransStateFinal(void);

private:
    /// Three possible states of the task.
    enum ETransState {
        eState_Initial,
        eState_Transition,
        eState_Final
    };

    /// Mutex to protect state changing.
    CMiniMutex m_TransLock;
    /// Current state of the task.
    ETransState m_TransState;
    /// Consumers waiting for transition to complete.
    TSrvConsList m_Consumers;
};


/// Consumer of notification about transition completeness in
/// CSrvTransitionTask.
///
/// Virtual inheritance from CSrvTask is used to allow derived classes to
/// combine several task types together (such as CSrvSocketTask,
/// CSrvStatesTask etc.) and still have one implementation of CSrvTask.
class CSrvTransConsumer : public virtual CSrvTask,
                          public TSrvConsListHook
{
public:
    CSrvTransConsumer(void);
    virtual ~CSrvTransConsumer(void);

    /// Check if transition completeness was consumed. Derived classes must
    /// use this method (instead of CSrvTransitionTask::IsTransStateFinal())
    /// because it returns TRUE not only when transition has been finished
    /// but also when this consumer was notified about that. Otherwise it will
    /// be possible that other thread finished transition and this thread will
    /// try to request transition on another task but it will unable to put
    /// this task to m_Consumers list as it wasn't removed from m_Consumers
    /// list of first task yet.
    bool IsTransFinished(void);

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    /// Flag showing that transition was already consumed.
    bool m_TransFinished;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_TASKS__HPP */
