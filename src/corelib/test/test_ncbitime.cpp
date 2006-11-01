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
 * Authors:  Anton Butanayev, Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Test for CTime - the standard Date/Time class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <stdio.h>
#include <stdlib.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


#define STR(t) string("[" + (t).AsString() + "]")


//============================================================================
//
// TestMisc
//
//============================================================================

static void s_TestMisc(void)
{
    LOG_POST("---------------------------");
    LOG_POST("Test Misc");
    LOG_POST("---------------------------\n");

    // Print current time
    {{
        CTime t(CTime::eCurrent);
        LOG_POST(STR(t));
        LOG_POST(STR(t++));
        LOG_POST(STR(t++));
        LOG_POST(STR(t++));
        LOG_POST(STR(++t));
        LOG_POST(STR(++t));
    }}

    // Month and Day name<->num conversion
    {{
        assert(CTime::MonthNameToNum("Jan")              == CTime::eJanuary); 
        assert(CTime::MonthNameToNum("January")          == 1); 
        assert(CTime::MonthNameToNum("Dec")              == CTime::eDecember); 
        assert(CTime::MonthNameToNum("December")         == 12); 
        assert(CTime::MonthNumToName(CTime::eJanuary)    == "January"); 
        assert(CTime::MonthNumToName(1, CTime::eAbbr)    == "Jan"); 
        assert(CTime::MonthNumToName(CTime::eDecember,
                                     CTime::eFull)       == "December"); 
        assert(CTime::MonthNumToName(12,CTime::eAbbr)    == "Dec"); 

        assert(CTime::DayOfWeekNameToNum("Sun")          == CTime::eSunday); 
        assert(CTime::DayOfWeekNameToNum("Sunday")       == 0); 
        assert(CTime::DayOfWeekNameToNum("Sat")          == CTime::eSaturday);
        assert(CTime::DayOfWeekNameToNum("Saturday")     == 6); 
        assert(CTime::DayOfWeekNumToName(CTime::eSunday) == "Sunday"); 
        assert(CTime::DayOfWeekNumToName(0,CTime::eAbbr) == "Sun"); 
        assert(CTime::DayOfWeekNumToName(CTime::eSaturday,
                                       CTime::eFull)     == "Saturday"); 
        assert(CTime::DayOfWeekNumToName(6,CTime::eAbbr) == "Sat"); 

        LOG_POST("Throw exception below:");
        try {
            CTime::MonthNameToNum("Month"); 
        } catch (CTimeException& e) {
            NCBI_REPORT_EXCEPTION("", e);
        }
    }}

    // String <-> CTime conversion
    {{
        {{
            CTime t;
            LOG_POST(STR(t));
            assert(t.AsString() == "");
        }}
        {{
            CTime t(2000, 365 / 2);
            CTime::SetFormat("M/D/Y h:m:s");
            LOG_POST(STR(t));
            assert(t.AsString() == "06/30/2000 00:00:00");
        }}
        {{
            CTime::SetFormat("M/D/Y");
            LOG_POST("\nYear 2000 problem:");
            CTime t(1999, 12, 30); 
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "12/31/1999");
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "01/01/2000");
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "01/02/2000");
            t="02/27/2000";
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "02/28/2000");
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "02/29/2000");
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "03/01/2000");
            t++; LOG_POST(STR(t));
            assert(t.AsString() == "03/02/2000");
        }}
        {{
            CTime::SetFormat("M/D/Y h:m:s");
            LOG_POST("\nString assignment:");
            try {
                CTime t("02/15/2000 01:12:33");
                LOG_POST(STR(t));
                assert(t.AsString() == "02/15/2000 01:12:33");
                t = "3/16/2001 02:13:34";
                LOG_POST(STR(t));
                assert(t.AsString() == "03/16/2001 02:13:34");
            } catch (CException& e) {
                NCBI_REPORT_EXCEPTION("",e);
            }
        }}
    }}

    // Addition
    {{
        CTime::SetFormat("M/D/Y h:m:s.S");
        {{
            LOG_POST("\nAdding Nanoseconds:");
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 59, 999999995);
                 tmp <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
                 t = tmp, tmp.AddNanoSecond(2)) {
                 LOG_POST(STR(tmp));
            }
            assert(t.AsString() == "01/01/2000 00:00:00.000000003");
        }}
        {{
            LOG_POST("\nCurrent time with NS (10 cicles)");
            CTime t;
            for (int i = 0; i < 10; i++) {
                 t.SetCurrent();
                 LOG_POST(STR(t));
            }
        }}

        CTime::SetFormat("M/D/Y h:m:s");
        {{
            LOG_POST("\nAdding seconds:");
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 5);
                 tmp <= CTime(2000, 1, 1, 0, 1, 20);
                 t = tmp, tmp.AddSecond(11)) {
                 LOG_POST(STR(tmp));
            }
            assert(t.AsString() == "01/01/2000 00:01:17");
        }}
        {{
            LOG_POST("\nAdding minutes:");
            for (CTime t(1999, 12, 31, 23, 45);
                 t <= CTime(2000, 1, 1, 0, 15);
                 t.AddMinute(11)) {
                 LOG_POST(STR(t));
            }
        }}
        {{
            LOG_POST("\nAdding hours:");
            for (CTime t(1999, 12, 31);
                 t <= CTime(2000, 1, 1, 15);
                 t.AddHour(11)) {
                 LOG_POST(STR(t));
            }
        }}
        {{
            LOG_POST("\nAdding months:");
            for (CTime t(1998, 12, 29);
                 t <= CTime(1999, 4, 1);
                 t.AddMonth()) {
                 LOG_POST(STR(t));
            }
        }}
        {{
            LOG_POST("\nAdding time span:");
            CTime t0(1999, 12, 31, 23, 59, 5);
            CTimeSpan ts(1, 2, 3, 4, 555555555);

            for (int i=0; i<10; i++) {
                 t0.AddTimeSpan(ts);
                 LOG_POST(STR(t0));
            }
            assert(t0.AsString() == "01/11/2000 20:29:50");

            CTime t1;
            t1 = t0 + ts;
            LOG_POST(STR(t1));
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = ts + t0;
            LOG_POST(STR(t1));
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0; t1 += ts;
            LOG_POST(STR(t1));
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0 - ts;
            LOG_POST(STR(t1));
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            t1 = t0; t1 -= ts;
            LOG_POST(STR(t1));
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            ts = t0 - t1;
            LOG_POST(STR(ts));
            assert(ts.AsString() == "93784.555555555");
            ts = t1 - t0;
            LOG_POST(STR(ts));
            assert(ts.AsString() == "-93784.555555555");
        }}
        LOG_POST("");
    }}

    // Difference
    {{
        CTime t1(2000, 10, 1, 12, 3, 45,1);
        CTime t2(2000, 10, 2, 14, 55, 1,2);
        CTimeSpan ts(1,2,51,16,1);

        LOG_POST("[" + t1.AsString() + " - " + t2.AsString() + "]");
        LOG_POST("DiffDay        = " +
                 NStr::DoubleToString(t2.DiffDay(t1),2));
        assert((t2.DiffDay(t1)-1.12) < 0.01);
        LOG_POST("DiffHour       = " +
                 NStr::DoubleToString(t2.DiffHour(t1),2));
        assert((t2.DiffHour(t1)-26.85) < 0.01);
        LOG_POST("DiffMinute     = " +
                 NStr::DoubleToString(t2.DiffMinute(t1),2));
        assert((t2.DiffMinute(t1)-1611.27) < 0.01);
        LOG_POST("DiffSecond     = " + NStr::Int8ToString(t2.DiffSecond(t1)));
        assert(t2.DiffSecond(t1) == 96676);
        LOG_POST("DiffNanoSecond = " +
                 NStr::DoubleToString(t2.DiffNanoSecond(t1),0));
        LOG_POST("DiffTimeSpan   = " + ts.AsString());
        assert(t2.DiffTimeSpan(t1) == ts);
        assert(t1.DiffTimeSpan(t2) == -ts);
    }}

    // Datebase formats conversion
    {{
        CTime t1(2000, 1, 1, 1, 1, 1, 10000000);
        CTime::SetFormat("M/D/Y h:m:s.S");

        LOG_POST("\nDB time formats " + STR(t1));

        TDBTimeU dbu = t1.GetTimeDBU();
        TDBTimeI dbi = t1.GetTimeDBI();

        LOG_POST("DBU days             = " + NStr::UIntToString(dbu.days));
        LOG_POST("DBU time (min)       = " + NStr::UIntToString(dbu.time));
        LOG_POST("DBI days             = " + NStr::UIntToString(dbi.days));
        LOG_POST("DBI time (1/300 sec) = " + NStr::UIntToString(dbi.time));
        LOG_POST("");
        CTime t2;
        t2.SetTimeDBU(dbu);
        LOG_POST("Time from DBU        = " + t2.AsString());
        t2.SetTimeDBI(dbi);
        LOG_POST("Time from DBI        = " + t2.AsString());

        CTime::SetFormat("M/D/Y h:m:s");
        dbi.days = 37093;
        dbi.time = 12301381;
        t2.SetTimeDBI(dbi);
        LOG_POST("Time from DBI        = " + t2.AsString());
        assert(t2.AsString() == "07/23/2001 11:23:24");
    }}

    // Set* functions
    {{
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t(2000, 1, 31);

        t.SetMonth(2);
        assert(t.AsString() == "02/29/2000 00:00:00");
        t.SetYear(2001);
        assert(t.AsString() == "02/28/2001 00:00:00");
        t.SetMonth(4);
        assert(t.AsString() == "04/28/2001 00:00:00");
        t.SetDay(31);
        assert(t.AsString() == "04/30/2001 00:00:00");
        t.SetHour(6);
        assert(t.AsString() == "04/30/2001 06:00:00");
        t.SetMinute(37);
        assert(t.AsString() == "04/30/2001 06:37:00");
        t.SetSecond(59);
        assert(t.AsString() == "04/30/2001 06:37:59");
    }}

    // Day of week
    {{   
        CTime t(1900, 1, 1);
        int i;
        for (i = 1; t <= CTime(2030, 12, 31); t++,i++) {
            assert(t.DayOfWeek() == (i%7));
        }
    }}

    // Number of days in the month
    {{
        CTime t(2000, 1, 31);
        assert(t.DaysInMonth() == 31);
        t.SetMonth(2);
        assert(t.DaysInMonth() == 29);
        t.SetYear(2001);
        assert(t.DaysInMonth() == 28);
        t.SetMonth(4);
        assert(t.DaysInMonth() == 30);
    }}

    // Week number in the year/month
    {{
        CTime t(1970, 1, 1);
        int i;
        char buf[3];

        time_t gt = t.GetTimeT();

        for (i = 1; t <= CTime(2030, 12, 31); i++, t++, gt += 24*3600) {
            struct tm *today = gmtime(&gt);
            assert(today != 0);
            int week_num_rtl, week_num, month_week_num;

            // Sunday-based weeks
            strftime(buf, sizeof(buf), "%U", today);
            week_num_rtl   = NStr::StringToInt(buf) + 1;
            week_num       = t.YearWeekNumber(/*CTime::eSunday*/);
            assert(week_num_rtl == week_num);
            month_week_num = t.MonthWeekNumber(/*CTime::eSunday*/);
            assert(month_week_num >= 1  &&  month_week_num <= 6);

            // Monday-based weeks
            strftime(buf, sizeof(buf), "%W", today);
            week_num_rtl   = NStr::StringToInt(buf) + 1;
            week_num       = t.YearWeekNumber(CTime::eMonday);
            assert(week_num_rtl == week_num);
            month_week_num = t.MonthWeekNumber(CTime::eMonday);
            assert(month_week_num >= 1  &&  month_week_num <= 6);
        }
    }}
}


//============================================================================
//
// TestFormats
//
//============================================================================

static void s_TestFormats(void)
{
    struct SFormatTest {
        const char* format;
        int         truncated; 
    };

    static const SFormatTest s_Fmt[] = {
        {"b D Y h:m:s:r",           1},
        {"b D Y h:m:s:lp",          1},
        {"b D Y H:m:s P",           1},
        {"M/D/Y h:m:s",             1},
        {"M/D/Y h:m:s.S",           0},
        {"M/D/y h:m:s",             1},
        {"M/DY  h:m:s",             1},
        {"M/Dy  h:m:s",             1},
        {"M/D/Y hm:s",              1},
        {"M/D/Y h:ms",              1},
        {"M/D/Y hms",               1},
        {"MD/y  h:m:s",             1},
        {"MD/Y  h:m:s",             1},
        {"MYD   m:h:s",             1},
        {"M/D/Y smh",               1},
        {"YMD   h:sm",              1},
        {"yDM   h:ms",              1},
        {"yMD   h:ms",              1},
        {"D B Y h:m:s",             1},
        {"B d, Y h:m:s",            1},
        {"D b Y h:m:s",             1},
        {"M/D/Y h:m:s z",           1},
        {"M/D/Y Z h:m:s",           1},
        {"smhyMD",                  1},
        {"y||||M++++D   h===ms",    1},
        {"   yM[][D   h:,.,.,ms  ", 1},
        {"\tkkkMy++D   h:ms\n",     1},
        {0,0}
    };

    LOG_POST("\n---------------------------");
    LOG_POST("Test Formats");
    LOG_POST("---------------------------\n");

    for ( int hour = 0; hour < 24; ++hour ) {
        for (int i = 0;  s_Fmt[i].format;  i++) {
            const char* fmt = s_Fmt[i].format;
            
            CTime t1(2001, 4, 2, hour, 4, 5, 88888888,
                     strchr(fmt, 'Z') ? CTime::eGmt : CTime::eLocal);
            
            CTime::SetFormat(fmt);
            string t1_str = t1.AsString();
            LOG_POST("[" + t1_str + "]");
            
            CTime::SetFormat("MDY__s");
            CTime t2(t1_str, fmt);
            if ( s_Fmt[i].truncated ) {
                string test_str = t2.AsString("M/D/Y h:m:s");
                CNcbiOstrstream s;
                s << "04/02/2001 " << hour/10 << hour%10 << ":04:05";
                string need_str = CNcbiOstrstreamToString(s);
                assert(test_str == need_str);
            } else {
                assert(t1 == t2);
            }
            CTime::SetFormat(fmt);
            string t2_str = t2;
            assert(t1_str.compare(t2_str) == 0);
        }
    }

    // Check against well-known dates
    {{
        const char fmtstr[] = "M/D/Y h:m:s Z W";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/2003 20:40:30 GMT Monday") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/1998 20:40:30 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2003 15:49:30 GMT Thursday") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2001 15:49:30 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/31/2002 23:59:59 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("01/01/2003 00:00:00 GMT Wednesday") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/13/2002 12:34:56 GMT Friday") == 0);
        }}
    }}
    {{
        const char fmtstr[] = "M/D/Y H:m:s P Z W";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/2003 08:40:30 PM GMT Monday") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/1998 08:40:30 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2003 03:49:30 PM GMT Thursday") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2001 03:49:30 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/31/2002 11:59:59 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("01/01/2003 12:00:00 AM GMT Wednesday") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/13/2002 12:34:56 PM GMT Friday") == 0);
        }}
    }}
}


//============================================================================
//
// TestGMT
//
//============================================================================

static void s_TestGMT(void)
{
    LOG_POST("\n---------------------------");
    LOG_POST("Test GMT and Local time");
    LOG_POST("---------------------------\n");

    {{
        LOG_POST("Write time in timezone format");

        CTime::SetFormat("M/D/Y h:m:s Z");

        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eGmt);
        LOG_POST(STR(t1));
        assert(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        LOG_POST(STR(t2));
        assert(t2.AsString() == "03/12/2001 11:22:33 ");

        CTime t3(CTime::eCurrent, CTime::eLocal);
        LOG_POST("Local time " + STR(t3));
        CTime t4(CTime::eCurrent, CTime::eGmt);
        LOG_POST("GMT time   " + STR(t4));
    }}
    {{   
        LOG_POST("\nProcess timezone string:");

        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t = "03/12/2001 11:22:33 GMT";
        LOG_POST(STR(t));
        assert(t.AsString() == "03/12/2001 11:22:33 GMT");
        t = "03/12/2001 11:22:33 ";
        LOG_POST(STR(t));
        assert(t.AsString() == "03/12/2001 11:22:33 ");
    }}
    {{   
        LOG_POST("\nDay of week:");

        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i = 0; t <= CTime(2001, 4, 10); t++,i++) {
            LOG_POST(t.AsString() + " is " +NStr::IntToString(t.DayOfWeek()));
            assert(t.DayOfWeek() == (i%7));
        }
    }}
    //------------------------------------------------------------------------
    {{   
        LOG_POST("\nTest GetTimeT");

        time_t timer = time(0);
        CTime tgmt(CTime::eCurrent, CTime::eGmt,   CTime::eTZPrecisionDefault);
        CTime tloc(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        CTime t(timer);
        // Set the same time to all time objects
        tgmt.SetTimeT(timer);
        tloc.SetTimeT(timer);

        time_t g_ = tgmt.GetTimeT();
        time_t l_ = tloc.GetTimeT();
        time_t t_ = t.GetTimeT();

        LOG_POST(STR(t));
        LOG_POST(NStr::Int8ToString(g_/3600) + " - " +
                 NStr::Int8ToString(l_/3600) + " - " +
                 NStr::Int8ToString(t_/3600) + " - " +
                 NStr::Int8ToString(timer/3600) + "\n");

        assert(timer == t_);
        assert(timer == g_);
        // On the day of changing to summer/winter time, the local time
        // converted to GMT may differ from the value returned by time(0),
        // because in the common case API don't know is DST in effect for
        // specified local time or not (see mktime()).
        if (timer != l_) {
            if ( abs((int)(timer - l_)) > 3600 )
                assert(timer == l_);
        }

        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 4, 1, i>0 ? 2 : 1, i>0 ? (i-1) : 59, 
                     0, 0, CTime::eLocal, CTime::eHour);
            LOG_POST(tt.AsString() + " - " + 
                     NStr::Int8ToString(tt.GetTimeT()/3600)); 
        }
        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 10, 28, i > 0 ? 1 : 0, i > 0 ? (i-1) : 59,
                     0, 0, CTime::eLocal, CTime::eHour);
            LOG_POST(tt.AsString() + " - " + 
                     NStr::Int8ToString(tt.GetTimeT()/3600)); 
        }
    }}
    //------------------------------------------------------------------------
    {{   
        LOG_POST("\nTest TimeZoneDiff (1)");

        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);

        LOG_POST(STR(tw) + " diff from GMT = " +
                 NStr::Int8ToString(tw.TimeZoneDiff() / 3600));
        assert(tw.TimeZoneDiff() / 3600 == -5);
        LOG_POST(STR(ts) + " diff from GMT = " +
                 NStr::Int8ToString(ts.TimeZoneDiff() / 3600));
        assert(ts.TimeZoneDiff()/3600 == -4);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -4) {
                LOG_POST("First daylight saving day = " + STR(tw));
                break;
            }
        }
    }}
    //------------------------------------------------------------------------
    {{   
        LOG_POST("\nTest TimeZoneDiff (2)");

        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        LOG_POST(STR(tw) + " diff from GMT = " +
                 NStr::Int8ToString(tw.TimeZoneDiff() / 3600));
        assert(tw.TimeZoneDiff() / 3600 == -4);
        LOG_POST(STR(ts) + " diff from GMT = " +
                 NStr::Int8ToString(ts.TimeZoneDiff() / 3600));
        assert(ts.TimeZoneDiff() / 3600 == -5);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -5) {
                LOG_POST("First non daylight saving day = " + STR(tw));
                break;
             
            }
        }
    }}
    //------------------------------------------------------------------------
    {{   
        LOG_POST("\nTest AdjustTime");

        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("04/01/2001 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eTZPrecisionDefault);
        LOG_POST("init  " + STR(t));

        t.SetTimeZoneFormat(CTime::eGmt);
        LOG_POST("GMT");
        tn = t + 5;  
        LOG_POST("+5d   " + STR(tn));
        assert(tn.AsString() == "04/06/2001 01:01:00");
        tn = t + 40; 
        LOG_POST("+40d  " + STR(tn));
        assert(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZoneFormat(CTime::eLocal);
        LOG_POST("Local eNone");
        t.SetTimeZonePrecision(CTime::eNone);
        tn=t+5;  LOG_POST("+5d   " + STR(tn));
        assert(tn.AsString() == "04/06/2001 01:01:00");
        tn=t+40; LOG_POST("+40d  " + STR(tn));
        assert(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZonePrecision(CTime::eMonth);
        LOG_POST("Local eMonth");
        tn = t + 5;
        LOG_POST("+5d   " + STR(tn));
        tn = t; 
        tn.AddMonth(-1);
        LOG_POST("-1m   " + STR(tn));
        assert(tn.AsString() == "03/01/2001 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        LOG_POST("+1m   " + STR(tn));
        assert(tn.AsString() == "05/01/2001 02:01:00");

        t.SetTimeZonePrecision(CTime::eDay);
        LOG_POST("Local eDay");
        tn = t - 1; 
        LOG_POST("-1d   " + STR(tn));
        assert(tn.AsString() == "03/31/2001 01:01:00");
        tn++;   
        LOG_POST("+0d   " + STR(tn));
        assert(tn.AsString() == "04/01/2001 01:01:00");
        tn = t + 1; 
        LOG_POST("+1d   " + STR(tn));
        assert(tn.AsString() == "04/02/2001 02:01:00");

        LOG_POST("Local eHour");
        t.SetTimeZonePrecision(CTime::eHour);
        tn = t; 
        tn.AddHour(-3);
        CTime te = t; 
        te.AddHour(3);
        LOG_POST("-3h   " + STR(tn));
        assert(tn.AsString() == "03/31/2001 22:01:00");
        LOG_POST("+3h   " + STR(te));
        assert(te.AsString() == "04/01/2001 05:01:00");
        CTime th = tn; 
        th.AddHour(49);
        LOG_POST("+49h  " + STR(th) + "\n");
        assert(th.AsString() == "04/03/2001 00:01:00");

        for (int i = 0;  i < 8;  i++,  tn.AddHour()) {
            LOG_POST(string(((tn.TimeZoneDiff()/3600) == -4) ? "  " : "* ") +
                     STR(tn));
        }
        tn.AddHour(-1);
        for (int i = 0;  i < 8;  i++,  tn.AddHour(-1)) {
            LOG_POST(string(((tn.TimeZoneDiff()/3600) == -4) ? "  " : "* ") +
                     STR(tn));
        }
        LOG_POST("");

        tn = "10/28/2001 00:01:00"; 
        LOG_POST("init  " + STR(tn));
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-3); 
        te.AddHour(9);
        LOG_POST("-3h   " + STR(tn));
        assert(tn.AsString() == "10/27/2001 21:01:00");
        LOG_POST("+9h   " + STR(te));
        assert(te.AsString() == "10/28/2001 08:01:00");
        th = tn; 
        th.AddHour(49);
        LOG_POST("+49h  " + STR(th));
        assert(th.AsString() == "10/29/2001 21:01:00");
        LOG_POST("");

        tn.AddHour(+2);
        for (int i = 0;  i < 10;  i++,  tn.AddHour()) {
            LOG_POST((((tn.TimeZoneDiff()/3600) == -4) ? "  ":"* ") +STR(tn));
        }
        LOG_POST("");
        tn.AddHour(-1);
        for (int i = 0;  i < 10;  i++,  tn.AddHour(-1)) {
            LOG_POST((((tn.TimeZoneDiff()/3600) == -4) ? "  ":"* ") +STR(tn));
        }
        LOG_POST("");

        tn = "10/28/2001 09:01:00"; 
        LOG_POST("init  " + STR(tn));
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-10); 
        te.AddHour(+10);
        LOG_POST("-10h  " + STR(tn));
        assert(tn.AsString() == "10/28/2001 00:01:00");
        LOG_POST("+10h  " + STR(te));
        assert(te.AsString() == "10/28/2001 19:01:00");
        
        LOG_POST("\n");
    }}
}


//============================================================================
//
// TestGMTSpeedRun
//
//============================================================================

static void s_TestGMTSpeedRun(string comment, CTime::ETimeZone tz, 
                              CTime::ETimeZonePrecision tzp)
{
    CTime t(CTime::eCurrent, tz, tzp);
    CStopWatch timer;
    double duration;

#if defined    NCBI_OS_MSWIN
    const long kCount=100000L;
#elif defined  NCBI_OS_UNIX
    const long kCount=10000L;
#else       // NCBI_OS_MAC
    const long kCount=100000L;
#endif

    t.SetFormat("M/D/Y h:m:s");
    t = "03/31/2001 00:00:00"; 

    LOG_POST("Minute add, " + comment);
    LOG_POST("Iterations  = " + NStr::UIntToString(kCount));

    timer.Start();
    for (long i = 0; i < kCount; i++) {
        t.AddMinute();
    }
    duration = timer.Elapsed();
    LOG_POST("Duration    = " + NStr::DoubleToString(duration) + " sec.");
}


//============================================================================
//
// TestGMTSpeed
//
//============================================================================

static void s_TestGMTSpeed(void)
{
    LOG_POST("---------------------------");
    LOG_POST("Test CTime Speed");
    LOG_POST("---------------------------\n");

    s_TestGMTSpeedRun("eLocal - eMinute", CTime::eLocal, CTime::eMinute);
    s_TestGMTSpeedRun("eLocal - eHour"  , CTime::eLocal, CTime::eHour);
    s_TestGMTSpeedRun("eLocal - eMonth" , CTime::eLocal, CTime::eMonth);
    s_TestGMTSpeedRun("eLocal - eNone"  , CTime::eLocal, CTime::eNone);
    s_TestGMTSpeedRun("eGmt   - eNone"  , CTime::eGmt, CTime::eNone);
}


//============================================================================
//
// TestTimeSpan
//
//============================================================================

static void s_TestTimeSpan(void)
{
    LOG_POST("\n---------------------------");
    LOG_POST("Test TimeSpan");
    LOG_POST("---------------------------\n");

    // Common constructors
    {{
        CTimeSpan t1(0,0,0,1,-2);
        LOG_POST(t1.AsString());
        assert(t1.AsString() == "0.999999998");
        CTimeSpan t2(0,0,0,-1,2);
        LOG_POST(t2.AsString());
        assert(t2.AsString() == "-0.999999998");
        CTimeSpan t3(0,0,0,0,-2);
        LOG_POST(t3.AsString());
        assert(t3.AsString() == "-0.000000002");
        CTimeSpan t4(0,0,0,0,2);
        LOG_POST(t4.AsString());
        assert(t4.AsString() == "0.000000002");
    }}
    {{
        CTimeSpan t1(2,3,4,5,6);
        assert(t1.GetCompleteHours()   == 51);
        assert(t1.GetCompleteMinutes() == (51*60+4));
        assert(t1.GetCompleteSeconds() == ((51*60+4)*60+5));
        assert(t1.GetNanoSecondsAfterSecond() == 6);
        LOG_POST(t1.AsString());
        assert(t1.AsString() == "183845.000000006");

        CTimeSpan t2(-2,-3,-4,-5,-6);
        assert(t2.GetCompleteHours()   == -51);
        assert(t2.GetCompleteMinutes() == -(51*60+4));
        assert(t2.GetCompleteSeconds() == -((51*60+4)*60+5));
        assert(t2.GetNanoSecondsAfterSecond() == -6);
        LOG_POST(t2.AsString());
        assert(t2.AsString() == "-183845.000000006");

        CTimeSpan t3(-2,+3,-4,-5,+6);
        assert(t3.GetCompleteHours()   == -45);
        assert(t3.GetCompleteMinutes() == -(45*60+4));
        assert(t3.GetCompleteSeconds() == -((45*60+4)*60+4));
        assert(t3.GetNanoSecondsAfterSecond() == -999999994);
        LOG_POST(t3.AsString());
        assert(t3.AsString() == "-162244.999999994");
    }}

    // Comparison
    {{
        CTimeSpan t0;
        CTimeSpan t1(123.4);
        CTimeSpan t2(123.45);
        CTimeSpan t3(123.4);

        assert(t0.GetSign() == eZero);
        assert(t1.GetSign() == ePositive);
        assert(t0 == CTimeSpan(0,0,0,0,0));
        assert(t1 != t2);
        assert(t1 == t3);
        assert(t1 <  t2);
        assert(t2 >  t1);
        assert(t1 <= t2);
        assert(t2 >= t2);
    }}

    // Assignment
    {{
        CTimeSpan t1(-123.4);
        CTimeSpan t2(123.45);

        t1 = t2;
        assert(t1 == t2);
        t1 = "-123.450000000";
        assert(t1 == -t2);
    }}

    // Arithmetics
    {{
        CTimeSpan t;
        t = CTimeSpan("100.3") - CTimeSpan("123.4");
        assert(t == CTimeSpan("-23.1"));
        t = CTimeSpan("63.7")  + CTimeSpan("2.3");
        assert(t == CTimeSpan("65.10"));
        t = CTimeSpan("63.7")  - CTimeSpan("72.6");
        assert(t == CTimeSpan("-8.999999999"));

        t = "-123.4";
        t += CTimeSpan("1.0");  
        assert(t == CTimeSpan("-122.4"));
        t += CTimeSpan("222.4");  
        assert(t == CTimeSpan("100.0") );
        t -= CTimeSpan("50.1");  
        assert(t == CTimeSpan("49.999999999"));
        t += CTimeSpan("0.1");  
        assert(t == CTimeSpan("50.0"));
        t += CTimeSpan("3.7");  
        assert(t == CTimeSpan("53.7"));
        t -= CTimeSpan("3.8");  
        assert(t == CTimeSpan("49.999999999"));
    }}

    // Formats
    {{
        static const char* s_Fmt[] = {
            "d h:m:s.n",
            "H m:s",
            "S",
            "H",
            "M",
            "d",
            "-d h:m:s.n",
            "-H m:s",
            "-S",
            "-H",
            "-M",
            "-d",
            0
        };

        for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
            CTimeSpan t1(-123456789.987654321);
            CTimeSpan::SetFormat(*fmt);
            string t1_str = t1.AsString();
            CTimeSpan::SetFormat("_s_");
            CTimeSpan t2(t1_str, *fmt);
            CTimeSpan::SetFormat(*fmt);
            string t2_str = t2;
            LOG_POST("[" + t1_str + "] --> [" << t2_str << "]");
            assert(t1_str.compare(t2_str) == 0);
        }
    }}
    LOG_POST("");

    // Smart string
    {{
        for (int prec = CTimeSpan::eSSP_Year;
            prec <= CTimeSpan::eSSP_Precision7;     
            prec++) 
        {
            CTimeSpan diff(559, 29, 59, 41, 17000000);
            string str;
            str = diff.AsSmartString(CTimeSpan::ESmartStringPrecision(prec), eTrunc);
            LOG_POST(str.c_str());
            str = diff.AsSmartString(CTimeSpan::ESmartStringPrecision(prec), eRound);
            LOG_POST(str.c_str());
        }
    }}
}


//============================================================================
//
// DemoStopWatch
//
//============================================================================

static void s_DemoStopWatch(void)
{
    LOG_POST("\n---------------------------");
    LOG_POST("Demo StopWatch");
    LOG_POST("---------------------------\n");
    
    CStopWatch sw;
    sw.SetFormat("S.n");
    sw.Start();

    CNcbiOstrstream s;
    for (int i=0; i<10; i++) {
        s  << sw << endl;
    }
    sw.Stop();
    SleepMilliSec(500);
    sw.Start();
    for (int i=0; i<10; i++) {
        s << sw << endl;
    }
    LOG_POST((string)CNcbiOstrstreamToString(s));
}


//============================================================================
//
// MAIN
//
//============================================================================

int main()
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Reinit global timezone variables
    tzset();

    // Run tests
    try {
        s_TestMisc();
        s_TestFormats();
        s_TestGMT();
        s_TestGMTSpeed();
        s_TestTimeSpan();
        s_DemoStopWatch();
    } catch (CException& e) {
        ERR_POST(Fatal << e);
    }
    // Success
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.32  2006/11/01 20:21:52  ivanov
 * Fixed s_testGMT for cases when DST is changing
 *
 * Revision 6.31  2006/06/06 12:21:05  ivanov
 * Use NStr::Int8ToString for time_t values
 *
 * Revision 6.30  2006/01/12 15:43:20  ivanov
 * Use LOG_POST instead of cout
 *
 * Revision 6.29  2005/01/04 11:59:38  ivanov
 * Get rid of an unused variable
 *
 * Revision 6.28  2004/12/29 21:41:30  vasilche
 * Fixed parsing and formatting of twelfth hour in AM/PM mode.
 *
 * Revision 6.27  2004/09/27 14:08:01  ivanov
 * Removed some debug code
 *
 * Revision 6.26  2004/09/27 13:54:22  ivanov
 * Added tests for CTimeSpan::AsSmartString
 *
 * Revision 6.25  2004/09/20 16:27:26  ivanov
 * CTime:: Added milliseconds, microseconds and AM/PM to string time format.
 *
 * Revision 6.24  2004/09/07 18:48:06  ivanov
 * CTimeSpan:: renamed GetTotal*() -> GetComplete*()
 *
 * Revision 6.23  2004/09/07 16:33:19  ivanov
 * + s_TestTimeSpan(), s_DemoStopWatch()
 *
 * Revision 6.22  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.21  2004/03/25 17:35:17  ivanov
 * Added tests for 'z' time format
 *
 * Revision 6.20  2003/11/25 20:03:56  ivanov
 * Fixed misspelled eTZPrecisionDefault
 *
 * Revision 6.19  2003/11/25 19:56:38  ivanov
 * Renamed eDefault to eTZPrecisionDefault.
 * Some cosmetic changes.
 *
 * Revision 6.18  2003/10/03 18:27:20  ivanov
 * Added tests for month and day of week names conversion functions
 *
 * Revision 6.17  2003/07/15 19:37:50  vakatov
 * Added test with weekday and timezone
 *
 * Revision 6.16  2003/04/16 20:29:26  ivanov
 * Using class CStopWatch instead clock()
 *
 * Revision 6.15  2003/02/10 17:46:25  lavr
 * Added more checks for well-known dates
 *
 * Revision 6.14  2002/10/17 16:56:03  ivanov
 * Added tests for 'b' and 'B' time format symbols
 *
 * Revision 6.13  2002/07/15 18:17:26  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.12  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.11  2002/04/16 18:49:09  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.10  2001/09/27 18:02:45  ivanov
 * Fixed division at zero in test of speed CTime class on fast computers.
 *
 * Revision 6.9  2001/07/23 15:51:46  ivanov
 * Changed test for work with DB-time formats
 *
 * Revision 6.8  2001/07/06 15:08:36  ivanov
 * Added tests for DataBase-time's
 *
 * Revision 6.7  2001/05/29 16:14:02  ivanov
 * Return to nanosecond-revision. Corrected mistake of the work with local
 * time on Linux. Polish and improvement source code.
 * Renamed AsTimeT() -> GetTimerT().
 *
 * Revision 6.6  2001/04/30 22:01:31  lavr
 * Rollback to pre-nanosecond-revision due to necessity to use
 * configure to figure out names of global variables governing time zones
 *
 * Revision 6.5  2001/04/29 03:06:10  lavr
 * #include <time.h>" moved from .cpp to ncbitime.hpp
 *
 * Revision 6.4  2001/04/27 20:39:47  ivanov
 * Tests for check Local/UTC time and Nanoseconds added.
 * Also speed test added.
 *
 * Revision 6.3  2000/11/21 18:15:05  butanaev
 * Fixed bug in operator ++/-- (int)
 *
 * Revision 6.2  2000/11/21 15:22:57  vakatov
 * Do not enforce "_DEBUG" -- it messes up the MSVC++ compilation
 *
 * Revision 6.1  2000/11/20 22:17:49  vakatov
 * Added NCBI date/time class CTime ("ncbitime.[ch]pp") and
 * its test suite ("test/test_ncbitime.cpp")
 *
 * ===========================================================================
 */
