#ifndef NS_PRECISE_TIME__HPP
#define NS_PRECISE_TIME__HPP
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
 *           Slightly modified by Sergey Satskiy to fit NetSchedule
 *
 * File Description: Precise time handling
 */


#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbitime.hpp>
#include <sys/time.h>


BEGIN_NCBI_SCOPE


enum {
    kMSecsPerSecond = 1000,
    kUSecsPerMSec   = 1000,
    kNSecsPerUSec   = 1000,
    kUSecsPerSecond = kUSecsPerMSec * kMSecsPerSecond,
    kNSecsPerMSec   = kNSecsPerUSec * kUSecsPerMSec,
    kNSecsPerSecond = kNSecsPerMSec * kMSecsPerSecond
};


class CNSPreciseTime : public timespec
{
    public:
        static CNSPreciseTime  Current(void)
        { CNSPreciseTime    result;
          clock_gettime(CLOCK_REALTIME, &result);
          return result; }
        static CNSPreciseTime  Never(void)
        { CNSPreciseTime    result;
          result.tv_sec = numeric_limits<time_t>::max();
          result.tv_nsec = kNSecsPerSecond - 1;
          return result;
        }

        CNSPreciseTime(void)
        { tv_sec = 0;
          tv_nsec = 0; }
        CNSPreciseTime(time_t  sec)
        { tv_sec = sec;
          tv_nsec = 0; }
        CNSPreciseTime(double  time)
        { tv_sec = int(time);
          tv_nsec = int( (time - tv_sec) * kNSecsPerSecond ); }
        CNSPreciseTime(unsigned int sec, unsigned int  nsec)
        { tv_sec = sec;
          tv_nsec = nsec; }
        time_t &  Sec(void)
        { return tv_sec; }
        time_t Sec(void) const
        { return tv_sec; }
        long & NSec(void)
        { return tv_nsec; }
        long NSec(void) const
        { return tv_nsec; }
        int Compare(const CNSPreciseTime &  t) const
        {
            if (tv_sec < t.tv_sec)
                return -1;
            else if (tv_sec > t.tv_sec)
                return 1;
            else if (tv_nsec < t.tv_nsec)
                return -1;
            else if (tv_nsec > t.tv_nsec)
                return 1;
            return 0;
        }

        CNSPreciseTime &  operator+= (const CNSPreciseTime &  t)
        {
            tv_sec += t.tv_sec;
            tv_nsec += t.tv_nsec;
            if (tv_nsec >= kNSecsPerSecond) {
                ++tv_sec;
                tv_nsec -= kNSecsPerSecond;
            }
            return *this;
        }
        CNSPreciseTime &  operator-= (const CNSPreciseTime &  t)
        {
            tv_sec -= t.tv_sec;
            if (tv_nsec >= t.tv_nsec) {
                tv_nsec -= t.tv_nsec;
            }
            else {
                --tv_sec;
                tv_nsec += kNSecsPerSecond;
                tv_nsec -= t.tv_nsec;
            }
            return *this;
        }
        bool operator> (const CNSPreciseTime &  t) const
        { return Compare(t) > 0; }
        bool operator>= (const CNSPreciseTime &  t) const
        { return Compare(t) >= 0; }
        bool operator< (const CNSPreciseTime &  t) const
        { return Compare(t) < 0; }
        bool operator<= (const CNSPreciseTime &  t) const
        { return Compare(t) <= 0; }

        operator double () const
        { return (double)tv_sec + ((double)tv_nsec / (double)kNSecsPerSecond); }

};

inline
CNSPreciseTime operator+ (const CNSPreciseTime &  lhs,
                          const CNSPreciseTime &  rhs)
{
    CNSPreciseTime      result(lhs);
    return result += rhs;
}

inline
CNSPreciseTime operator- (const CNSPreciseTime &  lhs,
                          const CNSPreciseTime &  rhs)
{
    CNSPreciseTime      result(lhs);
    return result -= rhs;
}

inline
bool operator== (const CNSPreciseTime &  lhs,
                 const CNSPreciseTime &  rhs)
{
    // Faster, than to use <
    return lhs.tv_sec  == rhs.tv_sec &&
           lhs.tv_nsec == rhs.tv_nsec;
}

inline
bool operator!= (const CNSPreciseTime &  lhs,
                 const CNSPreciseTime &  rhs)
{
    return !(lhs == rhs);
}


inline
string NS_FormatPreciseTime(const CNSPreciseTime &  t)
{
    CTime       converted(t.Sec());
    long        usec = t.NSec() / kNSecsPerUSec;

    converted.ToLocalTime();
    if (usec == 0)
        return converted.AsString() + ".0";

    char        buffer[32];
    sprintf(buffer, "%06lu", usec);
    return converted.AsString() + "." + buffer;
}

inline
string NS_FormatPreciseTimeAsSec(const CNSPreciseTime &  t)
{
    long        usec = t.NSec() / kNSecsPerUSec;
    char        buffer[32];

    if (usec == 0) {
        sprintf(buffer, "%lu.0", t.Sec());
        return buffer;
    }

    sprintf(buffer, "%lu.%06lu", t.Sec(), usec);
    return buffer;
}


END_NCBI_SCOPE

#endif /* NS_PRECISE_TIME__HPP */

