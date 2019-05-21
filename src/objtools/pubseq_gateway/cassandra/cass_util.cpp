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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 * Utility functions
 *
 */

#include <ncbi_pch.hpp>
#include <sys/time.h>
#include <objtools/pubseq_gateway/impl/cassandra/cass_util.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

int64_t gettime(void)
{
    int cnt = 100;
    struct timeval tv = {0, 0};

    while (gettimeofday(&tv, NULL) != 0 && cnt-- > 0) {}
    return (int64_t)tv.tv_usec + ((int64_t)tv.tv_sec) * 1000000L;
}

const string kDtFormat = "Y-M-D h:m:s.lZ";

string TimeTmsToString(int64_t time) {
    CTime t;
    TimeTmsToCTime(time, &t);
    return t.AsString(kDtFormat);
}

int64_t StringToTimeTms(const string& time) {
    try {
        CTime t(time, kDtFormat);
        return CTimeToTimeTms(t);
    }
    catch (CTimeException& e) {
    }
    CTime t(time, "Y/M/D h:m:g o");
    return CTimeToTimeTms(t);
}

int64_t CTimeToTimeTms(const CTime& time) {
    if (time.IsEmptyDate())
        return 0;
    if (time.IsUniversalTime())
        return time.GetTimeT() * 1000L + time.MilliSecond();
    struct tm t, t2;
    t.tm_sec   = time.Second();
    t.tm_min   = time.Minute();
    t.tm_hour  = time.Hour();
    t.tm_mday  = time.Day();
    t.tm_mon   = time.Month() - 1;
    t.tm_year  = time.Year() - 1900;
    t.tm_isdst = -1;

    time_t rv = mktime(&t);
    time_t rv_off = rv - 3600;
    
    localtime_r(&rv_off, &t2);
    if (t2.tm_hour == t.tm_hour)
        rv = rv_off;

    return (rv * 1000L + time.MilliSecond());
}

void TimeTmsToCTime(int64_t time, CTime* ct) {
    time_t time_sec = time / 1000L;
    time_t time_msec = time % 1000L;
    if (time_msec < 0) {
        time_msec += 1000L;
        --time_sec;
    }
    if (ct) {
        ct->SetTimeT(time_sec);
        ct->SetMilliSecond(time_msec);
    }
}

CTime TimeTmsToCTime(int64_t time) {
    CTime rv;
    TimeTmsToCTime(time, &rv);
    return rv;
}

END_IDBLOB_SCOPE
