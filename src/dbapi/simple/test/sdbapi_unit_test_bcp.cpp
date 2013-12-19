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
BOOST_AUTO_TEST_CASE(Test_DateTimeBCP)
{
    string table_name("#test_bcp_datetime");
    // string table_name("DBAPI_Sample..test_bcp_datetime");
    string sql;
    CQuery query = GetDatabase().NewQuery();
    CTime t;
    CTime dt_value;

    try {
        // Initialization ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id INT, \n"
                "   dt_field DATETIME NULL \n"
                ") \n";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data using BCP ...
        {
            t = CTime(CTime::eCurrent);

            // Insert data using parameters ...
            {
                query.SetSql("DELETE FROM " + table_name);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 1);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_DateTime);

                bi << 1 << t << EndRow;
                bi.Complete();

                query.SetSql("SELECT * FROM " + table_name);
                query.Execute();
                query.RequireRowCount(1);
                query.PurgeResults();
                int nRows = query.GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(1);
                BOOST_CHECK( query.HasMoreResultSets() );
                CQuery::iterator it = query.begin();
                BOOST_CHECK( it != query.end() );

                BOOST_CHECK( !it[2].IsNull() );

                CTime dt_value2 = it[2].AsDateTime();
                BOOST_CHECK( !dt_value2.IsEmpty() );
                BOOST_CHECK_EQUAL( dt_value2.AsString(), t.AsString() );
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert NULL data using parameters ...
            {
                query.SetSql("DELETE FROM " + table_name);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 1);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_DateTime);

                bi << 1 << NullValue << EndRow;
                bi.Complete();

                query.SetSql("SELECT * FROM " + table_name);
                query.Execute();
                query.RequireRowCount(1);
                query.PurgeResults();
                int nRows = query.GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(1);
                BOOST_CHECK( query.HasMoreResultSets() );
                CQuery::iterator it = query.begin();
                BOOST_CHECK( it != query.end() );
                BOOST_CHECK( it[2].IsNull() );
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BulkInsertBlob)
{
    string sql;
    enum { record_num = 100 };
    // string table_name = GetTableName();
    string table_name = "#dbapi_bcp_table2";

    try {
        CQuery query = GetDatabase().NewQuery();

        // First test ...
        {
            // Prepare data ...
            {
                // Clean table ...
                query.SetSql("DELETE FROM "+ table_name);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data ...
            {
                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, record_num + 1);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_Int4);
                bi.Bind(3, eSDB_String);
                bi.Bind(4, eSDB_Text);

                for( int i = 0; i < record_num; ++i ) {
                    string im = NStr::IntToString(i);
                    bi << i << i << im << im << EndRow;
                }
                bi.Complete();
            }

            // Check inserted data ...
            {
                size_t rec_num = GetNumOfRecords(query, table_name);
                BOOST_CHECK_EQUAL(rec_num, (size_t)record_num);
            }
        }

        // Second test ...
        {
            enum { batch_num = 10 };
            // Prepare data ...
            {
                // Clean table ...
                query.SetSql("DELETE FROM "+ table_name);
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // Insert data ...
            {
                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, batch_num);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_Int4);
                bi.Bind(3, eSDB_String);
                bi.Bind(4, eSDB_Text);

                for( int i = 0; i < (int)record_num * (int)batch_num; ++i ) {
                    string im = NStr::IntToString(i);
                    bi << i << i << im << im << EndRow;
                }
                bi.Complete();
            }

            // Check inserted data ...
            {
                size_t rec_num = GetNumOfRecords(query, table_name);
                BOOST_CHECK_EQUAL(rec_num, size_t(record_num) * size_t(batch_num));
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Overflow)
{
    string sql;
    enum {column_size = 32, data_size = 64};

    try {
        CQuery query = GetDatabase().NewQuery();

        // Initialize ...
        {
            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vc32_field VARCHAR(32) \n"
                ") \n";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        string str_data = string(data_size, 'O');

        // Insert data ...
        {
            CBulkInsert bi = GetDatabase().NewBulkInsert("#test_bulk_overflow", 1);

            bi.Bind(1, eSDB_String);

            bi << str_data;

            // Neither AddRow() nor Complete() should throw an exception.
            BOOST_CHECK_NO_THROW(
                bi << EndRow;
                bi.Complete();
                                );
        }

        // Retrieve data ...
        {
            sql = "SELECT * FROM #test_bulk_overflow";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            BOOST_CHECK( query.HasMoreResultSets() );
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );

            BOOST_CHECK(!it[1].IsNull());
            string str_value = it[1].AsString();
            BOOST_CHECK_EQUAL( string::size_type(column_size), str_value.size() );
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Initialize ...
        {
            sql = "DROP TABLE #test_bulk_overflow";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vb32_field VARBINARY(32) \n"
                ") \n";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            CBulkInsert bi = GetDatabase().NewBulkInsert("#test_bulk_overflow", 2);

            bi.Bind(1, eSDB_Binary);

            bi << str_data;

            // Here either AddRow() or Complete() should throw an exception.
            BOOST_CHECK_THROW(
                bi << EndRow;
                bi.Complete();
                              , CSDB_Exception);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing)
{
    string sql;
    string table_name = "#bin_bulk_insert_table";
    // string table_name = "DBAPI_Sample..bin_bulk_insert_table";

    try {
        CQuery query = GetDatabase().NewQuery();

        if (table_name[0] == '#') {
            // Table for bulk insert ...
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(8000) NULL, \n";
            sql += "    vb8000_field VARBINARY(8000) NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    bigint_field BIGINT NULL \n";
            sql += " )";

            // Create the table
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }


        // VARBINARY ...
        {
            enum { num_of_tests = 10 };
            const char char_val('2');

            // Clean table ...
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            {
                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_String);
                bi.Bind(3, eSDB_Binary);

                for(int i = 0; i < num_of_tests; ++i ) {
                    int int_value = 4000 / num_of_tests * i;
                    string str_val(int_value , char_val);

                    bi << int_value << NullValue << str_val << EndRow;
                }
                bi.Complete();
            }

            // Retrieve data ...
            {
                sql  = " SELECT id, vb8000_field FROM " + table_name;
                sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(num_of_tests);

                BOOST_CHECK( query.HasMoreResultSets() );
                CQuery::iterator it = query.begin();
                for(int i = 0; i < num_of_tests; ++i, ++it) {
                    BOOST_CHECK( it != query.end() );

                    int int_value = 4000 / num_of_tests * i;
                    Int4 id = it[1].AsInt4();
                    string vb8000_value = it[2].AsString();

                    BOOST_CHECK_EQUAL( int_value, id );
                    BOOST_CHECK_EQUAL(string::size_type(int_value),
                                      vb8000_value.size()
                                      );
                }

                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }

        // INT, BIGINT
        {

            // Clean table ...
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // INT collumn ...
            {
                enum { num_of_tests = 8 };

                // Insert data ...
                {
                    CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

                    bi.Bind(1, eSDB_Int4);
                    bi.Bind(2, eSDB_String);
                    bi.Bind(3, eSDB_String);
                    bi.Bind(4, eSDB_Int4);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        bi << i << NullValue << NullValue << (Int4( 1 ) << (i * 4)) << EndRow;
                    }
                    bi.Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT int_field FROM " + table_name;
                    sql += " ORDER BY id";

                    query.SetSql(sql);
                    query.Execute();
                    query.RequireRowCount(num_of_tests);

                    BOOST_CHECK( query.HasMoreResultSets() );
                    CQuery::iterator it = query.begin();
                    for(int i = 0; i < num_of_tests; ++i, ++it) {
                        BOOST_CHECK( it != query.end() );
                        Int4 value = it[1].AsInt4();
                        Int4 expected_value = Int4( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
                }
            }

            // Clean table ...
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // BIGINT collumn ...
            {
                enum { num_of_tests = 16 };

                // Insert data ...
                {
                    CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

                    bi.Bind(1, eSDB_Int4);
                    bi.Bind(2, eSDB_String);
                    bi.Bind(3, eSDB_String);
                    bi.Bind(4, eSDB_String);
                    bi.Bind(5, eSDB_Int8);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        bi << i << NullValue << NullValue << NullValue
                           << (Int8( 1 ) << (i * 4)) << EndRow;
                    }
                    bi.Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT bigint_field FROM " + table_name;
                    sql += " ORDER BY id";

                    query.SetSql(sql);
                    query.Execute();
                    query.RequireRowCount(num_of_tests);

                    BOOST_CHECK( query.HasMoreResultSets() );
                    CQuery::iterator it = query.begin();
                    for(int i = 0; i < num_of_tests; ++i, ++it) {
                        BOOST_CHECK( it != query.end() );
                        Int8 value = it[1].AsInt8();
                        Int8 expected_value = Int8( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    // Dump results ...
                    query.PurgeResults();
                    BOOST_CHECK_NO_THROW(
                        query.VerifyDone(CQuery::eAllResultSets));
                }
            }
        }

        // Yet another BIGINT test (and more) ...
        {
            // Create table ...
            {
                query.SetSql(
                    // "create table #__blki_test ( name char(32) not null, value bigint null )" );
                    "create table #__blki_test ( name char(32), value bigint null )"
                    );
                query.Execute();
                query.RequireRowCount(0);
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }

            // First test ...
            {
                CBulkInsert blki = GetDatabase().NewBulkInsert("#__blki_test", 10);

                blki.Bind(1, eSDB_String);
                blki.Bind(2, eSDB_Int8);

                blki << "Hello-1" << 123 << EndRow;
                blki << "Hello-2" << 1234 << EndRow;
                blki << "Hello-3" << 12345 << EndRow;
                blki << "Hello-4" << 123456 << EndRow;
                blki.Complete();
            }
        }

        // VARCHAR ...
        {
            int num_of_tests = 6;

            // Clean table ...
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Insert data ...
            {
                CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

                bi.Bind(1, eSDB_Int4);
                bi.Bind(2, eSDB_String);

                for(int i = 1; i <= num_of_tests; ++i ) {
                    bi << i;
                    int str_len = 0;
                    switch (i) {
                    case 1:
                        str_len = 4000;
                        break;
                    case 2:
                        str_len = 1024;
                        break;
                    case 3:
                        str_len = 4112;
                        break;
                    case 4:
                        str_len = 4113;
                        break;
                    case 5:
                        str_len = 4138;
                        break;
                    case 6:
                        str_len = 4139;
                        break;
                    };
                    if (str_len == 0) {
                        bi << NullValue;
                    }
                    else {
                        bi << string(str_len, 'A');
                    }
                    bi << EndRow;
                }
                bi.Complete();
            }

            // Retrieve data ...
            {
                sql  = " SELECT id, vc8000_field FROM " + table_name;
                sql += " ORDER BY id";

                query.SetSql(sql);
                query.Execute();
                query.RequireRowCount(num_of_tests);
                ITERATE(CQuery, it, query.SingleSet()) {
                    Int4 i = it[1].AsInt4();
                    string col1 = it[2].AsString();

                    switch (i) {
                    case 1:
                        BOOST_CHECK_EQUAL(
                            col1.size(),
                            string::size_type(4000)
                            );
                        break;
                    case 2:
                        BOOST_CHECK_EQUAL(col1.size(),
                                          string::size_type(1024)
                                          );
                        break;
                    case 3:
                        BOOST_CHECK_EQUAL(col1.size(),
                                          string::size_type(4112)
                                          );
                        break;
                    case 4:
                        BOOST_CHECK_EQUAL(col1.size(),
                                          string::size_type(4113)
                                          );
                        break;
                    case 5:
                        BOOST_CHECK_EQUAL(col1.size(),
                                          string::size_type(4138)
                                          );
                        break;
                    case 6:
                        BOOST_CHECK_EQUAL(col1.size(),
                                          string::size_type(4139)
                                          );
                        break;
                    };
                }
                BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing2)
{
    string sql;
    const string table_name("#SbSubs");

    try {
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + "( \n"
                "    sacc int NOT NULL , \n"
                "    ver smallint NOT NULL , \n"
                "    live bit DEFAULT (1) NOT NULL, \n"
                "    sid int NOT NULL , \n"
                "    date datetime DEFAULT (getdate()) NULL, \n"
                "    rlsdate smalldatetime NULL, \n"
                "    depdate datetime DEFAULT (getdate()) NOT NULL \n"
                ")"
                ;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

            bi.Bind(1, eSDB_Int4);
            bi.Bind(2, eSDB_Int4);
            bi.Bind(3, eSDB_String);
            bi.Bind(4, eSDB_Int4);

            bi << 15001 << 1 << 1 << 0 << EndRow;
            bi.Complete();
        }

        // Retrieve data ...
        {
            sql  = " SELECT live, date, rlsdate, depdate FROM " + table_name;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);

            BOOST_CHECK( query.HasMoreResultSets() );
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );
            BOOST_CHECK( !it[1].IsNull() );
            BOOST_CHECK( it[1].AsByte() );
            BOOST_CHECK( !it[2].IsNull() );
            BOOST_CHECK( !it[2].AsDateTime().IsEmpty() );
            BOOST_CHECK( it[3].IsNull() );
            BOOST_CHECK( !it[4].IsNull() );
            BOOST_CHECK( !it[4].AsDateTime().IsEmpty() );

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Second test
        {
            sql = "DELETE FROM " + table_name;
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

            bi.Bind(1, eSDB_Int4);
            bi.Bind(2, eSDB_Int4);
            bi.Bind(3, eSDB_Int4);
            bi.Bind(4, eSDB_Int4);
            bi.Bind(5, eSDB_DateTime);
            bi.Bind(6, eSDB_DateTime);
            bi.Bind(7, eSDB_DateTime);

            bi << 15001 << 1 << 2 << NullValue << NullValue << NullValue << NullValue;
            BOOST_CHECK_THROW(
                bi << EndRow;
                bi.Complete();
                              , CSDB_Exception);
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing3)
{
    string sql;
    string table_name("#blk_table3");
    const int test_num = 10;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    uid int NOT NULL , \n"
                "    info_id int NOT NULL \n"
                ")"
                ;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, test_num);

            bi.Bind(1, eSDB_Int4);
            bi.Bind(2, eSDB_Int4);

            for (int i = 0; i < test_num * test_num; ++i) {
                bi << i << i * 2 << EndRow;
            }
            bi.Complete();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing4)
{
    string sql;
    string table_name("#blk_table4");
    const int test_num = 10;

    try {
        CQuery query = GetDatabase().NewQuery();
        const string test_data("Test, test, tEST.");

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                // Identity doesn't work with old ctlib driver (12.5.0.6-ESD13-32bit)
                //"    id int IDENTITY NOT NULL , \n"
                "    id_nwparams int NOT NULL, \n"
                "    gi1 int NOT NULL, \n"
                "    gi2 int NOT NULL, \n"
                "    idty FLOAT NOT NULL, \n"
                "    transcript TEXT NOT NULL, \n"
                "    idty_tup2 FLOAT NULL, \n"
                "    idty_tup4 FLOAT NULL \n"
                ")"
                ;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            //
            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

            Uint2 pos = 0;
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Float);
            bi.Bind(++pos, eSDB_Text);
            bi.Bind(++pos, eSDB_Float);
            bi.Bind(++pos, eSDB_Float);

            for (int j = 0; j < test_num; ++j) {
                bi << 123456 << j + 1 << j + 2 << float(j + 1.1) << test_data
                   << float(j + 2.2) << float(j + 3.3) << EndRow;
            }
            bi.Complete();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Writing7)
{
    string sql;
    string table_name("#blk_table7");
    const int test_num = 10;
    const string str1 = "Test, test, tEST.";
    const string str2 = "asdfghjkl";
    const string str3 = "Test 1234567890";
    const int i1 = int(0xdeadbeaf);
    const int i2 = int(0xcac0ffee);
    const int i3 = int(0xba1c0c0a);
    const short s1 = short(0xfade);
    const short s2 = short(0x0ec0);
    const Uint1 t1 = char(0x42);
    const Uint1 b_vals[test_num][11] = {
          {1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1},
          {1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1},
          {0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1},
          {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0},
          {1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1},
          {0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0},
          {0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1},
          {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0},
          {0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1},
          {1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0},
        };

    try {
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    id int, \n"
                "    i1 int, \n"
                "    str1 varchar(100), \n"
                "    b1 bit, \n"
                "    b2 bit, \n"
                "    i2 int, \n"
                "    b3 bit, \n"
                "    s1 smallint, \n"
                "    b4 bit, \n"
                "    b5 bit, \n"
                "    i3 int, \n"
                "    b6 bit, \n"
                "    str2 varchar(100), \n"
                "    b7 bit, \n"
                "    s2 smallint, \n"
                "    b8 bit, \n"
                "    b9 bit, \n"
                "    t1 tinyint, \n"
                "    b10 bit, \n"
                "    str3 varchar(100), \n"
                "    b11 bit \n"
                ")"
                ;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            //
            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 1);

            Uint2 pos = 0;
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_String);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Short);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Int4);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_String);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Short);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_Byte);
            bi.Bind(++pos, eSDB_String);
            bi.Bind(++pos, eSDB_Byte);

            for (int j = 0; j < test_num; ++j) {
                bi << j
                   << i1
                   << str1
                   << b_vals[j][0]
                   << b_vals[j][1]
                   << i2
                   << b_vals[j][2]
                   << s1
                   << b_vals[j][3]
                   << b_vals[j][4]
                   << i3
                   << b_vals[j][5]
                   << str2
                   << b_vals[j][6]
                   << s2
                   << b_vals[j][7]
                   << b_vals[j][8]
                   << t1
                   << b_vals[j][9]
                   << str3
                   << b_vals[j][10];
                bi << EndRow;
            }
            bi.Complete();
        }

        // Check correctness
        {
            query.SetSql("select * from " + table_name + " order by id");
            query.Execute();
            query.RequireRowCount(test_num);
            int j = 0;
            ITERATE(CQuery, it, query) {
                BOOST_CHECK_EQUAL(it["i1"].AsInt4(), i1);
                BOOST_CHECK_EQUAL(it["str1"].AsString(), str1);
                BOOST_CHECK_EQUAL(it["b1"].AsByte(), b_vals[j][0]);
                BOOST_CHECK_EQUAL(it["b2"].AsByte(), b_vals[j][1]);
                BOOST_CHECK_EQUAL(it["i2"].AsInt4(), i2);
                BOOST_CHECK_EQUAL(it["b3"].AsByte(), b_vals[j][2]);
                BOOST_CHECK_EQUAL(it["s1"].AsShort(), s1);
                BOOST_CHECK_EQUAL(it["b4"].AsByte(), b_vals[j][3]);
                BOOST_CHECK_EQUAL(it["b5"].AsByte(), b_vals[j][4]);
                BOOST_CHECK_EQUAL(it["i3"].AsInt4(), i3);
                BOOST_CHECK_EQUAL(it["b6"].AsByte(), b_vals[j][5]);
                BOOST_CHECK_EQUAL(it["str2"].AsString(), str2);
                BOOST_CHECK_EQUAL(it["b7"].AsByte(), b_vals[j][6]);
                BOOST_CHECK_EQUAL(it["s2"].AsShort(), s2);
                BOOST_CHECK_EQUAL(it["b8"].AsByte(), b_vals[j][7]);
                BOOST_CHECK_EQUAL(it["b9"].AsByte(), b_vals[j][8]);
                BOOST_CHECK_EQUAL(it["t1"].AsByte(), t1);
                BOOST_CHECK_EQUAL(it["b10"].AsByte(), b_vals[j][9]);
                BOOST_CHECK_EQUAL(it["str3"].AsString(), str3);
                BOOST_CHECK_EQUAL(it["b11"].AsByte(), b_vals[j][10]);
                ++j;
            }
            BOOST_CHECK_EQUAL(j, test_num);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Bulk_Late_Bind)
{
    string sql;
    const string table_name("#blk_late_bind_table");

    try {
        CQuery query = GetDatabase().NewQuery();

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    id int NULL, \n"
                "    name varchar(200) NULL \n"
                ")"
                ;

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Check that data cannot be binded after rows sending
        {
            CBulkInsert bi = GetDatabase().NewBulkInsert(table_name, 100);

            bi.Bind(1, eSDB_Int4);

            bi << 5 << EndRow;
            BOOST_CHECK_THROW((bi.Bind(2, eSDB_String)), CSDB_Exception);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
