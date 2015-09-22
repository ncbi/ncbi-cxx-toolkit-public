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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include <corelib/ncbireg.hpp>

#include "time_man.hpp"


BEGIN_NCBI_SCOPE;


class CTZAdjuster : public CSrvTask
{
public:
    CTZAdjuster(void);
    virtual ~CTZAdjuster(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


CSrvTime s_SrvStartTime;
CSrvTime s_LastJiffyTime;
Uint8 s_CurJiffies = 0;
CSrvTime s_JiffyTime;
static time_t s_TZAdjustment = 0;
static CTZAdjuster* s_Adjuster = NULL;


static Uint4
s_InitTZAdjustment(void)
{
#ifdef NCBI_OS_LINUX
    CSrvTime srv_t = CSrvTime::Current();
    struct tm t;
    gmtime_r(&srv_t.Sec(), &t);
    t.tm_isdst = -1;
    time_t loc_time = mktime(&t);
    s_TZAdjustment = srv_t.Sec() - loc_time;

    // Return number of seconds until it's 1 second after the hour boundary
    return (60 - t.tm_min) * 60 - (t.tm_sec - 1);
#else
    return 0;
#endif
}

void
InitTime(void)
{
    s_LastJiffyTime = s_SrvStartTime = CSrvTime::Current();
    s_InitTZAdjustment();
}

void
InitTimeMan(void)
{
    s_Adjuster = new CTZAdjuster();
    s_Adjuster->SetRunnable();
}

void
ConfigureTimeMan(CNcbiRegistry* reg, CTempString section)
{
    Uint4 clock_freq = Uint4(reg->GetInt(section, "jiffies_per_sec", 100));
    s_JiffyTime.NSec() = kUSecsPerSecond * kNSecsPerUSec / clock_freq;
}

void
IncCurJiffies(void)
{
    s_LastJiffyTime = CSrvTime::Current();
    ++s_CurJiffies;
}

static inline void
s_Print1Dig(char*& buf, int num)
{
    *(buf++) = char(num + '0');
}

static inline void
s_Print2Digs(char*& buf, int num)
{
    int hi = num / 10;
    s_Print1Dig(buf, hi);
    s_Print1Dig(buf, num - hi * 10);
}

static inline void
s_Print3Digs(char*& buf, int num)
{
    int hi = num / 100;
    s_Print1Dig(buf, hi);
    s_Print2Digs(buf, num - hi * 100);
}

static inline void
s_Print4Digs(char*& buf, int num)
{
    int hi = num / 100;
    s_Print2Digs(buf, hi);
    s_Print2Digs(buf, num - hi * 100);
}

static inline void
s_Print6Digs(char*& buf, int num)
{
    int hi = num / 100;
    s_Print4Digs(buf, hi);
    s_Print2Digs(buf, num - hi * 100);
}

Uint1
CSrvTime::Print(char* buf, EFormatType fmt) const
{
    char* start_buf = buf;

#ifdef NCBI_OS_LINUX
    time_t sec = tv_sec + s_TZAdjustment;
    struct tm t;
    gmtime_r(&sec, &t);

    switch (fmt) {
    default:
        SRV_LOG(Error, "Unsupported time format: " << fmt);
        // no break
    case eFmtLogging:
        s_Print4Digs(buf, t.tm_year + 1900);
        *(buf++) = '-';
        s_Print2Digs(buf, t.tm_mon + 1);
        *(buf++) = '-';
        s_Print2Digs(buf, t.tm_mday);
        *(buf++) = 'T';
        s_Print2Digs(buf, t.tm_hour);
        *(buf++) = ':';
        s_Print2Digs(buf, t.tm_min);
        *(buf++) = ':';
        s_Print2Digs(buf, t.tm_sec);
        *(buf++) = '.';
        s_Print6Digs(buf, int(tv_nsec / kNSecsPerUSec));
        break;
    case eFmtJson:
        if ( tv_sec == 0 && tv_nsec == 0) {
            *(buf++) = 'n';
            *(buf++) = 'u';
            *(buf++) = 'l';
            *(buf++) = 'l';
            break;
        } else {
            *(buf++) = '\"';
        }
    case eFmtHumanSeconds:
    case eFmtHumanUSecs:
        s_Print2Digs(buf, t.tm_mon + 1);
        *(buf++) = '/';
        s_Print2Digs(buf, t.tm_mday);
        *(buf++) = '/';
        s_Print4Digs(buf, t.tm_year + 1900);
        *(buf++) = ' ';
        s_Print2Digs(buf, t.tm_hour);
        *(buf++) = ':';
        s_Print2Digs(buf, t.tm_min);
        *(buf++) = ':';
        s_Print2Digs(buf, t.tm_sec);
        if (fmt == eFmtHumanUSecs) {
            *(buf++) = '.';
            s_Print6Digs(buf, int(tv_nsec / kNSecsPerUSec));
        }
        if (fmt == eFmtJson) {
            *(buf++) = '\"';
        }
        break;
    }
#endif

    *buf = '\0';
    return Uint1(buf - start_buf);
}

int
CSrvTime::TZAdjustment(void)
{
    return int(s_TZAdjustment);
}


CTZAdjuster::CTZAdjuster(void)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CTZAdjuster";
#endif
}

CTZAdjuster::~CTZAdjuster(void)
{}

void
CTZAdjuster::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// on one second after an hour, calculates difference with GMT time
    Uint4 delay = s_InitTZAdjustment();
    if (!CTaskServer::IsInShutdown())
        RunAfter(delay);
}

END_NCBI_SCOPE;
