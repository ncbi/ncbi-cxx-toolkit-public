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

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


DEFINE_STATIC_FAST_MUTEX(s_OutMutex);

static void OUTS(int idx, long line, string value, const char* check) 
{
    CFastMutexGuard LOCK(s_OutMutex);
    cout << idx << " (" << line << ") - " << value << '\n';
    if ( check  &&  value != string(check) ) {
        cout << "LINE: " << line << " - "<< value << " != " << string(check) << endl;
        abort();
    }
}

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
        t++;
        OUTS( idx, __LINE__, t.AsString(), "12/31/1999");
        t++;
        OUTS( idx, __LINE__, t.AsString(), "01/01/2000");
        t++;
        OUTS( idx, __LINE__, t.AsString(), "01/02/2000");
        t="02/27/2000";
        t++;
        OUTS( idx, __LINE__, t.AsString(), "02/28/2000");
        t++;
        OUTS( idx, __LINE__, t.AsString(), "02/29/2000");
        t++;
        OUTS( idx, __LINE__, t.AsString(), "03/01/2000");
        t++;
        OUTS( idx, __LINE__, t.AsString(), "03/02/2000");
    }}

    // String assignment
    {{
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("02/15/2000 01:12:33");
        OUTS( idx, __LINE__,  t.AsString(), "02/15/2000 01:12:33");
        t = "3/16/2001 02:13:34";
        OUTS( idx, __LINE__,  t.AsString(), "03/16/2001 02:13:34");
    }}

    CTime::SetFormat("M/D/Y h:m:s.S");

    // Adding Nanoseconds
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 59, 59, 999999995);
             ti <= CTime(2000, 1, 1, 0, 0, 0, 000000003);
             t = ti, ti.AddNanoSecond(2)) {
        }
        OUTS( idx, __LINE__, t.AsString(), "01/01/2000 00:00:00.000000003");
    }}

    CTime::SetFormat("M/D/Y h:m:s");

    // Adding seconds
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 59, 5);
             ti <= CTime(2000, 1, 1, 0, 1, 20);
             t = ti, ti.AddSecond(11)) {
        }
        OUTS( idx, __LINE__, t.AsString(), "01/01/2000 00:01:17");
    }}

    // Adding minutes
    {{
        CTime t;
        for (CTime ti(1999, 12, 31, 23, 45);
             ti <= CTime(2000, 1, 1, 0, 15);
             t = ti, ti.AddMinute(11)) {
        }
        OUTS( idx, __LINE__, t.AsString(), "01/01/2000 00:07:00");
    }}

    // Adding hours
    {{
        CTime t;
        for (CTime ti(1999, 12, 31); ti <= CTime(2000, 1, 1, 15);
             t = ti, ti.AddHour(11)) {
        }
        OUTS( idx, __LINE__, t.AsString(), "01/01/2000 09:00:00");
    }}

    // Adding months
    {{
        CTime t;
        for (CTime ti(1998, 12, 29); ti <= CTime(1999, 4, 1);
             t = ti, ti.AddMonth()) {
        }
        OUTS( idx, __LINE__, t.AsString(), "03/28/1999 00:00:00");
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
        OUTS( idx, __LINE__, t.AsString(), "07/23/2001 11:23:24");
    }}
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
        "D B Y h:m:s",
        "D b Y h:m:s",
        "smhyMD",
        "y||||M++++D   h===ms",
        "   yM[][D   h:,.,.,ms  ",
        "\tkkkMy++D   h:ms\n",
        0
    };

    for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
        CTime t1(2001, 1, 2, 3, 4, 0);

        CTime::SetFormat(*fmt);
        string t1_str = t1.AsString();
        
        CTime::SetFormat("MDY__s");

        CTime t2(t1_str, *fmt);
        assert(t1 == t2);

        CTime::SetFormat(*fmt);
        string t2_str = t2;
        assert(t1_str.compare(t2_str) == 0);
    }
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
        OUTS( idx, __LINE__,  t1.AsString(), "03/12/2001 11:22:33 GMT");
        CTime t2(2001, 3, 12, 11, 22, 33, 999, CTime::eLocal);
        OUTS( idx, __LINE__,  t2.AsString(), "03/12/2001 11:22:33 ");
    }}

    // Process timezone string
    {{   
        CTime t;
        t.SetFormat("M/D/Y h:m:s Z");
        t="03/12/2001 11:22:33 GMT";
        OUTS( idx, __LINE__,  t.AsString(), "03/12/2001 11:22:33 GMT");
        t="03/12/2001 11:22:33 ";
        OUTS( idx, __LINE__,  t.AsString(), "03/12/2001 11:22:33 ");
    }}

    // Day of week
    {{   
        CTime t(2001, 4, 1);
        t.SetFormat("M/D/Y h:m:s w");
        int i;
        for (i=0; t<=CTime(2001, 4, 10); t++,i++) {
            assert(t.DayOfWeek() == (i%7));
        }
    }}

    // Test GetTimeT
    {{  
        time_t timer=time(0);
        CTime tg(CTime::eCurrent, CTime::eGmt, CTime::eTZPrecisionDefault);
        CTime tl(CTime::eCurrent, CTime::eLocal, CTime::eTZPrecisionDefault);
        CTime t(timer);
        tg.SetTimeT(timer);
        tl.SetTimeT(timer);

        assert(timer == tg.GetTimeT());
        assert(timer == tl.GetTimeT());
        assert(timer == t.GetTimeT());
    }}

    // Test TimeZoneDiff (1)
    {{   
        CTime tw(2001, 1, 1, 12); 
        CTime ts(2001, 6, 1, 12);
        assert(tw.TimeZoneDiff() / 3600 == -5);
        assert(ts.TimeZoneDiff()/3600 == -4);
    }}

    // Test TimeZoneDiff (2)
    {{   
        CTime tw(2001, 6, 1, 12); 
        CTime ts(2002, 1, 1, 12);
        assert(tw.TimeZoneDiff() / 3600 == -4);
        assert(ts.TimeZoneDiff() / 3600 == -5);
    }}

    // Test AdjustTime
    {{   
        CTime::SetFormat("M/D/Y h:m:s");
        CTime t("04/01/2001 01:01:00");
        CTime tn;
        t.SetTimeZonePrecision(CTime::eTZPrecisionDefault);

        // GMT
        t.SetTimeZoneFormat(CTime::eGmt);
        tn = t + 5;  
        OUTS( idx, __LINE__,  tn.AsString(), "04/06/2001 01:01:00");
        tn = t + 40; 
        OUTS( idx, __LINE__,  tn.AsString(), "05/11/2001 01:01:00");

        // Local eNone
        t.SetTimeZoneFormat(CTime::eLocal);
        t.SetTimeZonePrecision(CTime::eNone);
        tn=t+5;
        OUTS( idx, __LINE__,  tn.AsString(), "04/06/2001 01:01:00");
        tn=t+40;
        OUTS( idx, __LINE__,  tn.AsString(), "05/11/2001 01:01:00");

        //Local eMonth
        t.SetTimeZonePrecision(CTime::eMonth);
        tn = t + 5;
        tn = t; 
        tn.AddMonth(-1);
        OUTS( idx, __LINE__,  tn.AsString(), "03/01/2001 01:01:00");
        tn = t; 
        tn.AddMonth(+1);
        OUTS( idx, __LINE__,  tn.AsString(), "05/01/2001 02:01:00");

        // Local eDay
        t.SetTimeZonePrecision(CTime::eDay);
        tn = t - 1; 
        OUTS( idx, __LINE__,  tn.AsString(), "03/31/2001 01:01:00");
        tn++;   
        OUTS( idx, __LINE__,  tn.AsString(), "04/01/2001 01:01:00");
        tn = t + 1; 
        OUTS( idx, __LINE__,  tn.AsString(), "04/02/2001 02:01:00");

        // Local eHour
        t.SetTimeZonePrecision(CTime::eHour);
        tn = t; 
        tn.AddHour(-3);
        CTime te = t; 
        te.AddHour(3);
        OUTS( idx, __LINE__,  tn.AsString(), "03/31/2001 22:01:00");
        OUTS( idx, __LINE__,  te.AsString(), "04/01/2001 05:01:00");
        CTime th = tn; 
        th.AddHour(49);
        OUTS( idx, __LINE__,  th.AsString(), "04/03/2001 00:01:00");

        tn = "10/28/2001 00:01:00"; 
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-3); 
        te.AddHour(9);
        OUTS( idx, __LINE__,  tn.AsString(), "10/27/2001 21:01:00");
        OUTS( idx, __LINE__,  te.AsString(), "10/28/2001 08:01:00");
        th = tn; 
        th.AddHour(49);
        OUTS( idx, __LINE__,  th.AsString(), "10/29/2001 21:01:00");

        tn = "10/28/2001 09:01:00"; 
        tn.SetTimeZonePrecision(CTime::eHour);
        te = tn; 
        tn.AddHour(-10); 
        te.AddHour(+10);
        OUTS( idx, __LINE__,  tn.AsString(), "10/28/2001 00:01:00");
        OUTS( idx, __LINE__,  te.AsString(), "10/28/2001 19:01:00");
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
        for (int i=0; i<10; i++) {
            s_TestMisc(idx);
            s_TestFormats();
            s_TestGMT(idx);
        }
        OUTS( idx, __LINE__, "\n", 0);
        OUTS( idx, __LINE__,  "============ End thread =============" , 0);
        OUTS( idx, __LINE__, "\n", 0);
    } catch (CException& e) {
        ERR_POST(Fatal << e);
        return false;
    }

    return true;
}

bool CTestRegApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing NCBITIME "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    return true;
}

bool CTestRegApp::TestApp_Exit(void)
{
    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestRegApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.9  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.8  2003/11/25 20:03:56  ivanov
 * Fixed misspelled eTZPrecisionDefault
 *
 * Revision 6.7  2003/11/25 19:56:39  ivanov
 * Renamed eDefault to eTZPrecisionDefault.
 * Some cosmetic changes.
 *
 * Revision 6.6  2002/10/18 13:25:15  ivanov
 * Fixed typo in the time formats list
 *
 * Revision 6.5  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 6.4  2002/07/15 18:17:26  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.3  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.2  2002/05/14 20:08:20  ucko
 * Make last argument to OUTS const; we don't modify it, and without
 * const we may get compiler warnings when passing string literals.
 *
 * Revision 6.1  2002/05/13 13:58:45  ivanov
 * Initial revision
 *
 *
 * ===========================================================================
 */
