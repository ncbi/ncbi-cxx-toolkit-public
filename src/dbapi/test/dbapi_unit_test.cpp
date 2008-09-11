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

#include <ncbi_pch.hpp>

#include <corelib/ncbiargs.hpp>
//#include <corelib/ncbithr.hpp>
#include <corelib/expr.hpp>

#include <connect/ncbi_core_cxx.hpp>
#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_crypt.h>
#endif

#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include "dbapi_unit_test.hpp"


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
inline
CDiagCompileInfo GetBlankCompileInfo(void)
{
    return CDiagCompileInfo();
}

///////////////////////////////////////////////////////////////////////////////
// Patterns to test:
//      I) Statement:
//          1) New/dedicated statement for each test
//          2) Reusable statement for all tests

///////////////////////////////////////////////////////////////////////////////
CTestTransaction::CTestTransaction(
    IConnection& conn,
    ETransBehavior tb
    )
    : m_Conn( &conn )
    , m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        auto_ptr<IStatement> stmt( GetConnection().GetStatement() );
        stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            auto_ptr<IStatement> stmt( GetConnection().GetStatement() );
            stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            auto_ptr<IStatement> stmt( GetConnection().GetStatement() );
            stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}


static AutoPtr<IConnection> s_Conn = NULL;

IConnection&
GetConnection(void)
{
    _ASSERT(s_Conn.get());
    return *s_Conn;
}


///////////////////////////////////////////////////////////////////////////////
NCBITEST_AUTO_INIT()
{
    if (!CommonInit())
        return;

    s_Conn.reset(GetDS().CreateConnection( CONN_OWNERSHIP ));
    _ASSERT(s_Conn.get());

    s_Conn->Connect(GetArgs().GetConnParams());

    auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

    // Create a test table ...
    string sql;

    sql  = " CREATE TABLE " + GetTableName() + "( \n";
    sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    int_field INT NULL, \n";
    sql += "    vc1000_field VARCHAR(1000) NULL, \n";
    sql += "    text_field TEXT NULL, \n";
    sql += "    image_field IMAGE NULL \n";
    sql += " )";

    // Create the table
    auto_stmt->ExecuteUpdate(sql);

    sql  = " CREATE UNIQUE INDEX #ind01 ON " + GetTableName() + "( id ) \n";

    // Create an index
    auto_stmt->ExecuteUpdate( sql );

    sql  = " CREATE TABLE #dbapi_bcp_table2 ( \n";
    sql += "    id INT NULL, \n";
    // Identity won't work with bulk insert ...
    // sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    int_field INT NULL, \n";
    sql += "    vc1000_field VARCHAR(1000) NULL, \n";
    sql += "    text_field TEXT NULL \n";
    sql += " )";

    auto_stmt->ExecuteUpdate( sql );

    sql  = " CREATE TABLE #test_unicode_table ( \n";
    sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    nvc255_field NVARCHAR(255) NULL \n";
//        sql += "    nvc255_field VARCHAR(255) NULL \n";
    sql += " )";

    // Create table
    auto_stmt->ExecuteUpdate(sql);
}

NCBITEST_AUTO_FINI()
{
//     I_DriverContext* drv_context = GetDS().GetDriverContext();
//
//     drv_context->PopDefConnMsgHandler( m_ErrHandler.get() );
//     drv_context->PopCntxMsgHandler( m_ErrHandler.get() );

    s_Conn.reset(NULL);
    CommonFini();
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Unicode_Simple)
{
    string sql;
    string str_value;

    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());
        IResultSet* rs;
        char buff[128];
        size_t read_bytes = 0;

        sql  = "select 1 as Tag, null as Parent, ";
        sql += "1 as 'Test!1!id' ";
        sql += "for xml explicit";

        rs = auto_stmt->ExecuteQuery(sql);
        BOOST_CHECK(rs != NULL);

//         if (rs->Next()) {
//             const CVariant& value = rs->GetVariant(1);
//             str_value = value.GetString();
//         }

        rs->DisableBind(true);
        if (rs->Next()) {
            read_bytes = rs->Read(buff, sizeof(buff));
            BOOST_CHECK_EQUAL(size_t(14), read_bytes);
            // cout << string(buff, read_bytes) << endl;
        }

        sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
        rs = auto_stmt->ExecuteQuery(sql);
        BOOST_CHECK(rs != NULL);

//         if (rs->Next()) {
//             const CVariant& value = rs->GetVariant(1);
//             str_value = value.GetString();
//             str_value = str_value;
//         }

        rs->DisableBind(true);
        if (rs->Next()) {
            read_bytes = rs->Read(buff, sizeof(buff));
            BOOST_CHECK_EQUAL(size_t(11), read_bytes);
            // cout << string(buff, read_bytes) << endl;
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
// Test unicode without binding values ...
BOOST_AUTO_TEST_CASE(Test_UnicodeNB)
{
    string table_name("#test_unicode_table");
    // string table_name("DBAPI_Sample..test_unicode_table");
    const bool isValueInUTF8 = true;

    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
    string str_ger("Außerdem können Sie einzelne Einträge aus Ihrem "
                   "Suchprotokoll entfernen");

    // Russian phrase in WINDOWS-1251 encoding ...
    const unsigned char str_rus[] =
    {
        0xcc, 0xe0, 0xec, 0xe0, 0x20, 0xec, 0xfb, 0xeb, 0xe0, 0x20, 0xf0, 0xe0,
        0xec, 0xf3, 0x00
    };

    // Russian phrase in UTF8 encoding ...
    const unsigned char str_rus_utf8[] =
    {
        0xd0, 0x9c, 0xd0, 0xb0, 0xd0, 0xbc, 0xd0, 0xb0, 0x20, 0xd0, 0xbc, 0xd1,
        0x8b, 0xd0, 0xbb, 0xd0, 0xb0, 0x20, 0xd1, 0x80, 0xd0, 0xb0, 0xd0, 0xbc,
        0xd1, 0x83, 0x00
    };

    const CStringUTF8 utf8_str_ger(str_ger, eEncoding_Windows_1252);
    const CStringUTF8 utf8_str_rus(reinterpret_cast<const char*>(str_rus_utf8), eEncoding_UTF8);

    try {
        // Create table ...
        if (false) {
            string sql =
                "CREATE TABLE " + table_name + "( \n"
                "    id NUMERIC(18, 0) IDENTITY PRIMARY KEY, \n"
                "    nvc255_field NVARCHAR(255) NULL \n"
                ") ";

            auto_stmt->ExecuteUpdate(sql);
        }

        // Clean table ...
        {
            auto_stmt->ExecuteUpdate("DELETE FROM " + table_name);
        }

        // Insert data ...

        // The code below actually works ...
        if (false) {
            string sql_ger = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + str_ger + "')";

            CStringUTF8 utf8_sql_ger(sql_ger, eEncoding_Windows_1252);

            BOOST_CHECK( utf8_sql_ger.size() > 0 );

            auto_stmt->ExecuteUpdate( utf8_sql_ger );
        }

        // That works either ...
        if (isValueInUTF8) {
            string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + utf8_str_ger + "')";
            // string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + utf8_str_ger + "')";
            auto_stmt->ExecuteUpdate( sql );

            sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + utf8_str_rus + "')";
            // sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + utf8_str_rus + "')";
            auto_stmt->ExecuteUpdate( sql );
        } else {
            string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + str_ger + "')";
            // string sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + str_ger + "')";
            auto_stmt->ExecuteUpdate( sql );

            sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES(N'" + reinterpret_cast<const char*>(str_rus) + "')";
            // sql = "INSERT INTO " + table_name + "(nvc255_field) VALUES('" + reinterpret_cast<const char*>(str_rus) + "')";
            auto_stmt->ExecuteUpdate( sql );
        }

        // Retrieve data ...
        {
            string nvc255_value;
            string sql;

            sql  = " SELECT nvc255_field FROM " + table_name;
            sql += " ORDER BY id";

            CStringUTF8 utf8_sql(sql, eEncoding_Windows_1252);
            auto_stmt->SendSql( utf8_sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            // Read ...
            if (isValueInUTF8) {
                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( utf8_str_ger.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( utf8_str_ger, nvc255_value );
                CStringUTF8 utf8_ger(nvc255_value, eEncoding_UTF8);
                string value_ger =
                    utf8_ger.AsSingleByteString(eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( str_ger, value_ger );

                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( utf8_str_rus.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( utf8_str_rus, nvc255_value );
            } else {
                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( str_ger.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( str_ger, nvc255_value );

                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( strlen(reinterpret_cast<const char*>(str_rus)), nvc255_value.size() );
                BOOST_CHECK_EQUAL( reinterpret_cast<const char*>(str_rus), nvc255_value );
            }

            DumpResults(auto_stmt.get());
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
size_t
GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
                const string& table_name)
{
    size_t cur_rec_num = 0;

    DumpResults(auto_stmt.get());
    // ClearParamList is necessary here ...
    auto_stmt->ClearParamList();
    auto_stmt->SendSql( "select count(*) FROM " + table_name );
    while (auto_stmt->HasMoreResults()) {
        if (auto_stmt->HasRows()) {
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            if (rs.get() != NULL) {
                while (rs->Next()) {
                    cur_rec_num = rs->GetVariant(1).GetInt4();
                }
            }
        }
    }

    return cur_rec_num;
}

size_t
GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt)
{
    size_t rec_num = 0;

    BOOST_CHECK(auto_stmt->HasMoreResults());
    BOOST_CHECK(auto_stmt->HasRows());
    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

    if (rs.get() != NULL) {

        while (rs->Next()) {
            ++rec_num;
        }
    }

    DumpResults(auto_stmt.get());

    return rec_num;
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Unicode)
{
    string table_name("#test_unicode_table");
    // string table_name("DBAPI_Sample..test_unicode_table");

    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
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

    CStringUTF8 utf8_str_1252_rus(reinterpret_cast<const char*>(str_rus_utf8), eEncoding_UTF8);
    CStringUTF8 utf8_str_1252_ger(str_ger, eEncoding_Windows_1252);
    CStringUTF8 utf8_str_utf8(str_utf8, eEncoding_UTF8);

    BOOST_CHECK( str_utf8.size() > 0 );
    BOOST_CHECK_EQUAL( str_utf8, utf8_str_utf8 );

    BOOST_CHECK( utf8_str_1252_rus.IsValid() );
    BOOST_CHECK( utf8_str_1252_ger.IsValid() );
    BOOST_CHECK( utf8_str_utf8.IsValid() );

    try {
        // Clean table ...
        {
            auto_stmt->ExecuteUpdate("DELETE FROM " + table_name);
        }

        // Insert data ...
        {
            sql = "INSERT INTO " + table_name + "(nvc255_field) "
                  "VALUES(@nvc_val)";

            //
            BOOST_CHECK( utf8_str_utf8.size() > 0 );
            BOOST_CHECK_EQUAL( str_utf8, utf8_str_utf8 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_utf8.c_str(),
                                                   utf8_str_utf8.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_rus.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_rus.c_str(),
                                                   utf8_str_1252_rus.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_ger.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_ger.c_str(),
                                                   utf8_str_1252_ger.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            auto_stmt->ClearParamList();
        }

        // Retrieve data ...
        {
            string nvc255_value;

            sql  = " SELECT nvc255_field FROM " + table_name;
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            // Read utf8_str_utf8 ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_utf8.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_utf8, nvc255_value );

            // Read utf8_str_1252_rus ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_1252_rus, nvc255_value );

            // Read utf8_str_1252_ger ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_1252_ger, nvc255_value );
            CStringUTF8 utf8_ger(nvc255_value, eEncoding_UTF8);
            string value_ger =
                utf8_ger.AsSingleByteString(eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( str_ger, value_ger );

            DumpResults(auto_stmt.get());
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
BOOST_AUTO_TEST_CASE(Test_Insert)
{
    string sql;
    const string small_msg("%");
    const string test_msg(300, 'A');
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

    try {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM " + GetTableName() );

        // Insert data ...
        {
            // Pure SQL ...
            auto_stmt->ExecuteUpdate(
                "INSERT INTO " + GetTableName() + "(int_field, vc1000_field) "
                "VALUES(1, '" + test_msg + "')"
                );

            // Using parameters ...
            string sql = "INSERT INTO " + GetTableName() +
                         "(int_field, vc1000_field) VALUES(@id, @vc_val)";

            auto_stmt->SetParam( CVariant( Int4(2) ), "@id" );
            auto_stmt->SetParam( CVariant::LongChar(test_msg.c_str(),
                                                    test_msg.size()),
                                 "@vc_val"
                                 );
            auto_stmt->ExecuteUpdate( sql );

            auto_stmt->SetParam( CVariant( Int4(3) ), "@id" );
            auto_stmt->SetParam( CVariant::LongChar(small_msg.c_str(),
                                                    small_msg.size()),
                                 "@vc_val"
                                 );
            auto_stmt->ExecuteUpdate( sql );

            CVariant var_value(CVariant::LongChar(NULL, 1024));
            var_value = CVariant::LongChar(small_msg.c_str(), small_msg.size());
            auto_stmt->SetParam( CVariant( Int4(4) ), "@id" );
            auto_stmt->SetParam( CVariant::LongChar(small_msg.c_str(),
                                                    small_msg.size()),
                                 "@vc_val"
                                 );
            auto_stmt->ExecuteUpdate( sql );
        }

        // Retrieve data ...
        {
            enum { num_of_tests = 2 };
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            sql  = " SELECT int_field, vc1000_field FROM " + GetTableName();
            sql += " ORDER BY int_field";

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            for(int i = 0; i < num_of_tests; ++i) {
                BOOST_CHECK( rs->Next() );

                string vc1000_value = rs->GetVariant(2).GetString();

                BOOST_CHECK_EQUAL( test_msg.size(), vc1000_value.size() );
                BOOST_CHECK_EQUAL( test_msg, vc1000_value );
            }

            BOOST_CHECK( rs->Next() );
            string small_value1 = rs->GetVariant(2).GetString();
            BOOST_CHECK_EQUAL( small_msg, small_value1 );

            BOOST_CHECK( rs->Next() );
            string small_value2 = rs->GetVariant(2).GetString();
            BOOST_CHECK_EQUAL( small_msg, small_value2 );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
};

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_UNIQUE)
{
    string sql;
    CVariant value(eDB_VarBinary, 16);

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Initialization ...
        {
            sql =
                "CREATE TABLE #test_unique ( \n"
                "   id INT , \n"
                "   unique_field UNIQUEIDENTIFIER DEFAULT NEWID() \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );

            sql = "INSERT INTO #test_unique(id) VALUES(1)";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Retrieve data ...
        {
            sql = "SELECT * FROM #test_unique";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    while ( rs->Next() ) {
                        value = rs->GetVariant(2);
                        string str_value = value.GetString();
                        string str_value2 = str_value;
                    }
                }
            }
        }

        // Insert retrieved data back into the table ...
        {
            auto_stmt->SetParam( value, "@guid" );

            sql = "INSERT INTO #test_unique(id, unique_field) VALUES(2, @guid)";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Retrieve data again ...
        {
            sql = "SELECT * FROM #test_unique";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    while ( rs->Next() ) {
                        const CVariant& cur_value = rs->GetVariant(2);

                        BOOST_CHECK( !cur_value.IsNull() );
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
void
DumpResults(IStatement* const stmt)
{
    while ( stmt->HasMoreResults() ) {
        if ( stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( stmt->GetResultSet() );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDB_Exception)
{
    const char* message = "Very dangerous message";
    const int msgnumber = 67890;

    try {
        {
            const char* server_name = "server_name";
            const char* user_name = "user_name";
            const int severity = 12345;

            CDB_Exception ex(
                GetBlankCompileInfo(),
                NULL,
                CDB_Exception::eMulti,
                message,
                eDiag_Trace,
                msgnumber);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            BOOST_CHECK_EQUAL(server_name, ex.GetServerName());
            BOOST_CHECK_EQUAL(user_name, ex.GetUserName());
            BOOST_CHECK_EQUAL(severity, ex.GetSybaseSeverity());

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Trace, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eMulti, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(server_name, auto_ex->GetServerName());
            BOOST_CHECK_EQUAL(user_name, auto_ex->GetUserName());
            BOOST_CHECK_EQUAL(severity, auto_ex->GetSybaseSeverity());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Trace, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eMulti, auto_ex->Type());
        }

        {
            CDB_DSEx ex(
                GetBlankCompileInfo(),
                NULL,
                message,
                eDiag_Fatal,
                msgnumber);

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Fatal, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eDS, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Fatal, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eDS, auto_ex->Type());
        }

        {
            const string proc_name("proc_name");
            const int proc_line = 12345;

            CDB_RPCEx ex(
                GetBlankCompileInfo(),
                NULL,
                message,
                eDiag_Critical,
                msgnumber,
                proc_name,
                proc_line);

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Critical, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eRPC, ex.Type());
            BOOST_CHECK_EQUAL(proc_name, ex.ProcName());
            BOOST_CHECK_EQUAL(proc_line, ex.ProcLine());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Critical, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eRPC, auto_ex->Type());
        }

        {
            const string sql_state("sql_state");
            const int batch_line = 12345;

            CDB_SQLEx ex(
                GetBlankCompileInfo(),
                NULL,
                message,
                eDiag_Error,
                msgnumber,
                sql_state,
                batch_line);

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eSQL, ex.GetErrCode());
            BOOST_CHECK_EQUAL(CDB_Exception::eSQL, ex.Type());
            BOOST_CHECK_EQUAL(sql_state, ex.SqlState());
            BOOST_CHECK_EQUAL(batch_line, ex.BatchLine());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eSQL, auto_ex->Type());
        }

        {
            CDB_DeadlockEx ex(
                GetBlankCompileInfo(),
                NULL,
                message);

            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eDeadlock, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eDeadlock, auto_ex->Type());
        }

        {
            CDB_TimeoutEx ex(
                GetBlankCompileInfo(),
                NULL,
                message,
                msgnumber);

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eTimeout, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eTimeout, auto_ex->Type());
        }

        {
            CDB_ClientEx ex(
                GetBlankCompileInfo(),
                NULL,
                message,
                eDiag_Warning,
                msgnumber);

            BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(message, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Warning, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eClient, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Warning, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eClient, auto_ex->Type());
        }

        {
            CDB_MultiEx ex(
                GetBlankCompileInfo(),
                NULL);

            BOOST_CHECK_EQUAL(0, ex.GetDBErrCode());
            BOOST_CHECK_EQUAL(eDiag_Info, ex.GetSeverity());
            BOOST_CHECK_EQUAL(kEmptyStr, ex.GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Info, ex.GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eMulti, ex.Type());

            //
            auto_ptr<CDB_Exception> auto_ex(ex.Clone());

            BOOST_CHECK_EQUAL(0, auto_ex->GetDBErrCode());
            BOOST_CHECK_EQUAL(eDiag_Info, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(kEmptyStr, auto_ex->GetMsg());
            BOOST_CHECK_EQUAL(eDiag_Info, auto_ex->GetSeverity());
            BOOST_CHECK_EQUAL(CDB_Exception::eMulti, auto_ex->Type());
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_NCBI_LS)
{
    try {
        if (true) {
            static unsigned int s_tTimeOut = 30; // in seconds
            CDB_Connection* pDbLink = NULL;

            string sDbDriver(GetArgs().GetDriverName());
    //         string sDbServer("MSSQL57");
            string sDbServer("mssql57.nac.ncbi.nlm.nih.gov");
            string sDbName("NCBI_LS");
            string sDbUser("anyone");
            string sDbPasswd("allowed");
            string sErr;

            try {
                I_DriverContext* drv_context = GetDS().GetDriverContext();

                if( drv_context == NULL ) {
                    BOOST_FAIL("FATAL: Unable to load context for dbdriver " +
                               sDbDriver);
                }
                drv_context->SetMaxTextImageSize( 0x7fffffff );
                drv_context->SetLoginTimeout( s_tTimeOut );
                drv_context->SetTimeout( s_tTimeOut );

                pDbLink = drv_context->Connect( sDbServer, sDbUser, sDbPasswd,
                                0, true, "Service" );

                string sUseDb = "use "+sDbName;
                auto_ptr<CDB_LangCmd> pUseCmd( pDbLink->LangCmd( sUseDb.c_str() ) );
                if( pUseCmd->Send() ) {
                    pUseCmd->DumpResults();
                } else {
                    BOOST_FAIL("Unable to send sql command "+ sUseDb);
                }
            } catch( CDB_Exception& dbe ) {
                sErr = "CDB Exception from "+dbe.OriginatedFrom()+":" +
                    dbe.SeverityString()+": " + dbe.Message();

                BOOST_FAIL(sErr);
            }


            CDB_Connection* m_Connect = pDbLink;

            // id(7777777) is going to expire soon ...
    //         int uid = 0;
    //         bool need_userInfo = true;
    //
    //         try {
    //             CDB_RPCCmd* r_cmd= m_Connect->RPC("getAcc4Sid", 2);
    //             CDB_Int id(7777777);
    //             CDB_TinyInt info(need_userInfo? 1 : 0);
    //
    //             r_cmd->SetParam("@sid", &id);
    //             r_cmd->SetParam("@need_info", &info);
    //
    //             r_cmd->Send();
    //
    //             CDB_Result* r;
    //             const char* res_name;
    //             while(r_cmd->HasMoreResults()) {
    //                 r= r_cmd->Result();
    //                 if(r == 0) continue;
    //                 if(r->ResultType() == eDB_RowResult) {
    //                     res_name= r->ItemName(0);
    //                     if(strstr(res_name, "uId") == res_name) {
    //                         while(r->Fetch()) {
    //                             r->GetItem(&id);
    //                         }
    //                         uid = id.Value();
    //                     }
    //                     else if((strstr(res_name, "login_name") == res_name)) {
    //                         CDB_LongChar name;
    //                         while(r->Fetch()) {
    //                             r->GetItem(&name); // login name
    //                             r->GetItem(&name); // user name
    //                             r->GetItem(&name); // first name
    //                             r->GetItem(&name); // last name
    //                             r->GetItem(&name); // affiliation
    //                             r->GetItem(&name); // question
    //                             r->GetItem(&info); // status
    //                             r->GetItem(&info); // role
    //                         }
    //                     }
    //                     else if((strstr(res_name, "gID") == res_name)) {
    //                         // int gID;
    //                         // int bID= 0;
    //                         CDB_VarChar label;
    //                         CDB_LongChar val;
    //
    //                         while(r->Fetch()) {
    //                             r->GetItem(&id); // gID
    //                             r->GetItem(&id); // bID
    //
    //                             r->GetItem(&info); // flag
    //                             r->GetItem(&label);
    //                             r->GetItem(&val);
    //
    //                         }
    //                     }
    //                     else if((strstr(res_name, "addr") == res_name)) {
    //                         CDB_LongChar addr;
    //                         CDB_LongChar comment(2048);
    //                         while(r->Fetch()) {
    //                             r->GetItem(&addr);
    //                             r->GetItem(&info);
    //                             r->GetItem(&comment);
    //                         }
    //                     }
    //                     else if((strstr(res_name, "num") == res_name)) {
    //                         CDB_LongChar num;
    //                         CDB_LongChar comment(2048);
    //                         while(r->Fetch()) {
    //                             r->GetItem(&num);
    //                             r->GetItem(&info);
    //                             r->GetItem(&comment);
    //                         }
    //                     }
    //                 }
    //                 delete r;
    //             }
    //             delete r_cmd;
    //         }
    //         catch (CDB_Exception& e) {
    //             CDB_UserHandler::GetDefault().HandleIt(&e);
    //         }

            // Find accounts
            int acc_num   = 0;
            CDB_Int       status;
            // Request
            CDB_VarChar   db_login( NULL, 64 );
            CDB_VarChar   db_title( NULL, 64 );
            CDB_VarChar   db_first( NULL, 127 );
            CDB_VarChar   db_last( NULL, 127 );
            CDB_VarChar   db_affil( NULL, 127 );
            // grp ids
            CDB_Int       db_gid1;
            CDB_LongChar  db_val1;
            CDB_VarChar   db_label1( NULL, 32 );
            CDB_Int       db_gid2;
            CDB_LongChar  db_val2;
            CDB_VarChar   db_label2( NULL, 32 );
            CDB_Int       db_gid3;
            CDB_LongChar  db_val3;
            CDB_VarChar   db_label3( NULL, 32 );
            // emails
            CDB_VarChar   db_email1( NULL, 255 );
            CDB_VarChar   db_email2( NULL, 255 );
            CDB_VarChar   db_email3( NULL, 255 );
            // phones
            CDB_VarChar   db_phone1( NULL, 20 );
            CDB_VarChar   db_phone2( NULL, 20 );
            CDB_VarChar   db_phone3( NULL, 20 );
            // Reply
            CDB_Int       db_uid;
            CDB_Int       db_score;

            auto_ptr<CDB_RPCCmd> pRpc( NULL );

    //         pRpc.reset( m_Connect->RPC("FindAccount",20) );
            pRpc.reset( m_Connect->RPC("FindAccountMatchAll") );

            db_login.SetValue( "lsroot" );
            // db_last.SetValue( "%" );

            pRpc->SetParam( "@login",  &db_login );
            pRpc->SetParam( "@title",   &db_title );
            pRpc->SetParam( "@first", &db_first );
            pRpc->SetParam( "@last", &db_last );
            pRpc->SetParam( "@affil", &db_affil );
            pRpc->SetParam( "@gid1", &db_gid1 );
            pRpc->SetParam( "@val1", &db_val1 );
            pRpc->SetParam( "@label1", &db_label1 );
            pRpc->SetParam( "@gid2", &db_gid2 );
            pRpc->SetParam( "@val2", &db_val2 );
            pRpc->SetParam( "@label2", &db_label2 );
            pRpc->SetParam( "@gid3",  &db_gid3 );
            pRpc->SetParam( "@val3", &db_val3 );
            pRpc->SetParam( "@label3", &db_label3 );
            pRpc->SetParam( "@email1", &db_email1 );
            pRpc->SetParam( "@email2", &db_email2 );
            pRpc->SetParam( "@email3", &db_email3 );
            pRpc->SetParam( "@phone1", &db_phone1 );
            pRpc->SetParam( "@phone2", &db_phone2 );
            pRpc->SetParam( "@phone3", &db_phone3 );
            bool bFetchErr = false;
            string sError;

            try {
                if( pRpc->Send() ) {
                    // Save results
                    while ( !bFetchErr && pRpc->HasMoreResults() ) {
                        auto_ptr<CDB_Result> pRes( pRpc->Result() );
                        if( !pRes.get() ) {
                            continue;
                        }
                        if( pRes->ResultType() == eDB_RowResult ) {
                            while( pRes->Fetch() ) {
                                ++acc_num;
                                pRes->GetItem( &db_uid );
                                        // cout << "uid=" << db_uid.Value();
                                pRes->GetItem( &db_score );
                                        // cout << " score=" << db_score.Value() << endl;
                            }
                        } else if( pRes->ResultType() == eDB_StatusResult ) {
                            while( pRes->Fetch() ) {
                                pRes->GetItem( &status );
                                if( status.Value() < 0 ) {
                                    sError = "Bad status value " +
                                        NStr::IntToString(status.Value())
                                    + " from RPC 'FindAccount'";
                                    bFetchErr = true;
                                    break;
                                }
                            }
                        }
                    }
                } else { // Error sending rpc
                    sError = "Error sending rpc 'FindAccount' to db server";
                    bFetchErr = true;
                }
            }  catch( CDB_Exception& dbe ) {
                sError = "CDB Exception from " + dbe.OriginatedFrom() + ":" +
                    dbe.SeverityString() + ": " + dbe.Message();
                bFetchErr = true;
            }

            BOOST_CHECK( acc_num > 0 );
            BOOST_CHECK_EQUAL( bFetchErr, false );
        }

        auto_ptr<IConnection> auto_conn( GetDS().CreateConnection() );
        BOOST_CHECK( auto_conn.get() != NULL );

        auto_conn->Connect(
            "anyone",
            "allowed",
    //         "MSSQL57",
            "mssql57.nac.ncbi.nlm.nih.gov",
            "NCBI_LS"
            );

        // Does not work with ftds_odbc ...
    //     {
    //         int sid = 0;
    //
    //         {
    //             auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );
    //
    //             auto_ptr<IResultSet> rs( auto_stmt->ExecuteQuery( "SELECT sID FROM Session" ) );
    //             BOOST_CHECK( rs.get() != NULL );
    //             BOOST_CHECK( rs->Next() );
    //             sid = rs->GetVariant(1).GetInt4();
    //             BOOST_CHECK( sid > 0 );
    //         }
    //
    //         {
    //             int num = 0;
    //             auto_ptr<ICallableStatement> auto_stmt( auto_conn->GetCallableStatement("getAcc4Sid", 2) );
    //
    //             auto_stmt->SetParam( CVariant(Int4(sid)), "@sid" );
    //             auto_stmt->SetParam( CVariant(Int2(1)), "@need_info" );
    //
    //             auto_stmt->Execute();
    //
    //             BOOST_CHECK(auto_stmt->HasMoreResults());
    //             BOOST_CHECK(auto_stmt->HasMoreResults());
    //             BOOST_CHECK(auto_stmt->HasMoreResults());
    //             BOOST_CHECK(auto_stmt->HasRows());
    //             auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //             BOOST_CHECK(rs.get() != NULL);
    //
    //             while (rs->Next()) {
    //                 BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
    //                 BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
    //                 BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), false);
    //                 BOOST_CHECK(rs->GetVariant(4).GetString().size() > 0);
    //                 // BOOST_CHECK(rs->GetVariant(5).GetString().size() > 0); // Stoped to work for some reason ...
    //                 ++num;
    //             }
    //
    //             BOOST_CHECK(num > 0);
    //
    // //             DumpResults(auto_stmt.get());
    //         }
    //     }

        //
        {
            auto_ptr<ICallableStatement> auto_stmt(
                auto_conn->GetCallableStatement("FindAccount")
                );

            auto_stmt->SetParam( CVariant(eDB_VarChar), "@login" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@title" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@first" );
            auto_stmt->SetParam( CVariant("a%"), "@last" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@affil" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid1" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label1" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid2" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label2" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid3" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone3" );

            auto_stmt->Execute();

            BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

            auto_stmt->Execute();

            BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );
        }

        //
        {
            auto_ptr<ICallableStatement> auto_stmt(
                auto_conn->GetCallableStatement("FindAccountMatchAll")
                );

            auto_stmt->SetParam( CVariant(eDB_VarChar), "@login" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@title" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@first" );
            auto_stmt->SetParam( CVariant("a%"), "@last" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@affil" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid1" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label1" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid2" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label2" );
            auto_stmt->SetParam( CVariant(eDB_Int), "@gid3" );
            auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@label3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@email3" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone1" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone2" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone3" );

            auto_stmt->Execute();

            BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

            auto_stmt->Execute();

            BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

            // Another set of values ....
            auto_stmt->SetParam( CVariant("lsroot"), "@login" );
            auto_stmt->SetParam( CVariant(eDB_VarChar), "@last" );

            auto_stmt->Execute();

            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt), size_t(1) );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Truncation)
{
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // There should be no problems with TDS version 7, 8, and 12.5
        {
            sql = "SELECT replicate('x', 255)+'y'" ;

            auto_stmt->SendSql( sql );
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());
            const CVariant& str_field = rs->GetVariant(1);
            BOOST_CHECK_EQUAL(str_field.GetString().size(), 256U);
            DumpResults(auto_stmt.get());
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DEPENDS_ON(Test_GetRowCount,           Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Cursor,                Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Cursor2,               Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Cursor_Param,          Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_Cursor_Multiple,       Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_LOB,                   Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_LOB2,                  Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_LOB_Multiple,          Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_LOB_Multiple_LowLevel, Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_BlobStream,            Test_Cursor);
    NCBITEST_DEPENDS_ON(Test_UnicodeNB,             Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_Unicode,               Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_ClearParamList,        Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_SelectStmt2,           Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_NULL,                  Test_StatementParameters);
    NCBITEST_DEPENDS_ON(Test_UserErrorHandler,      Test_Exception_Safety);
    NCBITEST_DEPENDS_ON(Test_Recordset,             Test_SelectStmt);
    NCBITEST_DEPENDS_ON(Test_ResultsetMetaData,     Test_SelectStmt);
    NCBITEST_DEPENDS_ON(Test_SelectStmtXML,         Test_SelectStmt);
    NCBITEST_DEPENDS_ON(Test_Unicode_Simple,        Test_SelectStmtXML);
    NCBITEST_DEPENDS_ON(Test_SetMaxTextImageSize,   Test_ResultsetMetaData);
}


END_NCBI_SCOPE
