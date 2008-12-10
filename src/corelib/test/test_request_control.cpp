/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for tmp_CRequestRateControl class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
//#include <corelib/request_control.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbitime.hpp>
#include <deque>


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//----------------------------------------------------------------------------
// Temporary debug code
//----------------------------------------------------------------------------


class tmp_CRequestRateControlException : public CCoreException
{
public:
    /// Error types that CRequestRateControl can generate.
    enum EErrCode {
        eNumRequestsMax,         ///< Maximum number of requests exceeded;
        eNumRequestsPerPeriod,   ///< Number of requests per period exceeded;
        eMinTimeBetweenRequests  ///< The time between two consecutive requests
                                 ///< is too short;
    };
    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(tmp_CRequestRateControlException, CCoreException);
};

class tmp_CRequestRateControl
{
public:
    const static unsigned int kNoLimit = kMax_UInt;

    enum EThrottleAction {
        eSleep,      ///< Sleep till the rate requirements are met & return
        eErrCode,    ///< Return immediately with err code == FALSE
        eException,  ///< Throw an exception
        eDefault     ///< in c-tor -- eSleep;  in Approve() -- value set in c-tor
    };
    enum EThrottleMode {
        eContinuous, ///< Uses float time frame to check number of requests
        eDiscrete    ///< Uses fixed time frame to check number of requests
    };

    tmp_CRequestRateControl
              (unsigned int     num_requests_allowed,
               CTimeSpan        per_period                = CTimeSpan(1,0),
               CTimeSpan        min_time_between_requests = CTimeSpan(0,0),
               EThrottleAction  throttle_action           = eDefault,
               EThrottleMode    throttle_mode             = eContinuous);
    void Reset(unsigned int     num_requests_allowed,
               CTimeSpan        per_period                = CTimeSpan(1,0),
               CTimeSpan        min_time_between_requests = CTimeSpan(0,0),
               EThrottleAction  throttle_action           = eDefault,
               EThrottleMode    throttle_mode             = eContinuous);

    bool Approve(EThrottleAction action = eDefault);
    static void Sleep(CTimeSpan sleep_time);

private:
    typedef double TTime;
    bool x_Approve(EThrottleAction action, CTimeSpan *sleeptime);
    void x_CleanTimeLine(TTime now);

private:
    // Saved parameters
    EThrottleMode    m_Mode;
    unsigned int     m_NumRequestsAllowed;
    TTime            m_PerPeriod;
    TTime            m_MinTimeBetweenRequests;
    EThrottleAction  m_ThrottleAction;

    CStopWatch       m_StopWatch;      ///< Stopwatch to measure elapsed time
    typedef deque<TTime> TTimeLine;
    TTimeLine        m_TimeLine;       ///< Vector of times of approvals
    TTime            m_LastApproved;   ///< Last approve time
    unsigned int     m_NumRequests;    ///< Num requests per period
};


bool tmp_CRequestRateControl::Approve(EThrottleAction action)
{
    return x_Approve(action, 0);
}

tmp_CRequestRateControl::tmp_CRequestRateControl(
        unsigned int    num_requests_allowed,
        CTimeSpan       per_period,
        CTimeSpan       min_time_between_requests,
        EThrottleAction throttle_action,
        EThrottleMode   throttle_mode)
{
    Reset(num_requests_allowed, per_period, min_time_between_requests,
          throttle_action, throttle_mode);
}


void tmp_CRequestRateControl::Reset(
        unsigned int    num_requests_allowed,
        CTimeSpan       per_period,
        CTimeSpan       min_time_between_requests,
        EThrottleAction throttle_action,
        EThrottleMode   throttle_mode)
{
    // Save parameters
    m_NumRequestsAllowed     = num_requests_allowed;
    m_PerPeriod              = per_period.GetAsDouble();
    m_MinTimeBetweenRequests = min_time_between_requests.GetAsDouble();
    if ( throttle_action == eDefault ) {
        m_ThrottleAction = eSleep;
    } else {
        m_ThrottleAction = throttle_action;
    }
    m_Mode = throttle_mode;

    // Reset internal state
    m_NumRequests  =  0;
    m_LastApproved = -1;
    m_TimeLine.clear();
    m_StopWatch.Restart();
}


bool tmp_CRequestRateControl::x_Approve(EThrottleAction action, CTimeSpan *sleeptime)
{
    if ( sleeptime ) {
        *sleeptime = CTimeSpan(0,0);
    }
    // Is throttler disabled, that always approve request
    if ( m_NumRequests == kNoLimit ) {
        return true;
    }
    // Redefine default action
    if ( action == eDefault ) {
        action = m_ThrottleAction;
    }

    bool empty_period  = (m_PerPeriod <= 0);
    bool empty_between = (m_MinTimeBetweenRequests <= 0);

    // Check maximum number of requests at all (if times not specified)
    if ( !m_NumRequestsAllowed  ||  (empty_period  &&  empty_between) ) {
        if ( m_NumRequests >= m_NumRequestsAllowed ) {
            switch(action) {
                case eErrCode:
                    return false;
                case eSleep:
                    // cannot sleep in this case, return FALSE
                    if ( !sleeptime ) {
                        return false;
                    }
                    // or throw exception, see ApproveTime()
                case eException:
                    NCBI_THROW(
                        tmp_CRequestRateControlException, eNumRequestsMax, 
                        "tmp_CRequestRateControl::Approve(): "
                        "Maximum number of requests exceeded"
                    );
                case eDefault: ;
            }
        }
    }

    // Get current time
    TTime now = m_StopWatch.Elapsed();
    TTime x_sleeptime = 0;

    // Check number of requests per period
    if ( !empty_period ) {
        x_CleanTimeLine(now);
        if ( m_TimeLine.size() >= m_NumRequestsAllowed ) {
            switch(action) {
                case eSleep:
                    // Get sleep time
                    _ASSERT(m_TimeLine.size() > 0);
                    x_sleeptime = m_TimeLine.front() + m_PerPeriod - now;
                    break;
                case eErrCode:
                    return false;
                case eException:
                    NCBI_THROW(
                        tmp_CRequestRateControlException,
                        eNumRequestsPerPeriod, 
                        "tmp_CRequestRateControl::Approve(): "
                        "Maximum number of requests per period exceeded"
                    );
                case eDefault: ;
            }
        }
    }
    // Check time between two consecutive requests
    if ( !empty_between  &&  (m_LastApproved >= 0) ) {
        if ( now - m_LastApproved < m_MinTimeBetweenRequests ) {
            switch(action) {
                case eSleep:
                    // Get sleep time
                    {{
                        TTime st = m_LastApproved + m_MinTimeBetweenRequests - now;
                        // Get max of two sleep times
                        if ( st > x_sleeptime ) {
                            x_sleeptime = st;
                        }
                    }}
                    break;
                case eErrCode:
                    return false;
                case eException:
                    NCBI_THROW(
                        tmp_CRequestRateControlException,
                        eMinTimeBetweenRequests, 
                        "tmp_CRequestRateControl::Approve(): The time "
                        "between two consecutive requests is too short"
                    );
                case eDefault: ;
            }
        }
    }

    // eSleep case
    
    if ( x_sleeptime > 0 ) {
        if ( sleeptime ) {
            // ApproveTime() -- request is not approved,
            // return sleeping time.
            if ( sleeptime ) {
                *sleeptime = CTimeSpan(x_sleeptime);
            }
            return false;
        } else {
            // Approve() -- sleep before approve
            Sleep(CTimeSpan(x_sleeptime));
            now = m_StopWatch.Elapsed();
        }
    }
    // Update stored information
    if ( !empty_period ) {
        m_TimeLine.push_back(now);
    }
    m_LastApproved = now;
    m_NumRequests++;

    // Approve request
    return true;
}


void tmp_SleepMicroSec(unsigned long mc_sec, EInterruptOnSignal onsignal = eRestartOnSignal)
{
LOG_POST("SleepMicroSec: " + NStr::UIntToString(mc_sec));
CStopWatch sw(CStopWatch::eStart);


#if defined(NCBI_OS_MSWIN)
    Sleep((mc_sec + 500) / 1000);
#elif defined(NCBI_OS_UNIX)
#  if defined(HAVE_NANOSLEEP)
    struct timespec delay, unslept;
    delay.tv_sec  =  mc_sec / kMicroSecondsPerSecond;
    delay.tv_nsec = (mc_sec % kMicroSecondsPerSecond) * 1000;
    while (nanosleep(&delay, &unslept) < 0) {
LOG_POST("nanosleep interrupted: errno =" + NStr::IntToString(errno));
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
            break;
        delay = unslept;
    }
#  else
    // Portable but ugly.
    // Most implementations of select() do not modifies timeout to reflect
    // the amount of time not slept; but some do this. Also, on some
    // platforms it can be interrupted by a signal, on other not.
    struct timeval delay;
    delay.tv_sec  = mc_sec / kMicroSecondsPerSecond;
    delay.tv_usec = mc_sec % kMicroSecondsPerSecond;
    while (select(0, (fd_set*) 0, (fd_set*) 0, (fd_set*) 0, &delay) < 0) {
#    if defined(SELECT_UPDATES_TIMEOUT)
LOG_POST("select interrupted: errno =" + NStr::IntToString(errno));
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
#    endif
            break;
    }
#  endif /*HAVE_NANOSLEEP*/
#endif /*NCBI_OS_...*/
    double e = sw.Elapsed();
    LOG_POST("SleepMicroSec: elapsed = " + NStr::DoubleToString(e, -1, NStr::fDoubleFixed));
}


void tmp_CRequestRateControl::Sleep(CTimeSpan sleep_time)
{
    if ( sleep_time <= CTimeSpan(0, 0) ) {
        return;
    }
    long sec = sleep_time.GetCompleteSeconds();
    // We cannot sleep that much milliseconds, round it to seconds
    if (sec > long(kMax_ULong / kMicroSecondsPerSecond)) {
        SleepSec(sec);
    } else {
        unsigned long ms;
        ms = sec * kMicroSecondsPerSecond +
            (sleep_time.GetNanoSecondsAfterSecond() + 500) / 1000;
        tmp_SleepMicroSec(ms);
    }
}


void tmp_CRequestRateControl::x_CleanTimeLine(TTime now)
{
    if ( m_Mode == eContinuous ) {
        // Find first non-expired item
        TTimeLine::iterator current;
        for ( current = m_TimeLine.begin(); current != m_TimeLine.end();
            ++current) {
            if ( now - *current < m_PerPeriod) {
                break;
            }
        }
        // Erase all expired items
        m_TimeLine.erase(m_TimeLine.begin(), current);

    } else {
        _ASSERT(m_Mode == eDiscrete);
        if (m_TimeLine.size() > 0) {
            if (now - m_TimeLine.front() > m_PerPeriod) {
                // Period ends, remove all restrictions
                m_LastApproved = -1;
                m_TimeLine.clear();
            }
        }
    }
}


const char* tmp_CRequestRateControlException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNumRequestsMax:         return "eNumRequestsMax";
    case eNumRequestsPerPeriod:   return "eNumRequestsPerPeriod";
    case eMinTimeBetweenRequests: return "eMinTimeBetweenRequests";
    default:                      return CException::GetErrCodeString();
    }
}



//----------------------------------------------------------------------------




//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
}


// Sleep little more than 'x' milliseconds to avoid race conditions,
// because SleepSec can sleep on some platforms a some microseconds
// less than needed.
#define SLEEP(x) SleepMilliSec(x+100)

// Print elapsed time and wait a little to avoid race conditions
#define ELAPSED \
    e = sw.Elapsed(); \
    LOG_POST("elapsed: " + NStr::DoubleToString(e, -1, NStr::fDoubleFixed)); \
    SleepMilliSec(100)

// Start/stop test messages
#define START(x) \
    LOG_POST("Start test " << x)
#define DONE \
    LOG_POST("DONE")


int CTest::Run(void)
{
    double e;

    START(1);
    {{
        // Forbid zero requests per any time.
        tmp_CRequestRateControl mgr(0, CTimeSpan(0,0), CTimeSpan(1,0),
                                tmp_CRequestRateControl::eErrCode);
        assert( !mgr.Approve() );
    }}
    DONE;

    START(2);
    {{
        // Allow only 1 request per any period of time.
        tmp_CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(0,0),
                                tmp_CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
    }}
    DONE;

    START(3);
    {{
        // Allow any number of requests with frequency 1 request per second
        // The behaviour is the same for both modes.
        tmp_CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                tmp_CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1000);
        assert( mgr.Approve() );
    }}
    DONE;

    START(4);
    {{
        // Allow 2 request per second with any frequency.
        // The behaviour is different for eContinuous/eDiscrete modes.
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(1,0), CTimeSpan(0,0),
                                    tmp_CRequestRateControl::eErrCode);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            // Now, will try to split up time period
            assert( mgr.Approve() );
            SLEEP(500);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(500);
            // Time period is float, so only first request will be approved.
            // See difference with eDiscrete mode below.
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
        }}
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(1,0), CTimeSpan(0,0),
                                    tmp_CRequestRateControl::eErrCode,
                                    tmp_CRequestRateControl::eDiscrete);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            // Now, will try to split up time period
            assert( mgr.Approve() );
            SLEEP(500);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(500);
            // Time period ends, so both request will be approved.
            // See difference with eContinuous mode above
            assert( mgr.Approve() );
            assert( mgr.Approve() );
        }}
    }}
    DONE;

    START(5);
    {{
        // Allow 2 request per 3 seconds with frequency 1 request per second.
        // In this test behaviour of eContinuous and eDiscrete modes are the same
        // for 3 second time interval. 
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eErrCode);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
        }}
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eErrCode,
                                    tmp_CRequestRateControl::eDiscrete);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
        }}
    }}
    DONE;

    // eSleep

    START(6);
    {{
        // Allow any number of requests with frequency 1 request per second
        // with auto sleep beetween requests.
        // The behaviour is the same for both modes.
        {{
            tmp_CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eSleep);
            CStopWatch sw(CStopWatch::eStart);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            ELAPSED;
            // The StopWatch is more accurate then Sleep methods.
            assert( e > 1.8);
        }}
        {{
            tmp_CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eSleep,
                                    tmp_CRequestRateControl::eDiscrete);
            CStopWatch sw(CStopWatch::eStart);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            ELAPSED;
            // The StopWatch is more accurate then Sleep methods.
            assert( e > 1.8); 
        }}
    }}
    DONE;

    START(7);
    {{
        // Allow 2 request per 3 seconds with frequency 1 request per second
        // with auto sleep beetween requests.
        // The behaviour is different for both modes.
        CStopWatch sw(CStopWatch::eStart);
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eSleep);
            // sleep 0
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            sw.Restart();
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            sw.Restart();
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
            sw.Restart();
            // sleep 1
            // See difference with eDiscrete mode below.
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            sw.Restart();
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
            sw.Restart();
        }}
        {{
            tmp_CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    tmp_CRequestRateControl::eSleep,
                                    tmp_CRequestRateControl::eDiscrete);
            // sleep 0
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            sw.Restart();
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            sw.Restart();
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
            sw.Restart();
            // sleep 0
            // See difference with eContinuous mode above
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            sw.Restart();
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            sw.Restart();
        }}
    }}
    DONE;

    START(8);
    {{
        // Allow any number of requests (throtling is disabled)
        {{
            tmp_CRequestRateControl mgr(tmp_CRequestRateControl::kNoLimit);
            CStopWatch sw(CStopWatch::eStart);
            for (int i=0; i<1000; i++) {
                assert( mgr.Approve() );
            }
            ELAPSED;
            assert( e < 0.1); 
        }}
    }}
    DONE;

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
