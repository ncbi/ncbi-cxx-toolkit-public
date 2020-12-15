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

#include <util/row_reader_char_delimited.hpp>
#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
BOOST_AUTO_TEST_SUITE(CRowReader_Unit_Test)


typedef CRowReader<TRowReaderStream_SingleTabDelimited>   TTabDelimitedStream;
typedef CRowReader<TRowReaderStream_SingleSpaceDelimited> TSpaceDelimitedStream;
typedef CRowReader<TRowReaderStream_SingleCommaDelimited> TCommaDelimitedStream;


const string kRRDataFileName = "test_row_reader.txt";
const string kRRContextPattern = "Row reader context:";


BOOST_AUTO_TEST_CASE(RR_EMPTY_STREAM)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_FAIL("The stream is empy. Nothing is expected. "
                   "Row data: " + row.GetOriginalData());
    }
}


BOOST_AUTO_TEST_CASE(RR_EMPTY_LINES_STREAM)
{
    string          data = "\n"
                           "\n"
                           "\n";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    TLineNo     line_no = 0;
    TStreamPos  line_pos = 0;
    for (auto &  row : src_stream) {
        BOOST_CHECK(src_stream.GetCurrentRowPos() == line_pos);
        ++line_pos;
        BOOST_CHECK(src_stream.GetCurrentLineNo() == line_no);
        ++line_no;

        BOOST_CHECK(row.GetNumberOfFields() == 0);
        BOOST_CHECK(row.GetType() == eRR_Data);
        BOOST_CHECK(row.GetOriginalData() == string(""));
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_STREAM)
{
    string                  data = "11\t12\t13\t14\r\n"
                                   "21\t22\t23\t\t\r\n"
                                   "31\t\r\n"
                                   "41\t\t\t";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 5);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                BOOST_CHECK(row[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31\t"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 41);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                BOOST_CHECK(row[2].GetOriginalData() == string(""));
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_STREAM_VIA_ITERATOR)
{
    string                  data = "11\t12\t13\t14\r\n"
                                   "21\t22\t23\t\t\r\n"
                                   "31\t\r\n"
                                   "41\t\t\t";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    int     line_no = 0;
    for (TTabDelimitedStream::CRowIterator it = src_stream.begin();
         it != src_stream.end(); ++it) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(it[0].Get<int>() == 11);
                BOOST_CHECK(it[1].Get<int>() == 12);
                BOOST_CHECK(it[2].Get<int>() == 13);
                BOOST_CHECK(it[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(it[0].Get<int>() == 21);
                BOOST_CHECK(it[1].Get<int>() == 22);
                BOOST_CHECK(it[2].Get<int>() == 23);
                BOOST_CHECK(it[3].GetOriginalData() == string(""));
                BOOST_CHECK(it[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(it[0].Get<int>() == 31);
                BOOST_CHECK(it[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(it[0].Get<int>() == 41);
                BOOST_CHECK(it[1].GetOriginalData() == string(""));
                BOOST_CHECK(it[2].GetOriginalData() == string(""));
                BOOST_CHECK(it[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_WITH_OUTSIDE_COLUMN_NAMES)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    src_stream.SetFieldName(0, "index");
    src_stream.SetFieldName(1, "strval");
    src_stream.SetFieldName(2, "intval");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("1\tone\t111"));
                BOOST_CHECK(row["index"].Get<int>() == 1);
                BOOST_CHECK(row["index"].Get<string>() == string("1"));
                BOOST_CHECK(row["strval"].Get<string>() == string("one"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("one"));
                BOOST_CHECK(row["intval"].Get<int>() == 111);
                BOOST_CHECK(row["intval"].Get<string>() == string("111"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("111"));

                {
                    auto field_info = row.GetFieldsMetaInfo();
                    BOOST_CHECK(field_info.size() == 3);
                    if (field_info.size() == 3) {
                        BOOST_CHECK(field_info[0].is_name_initialized == true);
                        BOOST_CHECK(field_info[0].name == string("index"));
                        BOOST_CHECK(field_info[1].is_name_initialized == true);
                        BOOST_CHECK(field_info[1].name == string("strval"));
                        BOOST_CHECK(field_info[2].is_name_initialized == true);
                        BOOST_CHECK(field_info[2].name == string("intval"));
                    }
                }
                break;
            case 1:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("2\ttwo\t222"));
                BOOST_CHECK(row["index"].Get<int>() == 2);
                BOOST_CHECK(row["index"].Get<string>() == string("2"));
                BOOST_CHECK(row["strval"].Get<string>() == string("two"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("two"));
                BOOST_CHECK(row["intval"].Get<int>() == 222);
                BOOST_CHECK(row["intval"].Get<string>() == string("222"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("222"));
                break;
            case 2:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("3\tthree\t333"));
                BOOST_CHECK(row["index"].Get<int>() == 3);
                BOOST_CHECK(row["index"].Get<string>() == string("3"));
                BOOST_CHECK(row["strval"].Get<string>() == string("three"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("three"));
                BOOST_CHECK(row["intval"].Get<int>() == 333);
                BOOST_CHECK(row["intval"].Get<string>() == string("333"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("333"));
                break;
            }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_ROW_COPYING)
{
    string                      data = "1\tone\t111\r\n"
                                       "2\ttwo\t222\n"
                                       "3\tthree\t333";
    CNcbiIstrstream             data_stream(data);
    TTabDelimitedStream         src_stream(&data_stream, "");
    TTabDelimitedStream::CRow   row1;
    TTabDelimitedStream::CRow   row2;
    TTabDelimitedStream::CRow   row3;

    src_stream.SetFieldName(0, "index");
    src_stream.SetFieldName(1, "strval");
    src_stream.SetFieldName(2, "intval");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                row1 = row;
                break;
            case 1:
                row2 = row;
                break;
            case 2:
                row3 = row;
                break;
            }
        ++line_no;
    }

    BOOST_CHECK(row1.GetType() == eRR_Data);
    BOOST_CHECK(row1.GetOriginalData() == string("1\tone\t111"));
    BOOST_CHECK(row1["index"].Get<int>() == 1);
    BOOST_CHECK(row1["index"].Get<string>() == string("1"));
    BOOST_CHECK(row1["strval"].Get<string>() == string("one"));
    BOOST_CHECK(row1["strval"].GetOriginalData() == string("one"));
    BOOST_CHECK(row1["intval"].Get<int>() == 111);
    BOOST_CHECK(row1["intval"].Get<string>() == string("111"));
    BOOST_CHECK(row1["intval"].GetOriginalData() == string("111"));

    BOOST_CHECK(row2.GetType() == eRR_Data);
    BOOST_CHECK(row2.GetOriginalData() == string("2\ttwo\t222"));
    BOOST_CHECK(row2["index"].Get<int>() == 2);
    BOOST_CHECK(row2["index"].Get<string>() == string("2"));
    BOOST_CHECK(row2["strval"].Get<string>() == string("two"));
    BOOST_CHECK(row2["strval"].GetOriginalData() == string("two"));
    BOOST_CHECK(row2["intval"].Get<int>() == 222);
    BOOST_CHECK(row2["intval"].Get<string>() == string("222"));
    BOOST_CHECK(row2["intval"].GetOriginalData() == string("222"));

    BOOST_CHECK(row3.GetType() == eRR_Data);
    BOOST_CHECK(row3.GetOriginalData() == string("3\tthree\t333"));
    BOOST_CHECK(row3["index"].Get<int>() == 3);
    BOOST_CHECK(row3["index"].Get<string>() == string("3"));
    BOOST_CHECK(row3["strval"].Get<string>() == string("three"));
    BOOST_CHECK(row3["strval"].GetOriginalData() == string("three"));
    BOOST_CHECK(row3["intval"].Get<int>() == 333);
    BOOST_CHECK(row3["intval"].Get<string>() == string("333"));
    BOOST_CHECK(row3["intval"].GetOriginalData() == string("333"));


    auto field_info = src_stream.GetFieldsMetaInfo();

    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("index"));
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("strval"));
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("intval"));
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_WITH_COLUMN_NAMES)
{
    string                  data = "index\tstrval\tintval\n"
                                   "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("index\tstrval\tintval"));
                BOOST_CHECK(row[0].GetOriginalData() == string("index"));
                BOOST_CHECK(row[1].GetOriginalData() == string("strval"));
                BOOST_CHECK(row[2].GetOriginalData() == string("intval"));
                src_stream.SetFieldName(0, row[0].GetOriginalData());
                src_stream.SetFieldName(1, row[1].GetOriginalData());
                src_stream.SetFieldName(2, row[2].GetOriginalData());
                break;
            case 1:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("1\tone\t111"));
                BOOST_CHECK(row["index"].Get<int>() == 1);
                BOOST_CHECK(row["index"].Get<string>() == string("1"));
                BOOST_CHECK(row["strval"].Get<string>() == string("one"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("one"));
                BOOST_CHECK(row["intval"].Get<int>() == 111);
                BOOST_CHECK(row["intval"].Get<string>() == string("111"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("111"));
                break;
            case 2:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("2\ttwo\t222"));
                BOOST_CHECK(row["index"].Get<int>() == 2);
                BOOST_CHECK(row["index"].Get<string>() == string("2"));
                BOOST_CHECK(row["strval"].Get<string>() == string("two"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("two"));
                BOOST_CHECK(row["intval"].Get<int>() == 222);
                BOOST_CHECK(row["intval"].Get<string>() == string("222"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("222"));
                break;
            case 3:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("3\tthree\t333"));
                BOOST_CHECK(row["index"].Get<int>() == 3);
                BOOST_CHECK(row["index"].Get<string>() == string("3"));
                BOOST_CHECK(row["strval"].Get<string>() == string("three"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("three"));
                BOOST_CHECK(row["intval"].Get<int>() == 333);
                BOOST_CHECK(row["intval"].Get<string>() == string("333"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("333"));
                break;
            }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_CONVERSIONS)
{
    string                  data = "true\tfalse\t100\t100.25\t01/02/1903 12:13:14\t02/01/1903 12:13:14";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row[0].IsNull() == false);
        BOOST_CHECK(row[0].GetOriginalData() == string("true"));
        BOOST_CHECK(row[0].Get<bool>() == true);
        BOOST_CHECK(row[0].GetWithDefault(false) == true);

        BOOST_CHECK(row[1].IsNull() == false);
        BOOST_CHECK(row[1].GetOriginalData() == string("false"));
        BOOST_CHECK(row[1].Get<bool>() == false);
        BOOST_CHECK(row[1].GetWithDefault(true) == false);

        BOOST_CHECK(row[2].IsNull() == false);
        BOOST_CHECK(row[2].GetOriginalData() == string("100"));
        BOOST_CHECK(row[2].Get<short>() == 100);
        BOOST_CHECK(row[2].Get<unsigned short>() == 100);
        BOOST_CHECK(row[2].Get<int>() == 100);
        BOOST_CHECK(row[2].Get<unsigned int>() == 100);
        BOOST_CHECK(row[2].Get<long>() == 100);
        BOOST_CHECK(row[2].Get<unsigned long>() == 100);
        BOOST_CHECK(row[2].Get<Int1>() == 100);
        BOOST_CHECK(row[2].Get<Uint1>() == 100);
        BOOST_CHECK(row[2].Get<Int2>() == 100);
        BOOST_CHECK(row[2].Get<Uint2>() == 100);
        BOOST_CHECK(row[2].Get<Int4>() == 100);
        BOOST_CHECK(row[2].Get<Uint4>() == 100);
        BOOST_CHECK(row[2].Get<Int8>() == 100);
        BOOST_CHECK(row[2].Get<Uint8>() == 100);

        BOOST_CHECK(row[3].IsNull() == false);
        BOOST_CHECK(row[3].GetOriginalData() == string("100.25"));
        BOOST_CHECK(row[3].Get<float>() == 100.25);
        BOOST_CHECK(row[3].Get<double>() == 100.25);

        BOOST_CHECK(row[4].IsNull() == false);
        BOOST_CHECK(row[4].GetOriginalData() == string("01/02/1903 12:13:14"));
        BOOST_CHECK(row[4].Get<CTime>() == CTime("01/02/1903 12:13:14"));
        BOOST_CHECK(row[4].Get<CTime>().Month() == 1);
        BOOST_CHECK(row[4].Get<CTime>().Day() == 2);
        BOOST_CHECK(row[4].Get<CTime>().Year() == 1903);
        BOOST_CHECK(row[4].Get<CTime>().Hour() == 12);
        BOOST_CHECK(row[4].Get<CTime>().Minute() == 13);
        BOOST_CHECK(row[4].Get<CTime>().Second() == 14);

        BOOST_CHECK(row[5].IsNull() == false);
        BOOST_CHECK(row[5].GetOriginalData() == string("02/01/1903 12:13:14"));
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")) == CTime("01/02/1903 12:13:14"));
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Month() == 1);
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Day() == 2);
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Year() == 1903);
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Hour() == 12);
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Minute() == 13);
        BOOST_CHECK(row[5].Get(CTimeFormat("D/M/Y h:m:s")).Second() == 14);
    }
}


BOOST_AUTO_TEST_CASE(RR_ROW_COPY_METADATA)
{
    string                      data = "1\tone\t111\r\n"
                                       "2\ttwo\t222";
    CNcbiIstrstream             data_stream(data);
    TTabDelimitedStream         src_stream(&data_stream, "");
    TTabDelimitedStream::CRow   row1;
    TTabDelimitedStream::CRow   row2;

    int     line_no = 0;
    for (const auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                src_stream.SetFieldName(0, "index1");
                src_stream.SetFieldName(1, "strval1");
                src_stream.SetFieldName(2, "intval1");
                row1 = row;
                break;
            case 1:
                src_stream.ClearFieldsInfo();
                src_stream.SetFieldName(0, "index2");
                src_stream.SetFieldName(1, "strval2");
                src_stream.SetFieldName(2, "intval2");
                row2 = row;
                break;
            }
        ++line_no;
    }

    BOOST_CHECK(row1.GetType() == eRR_Data);
    BOOST_CHECK(row1.GetOriginalData() == string("1\tone\t111"));
    BOOST_CHECK(row1["index1"].Get<int>() == 1);
    BOOST_CHECK(row1["index1"].Get<string>() == string("1"));
    BOOST_CHECK(row1["strval1"].Get<string>() == string("one"));
    BOOST_CHECK(row1["strval1"].GetOriginalData() == string("one"));
    BOOST_CHECK(row1["intval1"].Get<int>() == 111);
    BOOST_CHECK(row1["intval1"].Get<string>() == string("111"));
    BOOST_CHECK(row1["intval1"].GetOriginalData() == string("111"));

    BOOST_CHECK(row2.GetType() == eRR_Data);
    BOOST_CHECK(row2.GetOriginalData() == string("2\ttwo\t222"));
    BOOST_CHECK(row2["index2"].Get<int>() == 2);
    BOOST_CHECK(row2["index2"].Get<string>() == string("2"));
    BOOST_CHECK(row2["strval2"].Get<string>() == string("two"));
    BOOST_CHECK(row2["strval2"].GetOriginalData() == string("two"));
    BOOST_CHECK(row2["intval2"].Get<int>() == 222);
    BOOST_CHECK(row2["intval2"].Get<string>() == string("222"));
    BOOST_CHECK(row2["intval2"].GetOriginalData() == string("222"));
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_WITH_OUTSIDE_COLUMN_TYPES)
{
    string                  data = "1\tone\t1.1\r\n"
                                   "2\ttwo\t2.2\n"
                                   "3\tthree\t3.3";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    src_stream.SetFieldType(0, eRR_Integer);
    src_stream.SetFieldType(1, eRR_String);
    src_stream.SetFieldType(2, eRR_Double);

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
        BOOST_CHECK(row.GetType() == eRR_Data);

        auto field_info = row.GetFieldsMetaInfo();
        BOOST_CHECK(field_info.size() == 3);
        if (field_info.size() == 3) {
            BOOST_CHECK(field_info[0].is_type_initialized == true);
            BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);
            BOOST_CHECK(field_info[1].is_type_initialized == true);
            BOOST_CHECK(field_info[1].type.GetType() == eRR_String);
            BOOST_CHECK(field_info[2].is_type_initialized == true);
            BOOST_CHECK(field_info[2].type.GetType() == eRR_Double);
        }
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_type_initialized == true);
        BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);
        BOOST_CHECK(field_info[1].is_type_initialized == true);
        BOOST_CHECK(field_info[1].type.GetType() == eRR_String);
        BOOST_CHECK(field_info[2].is_type_initialized == true);
        BOOST_CHECK(field_info[2].type.GetType() == eRR_Double);
    }
}


BOOST_AUTO_TEST_CASE(RR_TAB_DATA_WITH_OUTSIDE_COLUMN_EXT_TYPES)
{
    string                  data = "1\tone\t1.1\r\n"
                                   "2\ttwo\t2.2\n"
                                   "3\tthree\t3.3";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    src_stream.SetFieldTypeEx(0, eRR_Integer, eRR_String);
    src_stream.SetFieldTypeEx(1, eRR_String, eRR_Double);
    src_stream.SetFieldTypeEx(2, eRR_Double, eRR_Integer);

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
        BOOST_CHECK(row.GetType() == eRR_Data);

        auto field_info = row.GetFieldsMetaInfo();
        BOOST_CHECK(field_info.size() == 3);
        if (field_info.size() == 3) {
            BOOST_CHECK(field_info[0].is_type_initialized == true);
            BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);
            BOOST_CHECK(field_info[0].is_ext_type_initialized == true);
            BOOST_CHECK(field_info[0].ext_type.GetType() == eRR_String);
            BOOST_CHECK(field_info[1].is_type_initialized == true);
            BOOST_CHECK(field_info[1].type.GetType() == eRR_String);
            BOOST_CHECK(field_info[1].is_ext_type_initialized == true);
            BOOST_CHECK(field_info[1].ext_type.GetType() == eRR_Double);
            BOOST_CHECK(field_info[2].is_type_initialized == true);
            BOOST_CHECK(field_info[2].type.GetType() == eRR_Double);
            BOOST_CHECK(field_info[2].is_ext_type_initialized == true);
            BOOST_CHECK(field_info[2].ext_type.GetType() == eRR_Integer);
        }
    }

    auto field_info = src_stream.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_type_initialized == true);
        BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);
        BOOST_CHECK(field_info[0].is_ext_type_initialized == true);
        BOOST_CHECK(field_info[0].ext_type.GetType() == eRR_String);
        BOOST_CHECK(field_info[1].is_type_initialized == true);
        BOOST_CHECK(field_info[1].type.GetType() == eRR_String);
        BOOST_CHECK(field_info[1].is_ext_type_initialized == true);
        BOOST_CHECK(field_info[1].ext_type.GetType() == eRR_Double);
        BOOST_CHECK(field_info[2].is_type_initialized == true);
        BOOST_CHECK(field_info[2].type.GetType() == eRR_Double);
        BOOST_CHECK(field_info[2].is_ext_type_initialized == true);
        BOOST_CHECK(field_info[2].ext_type.GetType() == eRR_Integer);
    }
}


BOOST_AUTO_TEST_CASE(RR_CONVERSIONS_EXCEPTIONS)
{
    string                  data = "1000000\tone\t1.1";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        try {
            row[0].Get<Int2>();
            BOOST_FAIL("Expected overflow exception");
        } catch (const exception &  exc) {
            string  what = exc.what();
            if (what.find(kRRContextPattern) == string::npos)
                BOOST_FAIL("Expected overflow exception context");
        }

        try {
            row[1].Get<Int8>();
            BOOST_FAIL("Expected wrong type exception");
        } catch (const exception &  exc) {
            string  what = exc.what();
            if (what.find(kRRContextPattern) == string::npos)
                BOOST_FAIL("Expected overflow exception context");
        }

        try {
            row[3].Get<double>();
            BOOST_FAIL("Expected no field exception");
        } catch (const exception &  exc) {
            string  what = exc.what();
            if (what.find(kRRContextPattern) == string::npos)
                BOOST_FAIL("Expected no field exception context");
        }

        auto field_info = row.GetFieldsMetaInfo(0, 0);
        BOOST_CHECK(field_info.size() == 0);

        field_info = row.GetFieldsMetaInfo(1, 1);
        BOOST_CHECK(field_info.size() == 0);

        field_info = row.GetFieldsMetaInfo(2, 2);
        BOOST_CHECK(field_info.size() == 0);
    }
}


BOOST_AUTO_TEST_CASE(RR_FIELD_ACCESS_BY_NAME_OUT_OF_RANGE)
{
    string                  data = "1000000\tone\t1.1";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    src_stream.SetFieldName(500, "lb");
    for (auto &  row : src_stream) {
        try {
            row["lb"].GetOriginalData();
            BOOST_FAIL("Expected no field exception");
        } catch (const exception &  exc) {
            string  what = exc.what();
            if (what.find(kRRContextPattern) == string::npos)
                BOOST_FAIL("Expected no field exception context");
        }

        try {
            row["kilo"].GetOriginalData();
            BOOST_FAIL("Expected no field exception");
        } catch (const exception &  exc) {
            string  what = exc.what();
            if (what.find(kRRContextPattern) == string::npos)
                BOOST_FAIL("Expected no field exception context");
        }
    }
}


BOOST_AUTO_TEST_CASE(RR_ROW_COPY_FIELD_RENAME)
{
    string                      data = "1\tone\t1.1\r\n"
                                       "2\ttwo\t2.2\n"
                                       "3\tthree\t3.3";
    CNcbiIstrstream             data_stream(data);
    TTabDelimitedStream         src_stream(&data_stream, "");
    TTabDelimitedStream::CRow   row1;
    TTabDelimitedStream::CRow   row2;
    TTabDelimitedStream::CRow   row3;

    // Intentional: setting the same name for two fields
    src_stream.SetFieldName(0, "field");
    src_stream.SetFieldName(1, "field");
    src_stream.SetFieldName(2, "floatval");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                // Intentional: setting the same name twice for the same field
                src_stream.SetFieldName(1, "other_field");
                src_stream.SetFieldName(1, "other_field");
                src_stream.SetFieldType(0, eRR_Integer);
                row1 = row;
                break;
            case 1:
                src_stream.SetFieldName(1, "field");
                src_stream.SetFieldName(0, "new_name_int");
                src_stream.SetFieldName(1, "new_name_str");
                src_stream.SetFieldName(2, "new_name_float");
                src_stream.SetFieldTypeEx(1, eRR_String, eRR_String);
                row2 = row;
                break;
            case 2:
                src_stream.ClearFieldsInfo();
                src_stream.SetFieldName(0, "name_int");
                src_stream.SetFieldName(1, "name_str");
                src_stream.SetFieldName(2, "name_float");
                src_stream.SetFieldTypeEx(2, eRR_Double, eRR_Double);
                row3 = row;
                break;
            }
        ++line_no;
    }

    // Access to copied row 1
    auto field_info = row1.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("field"));
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("other_field"));
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("floatval"));

        BOOST_CHECK(field_info[0].is_type_initialized == true);
        BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);

        BOOST_CHECK(field_info[1].is_type_initialized == false);
        BOOST_CHECK(field_info[2].is_type_initialized == false);

        BOOST_CHECK(field_info[0].is_ext_type_initialized == false);
        BOOST_CHECK(field_info[1].is_ext_type_initialized == false);
        BOOST_CHECK(field_info[2].is_ext_type_initialized == false);
    }

    // Access to copied row 2
    field_info = row2.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("new_name_int"));
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("new_name_str"));
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("new_name_float"));

        BOOST_CHECK(field_info[0].is_type_initialized == true);
        BOOST_CHECK(field_info[0].type.GetType() == eRR_Integer);
        BOOST_CHECK(field_info[0].is_ext_type_initialized == false);

        BOOST_CHECK(field_info[1].is_type_initialized == true);
        BOOST_CHECK(field_info[1].type.GetType() == eRR_String);
        BOOST_CHECK(field_info[1].is_ext_type_initialized == true);
        BOOST_CHECK(field_info[1].ext_type.GetType() == eRR_String);

        BOOST_CHECK(field_info[2].is_type_initialized == false);
        BOOST_CHECK(field_info[2].is_ext_type_initialized == false);
    }

    // Access to copied row 3
    field_info = row3.GetFieldsMetaInfo();
    BOOST_CHECK(field_info.size() == 3);
    if (field_info.size() == 3) {
        BOOST_CHECK(field_info[0].is_name_initialized == true);
        BOOST_CHECK(field_info[0].name == string("name_int"));
        BOOST_CHECK(field_info[1].is_name_initialized == true);
        BOOST_CHECK(field_info[1].name == string("name_str"));
        BOOST_CHECK(field_info[2].is_name_initialized == true);
        BOOST_CHECK(field_info[2].name == string("name_float"));

        BOOST_CHECK(field_info[2].is_type_initialized == true);
        BOOST_CHECK(field_info[2].type.GetType() == eRR_Double);
        BOOST_CHECK(field_info[2].is_ext_type_initialized == true);
        BOOST_CHECK(field_info[2].ext_type.GetType() == eRR_Double);

        BOOST_CHECK(field_info[0].is_type_initialized == false);
        BOOST_CHECK(field_info[0].is_ext_type_initialized == false);
        BOOST_CHECK(field_info[1].is_type_initialized == false);
        BOOST_CHECK(field_info[1].is_ext_type_initialized == false);
    }
}



class CTestStreamTraits : public CRowReaderStream_Base
{
public:
    CTestStreamTraits() :
        m_BeginSourceCount(0), m_EndSourceCount(0),
        m_ValidateCount(0), m_MyStream(nullptr)
    {}

    enum MyExtendedFieldType {
        eAbnormalDecimal,
        eAbnormalFloat
    };

    typedef MyExtendedFieldType TExtendedFieldType;
    typedef CRR_Context TRR_Context;

    ERR_Action Validate(CTempString /*raw_line*/,
                        ERR_FieldValidationMode /*field_validation_mode*/)
    {
        ++m_ValidateCount;
        return eRR_Skip;
    }

    ERR_Action OnNextLine(CTempString raw_line)
    {
      // Check GetTraits()
      CTestStreamTraits &   traits = GetMyStream().GetTraits();
      BOOST_CHECK(traits.GetFlags() == fRR_Default);

      if (raw_line.empty())
        return eRR_Skip;
      if (raw_line[0] == '#')
        return eRR_Continue_Comment;
      if (raw_line[0] == '=')
        return eRR_Continue_Metadata;
      return eRR_Continue_Data; }

    ERR_Action Tokenize(const CTempString    raw_line,
                        vector<CTempString>& tokens)
    {
        NStr::Split(raw_line, ",", tokens, 0);
        return eRR_Continue_Data;
    }

    ERR_TranslationResult Translate(TFieldNo          /* field_no */,
                                    const CTempString raw_value,
                                    string&           translated_value)
    { if (raw_value == "null")
        return eRR_Null;
      if (raw_value == "one") {
        translated_value = "ONE";
        return eRR_Translated;
      }
      return eRR_UseOriginal; }

    TTraitsFlags GetFlags(void)  const { return fRR_Default; }

    TRR_Context GetContext(const CRR_Context& stream_ctx) const
    { return stream_ctx; }

    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode /*event_mode*/)
    {
        switch (event) {
            case eRR_Event_SourceEnd:
                ++m_EndSourceCount;
                break;
            case eRR_Event_SourceBegin:
                ++m_BeginSourceCount;
                break;
            case eRR_Event_SourceError:
            default:
                break;
        }
        return eRR_EventAction_Default;
    }

    size_t GetBeginSourceCount(void) const
    { return m_BeginSourceCount; }

    size_t  GetEndReachedCount(void) const
    { return m_EndSourceCount; }

    size_t  GetValidCount(void) const
    { return m_ValidateCount; }

private:
    size_t      m_BeginSourceCount;
    size_t      m_EndSourceCount;
    size_t      m_ValidateCount;

    RR_TRAITS_PARENT_STREAM(CTestStreamTraits);
};
typedef CRowReader<CTestStreamTraits>  TTestDelimitedStream;



BOOST_AUTO_TEST_CASE(RR_NULL_FIELD_VALUE)
{
    string                      data = "1,one,1.1\r\n"
                                       "null,two,null";
    CNcbiIstrstream             data_stream(data);
    TTestDelimitedStream        src_stream(&data_stream, "");

    src_stream.SetFieldTypeEx(2, eRR_Double,
                              CTestStreamTraits::eAbnormalFloat);

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row[1].Get<string>() == string("ONE"));
                BOOST_CHECK(row[1].GetOriginalData() == string("one"));
                BOOST_CHECK(row[0].IsNull() == false);
                BOOST_CHECK(row[1].IsNull() == false);
                BOOST_CHECK(row[2].IsNull() == false);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row[0].IsNull() == true);
                BOOST_CHECK(row[1].IsNull() == false);
                BOOST_CHECK(row[2].IsNull() == true);
                BOOST_CHECK(row[0].GetOriginalData() == string("null"));
                BOOST_CHECK(row[1].GetOriginalData() == string("two"));
                BOOST_CHECK(row[2].GetOriginalData() == string("null"));

                BOOST_CHECK(row[0].GetWithDefault(154) == 154);
                BOOST_CHECK(row[2].GetWithDefault(false) == false);

                try {
                    row[0].Get<string>();
                    BOOST_FAIL("Expected 'field is null' exception");
                } catch (const exception &  exc) {
                    string  what = exc.what();
                    if (what.find(kRRContextPattern) == string::npos)
                        BOOST_FAIL("Expected field is null exception context");
                }
                try {
                    row[2].Get<int>();
                    BOOST_FAIL("Expected 'field is null' exception");
                } catch (const exception &  exc) {
                    string  what = exc.what();
                    if (what.find(kRRContextPattern) == string::npos)
                        BOOST_FAIL("Expected field is null exception context");
                }
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_COMMENT_METADATA)
{
    string                      data = "1,one,1.1\n"
                                       "# comment\n"
                                       "= metadata";
    CNcbiIstrstream             data_stream(data);
    TTestDelimitedStream        src_stream(&data_stream, "");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetNumberOfFields() == 3);
                break;
            case 1:
                BOOST_CHECK(row.GetType() == eRR_Comment);
                BOOST_CHECK(row.GetNumberOfFields() == 0);
                break;
            case 2:
                BOOST_CHECK(row.GetType() == eRR_Metadata);
                BOOST_CHECK(row.GetNumberOfFields() == 0);
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_NON_EXISTENT_FILE_STREAM)
{
    try {
        TTestDelimitedStream    src_stream("non-existing");
        BOOST_FAIL("Expected non-existent file exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("File non-existing is not found") == string::npos)
            BOOST_FAIL("Expected field is null exception context");
    }
}


BOOST_AUTO_TEST_CASE(RR_SET_NON_EXISTING_FILE_STREAM)
{
    string                      data = "1,one,1.1\n"
                                       "# comment\n"
                                       "= metadata";
    CNcbiIstrstream             data_stream(data);
    TTestDelimitedStream        src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetType() == eRR_Data);
        break;
    }

    try {
        src_stream.SetDataSource("non-existing");
        BOOST_FAIL("Expected non-existent file exception");
    } catch (const exception &  exc) {
        string  what = exc.what();
        if (what.find("File non-existing is not found") == string::npos)
            BOOST_FAIL("Expected field is null exception context");
    }
}


BOOST_AUTO_TEST_CASE(RR_FILE_STREAM)
{
    TTabDelimitedStream     src_stream(kRRDataFileName);

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 5);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                BOOST_CHECK(row[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31\t"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 41);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                BOOST_CHECK(row[2].GetOriginalData() == string(""));
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_TWO_STREAMS_FILE_STR)
{
    TTabDelimitedStream     src_stream(kRRDataFileName);

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetType() == eRR_Data);
    }


    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    src_stream.SetDataSource(&data_stream, "");

    src_stream.SetFieldName(0, "index");
    src_stream.SetFieldName(1, "strval");
    src_stream.SetFieldName(2, "intval");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("1\tone\t111"));
                BOOST_CHECK(row["index"].Get<int>() == 1);
                BOOST_CHECK(row["index"].Get<string>() == string("1"));
                BOOST_CHECK(row["strval"].Get<string>() == string("one"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("one"));
                BOOST_CHECK(row["intval"].Get<int>() == 111);
                BOOST_CHECK(row["intval"].Get<string>() == string("111"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("111"));

                {
                    auto field_info = row.GetFieldsMetaInfo();

                    BOOST_CHECK(field_info.size() == 3);
                    if (field_info.size() == 3) {
                        BOOST_CHECK(field_info[0].is_name_initialized == true);
                        BOOST_CHECK(field_info[0].name == string("index"));
                        BOOST_CHECK(field_info[1].is_name_initialized == true);
                        BOOST_CHECK(field_info[1].name == string("strval"));
                        BOOST_CHECK(field_info[2].is_name_initialized == true);
                        BOOST_CHECK(field_info[2].name == string("intval"));
                    }
                }
                break;
            case 1:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("2\ttwo\t222"));
                BOOST_CHECK(row["index"].Get<int>() == 2);
                BOOST_CHECK(row["index"].Get<string>() == string("2"));
                BOOST_CHECK(row["strval"].Get<string>() == string("two"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("two"));
                BOOST_CHECK(row["intval"].Get<int>() == 222);
                BOOST_CHECK(row["intval"].Get<string>() == string("222"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("222"));
                break;
            case 2:
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("3\tthree\t333"));
                BOOST_CHECK(row["index"].Get<int>() == 3);
                BOOST_CHECK(row["index"].Get<string>() == string("3"));
                BOOST_CHECK(row["strval"].Get<string>() == string("three"));
                BOOST_CHECK(row["strval"].GetOriginalData() == string("three"));
                BOOST_CHECK(row["intval"].Get<int>() == 333);
                BOOST_CHECK(row["intval"].Get<string>() == string("333"));
                BOOST_CHECK(row["intval"].GetOriginalData() == string("333"));
                break;
            }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_TWO_STREAMS_STR_FILE)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    for (auto &  row : src_stream) {
        BOOST_CHECK(row.GetNumberOfFields() == 3);
    }


    src_stream.SetDataSource(kRRDataFileName);

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 5);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                BOOST_CHECK(row[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31\t"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 41);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                BOOST_CHECK(row[2].GetOriginalData() == string(""));
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_VALIDATE)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    src_stream.Validate();

    BOOST_CHECK(src_stream.GetCurrentRowPos() == 0);
    BOOST_CHECK(src_stream.GetCurrentLineNo() == 0);
}


BOOST_AUTO_TEST_CASE(RR_SWITCH_STREAM_CONTINUE_ITERATE)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    auto it = src_stream.begin();
    auto end_it = src_stream.end();

    for (; it != end_it; ++it) {
        BOOST_CHECK(it->GetNumberOfFields() == 3);
    }


    src_stream.SetDataSource(kRRDataFileName);

    int     line_no = 0;
    for (++it; it != end_it; ++it) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(it->GetNumberOfFields() == 4);
                BOOST_CHECK(it->GetType() == eRR_Data);
                BOOST_CHECK(it->GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(it[0].Get<int>() == 11);
                BOOST_CHECK(it[1].Get<int>() == 12);
                BOOST_CHECK(it[2].Get<int>() == 13);
                BOOST_CHECK(it[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(it->GetNumberOfFields() == 5);
                BOOST_CHECK(it->GetType() == eRR_Data);
                BOOST_CHECK(it->GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(it[0].Get<int>() == 21);
                BOOST_CHECK(it[1].Get<int>() == 22);
                BOOST_CHECK(it[2].Get<int>() == 23);
                BOOST_CHECK(it[3].GetOriginalData() == string(""));
                BOOST_CHECK(it[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(it->GetNumberOfFields() == 2);
                BOOST_CHECK(it->GetType() == eRR_Data);
                BOOST_CHECK(it->GetOriginalData() == string("31\t"));
                BOOST_CHECK(it[0].Get<int>() == 31);
                BOOST_CHECK(it[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(it->GetNumberOfFields() == 4);
                BOOST_CHECK(it->GetType() == eRR_Data);
                BOOST_CHECK(it->GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(it[0].Get<int>() == 41);
                BOOST_CHECK(it[1].GetOriginalData() == string(""));
                BOOST_CHECK(it[2].GetOriginalData() == string(""));
                BOOST_CHECK(it[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_SET_FILE_TWICE)
{
    TTabDelimitedStream     src_stream(kRRDataFileName);
    src_stream.SetDataSource(kRRDataFileName);
    src_stream.SetDataSource(kRRDataFileName);
}


BOOST_AUTO_TEST_CASE(RR_ON_END_STREAM_EVENT)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTestDelimitedStream    src_stream(&data_stream, "");

    auto it = src_stream.begin();
    auto end_it = src_stream.end();
    for (; it != end_it; ++it)
    {}
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 1);
    BOOST_CHECK(src_stream.GetTraits().GetBeginSourceCount() == 1);


    src_stream.SetDataSource(kRRDataFileName);
    int     line_no = 0;
    for (++it; it != end_it; ++it)
    {
        ++line_no;
    }
    BOOST_CHECK(src_stream.GetTraits().GetBeginSourceCount() == 2);
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 2);
    BOOST_CHECK(line_no == 4);
}


BOOST_AUTO_TEST_CASE(RR_VALID_COUNT)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTestDelimitedStream    src_stream(&data_stream, "");

    auto it = src_stream.begin();
    auto end_it = src_stream.end();
    for (; it != end_it; ++it)
    {}
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 1);
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 1);

    src_stream.SetDataSource(kRRDataFileName);
    src_stream.Validate();

    BOOST_CHECK(src_stream.GetTraits().GetBeginSourceCount() == 2);
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 2);
    BOOST_CHECK(src_stream.GetTraits().GetValidCount() == 4);
}


BOOST_AUTO_TEST_CASE(RR_VALID_COUNT2)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);
    TTestDelimitedStream    src_stream(kRRDataFileName);

    src_stream.Validate();
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 1);
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 1);
    BOOST_CHECK(src_stream.GetTraits().GetValidCount() == 4);

    src_stream.SetDataSource(&data_stream, "");
    auto it = src_stream.begin();
    auto end_it = src_stream.end();
    for (; it != end_it; ++it)
    {}

    BOOST_CHECK(src_stream.GetTraits().GetBeginSourceCount() == 2);
    BOOST_CHECK(src_stream.GetTraits().GetEndReachedCount() == 2);
}


BOOST_AUTO_TEST_CASE(RR_COPY_ITERATORS)
{
    string                  data = "11\t12\t13\t14\r\n"
                                   "21\t22\t23\t\t\r\n"
                                   "31\t\r\n"
                                   "41\t\t\t";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");
    auto                    it1 = src_stream.begin();
    auto                    it2(it1);

    int     line_no = 0;
    for (; it1 != src_stream.end(); ++it1) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(it1->GetNumberOfFields() == 4);
                BOOST_CHECK(it1->GetType() == eRR_Data);
                BOOST_CHECK(it1->GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(it1[0].Get<int>() == 11);
                BOOST_CHECK(it1[1].Get<int>() == 12);
                BOOST_CHECK(it1[2].Get<int>() == 13);
                BOOST_CHECK(it1[3].Get<int>() == 14);

                BOOST_CHECK(it1->GetNumberOfFields() == it2->GetNumberOfFields());
                BOOST_CHECK(it1->GetType() == it2->GetType());
                BOOST_CHECK(it1->GetOriginalData() == it2->GetOriginalData());
                BOOST_CHECK(it1[0].Get<int>() == it2[0].Get<int>());
                BOOST_CHECK(it1[1].Get<int>() == it2[1].Get<int>());
                BOOST_CHECK(it1[2].Get<int>() == it2[2].Get<int>());
                BOOST_CHECK(it1[3].Get<int>() == it2[3].Get<int>());
                break;
            case 1:
                BOOST_CHECK(it1->GetNumberOfFields() == 5);
                BOOST_CHECK(it1->GetType() == eRR_Data);
                BOOST_CHECK(it1->GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(it1[0].Get<int>() == 21);
                BOOST_CHECK(it1[1].Get<int>() == 22);
                BOOST_CHECK(it1[2].Get<int>() == 23);
                BOOST_CHECK(it1[3].GetOriginalData() == string(""));
                BOOST_CHECK(it1[4].GetOriginalData() == string(""));

                BOOST_CHECK(it1->GetNumberOfFields() == it2->GetNumberOfFields());
                BOOST_CHECK(it1->GetType() == it2->GetType());
                BOOST_CHECK(it1->GetOriginalData() == it2->GetOriginalData());
                BOOST_CHECK(it1[0].Get<int>() == it2[0].Get<int>());
                BOOST_CHECK(it1[1].Get<int>() == it2[1].Get<int>());
                BOOST_CHECK(it1[2].Get<int>() == it2[2].Get<int>());
                BOOST_CHECK(it1[3].GetOriginalData() == it2[3].GetOriginalData());
                BOOST_CHECK(it1[4].GetOriginalData() == it2[4].GetOriginalData());
                break;
            case 2:
                BOOST_CHECK(it1->GetNumberOfFields() == 2);
                BOOST_CHECK(it1->GetType() == eRR_Data);
                BOOST_CHECK(it1->GetOriginalData() == string("31\t"));
                BOOST_CHECK(it1[0].Get<int>() == 31);
                BOOST_CHECK(it1[1].GetOriginalData() == string(""));

                BOOST_CHECK(it1->GetNumberOfFields() == it2->GetNumberOfFields());
                BOOST_CHECK(it1->GetType() == it2->GetType());
                BOOST_CHECK(it1->GetOriginalData() == it2->GetOriginalData());
                BOOST_CHECK(it1[0].Get<int>() == it2[0].Get<int>());
                BOOST_CHECK(it1[1].GetOriginalData() == it2[1].GetOriginalData());
                break;
            case 3:
                BOOST_CHECK(it1->GetNumberOfFields() == 4);
                BOOST_CHECK(it1->GetType() == eRR_Data);
                BOOST_CHECK(it1->GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(it1[0].Get<int>() == 41);
                BOOST_CHECK(it1[1].GetOriginalData() == string(""));
                BOOST_CHECK(it1[2].GetOriginalData() == string(""));
                BOOST_CHECK(it1[3].GetOriginalData() == string(""));

                BOOST_CHECK(it1->GetNumberOfFields() == it2->GetNumberOfFields());
                BOOST_CHECK(it1->GetType() == it2->GetType());
                BOOST_CHECK(it1->GetOriginalData() == it2->GetOriginalData());
                BOOST_CHECK(it1[0].Get<int>() == it2[0].Get<int>());
                BOOST_CHECK(it1[1].GetOriginalData() == it2[1].GetOriginalData());
                BOOST_CHECK(it1[2].GetOriginalData() == it2[2].GetOriginalData());
                BOOST_CHECK(it1[3].GetOriginalData() == it2[3].GetOriginalData());
                break;
        }
        ++line_no;
    }

    BOOST_CHECK(it2 == src_stream.end());
}



BOOST_AUTO_TEST_CASE(RR_MULTI_SPACE_DATA_STREAM)
{
    typedef CRowReader<TRowReaderStream_MultiSpaceDelimited>    TMultiSpaceDelimitedStream;

    string                      data = "11\t12 13\v14\r\n"
                                       "21\t22 23\v\t\r\n"
                                       "31\t\r\n"
                                       "41\t \v";
    CNcbiIstrstream             data_stream(data);
    TMultiSpaceDelimitedStream  src_stream(&data_stream, "");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11\t12 13\v14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21\t22 23\v\t"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31\t"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("41\t \v"));
                BOOST_CHECK(row[0].Get<int>() == 41);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_THREE_STAR_DATA_STREAM)
{
    typedef CRowReader<TRowReaderStream_ThreeStarDelimited>    TThreeStarDelimitedStream;

    string                      data = "11***12***13***14\r\n"
                                       "21***22***23***\r\n"
                                       "31***";
    CNcbiIstrstream             data_stream(data);
    TThreeStarDelimitedStream   src_stream(&data_stream, "");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11***12***13***14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21***22***23***"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31***"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_ROW_COPY_CONSTRUCTOR)
{
    string                      data = "1\tone\t111\r\n"
                                       "2\ttwo\t222";
    CNcbiIstrstream             data_stream(data);
    TTabDelimitedStream         src_stream(&data_stream, "");

    vector<TTabDelimitedStream::CRow>   copies;

    int     line_no = 0;
    for (const auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                src_stream.SetFieldName(0, "index1");
                src_stream.SetFieldName(1, "strval1");
                src_stream.SetFieldName(2, "intval1");
                copies.push_back(row);
                break;
            case 1:
                src_stream.ClearFieldsInfo();
                src_stream.SetFieldName(0, "index2");
                src_stream.SetFieldName(1, "strval2");
                src_stream.SetFieldName(2, "intval2");
                copies.push_back(TTabDelimitedStream::CRow(row));
                break;
            }
        ++line_no;
    }

    TTabDelimitedStream::CRow&  row1 = copies[0];
    BOOST_CHECK(row1.GetType() == eRR_Data);
    BOOST_CHECK(row1.GetOriginalData() == string("1\tone\t111"));
    BOOST_CHECK(row1["index1"].Get<int>() == 1);
    BOOST_CHECK(row1["index1"].Get<string>() == string("1"));
    BOOST_CHECK(row1["strval1"].Get<string>() == string("one"));
    BOOST_CHECK(row1["strval1"].GetOriginalData() == string("one"));
    BOOST_CHECK(row1["intval1"].Get<int>() == 111);
    BOOST_CHECK(row1["intval1"].Get<string>() == string("111"));
    BOOST_CHECK(row1["intval1"].GetOriginalData() == string("111"));

    TTabDelimitedStream::CRow&  row2 = copies[1];
    BOOST_CHECK(row2.GetType() == eRR_Data);
    BOOST_CHECK(row2.GetOriginalData() == string("2\ttwo\t222"));
    BOOST_CHECK(row2["index2"].Get<int>() == 2);
    BOOST_CHECK(row2["index2"].Get<string>() == string("2"));
    BOOST_CHECK(row2["strval2"].Get<string>() == string("two"));
    BOOST_CHECK(row2["strval2"].GetOriginalData() == string("two"));
    BOOST_CHECK(row2["intval2"].Get<int>() == 222);
    BOOST_CHECK(row2["intval2"].Get<string>() == string("222"));
    BOOST_CHECK(row2["intval2"].GetOriginalData() == string("222"));
}


BOOST_AUTO_TEST_CASE(RR_FIELD_COPY_CONSTRUCTOR)
{
    string                      data = "1\tone\t111";
    CNcbiIstrstream             data_stream(data);
    TTabDelimitedStream         src_stream(&data_stream, "");

    TTabDelimitedStream::CRow   row1;

    for (const auto &  row : src_stream) {
        src_stream.SetFieldName(0, "index1");
        src_stream.SetFieldName(1, "strval1");
        src_stream.SetFieldName(2, "intval1");
        row1 = row;
    }

    BOOST_CHECK(row1.GetType() == eRR_Data);
    BOOST_CHECK(row1.GetOriginalData() == string("1\tone\t111"));
    BOOST_CHECK(row1["index1"].Get<int>() == 1);
    BOOST_CHECK(row1["index1"].Get<string>() == string("1"));
    BOOST_CHECK(row1["strval1"].Get<string>() == string("one"));
    BOOST_CHECK(row1["strval1"].GetOriginalData() == string("one"));
    BOOST_CHECK(row1["intval1"].Get<int>() == 111);
    BOOST_CHECK(row1["intval1"].Get<string>() == string("111"));
    BOOST_CHECK(row1["intval1"].GetOriginalData() == string("111"));

    vector<TTabDelimitedStream::CField>     fields;
    fields.push_back(row1[0]);
    fields.push_back(row1["strval1"]);
    fields.push_back(row1[2]);

    TTabDelimitedStream::CField     field1 = fields[0];
    TTabDelimitedStream::CField     field2 = fields[1];
    TTabDelimitedStream::CField     field3 = fields[2];

    BOOST_CHECK(field1.Get<int>() == 1);
    BOOST_CHECK(field1.Get<string>() == string("1"));
    BOOST_CHECK(field2.Get<string>() == string("one"));
    BOOST_CHECK(field2.GetOriginalData() == string("one"));
    BOOST_CHECK(field3.Get<int>() == 111);
    BOOST_CHECK(field3.Get<string>() == string("111"));
    BOOST_CHECK(field3.GetOriginalData() == string("111"));
}


class CBeginExceptionTestStreamTraits : public CRowReaderStream_Base
{
public:
    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode /*event_mode*/)
    {
        switch (event) {
            case eRR_Event_SourceBegin:
                throw std::runtime_error("ON_BEGIN_EXC");
            default:
                break;
        }
        return eRR_EventAction_Default;
    }

private:
    RR_TRAITS_PARENT_STREAM(CBeginExceptionTestStreamTraits);
};
typedef CRowReader<CBeginExceptionTestStreamTraits>  TBeginExceptionTestDelimitedStream;



BOOST_AUTO_TEST_CASE(RR_ON_BEGIN_EXCEPTION_STREAM_EVENT)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream                     data_stream(data);
    TBeginExceptionTestDelimitedStream  src_stream(&data_stream, "");

    try {
        src_stream.begin();
        BOOST_FAIL("Exception 'onBegin' is expected");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("ON_BEGIN_EXC") == string::npos)
            BOOST_FAIL("Expected on begin exception");
    }

    try {
        src_stream.begin();
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("Advancing end iterator is prohibited") == string::npos)
            BOOST_FAIL("Expected advancing exception");
    }

    BOOST_CHECK(src_stream.GetCurrentLineNo() == 0);
}


class CEndExceptionTestStreamTraits : public CRowReaderStream_Base
{
public:
    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode /*event_mode*/)
    {
        switch (event) {
            case eRR_Event_SourceEnd:
                throw std::runtime_error("ON_END_EXC");
            default:
                break;
        }
        return eRR_EventAction_Default;
    }

private:
    RR_TRAITS_PARENT_STREAM(CEndExceptionTestStreamTraits);
};
typedef CRowReader<CEndExceptionTestStreamTraits>  TEndExceptionTestDelimitedStream;


BOOST_AUTO_TEST_CASE(RR_ON_END_EXCEPTION_STREAM_EVENT)
{
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream                     data_stream(data);
    TEndExceptionTestDelimitedStream    src_stream(&data_stream, "");

    try {
        auto it = src_stream.begin();
        auto end_it = src_stream.end();
        for (; it != end_it; ++it)
        {}
        BOOST_FAIL("Exception 'onEnd' is expected");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("ON_END_EXC") == string::npos)
            BOOST_FAIL("Expected on end exception");
    }

    try {
        src_stream.begin();
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("Advancing end iterator is prohibited") == string::npos)
            BOOST_FAIL("Expected advancing exception");
    }

    BOOST_CHECK(src_stream.GetCurrentLineNo() == 0);
}


BOOST_AUTO_TEST_CASE(RR_DEFAULT_CONSTRUCT_AND_VALIDATE_OK)
{
    TTabDelimitedStream     src_stream;
    string                  data = "1\tone\t111\r\n"
                                   "2\ttwo\t222\n"
                                   "3\tthree\t333";
    CNcbiIstrstream         data_stream(data);

    src_stream.SetDataSource(&data_stream, "");
    src_stream.Validate();

    BOOST_CHECK(src_stream.GetCurrentRowPos() == 0);
    BOOST_CHECK(src_stream.GetCurrentLineNo() == 0);
}


BOOST_AUTO_TEST_CASE(RR_DEFAULT_CONSTRUCT_AND_VALIDATE_FAILURE)
{
    TTabDelimitedStream     src_stream;

    try {
        src_stream.Validate();
        BOOST_FAIL("Expected Validate() exception");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("stream has not been provided") == string::npos)
            BOOST_FAIL("Expected 'Invalid data source' exception");
    }
}


BOOST_AUTO_TEST_CASE(RR_DEFAULT_CONSTRUCT_AND_ITERATE_OK)
{
    TTabDelimitedStream     src_stream;
    string                  data = "11\t12\t13\t14\r\n"
                                   "21\t22\t23\t\t\r\n"
                                   "31\t\r\n"
                                   "41\t\t\t";
    CNcbiIstrstream         data_stream(data);

    src_stream.SetDataSource(&data_stream, "data");

    int     line_no = 0;
    for (auto &  row : src_stream) {
        switch (line_no) {
            case 0:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("11\t12\t13\t14"));
                BOOST_CHECK(row[0].Get<int>() == 11);
                BOOST_CHECK(row[1].Get<int>() == 12);
                BOOST_CHECK(row[2].Get<int>() == 13);
                BOOST_CHECK(row[3].Get<int>() == 14);
                break;
            case 1:
                BOOST_CHECK(row.GetNumberOfFields() == 5);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("21\t22\t23\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 21);
                BOOST_CHECK(row[1].Get<int>() == 22);
                BOOST_CHECK(row[2].Get<int>() == 23);
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                BOOST_CHECK(row[4].GetOriginalData() == string(""));
                break;
            case 2:
                BOOST_CHECK(row.GetNumberOfFields() == 2);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("31\t"));
                BOOST_CHECK(row[0].Get<int>() == 31);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                break;
            case 3:
                BOOST_CHECK(row.GetNumberOfFields() == 4);
                BOOST_CHECK(row.GetType() == eRR_Data);
                BOOST_CHECK(row.GetOriginalData() == string("41\t\t\t"));
                BOOST_CHECK(row[0].Get<int>() == 41);
                BOOST_CHECK(row[1].GetOriginalData() == string(""));
                BOOST_CHECK(row[2].GetOriginalData() == string(""));
                BOOST_CHECK(row[3].GetOriginalData() == string(""));
                break;
        }
        ++line_no;
    }
}


BOOST_AUTO_TEST_CASE(RR_DEFAULT_CONSTRUCT_AND_ITERATE_FAILURE)
{
    TTabDelimitedStream     src_stream;

    try {
        for (auto &  row : src_stream) {
            if (row.GetNumberOfFields() >= 1000000)
                std::cout << "Should never see" << std::endl;
        }
        BOOST_FAIL("Expected iterate exception");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("stream has not been provided") == string::npos)
            BOOST_FAIL("Expected 'Invalid data source' exception");
    }
}


BOOST_AUTO_TEST_CASE(RR_SET_NULL_DATA_SOURCE)
{
    string                  data = "";
    CNcbiIstrstream         data_stream(data);
    TTabDelimitedStream     src_stream(&data_stream, "");

    try {
        src_stream.SetDataSource(nullptr, "");
        BOOST_FAIL("Expected SetDataSource() exception");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("nvalid data source") == string::npos)
            BOOST_FAIL("Expected 'Invalid data source' exception");
    }
}


BOOST_AUTO_TEST_CASE(RR_CONSTRUCT_NULL_DATA_SOURCE)
{
    try {
        TTabDelimitedStream     src_stream(nullptr, "");
        BOOST_FAIL("Expected construction exception");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        if (what.find("nvalid data source") == string::npos)
            BOOST_FAIL("Expected 'Invalid data source' exception");
    }
}




BOOST_AUTO_TEST_SUITE_END()
END_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// main entry point for tests

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("UTIL Delimited Row Stream");
}
