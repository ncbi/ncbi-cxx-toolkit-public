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
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include "dbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDB_Object)
{
    try {
        // Check for NULL a default constructor ...
        {
            CDB_Bit value_Bit;
            CDB_Int value_Int;
            CDB_SmallInt value_SmallInt;
            CDB_TinyInt value_TinyInt;
            CDB_BigInt value_BigInt;
            CDB_VarChar value_VarChar;
            CDB_Char value_Char;
            CDB_LongChar value_LongChar;
            CDB_VarBinary value_VarBinary;
            CDB_Binary value_Binary(3);
            CDB_LongBinary value_LongBinary;
            CDB_Float value_Float;
            CDB_Double value_Double;
            CDB_SmallDateTime value_SmallDateTime;
            CDB_DateTime value_DateTime;
            CDB_Numeric value_Numeric;

            BOOST_CHECK(value_Bit.IsNULL());
            BOOST_CHECK(value_Int.IsNULL());
            BOOST_CHECK(value_SmallInt.IsNULL());
            BOOST_CHECK(value_TinyInt.IsNULL());
            BOOST_CHECK(value_BigInt.IsNULL());
            BOOST_CHECK(value_VarChar.IsNULL());
            BOOST_CHECK(value_Char.IsNULL());
            BOOST_CHECK(value_LongChar.IsNULL());
            BOOST_CHECK(value_VarBinary.IsNULL());
            BOOST_CHECK(value_Binary.IsNULL());
            BOOST_CHECK(value_LongBinary.IsNULL());
            BOOST_CHECK(value_Float.IsNULL());
            BOOST_CHECK(value_Double.IsNULL());
            BOOST_CHECK(value_SmallDateTime.IsNULL());
            BOOST_CHECK(value_DateTime.IsNULL());
            BOOST_CHECK(value_Numeric.IsNULL());

            // Value() ...
            BOOST_CHECK_EQUAL(value_Bit.Value(), 0);
            BOOST_CHECK_EQUAL(value_Int.Value(), 0);
            BOOST_CHECK_EQUAL(value_SmallInt.Value(), 0);
            BOOST_CHECK_EQUAL(value_TinyInt.Value(), 0);
            BOOST_CHECK_EQUAL(value_BigInt.Value(), 0);
            BOOST_CHECK(value_VarChar.Data() == NULL);
            BOOST_CHECK(value_Char.Data() == NULL);
            BOOST_CHECK(value_LongChar.Data() == NULL);
            BOOST_CHECK(value_VarBinary.Value() == NULL);
            BOOST_CHECK(value_Binary.Value() == NULL);
            BOOST_CHECK(value_LongBinary.Value() == NULL);
            BOOST_CHECK_EQUAL(value_Float.Value(), 0.0);
            BOOST_CHECK_EQUAL(value_Double.Value(), 0.0);
            BOOST_CHECK_EQUAL(value_SmallDateTime.Value(), CTime());
            BOOST_CHECK_EQUAL(value_DateTime.Value(), CTime());
            BOOST_CHECK_EQUAL(value_Numeric.Value(), string("0"));
        }

        // Check for NOT NULL a non-default constructor ...
        {
            CDB_Bit value_Bit(true);
            CDB_Int value_Int(1);
            CDB_SmallInt value_SmallInt(1);
            CDB_TinyInt value_TinyInt(1);
            CDB_BigInt value_BigInt(1);
            CDB_VarChar value_VarChar("ABC");
            CDB_VarChar value_VarChar2("");
            CDB_VarChar value_VarChar3(NULL);
            CDB_VarChar value_VarChar4(kEmptyStr);
            CDB_Char value_Char(3, "ABC");
            CDB_Char value_Char2(3, "");
            CDB_Char value_Char3(3, NULL);
            CDB_Char value_Char4(3, kEmptyStr);
            CDB_LongChar value_LongChar(3, "ABC");
            CDB_LongChar value_LongChar2(3, "");
            CDB_LongChar value_LongChar3(3, NULL);
            CDB_LongChar value_LongChar4(3, kEmptyStr);
            CDB_VarBinary value_VarBinary("ABC", 3);
            CDB_Binary value_Binary(3, "ABC", 3);
            CDB_LongBinary value_LongBinary(3, "ABC", 3);
            CDB_Float value_Float(1.0);
            CDB_Double value_Double(1.0);
            CDB_SmallDateTime value_SmallDateTime(CTime("04/24/2007 11:17:01"));
            CDB_DateTime value_DateTime(CTime("04/24/2007 11:17:01"));
            CDB_Numeric value_Numeric(10, 2, "10");

            BOOST_CHECK(!value_Bit.IsNULL());
            BOOST_CHECK(!value_Int.IsNULL());
            BOOST_CHECK(!value_SmallInt.IsNULL());
            BOOST_CHECK(!value_TinyInt.IsNULL());
            BOOST_CHECK(!value_BigInt.IsNULL());
            BOOST_CHECK(!value_VarChar.IsNULL());
            BOOST_CHECK(!value_VarChar2.IsNULL());
            // !!!
            BOOST_CHECK(value_VarChar3.IsNULL());
            BOOST_CHECK(!value_VarChar4.IsNULL());
            BOOST_CHECK(!value_Char.IsNULL());
            BOOST_CHECK(!value_Char2.IsNULL());
            // !!!
            BOOST_CHECK(value_Char3.IsNULL());
            BOOST_CHECK(!value_Char4.IsNULL());
            BOOST_CHECK(!value_LongChar.IsNULL());
            BOOST_CHECK(!value_LongChar2.IsNULL());
            // !!!
            BOOST_CHECK(value_LongChar3.IsNULL());
            BOOST_CHECK(!value_LongChar4.IsNULL());
            BOOST_CHECK(!value_VarBinary.IsNULL());
            BOOST_CHECK(!value_Binary.IsNULL());
            BOOST_CHECK(!value_LongBinary.IsNULL());
            BOOST_CHECK(!value_Float.IsNULL());
            BOOST_CHECK(!value_Double.IsNULL());
            BOOST_CHECK(!value_SmallDateTime.IsNULL());
            BOOST_CHECK(!value_DateTime.IsNULL());
            BOOST_CHECK(!value_Numeric.IsNULL());

            // Value() ...
            BOOST_CHECK(value_Bit.Value() != 0);
            BOOST_CHECK(value_Int.Value() != 0);
            BOOST_CHECK(value_SmallInt.Value() != 0);
            BOOST_CHECK(value_TinyInt.Value() != 0);
            BOOST_CHECK(value_BigInt.Value() != 0);
            BOOST_CHECK(value_VarChar.Data() != NULL);
            BOOST_CHECK(value_VarChar2.Data() != NULL);
            // !!!
            BOOST_CHECK(value_VarChar3.Data() == NULL);
            BOOST_CHECK(value_VarChar4.Data() != NULL);
            BOOST_CHECK(value_Char.Data() != NULL);
            BOOST_CHECK(value_Char2.Data() != NULL);
            // !!!
            BOOST_CHECK(value_Char3.Data() == NULL);
            BOOST_CHECK(value_Char4.Data() != NULL);
            BOOST_CHECK(value_LongChar.Data() != NULL);
            BOOST_CHECK(value_LongChar2.Data() != NULL);
            // !!!
            BOOST_CHECK(value_LongChar3.Data() == NULL);
            BOOST_CHECK(value_LongChar4.Data() != NULL);
            BOOST_CHECK(value_VarBinary.Value() != NULL);
            BOOST_CHECK(value_Binary.Value() != NULL);
            BOOST_CHECK(value_LongBinary.Value() != NULL);
            BOOST_CHECK(value_Float.Value() != 0);
            BOOST_CHECK(value_Double.Value() != 0);
            BOOST_CHECK(value_SmallDateTime.Value() != CTime());
            BOOST_CHECK(value_DateTime.Value() != CTime());
            BOOST_CHECK(value_Numeric.Value() != string("0"));


            // Check actual values ...
            BOOST_CHECK(value_Bit.Value() == 1);
            BOOST_CHECK_EQUAL(value_Int.Value(), 1);
            BOOST_CHECK_EQUAL(value_SmallInt.Value(), 1);
            BOOST_CHECK_EQUAL(value_TinyInt.Value(), 1);
            BOOST_CHECK_EQUAL(value_BigInt.Value(), 1);
            BOOST_CHECK_EQUAL(value_VarChar.AsString(), string("ABC"));
            BOOST_CHECK_EQUAL(value_Char.AsString(), string("ABC"));
            BOOST_CHECK_EQUAL(value_LongChar.AsString(), string("ABC"));
            BOOST_CHECK_EQUAL(strncmp((const char*)value_VarBinary.Value(), "ABC", 3), 0);
            BOOST_CHECK_EQUAL(strncmp((const char*)value_Binary.Value(), "ABC", 3), 0);
            BOOST_CHECK_EQUAL(strncmp((const char*)value_LongBinary.Value(), "ABC", 3), 0);
            BOOST_CHECK_EQUAL(value_Float.Value(), 1.0);
            BOOST_CHECK_EQUAL(value_Double.Value(), 1.0);
            BOOST_CHECK_EQUAL(value_SmallDateTime.Value(), CTime("04/24/2007 11:17:01"));
            BOOST_CHECK_EQUAL(value_DateTime.Value(), CTime("04/24/2007 11:17:01"));
            // value_Numeric(10, 2, "10");
        }

        // Check for NOT NULL after a value assignment operator ...
        {
            CDB_Bit value_Bit;
            CDB_Int value_Int;
            CDB_SmallInt value_SmallInt;
            CDB_TinyInt value_TinyInt;
            CDB_BigInt value_BigInt;
            CDB_VarChar value_VarChar;
            CDB_Char value_Char(10);
            CDB_LongChar value_LongChar;
            CDB_VarBinary value_VarBinary;
            CDB_Binary value_Binary(3);
            CDB_LongBinary value_LongBinary;
            CDB_Float value_Float;
            CDB_Double value_Double;
            CDB_SmallDateTime value_SmallDateTime;
            CDB_DateTime value_DateTime;
            CDB_Numeric value_Numeric;

            value_Bit = true;
            value_Int = 1;
            value_SmallInt = 1;
            value_TinyInt = 1;
            value_BigInt = 1;
            value_VarChar = "ABC";
            value_Char = "ABC";
            value_LongChar = "ABC";
            value_VarBinary.SetValue("ABC", 3);
            value_Binary.SetValue("ABC", 3);
            value_LongBinary.SetValue("ABC", 3);
            value_Float = 1.0;
            value_Double = 1.0;
            value_SmallDateTime = CTime("04/24/2007 11:17:01");
            value_DateTime = CTime("04/24/2007 11:17:01");
            value_Numeric = "10";

            BOOST_CHECK(!value_Bit.IsNULL());
            BOOST_CHECK(!value_Int.IsNULL());
            BOOST_CHECK(!value_SmallInt.IsNULL());
            BOOST_CHECK(!value_TinyInt.IsNULL());
            BOOST_CHECK(!value_BigInt.IsNULL());
            BOOST_CHECK(!value_VarChar.IsNULL());
            BOOST_CHECK(!value_Char.IsNULL());
            BOOST_CHECK(!value_LongChar.IsNULL());
            BOOST_CHECK(!value_VarBinary.IsNULL());
            BOOST_CHECK(!value_Binary.IsNULL());
            BOOST_CHECK(!value_LongBinary.IsNULL());
            BOOST_CHECK(!value_Float.IsNULL());
            BOOST_CHECK(!value_Double.IsNULL());
            BOOST_CHECK(!value_SmallDateTime.IsNULL());
            BOOST_CHECK(!value_DateTime.IsNULL());
            BOOST_CHECK(!value_Numeric.IsNULL());

            // A copy constructor ...
            CDB_Bit value_Bit2(value_Bit);
            CDB_Int value_Int2(value_Int);
            CDB_SmallInt value_SmallInt2(value_SmallInt);
            CDB_TinyInt value_TinyInt2(value_TinyInt);
            CDB_BigInt value_BigInt2(value_BigInt);
            CDB_VarChar value_VarChar2(value_VarChar);
            CDB_Char value_Char2(value_Char);
            CDB_LongChar value_LongChar2(value_LongChar);
            CDB_VarBinary value_VarBinary2(value_VarBinary);
            CDB_Binary value_Binary2(value_Binary);
            CDB_LongBinary value_LongBinary2(value_LongBinary);
            CDB_Float value_Float2(value_Float);
            CDB_Double value_Double2(value_Double);
            CDB_SmallDateTime value_SmallDateTime2(value_SmallDateTime);
            CDB_DateTime value_DateTime2(value_DateTime);
            CDB_Numeric value_Numeric2(value_Numeric);

            BOOST_CHECK(!value_Bit2.IsNULL());
            BOOST_CHECK(!value_Int2.IsNULL());
            BOOST_CHECK(!value_SmallInt2.IsNULL());
            BOOST_CHECK(!value_TinyInt2.IsNULL());
            BOOST_CHECK(!value_BigInt2.IsNULL());
            BOOST_CHECK(!value_VarChar2.IsNULL());
            BOOST_CHECK(!value_Char2.IsNULL());
            BOOST_CHECK(!value_LongChar2.IsNULL());
            BOOST_CHECK(!value_VarBinary2.IsNULL());
            BOOST_CHECK(!value_Binary2.IsNULL());
            BOOST_CHECK(!value_LongBinary2.IsNULL());
            BOOST_CHECK(!value_Float2.IsNULL());
            BOOST_CHECK(!value_Double2.IsNULL());
            BOOST_CHECK(!value_SmallDateTime2.IsNULL());
            BOOST_CHECK(!value_DateTime2.IsNULL());
            BOOST_CHECK(!value_Numeric2.IsNULL());

        }

        // Check that numeric stores values correctly
        {
            CDB_Numeric value_Numeric(10, 0, "2571");

            BOOST_CHECK_EQUAL(value_Numeric.Value(), string("2571"));
            value_Numeric = "25856";
            BOOST_CHECK_EQUAL(value_Numeric.Value(), string("25856"));
            value_Numeric = "2585856";
            BOOST_CHECK_EQUAL(value_Numeric.Value(), string("2585856"));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDB_Object2)
{
    try {
        string sql;

        // Int8   Int8_value = 0;
        // Uint8  Uint8_value = 0;
        Int4   Int4_value = 0;
        // Uint4  Uint4_value = 0;
        Int2   Int2_value = 0;
        // Uint2  Uint2_value = 0;
        // Int1   Int1_value = 0;
        Uint1  Uint1_value = 0;
        float  float_value = 0.0;
        double double_value = 0.0;
        bool   bool_value = false;

        // First test ...
        {


            // bit
            {
                sql = "select Convert(bit, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Bit db_obj;

                        rs->GetItem(&db_obj);
                        bool_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(bool_value, true);
                    }
                }
            }

            // tinyint
            {
                sql = "select Convert(tinyint, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_TinyInt db_obj;

                        rs->GetItem(&db_obj);
                        Uint1_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(Uint1_value, 1);
                    }
                }
            }

            // smallint
            {
                sql = "select Convert(smallint, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_SmallInt db_obj;

                        rs->GetItem(&db_obj);
                        Int2_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(Int2_value, 1);
                    }
                }
            }

            // int
            {
                sql = "select Convert(int, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Int db_obj;

                        rs->GetItem(&db_obj);
                        Int4_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(Int4_value, 1);
                    }
                }
            }

            // numeric
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                //
                {
                    sql = "select Convert(numeric(38, 0), 1)";

                    auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                    BOOST_CHECK( auto_stmt.get() != NULL );
                    bool rc = auto_stmt->Send();
                    BOOST_CHECK( rc );

                    while (auto_stmt->HasMoreResults()) {
                        auto_ptr<CDB_Result> rs(auto_stmt->Result());

                        if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                            continue;
                        }

                        while (rs->Fetch()) {
                            CDB_Numeric db_obj;

                            rs->GetItem(&db_obj);
                            string string_value = db_obj.Value();

                            BOOST_CHECK_EQUAL(string_value, string("1"));
                        }
                    }
                }

                //
                {
                    sql = "select Convert(numeric(18, 2), 2843113322)";

                    auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                    BOOST_CHECK( auto_stmt.get() != NULL );
                    bool rc = auto_stmt->Send();
                    BOOST_CHECK( rc );

                    while (auto_stmt->HasMoreResults()) {
                        auto_ptr<CDB_Result> rs(auto_stmt->Result());

                        if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                            continue;
                        }

                        while (rs->Fetch()) {
                            if (GetArgs().GetDriverName() == odbc_driver) {
                                CDB_Numeric db_obj;

                                rs->GetItem(&db_obj);
                                string string_value = db_obj.Value();

                                BOOST_CHECK_EQUAL(string_value, string("2843113322"));
                            } else {
                                CDB_Numeric db_obj;

                                rs->GetItem(&db_obj);
                                string string_value = db_obj.Value();

                                BOOST_CHECK_EQUAL(string_value, string("2843113322.00"));
                            }
                        }
                    }
                }
            }

            // decimal
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                sql = "select Convert(decimal(38, 0), 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Numeric db_obj;

                        rs->GetItem(&db_obj);
                        string string_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(string_value, string("1"));
                    }
                }
            }

            // float
            {
                sql = "select Convert(float(4), 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Float db_obj;

                        rs->GetItem(&db_obj);
                        float_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(float_value, 1);
                    }
                }
            }

            // double
            {
                sql = "select Convert(double precision, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Double db_obj;

                        rs->GetItem(&db_obj);
                        double_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(double_value, 1);
                    }
                }
            }

            // real
            {
                sql = "select Convert(real, 1)";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Float db_obj;

                        rs->GetItem(&db_obj);
                        float_value = db_obj.Value();

                        BOOST_CHECK_EQUAL(float_value, 1);
                    }
                }
            }

            // smalldatetime
            /*
               {
               sql = "select Convert(smalldatetime, 'January 1, 1900')";

               auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
               BOOST_CHECK( auto_stmt.get() != NULL );
               bool rc = auto_stmt->Send();
               BOOST_CHECK( rc );

               while (auto_stmt->HasMoreResults()) {
               auto_ptr<CDB_Result> rs(auto_stmt->Result());

               if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
               continue;
               }

               while (rs->Fetch()) {
               const CTime& CTime_value = Convert(*rs);
               BOOST_CHECK_EQUAL(CTime_value, CTime("January 1, 1900"));
               }
               }
               }

            // datetime
            {
            sql = "select Convert(datetime, 'January 1, 1753')";

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK( auto_stmt.get() != NULL );
            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );

            while (auto_stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(auto_stmt->Result());

            if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
            continue;
            }

            while (rs->Fetch()) {
            const CTime& CTime_value = Convert(*rs);
            BOOST_CHECK_EQUAL(CTime_value, CTime("January 1, 1753"));
            }
            }
            }
            */

            // char
            {
                sql = "select Convert(char(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Char db_obj(32);

                        rs->GetItem(&db_obj);
                        string string_value = db_obj.AsString();

                        BOOST_CHECK_EQUAL(NStr::TruncateSpaces(string_value), string("12345"));
                    }
                }
            }

            // varchar
            {
                sql = "select Convert(varchar(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_VarChar db_obj;

                        rs->GetItem(&db_obj);
                        string string_value = db_obj.AsString();

                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }

            // nchar
            {
                sql = "select Convert(nchar(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Char db_obj(32);

                        rs->GetItem(&db_obj);
                        string string_value = db_obj.AsString();

                        BOOST_CHECK_EQUAL(NStr::TruncateSpaces(string_value), string("12345"));
                    }
                }
            }

            // nvarchar
            {
                sql = "select Convert(nvarchar(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_VarChar db_obj;

                        rs->GetItem(&db_obj);
                        string string_value = db_obj.AsString();

                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }

            // binary
            {
                sql = "select Convert(binary(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Binary db_obj(32);

                        rs->GetItem(&db_obj);
                        string string_value = string(static_cast<const char*>(db_obj.Value()), db_obj.Size());

                        BOOST_CHECK_EQUAL(string_value.substr(0, 5), string("12345"));
                    }
                }
            }

            // varbinary
            {
                sql = "select Convert(varbinary(32), '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_VarBinary db_obj;

                        rs->GetItem(&db_obj);
                        string string_value = string(static_cast<const char*>(db_obj.Value()), db_obj.Size());

                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }

            // text
            {
                sql = "select Convert(text, '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Text db_obj;

                        rs->GetItem(&db_obj);

                        string string_value;
                        string_value.resize(db_obj.Size());
                        db_obj.Read(const_cast<void*>(static_cast<const void*>(string_value.data())),
                                db_obj.Size()
                                );

                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }

            // image
            {
                sql = "select Convert(image, '12345')";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));
                BOOST_CHECK( auto_stmt.get() != NULL );
                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while (auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL || rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    while (rs->Fetch()) {
                        CDB_Image db_obj;

                        rs->GetItem(&db_obj);

                        string string_value;
                        string_value.resize(db_obj.Size());
                        db_obj.Read(const_cast<void*>(static_cast<const void*>(string_value.data())),
                                db_obj.Size()
                                );

                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Variant)
{
    const Uint1 value_Uint1 = 1;
    const Int2 value_Int2 = -2;
    const Int4 value_Int4 = -4;
    const Int8 value_Int8 = -8;
    const float value_float = float(0.4);
    const double value_double = 0.8;
    const bool value_bool = false;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);

    try {
        // Check constructors
        {
            const CVariant variant_Int8( value_Int8 );
            BOOST_CHECK( !variant_Int8.IsNull() );
            BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

            const CVariant variant_Int4( value_Int4 );
            BOOST_CHECK( !variant_Int4.IsNull() );
            BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

            const CVariant variant_Int2( value_Int2 );
            BOOST_CHECK( !variant_Int2.IsNull() );
            BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

            const CVariant variant_Uint1( value_Uint1 );
            BOOST_CHECK( !variant_Uint1.IsNull() );
            BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

            const CVariant variant_float( value_float );
            BOOST_CHECK( !variant_float.IsNull() );
            BOOST_CHECK( variant_float.GetFloat() == value_float );

            const CVariant variant_double( value_double );
            BOOST_CHECK( !variant_double.IsNull() );
            BOOST_CHECK( variant_double.GetDouble() == value_double );

            const CVariant variant_bool( value_bool );
            BOOST_CHECK( !variant_bool.IsNull() );
            BOOST_CHECK( variant_bool.GetBit() == value_bool );

            const CVariant variant_string( value_string );
            BOOST_CHECK( !variant_string.IsNull() );
            BOOST_CHECK( variant_string.GetString() == value_string );

            const CVariant variant_string2( kEmptyStr );
            BOOST_CHECK( !variant_string2.IsNull() );
            BOOST_CHECK( variant_string2.GetString() == kEmptyStr );

            const CVariant variant_char( value_char );
            BOOST_CHECK( !variant_char.IsNull() );
            BOOST_CHECK( variant_char.GetString() == value_char );

            const CVariant variant_char2( (const char*)NULL );
            // !!!
            BOOST_CHECK( variant_char2.IsNull() );
            BOOST_CHECK( variant_char2.GetString() == string() );

            const CVariant variant_CTimeShort( value_CTime, eShort );
            BOOST_CHECK( !variant_CTimeShort.IsNull() );
            BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

            const CVariant variant_CTimeLong( value_CTime, eLong );
            BOOST_CHECK( !variant_CTimeLong.IsNull() );
            BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );

    //        explicit CVariant(CDB_Object* obj);

        }

        // Check the CVariant(EDB_Type type, size_t size = 0) constructor
        {
            {
                CVariant value_variant( eDB_Int );

                BOOST_CHECK_EQUAL( eDB_Int, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_SmallInt );

                BOOST_CHECK_EQUAL( eDB_SmallInt, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_TinyInt );

                BOOST_CHECK_EQUAL( eDB_TinyInt, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_BigInt );

                BOOST_CHECK_EQUAL( eDB_BigInt, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_VarChar );

                BOOST_CHECK_EQUAL( eDB_VarChar, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Char, max_text_size );

                BOOST_CHECK_EQUAL( eDB_Char, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_VarBinary );

                BOOST_CHECK_EQUAL( eDB_VarBinary, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Binary, max_text_size );

                BOOST_CHECK_EQUAL( eDB_Binary, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Float );

                BOOST_CHECK_EQUAL( eDB_Float, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Double );

                BOOST_CHECK_EQUAL( eDB_Double, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_DateTime );

                BOOST_CHECK_EQUAL( eDB_DateTime, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_SmallDateTime );

                BOOST_CHECK_EQUAL( eDB_SmallDateTime, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Text );

                BOOST_CHECK_EQUAL( eDB_Text, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Image );

                BOOST_CHECK_EQUAL( eDB_Image, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Bit );

                BOOST_CHECK_EQUAL( eDB_Bit, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_Numeric );

                BOOST_CHECK_EQUAL( eDB_Numeric, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_LongChar, max_text_size );

                BOOST_CHECK_EQUAL( eDB_LongChar, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            {
                CVariant value_variant( eDB_LongBinary, max_text_size );

                BOOST_CHECK_EQUAL( eDB_LongBinary, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
            if (false) {
                CVariant value_variant( eDB_UnsupportedType );

                BOOST_CHECK_EQUAL( eDB_UnsupportedType, value_variant.GetType() );
                BOOST_CHECK( value_variant.IsNull() );
            }
        }

        // Check the copy-constructor CVariant(const CVariant& v)
        {
            const CVariant variant_Int8 = CVariant(value_Int8);
            BOOST_CHECK( !variant_Int8.IsNull() );
            BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

            const CVariant variant_Int4 = CVariant(value_Int4);
            BOOST_CHECK( !variant_Int4.IsNull() );
            BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

            const CVariant variant_Int2 = CVariant(value_Int2);
            BOOST_CHECK( !variant_Int2.IsNull() );
            BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

            const CVariant variant_Uint1 = CVariant(value_Uint1);
            BOOST_CHECK( !variant_Uint1.IsNull() );
            BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

            const CVariant variant_float = CVariant(value_float);
            BOOST_CHECK( !variant_float.IsNull() );
            BOOST_CHECK( variant_float.GetFloat() == value_float );

            const CVariant variant_double = CVariant(value_double);
            BOOST_CHECK( !variant_double.IsNull() );
            BOOST_CHECK( variant_double.GetDouble() == value_double );

            const CVariant variant_bool = CVariant(value_bool);
            BOOST_CHECK( !variant_bool.IsNull() );
            BOOST_CHECK( variant_bool.GetBit() == value_bool );

            const CVariant variant_string = CVariant(value_string);
            BOOST_CHECK( !variant_string.IsNull() );
            BOOST_CHECK( variant_string.GetString() == value_string );

            const CVariant variant_char = CVariant(value_char);
            BOOST_CHECK( !variant_char.IsNull() );
            BOOST_CHECK( variant_char.GetString() == value_char );

            const CVariant variant_CTimeShort = CVariant( value_CTime, eShort );
            BOOST_CHECK( !variant_CTimeShort.IsNull() );
            BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

            const CVariant variant_CTimeLong = CVariant( value_CTime, eLong );
            BOOST_CHECK( !variant_CTimeLong.IsNull() );
            BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );
        }

        // Call Factories for different types
        {
            const CVariant variant_Int8 = CVariant::BigInt( const_cast<Int8*>(&value_Int8) );
            BOOST_CHECK( !variant_Int8.IsNull() );
            BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

            const CVariant variant_Int4 = CVariant::Int( const_cast<Int4*>(&value_Int4) );
            BOOST_CHECK( !variant_Int4.IsNull() );
            BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

            const CVariant variant_Int2 = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) );
            BOOST_CHECK( !variant_Int2.IsNull() );
            BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

            const CVariant variant_Uint1 = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) );
            BOOST_CHECK( !variant_Uint1.IsNull() );
            BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

            const CVariant variant_float = CVariant::Float( const_cast<float*>(&value_float) );
            BOOST_CHECK( !variant_float.IsNull() );
            BOOST_CHECK( variant_float.GetFloat() == value_float );

            const CVariant variant_double = CVariant::Double( const_cast<double*>(&value_double) );
            BOOST_CHECK( !variant_double.IsNull() );
            BOOST_CHECK( variant_double.GetDouble() == value_double );

            const CVariant variant_bool = CVariant::Bit( const_cast<bool*>(&value_bool) );
            BOOST_CHECK( !variant_bool.IsNull() );
            BOOST_CHECK( variant_bool.GetBit() == value_bool );

            // LongChar
            {
                {
                    const CVariant variant_LongChar = CVariant::LongChar( value_char, strlen(value_char) );
                    BOOST_CHECK( !variant_LongChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_LongChar.GetString(), str_value_char );
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( value_char );
                    BOOST_CHECK( !variant_LongChar.IsNull() );
                    const string value = variant_LongChar.GetString();
                    BOOST_CHECK_EQUAL( value, str_value_char );
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( NULL, 0 );
                    // !!!
                    BOOST_CHECK( variant_LongChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_LongChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( NULL, 10 );
                    // !!!
                    BOOST_CHECK( variant_LongChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_LongChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( NULL );
                    // !!!
                    BOOST_CHECK( variant_LongChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_LongChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( "", 80 );
                    BOOST_CHECK( !variant_LongChar.IsNull() );
                    const string value = variant_LongChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 80U);
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar( "", 0 );
                    BOOST_CHECK( !variant_LongChar.IsNull() );
                    const string value = variant_LongChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 0U);
                }

                {
                    const CVariant variant_LongChar = CVariant::LongChar("");
                    BOOST_CHECK( !variant_LongChar.IsNull() );
                    const string value = variant_LongChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 0U);
                }
            }

            // VarChar
            {
                {
                    const CVariant variant_VarChar = CVariant::VarChar( value_char, strlen(value_char) );
                    BOOST_CHECK( !variant_VarChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_VarChar.GetString(), str_value_char );
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar( value_char );
                    BOOST_CHECK( !variant_VarChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_VarChar.GetString(), str_value_char );
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar(NULL);
                    // !!!
                    BOOST_CHECK( variant_VarChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_VarChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar(NULL, 0);
                    // !!!
                    BOOST_CHECK( variant_VarChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_VarChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar(NULL, 10);
                    // !!!
                    BOOST_CHECK( variant_VarChar.IsNull() );
                    BOOST_CHECK_EQUAL( variant_VarChar.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar( "", 900 );
                    BOOST_CHECK( !variant_VarChar.IsNull() );
                    const string value = variant_VarChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 0U);
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar( "", 0 );
                    BOOST_CHECK( !variant_VarChar.IsNull() );
                    const string value = variant_VarChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 0U);
                }

                {
                    const CVariant variant_VarChar = CVariant::VarChar("");
                    BOOST_CHECK( !variant_VarChar.IsNull() );
                    const string value = variant_VarChar.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 0U);
                }
            }

            // Char
            {
                {
                    const CVariant variant_Char = CVariant::Char( strlen(value_char), const_cast<char*>(value_char) );
                    BOOST_CHECK( !variant_Char.IsNull() );
                    BOOST_CHECK_EQUAL( variant_Char.GetString(), str_value_char );
                }

                {
                    const CVariant variant_Char = CVariant::Char( 0, NULL );
                    // !!!
                    BOOST_CHECK( variant_Char.IsNull() );
                    BOOST_CHECK_EQUAL( variant_Char.GetString(), kEmptyStr );
                }

                {
                    const CVariant variant_Char = CVariant::Char( 90, "" );
                    BOOST_CHECK( !variant_Char.IsNull() );
                    const string value = variant_Char.GetString();
                    BOOST_CHECK_EQUAL(value.size(), 90U);
                }
            }

            const CVariant variant_LongBinary = CVariant::LongBinary( strlen(value_binary), value_binary, strlen(value_binary)) ;
            BOOST_CHECK( !variant_LongBinary.IsNull() );

            const CVariant variant_VarBinary = CVariant::VarBinary( value_binary, strlen(value_binary) );
            BOOST_CHECK( !variant_VarBinary.IsNull() );

            const CVariant variant_Binary = CVariant::Binary( strlen(value_binary), value_binary, strlen(value_binary) );
            BOOST_CHECK( !variant_Binary.IsNull() );

            const CVariant variant_SmallDateTime = CVariant::SmallDateTime( const_cast<CTime*>(&value_CTime) );
            BOOST_CHECK( !variant_SmallDateTime.IsNull() );
            BOOST_CHECK( variant_SmallDateTime.GetCTime() == value_CTime );

            const CVariant variant_DateTime = CVariant::DateTime( const_cast<CTime*>(&value_CTime) );
            BOOST_CHECK( !variant_DateTime.IsNull() );
            BOOST_CHECK( variant_DateTime.GetCTime() == value_CTime );

    //        CVariant variant_Numeric = CVariant::Numeric  (unsigned int precision, unsigned int scale, const char* p);
        }

        // Call Get method for different types
        {
            const Uint1 value_Uint1_tmp = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) ).GetByte();
            BOOST_CHECK_EQUAL( value_Uint1, value_Uint1_tmp );

            const Int2 value_Int2_tmp = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) ).GetInt2();
            BOOST_CHECK_EQUAL( value_Int2, value_Int2_tmp );

            const Int4 value_Int4_tmp = CVariant::Int( const_cast<Int4*>(&value_Int4) ).GetInt4();
            BOOST_CHECK_EQUAL( value_Int4, value_Int4_tmp );

            const Int8 value_Int8_tmp = CVariant::BigInt( const_cast<Int8*>(&value_Int8) ).GetInt8();
            BOOST_CHECK_EQUAL( value_Int8, value_Int8_tmp );

            const float value_float_tmp = CVariant::Float( const_cast<float*>(&value_float) ).GetFloat();
            BOOST_CHECK_EQUAL( value_float, value_float_tmp );

            const double value_double_tmp = CVariant::Double( const_cast<double*>(&value_double) ).GetDouble();
            BOOST_CHECK_EQUAL( value_double, value_double_tmp );

            const bool value_bool_tmp = CVariant::Bit( const_cast<bool*>(&value_bool) ).GetBit();
            BOOST_CHECK_EQUAL( value_bool, value_bool_tmp );

            const CTime value_CTime_tmp = CVariant::DateTime( const_cast<CTime*>(&value_CTime) ).GetCTime();
            BOOST_CHECK( value_CTime == value_CTime_tmp );

            // GetNumeric() ????
        }

        // Call operator= for different types
        //!!! It *fails* !!!
        if (false) {
            CVariant value_variant(0);
            value_variant.SetNull();

            value_variant = CVariant(0);
            BOOST_CHECK( CVariant(0) == value_variant );

            value_variant = value_Int8;
            BOOST_CHECK( CVariant( value_Int8 ) == value_variant );

            value_variant = value_Int4;
            BOOST_CHECK( CVariant( value_Int4 ) == value_variant );

            value_variant = value_Int2;
            BOOST_CHECK( CVariant( value_Int2 ) == value_variant );

            value_variant = value_Uint1;
            BOOST_CHECK( CVariant( value_Uint1 ) == value_variant );

            value_variant = value_float;
            BOOST_CHECK( CVariant( value_float ) == value_variant );

            value_variant = value_double;
            BOOST_CHECK( CVariant( value_double ) == value_variant );

            value_variant = value_string;
            BOOST_CHECK( CVariant( value_string ) == value_variant );

            value_variant = value_char;
            BOOST_CHECK( CVariant( value_char ) == value_variant );

            value_variant = value_bool;
            BOOST_CHECK( CVariant( value_bool ) == value_variant );

            value_variant = value_CTime;
            BOOST_CHECK( CVariant( value_CTime ) == value_variant );

        }

        // Check assigning of values of same type ...
        {
            CVariant variant_Int8( value_Int8 );
            BOOST_CHECK( !variant_Int8.IsNull() );
            variant_Int8 = CVariant( value_Int8 );
            BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );
            variant_Int8 = value_Int8;
            BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

            CVariant variant_Int4( value_Int4 );
            BOOST_CHECK( !variant_Int4.IsNull() );
            variant_Int4 = CVariant( value_Int4 );
            BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );
            variant_Int4 = value_Int4;
            BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

            CVariant variant_Int2( value_Int2 );
            BOOST_CHECK( !variant_Int2.IsNull() );
            variant_Int2 = CVariant( value_Int2 );
            BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );
            variant_Int2 = value_Int2;
            BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

            CVariant variant_Uint1( value_Uint1 );
            BOOST_CHECK( !variant_Uint1.IsNull() );
            variant_Uint1 = CVariant( value_Uint1 );
            BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );
            variant_Uint1 = value_Uint1;
            BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

            CVariant variant_float( value_float );
            BOOST_CHECK( !variant_float.IsNull() );
            variant_float = CVariant( value_float );
            BOOST_CHECK( variant_float.GetFloat() == value_float );
            variant_float = value_float;
            BOOST_CHECK( variant_float.GetFloat() == value_float );

            CVariant variant_double( value_double );
            BOOST_CHECK( !variant_double.IsNull() );
            variant_double = CVariant( value_double );
            BOOST_CHECK( variant_double.GetDouble() == value_double );
            variant_double = value_double;
            BOOST_CHECK( variant_double.GetDouble() == value_double );

            CVariant variant_bool( value_bool );
            BOOST_CHECK( !variant_bool.IsNull() );
            variant_bool = CVariant( value_bool );
            BOOST_CHECK( variant_bool.GetBit() == value_bool );
            variant_bool = value_bool;
            BOOST_CHECK( variant_bool.GetBit() == value_bool );

            CVariant variant_string( value_string );
            BOOST_CHECK( !variant_string.IsNull() );
            variant_string = CVariant( value_string );
            BOOST_CHECK( variant_string.GetString() == value_string );
            variant_string = value_string;
            BOOST_CHECK( variant_string.GetString() == value_string );

            CVariant variant_string2( kEmptyStr );
            BOOST_CHECK( !variant_string2.IsNull() );
            variant_string2 = CVariant( kEmptyStr );
            BOOST_CHECK( variant_string2.GetString() == kEmptyStr );
            variant_string2 = kEmptyStr;
            BOOST_CHECK( variant_string2.GetString() == kEmptyStr );

            CVariant variant_char( value_char );
            BOOST_CHECK( !variant_char.IsNull() );
            variant_char = CVariant( value_char );
            BOOST_CHECK( variant_char.GetString() == value_char );
            variant_char = value_char;
            BOOST_CHECK( variant_char.GetString() == value_char );

            CVariant variant_char2( (const char*) NULL );
            BOOST_CHECK( !variant_char.IsNull() );
            variant_char2 = CVariant( (const char*) NULL );
            BOOST_CHECK( variant_char2.GetString() == string() );
            variant_char2 = (const char*) NULL;
            BOOST_CHECK( variant_char2.GetString() == string() );

            CVariant variant_CTimeShort( value_CTime, eShort );
            BOOST_CHECK( !variant_CTimeShort.IsNull() );
            variant_CTimeShort = CVariant( value_CTime, eShort );
            BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

            CVariant variant_CTimeLong( value_CTime, eLong );
            BOOST_CHECK( !variant_CTimeLong.IsNull() );
            variant_CTimeLong = CVariant( value_CTime, eLong );
            BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );

    //        explicit CVariant(CDB_Object* obj);

        }

        // Call operator= for different Variant types
        {
            // Assign to CVariant(NULL)
            if (false) {
                {
                    CVariant value_variant(0);
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant(0);
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_Uint1 )
            if (false) {
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant(value_Uint1);
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_Int2 )
            if (false) {
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_Int2 );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_Int4 )
            if (false) {
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_Int4 );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_Int8 )
            if (false) {
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_Int8 );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_float )
            if (false) {
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_float );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_double )
            if (false) {
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_double );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_bool )
            if (false) {
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_bool );
                    value_variant = CVariant( value_CTime );
                }
            }

            // Assign to CVariant( value_CTime )
            if (false) {
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant(0);
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_Int8 );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_Int4 );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_Int2 );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_Uint1 );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_float );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_double );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_bool );
                }
                {
                    CVariant value_variant( value_CTime );
                    value_variant = CVariant( value_CTime );
                }
            }
        }

        // Test Null cases ...
        {
            CVariant value_variant(0);

            BOOST_CHECK( !value_variant.IsNull() );

            value_variant.SetNull();
            BOOST_CHECK( value_variant.IsNull() );

            value_variant.SetNull();
            BOOST_CHECK( value_variant.IsNull() );

            value_variant.SetNull();
            BOOST_CHECK( value_variant.IsNull() );
        }

        // Check operator==
        {
            // Check values of same type ...
            BOOST_CHECK( CVariant( true ) == CVariant( true ) );
            BOOST_CHECK( CVariant( false ) == CVariant( false ) );
            BOOST_CHECK( CVariant( Uint1(1) ) == CVariant( Uint1(1) ) );
            BOOST_CHECK( CVariant( Int2(1) ) == CVariant( Int2(1) ) );
            BOOST_CHECK( CVariant( Int4(1) ) == CVariant( Int4(1) ) );
            BOOST_CHECK( CVariant( Int8(1) ) == CVariant( Int8(1) ) );
            BOOST_CHECK( CVariant( float(1) ) == CVariant( float(1) ) );
            BOOST_CHECK( CVariant( double(1) ) == CVariant( double(1) ) );
            BOOST_CHECK( CVariant( string("abcd") ) == CVariant( string("abcd") ) );
            BOOST_CHECK( CVariant( "abcd" ) == CVariant( "abcd" ) );
            BOOST_CHECK( CVariant( value_CTime, eShort ) ==
                         CVariant( value_CTime, eShort )
                         );
            BOOST_CHECK( CVariant( value_CTime, eLong ) ==
                         CVariant( value_CTime, eLong )
                         );
        }

        // Check operator<
        {
            // Check values of same type ...
            {
                //  Type not supported
                // BOOST_CHECK( CVariant( false ) < CVariant( true ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
                BOOST_CHECK( CVariant( Int2(-1) ) < CVariant( Int2(1) ) );
                BOOST_CHECK( CVariant( Int4(-1) ) < CVariant( Int4(1) ) );
                BOOST_CHECK( CVariant( Int8(-1) ) < CVariant( Int8(1) ) );
                BOOST_CHECK( CVariant( float(-1) ) < CVariant( float(1) ) );
                BOOST_CHECK( CVariant( double(-1) ) < CVariant( double(1) ) );
                BOOST_CHECK( CVariant(string("abcd")) < CVariant( string("bcde")));
                BOOST_CHECK( CVariant( "abcd" ) < CVariant( "bcde" ) );
                CTime new_time = value_CTime;
                new_time.AddMinute();
                BOOST_CHECK( CVariant( value_CTime, eShort ) <
                             CVariant( new_time, eShort )
                             );
                BOOST_CHECK( CVariant( value_CTime, eLong ) <
                             CVariant( new_time, eLong )
                             );
            }

            // Check comparasion with Uint1(0) ...
            //!!! It *fails* !!!
            if (false) {
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int2(1) ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int4(1) ) );
                // !!! Does not work ...
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int8(1) ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( float(1) ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( double(1) ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( string("bcde") ) );
                BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( "bcde" ) );
            }
        }

        // Check GetType
        {
            const CVariant variant_Int8( value_Int8 );
            BOOST_CHECK_EQUAL( eDB_BigInt, variant_Int8.GetType() );

            const CVariant variant_Int4( value_Int4 );
            BOOST_CHECK_EQUAL( eDB_Int, variant_Int4.GetType() );

            const CVariant variant_Int2( value_Int2 );
            BOOST_CHECK_EQUAL( eDB_SmallInt, variant_Int2.GetType() );

            const CVariant variant_Uint1( value_Uint1 );
            BOOST_CHECK_EQUAL( eDB_TinyInt, variant_Uint1.GetType() );

            const CVariant variant_float( value_float );
            BOOST_CHECK_EQUAL( eDB_Float, variant_float.GetType() );

            const CVariant variant_double( value_double );
            BOOST_CHECK_EQUAL( eDB_Double, variant_double.GetType() );

            const CVariant variant_bool( value_bool );
            BOOST_CHECK_EQUAL( eDB_Bit, variant_bool.GetType() );

            const CVariant variant_string( value_string );
            BOOST_CHECK_EQUAL( eDB_VarChar, variant_string.GetType() );

            const CVariant variant_char( value_char );
            BOOST_CHECK_EQUAL( eDB_VarChar, variant_char.GetType() );
        }

        // Test BLOB ...
        {
        }

        // Check AsNotNullString
        {
        }

        // Assignment of bound values ...
        {
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Variant2)
{
    const long rec_num = 3;
    const size_t size_step = 10;
    string sql;

    try {
        // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new vc1000_field records ...
            sql  = " INSERT INTO " + GetTableName();
            sql += "(int_field, vc1000_field) VALUES(@id, @val) \n";

            string::size_type str_size(10);
            char char_val('1');
            for (long i = 0; i < rec_num; ++i) {
                string str_val(str_size, char_val);
                str_size *= size_step;
                char_val += 1;

                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                auto_stmt->SetParam(
                    CVariant::LongChar(str_val.data(), str_val.size()),
                    "@val"
                    );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
        }

        // Test VarChar ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            sql  = "SELECT vc1000_field FROM " + GetTableName();
            sql += " ORDER BY int_field";

            string::size_type str_size(10);
            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    // Retrieve results, if any
                    while( rs->Next() ) {
                        BOOST_CHECK_EQUAL(false, rs->GetVariant(1).IsNull());
                        string col1 = rs->GetVariant(1).GetString();
                        BOOST_CHECK(col1.size() == str_size || col1.size() == 255);
                        str_size *= size_step;
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DateTime)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
    CVariant value(eDB_DateTime);
    CTime t;
    CVariant null_date(t, eLong);
    CTime dt_value;

    try {
        if (true) {
            // Initialization ...
            {
                sql =
                    "CREATE TABLE #test_datetime ( \n"
                    "   id INT, \n"
                    "   dt_field DATETIME NULL \n"
                    ") \n";

                auto_stmt->ExecuteUpdate( sql );
            }

            {
                // Initialization ...
                {
                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, GETDATE() )";

                    auto_stmt->ExecuteUpdate( sql );
                }

                // Retrieve data ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    auto_stmt->SendSql( sql );
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );

                    value = rs->GetVariant(2);

                    BOOST_CHECK( !value.IsNull() );

                    dt_value = value.GetCTime();
                    BOOST_CHECK( !dt_value.IsEmpty() );
                }

                // Insert data using parameters ...
                {
                    auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                    auto_stmt->SetParam( value, "@dt_val" );

                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, @dt_val)";

                    auto_stmt->ExecuteUpdate( sql );
                }

                // Retrieve data again ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    // ClearParamList is necessary here ...
                    auto_stmt->ClearParamList();

                    auto_stmt->SendSql( sql );
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value2.IsNull() );

                    const CTime& dt_value2 = value2.GetCTime();
                    BOOST_CHECK( !dt_value2.IsEmpty() );
                    CTime dt_value3 = value.GetCTime();
                    BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );

                    // Tracing ...
                    if (dt_value2.AsString() != dt_value3.AsString()) {
                        cout << "dt_value2 nanoseconds = " << dt_value2.NanoSecond()
                            << " dt_value3 nanoseconds = " << dt_value3.NanoSecond()
                            << endl;
                    }
                }

                // Insert NULL data using parameters ...
                {
                    auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                    auto_stmt->SetParam( null_date, "@dt_val" );

                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, @dt_val)";

                    auto_stmt->ExecuteUpdate( sql );
                }


                // Retrieve data again ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    // ClearParamList is necessary here ...
                    auto_stmt->ClearParamList();

                    auto_stmt->SendSql( sql );
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( value2.IsNull() );
                }
            }

            // Insert data using stored procedure ...
            if (false) {
                value = CTime(CTime::eCurrent);

                // Set a database ...
                {
                    auto_stmt->ExecuteUpdate("use DBAPI_Sample");
                }
                // Create a stored procedure ...
                {
                    bool already_exist = false;

                    // auto_stmt->SendSql( "select * FROM sysobjects WHERE xtype = 'P' AND name = 'sp_test_datetime'" );
                    auto_stmt->SendSql( "select * FROM sysobjects WHERE name = 'sp_test_datetime'" );
                    while( auto_stmt->HasMoreResults() ) {
                        if( auto_stmt->HasRows() ) {
                            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                            while ( rs->Next() ) {
                                already_exist = true;
                            }
                        }
                    }

                    if ( !already_exist ) {
                        sql =
                            " CREATE PROC sp_test_datetime(@dt_val datetime) \n"
                            " AS \n"
                            " INSERT INTO #test_datetime(id, dt_field) VALUES(1, @dt_val) \n";

                        auto_stmt->ExecuteUpdate( sql );
                    }

                }

                // Insert data using parameters ...
                {
                    auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                    auto_ptr<ICallableStatement> call_auto_stmt(
                        GetConnection().GetCallableStatement("sp_test_datetime")
                        );

                    call_auto_stmt->SetParam( value, "@dt_val" );
                    call_auto_stmt->Execute();
                    DumpResults(call_auto_stmt.get());

                    auto_stmt->ExecuteUpdate( "SELECT * FROM #test_datetime" );
                    int nRows = auto_stmt->GetRowCount();
                    BOOST_CHECK_EQUAL(nRows, 1);
                }

                // Retrieve data ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    auto_stmt->SendSql( sql );
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value2.IsNull() );

                    const CTime& dt_value2 = value2.GetCTime();
                    BOOST_CHECK( !dt_value2.IsEmpty() );
                    CTime dt_value3 = value.GetCTime();
                    BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );

                    // Tracing ...
                    if (dt_value2.AsString() != dt_value3.AsString()) {
                        cout << "dt_value2 nanoseconds = " << dt_value2.NanoSecond()
                            << " dt_value3 nanoseconds = " << dt_value3.NanoSecond()
                            << endl;
                    }

                    // Failed for some reason ...
                    // BOOST_CHECK(dt_value2 == dt_value);
                }

                // Insert NULL data using parameters ...
                {
                    auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                    auto_ptr<ICallableStatement> call_auto_stmt(
                        GetConnection().GetCallableStatement("sp_test_datetime")
                        );

                    call_auto_stmt->SetParam( null_date, "@dt_val" );
                    call_auto_stmt->Execute();
                    DumpResults(call_auto_stmt.get());
                }

                // Retrieve data ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    auto_stmt->SendSql( sql );
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( value2.IsNull() );
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Numeric)
{
    const string table_name("#test_numeric");
    const string str_value("2843113322.00");
    const string str_value_short("2843113322");
    const Uint8 value = 2843113322U;
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Initialization ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC(18, 0) IDENTITY NOT NULL, \n"
                "   num_field1 NUMERIC(18, 2) NULL, \n"
                "   num_field2 NUMERIC(35, 2) NULL \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

    // First test ...
        {
            // Initialization ...
            {
                sql = "INSERT INTO " + table_name + "(num_field1, num_field2) "
                    "VALUES(" + str_value + ", " + str_value + " )";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Retrieve data ...
            {
                sql = "SELECT num_field1, num_field2 FROM " + table_name;

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant value1 = rs->GetVariant(1);
                const CVariant value2 = rs->GetVariant(2);

                BOOST_CHECK( !value1.IsNull() );
                BOOST_CHECK( !value2.IsNull() );

                if (GetArgs().IsODBCBased()) {
                    BOOST_CHECK_EQUAL(value1.GetNumeric(), str_value_short);
                    BOOST_CHECK_EQUAL(value2.GetNumeric(), str_value_short);
                } else {
                    BOOST_CHECK_EQUAL(value1.GetNumeric(), str_value);
                    BOOST_CHECK_EQUAL(value2.GetNumeric(), str_value);
                }
            }

            // Insert data using parameters ...
            {
                CVariant value1(eDB_Double);
                CVariant value2(eDB_Double);

                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

                sql = "INSERT INTO " + table_name + "(num_field1, num_field2) "
                    "VALUES(@value1, @value2)";

                //
                {
                    //
                    value1 = static_cast<double>(value);
                    value2 = static_cast<double>(value);

                    auto_stmt->SetParam( value1, "@value1" );
                    auto_stmt->SetParam( value2, "@value2" );

                    auto_stmt->ExecuteUpdate( sql );

                    //
                    value1 = 98.79;
                    value2 = 98.79;

                    auto_stmt->SetParam( value1, "@value1" );
                    auto_stmt->SetParam( value2, "@value2" );

                    auto_stmt->ExecuteUpdate( sql );

                    //
                    value1 = 1.21;
                    value2 = 1.21;

                    auto_stmt->SetParam( value1, "@value1" );
                    auto_stmt->SetParam( value2, "@value2" );

                    auto_stmt->ExecuteUpdate( sql );
                }

                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();
            }

            // Retrieve data again ...
            {
                sql = "SELECT num_field1, num_field2 FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );

                //
                {
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value1 = rs->GetVariant(1);
                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value1.IsNull() );
                    BOOST_CHECK( !value2.IsNull() );

                    if (GetArgs().IsODBCBased()) {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), str_value_short);
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), str_value_short);
                    } else {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), str_value);
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), str_value);
                    }
                }

                //
                {
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value1 = rs->GetVariant(1);
                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value1.IsNull() );
                    BOOST_CHECK( !value2.IsNull() );

                    if (!GetArgs().IsODBCBased()) {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), string("98.79"));
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), string("98.79"));
                    } else {
                        GetArgs().PutMsgDisabled("Test_Numeric - part 2");
                    }
                }

                //
                {
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value1 = rs->GetVariant(1);
                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value1.IsNull() );
                    BOOST_CHECK( !value2.IsNull() );

                    if (!GetArgs().IsODBCBased()) {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), string("1.21"));
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), string("1.21"));
                    } else {
                        GetArgs().PutMsgDisabled("Test_Numeric - part 3");
                    }
                }
            }
        }

    // Second test ...
    {
        /*
        double double_array[] = {
        87832866,
        2661188789U,
        2661188789U,
        30811,
        0.00115779083871678,
        16727456,
        0.628570812756419,
        2536866043U,
        95.3283004004118,
        0,
        8583,
        0.000322525032251667,
        13634779,
        0.512356697741973,
        93921117,
        3.52929177321887,
        40319553,
        160410,
        1218,
        125
        };
        */

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data using parameters ...
            {
                CVariant value1(eDB_Double);
                CVariant value2(eDB_Double);

                sql = "INSERT INTO " + table_name + "(num_field1, num_field2) "
                    "VALUES(@value1, @value2)";

                //
                {
                    // TODO
                }

                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();
            }

    }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_VARCHAR_MAX)
{
    string sql;
    const string table_name = "#test_varchar_max_table";
    // const string table_name = "DBAPI_Sample..test_varchar_max_table";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   vc_max VARCHAR(MAX) NULL"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // SQL value injection technique ...
        {
            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

            const string msg(8001, 'Z');

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(vc_max) VALUES(\'" + msg + "\')";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK( rs.get() != NULL );

                        BOOST_CHECK( rs->Next() );
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), msg.size());
                        BOOST_CHECK_EQUAL(value, msg);
                    }
                }
            }
        }

        // Parameters ...
        {
            const string msg(4000, 'Z');
            const CVariant vc_max_value = CVariant::LongChar(msg.data());

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(vc_max) VALUES(@vc_max)";

                auto_stmt->SetParam( vc_max_value, "@vc_max" );
                auto_stmt->ExecuteUpdate( sql );
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK( rs.get() != NULL );

                        BOOST_CHECK( rs->Next() );
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), msg.size());
                        // BOOST_CHECK_EQUAL(value, msg);
                    }
                }
            }
        }

        // Streaming ...
        {
            string long_msg = "New text.";
            for (int i = 0;  i < 4000;  ++i) {
                long_msg += "  Let's make it long!";
            }
            // Modify the table ...
            {
                CDB_ITDescriptor desc(table_name, "vc_max", "1 = 1",
                                      CDB_ITDescriptor::eText);
                auto_stmt->GetBlobOStream(desc, long_msg.size()) << long_msg
                                                                 << flush;
            }
            // Actual check ...
            {
                auto_stmt->SendSql("SELECT vc_max FROM " + table_name);
                while (auto_stmt->HasMoreResults()) {
                    if(auto_stmt->HasRows()) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK(rs.get() != NULL);

                        BOOST_CHECK(rs->Next());
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), long_msg.size());
                        // BOOST_CHECK_EQUAL(value, msg);
                    }
                }
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_NVARCHAR_MAX_Stream)
{
    string sql;
    const string table_name = "#test_nvarchar_max_stream_table";
    // const string table_name = "DBAPI_Sample..test_varchar_max_stream_table";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   vc_max NVARCHAR(MAX) NULL"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert initial data.
	auto_stmt->ExecuteUpdate( "INSERT INTO " + table_name
				  + "(vc_max) VALUES(' ')" );

        // Streaming ...
        {
            string long_msg = "New text.";
            for (int i = 0;  i < 4000;  ++i) {
                // Embedding U+2019 to test Unicode handling.
                long_msg += "  Let\xe2\x80\x99s make it long!";
            }
            // Modify the table ...
            {
                CDB_ITDescriptor desc(table_name, "vc_max", "1 = 1",
                                      CDB_ITDescriptor::eText);
                auto_stmt->GetBlobOStream(desc, long_msg.size()) << long_msg
                                                                 << flush;
            }
            // Actual check ...
            {
                auto_stmt->SendSql("SELECT vc_max FROM " + table_name);
                while (auto_stmt->HasMoreResults()) {
                    if(auto_stmt->HasRows()) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK(rs.get() != NULL);

                        BOOST_CHECK(rs->Next());
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), long_msg.size());
                        // BOOST_CHECK_EQUAL(value, msg);
                    }
                }
            }
        }
    } catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_VARCHAR_MAX_BCP)
{
    string sql;
    const string table_name = "#test_varchar_max_bcp_table";
    // const string table_name = "DBAPI_Sample..test_varchar_max_bcp_table";
    // const string msg(32000, 'Z');
    const string msg(8001, 'Z');

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   vc_max VARCHAR(MAX) NULL"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        {
            const CVariant vc_max_value(msg);

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data into the table ...
            {
                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );
                CVariant col1(eDB_Int);
                // CVariant col2(eDB_VarChar);
                CVariant col2(eDB_Text);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                col1 = 1;
                // col2 = msg;
                col2.Append(msg.data(), msg.size());

                bi->AddRow();
                bi->Complete();
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK( rs.get() != NULL );

                        BOOST_CHECK( rs->Next() );
                        BOOST_CHECK_EQUAL(msg.size(), rs->GetVariant(1).GetBlobSize());
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), msg.size());
                        BOOST_CHECK_EQUAL(value, msg);
                    }
                }
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CHAR)
{
    string sql;
    const string table_name = "#test_char_table";
    // const string table_name = "DBAPI_Sample..test_char_table";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   char1_field CHAR(1) NULL"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Parameters ...
        {
            // const CVariant char_value;

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(char1_field) VALUES(@char1)";

                auto_stmt->SetParam( CVariant(string()), "@char1" );
                auto_stmt->ExecuteUpdate( sql );
            }

            // Actual check ...
            {
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                sql = "SELECT char1_field FROM " + table_name + " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        BOOST_CHECK( rs.get() != NULL );

                        BOOST_CHECK( rs->Next() );
                        const string value = rs->GetVariant(1).GetString();
                        BOOST_CHECK_EQUAL(value.size(), 1U);
                        BOOST_CHECK_EQUAL(value, string(" "));
                    }
                }
            }
        }

    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_NVARCHAR)
{
    string sql;

    try {
        const string kTableName = "#test_nvarchar", kText(5, 'A');
        const TStringUCS2 kTextUCS2(5, (TCharUCS2)'A');
        const EDB_Type kTypes[] = { eDB_Char, eDB_LongChar, eDB_VarChar };
        const Int4 kNumTypes = sizeof(kTypes) / sizeof(*kTypes);

        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        sql = "create table " + kTableName + " (n integer, nvc9 nvarchar(9))";
        auto_stmt->ExecuteUpdate(sql);

        sql = "insert into " + kTableName + " values (1, '" + kText + "')";
        auto_stmt->ExecuteUpdate(sql);

        auto_ptr<IBulkInsert> bi;
        CVariant n(eDB_Int);

        for (Uint4 i = 0;  i < kNumTypes;  ++i) {
            auto_ptr<CVariant> s;
            switch (kTypes[i]) {
            case eDB_Char:
                s.reset(new CVariant(CVariant::Char(5, kEmptyCStr)));
                break;
            case eDB_LongChar:
                s.reset(new CVariant(CVariant::LongChar(kEmptyCStr, 5)));
                break;
            default: // only VarChar is okay as is
                s.reset(new CVariant(kTypes[i]));
            }

            bi.reset(GetConnection().GetBulkInsert(kTableName));
            bi->Bind(1, &n);
            bi->Bind(2, s.get());

            n = Int4(i) * 2 + 2;
            s->SetBulkInsertionEnc(eBulkEnc_UCS2FromChar);
            *s = kText;
            bi->AddRow();
            n = n.GetInt4() + 1;
            *s = kTextUCS2;
            bi->AddRow();
            bi->Complete();
        }

        auto_stmt->SendSql("SELECT n, nvc9 from " + kTableName);
        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                while (rs->Next()) {
                    const CVariant& var = rs->GetVariant(2);
                    BOOST_CHECK_EQUAL(var.GetString(), kText);
                }
            }
        }

        if (false) {
            string Title69;

            auto_ptr<IConnection> conn(
                GetDS().CreateConnection(CONN_OWNERSHIP)
                );

            conn->Connect("anyone","allowed","MSSQL69", "PMC3");
//             conn->Connect("anyone","allowed","MSSQL42", "PMC3");
            auto_stmt.reset(conn->GetStatement());

            sql = "SELECT Title from Subject where SubjectId=7";
            auto_stmt->SendSql(sql);
            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs69(auto_stmt->GetResultSet());
                    while (rs69->Next()) {
                        Title69 = rs69->GetVariant("Title").GetString();
                    }
                }
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_NTEXT)
{
    string sql;

    try {
        const string ins_value = "asdfghjkl", kTableName = "#test_ntext";
        char buffer[20];

        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        sql = "SET TEXTSIZE 2147483647";
        auto_stmt->ExecuteUpdate(sql);

        sql = "create table " + kTableName + " (n integer, txt_fld ntext)";
        auto_stmt->ExecuteUpdate(sql);

        sql = "insert into " + kTableName + " values (1, '" + ins_value + "')";
        auto_stmt->ExecuteUpdate(sql);

        auto_ptr<IBulkInsert> bi(GetConnection().GetBulkInsert(kTableName));
        CVariant n(eDB_Int), txt_fld(eDB_Text);
        bi->Bind(1, &n);
        bi->Bind(2, &txt_fld);
        for (size_t i = 0, l = ins_value.size();  i < l;  ++i) {
            n = (Int4)i + 2;
            txt_fld.Truncate();
            txt_fld.SetBulkInsertionEnc(eBulkEnc_RawBytes);
            txt_fld.Append(CUtf8::AsBasicString<TCharUCS2>
                           (CTempString(ins_value, 0, i)));
            BOOST_CHECK_EQUAL(txt_fld.GetBulkInsertionEnc(), eBulkEnc_RawUCS2);
            if (i > 0  &&  i * 2 < l) {
                txt_fld.Append(ins_value.substr(i, i));
                txt_fld.Append(CUtf8::AsBasicString<TCharUCS2>
                               (CTempString(ins_value, i * 2, NPOS)));
            } else {
                txt_fld.Append(ins_value.substr(i));
            }
            bi->AddRow();
        }
        bi->Complete();

        sql = "SELECT n, txt_fld from " + kTableName;
        auto_stmt->SendSql(sql);
        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                while (rs->Next()) {
                    const CVariant& var = rs->GetVariant("txt_fld");
                    BOOST_CHECK_EQUAL(var.GetBlobSize(), ins_value.size());
                    var.Read(buffer, ins_value.size());
                    buffer[ins_value.size()] = 0;
                    BOOST_CHECK_EQUAL(string(buffer), ins_value);
                }
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Decimal)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

    sql = "declare @num decimal(6,2) set @num = 1.5 select @num as NumCol";
    auto_stmt->SendSql(sql);

    BOOST_CHECK( auto_stmt->HasMoreResults() );
    BOOST_CHECK( auto_stmt->HasRows() );
    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
    BOOST_CHECK( rs.get() != NULL );
    BOOST_CHECK( rs->Next() );

    DumpResults(auto_stmt.get());
}


END_NCBI_SCOPE
