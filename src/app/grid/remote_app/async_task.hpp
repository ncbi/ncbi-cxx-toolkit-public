#ifndef __REMOTE_APP_ASYNC_TASK__HPP
#define __REMOTE_APP_ASYNC_TASK__HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <list>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE

template <class TTask>
class CAsyncTaskProcessor
{
    // Context holds all data that is shared between Scheduler and Executor.
    // Also, it actually implements both these actors
    class CContext
    {
    public:
        CContext(int s, int m)
            : sleep(s > 1 ? s : 1), // Sleep at least one second between executions
              max_attempts(m),
              stop(false)
        {}

        bool Enabled() const { return max_attempts > 0; }

        bool SchedulerImpl(TTask);
        void ExecutorImpl();

        void ExecutorImplStop()
        {
            CMutexGuard guard(lock);
            stop = true;
            cond.SignalSome();
        }

    private:
        typedef list<pair<int, TTask>> TTasks;
        typedef typename TTasks::iterator TTasks_I;

        bool FillBacklog(TTasks_I&);

        CMutex lock;
        CConditionVariable cond;
        TTasks tasks;
        TTasks backlog;
        const unsigned sleep;
        const int max_attempts;
        bool stop;
    };

    // Executor runs async tasks (in a separate thread)
    class CExecutor
    {
        struct SThread : public CThread
        {
            SThread(CContext& context, string thread_name)
                : m_Context(context),
                m_ThreadName(move(thread_name))
            {}

            void Stop()
            {
                try {
                    if (Discard()) return;

                    m_Context.ExecutorImplStop();
                    Join();
                } STD_CATCH_ALL("Exception in CExecutor::SThread::Stop()")
            }

        private:
            // This is the only method called in a different thread
            void* Main(void)
            {
                SetCurrentThreadName(m_ThreadName);
                m_Context.ExecutorImpl();
                return NULL;
            }

            CContext& m_Context;
            const string m_ThreadName;
        };

    public:
        CExecutor(CContext& context, string thread_name)
            : m_Thread(context.Enabled() ? new SThread(context, move(thread_name)) : nullptr)
        {}

        void Start() { if (m_Thread) m_Thread->Run(); }
        ~CExecutor() { if (m_Thread) m_Thread->Stop(); }

    private:
        SThread* m_Thread;
    };

public:
    // Scheduler gives tasks to Executor
    class CScheduler
    {
    public:
        bool operator()(TTask task)
        {
            return m_Context.SchedulerImpl(task);
        }

    private:
        CScheduler(CContext& context) : m_Context(context) {}

        CContext& m_Context;

        friend class CAsyncTaskProcessor;
    };

    CAsyncTaskProcessor(int sleep, int max_attempts, string thread_name);

    CScheduler& GetScheduler() { return m_Scheduler; }
    void StartExecutor() { m_Executor.Start(); }

private:
    CContext m_Context;
    CScheduler m_Scheduler;
    CExecutor m_Executor;
};

template <class TTask>
inline bool CAsyncTaskProcessor<TTask>::CContext::SchedulerImpl(TTask task)
{
    if (Enabled()) {
        CMutexGuard guard(lock);
        tasks.emplace_back(0, task);
        cond.SignalSome();
        return true;
    }

    return false;
}

template <class TTask>
inline void CAsyncTaskProcessor<TTask>::CContext::ExecutorImpl()
{
    for (;;) {
        auto backlog_end = backlog.end();

        // If stop was requested
        if (!FillBacklog(backlog_end)) {
            return;
        }

        // Execute tasks from the backlog
        auto it = backlog.begin();
        while (it != backlog_end) {
            if (it->second(++it->first, max_attempts)) {
                backlog.erase(it++);
            } else {
                ++it;
            }
        }
    }
}

template <class TTask>
inline bool CAsyncTaskProcessor<TTask>::CContext::FillBacklog(TTasks_I& backlog_end)
{
    CMutexGuard guard(lock);

    while (!stop) {
        // If there are some new tasks to execute
        if (!tasks.empty()) {
            // Move them to the backlog, these only will be processed this time
            backlog_end = backlog.begin();
            backlog.splice(backlog_end, tasks);
            return true;

        // If there is nothing to do, wait for a signal
        } else if (backlog.empty()) {
            while (!cond.WaitForSignal(lock));

        // No backlog processing if there is a signal
        } else if (!cond.WaitForSignal(lock, sleep)) {
            return true;
        }
    }

    return false;
}
template <class TTask>
inline CAsyncTaskProcessor<TTask>::CAsyncTaskProcessor(int sleep, int max_attempts, string thread_name)
    : m_Context(sleep, max_attempts),
      m_Scheduler(m_Context),
      m_Executor(m_Context, move(thread_name))
{
}

END_NCBI_SCOPE

#endif
