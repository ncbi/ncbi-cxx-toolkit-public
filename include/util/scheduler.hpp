#ifndef UTIL___SCHEDULER__HPP
#define UTIL___SCHEDULER__HPP

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
 */

/// @file scheduler.hpp
///   Scheduler-related classes
///

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiobj.hpp>

#include <vector>


BEGIN_NCBI_SCOPE


/// General interface for scheduled tasks
/// @note
///   The user task class implementation must be derived from CObject
class NCBI_XUTIL_EXPORT IScheduler_Task
{
public:
    /// Method for the scheduled task execution
    virtual void Execute(void) = 0;

    /// Pure virtual destructor
    virtual ~IScheduler_Task(void) {}
};



//
class IScheduler;



/// Interface to get notifications about selected changes in the scheduler
/// timeline
///
/// @sa IScheduler::RegisterListener()
class NCBI_XUTIL_EXPORT IScheduler_Listener
{
public:
    /// This callback method is called by the scheduler whenever the time
    /// of earliest scheduled task execution gets changed
    ///
    /// @sa IScheduler::GetNextExecutionTime()
    virtual void OnNextExecutionTimeChange(IScheduler* scheduler) = 0;

    /// Pure virtual destructor
    virtual ~IScheduler_Listener(void) {}
};



/// Type of task identifier
typedef unsigned int TScheduler_TaskID;


/// Information about scheduled task
struct NCBI_XUTIL_EXPORT SScheduler_TaskInfo
{
    /// Identifier of the task
    TScheduler_TaskID       id;

    /// Smart-pointer to the task
    CIRef<IScheduler_Task>  task;
};



/// Task scheduler interface

class NCBI_XUTIL_EXPORT IScheduler
{
public:
    /// Create a stock MT-safe scheduler
    static CIRef<IScheduler> Create(void);

    /// Schedule task for one-time execution
    ///
    /// @param task
    ///   Task to execute
    /// @param exec_time
    ///   Time when the task should be executed
    virtual
    TScheduler_TaskID AddTask(IScheduler_Task*  task,
                              const CTime&      exec_time) = 0;


    /// How to run repetitive tasks
    /// @sa AddRepetitiveTask()
    enum ERepeatPattern {
        /// Execute tasks in the specified period of time after the *START*
        /// of previous task's execution
        eWithRate,
        /// Execute tasks in the specified period of time after the *END*
        /// of previous task's execution
        eWithDelay
    };


    /// Schedule task for repetitive execution
    ///
    /// @param task
    ///   Task to execute
    /// @param start_time
    ///   When to start repetitive task executions
    /// @param rate_period
    ///   Time period between the executions
    virtual
    TScheduler_TaskID AddRepetitiveTask(IScheduler_Task*  task,
                                        const CTime&      start_time,
                                        const CTimeSpan&  period,
                                        ERepeatPattern    repeat_pattern) = 0;


    /// Unschedule task
    /// @note
    ///   Do nothing if there is no task with this ID in the scheduler queue
    virtual
    void RemoveTask(TScheduler_TaskID task_id) = 0;


    /// Get list of all scheduled tasks
    virtual
    vector<SScheduler_TaskInfo> GetScheduledTasks(void) const = 0;


    /// Add listener which will be notified whenever the time of the earliest
    /// scheduled execution changes
    ///
    /// @sa GetNextExecutionTime(), UnregisterListener()
    virtual
    void RegisterListener(IScheduler_Listener* listener) = 0;


    /// Remove scheduler listener
    ///
    /// @sa RegisterListener()
    virtual
    void UnregisterListener(IScheduler_Listener* listener) = 0;


    /// Get next time point when scheduler will be ready to execute a task
    /// @note
    ///   The returned time can be in the past
    virtual
    CTime GetNextExecutionTime(void) const = 0;


    /// Check if there are tasks ready to be executed
    /// @param now
    ///   Moment in time against which to check
    virtual
    bool HasTasksToExecute(const CTime& now) const = 0;


    /// Get the next task that is ready to execute
    /// @param now
    ///   Moment in time against which to check
    /// @note
    ///   You must(!) call TaskFinished() when this task has finished executing
    /// @note
    ///   If there are no tasks to execute at the specified time, then
    ///   return {id=0, task=NULL}
    /// @sa TaskFinished()
    virtual
    SScheduler_TaskInfo GetNextTaskToExecute(const CTime& now) = 0;


    /// This method must be called when the task execution has finished.
    /// The scheduler will:
    ///  - if it's a one-time task, then remove the task for good;
    ///  - if it's a repetitive and "eWithDelay" task, then schedule the task
    ///    for subsequent execution.
    ///
    /// @param task_id
    ///   Identificator of the task
    /// @param now
    ///   Time when the task has finished executing
    ///
    /// @sa AddTask(), AddRepetitiveTask(), RemoveTask()
    virtual
    void TaskFinished(TScheduler_TaskID task_id, const CTime& now) = 0;


    /// Pure virtual destructor
    virtual ~IScheduler(void) {}
};



// fwd-decl
class CScheduler_ExecThread_Impl;


/// Standalone thread to execute scheduled tasks
///
/// @example
///   CIRef<IScheduler> scheduler = IScheduler::Create();
///   CScheduler_ExecutionThread execution_thread(scheduler);
///   scheduler->AddTask(some_task, some_time);

class NCBI_XUTIL_EXPORT CScheduler_ExecutionThread
{
public:
    /// Constructor
    /// @param scheduler
    ///   Scheduler which tasks will be executed
    CScheduler_ExecutionThread(IScheduler* scheduler);

    // dtor
    virtual ~CScheduler_ExecutionThread();

private:
    /// Prohibit copying and assignment
    CScheduler_ExecutionThread(const CScheduler_ExecutionThread&);
    CScheduler_ExecutionThread& operator= (const CScheduler_ExecutionThread&);

    /// Implementation of the thread
    CScheduler_ExecThread_Impl* m_Impl;
};



END_NCBI_SCOPE


#endif  /* UTIL___SCHEDULER__HPP */
