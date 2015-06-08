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
        assert(CTime::MonthNameToNum("JAN")              == CTime::eJanuary); 
        assert(CTime::MonthNameToNum("JANUARY")          == 1); 
        assert(CTime::MonthNameToNum("Dec")              == CTime::eDecember); 
        assert(CTime::MonthNameToNum("December")         == 12); 
        assert(CTime::MonthNumToName(CTime::eJanuary)    == "January"); 
        assert(CTime::MonthNumToName(1, CTime::eAbbr)    == "Jan"); 
        assert(CTime::MonthNumToName(CTime::eDecember,
                                     CTime::eFull)       == "December"); 
        assert(CTime::MonthNumToName(12,CTime::eAbbr)    == "Dec"); 

        assert(CTime::DayOfWeekNameToNum("Sun")          == CTime::eSunday); 
        assert(CTime::DayOfWeekNameToNum("Sunday")       == 0); 
        assert(CTime::DayOfWeekNameToNum("SUN")          == CTime::eSunday); 
        assert(CTime::DayOfWeekNameToNum("SUNDAY")       == 0); 
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
        t1.SetTimeZone(CTime::eUTC);
        ts = CTimeSpan(long(t1.DiffSecond(t2)));
        assert(ts.GetAsDouble() == double(t2.TimeZoneOffset()));
    }}

    // CXX-195
    {{
        CTime time1("12/31/2007 20:00", "M/D/Y h:m");
        CTime time2("1/1/2008 20:00", "M/D/Y h:m");
        CTime time3("12/31/2007", "M/D/Y");
        CTime time4("1/1/2008", "M/D/Y");
        ERR_POST(Note << "time1=" << time1.AsString("M/D/Y h:m:s")
                      << "  time_t=" << time1.GetTimeT()
                      << "  time-zone: " << time1.TimeZoneOffset());
        ERR_POST(Note << "time2=" << time2.AsString("M/D/Y h:m:s")
                      << "  time_t=" << time2.GetTimeT()
                      << "  time-zone: " << time2.TimeZoneOffset());
        assert(time1.TimeZoneOffset() == time2.TimeZoneOffset());
        ERR_POST(Note << "time3=" << time3.AsString("M/D/Y h:m:s")
                      << "  time_t=" << time3.GetTimeT()
                      << "  time-zone: " << time3.TimeZoneOffset());
        assert(time2.TimeZoneOffset() == time3.TimeZoneOffset());
        ERR_POST(Note << "time4=" << time4.AsString("M/D/Y h:m:s")
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
        {"M/D/Y h:m:G",             0},
        {"M/D/Y h:m g",             0},
        {"smhyMD",                  1},
        {"y||||M++++D   h===ms",    1},
        {"   yM[][D   h:,.,.,ms  ", 1},
        {"\tkkkMy++D   h:ms\n",     1},
        {0,0}
    };

    for ( int hour = 0; hour < 24; ++hour ) {
        for (int i = 0;  s_Fmt[i].format;  i++) {
            const char* fmt = s_Fmt[i].format;
            
            bool is_UTC = (strchr(fmt, 'Z') ||  strchr(fmt, 'z'));
            CTime t1(2001, 4, 2, hour, 4, 5, 88888888,
                     is_UTC ? CTime::eUTC : CTime::eLocal);
            
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
            assert(CTime::ValidateString(t1_str, fmt));
        }
    }

    // Check against well-known dates
    {{
        const char fmtstr[] = "M/D/Y h:m:s Z W";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/2003 20:40:30 GMT Monday") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/1998 20:40:30 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2003 15:49:30 GMT Thursday") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2001 15:49:30 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/31/2002 23:59:59 GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("01/01/2003 00:00:00 GMT Wednesday") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/13/2002 12:34:56 GMT Friday") == 0);
        }}
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(CTimeFormat(fmtstr, CTimeFormat::fConf_UTC));
            string s = t.AsString();
            assert(s.compare("02/10/2003 20:40:30 UTC Monday") == 0);
        }}
    }}
    {{
        const char fmtstr[] = "M/D/Y H:m:s P Z W";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/2003 08:40:30 PM GMT Monday") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("02/10/1998 08:40:30 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2003 03:49:30 PM GMT Thursday") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("03/13/2001 03:49:30 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/31/2002 11:59:59 PM GMT Tuesday") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("01/01/2003 12:00:00 AM GMT Wednesday") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("12/13/2002 12:34:56 PM GMT Friday") == 0);
        }}
    }}
    {{
        const char fmtstr[] = "b d, Y H:m P";
        {{
            CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Feb 10, 2003 08:40 PM") == 0);
        }}
        {{
            CTime t(1998, 2, 10, 20, 40, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Feb 10, 1998 08:40 PM") == 0);
        }}
        {{
            CTime t(2003, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Mar 13, 2003 03:49 PM") == 0);
        }}
        {{
            CTime t(2001, 3, 13, 15, 49, 30, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Mar 13, 2001 03:49 PM") == 0);
        }}
        {{
            CTime t(2002, 12, 31, 23, 59, 59, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Dec 31, 2002 11:59 PM") == 0);
        }}
        {{
            CTime t(2003, 1, 1, 0, 0, 0, 0, CTime::eUTC);
            t.SetFormat(fmtstr);
            string s = t.AsString();
            assert(s.compare("Jan 1, 2003 12:00 AM") == 0);
        }}
        {{
            CTime t(2002, 12, 13, 12, 34, 56, 0, CTime::eUTC);
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
            t.Clear();
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001/2 00:00", "Y/D h:m");
            _TROUBLE; // month is not defined
            t.Clear();
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001/2", "Y/D");
            _TROUBLE; // month is not defined
            t.Clear();
        }
        catch (CTimeException&) {}

        try {
            CTime t("2001 00:00", "Y h:m");
            _TROUBLE; // month and day are not defined
            t.Clear();
        }
        catch (CTimeException&) {}

        try {
            CTime t("2 00:00", "M h:m");
            _TROUBLE; // year and day are not defined
            t.Clear();
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
            try {
                CTime t("2001...", CTimeFormat("Y/M/D", CTimeFormat::fMatch_Weak));
                s = t.AsString("M/D/Y h:m:s");
                assert(s.compare("01/01/2001 00:00:00") == 0);
            }
            catch (CTimeException&) {}
        }}
        {{
            try {
                CTime t("2001/01/02", CTimeFormat("Y...", CTimeFormat::fMatch_Weak));
                s = t.AsString("M/D/Y h:m:s");
                assert(s.compare("01/01/2001 00:00:00") == 0);
            }
            catch (CTimeException&) {}
        }}
        // Check that strict matching is by default.
        {{  
            try {
                CTime t("2001", "Y/M/D");
                _TROUBLE;  // by default used strict format matching
                t.Clear();
            }
            catch (CTimeException&) {}
            try {
                CTime t("2001/01/02", "Y");
                _TROUBLE;  // by default used strict format matching
                t.Clear();
            }
            catch (CTimeException&) {}
        }}
    }}

    // Observing spaces by default. Make sure that:
    //  - if fields in the format string are separated by one or more
    //    white spaces, then the fields in the parsed string also must
    //    be separated by one or more spaces;
    //  - if fields in the format string are adjacent, then the fields
    //    in the parsed string also must be adjacent.
    //
    // fMatch_IgnoreSpaces changes default behavior to backward-compatible.
    //
    // JIRA: CXX-5422
    {{
        CTimeFormat::EFlags ign = (CTimeFormat::EFlags)(CTimeFormat::fDefault | 
                                                        CTimeFormat::fMatch_IgnoreSpaces);

        // default flags
        assert(  CTime::ValidateString("01/01/2001",     CTimeFormat("M/D/Y")   ));
        assert(! CTime::ValidateString("01/01/2001   ",  CTimeFormat("M/D/Y")   ));
        assert(  CTime::ValidateString("01/01/2001   ",  CTimeFormat("M/D/Y ")  ));
        assert(  CTime::ValidateString("01012001",       CTimeFormat("MDY")     ));
        assert( !CTime::ValidateString("01 01 2001",     CTimeFormat("MDY")     ));
        assert(  CTime::ValidateString("01 01 2001",     CTimeFormat("M D Y")   ));
        assert( !CTime::ValidateString(" 01 01 2001",    CTimeFormat("M D Y")   ));
        assert( !CTime::ValidateString("01 01 2001 ",    CTimeFormat("M D Y")   ));
        assert(  CTime::ValidateString("01  01  2001",   CTimeFormat("M D Y")   ));
        assert( !CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y")   ));
        assert(  CTime::ValidateString("01\n01\t2001",   CTimeFormat("M D Y")   ));
        assert(  CTime::ValidateString("01\n  01\t2001", CTimeFormat("M D Y")   ));
        assert(  CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y ")  ));
        assert(  CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y  ") ));
        assert(  CTime::ValidateString(" 0101 2001",     CTimeFormat(" MD Y")   ));
        assert(  CTime::ValidateString("   0101 2001",   CTimeFormat(" MD Y")   ));
        assert( !CTime::ValidateString("   0101 2001  ", CTimeFormat("MDY")     ));
        assert( !CTime::ValidateString("01/01/2001\n\n", CTimeFormat("M/D/Y")   ));
        assert(  CTime::ValidateString("01/01/2001\n\n", CTimeFormat("M/D/Y ")  ));
        
        // "short" flags
        assert(  CTime::ValidateString("01/01/2001\n\n", CTimeFormat("M/D/Y",   
                                                         CTimeFormat::fMatch_ShortFormat)));
        assert(  CTime::ValidateString("01/01/2001",     CTimeFormat("M/D/Y ",   
                                                         CTimeFormat::fMatch_ShortTime)));
        // fMatch_IgnoreSpaces -- all valid
        assert(  CTime::ValidateString("01/01/2001",     CTimeFormat("M/D/Y",  ign) ));
        assert(  CTime::ValidateString("01/01/2001   ",  CTimeFormat("M/D/Y",  ign) ));
        assert(  CTime::ValidateString("01/01/2001   ",  CTimeFormat("M/D/Y ", ign) ));
        assert(  CTime::ValidateString("01012001",       CTimeFormat("MDY",    ign) ));
        assert(  CTime::ValidateString("01 01 2001",     CTimeFormat("MDY",    ign) ));
        assert(  CTime::ValidateString("01 01 2001",     CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString(" 01 01 2001",    CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01 01 2001 ",    CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01  01  2001",   CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01\n01\t2001",   CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01\n  01\t2001", CTimeFormat("M D Y",  ign) ));
        assert(  CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y ", ign) ));
        assert(  CTime::ValidateString("01  01  2001  ", CTimeFormat("M D Y  ",ign) ));
        assert(  CTime::ValidateString(" 0101 2001",     CTimeFormat(" MD Y",  ign) ));
        assert(  CTime::ValidateString("   0101 2001",   CTimeFormat(" MD Y",  ign) ));
        assert(  CTime::ValidateString("   0101 2001  ", CTimeFormat("MDY",    ign) ));
        assert(  CTime::ValidateString("01/01/2001\n\n", CTimeFormat("M/D/Y",  ign) ));
        assert(  CTime::ValidateString("01/01/2001\n\n", CTimeFormat("M/D/Y ", ign) ));
    }}

    // SetFormat/AsString with flag parameter test
    {{
        CTime t(2003, 2, 10, 20, 40, 30, 0, CTime::eUTC);
        string s;
        s = t.AsString("M/D/Y h:m:s");
        assert(s.compare("02/10/2003 20:40:30") == 0);
        s = t.AsString("MDY $M/$D/$Y $h:$m:$s hms");
        assert(s.compare("02102003 $02/$10/$2003 $20:$40:$30 204030") == 0);
        s = t.AsString(CTimeFormat("MDY $M/$D/$Y $h:$m:$s hms", CTimeFormat::eNcbi));
        assert(s.compare("MDY 02/10/2003 20:40:30 hms") == 0);
        s = t.AsString(CTimeFormat("$YY $MM $dd $hh $mm $ss", CTimeFormat::eNcbi));
        assert(s.compare("2003Y 02M 10d 20h 40m 30s") == 0);
    }}

    // CTimeFormat::GetPredefined() test
    {{
        CTime t(2003, 2, 10, 20, 40, 30, 123456789, CTime::eUTC);
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

    // Formats with floating numbers of seconds ("g", "G")
    {{
        CTime t;
        {{
            t.SetFormat("g");
            t = "3";
            assert(t.AsString()      == "3.0");
            assert(t.AsString("s.S") == "03.000000000");
            t = "3.45";
            assert(t.AsString()      == "3.45");
            assert(t.AsString("s.S") == "03.450000000");
            // long string with ignored symbols after 9th digit.
            t = "3.123456789123456";
            assert(t.AsString()      == "3.123456789");
            assert(t.AsString("s.S") == "03.123456789");
        }}
        {{
            t.SetFormat("G");
            t = "3";
            assert(t.AsString()      == "03.0");
            assert(t.AsString("s.S") == "03.000000000");
            t = "3.45";
            assert(t.AsString()      == "03.45");
            assert(t.AsString("s.S") == "03.450000000");
            // long string with ignored symbols after 9th digit.
            t = "3.123456789123456";
            assert(t.AsString()      == "03.123456789");
            assert(t.AsString("s.S") == "03.123456789");
        }}
    }}
}


//============================================================================
//
// Test GMT
//
//============================================================================

static void s_TestUTC(void)
{
    //------------------------------------------------------------------------
    // Local/universal times
    {{
        CTime::SetFormat("M/D/Y h:m:s Z");
        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eUTC);
        assert(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        assert(t2.AsString() == "03/12/2001 11:22:33 ");

        CTime::SetFormat(CTimeFormat("M/D/Y h:m:s Z", CTimeFormat::fConf_UTC));
        CTime t3(2001, 3, 12, 11, 22, 33, 999, CTime::eUTC);
        assert(t1.AsString() == "03/12/2001 11:22:33 UTC");

        // Current time
        CTime cur_tl(CTime::eCurrent, CTime::eLocal);
        CTime cur_tu(CTime::eCurrent, CTime::eUTC);
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
        CTime tgmt(CTime::eCurrent, CTime::eUTC,   CTime::eTZPrecisionDefault);
        CTime tloc(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        CTime t(timer);
        // Set the same time to all time objects
        tgmt.SetTimeT(timer);
        tloc.SetTimeT(timer);

        time_t g_ = tgmt.GetTimeT();
        time_t l_ = tloc.GetTimeT();
        time_t t_ = t.GetTimeT();

        ERR_POST(Note << STR(t));
        ERR_POST(Note << NStr::Int8ToString(g_/3600) + " - " +
                         NStr::Int8ToString(l_/3600) + " - " +
                         NStr::Int8ToString(t_/3600) + " - " +
                         NStr::Int8ToString(timer/3600) + "\n");

        assert(timer == t_);
        assert(timer == g_);
        // On the day of changing to summer/winter time, the local time
        // converted to UTC may differ from the value returned by time(0),
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

        // Check for underlying implementations not based on time_t (MSWIN)
        t.GetCurrentTimeT(&timer);
        time_t now = time(0);
        assert(timer <= now  &&  now - timer < 5);
    }}

    //------------------------------------------------------------------------
    // Test GetTimeTM
    {{   
        CTime tloc(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        struct tm l_ = tloc.GetTimeTM();
        CTime tmp(CTime::eCurrent, CTime::eUTC, CTime::eTZPrecisionDefault);
        assert(tmp.GetTimeZone() == CTime::eUTC);
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
                ERR_POST(Note << "First daylight saving day = " + STR(tw));
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
                ERR_POST(Note << "First non daylight saving day = " + STR(tw));
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

        t.SetTimeZone(CTime::eUTC);
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
// TestUTCSpeed
//
//============================================================================

#if ENABLE_SPEED_TESTS

static void s_TestUTCSpeedRun(string comment, CTime::ETimeZone tz, 
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
    ERR_POST(Note << comment + ", duration = " + NStr::DoubleToString(timer.Elapsed()) + " sec.");
}

static void s_TestUTCSpeed(void)
{
    s_TestUTCSpeedRun("eLocal - eMinute", CTime::eLocal, CTime::eMinute);
    s_TestUTCSpeedRun("eLocal - eHour  ", CTime::eLocal, CTime::eHour);
    s_TestUTCSpeedRun("eLocal - eMonth ", CTime::eLocal, CTime::eMonth);
    s_TestUTCSpeedRun("eLocal - eNone  ", CTime::eLocal, CTime::eNone);
    s_TestUTCSpeedRun("eUTC   - eNone  ", CTime::eUTC,   CTime::eNone);
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
        // This case test defaut behavior -- SetFormat() never used before!
        // and "-G" is format by default to init from strings in this case.
        {{
            CTimeSpan t;
            t = CTimeSpan("100.3") - CTimeSpan("123.4");
            assert(t == CTimeSpan("-23.1"));
            t = CTimeSpan("63.7")  + CTimeSpan("2.3");
            assert(t == CTimeSpan("66"));
            t = CTimeSpan("63.7")  - CTimeSpan("72.6");
            assert(t == CTimeSpan("-8.9"));

            t = "-123.4";
            t += CTimeSpan("1.0");
            assert(t == CTimeSpan("-122.4"));
            t += CTimeSpan("222.4");  
            assert(t == CTimeSpan("100.0") );
            t -= CTimeSpan("50.1");  
            assert(t == CTimeSpan("49.9"));
            t += CTimeSpan("0.1");  
            assert(t == CTimeSpan("50.0"));
            t += CTimeSpan("3.7");  
            assert(t == CTimeSpan("53.7"));
            t -= CTimeSpan("3.8");  
            assert(t == CTimeSpan("49.9"));
        }}
        // And now the same with another format "-S.n".
        {{
            CTimeSpan::SetFormat("-S.n");
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
    }}

    // Formats with floating numbers of seconds ("g", "G")
    {{
        CTimeSpan t;
        {{
            t.SetFormat("g");
            t = "123";
            assert(t.AsString()      == "3.0");
            assert(t.AsString("s.n") == "03.000000000");
            t = "123.45";
            assert(t.AsString()      == "3.45");
            assert(t.AsString("s.n") == "03.450000000");
            // long string with ignored symbols after 9th digit.
            t = "123.123456789123456";
            assert(t.AsString()      == "3.123456789");
            assert(t.AsString("s.n") == "03.123456789");
        }}
        {{
            t.SetFormat("G");
            t = "123";
            assert(t.AsString()      == "123.0");
            assert(t.AsString("S.n") == "123.000000000");
            t = "123.45";
            assert(t.AsString()      == "123.45");
            assert(t.AsString("S.n") == "123.450000000");
            // long string with ignored symbols after 9th digit.
            t = "123.123456789123456";
            assert(t.AsString()      == "123.123456789");
            assert(t.AsString("S.n") == "123.123456789");
        }}
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
            "G",
            "g",
            "-d h:m:s.n",
            "-H m:s",
            "-S",
            "-H",
            "-M",
            "-d",
            "-G",
            "-g",
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
        string s;
        s = t.AsString("S.n");
        assert(s.compare("123.456000000") == 0);
        s = t.AsString("$S.$n");
        assert(s.compare("$123.$456000000") == 0);
        s = t.AsString(CTimeFormat("$S.$n", CTimeFormat::eNcbi));
        assert(s.compare("123.456000000") == 0);
        s = t.AsString(CTimeFormat("$mm $ss", CTimeFormat::eNcbi));
        assert(s.compare("02m 03s") == 0);
    }}
}



//============================================================================
//
// TestTimeSpan -- AssignFromSmartString()
// Cases not covered by converting strings produced by AsSmartString().
//
//============================================================================

static void s_TestTimeSpan_AssignFromSmartString(void)
{
    struct STest {
        const char* str;
        CTimeSpan   result;
    };

    static const STest s_Test[] = {
        { "",                CTimeSpan()              },
        { " ",               CTimeSpan()              },
        { "\t",              CTimeSpan()              },
        { "\t ",             CTimeSpan()              },
        { "1s",              CTimeSpan(0,  0,  0,  1) },
        { "44s",             CTimeSpan(0,  0,  0, 44) },
        { "35m",             CTimeSpan(0,  0, 35,  0) }, 
        { "1d 13h 35m 44s",  CTimeSpan(1, 13, 35, 44) }, 
        { "1d13h35m44s",     CTimeSpan(1, 13, 35, 44) }, 
        { "13h 35m 44s",     CTimeSpan(0, 13, 35, 44) }, 
        { "35m 44s",         CTimeSpan(0,  0, 35, 44) }, 
        { "1d 13h 35m",      CTimeSpan(1, 13, 35,  0) }, 
        { "13h 35m",         CTimeSpan(0, 13, 35,  0) }, 
        { "1d 44s",          CTimeSpan(1,  0,  0, 44) }, 
        { "1d 13h",          CTimeSpan(1, 13,  0,  0) }, 
        { "1d 35m 44s",      CTimeSpan(1,  0, 35, 44) }, 
        { "1 day 35m 3.4s",  CTimeSpan(1,  0, 35,  3, 400000000) }, 
        { "1d 35m 44s 3ms",  CTimeSpan(1,  0, 35, 44,   3000000) }, 
        { "2mo 13h",         CTimeSpan(2*(long)kAverageSecondsPerMonth + 13*3600L, 0) }, 
        { "2y 1 nanosecond", CTimeSpan(2*(long)kAverageSecondsPerYear, 1) },

        // fractional values
        { "0.2ns",           CTimeSpan(0,  0,  0,  0,         0) },   // fraction of ns - rounding
        { "1.2ns",           CTimeSpan(0,  0,  0,  0,         1) },   // fraction of ns - rounding
        { "1.6ns",           CTimeSpan(0,  0,  0,  0,         2) },   // fraction of ns - rounding
        { "1.2ms",           CTimeSpan(0,  0,  0,  0,   1200000) },
        { "1.2us",           CTimeSpan(0,  0,  0,  0,      1200) },
        { "1.2s",            CTimeSpan(0,  0,  0,  1, 200000000) },
        { "1.003s",          CTimeSpan(0,  0,  0,  1,   3000000) },
        { "1.000004s",       CTimeSpan(0,  0,  0,  1,      4000) },
        { "1.000000005s",    CTimeSpan(0,  0,  0,  1,         5) },
        { "0.000000001s",    CTimeSpan(0,  0,  0,  0,         1) },
        { "1.s",             CTimeSpan(0,  0,  0,  1) },
        { "1.5m",            CTimeSpan(0,  0,  0, 90) },
        { "1.2h",            CTimeSpan(0,  1, 12,  0) },
        { "1.25h",           CTimeSpan(0,  1, 15,  0) },
        { "2.3d",            CTimeSpan(  2,  7, 12, 0) },  // 24*2 + 7.2 hours
        { "1.2mo",           CTimeSpan((long)kAverageSecondsPerMonth*12/10) },
        { "1.5mo 1.8d",      CTimeSpan((long)kAverageSecondsPerMonth*15/10 + 43*3600L + 12*60L) },  // 1.8d = 1d + 19.2h = 43h 12m
        { "1.2y",            CTimeSpan((long)kAverageSecondsPerYear*12/10) },
        { "2.3y",            CTimeSpan((long)kAverageSecondsPerYear*23/10) },
        { "1y 3.2s",         CTimeSpan((long)kAverageSecondsPerYear + 3, 200000000) },
        { " 1y3.2s ",        CTimeSpan((long)kAverageSecondsPerYear + 3, 200000000) },

        // stopper
        { NULL, CTimeSpan() }
    };

    CTimeSpan ts;

    for (int i = 0; s_Test[i].str;  i++) {
        ts.AssignFromSmartString(s_Test[i].str);
#if 1
        assert(ts == s_Test[i].result);
#else
        if (ts != s_Test[i].result) {
            cout << i << ".  " << s_Test[i].str << " != " << s_Test[i].result.AsString() << endl;
        }
#endif
    }

    // Tests that throw exceptions
    static const STest s_Test_Ex[] = {
        { "1",        CTimeSpan() },   // no time specifier
        { "s",        CTimeSpan() },   // no value
        { "2k",       CTimeSpan() },   // unknown specifier
        { "1d 2d",    CTimeSpan() },   // repetitions not allowed 
        { "1day 2d",  CTimeSpan() },   // repetitions not allowed 
        { "1.2s 3ns", CTimeSpan() },   // implicit repetitions not allowed 
        { "1..2s",    CTimeSpan() },   // double ..
        // stopper
        { NULL,       CTimeSpan() }
    };
    for (int i = 0; s_Test_Ex[i].str; i++) {
        try {
            ts.AssignFromSmartString(s_Test_Ex[i].str);
            _TROUBLE;
        }
        catch (CTimeException&) {}
    }
}



//============================================================================
//
// TestTimeSpan -- AsSmartString()
//
//============================================================================

static void s_TestTimeSpan_AsSmartString(void)
{
    typedef CTimeSpan TS;
    struct STest {
        TS                    timespan;
        TS::TSmartStringFlags flags;
        const char*           res_full;
        const char*           res_short;
    };

    //  Shorten some common flags combinations
    TS::TSmartStringFlags fTN = TS::fSS_Trunc | TS::fSS_NoSkipZero;
    TS::TSmartStringFlags fTS = TS::fSS_Trunc | TS::fSS_SkipZero;
    TS::TSmartStringFlags fRN = TS::fSS_Round | TS::fSS_NoSkipZero;
    TS::TSmartStringFlags fRS = TS::fSS_Round | TS::fSS_SkipZero;

    static const STest s_Test[] = {

        // Smart mode for timespans < 1 min (CXX-4101)
        { TS( 0,          0), fTS, "0 seconds",         "0s"     },
        { TS( 1,          0), fTS, "1 second",          "1s"     },
        { TS( 1,    1000000), fTS, "1 second",          "1s"     },
        { TS( 0,        999), fTS, "999 nanoseconds",   "999ns"  },
        { TS( 0,       1000), fTS, "1 microsecond",     "1us"    },
        { TS( 0,    1000000), fTS, "1 millisecond",     "1ms"    },
        { TS( 0,  100000000), fTS, "100 milliseconds",  "100ms"  },
        { TS( 0, 1000000001), fTS, "1 second",          "1s"     },
        { TS(59,  900000000), fTS, "59.9 seconds",      "59.9s"  },
        { TS(12,   41000000), fTS, "12 seconds",        "12s"    },
        { TS(12,  341000000), fTS, "12.3 seconds",      "12.3s"  },
        { TS( 1,  234100000), fTS, "1.23 seconds",      "1.23s"  },
        { TS( 0,  123410000), fTS, "123 milliseconds",  "123ms"  },
        { TS( 0,   12341000), fTS, "12.3 milliseconds", "12.3ms" },
        { TS( 0,    1234100), fTS, "1.23 milliseconds", "1.23ms" },
        { TS( 0,     123410), fTS, "123 microseconds",  "123us"  },
        { TS( 0,      12341), fTS, "12.3 microseconds", "12.3us" },
        { TS( 0,       1234), fTS, "1.23 microseconds", "1.23us" },
        { TS( 0,        123), fTS, "123 nanoseconds",   "123ns"  },
        { TS( 0,         12), fTS, "12 nanoseconds",    "12ns"   },
        { TS( 0,  999000000), fTS, "999 milliseconds",  "999ms"  },
        { TS( 0,  999500000), fTS, "999 milliseconds",  "999ms"  },
        { TS( 0,     999000), fTS, "999 microseconds",  "999us"  },
        { TS( 0,     999500), fTS, "999 microseconds",  "999us"  },

        { TS( 0,          0), fRS, "0 seconds",         "0s"     },
        { TS( 1,          0), fRS, "1 second",          "1s"     },
        { TS( 1,    1000000), fRS, "1 second",          "1s"     },
        { TS( 0,        999), fRS, "999 nanoseconds",   "999ns"  },
        { TS( 0,       1000), fRS, "1 microsecond",     "1us"    },
        { TS( 0,    1000000), fRS, "1 millisecond",     "1ms"    },
        { TS( 0,  100000000), fRS, "100 milliseconds",  "100ms"  },
        { TS( 0, 1000000001), fRS, "1 second",          "1s"     },
        { TS(59,  940000000), fRS, "59.9 seconds",      "59.9s"  },
        { TS(59,  950000000), fRS, "1 minute",          "1m"     },
        { TS(12,   50000000), fRS, "12.1 seconds",      "12.1s"  },
        { TS(12,  341000000), fRS, "12.3 seconds",      "12.3s"  },
        { TS(12,  351000000), fRS, "12.4 seconds",      "12.4s"  },
        { TS( 1,  234100000), fRS, "1.23 seconds",      "1.23s"  },
        { TS( 1,  235100000), fRS, "1.24 seconds",      "1.24s"  },
        { TS( 0,  123410000), fRS, "123 milliseconds",  "123ms"  },
        { TS( 0,  123510000), fRS, "124 milliseconds",  "124ms"  },
        { TS( 0,   12341000), fRS, "12.3 milliseconds", "12.3ms" },
        { TS( 0,    1234100), fRS, "1.23 milliseconds", "1.23ms" },
        { TS( 0,     123410), fRS, "123 microseconds",  "123us"  },
        { TS( 0,      12341), fRS, "12.3 microseconds", "12.3us" },
        { TS( 0,       1234), fRS, "1.23 microseconds", "1.23us" },
        { TS( 0,        123), fRS, "123 nanoseconds",   "123ns"  },
        { TS( 0,         12), fRS, "12 nanoseconds",    "12ns"   },
        { TS( 0,  999000000), fRS, "999 milliseconds",  "999ms"  },
        { TS( 0,  999500000), fRS, "1 second",          "1s"     },
        { TS( 0,     999000), fRS, "999 microseconds",  "999us"  },
        { TS( 0,     999500), fRS, "1 millisecond",     "1ms"    },

        // Smart mode for timespans >= 1 min
        { TS(   60,         0), fTS, "1 minute",              "1m"      },
        { TS(   61,         0), fTS, "1 minute 1 second",     "1m 1s"   },
        { TS(  119, 900000000), fTS, "1 minute 59 seconds",   "1m 59s"  },
        { TS(  600,         0), fTS, "10 minutes",            "10m"     },
        { TS(  629,         0), fTS, "10 minutes 29 seconds", "10m 29s" },
        { TS( 3599, 900000000), fTS, "59 minutes 59 seconds", "59m 59s" },
        { TS( 3600,         0), fTS, "1 hour",                "1h"      },
        { TS(36000,         0), fTS, "10 hours",              "10h"     },
        { TS(36059,         0), fTS, "10 hours",              "10h"     },
        { TS(86400,         0), fTS, "1 day",                 "1d"      },

        { TS(   60,         0), fRS, "1 minute",              "1m"      },
        { TS(   61,         0), fRS, "1 minute 1 second",     "1m 1s"   },
        { TS(  119, 900000000), fRS, "2 minutes",             "2m"      },
        { TS(  600,         0), fRS, "10 minutes",            "10m"     },
        { TS(  629,         0), fRS, "10 minutes 29 seconds", "10m 29s" },
        { TS( 3599, 900000000), fRS, "1 hour",                "1h"      },
        { TS( 3600,         0), fRS, "1 hour",                "1h"      },
        { TS(36000,         0), fRS, "10 hours",              "10h"     },
        { TS(36059,         0), fRS, "10 hours 1 minute",     "10h 1m"  },
        { TS(86400,         0), fRS, "1 day",                 "1d"      },

        { TS(29,23,59,1,0),                    fTS,  "29 days 23 hours", "29d 23h" },
        { TS(29,23,59,1,0),                    fRS,  "30 days",          "30d"     },
        { TS((long)kAverageSecondsPerMonth-1), fTS,  "30 days 10 hours", "30d 10h" },
        { TS((long)kAverageSecondsPerMonth-1), fRS,  "1 month",          "1mo"     },
        { TS((long)kAverageSecondsPerMonth),   fTS,  "1 month",          "1mo"     },
        { TS((long)kAverageSecondsPerMonth),   fRS,  "1 month",          "1mo"     },
        { TS((long)kAverageSecondsPerMonth+1), fTS,  "1 month",          "1mo" },
        { TS((long)kAverageSecondsPerMonth+1), fRS,  "1 month",          "1mo" },
        { TS(559,0,59,41,900500),              fTS,  "1 year 6 months",  "1y 6mo"  },
        { TS(559,0,59,41,900500),              fRS,  "1 year 6 months",  "1y 6mo"  },


        // zero time span
        { TS(0,0), fTN | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(0,0), fTN | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(0,0), fTN | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(0,0), fTN | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(0,0), fTN | TS::fSS_Second,      "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Millisecond, "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Microsecond, "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Nanosecond,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision1,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision2,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision3,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision4,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision5,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision6,  "0 seconds", "0s"  },
        { TS(0,0), fTN | TS::fSS_Precision7,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(0,0), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(0,0), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(0,0), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(0,0), fTS | TS::fSS_Second,      "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Millisecond, "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Microsecond, "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Nanosecond,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision1,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision2,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision3,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision4,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision5,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision6,  "0 seconds", "0s"  },
        { TS(0,0), fTS | TS::fSS_Precision7,  "0 seconds", "0s"  },

        // 1 second
        { TS(1,0), fTN | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(1,0), fTN | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(1,0), fTN | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(1,0), fTN | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(1,0), fTN | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Nanosecond,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision1,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision2,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision3,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision4,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision5,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision6,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Precision7,  "1 second",  "1s"  },
        { TS(1,0), fTN | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(1,0), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(1,0), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(1,0), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(1,0), fTS | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Nanosecond,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision1,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision2,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision3,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision4,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision5,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision6,  "1 second",  "1s"  },
        { TS(1,0), fTS | TS::fSS_Precision7,  "1 second",  "1s"  },

        // 1 second 1 millisecond
        { TS(1,1000000), fTN | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(1,1000000), fTN | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(1,1000000), fTN | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(1,1000000), fTN | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(1,1000000), fTN | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Millisecond, "1 second 1 millisecond", "1s 1ms" },
        { TS(1,1000000), fTN | TS::fSS_Microsecond, "1 second 1000 microseconds", "1s 1000us" },
        { TS(1,1000000), fTN | TS::fSS_Nanosecond,  "1 second 1000000 nanoseconds", "1s 1000000ns" },
        { TS(1,1000000), fTN | TS::fSS_Precision1,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision2,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision3,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision4,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision5,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision6,  "1 second",  "1s"  },
        { TS(1,1000000), fTN | TS::fSS_Precision7,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(1,1000000), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(1,1000000), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(1,1000000), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(1,1000000), fTS | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Millisecond, "1 second 1 millisecond", "1s 1ms" },
        { TS(1,1000000), fTS | TS::fSS_Microsecond, "1 second 1000 microseconds", "1s 1000us" },
        { TS(1,1000000), fTS | TS::fSS_Nanosecond,  "1 second 1000000 nanoseconds", "1s 1000000ns" },
        { TS(1,1000000), fTS | TS::fSS_Precision1,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision2,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision3,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision4,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision5,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision6,  "1 second",  "1s"  },
        { TS(1,1000000), fTS | TS::fSS_Precision7,  "1 second",  "1s"  },

        // 1 minute 1 second
        { TS(61,0), fTN | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(61,0), fTN | TS::fSS_Month,       "0 months", "0mo" },
        { TS(61,0), fTN | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(61,0), fTN | TS::fSS_Minute,      "1 minute", "1m"  },
        { TS(61,0), fTN | TS::fSS_Second,      "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Millisecond, "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Microsecond, "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Nanosecond,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision1,  "1 minute", "1m"  },
        { TS(61,0), fTN | TS::fSS_Precision2,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision3,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision4,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision5,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision6,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTN | TS::fSS_Precision7,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(61,0), fTS | TS::fSS_Month,       "0 months", "0mo" },
        { TS(61,0), fTS | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(61,0), fTS | TS::fSS_Minute,      "1 minute", "1m"  },
        { TS(61,0), fTS | TS::fSS_Second,      "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Millisecond, "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Microsecond, "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Nanosecond,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision1,  "1 minute", "1m"  },
        { TS(61,0), fTS | TS::fSS_Precision2,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision3,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision4,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision5,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision6,  "1 minute 1 second", "1m 1s" },
        { TS(61,0), fTS | TS::fSS_Precision7,  "1 minute 1 second", "1m 1s" },

        // 999 nanoseconds
        { TS(0,999), fTS | TS::fSS_Year,        "0 years",         "0y"    },
        { TS(0,999), fTS | TS::fSS_Month,       "0 months",        "0mo"   },
        { TS(0,999), fTS | TS::fSS_Day,         "0 days",          "0d"    },
        { TS(0,999), fTS | TS::fSS_Minute,      "0 minutes",       "0m"    },
        { TS(0,999), fTS | TS::fSS_Second,      "0 seconds",       "0s"    },
        { TS(0,999), fTS | TS::fSS_Millisecond, "0 seconds",       "0s"    },
        { TS(0,999), fTS | TS::fSS_Microsecond, "0 seconds",       "0s"    },
        { TS(0,999), fTS | TS::fSS_Nanosecond,  "999 nanoseconds", "999ns" },
        { TS(0,999), fRS | TS::fSS_Millisecond, "0 seconds",       "0s"    },
        { TS(0,999), fRS | TS::fSS_Microsecond, "1 microsecond",   "1us"   },
        { TS(0,999), fRS | TS::fSS_Nanosecond,  "999 nanoseconds", "999ns" },

        // 1000 nanoseconds
        { TS(0,1000), fTS | TS::fSS_Year,        "0 years",          "0y"  },
        { TS(0,1000), fTS | TS::fSS_Month,       "0 months",         "0mo" },
        { TS(0,1000), fTS | TS::fSS_Day,         "0 days",           "0d"  },
        { TS(0,1000), fTS | TS::fSS_Minute,      "0 minutes",        "0m"  },
        { TS(0,1000), fTS | TS::fSS_Second,      "0 seconds",        "0s"  },
        { TS(0,1000), fTS | TS::fSS_Millisecond, "0 seconds",        "0s"  },
        { TS(0,1000), fTS | TS::fSS_Microsecond, "1 microsecond",    "1us" },
        { TS(0,1000), fTS | TS::fSS_Nanosecond,  "1000 nanoseconds", "1000ns" },

        // 1,000,000 nanoseconds
        { TS(0,1000000), fTS | TS::fSS_Year,        "0 years",       "0y"  },
        { TS(0,1000000), fTS | TS::fSS_Month,       "0 months",      "0mo" },
        { TS(0,1000000), fTS | TS::fSS_Day,         "0 days",        "0d"  },
        { TS(0,1000000), fTS | TS::fSS_Minute,      "0 minutes",     "0m"  },
        { TS(0,1000000), fTS | TS::fSS_Second,      "0 seconds",     "0s"  },
        { TS(0,1000000), fTS | TS::fSS_Millisecond, "1 millisecond", "1ms" },
        { TS(0,1000000), fTS | TS::fSS_Microsecond, "1000 microseconds", "1000us" },
        { TS(0,1000000), fTS | TS::fSS_Nanosecond,  "1000000 nanoseconds", "1000000ns" },

        // 100,000,000 nanoseconds
        { TS(0,100000000), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(0,100000000), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(0,100000000), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(0,100000000), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(0,100000000), fTS | TS::fSS_Second,      "0 seconds", "0s"  },
        { TS(0,100000000), fTS | TS::fSS_Millisecond, "100 milliseconds", "100ms" },
        { TS(0,100000000), fTS | TS::fSS_Microsecond, "100000 microseconds", "100000us" },
        { TS(0,100000000), fTS | TS::fSS_Nanosecond,  "100000000 nanoseconds", "100000000ns" },

        // 1,000,000,000 nanoseconds
        { TS(0,1000000000), fTN | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(0,1000000000), fTN | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(0,1000000000), fTN | TS::fSS_Nanosecond,  "1 second",  "1s"  },
        { TS(0,1000000000), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(0,1000000000), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(0,1000000000), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(0,1000000000), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(0,1000000000), fTS | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(0,1000000000), fTS | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(0,1000000000), fTS | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(0,1000000000), fTS | TS::fSS_Nanosecond,  "1 second",  "1s"  },

        // 1,000,000,001 nanoseconds
        { TS(0,1000000001), fTN | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(0,1000000001), fTN | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(0,1000000001), fTN | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(0,1000000001), fTN | TS::fSS_Nanosecond,  "1 second 1 nanosecond", "1s 1ns" },
        { TS(0,1000000001), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(0,1000000001), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(0,1000000001), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(0,1000000001), fTS | TS::fSS_Minute,      "0 minutes", "0m"  },
        { TS(0,1000000001), fTS | TS::fSS_Second,      "1 second",  "1s"  },
        { TS(0,1000000001), fTS | TS::fSS_Millisecond, "1 second",  "1s"  },
        { TS(0,1000000001), fTS | TS::fSS_Microsecond, "1 second",  "1s"  },
        { TS(0,1000000001), fTS | TS::fSS_Nanosecond,  "1 second 1 nanosecond", "1s 1ns" },

#if (SIZEOF_LONG == 8)
        // 10,000,000,000 nanoseconds
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Smart,       "10 seconds", "10s" },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Year,        "0 years",    "0y"  },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Month,       "0 months",   "0mo" },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Day,         "0 days",     "0d"  },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Minute,      "0 minutes",  "0m"  },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Second,      "10 seconds", "10s" },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Millisecond, "10 seconds", "10s" },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Microsecond, "10 seconds", "10s" },
        { TS(0,NCBI_CONST_INT8(10000000000)), fTS | TS::fSS_Nanosecond,  "10 seconds", "10s" },
#endif

        // 60 second
        { TS(60,0), fTN | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(60,0), fTN | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(60,0), fTN | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(60,0), fTN | TS::fSS_Minute,      "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Second,      "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Millisecond, "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Microsecond, "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Nanosecond,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision1,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision2,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision3,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision4,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision5,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision6,  "1 minute",  "1m"  },
        { TS(60,0), fTN | TS::fSS_Precision7,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Year,        "0 years",   "0y"  },
        { TS(60,0), fTS | TS::fSS_Month,       "0 months",  "0mo" },
        { TS(60,0), fTS | TS::fSS_Day,         "0 days",    "0d"  },
        { TS(60,0), fTS | TS::fSS_Minute,      "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Second,      "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Millisecond, "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Microsecond, "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Nanosecond,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision1,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision2,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision3,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision4,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision5,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision6,  "1 minute",  "1m"  },
        { TS(60,0), fTS | TS::fSS_Precision7,  "1 minute",  "1m"  },

        // 600 seconds
        { TS(600,0), fTN | TS::fSS_Year,        "0 years",    "0y"  },
        { TS(600,0), fTN | TS::fSS_Month,       "0 months",   "0mo" },
        { TS(600,0), fTN | TS::fSS_Day,         "0 days",     "0d"  },
        { TS(600,0), fTN | TS::fSS_Minute,      "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Second,      "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Millisecond, "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Microsecond, "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Nanosecond,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision1,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision2,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision3,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision4,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision5,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision6,  "10 minutes", "10m" },
        { TS(600,0), fTN | TS::fSS_Precision7,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Year,        "0 years",    "0y"  },
        { TS(600,0), fTS | TS::fSS_Month,       "0 months",   "0mo" },
        { TS(600,0), fTS | TS::fSS_Day,         "0 days",     "0d"  },
        { TS(600,0), fTS | TS::fSS_Minute,      "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Second,      "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Millisecond, "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Microsecond, "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Nanosecond,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision1,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision2,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision3,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision4,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision5,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision6,  "10 minutes", "10m" },
        { TS(600,0), fTS | TS::fSS_Precision7,  "10 minutes", "10m" },

        // 3600 seconds
        { TS(3600,0), fTN | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(3600,0), fTN | TS::fSS_Month,       "0 months", "0mo" },
        { TS(3600,0), fTN | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(3600,0), fTN | TS::fSS_Minute,      "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Second,      "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Millisecond, "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Microsecond, "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Nanosecond,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision1,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision2,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision3,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision4,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision5,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision6,  "1 hour",   "1h"  },
        { TS(3600,0), fTN | TS::fSS_Precision7,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(3600,0), fTS | TS::fSS_Month,       "0 months", "0mo" },
        { TS(3600,0), fTS | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(3600,0), fTS | TS::fSS_Minute,      "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Second,      "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Millisecond, "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Microsecond, "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Nanosecond,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision1,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision2,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision3,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision4,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision5,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision6,  "1 hour",   "1h"  },
        { TS(3600,0), fTS | TS::fSS_Precision7,  "1 hour",   "1h"  },

        // 36000 seconds
        { TS(36000,0), fTN | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(36000,0), fTN | TS::fSS_Month,       "0 months", "0mo" },
        { TS(36000,0), fTN | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(36000,0), fTN | TS::fSS_Minute,      "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Second,      "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Millisecond, "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Microsecond, "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Nanosecond,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision1,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision2,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision3,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision4,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision5,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision6,  "10 hours", "10h" },
        { TS(36000,0), fTN | TS::fSS_Precision7,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(36000,0), fTS | TS::fSS_Month,       "0 months", "0mo" },
        { TS(36000,0), fTS | TS::fSS_Day,         "0 days",   "0d"  },
        { TS(36000,0), fTS | TS::fSS_Minute,      "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Second,      "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Millisecond, "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Microsecond, "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Nanosecond,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision1,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision2,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision3,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision4,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision5,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision6,  "10 hours", "10h" },
        { TS(36000,0), fTS | TS::fSS_Precision7,  "10 hours", "10h" },

        // 86400 seconds
        { TS(86400,0), fTN | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(86400,0), fTN | TS::fSS_Month,       "0 months", "0mo" },
        { TS(86400,0), fTN | TS::fSS_Day,         "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Minute,      "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Second,      "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Millisecond, "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Microsecond, "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Nanosecond,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision1,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision2,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision3,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision4,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision5,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision6,  "1 day",    "1d"  },
        { TS(86400,0), fTN | TS::fSS_Precision7,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Year,        "0 years",  "0y"  },
        { TS(86400,0), fTS | TS::fSS_Month,       "0 months", "0mo" },
        { TS(86400,0), fTS | TS::fSS_Day,         "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Minute,      "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Second,      "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Millisecond, "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Microsecond, "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Nanosecond,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision1,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision2,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision3,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision4,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision5,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision6,  "1 day",    "1d"  },
        { TS(86400,0), fTS | TS::fSS_Precision7,  "1 day",    "1d"  },

        // Some long time
        // "1 year 6 months 11 days 0 hours 59 minutes 41 seconds"
        // kAverageSecondsPerYear + kAverageSecondsPerMonth*6 + 11*24*3600 + 59*60 + 41 == 48289409 sec

        { TS(48289409,900500), fTN | TS::fSS_Year,        "1 year", "1y" },
        { TS(48289409,900500), fTN | TS::fSS_Month,       "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fTN | TS::fSS_Day,         "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTN | TS::fSS_Minute,      "1 year 6 months 11 days 0 hours 59 minutes", "1y 6mo 11d 0h 59m" },
        { TS(48289409,900500), fTN | TS::fSS_Second,      "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },
        { TS(48289409,900500), fTN | TS::fSS_Millisecond, "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },
        { TS(48289409,900500), fTN | TS::fSS_Microsecond, "1 year 6 months 11 days 0 hours 59 minutes 41 seconds 900 microseconds", "1y 6mo 11d 0h 59m 41s 900us" },
        { TS(48289409,900500), fTN | TS::fSS_Nanosecond,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds 900500 nanoseconds", "1y 6mo 11d 0h 59m 41s 900500ns" },
        { TS(48289409,900500), fTN | TS::fSS_Precision1,  "1 year", "1y" },
        { TS(48289409,900500), fTN | TS::fSS_Precision2,  "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fTN | TS::fSS_Precision3,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTN | TS::fSS_Precision4,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTN | TS::fSS_Precision5,  "1 year 6 months 11 days 0 hours 59 minutes", "1y 6mo 11d 0h 59m" },
        { TS(48289409,900500), fTN | TS::fSS_Precision6,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },
        { TS(48289409,900500), fTN | TS::fSS_Precision7,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },

        { TS(48289409,900500), fTS | TS::fSS_Year,        "1 year", "1y" },
        { TS(48289409,900500), fTS | TS::fSS_Month,       "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fTS | TS::fSS_Day,         "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTS | TS::fSS_Minute,      "1 year 6 months 11 days 59 minutes", "1y 6mo 11d 59m" },
        { TS(48289409,900500), fTS | TS::fSS_Second,      "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },
        { TS(48289409,900500), fTS | TS::fSS_Millisecond, "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },
        { TS(48289409,900500), fTS | TS::fSS_Microsecond, "1 year 6 months 11 days 59 minutes 41 seconds 900 microseconds", "1y 6mo 11d 59m 41s 900us" },
        { TS(48289409,900500), fTS | TS::fSS_Nanosecond,  "1 year 6 months 11 days 59 minutes 41 seconds 900500 nanoseconds", "1y 6mo 11d 59m 41s 900500ns" },
        { TS(48289409,900500), fTS | TS::fSS_Precision1,  "1 year", "1y" },
        { TS(48289409,900500), fTS | TS::fSS_Precision2,  "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fTS | TS::fSS_Precision3,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTS | TS::fSS_Precision4,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fTS | TS::fSS_Precision5,  "1 year 6 months 11 days 59 minutes", "1y 6mo 11d 59m" },
        { TS(48289409,900500), fTS | TS::fSS_Precision6,  "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },
        { TS(48289409,900500), fTS | TS::fSS_Precision7,  "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },

        { TS(48289409,900500), fRN | TS::fSS_Year,        "2 years", "2y" },
        { TS(48289409,900500), fRN | TS::fSS_Month,       "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fRN | TS::fSS_Day,         "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fRN | TS::fSS_Minute,      "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRN | TS::fSS_Second,      "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },
        { TS(48289409,900500), fRN | TS::fSS_Millisecond, "1 year 6 months 11 days 0 hours 59 minutes 41 seconds 1 millisecond", "1y 6mo 11d 0h 59m 41s 1ms" },
        { TS(48289409,900500), fRN | TS::fSS_Microsecond, "1 year 6 months 11 days 0 hours 59 minutes 41 seconds 901 microseconds", "1y 6mo 11d 0h 59m 41s 901us" },
        { TS(48289409,900500), fRN | TS::fSS_Nanosecond,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds 900500 nanoseconds", "1y 6mo 11d 0h 59m 41s 900500ns" },
        { TS(48289409,900500), fRN | TS::fSS_Precision1,  "2 years", "2y" },
        { TS(48289409,900500), fRN | TS::fSS_Precision2,  "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fRN | TS::fSS_Precision3,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fRN | TS::fSS_Precision4,  "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRN | TS::fSS_Precision5,  "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRN | TS::fSS_Precision6,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },
        { TS(48289409,900500), fRN | TS::fSS_Precision7,  "1 year 6 months 11 days 0 hours 59 minutes 41 seconds", "1y 6mo 11d 0h 59m 41s" },

        { TS(48289409,900500), fRS | TS::fSS_Year,        "2 years", "2y" },
        { TS(48289409,900500), fRS | TS::fSS_Month,       "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fRS | TS::fSS_Day,         "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fRS | TS::fSS_Minute,      "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRS | TS::fSS_Second,      "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },
        { TS(48289409,900500), fRS | TS::fSS_Millisecond, "1 year 6 months 11 days 59 minutes 41 seconds 1 millisecond", "1y 6mo 11d 59m 41s 1ms" },
        { TS(48289409,900500), fRS | TS::fSS_Microsecond, "1 year 6 months 11 days 59 minutes 41 seconds 901 microseconds", "1y 6mo 11d 59m 41s 901us" },
        { TS(48289409,900500), fRS | TS::fSS_Nanosecond,  "1 year 6 months 11 days 59 minutes 41 seconds 900500 nanoseconds", "1y 6mo 11d 59m 41s 900500ns" },
        { TS(48289409,900500), fRS | TS::fSS_Precision1,  "2 years", "2y" },
        { TS(48289409,900500), fRS | TS::fSS_Precision2,  "1 year 6 months", "1y 6mo" },
        { TS(48289409,900500), fRS | TS::fSS_Precision3,  "1 year 6 months 11 days", "1y 6mo 11d" },
        { TS(48289409,900500), fRS | TS::fSS_Precision4,  "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRS | TS::fSS_Precision5,  "1 year 6 months 11 days 1 hour", "1y 6mo 11d 1h" },
        { TS(48289409,900500), fRS | TS::fSS_Precision6,  "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },
        { TS(48289409,900500), fRS | TS::fSS_Precision7,  "1 year 6 months 11 days 59 minutes 41 seconds", "1y 6mo 11d 59m 41s" },

        // stopper
        { TS(0,0), TS::fSS_Default,  NULL, NULL }
    };

    for (int i = 0;  s_Test[i].res_full;  i++) {

        // timespan -> string

        string s_full  = s_Test[i].timespan.AsSmartString(s_Test[i].flags | TS::fSS_Full);
        string s_short = s_Test[i].timespan.AsSmartString(s_Test[i].flags | TS::fSS_Short);
#if 1
        assert(s_full  == s_Test[i].res_full);
        assert(s_short == s_Test[i].res_short);
#else
        if (s_full != s_Test[i].res_full) {
            cout << i << ".  Print:" << s_full << " != " << s_Test[i].res_full << endl;
        }
        if (s_short != s_Test[i].res_short) {
            cout << i << ".  Print:" << s_short << " != " << s_Test[i].res_short << endl;
        }
#endif

        // string -> timespan -> string (AssignFromSmartString() test)

        CTimeSpan ts;
        ts.AssignFromSmartString(s_full);
        string s_full_new  = ts.AsSmartString(s_Test[i].flags | TS::fSS_Full);
        ts.AssignFromSmartString(s_short);
        string s_short_new = ts.AsSmartString(s_Test[i].flags | TS::fSS_Short);
#if 1
        assert(s_full_new  == s_Test[i].res_full);
        assert(s_short_new == s_Test[i].res_short);
#else
        if (s_full_new != s_Test[i].res_full) {
            cout << i << ".  Assign:" << s_full_new << " != " << s_Test[i].res_full << endl;
        }
        if (s_short_new != s_Test[i].res_short) {
            cout << i << ".   Assign:" << s_short_new << " != " << s_Test[i].res_short << endl;
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
            tmo_default.GetAsDouble();
            _TROUBLE;
        }
        catch (CTimeException&) {}
        try {
            tmo_infinite.GetAsDouble();
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
            (void)(t1 != t2);
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
        //ERR_POST(Note << (string)CNcbiOstrstreamToString(s));
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
        s_TestUTC();
        #if ENABLE_SPEED_TESTS
            s_TestUTCSpeed();
        #endif
        s_TestTimeSpan();
        s_TestTimeSpan_AsSmartString();
        s_TestTimeSpan_AssignFromSmartString();
        s_TestTimeout();
        s_DemoStopWatch();
    } catch (CException& e) {
        ERR_FATAL(e);
    }
    // Success
    return 0;
}
