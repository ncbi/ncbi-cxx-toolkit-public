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
 * Authors:  Anton Butanayev, Denis Vakatov
 * ---------------------------------------------------------------------------
 * $Log$
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


#include <corelib/ncbitime.hpp>
#include <stdio.h>


USING_NCBI_SCOPE;


//=============================================================================
//
// TestMisc
//
//=============================================================================

static void s_TestMisc(void)
{
    cout << "---------------------------" << endl;
    cout << "Test Misc" << endl;
    cout << "---------------------------" << endl << endl;

    CTime t1(CTime::eCurrent);
    cout << "[" << t1.AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (++t1).AsString() << "]" << endl;
    cout << "[" << (++t1).AsString() << "]" << endl;

    CTime t2;
    cout << "[" << t2.AsString() << "]" << endl;
    _ASSERT(t2.AsString() == "");

    CTime t3(2000, 365 / 2);
    cout << "[" << t3.AsString() << "]" << endl;
    _ASSERT(t3.AsString() == "06/30/2000 00:00:00");
    cout << endl;

    CTime::SetFormat("M/D/Y");

    cout << "Year 2000 problem:" << endl << endl;
    CTime t4(1999, 12, 30); 
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "12/31/1999");
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "01/01/2000");
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "01/02/2000");
    t4="02/27/2000";
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "02/28/2000");
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "02/29/2000");
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "03/01/2000");
    t4++; cout << "[" << t4.AsString() << "] " << endl;
    _ASSERT(t4.AsString() == "03/02/2000");
    cout << endl;

    CTime::SetFormat("M/D/Y h:m:s");
    cout << "String assignment:" << endl;
    try {
        CTime t5("02/15/2000 01:12:33");
        cout << "[" << t5.AsString() << "]" << endl;
        _ASSERT(t5.AsString() == "02/15/2000 01:12:33");
        t5 = "3/16/2001 02:13:34";
        cout << "[" << t5.AsString() << "]" << endl;
        _ASSERT(t5.AsString() == "03/16/2001 02:13:34");
    } catch (exception& e) {
        cout << e.what() << endl;
    }
    cout << endl;

    CTime::SetFormat("M/D/Y h:m:s.S");

    cout << "Adding Nanoseconds:" << endl;
    CTime t;
    for (CTime t6(1999, 12, 31, 23, 59, 59, 999999995);
         t6 <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
         t = t6, t6.AddNanoSecond(2)) {
         cout << "[" << t6.AsString() << "] " << endl;
    }
    _ASSERT(t.AsString() == "01/01/2000 00:00:00.000000003");
    cout << endl;

    cout << "Current time with NS (10 cicles)" << endl;
    for (int i = 0; i < 10; i++) {
         t.SetCurrent();
         cout << "[" << t.AsString() << "] " << endl;
    }
    cout << endl;

    CTime::SetFormat("M/D/Y h:m:s");

    cout << "Adding seconds:" << endl;
    for (CTime t6(1999, 12, 31, 23, 59, 5);
         t6 <= CTime(2000, 1, 1, 0, 1, 20);
         t = t6, t6.AddSecond(11)) {
         cout << "[" << t6.AsString() << "] " << endl;
    }
    _ASSERT(t.AsString() == "01/01/2000 00:01:17");
    cout << endl;

    cout << "Adding minutes:" << endl;
    for (CTime t61(1999, 12, 31, 23, 45);
         t61 <= CTime(2000, 1, 1, 0, 15);
         t61.AddMinute(11)) {
         cout << "[" << t61.AsString() << "] " << endl;
    }
    cout << endl;

    cout << "Adding hours:" << endl;
    for (CTime t62(1999, 12, 31);
         t62 <= CTime(2000, 1, 1, 15);
         t62.AddHour(11)) {
         cout << "[" << t62.AsString() << "] " << endl;
    }
    cout << endl;

    cout << "Adding months:" << endl;
    for (CTime t63(1998, 12, 29);
         t63 <= CTime(1999, 4, 1);
         t63.AddMonth()) {
         cout << "[" << t63.AsString() << "] " << endl;
    }
    cout << endl;

    CTime t7(2000, 10, 1, 12, 3, 45,1);
    CTime t8(2000, 10, 2, 14, 55, 1,2);

    cout << "[" << t7.AsString() << " - " << t8.AsString() << "]" << endl;
    printf("DiffDay        = %.2f\n", t8.DiffDay   (t7));
    _ASSERT((t8.DiffDay(t7)-1.12) < 0.01);
    printf("DiffHour       = %.2f\n", t8.DiffHour  (t7));
    _ASSERT((t8.DiffHour(t7)-26.85) < 0.01);
    printf("DiffMinute     = %.2f\n", t8.DiffMinute(t7));
    _ASSERT((t8.DiffMinute(t7)-1611.27) < 0.01);
    printf("DiffSecond     = %d\n",   t8.DiffSecond(t7));
    _ASSERT(t8.DiffSecond(t7) == 96676);
    printf("DiffNanoSecond = %.0f\n", t8.DiffNanoSecond(t7));
    //    _ASSERT(t8.DiffNanoSecond(t7) == 96676000000001);

    CTime t9(2000, 1, 1, 1, 1, 1, 10000000);
    CTime::SetFormat("M/D/Y h:m:s.S");

    cout << endl << "DB time formats [" << t9.AsString() << "]" << endl;

    TDBTimeU dbu = t9.GetTimeDBU();
    TDBTimeI dbi = t9.GetTimeDBI();

    cout << "DBU days             = " << dbu.days << endl;
    cout << "DBU time (min)       = " << dbu.time << endl;
    cout << "DBI days             = " << dbi.days << endl;
    cout << "DBI time (1/300 sec) = " << dbi.time << endl;
    cout << endl;
    CTime t10;
    t10.SetTimeDBU(dbu);
    cout << "Time from DBU        = " << t10.AsString() << endl;
    t10.SetTimeDBI(dbi);
    cout << "Time from DBI        = " << t10.AsString() << endl;

    CTime::SetFormat("M/D/Y h:m:s");
    dbi.days = 37093;
    dbi.time = 12301381;
    t10.SetTimeDBI(dbi);
    cout << "Time from DBI        = " << t10.AsString() << endl;
    _ASSERT(t10.AsString() == "07/23/2001 11:23:24");

    cout << endl;
}


//=============================================================================
//
// TestFormats
//
//=============================================================================

static void s_TestFormats(void)
{
    static const char* s_Fmt[] = {
        "M/D/Y h:m:s",
        "M/D/Y h:m:s.S",
        "M/D/y h:m:s",
        "M/DY  h:m:s",
        "M/Dy  h:m:s",
        "M/D/Y hm:s",
        "M/D/Y h:ms",
        "M/D/Y hms",
        "MD/y  h:m:s",
        "MD/Y  h:m:s",
        "MYD   m:h:s",
        "M/D/Y smh",
        "YMD   h:sm",
        "yDM   h:ms",
        "yMD   h:ms",
        "smhyMD",
        "y||||M++++D   h===ms",
        "   yM[][D   h:,.,.b,bms  ",
        "\tkkkMy++D   h:ms\n",
        0
    };

    cout << "---------------------------" << endl;
    cout << "Test Formats" << endl;
    cout << "---------------------------" << endl << endl;

    for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
        CTime t1(2001, 1, 2, 3, 4, 0);

        CTime::SetFormat(*fmt);
        string t1_str = t1.AsString();
        cout << "[" << t1_str << "]" << endl;

        CTime::SetFormat("MDY__s");

        CTime t2(t1_str, *fmt);
        _ASSERT(t1 == t2);

        CTime::SetFormat(*fmt);
        string t2_str = t2;
        _ASSERT(t1_str.compare(t2_str) == 0);
    }
}


//=============================================================================
//
// TestGMT
//
//=============================================================================

static void s_TestGMT(void)
{
    cout << "---------------------------" << endl;
    cout << "Test GMT and Local time"     << endl;
    cout << "---------------------------" << endl << endl;

    {   
        cout << "Write time in timezone format" << endl;

        CTime::SetFormat("M/D/Y h:m:s Z");

        CTime t1(2001, 3, 12, 11, 22, 33, 999, CTime::eGmt);
        cout << "[" << t1.AsString() << "]" << endl;
        _ASSERT(t1.AsString() == "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        cout << "[" << t2.AsString() << "]" << endl;
        _ASSERT(t2.AsString() == "03/12/2001 11:22:33 ");

        CTime t3(CTime::eCurrent, CTime::eLocal);
        cout << "Local time [" << t3.AsString() << "]" << endl;
        CTime t4(CTime::eCurrent, CTime::eGmt);
        cout << "GMT time   [" << t4.AsString() << "]" << endl;
        cout << endl;
    }
    {   
        cout << "Process timezone string" << endl;

        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t="03/12/2001 11:22:33 GMT";
        cout << "[" << t.AsString() << "]" << endl;
        _ASSERT(t.AsString() == "03/12/2001 11:22:33 GMT");
        t="03/12/2001 11:22:33 ";
        cout << "[" << t.AsString() << "]" << endl;
        _ASSERT(t.AsString() == "03/12/2001 11:22:33 ");
        cout << endl;
    }
    {   
        cout << "Day of week" << endl;

        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i=0; t<=CTime(2001, 4, 10); t++,i++) {
            cout << t.AsString() << " is " << t.DayOfWeek() << endl;
            _ASSERT(t.DayOfWeek() == (i%7));
        }
        cout << endl;
    }
    //------------------------------------------------------------------------
    {   
        cout << "Test GetTimeT" << endl;

        time_t timer=time(0);
        CTime tg(CTime::eCurrent, CTime::eGmt, CTime::eDefault);
        CTime tl(CTime::eCurrent, CTime::eLocal, CTime::eDefault);
        CTime t(timer);
        cout << "[" << t.AsString() << "] " << endl;
        cout << tg.GetTimeT()/3600 << " - " << tl.GetTimeT()/3600 << " - ";
        cout << t.GetTimeT()/3600 << " - " << timer/3600 << endl;
        _ASSERT(timer == tg.GetTimeT());
        _ASSERT(timer == tl.GetTimeT());
        _ASSERT(timer == t.GetTimeT());
        cout << endl;

        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 4, 1, i>0 ? 2 : 1, i>0 ? (i-1) : 59, 
                     0, 0, CTime::eLocal, CTime::eHour);
            cout << tt.AsString() << " - " << tt.GetTimeT() / 3600 << endl; 
        }
        for (int i = 0; i < 2; i++) {
            CTime tt(2001, 10, 28, i > 0 ? 1 : 0, i > 0 ? (i-1) : 59,
                     0, 0, CTime::eLocal, CTime::eHour);
            cout << tt.AsString() << " - " << tt.GetTimeT() / 3600 << endl; 
        }
        cout << endl;
    }
    //------------------------------------------------------------------------
    {   
        cout << "Test TimeZoneDiff (1)" << endl;

        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);

        cout << "[" << tw.AsString() << "] diff from GMT = " << \
            tw.TimeZoneDiff() / 3600 << endl;
        _ASSERT(tw.TimeZoneDiff() / 3600 == -5);
        cout << "[" << ts.AsString() << "] diff from GMT = " << \
            ts.TimeZoneDiff() / 3600 << endl;
        _ASSERT(ts.TimeZoneDiff()/3600 == -4);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -4) {
                cout << "First daylight saving day = [" << \
                    tw.AsString() << "]" << endl;
                break;
            }
        }
        cout << endl;
    }
    //------------------------------------------------------------------------
    {   
        cout << "Test TimeZoneDiff (2)" << endl;

        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        cout << "[" << tw.AsString() << "] diff from GMT = " << \
            tw.TimeZoneDiff() / 3600 << endl;
        _ASSERT(tw.TimeZoneDiff() / 3600 == -4);
        cout << "[" << ts.AsString() << "] diff from GMT = " << \
            ts.TimeZoneDiff() / 3600 << endl;
        _ASSERT(ts.TimeZoneDiff() / 3600 == -5);

        for (; tw < ts; tw++) {
            if ((tw.TimeZoneDiff() / 3600) == -5) {
                cout << "First non daylight saving day = [" << \
                    tw.AsString() << "]" << endl;
                break;
             
            }
        }
        cout << endl;
    }
    //------------------------------------------------------------------------
    {   
        cout << "Test AdjustTime" << endl;

        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("04/01/2001 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eDefault);
        cout << "init  [" << t.AsString() << "]" << endl;

        t.SetTimeZoneFormat(CTime::eGmt);
        cout << "GMT" << endl;
        tn = t + 5;  
        cout << "+5d   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "04/06/2001 01:01:00");
        tn = t + 40; 
        cout << "+40d  [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZoneFormat(CTime::eLocal);
        cout << "Local eNone" << endl;
        t.SetTimeZonePrecision(CTime::eNone);
        tn=t+5;  cout << "+5d   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "04/06/2001 01:01:00");
        tn=t+40; cout << "+40d  [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "05/11/2001 01:01:00");

        t.SetTimeZonePrecision(CTime::eMonth);
        cout << "Local eMonth" << endl;
        tn = t + 5;
        cout << "+5d   [" << tn.AsString() << "]" << endl;
        tn = t; 
        tn.AddMonth(-1);
        cout << "-1m   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "03/01/2001 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        cout << "+1m   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "05/01/2001 02:01:00");

        t.SetTimeZonePrecision(CTime::eDay);
        cout << "Local eDay" << endl;
        tn = t - 1; 
        cout << "-1d   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "03/31/2001 01:01:00");
        tn++;   
        cout << "+0d   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "04/01/2001 01:01:00");
        tn = t + 1; 
        cout << "+1d   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "04/02/2001 02:01:00");

        cout << "Local eHour" << endl;
        t.SetTimeZonePrecision(CTime::eHour);
        tn = t; 
        tn.AddHour(-3);
        CTime te = t; 
        te.AddHour(3);
        cout << "-3h   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "03/31/2001 22:01:00");
        cout << "+3h   [" << te.AsString() << "]" << endl;
        _ASSERT(te.AsString() == "04/01/2001 05:01:00");
        CTime th = tn; 
        th.AddHour(49);
        cout << "+49h  [" << th.AsString() << "]" << endl;
        _ASSERT(th.AsString() == "04/03/2001 00:01:00");

        for (int i = 0;  i < 8;  i++,  tn.AddHour()) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn.AddHour(-1);
        for (int i = 0;  i < 8;  i++,  tn.AddHour(-1)) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn = "10/28/2001 00:01:00"; 
        cout << "init  [" << tn.AsString() << "]" << endl;
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-3); 
        te.AddHour(9);
        cout << "-3h   [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "10/27/2001 21:01:00");
        cout << "+9h   [" << te.AsString() << "]" << endl;
        _ASSERT(te.AsString() == "10/28/2001 08:01:00");
        th = tn; 
        th.AddHour(49);
        cout << "+49h  [" << th.AsString() << "]" << endl;
        _ASSERT(th.AsString() == "10/29/2001 21:01:00");

        tn.AddHour(+2);
        for (int i = 0;  i < 10;  i++,  tn.AddHour()) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;
        tn.AddHour(-1);
        for (int i = 0;  i < 10;  i++,  tn.AddHour(-1)) {
            cout << (((tn.TimeZoneDiff()/3600) == -4) ? " ":"*") \
                 << " [" <<  tn.AsString() << "]" << endl;
        }
        cout << endl;

        tn = "10/28/2001 09:01:00"; 
        cout << "init  [" << tn.AsString() << "]" << endl;
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-10); 
        te.AddHour(+10);
        cout << "-10h  [" << tn.AsString() << "]" << endl;
        _ASSERT(tn.AsString() == "10/28/2001 00:01:00");
        cout << "+10h  [" << te.AsString() << "]" << endl;
        _ASSERT(te.AsString() == "10/28/2001 19:01:00");
        
        cout << endl; 
        cout << endl;
    }
}


//=============================================================================
//
// TestGMTSpeedRun
//
//=============================================================================

static void s_TestGMTSpeedRun(string comment, CTime::ETimeZone tz, 
                              CTime::ETimeZonePrecision tzp)
{
    CTime t(CTime::eCurrent, tz, tzp);
    clock_t start, finish;
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

    cout << "Minute add, "<< comment << endl;

    start = clock();
    for (long i = 0; i < kCount; i++) {
        t.AddMinute();
    }
    cout << "End time    = " << t.AsString() << endl;
//    _ASSERT(th.AsString() == "04/03/2001 00:01:00"); 
    finish = clock();
    duration = (double) (finish - start) / CLOCKS_PER_SEC;
    cout << "Duration    = " << duration << " sec." << endl;
    cout << "Speed       = " << kCount/duration 
         << " operations per second" << endl;
    cout << endl;
}


//=============================================================================
//
// TestGMTSpeed
//
//=============================================================================

static void s_TestGMTSpeed(void)
{
    cout << "---------------------------" << endl;
    cout << "Test CTime Speed"            << endl;
    cout << "---------------------------" << endl << endl;

    s_TestGMTSpeedRun("eLocal - eMinute", CTime::eLocal, CTime::eMinute);
    s_TestGMTSpeedRun("eLocal - eHour"  , CTime::eLocal, CTime::eHour);
    s_TestGMTSpeedRun("eLocal - eMonth" , CTime::eLocal, CTime::eMonth);
    s_TestGMTSpeedRun("eLocal - eNone"  , CTime::eLocal, CTime::eNone);
    s_TestGMTSpeedRun("eGmt   - eNone"  , CTime::eGmt, CTime::eNone);
}


//=============================================================================
//
// MAIN
//
//=============================================================================

int main()
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Run tests
    try {
        s_TestMisc();
        s_TestFormats();
        s_TestGMT();
        s_TestGMTSpeed();
    } catch (exception& e) {
        ERR_POST(Fatal << e.what());
    }
    // Success
    return 0;
}
