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
*   thread pool test
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/thread_pool.hpp>

#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbitime.hpp>
#include <util/random_gen.hpp>

USING_NCBI_SCOPE;

#define MY_LOG_POST(message)                              \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO,          \
      eDiag_Error,                                        \
      eDPF_Log).GetRef()                                  \
      << CTime(CTime::eCurrent).AsString("M/D/Y h:m:s.l") \
      << "  " << message                                  \
      << NCBI_NS_NCBI::Endm )


const int kTasksPerThread = 120;
const int kQueueSize      = 20;
const int kMaxThreads     = 20;

enum EActionType {
    eAddTask,
    eAddExclusiveTask,
    eCancelTask,
    eFlushWaiting,
    eFlushNoWait,
    eCancelAll
};

enum ECheckCancelType {
    eSimpleWait,
    eCheckCancel
};

CRandom                           s_RNG;
CAtomicCounter                    s_SerialNum;
CThreadPool*                      s_Pool;
CStopWatch                        s_Timer;

vector<EActionType>               s_Actions;
vector<int>                       s_ActionTasks;
vector<ECheckCancelType>          s_CancelTypes;
vector<int>                       s_WaitPeriods;
vector<int>                       s_PostTimes;
vector< CRef<CThreadPool_Task> >  s_Tasks;


class CThreadPoolTester : public CThreadedApp
{
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool Thread_Run(int idx);
};

bool CThreadPoolTester::TestApp_Init(void)
{
    s_Pool = new CThreadPool(kQueueSize, kMaxThreads);

    s_RNG.SetSeed(CProcess::GetCurrentPid());

    if (s_NumThreads > kQueueSize) {
        s_NumThreads = kQueueSize;
    }

    int total_cnt = kTasksPerThread * s_NumThreads;
    s_Actions.resize(total_cnt + 1);
    s_ActionTasks.resize(total_cnt + 1);
    s_CancelTypes.resize(total_cnt + 1);
    s_WaitPeriods.resize(total_cnt + 1);
    s_PostTimes.resize(total_cnt + 1);
    s_Tasks.resize(total_cnt + 1);

    int req_num = 0;
    for (int i = 1; i <= total_cnt; ++i) {
        int rnd = s_RNG.GetRand(1, 1000);
        if (req_num <= 10 || rnd > 100) {
            s_Actions[i] = eAddTask;
            s_ActionTasks[i] = req_num;
            s_WaitPeriods[req_num] = s_RNG.GetRand(50, 100);
            rnd = s_RNG.GetRand(0, 100);
            s_CancelTypes[req_num] = (rnd <= 1? eSimpleWait: eCheckCancel);
            ++req_num;
        }
        else if (rnd <= 1) {
            s_Actions[i] = eFlushWaiting;
        }
        else if (rnd <= 2) {
            s_Actions[i] = eFlushNoWait;
        }
        else if (rnd <= 4) {
            s_Actions[i] = eAddExclusiveTask;
            s_ActionTasks[i] = req_num;
            ++req_num;
        }
        else if (rnd <= 6) {
            s_Actions[i] = eCancelAll;
        }
        else /* if (rnd <= 100) */ {
            s_Actions[i] = eCancelTask;
            s_ActionTasks[i] = req_num - 8;
        }
        s_PostTimes[i] = 100 * (i / 20) + s_RNG.GetRand(50, 100);
    }

    MY_LOG_POST("Starting test for CThreadPool");

    s_Timer.Start();

    return true;
}

bool CThreadPoolTester::TestApp_Exit(void)
{
    MY_LOG_POST("Destroying pool");
    delete s_Pool;
    s_Pool = NULL;
    MY_LOG_POST("Exiting from app");
    MY_LOG_POST("Test for CThreadPool is finished");

    return true;
}

class CTestTask : public CThreadPool_Task
{
public:
    CTestTask(int i) {
        m_Serial = i;
        s_Tasks[i] = Ref<CThreadPool_Task>(this);
    }

protected:
    EStatus Execute(void);

private:
    int m_Serial;
};

CTestTask::EStatus CTestTask::Execute(void)
{
    int duration = s_WaitPeriods[m_Serial];
    CStopWatch timer(CStopWatch::eStart);

    if (s_CancelTypes[m_Serial] == eCheckCancel) {
        MY_LOG_POST("Task " << m_Serial << ": " << duration << " with checking");
        for (int i = 0; i < 10; ++i) {
            SleepMicroSec(duration * 100);
            if (IsCancelRequested()) {
                MY_LOG_POST("Task " << m_Serial << " was cancelled");
                return eCanceled;
            }
        }
    }
    else {
        MY_LOG_POST("Task " << m_Serial << ": " << duration);
        SleepMilliSec(duration);
    }

    if (IsCancelRequested()) {
        MY_LOG_POST("Task " << m_Serial
                    << " was cancelled without check (time spent - "
                    << timer.Elapsed() << ")");
        return eCanceled;
    }
    else {
        MY_LOG_POST("Task " << m_Serial << " complete (time spent - "
                    << timer.Elapsed() << ")");
        return eCompleted;
    }
}


class CExclusiveTask : public CThreadPool_Task
{
public:
    CExclusiveTask(int i) {
        m_Serial = i;
        s_Tasks[i] = Ref<CThreadPool_Task>(this);
    }

protected:
    EStatus Execute(void);

private:
    int m_Serial;
};

CExclusiveTask::EStatus CExclusiveTask::Execute(void)
{
    MY_LOG_POST("Task " << m_Serial << ": exclusive running");
    return eCompleted;
}


bool CThreadPoolTester::Thread_Run(int idx)
{
    bool status = true;

    try {
        for (int i = 0;  i < kTasksPerThread;  ++i) {
            int serial_num = s_SerialNum.Add(1);
            double need_time = double(s_PostTimes[serial_num]) / 1000;
            double now_time = s_Timer.Elapsed();
            if (now_time < need_time) {
                SleepMicroSec(int((need_time - now_time) * 1000000));
            }

            int req_num = s_ActionTasks[serial_num];
            switch (s_Actions[serial_num]) {
            case eAddTask:
                MY_LOG_POST("Task " << req_num << " to be queued");
                s_Pool->AddTask(new CTestTask(req_num));
                MY_LOG_POST("Task " << req_num << " queued");
                break;

            case eAddExclusiveTask:
                MY_LOG_POST("Task " << req_num << " to be queued");
                s_Pool->RequestExclusiveExecution(new CExclusiveTask(req_num),
                                CThreadPool::fFlushThreads
                                + CThreadPool::fCancelExecutingTasks
                                + CThreadPool::fCancelQueuedTasks);
                MY_LOG_POST("Task " << req_num << " queued");
                break;

            case eCancelTask:
                while (!s_Tasks[req_num]) {
                    SleepMilliSec(10);
                }
                MY_LOG_POST("Task " << req_num << " to be cancelled");
                s_Pool->CancelTask(s_Tasks[req_num]);
                MY_LOG_POST("Cancelation of task " << req_num << " requested");
                break;

            case eFlushWaiting:
            case eFlushNoWait:
                MY_LOG_POST("Flushing threads with "
                            << (s_Actions[serial_num] == eFlushWaiting?
                                    "waiting": "immediate restart"));
                s_Pool->FlushThreads(
                                s_Actions[serial_num] == eFlushWaiting?
                                        CThreadPool::eWaitToFinish:
                                        CThreadPool::eStartImmediately);
                MY_LOG_POST("Flushing process began");
                break;

            case eCancelAll:
                MY_LOG_POST("Cancelling all tasks");
                s_Pool->CancelTasks(CThreadPool::fCancelExecutingTasks
                                     + CThreadPool::fCancelQueuedTasks);
                MY_LOG_POST("Cancellation of all tasks requested");
                break;
            }
        }
    } STD_CATCH_ALL("CThreadPoolTester: status " << (status = false))

    return status;
}

int main(int argc, const char* argv[])
{
    putenv("LOG_TRUNCATE=yes");
    return CThreadPoolTester().AppMain(argc, argv, 0, eDS_Default, 0);
}
