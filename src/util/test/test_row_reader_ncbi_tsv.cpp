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

#include <util/row_reader_ncbi_tsv.hpp>
#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BOOST_AUTO_TEST_SUITE(CRowReader_NCBI_TSV_Unit_Test)

typedef CRowReader<CRowReaderStream_NCBI_TSV> TNCBITSVStream;


BOOST_AUTO_TEST_CASE(NCBI_TSV_EMPTY_STREAM)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empy. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }
}


BOOST_AUTO_TEST_CASE(NCBI_TSV_HEADER_ONLY)
{
    string                  data = "\n\n#Name\tAge\tAddress\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empy. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].field_no == 0);
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("Name"));

        BOOST_CHECK(field_info[1].field_no == 1);
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("Age"));

        BOOST_CHECK(field_info[2].field_no == 2);
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("Address"));
    }
}


BOOST_AUTO_TEST_CASE(NCBI_TSV_DATA)
{
    string                  data = "#Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\tBig Farm Way\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() ==
                            string("Paul\t23\t1115 W Franklin"));
                BOOST_CHECK(row[0].Get<string>() == string("Paul"));
                BOOST_CHECK(row[1].Get<string>() == string("23"));
                BOOST_CHECK(row[1].Get<int>() == 23);
                BOOST_CHECK(row[2].Get<string>() == string("1115 W Franklin"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() ==
                            string("Bessy the Cow\t5\tBig Farm Way"));
                BOOST_CHECK(row["Name"].Get<string>() ==
                            string("Bessy the Cow"));
                BOOST_CHECK(row["Age"].Get<string>() == string("5"));
                BOOST_CHECK(row["Age"].Get<int>() == 5);
                BOOST_CHECK(row["Address"].Get<string>() ==
                            string("Big Farm Way"));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() ==
                            string("Zeke\t45\tW Main St"));
                BOOST_CHECK(row[0].Get<string>() == string("Zeke"));
                BOOST_CHECK(row[1].Get<string>() == string("45"));
                BOOST_CHECK(row[1].Get<int>() == 45);
                BOOST_CHECK(row[2].Get<string>() == string("W Main St"));
                break;
        }
        ++line_no;
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].field_no == 0);
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("Name"));

        BOOST_CHECK(field_info[1].field_no == 1);
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("Age"));

        BOOST_CHECK(field_info[2].field_no == 2);
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("Address"));
    }

    BOOST_CHECK(line_no == 4);
}


BOOST_AUTO_TEST_CASE(NCBI_TSV_VALIDATE_OK)
{
    string                  data = "#Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "Bessy the Cow\t5\tBig Farm Way\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(NCBI_TSV_VALIDATE_OK_VARIABLE_FIELD_CNT)
{
    string                  data = "#Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "\n"
                                   "Bessy the Cow\t5\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(NCBI_TSV_ITERATE_FAILURE)
{
    string                  data = "#Name\tAge\tAddress\n"
                                   "Paul\t23\t1115 W Franklin\n"
                                   "\n"
                                   "Bessy the Cow\t5\n"
                                   "Zeke\t45\tW Main St";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    // No exception is expected
    for (auto &  row : src_stream) {
        // This is to suppress a compiler warning
        BOOST_CHECK(row.GetNumberOfFields() < 1000000);
    }
}


BOOST_AUTO_TEST_CASE(VALIDATION_EMPTY_FIELD_NAME)
{
    string                  data = "#Name\t\tAddress\n"
                                   "Zeke\t45\tW Main St\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(VALIDATION_EMPTY_FIELD_VALUE)
{
    string                  data = "#Name\tAge\tAddress\n"
                                   "Zeke\t\tW Main St\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION)
{
    string                  data = "#Integer\tDouble\tBool\n"
                                   "1\t2.2\tTrue\n"
                                   "3\t3.14\tFalse\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_FAIL)
{
    string                  data = "#Integer\tDouble\tBool\n"
                                   "1\t2.2\tTrue\n"
                                   "\n"
                                   "3\tExpectedDouble\tFalse\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_NO_FAIL)
{
    string                  data = "#Integer\tDouble\tBool\n"
                                   "1\t2.2\tTrue\n"
                                   "3\tExpectedDouble\tFalse\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    src_stream.Validate(CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
                        eRR_NoFieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT)
{
    string                  data = "#DateTime\n"
                                   "01/02/1903 12:13:14\n"
                                   "\n\n\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    src_stream.Validate(CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT_FAIL)
{
    string                  data = "#DateTime\n"
                                   "12:13:14 04/04/1999\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    try {
        src_stream.Validate(
            CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_EXPLICIT_FORMAT)
{
    string                  data = "#DateTime\n"
                                   "1903 01 02 12:13:14\n"
                                   "##\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    src_stream.Validate(CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_EXPLICIT_FORMAT_FAIL)
{
    string                  data = "#DateTime\n"
                                   "01 02 1903 12:13:14\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    try {
        src_stream.Validate(
            CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_CASE(NULL_FIELD_VALIDATION)
{
    string                  data = "#Integer\tDouble\tBool\n"
                                   "na\t2.2\tTrue\n"
                                   "3\t3.14\tna\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(EMPTY_FIELD_VALIDATION_FAIL)
{
    string                  data = "#Integer\tDouble\tBool\n"
                                   "1\t2.2\tTrue\n"
                                   "\n"
                                   "3\t-\tFalse\n";
    CNcbiIstrstream         data_stream(data);
    TNCBITSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_NCBI_TSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_SUITE_END()
END_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// main entry point for tests

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("UTIL NCBI TSV Row Stream");
}
