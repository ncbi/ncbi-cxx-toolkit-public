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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test for CTime class in multithreaded environment
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/test_mt.hpp>
#include <stdlib.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

#if defined(NCBI_OS_DARWIN)  ||  defined(NCBI_OS_BSD)
#  define TIMEZONE_IS_UNDEFINED  1
#endif


//=============================================================================
//
// TestMisc
//
//=============================================================================

static void s_TestMisc(int idx)
{
    // AsString()
    {{
        CTime t1;
        assert(t1.AsString() == "");

        CTime t2(2000, 365 / 2);
        t2.SetFormat("M/D/Y h:m:s");
        assert(t2.AsString() == "06/30/2000 00:00:00");
    }}
    
    // Year 2000 problem
    {{
        CTime t(1999, 12, 30); 
        t.SetFormat("M/D/Y");
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

    // String assignment
    {{
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("02/15/2000 01:12:33");
        assert(t.AsString() == "02/15/2000 01:12:33");
        t = "3/16/2001 02:13:34";
        assert(t.AsString() == "03/16/2001 02:13:34");
    }}

    CTime::SetFormat("M/D/Y h:m:s.S");

    // Adding Nanoseconds
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 59, 59, 999999995);
             ti <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
             t = ti, ti.AddNanoSecond(2)) {
        }
        assert(t.AsString() == "01/01/2000 00:00:00.000000003");
    }}

    CTime::SetFormat("M/D/Y h:m:s");

    // Adding seconds
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 59, 5);
             ti <= CTime(2000, 1, 1, 0, 1, 20);
             t = ti, ti.AddSecond(11)) {
        }
        assert(t.AsString() == "01/01/2000 00:01:17");
    }}

    // Adding minutes
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 45);
             ti <= CTime(2000, 1, 1, 0, 15);
             t = ti, ti.AddMinute(11)) {
        }
        assert(t.AsString() == "01/01/2000 00:07:00");
    }}

    // Adding hours
    {{
        CTime t;
        for (CTime ti(1999, 12, 31); ti <= CTime(2000, 1, 1, 15);
             t = ti, ti.AddHour(11)) {
        }
        assert(t.AsString() == "01/01/2000 09:00:00");
    }}

    // Adding months
    {{
        CTime t;
        for (CTime ti(1998, 12, 29); ti <= CTime(1999, 4, 1);
             t = ti, ti.AddMonth()) {
        }
        assert(t.AsString() == "03/28/1999 00:00:00");
    }}
    
    // Difference
    {{
        CTime t1(2000, 10, 1, 12, 3, 45,1);
        CTime t2(2000, 10, 2, 14, 55, 1,2);

        assert((t2.DiffDay(t1)-1.12) < 0.01);
        assert((t2.DiffHour(t1)-26.85) < 0.01);
        assert((t2.DiffMinute(t1)-1611.27) < 0.01);
        assert(t2.DiffSecond(t1) == 96676);
    }}

    // Database time formats
    {{
        CTime t(2000, 1, 1, 1, 1, 1, 10000000);
        CTime::SetFormat("M/D/Y h:m:s.S");
        
        TDBTimeI dbi = t.GetTimeDBI();

        CTime::SetFormat("M/D/Y h:m:s");
        dbi.days = 37093;
        dbi.time = 12301381;
        t.SetTimeDBI(dbi);
        assert(t.AsString() == "07/23/2001 11:23:24");
    }}
}


//=============================================================================
//
// TestFormats
//
//=============================================================================


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

    // Partialy defined time
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


//=============================================================================
//
// TestGMT
//
//=============================================================================

static void s_TestGMT(int idx)
{
    // Write time in timezone format
    {{   
        CTime::SetFormat("M/D/Y h:m:s Z");
        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eGmt);
        assert(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        assert(t2.AsString() == "03/12/2001 11:22:33 ");
    }}

    // Process timezone string
    {{   
        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t="03/12/2001 11:22:33 GMT";
        assert(t.AsString() == "03/12/2001 11:22:33 GMT");
        t="03/12/2001 11:22:33 ";
        assert(t.AsString() == "03/12/2001 11:22:33 ");
    }}

    // Day of week
    {{   
        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i=0; t<=CTime(2001, 4, 10); t.AddDay(),i++) {
            assert(t.DayOfWeek() == (i%7));
        }
    }}

    // Test GetTimeT
    {{  
        time_t timer = time(0);
        CTime tgmt(CTime::eCurrent, CTime::eGmt, CTime::eTZPrecisionDefault);
        CTime tloc(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        CTime t(timer);
        // Set the same time to all time objects
        tgmt.SetTimeT(timer);
        tloc.SetTimeT(timer);

        assert(timer == t.GetTimeT());
        assert(timer == tgmt.GetTimeT());
        // On the day of changing to summer/winter time, the local time
        // converted to GMT may differ from the value returned by time(0),
        // because in the common case API don't know if DST is in effect for
        // specified local time or not (see mktime()).
        time_t l_ = tloc.GetTimeT();
        if (timer != l_ ) {
            if ( abs((int)(timer - l_)) > 3600 )
                assert(timer == l_);
        }
    }}

    // Test TimeZoneOffset (1) -- EST timezone only
    {{   
        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);
        assert(tw.TimeZoneOffset() / 3600 == -5);
        assert(ts.TimeZoneOffset()/3600 == -4);
    }}

    // Test TimeZoneOffset (2) -- EST timezone only
    {{   
        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        assert(tw.TimeZoneOffset() / 3600 == -4);
        assert(ts.TimeZoneOffset() / 3600 == -5);
    }}

    // Test AdjustTime
    {{   
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("03/11/2007 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eTZPrecisionDefault);

        // GMT
        t.SetTimeZone(CTime::eGmt);
        tn = t;
        tn.AddDay(5);  
        assert(tn.AsString() == "03/16/2007 01:01:00");
        tn = t;
        tn.AddDay(40); 
        assert(tn.AsString() == "04/20/2007 01:01:00");

        // Local eNone
        t.SetTimeZone(CTime::eLocal);
        t.SetTimeZonePrecision(CTime::eNone);
        tn = t;
        tn.AddDay(5);
        assert(tn.AsString() == "03/16/2007 01:01:00");
        tn = t;
        tn.AddDay(40);
        assert(tn.AsString() == "04/20/2007 01:01:00");

        //Local eMonth
        t.SetTimeZonePrecision(CTime::eMonth);
        tn = t;
        tn.AddDay(5);
        assert(tn.AsString() == "03/16/2007 01:01:00");
        tn = t; 
        tn.AddMonth(-1);
        assert(tn.AsString() == "02/11/2007 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        assert(tn.AsString() == "04/11/2007 02:01:00");

        // Local eDay
        t.SetTimeZonePrecision(CTime::eDay);
        tn = t;
        tn.AddDay(-1); 
        assert(tn.AsString() == "03/10/2007 01:01:00");
        tn.AddDay();   
        assert(tn.AsString() == "03/11/2007 01:01:00");
        tn = t;
        tn.AddDay(); 
        assert(tn.AsString() == "03/12/2007 02:01:00");

        // Local eHour
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


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestRegApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
};


bool CTestRegApp::Thread_Run(int idx)
{
    // Run tests
    try {
        for (int i=0; i<3; i++) {
            s_TestMisc(idx);
            s_TestFormats();
            s_TestGMT(idx);
        }
    } catch (CException& e) {
        ERR_POST(Fatal << e);
        return false;
    }

    return true;
}

bool CTestRegApp::TestApp_Init(void)
{
    LOG_POST("Testing NCBITIME " +
             NStr::UIntToString(s_NumThreads) + " threads...");

    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    return true;
}

bool CTestRegApp::TestApp_Exit(void)
{
    LOG_POST("Test completed successfully!\n");
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Reinit global timezone variables
    tzset();
    return CTestRegApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
