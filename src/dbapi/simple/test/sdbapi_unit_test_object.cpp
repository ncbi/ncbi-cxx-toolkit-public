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

#include "sdbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DateTime)
{
    string sql;
    CQuery query = GetDatabase().NewQuery();
    CTime t;
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

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            {
                // Initialization ...
                {
                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, GETDATE() )";

                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }

                // Retrieve data ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(1);
                    BOOST_CHECK( query.HasMoreResultSets() );
                    CQuery::iterator it = query.begin();
                    BOOST_CHECK( it != query.end() );
                    BOOST_CHECK( !it[2].IsNull());
                    dt_value = it[2].AsDateTime();
                    BOOST_CHECK( !dt_value.IsEmpty() );
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }

                // Insert data using parameters ...
                {
                    query.SetSql( "DELETE FROM #test_datetime" );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                    query.SetParameter( "@dt_val", dt_value );

                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, @dt_val)";

                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }

                // Retrieve data again ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    // ClearParamList is necessary here ...
                    query.ClearParameters();
                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(1);
                    BOOST_CHECK( query.HasMoreResultSets() );
                    CQuery::iterator it = query.begin();
                    BOOST_CHECK( it != query.end() );
                    BOOST_CHECK( !it[2].IsNull());
                    CTime dt_value2 = it[2].AsDateTime();
                    BOOST_CHECK_EQUAL( dt_value.AsString(), dt_value2.AsString() );
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }

                // Insert NULL data using parameters ...
                {
                    query.SetSql( "DELETE FROM #test_datetime" );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                    query.SetNullParameter( "@dt_val", eSDB_DateTime );

                    sql = "INSERT INTO #test_datetime(id, dt_field) "
                          "VALUES(1, @dt_val)";

                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }


                // Retrieve data again ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    // ClearParamList is necessary here ...
                    query.ClearParameters();
                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(1);
                    BOOST_CHECK( query.HasMoreResultSets() );
                    CQuery::iterator it = query.begin();
                    BOOST_CHECK( it != query.end() );
                    BOOST_CHECK( it[2].IsNull());
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
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
        CQuery query = GetDatabase().NewQuery();

        // Initialization ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC(18, 0) IDENTITY NOT NULL, \n"
                "   num_field1 NUMERIC(18, 2) NULL, \n"
                "   num_field2 NUMERIC(35, 2) NULL \n"
                ") \n";

            query.SetSql( sql );
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

    // First test ...
        {
            // Initialization ...
            {
                sql = "INSERT INTO " + table_name + "(num_field1, num_field2) "
                    "VALUES(" + str_value + ", " + str_value + " )";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Retrieve data ...
            {
                sql = "SELECT num_field1, num_field2 FROM " + table_name;

                query.SetSql( sql );
                query.Execute();
                BOOST_CHECK( query.HasMoreResultSets() );
                query.RequireRowCount(1);
                CQuery::iterator it = query.begin();
                BOOST_CHECK( it != query.end() );

                BOOST_CHECK(!it[1].IsNull());
                BOOST_CHECK(!it[2].IsNull());
                BOOST_CHECK_EQUAL(it[1].AsString(), str_value);
                BOOST_CHECK_EQUAL(it[2].AsString(), str_value);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data using parameters ...
            {
                query.SetSql( "DELETE FROM " + table_name );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                sql = "INSERT INTO " + table_name + "(num_field1, num_field2) "
                    "VALUES(@value1, @value2)";

                //
                {
                    //
                    query.SetParameter( "@value1", static_cast<double>(value) );
                    query.SetParameter( "@value2", static_cast<double>(value) );

                    query.SetSql( sql );
                    query.Execute();
                    query.RequireRowCount(0);
                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }

                // ClearParamList is necessary here ...
                query.ClearParameters();
            }

            // Retrieve data again ...
            {
                sql = "SELECT num_field1, num_field2 FROM " + table_name +
                    " ORDER BY id";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(1);
                BOOST_CHECK( query.HasMoreResultSets() );
                CQuery::iterator it = query.begin();
                BOOST_CHECK( it != query.end() );

                BOOST_CHECK(!it[1].IsNull());
                BOOST_CHECK(!it[2].IsNull());
                BOOST_CHECK_EQUAL(it[1].AsString(), str_value);
                BOOST_CHECK_EQUAL(it[2].AsString(), str_value);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
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
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   vc_max VARCHAR(MAX) NULL"
                ") \n";

            query.SetSql( sql );
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // SQL value injection technique ...
        {
            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            const string msg(8001, 'Z');

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(vc_max) VALUES(\'" + msg + "\')";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(1);
                ITERATE(CQuery, it, query.SingleSet()) {
                    const string value = it[1].AsString();
                    BOOST_CHECK_EQUAL(value.size(), msg.size());
                    BOOST_CHECK_EQUAL(value, msg);
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // Parameters ...
        {
            const string msg(4000, 'Z');

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(vc_max) VALUES(@vc_max)";

                query.SetParameter( "@vc_max", msg );
                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(1);
                ITERATE(CQuery, it, query.SingleSet()) {
                    const string value = it[1].AsString();
                    BOOST_CHECK_EQUAL(value.size(), msg.size());
                    // BOOST_CHECK_EQUAL(value, msg);
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

    }
    catch(const CSDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
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
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   vc_max VARCHAR(MAX) NULL"
                ") \n";

            query.SetSql( sql );
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        {
            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data into the table ...
            {
                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 1);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_Text);

                bi << 1 << msg << EndRow;
                bi.Complete();
            }

            // Actual check ...
            {
                sql = "SELECT vc_max FROM " + table_name + " ORDER BY id";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(1);
                ITERATE(CQuery, it, query.SingleSet()) {
                    const string value = it[1].AsString();
                    BOOST_CHECK_EQUAL(value.size(), msg.size());
                    BOOST_CHECK_EQUAL(value, msg);
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }
    }
    catch(const CSDB_Exception& ex) {
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
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        if (table_name[0] =='#') {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id NUMERIC IDENTITY NOT NULL, \n"
                "   char1_field CHAR(1) NULL"
                ") \n";

            query.SetSql( sql );
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Parameters ...
        {
            // const CVariant char_value;

            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data into the table ...
            {
                sql =
                    "INSERT INTO " + table_name + "(char1_field) VALUES(@char1)";

                query.SetParameter( "@char1", "" );
                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Actual check ...
            {
                // ClearParamList is necessary here ...
                query.ClearParameters();

                sql = "SELECT char1_field FROM " + table_name + " ORDER BY id";

                query.SetSql( sql );
                query.Execute();
                query.RequireRowCount(1);
                ITERATE(CQuery, it, query.SingleSet()) {
                    const string value = it[1].AsString();
                    BOOST_CHECK_EQUAL(value.size(), 1U);
                    BOOST_CHECK_EQUAL(value, string(" "));
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

    }
    catch(const CSDB_Exception& ex) {
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
        const string kInsValue = "asdfghjkl", kTableName = "#test_ntext";

        CQuery query = GetDatabase().NewQuery();

        sql = "SET TEXTSIZE 2147483647";
        query.SetSql( sql );
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        sql = "create table " + kTableName + " (n integer, txt_fld ntext)";
        query.SetSql( sql );
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        sql = "insert into " + kTableName + " values (1, '" + kInsValue + "')";
        query.SetSql( sql );
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        CBulkInsert bi = GetDatabase().NewBulkInsert(kTableName, 10);
        bi.Bind(1, eSDB_Int4);
        bi.Bind(2, eSDB_Text);
        bi << 2 << CUtf8::AsBasicString<TCharUCS2>(kInsValue) << EndRow;
        bi.Complete();

        bi = GetDatabase().NewBulkInsert(kTableName, 10);
        bi.Bind(1, eSDB_Int4);
        bi.Bind(2, eSDB_TextUCS2);
        bi << 3 << kInsValue << EndRow;
        bi << 4 << CUtf8::AsBasicString<TCharUCS2>(kInsValue) << EndRow;
        bi.Complete();

        sql = "SELECT n, txt_fld from " + kTableName;
        query.SetSql( sql );
        query.Execute();
        query.RequireRowCount(4);
        ITERATE(CQuery, it, query.SingleSet()) {
            string var = it["txt_fld"].AsString();
            BOOST_CHECK_EQUAL(var, kInsValue);
        }
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
    }
    catch(const CSDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


BOOST_AUTO_TEST_CASE(Test_NVARCHAR)
{
    string sql;

    try {
        const string kTableName = "#test_nvarchar", kText(5, 'A');
        const TStringUCS2 kTextUCS2(5, (TCharUCS2)'A');
        CQuery query = GetDatabase().NewQuery();

        sql = "create table " + kTableName + " (n integer, nvc9 nvarchar(9))";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        sql = "insert into " + kTableName + " values (1, '" + kText + "')";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        CBulkInsert bi = GetDatabase().NewBulkInsert(kTableName, 10);
        bi.Bind(1, eSDB_Int4);
        bi.Bind(2, eSDB_String);
        bi << 2 << kTextUCS2 << EndRow;
        bi.Complete();

        bi = GetDatabase().NewBulkInsert(kTableName, 10);
        bi.Bind(1, eSDB_Int4);
        bi.Bind(2, eSDB_StringUCS2);
        bi << 3 << kText << EndRow;
        bi << 4 << kTextUCS2 << EndRow;
        bi.Complete();

        sql = "SELECT n, nvc9 from " + kTableName;
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(4);
        ITERATE (CQuery, it, query.SingleSet()) {
            string var = it[2].AsString();
            BOOST_CHECK_EQUAL(var, kText);
        }
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

    } catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

END_NCBI_SCOPE
