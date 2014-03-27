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
 *   Test for CTime - the standard Date/Time class.
 *
 * NOTE: 
 *   The time change dates for Daylight Saving Time in the U.S. and 
 *   Europe are different. Because this test have hardcoded dates, it works
 *   only on the machines with the same DST standards as in the U.S. 
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <stdio.h>
#include <stdlib.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


// Macro to enable/disable speed tests
#define ENABLE_SPEED_TESTS  0

// String printout
#define STR(t) string("[" + (t).AsString() + "]")

#if defined(NCBI_OS_DARWIN)  ||  defined(NCBI_OS_BSD)
#  define TIMEZONE_IS_UNDEFINED  1
#endif


//============================================================================
//
// TestMisc
//
//============================================================================

static void s_TestMisc(void)
{
    // Print current time
    {{
        CTime t(CTime::eCurrent);
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
        try {
            CTime::MonthNameToNum("Month"); 
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }}

    // String <-> CTime conversion
    {{
        {{
            CTime t;
            assert(t.AsString() == "");
        }}
        {{
            CTime t(2000, 365 / 2);
            CTime::SetFormat("M/D/Y h:m:s");
            assert(t.AsString() == "06/30/2000 00:00:00");
        }}
        {{
            // Year 2000 problem:
            CTime::SetFormat("M/D/Y");
            CTime t(1999, 12, 30); 
            t.AddDay();
            assert(t.AsString() == "12/31/1999");
            t.AddDay();
            assert(t.AsString() == "01/01/2000");
            t.AddDay();
            assert(t.AsString() == "01/02/2000");
            t="02/27/2000";
            t.AddDay();
            assert(t.AsString() == "02/28/2000");
            t.AddDay();
            assert(t.AsString() == "02/29/2000");
            t.AddDay();
            assert(t.AsString() == "03/01/2000");
            t.AddDay();
            assert(t.AsString() == "03/02/2000");
        }}
        {{
            // String assignment:
            CTime::SetFormat("M/D/Y h:m:s");
            try {
                CTime t("02/15/2000 01:12:33");
                assert(t.AsString() == "02/15/2000 01:12:33");
                t = "6/16/2001 02:13:34";
                assert(t.AsString() == "06/16/2001 02:13:34");
            }
            catch (CTimeException&) {}
        }}
    }}

    // Comparison
    {{
        CTime empty;
        CTime cl(CTime::eCurrent, CTime::eLocal);
        CTime cg(cl);
        cg.ToGmtTime();

        assert( !(empty > empty) );
        assert( !(empty < empty) );
        assert(  (empty == empty) );
        assert( !(empty > cl) );
        assert(  (empty < cl) );
        assert( !(empty == cl) );
        assert( !(empty > cg) );
        assert(  (empty < cg) );
        assert( !(empty == cg) );
        assert(  (cl > empty) );
        assert( !(cl < empty) );
        assert( !(cl == empty) );
        assert(  (cg > empty) );
        assert( !(cg < empty) );
        assert( !(cg == empty) );
        assert( !(cg > cl) );
        assert( !(cg < cl) );
        assert(  (cg == cl) );
        assert( !(cl > cg) );
        assert( !(cl < cg) );
        assert(  (cl == cg) );
    }}

    // Addition
    {{
        CTime::SetFormat("M/D/Y h:m:s.S");
        {{
            // Adding Nanoseconds
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 59, 999999995);
                 tmp <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
                 t = tmp, tmp.AddNanoSecond(2)) {
            }
            assert(t.AsString() == "01/01/2000 00:00:00.000000003");
        }}
        {{
            // Current time with nanoseconds (10 cycles)
            CTime t;
            for (int i = 0; i < 10; i++) {
                 t.SetCurrent();
            }
        }}

        CTime::SetFormat("M/D/Y h:m:s");
        {{
            // nAdding seconds
            CTime t;
            for (CTime tmp(1999, 12, 31, 23, 59, 5);
                 tmp <= CTime(2000, 1, 1, 0, 1, 20);
                 t = tmp, tmp.AddSecond(11)) {
            }
            assert(t.AsString() == "01/01/2000 00:01:17");
        }}
        {{
            // Adding minutes
            for (CTime t(1999, 12, 31, 23, 45);
                 t <= CTime(2000, 1, 1, 0, 15);
                 t.AddMinute(11)) {
            }
        }}
        {{
            // Adding hours
            for (CTime t(1999, 12, 31);
                 t <= CTime(2000, 1, 1, 15);
                 t.AddHour(11)) {
            }
        }}
        {{
            // Adding months
            for (CTime t(1998, 12, 29);
                 t <= CTime(1999, 4, 1);
                 t.AddMonth()) {
            }
        }}
        {{
            // Adding time span
            CTime t0(1999, 12, 31, 23, 59, 5);
            CTimeSpan ts(1, 2, 3, 4, 555555555);

            for (int i=0; i<10; i++) {
                 t0.AddTimeSpan(ts);
            }
            assert(t0.AsString() == "01/11/2000 20:29:50");

            CTime t1;
            t1 = t0 + ts;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = ts + t0;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0; t1 += ts;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/12/2000 22:32:55");
            t1 = t0 - ts;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            t1 = t0; t1 -= ts;
            assert(t0.AsString() == "01/11/2000 20:29:50");
            assert(t1.AsString() == "01/10/2000 18:26:45");
            ts = t0 - t1;
            assert(ts.AsString() == "93784.555555555");
            ts = t1 - t0;
            assert(ts.AsString() == "-93784.555555555");
        }}
    }}

    // Difference
    {{
        CTime t1(2000, 10, 1, 12, 3, 45,1);
        CTime t2(2000, 10, 2, 14, 55, 1,2);
        CTimeSpan ts(1,2,51,16,1);

        assert((t2.DiffDay(t1) - 1.12) < 0.01);
        assert((t2.DiffHour(t1) - 26.85) < 0.01);
        assert((t2.DiffMinute(t1) - 1611.27) < 0.01);
        assert( t2.DiffSecond(t1) ==  96676);
        assert( t1.DiffSecond(t2) == -96676);
        assert(NStr::DoubleToString(t2.DiffNanoSecond(t1),0) == "96676000000001");
        assert(t2.DiffTimeSpan(t1) ==  ts);
        assert(t1.DiffTimeSpan(t2) == -ts);

        t1 = t2; // both local times
        t1.SetTimeZone(CTime::eGmt);
        ts = CTimeSpan(long(t1.DiffSecond(t2)));
        assert(ts.GetAsDouble() == double(t2.TimeZoneOffset()));
    }}

    // CXX-195
    {{
        CTime time1("12/31/2007 20:00", "M/D/Y h:m");
        CTime time2("1/1/2008 20:00", "M/D/Y h:m");
        CTime time3("12/31/2007", "M/D/Y");
        CTime time4("1/1/2008", "M/D/Y");
        LOG_POST("time1=" << time1.AsString("M/D/Y h:m:s")
                 << "  time_t=" << time1.GetTimeT()
                 << "  time-zone: " << time1.TimeZoneOffset());
        LOG_POST("time2=" << time2.AsString("M/D/Y h:m:s")
                 << "  time_t=" << time2.GetTimeT()
                 << "  time-zone: " << time2.TimeZoneOffset());
        assert(time1.TimeZoneOffset() == time2.TimeZoneOffset());
        LOG_POST("time3=" << time3.AsString("M/D/Y h:m:s")
                 << "  time_t=" << time3.GetTimeT()
                 << "  time-zone: " << time3.TimeZoneOffset());
        assert(time2.TimeZoneOffset() == time3.TimeZoneOffset());
        LOG_POST("time4=" << time4.AsString("M/D/Y h:m:s")
                 << "  time_t=" << time4.GetTimeT()
                 << "  time-zone: " << time4.TimeZoneOffset());
        assert(time3.TimeZoneOffset() == time4.TimeZoneOffset());
    }}

    // Database formats conversion
    {{
        CTime t1(2000, 1, 1, 1, 1, 1, 10000000);
        CTime::SetFormat("M/D/Y h:m:s.S");

        TDBTimeU dbu = t1.GetTimeDBU();
        assert(dbu.days == 36524);
        assert(dbu.time == 61);
        TDBTimeI dbi = t1.GetTimeDBI();
        assert(dbi.days == 36524);
        assert(dbi.time == 1098303);

        CTime t2;
        t2.SetTimeDBU(dbu);
        assert(t2.AsString() == "01/01/2000 01:01:00.000000000");
        t2.SetTimeDBI(dbi);
        assert(t2.AsString() == "01/01/2000 01:01:01.010000000");
        CTime::SetFormat("M/D/Y h:m:s");
        dbi.days = 37093;
        dbi.time = 12301381;
        t2.SetTimeDBI(dbi);
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
        for (i = 1; t <= CTime(2030, 12, 31); t.AddDay(),i++) {
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

        for (i = 1; t <= CTime(2030, 12, 31); i++, t.AddDay(), gt += 24*3600) {
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

    // Rounding time
    {{
        CTime::SetFormat("M/D/Y h:m:s.S");

        // Round
        CTime t(2000, 1, 2, 20, 40, 29, 998933833);
        t.Round(CTime::eRound_Microsecond);
        assert(t.AsString() == "01/02/2000 20:40:29.998934000");
        t.Round(CTime::eRound_Millisecond);
        assert(t.AsString() == "01/02/2000 20:40:29.999000000");
        t.Round(CTime::eRound_Second);
        assert(t.AsString() == "01/02/2000 20:40:30.000000000");
        t.Round(CTime::eRound_Minute);
        assert(t.AsString() == "01/02/2000 20:41:00.000000000");
        t.Round(CTime::eRound_Hour);
        assert(t.AsString() == "01/02/2000 21:00:00.000000000");
        t.Round(CTime::eRound_Day);
        assert(t.AsString() == "01/03/2000 00:00:00.000000000");

        // Round - special case
        CTime t1(2000, 1, 2, 20, 40, 29, 999999991);
        t1.Round(CTime::eRound_Microsecond);
        assert(t1.AsString() == "01/02/2000 20:40:30.000000000");

        // Truncate
        CTime t2(2000, 1, 2, 20, 40, 29, 998933833);
        t2.Truncate(CTime::eRound_Microsecond);
        assert(t2.AsString() == "01/02/2000 20:40:29.998933000");
        t2.Truncate(CTime::eRound_Millisecond);
        assert(t2.AsString() == "01/02/2000 20:40:29.998000000");
        t2.Truncate(CTime::eRound_Second);
        assert(t2.AsString() == "01/02/2000 20:40:29.000000000");
        t2.Truncate(CTime::eRound_Minute);
        assert(t2.AsString() == "01/02/2000 20:40:00.000000000");
        t2.Truncate(CTime::eRound_Hour);
        assert(t2.AsString() == "01/02/2000 20:00:00.000000000");
        t2.Truncate(CTime::eRound_Day);
        assert(t2.AsString() == "01/02/2000 00:00:00.000000000");
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
#if !defined(TIMEZONE_IS_UNDEFINED)
        {"M/D/Y h:m:s z",           1},
#endif
        {"M/D/Y Z h:m:s",           1},
        {"smhyMD",                  1},
        {"y||||M++++D   h===ms",    1},
        {"   yM[][D   h:,.,.,ms  ", 1},
        {"\tkkkMy++D   h:ms\n",     1},
        {0,0}
    };

    for ( int hour = 0; hour < 24; ++hour ) {
        for (int i = 0;  s_Fmt[i].format;  i++) {
            const char* fmt = s_Fmt[i].format;
            
            bool is_gmt = (strchr(fmt, 'Z') ||  strchr(fmt, 'z'));
            CTime t1(2001, 4, 2, hour, 4, 5, 88888888,
                     is_gmt ? CTime::eGmt : CTime::eLocal);
            
            CTime::SetFormat(fmt);
            string t1_str = t1.AsString();
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
    {{
        const char fmtstr[] = "b d, Y H:m P";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Feb 10, 2003 08:40 PM") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Feb 10, 1998 08:40 PM") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Mar 13, 2003 03:49 PM") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Mar 13, 2001 03:49 PM") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Dec 31, 2002 11:59 PM") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Jan 1, 2003 12:00 AM") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eGmt);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Dec 13, 2002 12:34 PM") == 0);
        }}
    }}

    // Partially defined time
    {{
        string s;
        {{  // Y
            CTime t("2001", "Y");
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("01/01/2001 00:00:00") == 0);
        }}
        {{  // Y/M
            CTime t("2001/2", "Y/M");
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("02/01/2001 00:00:00") == 0);
        }}
        {{  // M/D
            CTime current(CTime::eCurrent);
            current.Truncate();
            CTime t("01/02", "M/D");
            current.SetMonth(1);
            current.SetDay(2);
            assert(t == current);
        }}
        {{  // M
            CTime current(CTime::eCurrent);
            current.Truncate();
            CTime t("2", "M");
            current.SetMonth(2);
            current.SetDay(1);
            assert(t == current);
        }}
        {{  // D time
            CTime current(CTime::eCurrent);
            current.Truncate();
            CTime t("2 11:22", "D h:m");
            current.SetDay(2);
            current.SetHour(11);
            current.SetMinute(22);
            assert(t == current);
        }}
        {{  // D
            CTime current(CTime::eCurrent);
            current.Truncate();
            CTime t("2", "D");
            current.SetDay(2);
            assert(t == current);
        }}
        {{  // time
            CTime current(CTime::eCurrent);
            CTime t("11:22", "h:m");
            current.SetHour(11);
            current.SetMinute(22);
            current.SetSecond(0);
            current.SetNanoSecond(0);
            assert(t == current);
        }}

        try {
            CTime t("2001/2 00:00", "Y/M h:m");
            _TROUBLE; // day is not defined
            assert(t.IsValid());
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001/2 00:00", "Y/D h:m");
            _TROUBLE; // month is not defined
            assert(t.IsValid());
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001/2", "Y/D");
            _TROUBLE; // month is not defined
            assert(t.IsValid());
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001 00:00", "Y h:m");
            _TROUBLE; // month and day are not defined
            assert(t.IsValid());
        }
        catch (CTimeException&) {}

        try {
            CTime t("2 00:00", "M h:m");
            _TROUBLE; // year and day are not defined
            assert(t.IsValid());
        }
        catch (CTimeException&) {}
    }}

    // Strict/weak time assignment from a string
    {{
        string s;
        {{
            CTime t("2001", CTimeFormat("Y/M/D", CTimeFormat::fMatch_ShortTime));
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("01/01/2001 00:00:00") == 0);
        }}
        {{
            // Note that day and month changed
            CTime t("2001/01/02", CTimeFormat("Y", CTimeFormat::fMatch_ShortFormat));
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("01/01/2001 00:00:00") == 0);
        }}
        {{
            CTime t("2001", CTimeFormat("Y/M/D", CTimeFormat::fMatch_Weak));
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("01/01/2001 00:00:00") == 0);
        }}
        {{
            // Note that day and month changed
            CTime t("2001/01/02", CTimeFormat("Y", CTimeFormat::fMatch_Weak));
            s = t.AsString("M/D/Y h:m:s");
            assert(s.compare("01/01/2001 00:00:00") == 0);
        }}
        {{  
            try {
                CTime t("2001", "Y/M/D");
                _TROUBLE;  // by default used strict format matching
                assert(t.IsValid());
            }
            catch (CTimeException&) {}
            try {
                CTime t("2001/01/02", "Y");
                _TROUBLE;  // by default used strict format matching
                assert(t.IsValid());
            }
            catch (CTimeException&) {}
        }}
    }}

    // SetFormat/AsString with flag parameter test
    {{
        CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eGmt);
        string s;
        s = t.AsString("M/D/Y h:m:s");
        assert(s.compare("02/10/2003 20:40:30") == 0);
        s = t.AsString("MDY $M/$D/$Y $h:$m:$s hms");
        assert(s.compare("02102003 $02/$10/$2003 $20:$40:$30 204030") == 0);
        s = t.AsString(CTimeFormat("MDY $M/$D/$Y $h:$m:$s hms",
                                   CTimeFormat::eNcbi));
        assert(s.compare("MDY 02/10/2003 20:40:30 hms") == 0);
    }}

    // CTimeFormat::GetPredefined() test
    {{
        CTime t(2003, 2, 10, 20, 40, 30, 123456789, CTime::eGmt);
        string s;
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_Year));
        assert(s.compare("2003") == 0);
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_YearMonth));
        assert(s.compare("2003-02") == 0);
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_Date));
        assert(s.compare("2003-02-10") == 0);
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeMin));
        assert(s.compare("2003-02-10T20:40") == 0);
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeSec));
        assert(s.compare("2003-02-10T20:40:30") == 0);
        s = t.AsString(CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeFrac));
        assert(s.compare("2003-02-10T20:40:30.123") == 0);
    }}

    // Test assignment operator in different (from default) time format
    {{
        CTime t0(2003, 2, 10, 20, 40, 30, 0, CTime::eLocal);
        CTime::SetFormat(CTimeFormat::GetPredefined(
                                CTimeFormat::eISO8601_DateTimeMin,
                                CTimeFormat::eNcbi));
        assert(t0.AsString() == "2003-02-10T20:40");
        CTime t("2003-02-10T20:40");
        t.SetSecond(30);
        assert(t == t0);
    }}
}


//============================================================================
//
// TestGMT
//
//============================================================================

static void s_TestGMT(void)
{
    //------------------------------------------------------------------------
    // Local/GMT times
    {{
        CTime::SetFormat("M/D/Y h:m:s Z");
        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eGmt);
        assert(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        assert(t2.AsString() == "03/12/2001 11:22:33 ");
        CTime t3(CTime::eCurrent, CTime::eLocal);
        CTime t4(CTime::eCurrent, CTime::eGmt);
    }}
    //------------------------------------------------------------------------
    // Process timezone string
    {{   
        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t = "03/12/2001 11:22:33 GMT";
        assert(t.AsString() == "03/12/2001 11:22:33 GMT");
        t = "03/12/2001 11:22:33 ";
        assert(t.AsString() == "03/12/2001 11:22:33 ");
    }}
    //------------------------------------------------------------------------
    // Day of week
    {{   
        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i = 0; t <= CTime(2001, 4, 10); t.AddDay(),i++) {
            assert(t.DayOfWeek() == (i%7));
        }
    }}
    //------------------------------------------------------------------------
    // Test GetTimeT
    {{   

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

        CTime t11(2013, 3, 10, 1, 59, 0, 0, CTime::eLocal, CTime::eHour);
        CTime t12(2013, 3, 10, 3,  0, 0, 0, CTime::eLocal, CTime::eHour);
        assert(t12.GetTimeT()/3600 - t11.GetTimeT()/3600 == 1);

        CTime t21(2013, 11, 3, 0, 59, 0, 0, CTime::eLocal, CTime::eHour);
        CTime t22(2013, 11, 3, 2,  0, 0, 0, CTime::eLocal, CTime::eHour);
        assert(t22.GetTimeT()/3600 - t21.GetTimeT()/3600 == 3);
    }}

    //------------------------------------------------------------------------
    // Test GetTimeTM
    {{   
        CTime tloc(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        struct tm l_ = tloc.GetTimeTM();
        CTime tmp(CTime::eCurrent, CTime::eGmt, CTime::eTZPrecisionDefault);
        assert(tmp.GetTimeZone() == CTime::eGmt);
        tmp.SetTimeTM(l_);
        assert(tmp.GetTimeZone() == CTime::eLocal);
        assert(tloc.AsString() == tmp.AsString());

        CTime tgmt(tloc.GetTimeT());
        struct tm g_ = tgmt.GetTimeTM();
        tmp.SetTimeTM(g_);
        assert(tmp.GetTimeZone() == CTime::eLocal);
        assert(tgmt.AsString() != tloc.AsString());
    }}
    //------------------------------------------------------------------------
    // Test TimeZoneOffset (1) -- EST timezone only
    {{  
        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);
        assert(tw.TimeZoneOffset() / 3600 == -5);
        assert(ts.TimeZoneOffset() / 3600 == -4);
        tw.SetFormat("M/D/Y");
        for (; tw < ts; tw.AddDay()) {
            if ((tw.TimeZoneOffset() / 3600) == -4) {
                LOG_POST("First daylight saving day = " + STR(tw));
                break;
            }
        }
    }}
    //------------------------------------------------------------------------
    // Test TimeZoneOffset (2) -- EST timezone only
    {{   
        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        assert(tw.TimeZoneOffset() / 3600 == -4);
        assert(ts.TimeZoneOffset() / 3600 == -5);
        tw.SetFormat("M/D/Y");
        for (; tw < ts; tw.AddDay()) {
            if ((tw.TimeZoneOffset() / 3600) == -5) {
                LOG_POST("First non daylight saving day = " + STR(tw));
                break;
             
            }
        }
    }}
    //------------------------------------------------------------------------
    // Test AdjusyTime -- EST timezone only
    {{   
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("03/11/2007 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eTZPrecisionDefault);

        t.SetTimeZone(CTime::eGmt);
        tn = t;
        tn.AddDay(5);  
        assert(tn.AsString() == "03/16/2007 01:01:00");
        tn = t;
        tn.AddDay(40); 
        assert(tn.AsString() == "04/20/2007 01:01:00");

        t.SetTimeZone(CTime::eLocal);
        t.SetTimeZonePrecision(CTime::eNone);
        tn = t;
        tn.AddDay(5);
        assert(tn.AsString() == "03/16/2007 01:01:00");
        tn = t;
        tn.AddDay(40);
        assert(tn.AsString() == "04/20/2007 01:01:00");

        t.SetTimeZonePrecision(CTime::eMonth);
        tn = t;
        tn.AddDay(5);
        tn = t; 
        tn.AddMonth(-1);
        assert(tn.AsString() == "02/11/2007 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        assert(tn.AsString() == "04/11/2007 02:01:00");

        t.SetTimeZonePrecision(CTime::eDay);
        tn = t;
        tn.AddDay(-1); 
        assert(tn.AsString() == "03/10/2007 01:01:00");
        tn.AddDay();   
        assert(tn.AsString() == "03/11/2007 01:01:00");
        tn = t;
        tn.AddDay(); 
        assert(tn.AsString() == "03/12/2007 02:01:00");

        t.SetTimeZonePrecision(CTime::eHour);
        tn = t; 
        tn.AddHour(-3);
        CTime te = t; 
        te.AddHour(3);
        assert(tn.AsString() == "03/10/2007 22:01:00");
        assert(te.AsString() == "03/11/2007 05:01:00");
        CTime th = tn; 
        th.AddHour(49);
        assert(th.AsString() == "03/13/2007 00:01:00");

        tn = "11/04/2007 00:01:00"; 
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-3); 
        te.AddHour(9);
        assert(tn.AsString() == "11/03/2007 21:01:00");
        assert(te.AsString() == "11/04/2007 08:01:00");
        th = tn; 
        th.AddHour(49);
        assert(th.AsString() == "11/05/2007 21:01:00");

        tn = "11/04/2007 09:01:00"; 
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-10); 
        te.AddHour(+10);
        assert(tn.AsString() == "11/04/2007 00:01:00");
        assert(te.AsString() == "11/04/2007 19:01:00");
    }}
}


//============================================================================
//
// TestGMTSpeed
//
//============================================================================

#if ENABLE_SPEED_TESTS

static void s_TestGMTSpeedRun(string comment, CTime::ETimeZone tz, 
                              CTime::ETimeZonePrecision tzp)
{
    CTime t(CTime::eCurrent, tz, tzp);
    CStopWatch timer;

#if defined    NCBI_OS_MSWIN
    const long kCount=100000L;
#elif defined  NCBI_OS_UNIX
    const long kCount=10000L;
#else
    const long kCount=100000L;
#endif

    t.SetFormat("M/D/Y h:m:s");
    t = "03/31/2001 00:00:00"; 

    timer.Start();
    for (long i = 0; i < kCount; i++) {
        t.AddMinute();
    }
    LOG_POST(comment + ", duration = " + NStr::DoubleToString(timer.Elapsed()) + " sec.");
}

static void s_TestGMTSpeed(void)
{
    s_TestGMTSpeedRun("eLocal - eMinute", CTime::eLocal, CTime::eMinute);
    s_TestGMTSpeedRun("eLocal - eHour  ", CTime::eLocal, CTime::eHour);
    s_TestGMTSpeedRun("eLocal - eMonth ", CTime::eLocal, CTime::eMonth);
    s_TestGMTSpeedRun("eLocal - eNone  ", CTime::eLocal, CTime::eNone);
    s_TestGMTSpeedRun("eGmt   - eNone  ", CTime::eGmt,   CTime::eNone);
}

#endif

//============================================================================
//
// TestTimeSpan
//
//============================================================================

static void s_TestTimeSpan(void)
{
    // Common constructors
    {{
        CTimeSpan t1(0,0,0,1,-2);
        assert(t1.AsString() == "0.999999998");
        CTimeSpan t2(0,0,0,-1,2);
        assert(t2.AsString() == "-0.999999998");
        CTimeSpan t3(0,0,0,0,-2);
        assert(t3.AsString() == "-0.000000002");
        CTimeSpan t4(0,0,0,0,2);
        assert(t4.AsString() == "0.000000002");
    }}
    {{
        CTimeSpan t1(2,3,4,5,6);
        assert(t1.GetCompleteHours()   == 51);
        assert(t1.GetCompleteMinutes() == (51*60+4));
        assert(t1.GetCompleteSeconds() == ((51*60+4)*60+5));
        assert(t1.GetNanoSecondsAfterSecond() == 6);
        assert(t1.AsString() == "183845.000000006");

        CTimeSpan t2(-2,-3,-4,-5,-6);
        assert(t2.GetCompleteHours()   == -51);
        assert(t2.GetCompleteMinutes() == -(51*60+4));
        assert(t2.GetCompleteSeconds() == -((51*60+4)*60+5));
        assert(t2.GetNanoSecondsAfterSecond() == -6);
        assert(t2.AsString() == "-183845.000000006");

        CTimeSpan t3(-2,+3,-4,-5,+6);
        assert(t3.GetCompleteHours()   == -45);
        assert(t3.GetCompleteMinutes() == -(45*60+4));
        assert(t3.GetCompleteSeconds() == -((45*60+4)*60+4));
        assert(t3.GetNanoSecondsAfterSecond() == -999999994);
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
            assert(t1_str.compare(t2_str) == 0);
        }
    }}

    // SetFormat/AsString with flag parameter test
    {{
        CTimeSpan t(123.456);
        {{
            string s = t.AsString("S.n");
            assert(s.compare("123.456000000") == 0);
        }}
        {{
            string s = t.AsString("$S.$n");
            assert(s.compare("$123.$456000000") == 0);
        }}
        {{
            string s = t.AsString(CTimeFormat("$S.$n", CTimeFormat::eNcbi));
            assert(s.compare("123.456000000") == 0);
        }}
    }}
}



//============================================================================
//
// TestTimeSpan -- AsSmartString()
//
//============================================================================

static void s_TestTimeSpan_AsSmartString(void)
{
    struct STest {
        CTimeSpan                        timespan;
        CTimeSpan::ESmartStringPrecision precision;
        ERound                           rounding;
        CTimeSpan::ESmartStringZeroMode  zeromode;
        const char*                      result;
    };

    // Shortcode for testing in smart mode -- (sec,ns) constructor only
    #define SMART_STR_TEST(round, sec, nanosec, result) \
            { CTimeSpan(sec, nanosec), CTimeSpan::eSSP_Smart, round, CTimeSpan::eSSZ_SkipZero,   result }, \
            { CTimeSpan(sec, nanosec), CTimeSpan::eSSP_Smart, round, CTimeSpan::eSSZ_NoSkipZero, result }
    // Same, but for an timespan object
    #define SMART_STR_TEST_TS(round, ts, result) \
            { ts, CTimeSpan::eSSP_Smart, round, CTimeSpan::eSSZ_SkipZero,   result }, \
            { ts, CTimeSpan::eSSP_Smart, round, CTimeSpan::eSSZ_NoSkipZero, result }

    static const STest s_Test[] = {

        // Smart mode for timespans < 1 min (CXX-4101)

        SMART_STR_TEST(eTrunc,  0,          0,  "0 seconds" ),
        SMART_STR_TEST(eTrunc,  1,          0,  "1 second" ),
        SMART_STR_TEST(eTrunc,  1,    1000000,  "1 second" ),
        SMART_STR_TEST(eTrunc,  0,        999,  "999 nanoseconds" ),
        SMART_STR_TEST(eTrunc,  0,       1000,  "1 microsecond" ),
        SMART_STR_TEST(eTrunc,  0,    1000000,  "1 millisecond" ),
        SMART_STR_TEST(eTrunc,  0,  100000000,  "100 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0, 1000000001,  "1 second" ),
        SMART_STR_TEST(eTrunc, 59,  900000000,  "59.9 seconds" ),
        SMART_STR_TEST(eTrunc, 12,   41000000,  "12 seconds" ),
        SMART_STR_TEST(eTrunc, 12,  341000000,  "12.3 seconds" ),
        SMART_STR_TEST(eTrunc,  1,  234100000,  "1.23 seconds" ),
        SMART_STR_TEST(eTrunc,  0,  123410000,  "123 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0,   12341000,  "12.3 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0,    1234100,  "1.23 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0,     123410,  "123 microseconds" ),
        SMART_STR_TEST(eTrunc,  0,      12341,  "12.3 microseconds" ),
        SMART_STR_TEST(eTrunc,  0,       1234,  "1.23 microseconds" ),
        SMART_STR_TEST(eTrunc,  0,        123,  "123 nanoseconds" ),
        SMART_STR_TEST(eTrunc,  0,         12,  "12 nanoseconds" ),
        SMART_STR_TEST(eTrunc,  0,  999000000,  "999 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0,  999500000,  "999 milliseconds" ),
        SMART_STR_TEST(eTrunc,  0,     999000,  "999 microseconds" ),
        SMART_STR_TEST(eTrunc,  0,     999500,  "999 microseconds" ),

        SMART_STR_TEST(eRound,  0,          0,  "0 seconds" ),
        SMART_STR_TEST(eRound,  1,          0,  "1 second" ),
        SMART_STR_TEST(eRound,  1,    1000000,  "1 second" ),
        SMART_STR_TEST(eRound,  0,        999,  "999 nanoseconds" ),
        SMART_STR_TEST(eRound,  0,       1000,  "1 microsecond" ),
        SMART_STR_TEST(eRound,  0,    1000000,  "1 millisecond" ),
        SMART_STR_TEST(eRound,  0,  100000000,  "100 milliseconds" ),
        SMART_STR_TEST(eRound,  0, 1000000001,  "1 second" ),
        SMART_STR_TEST(eRound, 59,  940000000,  "59.9 seconds" ),
        SMART_STR_TEST(eRound, 59,  950000000,  "1 minute" ),
        SMART_STR_TEST(eRound, 12,   50000000,  "12.1 seconds" ),
        SMART_STR_TEST(eRound, 12,  341000000,  "12.3 seconds" ),
        SMART_STR_TEST(eRound, 12,  351000000,  "12.4 seconds" ),
        SMART_STR_TEST(eRound,  1,  234100000,  "1.23 seconds" ),
        SMART_STR_TEST(eRound,  1,  235100000,  "1.24 seconds" ),
        SMART_STR_TEST(eRound,  0,  123410000,  "123 milliseconds" ),
        SMART_STR_TEST(eRound,  0,  123510000,  "124 milliseconds" ),
        SMART_STR_TEST(eRound,  0,   12341000,  "12.3 milliseconds" ),
        SMART_STR_TEST(eRound,  0,    1234100,  "1.23 milliseconds" ),
        SMART_STR_TEST(eRound,  0,     123410,  "123 microseconds" ),
        SMART_STR_TEST(eRound,  0,      12341,  "12.3 microseconds" ),
        SMART_STR_TEST(eRound,  0,       1234,  "1.23 microseconds" ),
        SMART_STR_TEST(eRound,  0,        123,  "123 nanoseconds" ),
        SMART_STR_TEST(eRound,  0,         12,  "12 nanoseconds" ),
        SMART_STR_TEST(eRound,  0,  999000000,  "999 milliseconds" ),
        SMART_STR_TEST(eRound,  0,  999500000,  "1 second" ),
        SMART_STR_TEST(eRound,  0,     999000,  "999 microseconds" ),
        SMART_STR_TEST(eRound,  0,     999500,  "1 millisecond" ),

        // Smart mode for timespans >= 1 min

        SMART_STR_TEST( eTrunc,    60,         0, "1 minute" ),
        SMART_STR_TEST( eTrunc,    61,         0, "1 minute 1 second" ),
        SMART_STR_TEST( eTrunc,   119, 900000000, "1 minute 59 seconds" ),
        SMART_STR_TEST( eTrunc,   600,         0, "10 minutes" ),
        SMART_STR_TEST( eTrunc,   629,         0, "10 minutes 29 seconds" ),
        SMART_STR_TEST( eTrunc,  3599, 900000000, "59 minutes 59 seconds" ),
        SMART_STR_TEST( eTrunc,  3600,         0, "1 hour" ),
        SMART_STR_TEST( eTrunc, 36000,         0, "10 hours" ),
        SMART_STR_TEST( eTrunc, 36059,         0, "10 hours" ),
        SMART_STR_TEST( eTrunc, 86400,         0, "1 day" ),

        SMART_STR_TEST( eRound,    60,         0, "1 minute" ),
        SMART_STR_TEST( eRound,    61,         0, "1 minute 1 second" ),
        SMART_STR_TEST( eRound,   119, 900000000, "2 minutes" ),
        SMART_STR_TEST( eRound,   600,         0, "10 minutes" ),
        SMART_STR_TEST( eRound,   629,         0, "10 minutes 29 seconds" ),
        SMART_STR_TEST( eRound,  3599, 900000000, "1 hour" ),
        SMART_STR_TEST( eRound,  3600,         0, "1 hour" ),
        SMART_STR_TEST( eRound, 36000,         0, "10 hours" ),
        SMART_STR_TEST( eRound, 36059,         0, "10 hours 1 minute" ),
        SMART_STR_TEST( eRound, 86400,         0, "1 day" ),

        SMART_STR_TEST_TS( eTrunc, CTimeSpan(29,23,59,1,0),       "29 days 23 hours" ),
        SMART_STR_TEST_TS( eRound, CTimeSpan(29,23,59,1,0),       "1 month" ),
        SMART_STR_TEST_TS( eTrunc, CTimeSpan(559,0,59,41,900500), "1 year 6 months"),
        SMART_STR_TEST_TS( eRound, CTimeSpan(559,0,59,41,900500), "1 year 6 months"),

        // zero time span
        { CTimeSpan(0,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"   },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months"  },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"    },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 minutes" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },

        // 1 second
        { CTimeSpan(1,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"   },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"    },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 minutes" },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },

        // 1 second 1 millisecond
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"   },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"    },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 minutes" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second 1 millisecond" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second 1000 microseconds" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second 1000000 nanoseconds" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second 1 millisecond" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second 1000 microseconds" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second 1000000 nanoseconds" },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(1,1000000), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },

        // 1 minute 1 second
        { CTimeSpan(61,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"  },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"   },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"  },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"   },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },
        { CTimeSpan(61,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute 1 second" },

        // 999 nanoseconds
        { CTimeSpan(0,999), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "999 nanoseconds" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Millisecond, eRound, CTimeSpan::eSSZ_SkipZero,   "0 seconds" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Microsecond, eRound, CTimeSpan::eSSZ_SkipZero,   "1 microsecond" },
        { CTimeSpan(0,999), CTimeSpan::eSSP_Nanosecond,  eRound, CTimeSpan::eSSZ_SkipZero,   "999 nanoseconds" },

        // 1000 nanoseconds
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 years"   },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 months"  },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 days"    },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 minutes" },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 seconds" },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,  "0 seconds" },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,  "1 microsecond" },
        { CTimeSpan(0,1000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,  "1000 nanoseconds" },

        // 1,000,000 nanoseconds
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero, "0 years"   },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero, "0 months"  },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero, "0 days"    },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero, "0 minutes" },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero, "0 seconds" },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero, "1 millisecond" },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero, "1000 microseconds" },
        { CTimeSpan(0,1000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero, "1000000 nanoseconds" },

        // 100,000,000 nanoseconds
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero, "0 years"   },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero, "0 months"  },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero, "0 days"    },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero, "0 minutes" },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero, "0 seconds" },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero, "100 milliseconds" },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero, "100000 microseconds" },
        { CTimeSpan(0,100000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero, "100000000 nanoseconds" },

        // 1,000,000,000 nanoseconds
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000000), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },

        // 1,000,000,001 nanoseconds
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 second 1 nanosecond" },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes" },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second"  },
        { CTimeSpan(0,1000000001), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 second 1 nanosecond" },

#if (SIZEOF_LONG == 8)
        // 10,000,000,000 nanoseconds
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Smart,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 seconds" },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Smart,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 seconds" },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"    },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"   },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"     },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 minutes"  },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 seconds" },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 seconds" },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 seconds" },
        { CTimeSpan(0,NCBI_CONST_INT8(10000000000)), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 seconds" },
#endif

        // 60 second
        { CTimeSpan(60,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"   },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"    },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"   },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"    },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },
        { CTimeSpan(60,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 minute"  },

        // 600 seconds
        { CTimeSpan(600,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"    },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months"   },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"     },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"    },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months"   },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"     },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },
        { CTimeSpan(600,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 minutes" },

        // 3600 seconds
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"  },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months" },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"  },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months" },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },
        { CTimeSpan(3600,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 hour"   },

        // 36000 seconds
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"  },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 days"   },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"  },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 days"   },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },
        { CTimeSpan(36000,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "10 hours" },

        // 86400 seconds
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 years"  },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "0 months" },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 years"  },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "0 months" },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },
        { CTimeSpan(86400,0), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 day"    },

        // Some long time
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds 900 microseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds 900500 nanoseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Year,        eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Month,       eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Day,         eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Minute,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Second,      eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Millisecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Microsecond, eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds 900 microseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Nanosecond,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds 900500 nanoseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision1,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision2,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision3,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision4,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision5,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision6,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision7,  eTrunc, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },

        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Year,        eRound, CTimeSpan::eSSZ_NoSkipZero, "2 years" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Month,       eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Day,         eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Minute,      eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Second,      eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Millisecond, eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds 1 millisecond" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Microsecond, eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds 901 microseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Nanosecond,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds 900500 nanoseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision1,  eRound, CTimeSpan::eSSZ_NoSkipZero, "2 years" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision2,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision3,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision4,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision5,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision6,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision7,  eRound, CTimeSpan::eSSZ_NoSkipZero, "1 year 6 months 14 days 0 hours 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Year,        eRound, CTimeSpan::eSSZ_SkipZero,   "2 years" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Month,       eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Day,         eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Minute,      eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Second,      eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Millisecond, eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds 1 millisecond" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Microsecond, eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds 901 microseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Nanosecond,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds 900500 nanoseconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision1,  eRound, CTimeSpan::eSSZ_SkipZero,   "2 years" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision2,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision3,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision4,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision5,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 1 hour" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision6,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },
        { CTimeSpan(559,0,59,41,900500), CTimeSpan::eSSP_Precision7,  eRound, CTimeSpan::eSSZ_SkipZero,   "1 year 6 months 14 days 59 minutes 41 seconds" },

        // stopper
        { CTimeSpan(0,0), CTimeSpan::eSSP_Smart, eTrunc, CTimeSpan::eSSZ_SkipZero,  NULL }
    };

    for (int i = 0;  s_Test[i].result;  i++) {
        string s = s_Test[i].timespan.AsSmartString(s_Test[i].precision, s_Test[i].rounding, s_Test[i].zeromode);
#if 1
        assert(s == s_Test[i].result);
#else
        if (s != s_Test[i].result) {
            cout << i << ".  " << s << "  != " << s_Test[i].result << endl;
        }
#endif
    }
}



//============================================================================
//
// TestTimeout
//
//============================================================================

static void s_TestTimeout(void)
{
    unsigned long ms;
    double        ds;
    CTimeout      tmo_default;
    CTimeout      tmo_infinite(CTimeout::eInfinite);
    CTimeout      tmo_zero(CTimeout::eZero);

    // Special values
    {{
        assert(tmo_default.IsDefault());
        assert(tmo_infinite.IsInfinite());
        assert(tmo_zero.IsZero());
        try {
            ds = tmo_default.GetAsDouble();
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            ds = tmo_infinite.GetAsDouble();
            _TROUBLE;
        }
        catch (CTimeException&) {}
        assert(!tmo_infinite.IsZero());
        try {
            // Default value cannot be check on zero, unlike infinite timeout.
            assert(tmo_default.IsZero());
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }}

    // Constructors
    {{
        CTimeout t1(3,400000);
        ms = t1.GetAsMilliSeconds();
        ds = t1.GetAsDouble();
        assert(ms == 3400);
        assert(ds > 3.3  &&  ds < 3.5);
        t1.Set(1,2000);
        assert(t1.GetAsMilliSeconds() == 1002);

        CTimeout t2(6.75);
        ms = t2.GetAsMilliSeconds();
        ds = t2.GetAsDouble();
        assert(ms > 6749  &&  ms < 6751);
        assert(ds > 6.74  &&  ds < 6.76);
        t1.Set(1,2000);
        assert(t1.GetAsMilliSeconds() == 1002);

        CTimeout t3(t1);
        assert(t3.GetAsMilliSeconds() == 1002);
        t3 = t1;
        assert(t1.GetAsMilliSeconds() == 1002);
        t3 = tmo_default;
        assert(t3.IsDefault());
        t3 = tmo_infinite;
        assert(t3.IsInfinite());
    }}
    {{
        CTimeSpan ts;
        CTimeSpan ts1(1,200);
        CTimeSpan ts2(1,200000);
        CTimeSpan ts3(-1,200);

        CTimeout t(ts1);
        // Nanoseconds truncates to 0 here
        ts = t.GetAsTimeSpan();
        assert(ts.GetCompleteSeconds() == 1);
        assert(ts.GetNanoSecondsAfterSecond() == 200); 
        assert(ts == ts1);

        // Microseconds correcly converts to nanoseconds
        t = ts2;
        ts = t.GetAsTimeSpan();
        assert(ts.GetCompleteSeconds() == 1);
        assert(ts.GetNanoSecondsAfterSecond() == 200000); 
        assert(ts == ts2);

        // Cannot copy from negative time span
        try {
            t = ts3;
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }}

    // Check Get()
    {{
        CTimeout t(123, 456);
        unsigned int sec, usec;
        t.Get(&sec, &usec);
        assert(sec  == 123);
        assert(usec == 456);
        t.Set(CTimeout::eInfinite);
        try {
            t.Get(&sec, &usec);
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }}

    // Comparison
    {{
        CTimeout t0;
        CTimeout ti(CTimeout::eInfinite);
        CTimeout t1(123.4);
        CTimeout t2(123.45);
        CTimeout t3(123.4);

        // Compare finite timeouts
        assert(t1 != t2);
        assert(t1 == t3);
        assert(t1 <  t2);
        assert(t2 >  t1);
        assert(t1 <= t2);
        assert(t2 >= t2);

        // Compare infinite timeout
        assert(t1 >  tmo_zero);
        assert(t1 <  tmo_infinite);
        assert(t1 <= tmo_infinite);
        assert(!(t1 > tmo_infinite));
        assert(ti == tmo_infinite);
        assert(tmo_infinite == tmo_infinite);

        // Default timeout is almost not comparable
        try {
            assert(t0 == tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(t1 < tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_default == tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_default == tmo_infinite);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_default != tmo_infinite);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(t1 != tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_default > tmo_zero);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_default < tmo_infinite);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_zero < tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            assert(tmo_infinite > tmo_default);
            _TROUBLE;
        }
        catch (CTimeException&) {}

        // Only >= and <= can be applied in some cases
        assert(tmo_infinite >= tmo_default);
        assert(tmo_default  <= tmo_infinite);
        assert(tmo_default  >= tmo_zero);
        assert(tmo_zero     <= tmo_default);
    }}

    // Assignment
    {{
        CTimeout t1(1.23);
        CTimeout t2(4.56);
        t1 = t2;
        assert(t1 == t2);
        t1 = tmo_default;
        assert(t1.IsDefault());
        try {
            // assert(t1 != t2);
            (t1 != t2);
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }}
}


//============================================================================
//
// DemoStopWatch
//
//============================================================================

static void s_DemoStopWatch(void)
{
    for ( int t = 0; t < 2; ++t ) {
        CStopWatch sw;
        assert(!sw.IsRunning());
        assert(sw.Elapsed() == 0);
        if ( t == 0 ) {
            sw.Start();
        }
        else {
            assert(sw.Restart() == 0);
        }
        assert(sw.IsRunning());
        sw.SetFormat("S.n");

        CNcbiOstrstream s;
        for (int i=0; i<10; i++) {
            s << sw << endl;
        }
        assert(sw.IsRunning());
        sw.Stop();
        assert(!sw.IsRunning());
        SleepMilliSec(500);
        sw.Start();
        for (int i=0; i<10; i++) {
            s << sw << endl;
        }
        //LOG_POST((string)CNcbiOstrstreamToString(s));
    }
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
        #if ENABLE_SPEED_TESTS
            s_TestGMTSpeed();
        #endif
        s_TestTimeSpan();
        s_TestTimeSpan_AsSmartString();
        s_TestTimeout();
        s_DemoStopWatch();
    } catch (CException& e) {
        ERR_POST(Fatal << e);
    }
    // Success
    return 0;
}
