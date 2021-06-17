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

#include <util/row_reader_iana_csv.hpp>
#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BOOST_AUTO_TEST_SUITE(CRowReader_IANA_CSV_Unit_Test)

typedef CRowReader<CRowReaderStream_IANA_CSV> TIANACSVStream;



BOOST_AUTO_TEST_CASE(IANA_CSV_EMPTY_STREAM)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }
}

BOOST_AUTO_TEST_CASE(IANA_TSV_HEADER_ONLY)
{
    string                  data = "Name,Age,Address";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
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


BOOST_AUTO_TEST_CASE(IANA_CSV_DATA)
{
    string                  data = "Name,Age,Address\r\n"
                                   "Paul,23,1115 W Franklin\n"
                                   "Bessy the Cow,5,Big Farm Way\n"
                                   "Zeke,45,W Main St";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Paul,23,1115 W Franklin"));
                BOOST_CHECK(row[0].Get<string>() == string("Paul"));
                BOOST_CHECK(row[1].Get<string>() == string("23"));
                BOOST_CHECK(row[1].Get<int>() == 23);
                BOOST_CHECK(row[2].Get<string>() == string("1115 W Franklin"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Bessy the Cow,5,Big Farm Way"));
                BOOST_CHECK(row["Name"].Get<string>() == string("Bessy the Cow"));
                BOOST_CHECK(row["Age"].Get<string>() == string("5"));
                BOOST_CHECK(row["Age"].Get<int>() == 5);
                BOOST_CHECK(row["Address"].Get<string>() == string("Big Farm Way"));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Zeke,45,W Main St"));
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


BOOST_AUTO_TEST_CASE(IANA_CSV_EMPTY_FIELDS)
{
    string                  data = ",,,";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.GetTraits().SetHasHeader(false);
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetOriginalData() == string(",,,"));
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row[0].Get<string>() == string(""));
        BOOST_CHECK(row[1].Get<string>() == string(""));
        BOOST_CHECK(row[2].Get<string>() == string(""));
        BOOST_CHECK(row[3].Get<string>() == string(""));
        BOOST_CHECK(row[0].GetOriginalData() == string(""));
        BOOST_CHECK(row[1].GetOriginalData() == string(""));
        BOOST_CHECK(row[2].GetOriginalData() == string(""));
        BOOST_CHECK(row[3].GetOriginalData() == string(""));
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_QUOTED_EMPTY_FIELDS)
{
    string                  data = "\"\",\"\",\"\",\"\"";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.GetTraits().SetHasHeader(false);
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetOriginalData() == string("\"\",\"\",\"\",\"\""));
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row[0].Get<string>() == string(""));
        BOOST_CHECK(row[1].Get<string>() == string(""));
        BOOST_CHECK(row[2].Get<string>() == string(""));
        BOOST_CHECK(row[3].Get<string>() == string(""));
        BOOST_CHECK(row[0].GetOriginalData() == string("\"\""));
        BOOST_CHECK(row[1].GetOriginalData() == string("\"\""));
        BOOST_CHECK(row[2].GetOriginalData() == string("\"\""));
        BOOST_CHECK(row[3].GetOriginalData() == string("\"\""));
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_QUOTED_FIELDS)
{
    string                  data = "Name,Age,Address\r\n"
                                   "\"Paul\",23,1115 W Franklin\n"
                                   "Bessy the Cow,\"5\",Big Farm Way\n"
                                   "Zeke,45,\"W Main St\"";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"Paul\",23,1115 W Franklin"));
                BOOST_CHECK(row[0].Get<string>() == string("Paul"));
                BOOST_CHECK(row[1].Get<string>() == string("23"));
                BOOST_CHECK(row[1].Get<int>() == 23);
                BOOST_CHECK(row[2].Get<string>() == string("1115 W Franklin"));
                BOOST_CHECK(row[0].GetOriginalData() == string("\"Paul\""));
                BOOST_CHECK(row[1].GetOriginalData() == string("23"));
                BOOST_CHECK(row[2].GetOriginalData() == string("1115 W Franklin"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Bessy the Cow,\"5\",Big Farm Way"));
                BOOST_CHECK(row["Name"].Get<string>() == string("Bessy the Cow"));
                BOOST_CHECK(row["Age"].Get<string>() == string("5"));
                BOOST_CHECK(row["Age"].Get<int>() == 5);
                BOOST_CHECK(row["Address"].Get<string>() == string("Big Farm Way"));
                BOOST_CHECK(row["Name"].GetOriginalData() == string("Bessy the Cow"));
                BOOST_CHECK(row["Age"].GetOriginalData() == string("\"5\""));
                BOOST_CHECK(row["Address"].GetOriginalData() == string("Big Farm Way"));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("Zeke,45,\"W Main St\""));
                BOOST_CHECK(row[0].Get<string>() == string("Zeke"));
                BOOST_CHECK(row[1].Get<string>() == string("45"));
                BOOST_CHECK(row[1].Get<int>() == 45);
                BOOST_CHECK(row[2].Get<string>() == string("W Main St"));
                BOOST_CHECK(row[0].GetOriginalData() == string("Zeke"));
                BOOST_CHECK(row[1].GetOriginalData() == string("45"));
                BOOST_CHECK(row[2].GetOriginalData() == string("\"W Main St\""));
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


BOOST_AUTO_TEST_CASE(IANA_TSV_QUOTED_HEADER)
{
    string                  data = "\"Name\",Age,Address";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
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

    data = "Name,\"Age\",Address";
    CNcbiIstrstream         data_stream1(data);
    TIANACSVStream          src_stream1(&data_stream1, "");

    for (auto &  row : src_stream1) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }

    field_info = src_stream1.GetFieldsMetaInfo();
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

    data = "Name,Age,\"Address\"";
    CNcbiIstrstream         data_stream2(data);
    TIANACSVStream          src_stream2(&data_stream2, "");

    for (auto &  row : src_stream2) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }

    field_info = src_stream2.GetFieldsMetaInfo();
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


BOOST_AUTO_TEST_CASE(IANA_CSV_QUOTED_FIELDS_WITH_QUOTES)
{
    string                  data = "\"Na\"\"me\",\"A\"\"ge\",\"Addr\"\"ess\"\r\n"
                                   "\"Pa\"\"ul\",\"2\"\"3\",\"1115 W \"\"Franklin\"";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"Pa\"\"ul\",\"2\"\"3\",\"1115 W \"\"Franklin\""));
                BOOST_CHECK(row[0].Get<string>() == string("Pa\"ul"));
                BOOST_CHECK(row[1].Get<string>() == string("2\"3"));
                BOOST_CHECK(row[2].Get<string>() == string("1115 W \"Franklin"));
                BOOST_CHECK(row[0].GetOriginalData() == string("\"Pa\"\"ul\""));
                BOOST_CHECK(row[1].GetOriginalData() == string("\"2\"\"3\""));
                BOOST_CHECK(row[2].GetOriginalData() == string("\"1115 W \"\"Franklin\""));
                break;
        }
        ++line_no;
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].field_no == 0);
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("Na\"me"));

        BOOST_CHECK(field_info[1].field_no == 1);
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("A\"ge"));

        BOOST_CHECK(field_info[2].field_no == 2);
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("Addr\"ess"));
    }

    BOOST_CHECK(line_no == 2);
}



BOOST_AUTO_TEST_CASE(IANA_CSV_QUOTED_QUOTED_MULTILINE)
{
    string                  data = "\"abc\r\ncde\",\"fgh\nklm\"\n"
                                   "\"data1\r\ndata2\",\"data3\ndata4\"";

    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"data1\r\ndata2\",\"data3\ndata4\""));
                BOOST_CHECK(row[0].Get<string>() == string("data1\r\ndata2"));
                BOOST_CHECK(row[1].Get<string>() == string("data3\ndata4"));
                BOOST_CHECK(row[0].GetOriginalData() == string("\"data1\r\ndata2\""));
                BOOST_CHECK(row[1].GetOriginalData() == string("\"data3\ndata4\""));
                break;
        }
        ++line_no;
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 2);
    if (field_info.size() == 2) {
        BOOST_CHECK(field_info[0].field_no == 0);
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("abc\r\ncde"));

        BOOST_CHECK(field_info[1].field_no == 1);
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("fgh\nklm"));
    }

    BOOST_CHECK(line_no == 2);
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NONFIRST_QUOTE)
{
    string                  data = "abc\"\"def\n"
                                   "ghk";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        for (auto &  row : src_stream) {
            BOOST_FAIL("Exception is expected due to non first double quote");

            // Suppress a compiler warning
            BOOST_CHECK(row.GetNumberOfFields() < 1000000);
        }
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected parsing context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NONLAST_QUOTE)
{
    string                  data = "\"abc\"22,def\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        for (auto &  row : src_stream) {
            BOOST_FAIL("Exception is expected due to non last double quote");

            // Suppress a compiler warning
            BOOST_CHECK(row.GetNumberOfFields() < 1000000);
        }
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected parsing context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NON_BALANCED_QUOTE)
{
    string                  data = "\"abc,def\n"
                                   "111,222\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        for (auto &  row : src_stream) {
            BOOST_FAIL("Exception is expected due to non balanced double quote");

            // Suppress a compiler warning
            BOOST_CHECK(row.GetNumberOfFields() < 1000000);
        }
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected parsing context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_VALIDATION_OK)
{
    string                  data = "Name,Age,Address\r\n"
                                   "\"Paul\",23,1115 W Franklin\n"
                                   "Bessy the Cow,\"5\",Big Farm Way\n"
                                   "Zeke,45,\"W Main St\"";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    // No exception is expected
    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NONFIRST_QUOTE_VALIDATION)
{
    string                  data = "abc\"\"def\n"
                                   "ghk";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        src_stream.Validate();
        BOOST_FAIL("Exception is expected due to non first double quote");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected validating context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NONLAST_QUOTE_VALIDATE)
{
    string                  data = "\"abc\"22,def\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        src_stream.Validate();
        BOOST_FAIL("Exception is expected due to non last double quote");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected validating context");
    }
}


BOOST_AUTO_TEST_CASE(IANA_CSV_NON_BALANCED_QUOTE_VALIDATING)
{
    string                  data = "\"abc,def\n"
                                   "111,222\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    try {
        src_stream.Validate();
        BOOST_FAIL("Exception is expected due to non balanced double quote");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("context:") == string::npos)
            BOOST_FAIL("Expected parsing context");
    }
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION)
{
    string                  data = "Integer,Double,Bool\n"
                                   "1,2.2,True\n"
                                   "3,3.14,False\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_FAIL)
{
    string                  data = "Integer,Double,Bool\n"
                                   "1,2.2,True\n"
                                   "3,ExpectedDouble,False\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
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
    string                  data = "Integer,Double,Bool\n"
                                   "1,2.2,True\n"
                                   "3,ExpectedDouble,False\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    src_stream.Validate(CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
                        eRR_NoFieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT)
{
    string                  data = "DateTime\n"
                                   "01/02/1903 12:13:14\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    src_stream.Validate(CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT_FAIL)
{
    string                  data = "DateTime\n"
                                   "12:13:14 04/04/1999\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    try {
        src_stream.Validate(
            CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
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
    string                  data = "DateTime\n"
                                   "1903 01 02 12:13:14\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    src_stream.Validate(CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_EXPLICIT_FORMAT_FAIL)
{
    string                  data = "DateTime\n"
                                   "01 02 1903 12:13:14\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    try {
        src_stream.Validate(
            CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_WITH_TRANSLATION)
{
    string                  data = "Integer,Double,Bool\n"
                                   "\"1\",\"2.2\",\"True\"\n"
                                   "\"3\",\"3.14\",\"False\"\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_FAIL_WITH_TRANSLATION)
{
    string                  data = "Integer,Double,Bool\n"
                                   "\"1\",\"2.2\",\"True\"\n"
                                   "\"3\",\"ExpectedDouble\",\"False\"\n";
    CNcbiIstrstream         data_stream(data);
    TIANACSVStream          src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_IANA_CSV::eRR_ValidationMode_Default,
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
        ("UTIL IANA CSV Row Stream");
}
