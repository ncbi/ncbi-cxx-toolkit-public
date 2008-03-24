#ifndef UTIL___THREAD_POOL_CTRL__HPP
#define UTIL___THREAD_POOL_CTRL__HPP

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
 */

/// @file thread_pool_ctrl.hpp
/// Implementations of controllers for ThreadPool
///
///  CThreadPool_Controller_PID -- default controller of pool of threads based
///     on Proportional-Integral-Derrivative algorithm depending on number
///     of requests waiting for execution per thread in pool


#include <corelib/ncbistd.hpp>
#include <util/thread_pool.hpp>


/** @addtogroup ThreadedPools
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Entry in "error" changing history
/// Information about "error" in some point of time in the past kept in
/// CThreadPool_Control_PID.
struct SThreadPool_PID_ErrInfo
{
    /// Time of history entry
    double call_time;
    /// Value of the error
    double err;

    SThreadPool_PID_ErrInfo(double time_, double err_)
        : call_time(time_), err(err_)
    {}
};



/// Default ThreadPool controller based on Proportional-Integral-Derivative
/// algorithm. Controller looks at number of tasks waiting in the queue
/// per each thread running and adjusts number of threads with respect to
/// all coefficients set in it.
/// Implementation of the class assumes that all coefficients are set before
/// pool begin to work and controller begins to be extencively used.
/// All changing of coefficients implemented in non-threadsafe manner and if
/// they will be changed at the same time when OnEvent() is executed
/// unpredictable consequences can happen.
class CThreadPool_Controller_PID : public CThreadPool_Controller
{
public:
    /// Constructor
    /// @param max_threads
    ///   Maximum number of threads in pool
    /// @param min_threads
    ///   Minimum number of threads in pool
    CThreadPool_Controller_PID(unsigned int  max_threads,
                               unsigned int  min_threads);

    /// Set maximum number of tasks in queue per each thread
    /// The meaning of parameter is only approximate. In fact it is the
    /// coefficient in proportional part of the algorithm and adjustment for
    /// all other coefficients.
    /// By default parameter is set to 3.
    void SetQueuedTasksThreshold(double threshold);

    /// Get maximum number of tasks in queue per each thread
    ///
    /// @sa SetQueuedTasksThreshold()
    double GetQueuedTasksThreshold(void);

    /// Set maximum time (in seconds) that task can wait in queue for
    /// processing until new thread will be launched.
    /// The meaning of parameter is only approximate. In fact it is the
    /// coefficient in integral part of the algorithm and effectively if only
    /// one task will be considered then coefficient will be multiplied
    /// by number of currently running threads and currently set threshold.
    /// By default parameter is set to 0.2.
    ///
    /// @sa SetQueuedTasksThreshold()
    void SetTaskMaxQueuedTime(double queued_time);

    /// Get maximum time that task can wait in queue for processing until
    /// new thread will be launched.
    ///
    /// @sa SetTaskMaxQueuedTime()
    double GetTaskMaxQueuedTime(void);

    /// Set the time period (in seconds) for which average speed of changing
    /// of waiting tasks number is calculated.
    /// Average speed is calculated by simple division of changing in waiting
    /// tasks number during this time period per time period value (all
    /// counts of tasks are calculated per each thread).
    /// By default parameter is set to 0.3.
    void SetChangeCalcTime(double calc_time);

    /// Get the time period for which average speed of changing of waiting
    /// tasks number is calculated.
    double GetChangeCalcTime(void);

    /// Set period of prediction of number of tasks in queue
    /// The meaning of parameter is only approximate. In fact it is the
    /// coefficient in derivative part of the algorithm. Meaning of the
    /// coefficient is like this: take average speed of changing of tasks
    /// count, multiply it by this prediction time, if the resulting value
    /// is greater than threshold then new thread is needed.
    /// By default parameter is set to 0.5.
    ///
    /// @sa SetQueuedTasksThreshold()
    void SetChangePredictTime(double predict_time);

    /// Get period of prediction of number of tasks in queue
    ///
    /// @sa SetChangePredictTime()
    double GetChangePredictTime(void);

    /// Get maximum timeout for which calls to method HandleEvent() can be
    /// missing.
    ///
    /// @sa CThreadPool_Controller::GetSafeSleepTime()
    virtual CTimeSpan GetSafeSleepTime(void) const;

protected:
    /// Main method for implementation of controlling algorithm
    virtual void OnEvent(EEvent event);

private:
    /// Timer for measuring time periods
    CStopWatch                      m_Timer;
    /// History of changing of "error" value
    /// "error" - number of tasks per thread waiting in queue. Controller
    /// will try to tend this value to zero.
    deque<SThreadPool_PID_ErrInfo>  m_ErrHistory;
    /// Value of "error" integrated over all working time
    double                          m_IntegrErr;
    /// Threshold value
    /// @sa SetQueuedTasksThreshold()
    double                          m_Threshold;
    /// Integral coefficient
    /// @sa SetTaskMaxQueuedTime()
    double                          m_IntegrCoeff;
    /// Derivative coefficient
    /// @sa SetChangePredictTime()
    double                          m_DerivCoeff;
    /// Period of taking average "error" change speed
    /// @sa SetChangeCalcTime()
    double                          m_DerivTime;
};


//////////////////////////////////////////////////////////////////////////
//  All inline methods
//////////////////////////////////////////////////////////////////////////

inline
void CThreadPool_Controller_PID::SetQueuedTasksThreshold(double threshold)
{
    m_Threshold = threshold;
}

inline
double CThreadPool_Controller_PID::GetQueuedTasksThreshold(void)
{
    return m_Threshold;
}

inline
void CThreadPool_Controller_PID::SetTaskMaxQueuedTime(double queued_time)
{
    m_IntegrCoeff = queued_time;
}

inline
double CThreadPool_Controller_PID::GetTaskMaxQueuedTime(void)
{
    return m_IntegrCoeff;
}

inline
void CThreadPool_Controller_PID::SetChangeCalcTime(double calc_time)
{
    m_DerivTime = calc_time;
}

inline
double CThreadPool_Controller_PID::GetChangeCalcTime(void)
{
    return m_DerivTime;
}

inline
void CThreadPool_Controller_PID::SetChangePredictTime(double predict_time)
{
    m_DerivCoeff = predict_time;
}

inline
double CThreadPool_Controller_PID::GetChangePredictTime(void)
{
    return m_DerivCoeff;
}


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL___THREAD_POOL_CTRL__HPP */
