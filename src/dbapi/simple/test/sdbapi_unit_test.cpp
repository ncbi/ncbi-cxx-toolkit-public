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
// Patterns to test:
//      I) Statement:
//          1) New/dedicated statement for each test
//          2) Reusable statement for all tests

///////////////////////////////////////////////////////////////////////////////
CTestTransaction::CTestTransaction(
    const CDatabase& conn,
    ETransBehavior tb
    )
    : m_Conn( conn )
    , m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        CQuery query = m_Conn.NewQuery("BEGIN TRANSACTION");
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            CQuery query = m_Conn.NewQuery("COMMIT TRANSACTION");
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        } else if ( m_TransBehavior == eTransRollback ) {
            CQuery query = m_Conn.NewQuery("ROLLBACK TRANSACTION");
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Unicode_Simple)
{
    string sql;
    string str_value;

    try {
        CQuery query = GetDatabase().NewQuery();
        size_t read_bytes = 0;

        sql  = "select 1 as Tag, null as Parent, ";
        sql += "1 as 'Test!1!id' ";
        sql += "for xml explicit";

        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(1);
        BOOST_CHECK(query.HasMoreResultSets());

        CQuery::iterator it = query.begin();
        if (it != query.end()) {
            read_bytes = it[1].AsString().size();
            BOOST_CHECK_EQUAL(size_t(14), read_bytes);
        }
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(1);
        BOOST_CHECK(query.HasMoreResultSets());

        it = query.begin();
        if (it != query.end()) {
            read_bytes = it[1].AsString().size();
            BOOST_CHECK_EQUAL(size_t(11), read_bytes);
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


///////////////////////////////////////////////////////////////////////////////
// Test unicode without binding values ...
BOOST_AUTO_TEST_CASE(Test_UnicodeNB)
{
    string table_name("#test_unicode_table");
    // string table_name("DBAPI_Sample..test_unicode_table");

    CQuery query = GetDatabase().NewQuery();
    string str_ger("Außerdem können Sie einzelne Einträge aus Ihrem "
                   "Suchprotokoll entfernen");

    // Russian phrase in UTF8 encoding ...
    const unsigned char str_rus_utf8[] =
    {
        0xd0, 0x9c, 0xd0, 0xb0, 0xd0, 0xbc, 0xd0, 0xb0, 0x20, 0xd0, 0xbc, 0xd1,
        0x8b, 0xd0, 0xbb, 0xd0, 0xb0, 0x20, 0xd1, 0x80, 0xd0, 0xb0, 0xd0, 0xbc,
        0xd1, 0x83, 0x00
    };

    const CStringUTF8 utf8_str_ger(CUtf8::AsUTF8(str_ger, eEncoding_Windows_1252));
    const CStringUTF8 utf8_str_rus(CUtf8::AsUTF8(reinterpret_cast<const char*>(str_rus_utf8), eEncoding_UTF8));

    try {
        // Create table ...
        if (false) {
            string sql =
                "CREATE TABLE " + table_name + "( \n"
                "    id NUMERIC(18, 0) IDENTITY PRIMARY KEY, \n"
                "    nvc255_field NVARCHAR(255) NULL \n"
                ") ";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Clean table ...
        {
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...

        string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + utf8_str_ger + "')";
        // string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + utf8_str_ger + "')";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + utf8_str_rus + "')";
        // sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + utf8_str_rus + "')";
        query.SetSql(sql);
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        // Retrieve data ...
        {
            string nvc255_value;

            sql  = " SELECT nvc255_field FROM " + table_name;
            sql += " ORDER BY id";

            CStringUTF8 utf8_sql(CUtf8::AsUTF8(sql, eEncoding_Windows_1252));
            query.SetSql(utf8_sql);
            query.Execute();
            query.RequireRowCount(2);

            BOOST_CHECK( query.HasMoreResultSets() );

            // Read ...
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );
            nvc255_value = it[1].AsString();
            BOOST_CHECK( nvc255_value.size() > 0);

            BOOST_CHECK_EQUAL( utf8_str_ger.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_ger), NStr::PrintableString(nvc255_value) );
            CStringUTF8 utf8_ger(CUtf8::AsUTF8(nvc255_value, eEncoding_UTF8));
            string value_ger =
                CUtf8::AsSingleByteString(utf8_ger, eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( NStr::PrintableString(str_ger), NStr::PrintableString(value_ger) );

            //
            ++it;
            BOOST_CHECK( it != query.end() );
            nvc255_value = it[1].AsString();
            BOOST_CHECK( nvc255_value.size() > 0);

            BOOST_CHECK_EQUAL( utf8_str_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_rus), NStr::PrintableString(nvc255_value) );

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
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
size_t
GetNumOfRecords(CQuery& query, const string& table_name)
{
    int cur_rec_num = 0;
    query.ClearParameters();
    query.SetSql("select count(*) FROM " + table_name);
    query.Execute();
    query.RequireRowCount(1);
    ITERATE(CQuery, it, query.SingleSet()) {
        cur_rec_num = it[1].AsInt4();
    }
    BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
    return cur_rec_num;
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Unicode)
{
    string table_name("#test_unicode_table");
    // string table_name("DBAPI_Sample..test_unicode_table");

    string sql;
    CQuery query = GetDatabase().NewQuery();
    string str_ger("Außerdem können Sie einzelne Einträge aus Ihrem "
                   "Suchprotokoll entfernen");

//     string str_utf8("\320\237\321\203\320\277\320\272\320\270\320\275");
    string str_utf8("\xD0\x9F\xD1\x83\xD0\xBF\xD0\xBA\xD0\xB8\xD0\xBD");
//     string str_utf8("HELLO");

    // Russian phrase in UTF8 encoding ...
    const unsigned char str_rus_utf8[] =
    {
        0xd0, 0x9c, 0xd0, 0xb0, 0xd0, 0xbc, 0xd0, 0xb0, 0x20, 0xd0, 0xbc, 0xd1,
        0x8b, 0xd0, 0xbb, 0xd0, 0xb0, 0x20, 0xd1, 0x80, 0xd0, 0xb0, 0xd0, 0xbc,
        0xd1, 0x83, 0x00
    };

    CStringUTF8 utf8_str_1252_rus(CUtf8::AsUTF8(reinterpret_cast<const char*>(str_rus_utf8), eEncoding_UTF8));
    CStringUTF8 utf8_str_1252_ger(CUtf8::AsUTF8(str_ger, eEncoding_Windows_1252));
    CStringUTF8 utf8_str_utf8(CUtf8::AsUTF8(str_utf8, eEncoding_UTF8));

    BOOST_CHECK( str_utf8.size() > 0 );
    BOOST_CHECK_EQUAL( NStr::PrintableString(str_utf8), NStr::PrintableString(utf8_str_utf8) );

    BOOST_CHECK( CUtf8::MatchEncoding( utf8_str_1252_rus, eEncoding_UTF8) );
    BOOST_CHECK( CUtf8::MatchEncoding( utf8_str_1252_ger, eEncoding_UTF8) );
    BOOST_CHECK( CUtf8::MatchEncoding( utf8_str_utf8, eEncoding_UTF8) );

    try {
        // Clean table ...
        {
            query.SetSql("DELETE FROM " + table_name);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert data ...
        {
            sql = "INSERT INTO " + table_name + "(nvc255_field) "
                  "VALUES(@nvc_val)";

            //
            BOOST_CHECK( utf8_str_utf8.size() > 0 );
            BOOST_CHECK_EQUAL( NStr::PrintableString(str_utf8), NStr::PrintableString(utf8_str_utf8) );
            query.SetParameter("@nvc_val", utf8_str_utf8);
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            //
            BOOST_CHECK( utf8_str_1252_rus.size() > 0 );
            query.SetParameter("@nvc_val", utf8_str_1252_rus);
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            //
            BOOST_CHECK( utf8_str_1252_ger.size() > 0 );
            query.SetParameter("@nvc_val", utf8_str_1252_ger);
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            query.ClearParameters();
        }

        // Retrieve data ...
        {
            string nvc255_value;

            sql  = " SELECT nvc255_field FROM " + table_name;
            sql += " ORDER BY id";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(3);

            BOOST_CHECK( query.HasMoreResultSets() );

            // Read utf8_str_utf8 ...
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );
            nvc255_value = it[1].AsString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_utf8.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_utf8), NStr::PrintableString(nvc255_value) );

            // Read utf8_str_1252_rus ...
            ++it;
            BOOST_CHECK( it != query.end() );
            nvc255_value = it[1].AsString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_1252_rus), NStr::PrintableString(nvc255_value) );

            // Read utf8_str_1252_ger ...
            ++it;
            BOOST_CHECK( it != query.end() );
            nvc255_value = it[1].AsString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_1252_ger), NStr::PrintableString(nvc255_value) );
            CStringUTF8 utf8_ger(CUtf8::AsUTF8(nvc255_value, eEncoding_UTF8));
            string value_ger =
                CUtf8::AsSingleByteString(utf8_ger, eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( NStr::PrintableString(str_ger), NStr::PrintableString(value_ger) );

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
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
BOOST_AUTO_TEST_CASE(Test_Insert)
{
    string sql;
    const string small_msg("%");
    const string test_msg(300, 'A');
    CQuery query = GetDatabase().NewQuery();

    try {
        // Clean table ...
        query.SetSql( "DELETE FROM " + GetTableName() );
        query.Execute();
        query.RequireRowCount(0);
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

        // Insert data ...
        {
            // Pure SQL ...
            query.SetSql(
                "INSERT INTO " + GetTableName() + "(int_field, vc1000_field) "
                "VALUES(1, '" + test_msg + "')"
                );
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Using parameters ...
            sql = "INSERT INTO " + GetTableName() +
                  "(int_field, vc1000_field) VALUES(@id, @vc_val)";

            query.SetParameter( "@id", 2 );
            query.SetParameter( "@vc_val", test_msg);
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            query.SetParameter( "@id", 3 );
            query.SetParameter( "@vc_val", small_msg);
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            enum { num_of_tests = 2 };

            sql  = " SELECT int_field, vc1000_field FROM " + GetTableName();
            sql += " ORDER BY int_field";

            query.ClearParameters();
            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(3);

            BOOST_CHECK( query.HasMoreResultSets() );

            CQuery::iterator it = query.begin();
            for(int i = 0; i < num_of_tests; ++i, ++it) {
                BOOST_CHECK( it != query.end() );

                string vc1000_value = it[2].AsString();

                BOOST_CHECK_EQUAL( test_msg.size(), vc1000_value.size() );
                BOOST_CHECK_EQUAL( test_msg, vc1000_value );
            }

            BOOST_CHECK( it != query.end() );
            string small_value1 = it[2].AsString();
            BOOST_CHECK_EQUAL( small_msg, small_value1 );

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
};

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_UNIQUE)
{
    string sql, value;

    try {
        CQuery query = GetDatabase().NewQuery();

        // Initialization ...
        {
            sql =
                "CREATE TABLE #test_unique ( \n"
                "   id INT , \n"
                "   unique_field UNIQUEIDENTIFIER DEFAULT NEWID() \n"
                ") \n";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            sql = "INSERT INTO #test_unique(id) VALUES(1)";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data ...
        {
            sql = "SELECT * FROM #test_unique";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(1);
            ITERATE(CQuery, it, query.SingleSet()) {
                value = it[2].AsString();
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Insert retrieved data back into the table ...
        {
            query.SetParameter( "@guid", value, eSDB_Binary );

            sql = "INSERT INTO #test_unique(id, unique_field) VALUES(2, @guid)";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }

        // Retrieve data again ...
        {
            sql = "SELECT * FROM #test_unique";

            query.SetSql(sql);
            query.Execute();
            query.RequireRowCount(2);
            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK( !it[2].IsNull() );
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Verify correct handling of usage errors.
BOOST_AUTO_TEST_CASE(Test_RequireRowCount)
{
    try {
        auto_ptr<CQuery> query(new CQuery(GetDatabase().NewQuery()));

        query->SetSql("select 1");
        // called too soon (before Execute)
        BOOST_CHECK_THROW(query->RequireRowCount(1), CSDB_Exception);
        query->Execute();
        query->RequireRowCount(0);
        BOOST_CHECK_THROW(query->begin(), CSDB_Exception);
        query->VerifyDone();

        query->Execute();
        query->RequireRowCount(2, kMax_Auto);
        query->begin();
        BOOST_CHECK_THROW(query->HasMoreResultSets(), CSDB_Exception);
        query->VerifyDone();

        query->SetSql("select 1 where 0=1");
        query->Execute();
        BOOST_CHECK_THROW(query->RequireRowCount(1, kMax_Auto); query->begin(),
                          CSDB_Exception);
        query->PurgeResults();

        // Call a procedure yielding two result sets -- the first with
        // one row, the second with more.
        if (GetArgs().GetServerType() == eSqlSrvMsSql) {
            query->SetSql("exec sp_help @objname='sp_helptext'");
        } else {
            query->SetSql("exec sp_helpuser @name_in_db='dbo'");
        }
        query->Execute();
        query->RequireRowCount(1); // correct only for the first result set
        query->MultiSet();
        BOOST_CHECK(query->HasMoreResultSets());
        query->begin();
        BOOST_CHECK(query->HasMoreResultSets());
        query->begin();
        BOOST_CHECK_THROW(query->HasMoreResultSets(), CSDB_Exception);
        query->VerifyDone();

        {{
            query->Execute();
            query->RequireRowCount(0, 2);
            CQuery::CRowIterator it = query->SingleSet().begin();
            ++it;
            BOOST_CHECK_THROW(query->HasMoreResultSets(), CSDB_Exception);
            query->VerifyDone();
        }}

        query->SetSql("select 1");
        query->Execute();
        query->RequireRowCount(2, kMax_Auto);
        ERR_POST(Warning
                 << "Set bad minimum count; expect a library warning.");
        BOOST_CHECK_NO_THROW(query.reset());
    } catch (const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    } 
}

///////////////////////////////////////////////////////////////////////////////
NCBITEST_INIT_TREE()
{
    NCBITEST_DEPENDS_ON(Test_GetRowCount,           Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_UnicodeNB,             Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Unicode,               Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_ClearParamList,        Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_SelectStmt2,           Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_NULL,                  Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Recordset,             Test_SelectStmt);
    NCBITEST_DEPENDS_ON(Test_SelectStmtXML,         Test_SelectStmt);
    NCBITEST_DEPENDS_ON(Test_Unicode_Simple,        Test_SelectStmtXML);
}


END_NCBI_SCOPE
