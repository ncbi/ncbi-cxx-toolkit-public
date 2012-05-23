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
 * Authors:  Denis Vakatov, Vladimir Ivanov, Victor Joukov
 *
 * File Description:
 *   Test for request test control classes.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_control.hpp>


/** @addtogroup Utility
 *
 * @{
 */

BEGIN_NCBI_SCOPE


CRequestRateControl::CRequestRateControl(
        unsigned int    num_requests_allowed,
        CTimeSpan       per_period,
        CTimeSpan       min_time_between_requests,
        EThrottleAction throttle_action,
        EThrottleMode   throttle_mode)
{
    Reset(num_requests_allowed, per_period, min_time_between_requests,
          throttle_action, throttle_mode);
}


void CRequestRateControl::Reset(
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


bool CRequestRateControl::x_Approve(EThrottleAction action, CTimeSpan *sleeptime)
{
    if ( sleeptime ) {
        *sleeptime = CTimeSpan(0,0);
    }
    // Is throttler disabled, that always approve request
    if ( m_NumRequestsAllowed == kNoLimit ) {
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
                        CRequestRateControlException, eNumRequestsMax, 
                        "CRequestRateControl::Approve(): "
                        "Maximum number of requests exceeded"
                    );
                case eDefault: ;
            }
        }
    }

    // Special case for eDiscrete mode and empty time between requests.
    // We don't need to get time marks in this case, just increase number
    // of requests and approve it.
    if ( m_Mode == eDiscrete  &&  !empty_period  &&  empty_between  &&
         m_NumRequests < m_NumRequestsAllowed
         ) {
        if (m_TimeLine.size() == 0) {
            // Save only first request time, used in x_CleanTimeLine()
            TTime now = m_StopWatch.Elapsed();
            m_TimeLine.push_back(now);
            // We will not update m_LastApproved except first time,
            // we don't needed this information in this case.
            m_LastApproved = now;
        }
        m_NumRequests++;
        // Approve request
        return true;
    }

    // Get current time
    TTime now = m_StopWatch.Elapsed();
    TTime x_sleeptime = 0;

    // Check number of requests per period
    if ( !empty_period ) {
        x_CleanTimeLine(now);
        if ( m_Mode == eContinuous ) {
            m_NumRequests = (unsigned int)m_TimeLine.size();
        }
        if ( m_NumRequests >= m_NumRequestsAllowed ) {
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
                        CRequestRateControlException,
                        eNumRequestsPerPeriod, 
                        "CRequestRateControl::Approve(): "
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
                        CRequestRateControlException,
                        eMinTimeBetweenRequests, 
                        "CRequestRateControl::Approve(): The time "
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


void CRequestRateControl::Sleep(CTimeSpan sleep_time)
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
             sleep_time.GetNanoSecondsAfterSecond() / 1000;
        if (sleep_time.GetNanoSecondsAfterSecond() % 1000) ms++;
        SleepMicroSec(ms);
    }
}


void CRequestRateControl::x_CleanTimeLine(TTime now)
{
    switch (m_Mode) {

    case eContinuous: {
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
        break;
    }
    case eDiscrete: {
        if (m_TimeLine.size() > 0) {
            if (now - m_TimeLine.front() > m_PerPeriod) {
                // Period ends, remove all restrictions
                m_LastApproved = -1;
                m_TimeLine.clear();
                m_NumRequests = 0;
            }
        }
        break;
    }
    } // switch
}


const char* CRequestRateControlException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNumRequestsMax:         return "eNumRequestsMax";
    case eNumRequestsPerPeriod:   return "eNumRequestsPerPeriod";
    case eMinTimeBetweenRequests: return "eMinTimeBetweenRequests";
    default:                      return CException::GetErrCodeString();
    }
}


/* @} */

END_NCBI_SCOPE
