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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>
#include <util/request_throttler.hpp>


/** @addtogroup UTIL
 *
 * @{
 */

BEGIN_NCBI_SCOPE


CRequestThrottler::CRequestThrottler(
    unsigned int    num_requests_allowed,
    CTimeSpan       per_period,
    CTimeSpan       min_time_between_requests,
    EThrottleAction throttle_action)

    : m_NumRequestsAllowed(num_requests_allowed),
      m_PerPeriod(per_period),
      m_MinTimeBetweenRequests(min_time_between_requests),
      m_ThrottleAction(throttle_action),
      m_LastApproved(CTime::eEmpty, CTime::eGmt),
      m_NumRequests(0)
{
    return;
}


bool CRequestThrottler::Approve(EThrottleAction action)
{
    // Get current time
    CTime now(CTime::eCurrent, CTime::eGmt);

    // Redefine default action
    if ( action == eDefault ) {
        action = m_ThrottleAction;
    }
    bool empty_period  = m_PerPeriod.IsEmpty();
    bool empty_between = m_MinTimeBetweenRequests.IsEmpty();
    CTimeSpan sleep_time;

    // Check maximum number of requests at all (if times not specified)
    if ( !m_NumRequestsAllowed  ||  (empty_period  &&  empty_between) ){
        if ( m_NumRequests >= m_NumRequestsAllowed ) {
            switch(action) {
                case eSleep:
                    // cannot sleep in this case, return FALSE
                case eErrCode:
                    return false;
                case eException:
                    NCBI_THROW(
                        CRequestThrottlerException, eNumRequestsMax, 
                        "CRequestThrottler::Approve(): "
                        "Maximum number of requests exceeded"
                    );
                case eDefault: ;
            }
        }
    }

    // Check number of requests per period
    if ( !empty_period ) {
        x_CleanTimeLine(now);
        if ( m_TimeLine.size() >= m_NumRequestsAllowed ) {
            switch(action) {
                case eSleep:
                    // Get sleep time
                    sleep_time = *m_TimeLine.front() + m_PerPeriod - now;
                    break;
                case eErrCode:
                    return false;
                case eException:
                    NCBI_THROW(
                        CRequestThrottlerException,
                        eNumRequestsPerPeriod, 
                        "CRequestThrottler::Approve(): "
                        "Maximum number of requests per period exceeded"
                    );
                case eDefault: ;
            }
        }
    }

    // Check time between two consecutive requests
    if ( !empty_between  &&  !m_LastApproved.IsEmpty() ) {
        if ( now - m_LastApproved < m_MinTimeBetweenRequests ) {
            switch(action) {
                case eSleep:
                    {{
                    CTimeSpan st(m_LastApproved +
                                 m_MinTimeBetweenRequests - now);
                    // Get max of 2 sleep times
                    if ( st > sleep_time ) {
                        sleep_time = st;
                    }
                    }}
                    break;
                case eErrCode:
                    return false;
                case eException:
                    NCBI_THROW(
                        CRequestThrottlerException,
                        eMinTimeBetweenRequests, 
                        "CRequestThrottler::Approve(): The time "
                        "between two consecutive requests is too short"
                    );
                case eDefault: ;
            }
        }
    }

    // eSleep case
    
    if ( !sleep_time.IsEmpty()  &&  sleep_time > CTimeSpan(0,0) ) {
        long sec = sleep_time.GetCompleteSeconds();
        // We cannot sleep that much milliseconds, round it to seconds
        if (sec > long(kMax_ULong / kMicroSecondsPerSecond)) {
            SleepSec(sec);
        } else {
            unsigned long ms;
	    ms = sec * kMicroSecondsPerSecond +
	         (sleep_time.GetNanoSecondsAfterSecond() + 500)/ 1000;
            SleepMicroSec(ms);
        }
        now.SetCurrent();
    }

    // Update stored information
    if ( !empty_period ) {
        x_AddToTimeLine(now);
    }
    m_LastApproved = now;
    m_NumRequests++;

    // Approve request
    return true;
}


void CRequestThrottler::x_AddToTimeLine(const CTime& time)
{
    m_TimeLine.push_back(TTime(new CTime(time)));
}


void CRequestThrottler::x_CleanTimeLine(CTime& now)
{
    // Find first non expired item
    TTimeLine::iterator current;
    for ( current = m_TimeLine.begin(); current != m_TimeLine.end();
          ++current) {
        if ( now - **current < m_PerPeriod) {
            break;
        }
    }
    // Erase all expired items
    m_TimeLine.erase(m_TimeLine.begin(), current);
}


/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/02 13:53:06  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
