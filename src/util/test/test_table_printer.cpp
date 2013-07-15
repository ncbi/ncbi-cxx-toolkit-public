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
 * Author: Michael Kornbluh
 *
 * File Description: 
 *   Unit test for CTablePrinter.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <util/table_printer.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// Test Cases

BOOST_AUTO_TEST_CASE(TestBasic)
{
    CNcbiOstrstream result_strm;

    {{
        CTablePrinter::SColInfoVec vecColInfo;
        vecColInfo.AddCol("Name", 20);
        vecColInfo.AddCol("Age", 4, CTablePrinter::eJustify_Right);
        vecColInfo.AddCol("City", 8, CTablePrinter::eJustify_Right, 
            CTablePrinter::eDataTooLong_ShowErrorInColumn);
        vecColInfo.AddCol("State", 2, CTablePrinter::eJustify_Right, 
            CTablePrinter::eDataTooLong_ShowWholeData);
        vecColInfo.AddCol("ZIP", 5, CTablePrinter::eJustify_Right, 
            CTablePrinter::eDataTooLong_TruncateWithEllipses);

        CTablePrinter table_printer(vecColInfo, result_strm, "  |  ");

        // add a row that's fine
        *table_printer.AddCell() << "Jean" << ' ' << "Doe";
        *table_printer.AddCell() << 88;
        *table_printer.AddCell() << "Crozet";
        *table_printer.AddCell() << "VA";
        *table_printer.AddCell() << "22601";

        // add a row where every cell is overflowing
        *table_printer.AddCell() << "Hubert Blaine Wolfeschlegelsteinhausenbergerdorff..., record holder for longest name";
        *table_printer.AddCell() << 12345;
        *table_printer.AddCell() << "Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch";
        *table_printer.AddCell() << "Wales, part of the United Kingdom, and part of the island of Great Britain";
        *table_printer.AddCell() << "zip code too long test";
    }}

    CNcbiOstrstream expected_result_strm;
    expected_result_strm << "--------------------  |  ----  |  --------  |  -----  |  -----" << endl;
    expected_result_strm << "Name                  |  Age   |  City      |  State  |  ZIP  " << endl;
    expected_result_strm << "--------------------  |  ----  |  --------  |  -----  |  -----" << endl;
    expected_result_strm << "Jean Doe              |    88  |    Crozet  |     VA  |  22601" << endl;
    expected_result_strm << "**ERROR**             |  ????  |  ????????  |  Wales, part of the United Kingdom, and part of the island of Great Britain  |  zip code too long t..." << endl;
    expected_result_strm << "--------------------  |  ----  |  --------  |  -----  |  -----" << endl;

    const string sResult = CNcbiOstrstreamToString(result_strm);
    const string sExpected = CNcbiOstrstreamToString(expected_result_strm);

    BOOST_CHECK_EQUAL(sExpected, sResult);
}

BOOST_AUTO_TEST_CASE(TestOverflowException)
{
    CNcbiOstrstream dummy_strm;

    CTablePrinter::SColInfoVec vecColInfo;
    vecColInfo.AddCol("Name", 20, CTablePrinter::eJustify_Left,
        CTablePrinter::eDataTooLong_ThrowException );
    CTablePrinter table_printer(vecColInfo, dummy_strm);

    BOOST_CHECK_THROW(
        *table_printer.AddCell() << "THIS NAME IS TOO LONG.",
        CException);
}

BOOST_AUTO_TEST_CASE(TestFinishTable)
{
    CNcbiOstrstream result_strm;

    CTablePrinter::SColInfoVec vecColInfo;
    vecColInfo.AddCol("Name", 20);
    vecColInfo.AddCol("ZIP code", 5);

    CTablePrinter table_printer(vecColInfo, result_strm);

    *table_printer.AddCell() << "Pat Doe";
    *table_printer.AddCell() << "22801";

    *table_printer.AddCell() << "Sammy Smith";
    *table_printer.AddCell() << "20852";

    table_printer.FinishTable();

    *table_printer.AddCell() << "Chris Doe";
    *table_printer.AddCell() << "08361";

    table_printer.FinishTable();

    CNcbiOstrstream expected_result_strm;
    expected_result_strm << "--------------------   --------" << endl;
    expected_result_strm << "Name                   ZIP code" << endl;
    expected_result_strm << "--------------------   --------" << endl;
    expected_result_strm << "Pat Doe                22801   " << endl;
    expected_result_strm << "Sammy Smith            20852   " << endl;
    expected_result_strm << "--------------------   --------" << endl;
    expected_result_strm << "--------------------   --------" << endl;
    expected_result_strm << "Name                   ZIP code" << endl;
    expected_result_strm << "--------------------   --------" << endl;
    expected_result_strm << "Chris Doe              08361   " << endl;
    expected_result_strm << "--------------------   --------" << endl;

    const string sExpected = CNcbiOstrstreamToString(expected_result_strm);
    const string sResult = CNcbiOstrstreamToString(result_strm);
    BOOST_CHECK_EQUAL(sExpected, sResult);
}

BOOST_AUTO_TEST_CASE(TestEmptyColName)
{
    CNcbiOstrstream result_strm;

    CTablePrinter::SColInfoVec vecColInfo;
    vecColInfo.AddCol("", 2);
    vecColInfo.AddCol("Foo", 5);

    CTablePrinter table_printer(vecColInfo, result_strm);

    *table_printer.AddCell() << "";
    *table_printer.AddCell() << "abc";

    *table_printer.AddCell() << "X";
    *table_printer.AddCell() << "defgh";

    table_printer.FinishTable();

    CNcbiOstrstream expected_result_strm;
    expected_result_strm << "--   -----" << endl;
    expected_result_strm << "     Foo  " << endl;
    expected_result_strm << "--   -----" << endl;
    expected_result_strm << "     abc  " << endl;
    expected_result_strm << "X    defgh" << endl;
    expected_result_strm << "--   -----" << endl;

    const string sExpected = CNcbiOstrstreamToString(expected_result_strm);
    const string sResult = CNcbiOstrstreamToString(result_strm);
    BOOST_CHECK_EQUAL(sExpected, sResult);
}

END_NCBI_SCOPE

