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


static void s_TestMisc(void)
{
    CTime t1(CTime::eCurrent);
    cout << "[" << t1.AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (t1++).AsString() << "]" << endl;
    cout << "[" << (++t1).AsString() << "]" << endl;
    cout << "[" << (++t1).AsString() << "]" << endl;

    CTime t2;
    cout << "[" << t2.AsString() << "]" << endl;

    CTime t3(2000, 365 / 2);
    cout << "[" << t3.AsString() << "]" << endl;


    CTime::SetFormat("M/D/Y");

    cout << "Year 2000 problem:" << endl;
    for (CTime t4(1999, 12, 30);  t4 <= CTime(2000, 1, 2);  ++t4) {
        cout << "[" << t4.AsString() << "] ";
    }
    cout << endl;

    for (CTime t44(2000, 2, 27);  t44 <= CTime(2000, 3, 2);  t44.AddDay()) {
        cout << "[" << t44.AsString() << "] ";
    }
    cout << endl;

    CTime::SetFormat("M/D/Y h:m:s");

    try {
        CTime t5("02/15/2000 01:12:33");
        cout << "[" << t5.AsString() << "]" << endl;

        t5 = "3/16/2001 2:13:34";
        cout << "[" << t5.AsString() << "]" << endl;
    } catch (exception& e) {
        cout << e.what() << endl;
    }

    cout << "Adding seconds:" << endl;
    for (CTime t6(1999, 12, 31, 23, 59, 5);
         t6 <= CTime(2000, 1, 1, 0, 1, 20);
         t6.AddSecond(11)) {
        cout << "[" << t6.AsString() << "] ";
    }
    cout << endl;

    cout << "Adding minutes:" << endl;
    for (CTime t61(1999, 12, 31, 23, 45);
         t61 <= CTime(2000, 1, 1, 0, 15);
         t61.AddMinute(11)) {
        cout << "[" << t61.AsString() << "] ";
    }
    cout << endl;

    cout << "Adding hours:" << endl;
    for (CTime t62(1999, 12, 31);
         t62 <= CTime(2000, 1, 1, 15);
         t62.AddHour(11)) {
        cout << "[" << t62.AsString() << "] ";
    }
    cout << endl;

    cout << "Adding months:" << endl;
    for (CTime t63(1998, 12, 29);
         t63 <= CTime(1999, 4, 1);
         t63.AddMonth()) {
        cout << "[" << t63.AsString() << "] ";
    }
    cout << endl;

    CTime t7(2000, 10, 1, 12, 3, 45);
    CTime t8(2000, 10, 2, 14, 55, 1);

    cout << "[" << t7.AsString() << " - " << t8.AsString() << "]" << endl;
    printf("DiffDay    = %.2f\n", t8.DiffDay   (t7));
    printf("DiffHour   = %.2f\n", t8.DiffHour  (t7));
    printf("DiffMinute = %.2f\n", t8.DiffMinute(t7));
    printf("DiffSecond = %d\n",   t8.DiffSecond(t7));
}


static void s_TestFormats(void)
{
    static const char* s_Fmt[] = {
        "M/D/Y h:m:s",
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

    for (const char** fmt = s_Fmt;  *fmt;  fmt++) {
        CTime t1(CTime::eCurrent);

        CTime::SetFormat(*fmt);
        string t1_str = t1.AsString();
        CTime::SetFormat("MDY__s");


        CTime t2(t1_str, *fmt);
        _ASSERT(t1 == t2);

        CTime::SetFormat(*fmt);
        string t2_str = t2;
        _ASSERT(t1_str.compare(t2_str) == 0);
    }
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main()
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Run tests
    try {
        s_TestMisc();
    } catch (exception& e) {
        ERR_POST(Fatal << "s_TestMisc(): " << e.what());
    }

    try {
        s_TestFormats();
    }  catch (exception& e) {
        ERR_POST(Fatal << "s_TestFormats(): " << e.what());
    }

    // Success
    return 0;
}
