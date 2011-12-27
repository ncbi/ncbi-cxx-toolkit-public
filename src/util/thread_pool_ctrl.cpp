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
*   Implementations of controllers for ThreadPool.
*/


#include <ncbi_pch.hpp>
#include <util/thread_pool_ctrl.hpp>


BEGIN_NCBI_SCOPE


CThreadPool_Controller_PID::CThreadPool_Controller_PID
(
    unsigned int max_threads,
    unsigned int min_threads
)
  : CThreadPool_Controller(max_threads, min_threads),
    m_Timer(CStopWatch::eStart),
    m_IntegrErr(0),
    m_Threshold(3),
    m_IntegrCoeff(0.2),
    m_DerivCoeff(0.5),
    m_DerivTime(0.3)
{
    m_ErrHistory.push_back(SThreadPool_PID_ErrInfo(0, 0));
}

void
CThreadPool_Controller_PID::OnEvent(EEvent event)
{
    if (event == eSuspend) {
        return;
    }

    // All reads below are atomic reads of one variable, thus they cannot
    // return bad results. They can be a little bit inconsistent with each
    // other because of races with other threads but that's okay for the
    // purposes of this controller.
    unsigned int threads_count = GetPool()->GetThreadsCount();
    unsigned int queued_tasks  = GetPool()->GetQueuedTasksCount();
    unsigned int run_tasks     = GetPool()->GetExecutingTasksCount();

    if (threads_count == 0) {
        EnsureLimits();
        threads_count = GetMinThreads();

        // Special case when MinThreads == 0
        if (threads_count == 0) {
            if (queued_tasks == 0) {
                return;
            }

            threads_count = 1;
            SetThreadsCount(threads_count);
        }
    }

    double now_err = (double(queued_tasks + run_tasks) - threads_count)
                            / threads_count;
    double now_time = m_Timer.Elapsed();

    if (event == eResume) {
        // When we resuming we need to avoid panic because of big changes in
        // error value. So we will assume that current error value was began
        // long time ago and didn't change afterwards.
        m_ErrHistory.clear();
        m_ErrHistory.push_back(SThreadPool_PID_ErrInfo(now_time - m_DerivTime,
                                                       now_err));
    }

    double period = now_time - m_ErrHistory.back().call_time;

    if (now_err < 0  &&  threads_count == GetMinThreads()
        &&  m_IntegrErr <= 0)
    {
        now_err = 0;
    }

    double integr_err = m_IntegrErr + (now_err + m_ErrHistory.back().err) / 2
                                       * period / m_IntegrCoeff;

    while (m_ErrHistory.size() > 1
           &&  now_time - m_ErrHistory[1].call_time >= m_DerivTime)
    {
        m_ErrHistory.pop_front();
    }
    if (now_time - m_ErrHistory.back().call_time >= m_DerivTime / 10) {
        m_ErrHistory.push_back(SThreadPool_PID_ErrInfo(now_time, now_err));
        if (threads_count == GetMaxThreads()  &&  integr_err > m_Threshold) {
            m_IntegrErr = m_Threshold;
        }
        else if (threads_count == GetMinThreads()
                 &&  integr_err < -m_Threshold)
        {
            m_IntegrErr = -m_Threshold;
        }
        else {
            m_IntegrErr = integr_err;
        }
    }

    double deriv_err = (now_err - m_ErrHistory[0].err)
                        / m_DerivTime * m_DerivCoeff;

    double final_val = (now_err + integr_err + deriv_err) / m_Threshold;
/*
    LOG_POST(CTime(CTime::eCurrent).AsString("M/D/Y h:m:s.l").c_str()
             << "  count=" << threads_count << ", queued=" << queued_tasks
             << ", run=" << run_tasks << ", err=" << now_err << ", time=" << now_time
             << ", intErr=" << m_IntegrErr << ", derivErr=" << deriv_err
             << ", final=" << final_val << ", hist_size=" << m_ErrHistory.size());
*/
    if (final_val >= 1  ||  final_val <= -1) {
        if (final_val < 0 && -final_val > threads_count)
            SetThreadsCount(GetMinThreads());
        else
            SetThreadsCount(threads_count + int(final_val));
    }
    else {
        EnsureLimits();
    }
}

CTimeSpan
CThreadPool_Controller_PID::GetSafeSleepTime(void) const
{
    double last_err = 0, integr_err = 0;
    CThreadPool* pool = GetPool();
    if (!pool) {
        return CTimeSpan(0, 0);
    }
    {{
        CMutexGuard guard(GetMainPoolMutex(pool));

        if (m_ErrHistory.size() == 0) {
            return CThreadPool_Controller::GetSafeSleepTime();
        }

        last_err = m_ErrHistory.back().err;
        integr_err = m_IntegrErr;
    }}

    unsigned int threads_cnt = pool->GetThreadsCount();
    if (last_err == 0
        ||  (last_err > 0  &&  threads_cnt == GetMaxThreads())
        ||  (last_err < 0  &&  threads_cnt == GetMinThreads()))
    {
        return CThreadPool_Controller::GetSafeSleepTime();
    }

    double sleep_time = 0;
    if (last_err > 0) {
        sleep_time = (m_Threshold - last_err - integr_err)
                     * m_IntegrCoeff / last_err;
    }
    else {
        sleep_time = (-m_Threshold - last_err - integr_err)
                     * m_IntegrCoeff / last_err;
    }
    if (sleep_time < 0)
        sleep_time = 0;

    return CTimeSpan(sleep_time);
}


END_NCBI_SCOPE
