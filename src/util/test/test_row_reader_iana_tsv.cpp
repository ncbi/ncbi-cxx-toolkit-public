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
 * Author: Sergey Satskiy
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <util/row_reader_iana_tsv.hpp>
#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BOOST_AUTO_TEST_SUITE(CRowReader_IANA_TSV_Unit_Test)

typedef CRowReader<CRowReaderStream_IANA_TSV> TIANATSVStream;



BOOST_AUTO_TEST_CASE(IANA_TSV_EMPTY_STREAM)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empy. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }
}


BOOST_AUTO_TEST_CASE(IANA_TSV_HEADER_ONLY)
{
    string                  data = "Name\tAge\tAddress";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empy. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }

    BOOST_CHECK(src_stream.GetFieldName(0) == string("Name"));
    BOOST_CHECK(src_stream.GetFieldName(1) == string("Age"));
    BOOST_CHECK(src_stream.GetFieldName(2) == string("Address"));
}


BOOST_AUTO_TEST_CASE(IANA_TSV_DATA)
{
    string                  data = "Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\tBig Farm Way\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Paul\t23\t1115 W Franklin"));
                BOOST_CHECK(row[0].Get<string>() == string("Paul"));
                BOOST_CHECK(row[1].Get<string>() == string("23"));
                BOOST_CHECK(row[1].Get<int>() == 23);
                BOOST_CHECK(row[2].Get<string>() == string("1115 W Franklin"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Bessy the Cow\t5\tBig Farm Way"));
                BOOST_CHECK(row["Name"].Get<string>() == string("Bessy the Cow"));
                BOOST_CHECK(row["Age"].Get<string>() == string("5"));
                BOOST_CHECK(row["Age"].Get<int>() == 5);
                BOOST_CHECK(row["Address"].Get<string>() == string("Big Farm Way"));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Zeke\t45\tW Main St"));
                BOOST_CHECK(row[0].Get<string>() == string("Zeke"));
                BOOST_CHECK(row[1].Get<string>() == string("45"));
                BOOST_CHECK(row[1].Get<int>() == 45);
                BOOST_CHECK(row[2].Get<string>() == string("W Main St"));
                break;
        }
        ++line_no;
    }

    BOOST_CHECK(src_stream.GetFieldName(0) == string("Name"));
    BOOST_CHECK(src_stream.GetFieldName(1) == string("Age"));
    BOOST_CHECK(src_stream.GetFieldName(2) == string("Address"));
    BOOST_CHECK(line_no == 4);
}


BOOST_AUTO_TEST_CASE(IANA_TSV_VALIDATE_OK)
{
    string                  data = "Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\tBig Farm Way\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(IANA_TSV_VALIDATE_FAILURE)
{
    string                  data = "Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    // An exception is expected
    try {
        src_stream.Validate();
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected validation context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_TSV_ITERATE_FAILURE)
{
    string                  data = "Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data.c_str());
    TIANATSVStream          src_stream(&data_stream, "");

    // An exception is expected
    try {
        for (auto &  row : src_stream) {
            // This is to suppress a compiler warning
            BOOST_CHECK(row.GetNumberOfFields() < 1000000);
        }

        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected validation context");
    }
}


BOOST_AUTO_TEST_SUITE_END()
END_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// main entry point for tests

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("UTIL IANA TSV Row Stream");
}
