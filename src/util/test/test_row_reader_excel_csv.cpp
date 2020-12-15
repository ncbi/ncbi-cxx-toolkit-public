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

#include <util/row_reader_excel_csv.hpp>
#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BOOST_AUTO_TEST_SUITE(CRowReader_EXCEL_CSV_Unit_Test)

typedef CRowReader<CRowReaderStream_Excel_CSV> TExcelCSVStream;



BOOST_AUTO_TEST_CASE(EXCEL_CSV_EMPTY_STREAM)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empty. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_BASIC_VALUES)
{
    string                  data = "123,45.67,True,1988/09/25";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (const auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetOriginalData() == string("123,45.67,True,1988/09/25"));

        BOOST_CHECK(row[0].GetOriginalData() == string("123"));
        BOOST_CHECK(row[0].Get<string>() == string("123"));
        BOOST_CHECK(row[0].Get<long>() == 123);

        BOOST_CHECK(row[1].GetOriginalData() == string("45.67"));
        BOOST_CHECK(row[1].Get<string>() == string("45.67"));
        BOOST_CHECK(row[1].Get<double>() == 45.67);

        BOOST_CHECK(row[2].GetOriginalData() == string("True"));
        BOOST_CHECK(row[2].Get<string>() == string("True"));
        BOOST_CHECK(row[2].Get<bool>() == true);

        BOOST_CHECK(row[3].GetOriginalData() == string("1988/09/25"));
        BOOST_CHECK(row[3].Get<string>() == string("1988/09/25"));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_COMMAS_QUOTES)
{
     string                  data = "read this\n"
                                    "also this'\n"
                                    "and this''\n"
                                    "\"plus this\"\"\"\n"
                                    "\"and, also commas\"\n"
                                    "\"plus, commas and '\"\n"
                                    "\"and, maybe also \"\"\"\n"
                                    "\"plus, a double ''\"\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    int     line_no = 1;
    for (const auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("read this"));
                BOOST_CHECK(row[0].Get<string>() == string("read this"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("also this'"));
                BOOST_CHECK(row[0].Get<string>() == string("also this'"));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("and this''"));
                BOOST_CHECK(row[0].Get<string>() == string("and this''"));
                break;
            case 4:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"plus this\"\"\""));
                BOOST_CHECK(row[0].Get<string>() == string("plus this\""));
                break;
            case 5:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"and, also commas\""));
                BOOST_CHECK(row[0].Get<string>() == string("and, also commas"));
                break;
            case 6:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"plus, commas and '\""));
                BOOST_CHECK(row[0].Get<string>() == string("plus, commas and '"));
                break;
            case 7:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"and, maybe also \"\"\""));
                BOOST_CHECK(row[0].Get<string>() == string("and, maybe also \""));
                break;
            case 8:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"plus, a double ''\""));
                BOOST_CHECK(row[0].Get<string>() == string("plus, a double ''"));
                break;
        }
        ++line_no;
    }

    BOOST_CHECK(line_no == 9);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_EMPTY_FIELDS)
{
    string                  data = ",,,";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    int                     line_no = 1;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 0);
        BOOST_CHECK(row.GetOriginalData() == string(",,,"));
        BOOST_CHECK(row.GetType() == eRR_Data);
        ++line_no;
    }
    BOOST_CHECK(line_no == 2);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_QUOTED_EMPTY_FIELDS)
{
    string                  data = "\"\",\"\",\"\",\"\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    int                     line_no = 1;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 0);
        BOOST_CHECK(row.GetOriginalData() == string("\"\",\"\",\"\",\"\""));
        BOOST_CHECK(row.GetType() == eRR_Data);
        ++line_no;
    }
    BOOST_CHECK(line_no == 2);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_QUOTED_FIELDS)
{
    string                  data = "\"Paul\",23,1115 W Franklin\n"
                                   "Bessy the Cow,\"5\",Big Farm Way\n"
                                   "Zeke,45,\"W Main St\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

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
                BOOST_CHECK(row[0].Get<string>() == string("Bessy the Cow"));
                BOOST_CHECK(row[1].Get<string>() == string("5"));
                BOOST_CHECK(row[1].Get<int>() == 5);
                BOOST_CHECK(row[2].Get<string>() == string("Big Farm Way"));
                BOOST_CHECK(row[0].GetOriginalData() == string("Bessy the Cow"));
                BOOST_CHECK(row[1].GetOriginalData() == string("\"5\""));
                BOOST_CHECK(row[2].GetOriginalData() == string("Big Farm Way"));
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

    BOOST_CHECK(line_no == 4);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_QUOTED_QUOTED_MULTILINE)
{
    string                  data = "\"data1\r\ndata2\",\"data3\ndata4\"";

    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 2);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("\"data1\r\ndata2\",\"data3\ndata4\""));
        BOOST_CHECK(row[0].Get<string>() == string("data1\r\ndata2"));
        BOOST_CHECK(row[1].Get<string>() == string("data3\ndata4"));
        BOOST_CHECK(row[0].GetOriginalData() == string("\"data1\r\ndata2\""));
        BOOST_CHECK(row[1].GetOriginalData() == string("\"data3\ndata4\""));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELDS_WITH_EQUALS)
{
    string                  data = "=777,=888.999,=True,=1989/10/13\r\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("=777,=888.999,=True,=1989/10/13"));

        BOOST_CHECK(row[0].Get<string>() == string("777"));
        BOOST_CHECK(row[0].GetOriginalData() == string("=777"));
        BOOST_CHECK(row[0].Get<int>() == 777);

        BOOST_CHECK(row[1].Get<string>() == string("888.999"));
        BOOST_CHECK(row[1].GetOriginalData() == string("=888.999"));
        BOOST_CHECK(row[1].Get<double>() == 888.999);

        BOOST_CHECK(row[2].Get<string>() == string("True"));
        BOOST_CHECK(row[2].GetOriginalData() == string("=True"));
        BOOST_CHECK(row[2].Get<bool>() == true);

        BOOST_CHECK(row[3].Get<string>() == string("1989/10/13"));
        BOOST_CHECK(row[3].GetOriginalData() == string("=1989/10/13"));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELDS_QUOTED_WITH_EQUALS)
{
    string                  data = "\"=\"\"44\"\"\",\"=\"\"22.11\"\"\",\"=\"\"True\"\"\",\"=\"\"1988/09/25\"\"\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("\"=\"\"44\"\"\",\"=\"\"22.11\"\"\",\"=\"\"True\"\"\",\"=\"\"1988/09/25\"\"\""));

        BOOST_CHECK(row[0].Get<string>() == string("44"));
        BOOST_CHECK(row[0].GetOriginalData() == string("\"=\"\"44\"\"\""));
        BOOST_CHECK(row[0].Get<int>() == 44);

        BOOST_CHECK(row[1].Get<string>() == string("22.11"));
        BOOST_CHECK(row[1].GetOriginalData() == string("\"=\"\"22.11\"\"\""));
        BOOST_CHECK(row[1].Get<double>() == 22.11);

        BOOST_CHECK(row[2].Get<string>() == string("True"));
        BOOST_CHECK(row[2].GetOriginalData() == string("\"=\"\"True\"\"\""));
        BOOST_CHECK(row[2].Get<bool>() == true);

        BOOST_CHECK(row[3].Get<string>() == string("1988/09/25"));
        BOOST_CHECK(row[3].GetOriginalData() == string("\"=\"\"1988/09/25\"\"\""));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELDS_QUOTED_WIERD)
{
    string                  data = "\"123\"456,\"999.\"939,\"AA\" \"BB\",  \"CC\" \"DD\" \"EE\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("\"123\"456,\"999.\"939,\"AA\" \"BB\",  \"CC\" \"DD\" \"EE\""));

        BOOST_CHECK(row[0].Get<string>() == string("123456"));
        BOOST_CHECK(row[0].GetOriginalData() == string("\"123\"456"));
        BOOST_CHECK(row[0].Get<unsigned long>() == 123456);

        BOOST_CHECK(row[1].Get<string>() == string("999.939"));
        BOOST_CHECK(row[1].GetOriginalData() == string("\"999.\"939"));
        BOOST_CHECK(row[1].Get<double>() == 999.939);

        BOOST_CHECK(row[2].Get<string>() == string("AA \"BB\""));
        BOOST_CHECK(row[2].GetOriginalData() == string("\"AA\" \"BB\""));

        BOOST_CHECK(row[3].Get<string>() == string("  \"CC\" \"DD\" \"EE\""));
        BOOST_CHECK(row[3].GetOriginalData() == string("  \"CC\" \"DD\" \"EE\""));
    }
}

BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELD_SPACE_PRESERVED)
{
    string                  data = "   123,456   ";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 2);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("   123,456   "));

        BOOST_CHECK(row[0].Get<string>() == string("   123"));
        BOOST_CHECK(row[0].GetOriginalData() == string("   123"));

        BOOST_CHECK(row[1].Get<string>() == string("456   "));
        BOOST_CHECK(row[1].GetOriginalData() == string("456   "));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELD_QUOTED_WIERD_2)
{
    string                  data = "\"FFF\" \"GG,GG\",\"HHH\" \"KK\nLL\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("\"FFF\" \"GG,GG\",\"HHH\" \"KK"));

                BOOST_CHECK(row[0].Get<string>() == string("FFF \"GG"));
                BOOST_CHECK(row[0].GetOriginalData() == string("\"FFF\" \"GG"));

                BOOST_CHECK(row[1].Get<string>() == string("GG\""));
                BOOST_CHECK(row[1].GetOriginalData() == string("GG\""));

                BOOST_CHECK(row[2].Get<string>() == string("HHH \"KK"));
                BOOST_CHECK(row[2].GetOriginalData() == string("\"HHH\" \"KK"));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 1);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("LL\""));

                BOOST_CHECK(row[0].Get<string>() == string("LL\""));
                BOOST_CHECK(row[0].GetOriginalData() == string("LL\""));
                break;
        }
        ++line_no;
    }

    BOOST_CHECK(line_no == 3);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELD_QUOTED_MORE)
{
    string                  data = "M\"\"MMM\"\",NNN\"\"\"\",\"=\"\"some\"\"one\",=\"MARCH1\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("M\"\"MMM\"\",NNN\"\"\"\",\"=\"\"some\"\"one\",=\"MARCH1\""));

        BOOST_CHECK(row[0].Get<string>() == string("M\"\"MMM\"\""));
        BOOST_CHECK(row[0].GetOriginalData() == string("M\"\"MMM\"\""));

        BOOST_CHECK(row[1].Get<string>() == string("NNN\"\"\"\""));
        BOOST_CHECK(row[1].GetOriginalData() == string("NNN\"\"\"\""));

        BOOST_CHECK(row[2].Get<string>() == string("=\"some\"one"));
        BOOST_CHECK(row[2].GetOriginalData() == string("\"=\"\"some\"\"one\""));

        BOOST_CHECK(row[3].Get<string>() == string("MARCH1"));
        BOOST_CHECK(row[3].GetOriginalData() == string("=\"MARCH1\""));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_FIELD_QUOTED_EVEN_MORE)
{
    string                  data = "=MARCH1\",=\"MARCH1,=\"MAR\"\"CH1\",=\"MAR\"CH1,=MAR\"CH1";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 5);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("=MARCH1\",=\"MARCH1,=\"MAR\"\"CH1\",=\"MAR\"CH1,=MAR\"CH1"));

        BOOST_CHECK(row[0].Get<string>() == string("=MARCH1\""));
        BOOST_CHECK(row[0].GetOriginalData() == string("=MARCH1\""));

        BOOST_CHECK(row[1].Get<string>() == string("=\"MARCH1"));
        BOOST_CHECK(row[1].GetOriginalData() == string("=\"MARCH1"));

        BOOST_CHECK(row[2].Get<string>() == string("MAR\"CH1"));
        BOOST_CHECK(row[2].GetOriginalData() == string("=\"MAR\"\"CH1\""));

        BOOST_CHECK(row[3].Get<string>() == string("=\"MAR\"CH1"));
        BOOST_CHECK(row[3].GetOriginalData() == string("=\"MAR\"CH1"));

        BOOST_CHECK(row[4].Get<string>() == string("=MAR\"CH1"));
        BOOST_CHECK(row[4].GetOriginalData() == string("=MAR\"CH1"));
    }
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_EMPTY_LINE)
{
    string                  data = "\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    int     line_no = 1;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 0);
        BOOST_CHECK(row.GetType() == eRR_Data);
        ++line_no;
    }

    BOOST_CHECK(line_no == 2);
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_UNBALANCED_DBL_QUOTE)
{
    string                  data = "a,\"one\ntwo,\nthree";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 2);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string("a,\"one\ntwo,\nthree"));

        BOOST_CHECK(row[0].Get<string>() == string("a"));
        BOOST_CHECK(row[0].GetOriginalData() == string("a"));

        BOOST_CHECK(row[1].Get<string>() == string("one\ntwo,\nthree"));
        BOOST_CHECK(row[1].GetOriginalData() == string("\"one\ntwo,\nthree"));
    }
}



BOOST_AUTO_TEST_CASE(EXCEL_CSV_VALIDATION_OK)
{
    string                  data = "Name,Age,Address\r\n"
                                   "\"Paul\",23,1115 W Franklin\n"
                                   "Bessy the Cow,\"5\",Big Farm Way\n"
                                   "Zeke,45,\"W Main St\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_NONFIRST_QUOTE_VALIDATION)
{
    string                  data = "abc\"\"def\n"
                                   "ghk";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_NONLAST_QUOTE_VALIDATE)
{
    string                  data = "\"abc\"22,def\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(EXCEL_CSV_NON_BALANCED_QUOTE_VALIDATING)
{
    string                  data = "\"abc,def\n"
                                   "111,222\n"
                                   "333,444";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.Validate();
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION)
{
    string                  data = "1,2.2,True\n"
                                   "3,3.14,False\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_FAIL)
{
    string                  data = "1,2.2,True\n"
                                   "3,ExpectedDouble,False\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
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
    string                  data = "1,2.2,True\n"
                                   "3,ExpectedDouble,False\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    src_stream.Validate(CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
                        eRR_NoFieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT)
{
    string                  data = "01/02/1903 12:13:14\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    src_stream.Validate(CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_DEFAULT_FORMAT_FAIL)
{
    string                  data = "12:13:14 04/04/1999\n"
                                   "02/03/1967 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_DateTime);

    try {
        src_stream.Validate(
            CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
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
    string                  data = "1903 01 02 12:13:14\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    src_stream.Validate(CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(CTIME_VALIDATION_EXPLICIT_FORMAT_FAIL)
{
    string                  data = "01 02 1903 12:13:14\n"
                                   "1967 02 03 07:56:00\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, CRR_FieldType<ERR_FieldType>(eRR_DateTime,
                                                            "Y M D h:m:s"));

    try {
        src_stream.Validate(
            CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
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
    string                  data = "\"1\",\"2.2\",\"True\"\n"
                                   "\"3\",\"3.14\",\"False\"\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);
    src_stream.Validate(CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
                        eRR_FieldValidation);
}


BOOST_AUTO_TEST_CASE(BASIC_TYPE_VALIDATION_FAIL_WITH_TRANSLATION)
{
    string                  data = "\"1\",\"2.2\",\"True\"\n"
                                   "\"3\",\"ExpectedDouble\",\"False\"\n";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_Double);
    src_stream.SetFieldType(2, eRR_Boolean);

    try {
        src_stream.Validate(
            CRowReaderStream_Excel_CSV::eRR_ValidationMode_Default,
            eRR_FieldValidation);
        BOOST_FAIL("Expected a validation exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("Error validating") == string::npos)
            BOOST_FAIL("Expected validating error");
    }
}


BOOST_AUTO_TEST_CASE(TRANSLATION_TO_NULL)
{
    string                  data = ",\"\",=\"\",a,,\"\"";
    CNcbiIstrstream         data_stream(data);
    TExcelCSVStream         src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        // The last two fields are stripped because they are translated to null
        BOOST_CHECK(row.GetNumberOfFields() == 4);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string(",\"\",=\"\",a,,\"\""));

        // The first two fields are translated to null
        BOOST_CHECK(row[0].IsNull() == true);
        BOOST_CHECK(row[0].GetOriginalData() == string(""));

        BOOST_CHECK(row[1].IsNull() == true);
        BOOST_CHECK(row[1].GetOriginalData() == string("\"\""));

        // The third is translated to an empty string
        BOOST_CHECK(row[2].IsNull() == false);
        BOOST_CHECK(row[2].GetOriginalData() == string("=\"\""));
        BOOST_CHECK(row[2].Get<string>() == string(""));

        BOOST_CHECK(row[3].IsNull() == false);
        BOOST_CHECK(row[3].GetOriginalData() == string("a"));
    }
}


BOOST_AUTO_TEST_SUITE_END()
END_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// main entry point for tests

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("UTIL EXCEL CSV Row Stream");
}
