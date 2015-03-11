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

#include <corelib/ncbiargs.hpp>

#include <connect/ncbi_core_cxx.hpp>
#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_crypt.h>
#endif

#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include <dbapi/dbapi_variant_convert.hpp>
#include <dbapi/driver/dbapi_driver_convert.hpp>


BEGIN_NCBI_SCOPE

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
        BOOST_REQUIRE(rs != NULL);

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
        BOOST_REQUIRE(rs != NULL);

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
    string str_ger("Au\xdf" "erdem k\xf6nnen Sie einzelne Eintr\xe4ge aus "
                   "Ihrem Suchprotokoll entfernen");

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

            CStringUTF8 utf8_sql_ger(CUtf8::AsUTF8(sql_ger, eEncoding_Windows_1252));

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

            CStringUTF8 utf8_sql(CUtf8::AsUTF8(sql, eEncoding_Windows_1252));
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
                BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_ger), NStr::PrintableString(nvc255_value) );
                CStringUTF8 utf8_ger(CUtf8::AsUTF8(nvc255_value, eEncoding_UTF8));
                string value_ger =
                    CUtf8::AsSingleByteString(utf8_ger,eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( NStr::PrintableString(str_ger), NStr::PrintableString(value_ger) );

                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( utf8_str_rus.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_rus), NStr::PrintableString(nvc255_value) );
            } else {
                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( str_ger.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( NStr::PrintableString(str_ger), NStr::PrintableString(nvc255_value) );

                //
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK( nvc255_value.size() > 0);

                BOOST_CHECK_EQUAL( strlen(reinterpret_cast<const char*>(str_rus)), nvc255_value.size() );
                BOOST_CHECK_EQUAL( NStr::PrintableString(reinterpret_cast<const char*>(str_rus)), NStr::PrintableString(nvc255_value) );
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
    string str_ger("Au\xdf" "erdem k\xf6nnen Sie einzelne Eintr\xe4ge aus "
                   "Ihrem Suchprotokoll entfernen");

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

    BOOST_CHECK( CUtf8::MatchEncoding(utf8_str_1252_rus, eEncoding_UTF8) );
    BOOST_CHECK( CUtf8::MatchEncoding(utf8_str_1252_ger, eEncoding_UTF8) );
    BOOST_CHECK( CUtf8::MatchEncoding(utf8_str_utf8, eEncoding_UTF8) );

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
            BOOST_CHECK_EQUAL( NStr::PrintableString(str_utf8), NStr::PrintableString(utf8_str_utf8) );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_utf8.data(),
                                                   utf8_str_utf8.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_rus.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_rus.data(),
                                                   utf8_str_1252_rus.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_ger.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_ger.data(),
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
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_utf8), NStr::PrintableString(nvc255_value) );

            // Read utf8_str_1252_rus ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_1252_rus), NStr::PrintableString(nvc255_value) );

            // Read utf8_str_1252_ger ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( NStr::PrintableString(utf8_str_1252_ger), NStr::PrintableString(nvc255_value) );
            CStringUTF8 utf8_ger(CUtf8::AsUTF8(nvc255_value, eEncoding_UTF8));
            string value_ger =
                CUtf8::AsSingleByteString(utf8_ger,eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( NStr::PrintableString(str_ger), NStr::PrintableString(value_ger) );

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
            auto_stmt->SetParam( CVariant::LongChar(test_msg.data(),
                                                    test_msg.size()),
                                 "@vc_val"
                                 );
            auto_stmt->ExecuteUpdate( sql );

            auto_stmt->SetParam( CVariant( Int4(3) ), "@id" );
            auto_stmt->SetParam( CVariant::LongChar(small_msg.data(),
                                                    small_msg.size()),
                                 "@vc_val"
                                 );
            auto_stmt->ExecuteUpdate( sql );

            CVariant var_value(CVariant::LongChar(NULL, 1024));
            var_value = CVariant::LongChar(small_msg.data(), small_msg.size());
            auto_stmt->SetParam( CVariant( Int4(4) ), "@id" );
            auto_stmt->SetParam( CVariant::LongChar(small_msg.data(),
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
                DIAG_COMPILE_INFO,
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
            string sDbServer("");
            string sDbName("NCBI_LS");
            string sDbUser("");
            string sDbPasswd("");
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

                pDbLink->SetDatabaseName(sDbName);
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
            "",
            "",
    //         "MSSQL57",
            "",
            "NCBI_LS"
            );

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

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CVariantConvert)
{
    const Uint1 value_Uint1 = 1;
    const Int2 value_Int2 = 2;
    const Int4 value_Int4 = 4;
    const Int8 value_Int8 = 8;
    const float value_float = float(0.4);
    const double value_double = 0.8;
    const bool value_bool = false;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);

    Int8   Int8_value = 0;
    Uint8  Uint8_value = 0;
    Int4   Int4_value = 0;
    Uint4  Uint4_value = 0;
    Int2   Int2_value = 0;
    Uint2  Uint2_value = 0;
    Int1   Int1_value = 0;
    Uint1  Uint1_value = 0;
    float  float_value = 0.0;
    double double_value = 0.0;
    bool   bool_value = false;
    string string_value;

    try {
        {
            const CVariant variant_Int8( value_Int8 );

            Int8_value  = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int8_value, value_Int8 );
            Uint8_value = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int8(Uint8_value), value_Int8 );
            Int4_value  = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int4_value, value_Int8 );
            Uint4_value = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Uint4_value, value_Int8 );
            Int2_value  = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int2_value, value_Int8 );
            Uint2_value = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Uint2_value, value_Int8 );
            Int1_value  = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int1_value, value_Int8 );
            Uint1_value = Convert(variant_Int8);
            BOOST_CHECK_EQUAL( Int8(Uint1_value), value_Int8 );
            bool_value  = Convert(variant_Int8);

            string str_value = NCBI_CONVERT_TO(Convert(variant_Int8), string);
        }

        {
            const CVariant variant_Int4( value_Int4 );

            Int4_value = Convert(variant_Int4);
            BOOST_CHECK_EQUAL( Int4_value, value_Int4 );

            string str_value = NCBI_CONVERT_TO(Convert(variant_Int4), string);
        }

        {
            const CVariant variant_Int2( value_Int2 );

            Int2_value = Convert(variant_Int2);
            BOOST_CHECK_EQUAL( Int2_value, value_Int2 );

            string str_value = NCBI_CONVERT_TO(Convert(variant_Int2), string);
        }

        {
            const CVariant variant_Uint1( value_Uint1 );

            Uint1_value = Convert(variant_Uint1);
            BOOST_CHECK_EQUAL( Uint1_value, value_Uint1 );

            string str_value = NCBI_CONVERT_TO(Convert(variant_Uint1), string);
        }

        {
            const CVariant variant_float( value_float );

            float_value = Convert(variant_float);
            BOOST_CHECK_EQUAL( float_value, value_float );

            string str_value = NCBI_CONVERT_TO(Convert(variant_float), string);
        }

        {
            const CVariant variant_double( value_double );

            double_value = Convert(variant_double);
            BOOST_CHECK_EQUAL( double_value, value_double );

            string str_value = NCBI_CONVERT_TO(Convert(variant_double), string);
        }

        {
            const CVariant variant_bool( value_bool );

            bool_value = Convert(variant_bool);
            BOOST_CHECK_EQUAL( bool_value, value_bool );

            string str_value = NCBI_CONVERT_TO(Convert(variant_bool), string);
        }

        {
            const CVariant variant_string( value_string );

            string_value = Convert(variant_string).operator string();
            BOOST_CHECK_EQUAL( string_value, value_string );
        }

        {
            const CVariant variant_char( value_char );

            string_value = Convert(variant_char).operator string();
            BOOST_CHECK_EQUAL( string_value, value_char );
        }

        {
            const CVariant variant_CTimeShort( value_CTime, eShort );

            const CTime& CTime_value = Convert(variant_CTimeShort);
            BOOST_CHECK_EQUAL( CTime_value, value_CTime );
        }

        {
            const CVariant variant_CTimeLong( value_CTime, eLong );

            const CTime& CTime_value = Convert(variant_CTimeLong);
            BOOST_CHECK_EQUAL( CTime_value, value_CTime );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }

}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CVariantConvertSafe)
{
    const Uint1 value_Uint1 = 1;
    const Int2 value_Int2 = 2;
    const Int4 value_Int4 = 4;
    const Int8 value_Int8 = 8;
    const float value_float = float(0.4);
    const double value_double = 0.8;
    const bool value_bool = false;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);

    Int8   Int8_value = 0;
    Uint8  Uint8_value = 0;
    Int4   Int4_value = 0;
    // Uint4  Uint4_value = 0;
    Int2   Int2_value = 0;
    // Uint2  Uint2_value = 0;
    // Int1   Int1_value = 0;
    Uint1  Uint1_value = 0;
    float  float_value = 0.0;
    double double_value = 0.0;
    bool   bool_value = false;
    string string_value;

    try {
        {
            const CVariant variant_Int8( value_Int8 );

            Int8_value  = ConvertSafe(variant_Int8);
            BOOST_CHECK_EQUAL( Int8_value, value_Int8 );
            Uint8_value = ConvertSafe(variant_Int8);
            BOOST_CHECK_EQUAL( Uint8_value, value_Int8 );
            /*
            Int4_value  = ConvertSafe(variant_Int8);
            Uint4_value = ConvertSafe(variant_Int8);
            Int2_value  = ConvertSafe(variant_Int8);
            Uint2_value = ConvertSafe(variant_Int8);
            Int1_value  = ConvertSafe(variant_Int8);
            Uint1_value = ConvertSafe(variant_Int8);
            bool_value  = ConvertSafe(variant_Int8);
            */

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_Int8), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_Int8) );
        }

        {
            const CVariant variant_Int4( value_Int4 );

            Int4_value = ConvertSafe(variant_Int4);
            BOOST_CHECK_EQUAL( Int4_value, value_Int4 );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_Int4), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_Int4) );
        }

        {
            const CVariant variant_Int2( value_Int2 );

            Int2_value = ConvertSafe(variant_Int2);
            BOOST_CHECK_EQUAL( Int2_value, value_Int2 );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_Int2), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_Int2) );
        }

        {
            const CVariant variant_Uint1( value_Uint1 );

            Uint1_value = ConvertSafe(variant_Uint1);
            BOOST_CHECK_EQUAL( Uint1_value, value_Uint1 );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_Uint1), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_Uint1) );
        }

        {
            const CVariant variant_float( value_float );

            float_value = ConvertSafe(variant_float);
            BOOST_CHECK_EQUAL( float_value, value_float );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_float), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_float) );
        }

        {
            const CVariant variant_double( value_double );

            double_value = ConvertSafe(variant_double);
            BOOST_CHECK_EQUAL( double_value, value_double );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_double), string);
            BOOST_CHECK_EQUAL( str_value, NStr::NumericToString(value_double) );
        }

        {
            const CVariant variant_bool( value_bool );

            bool_value = ConvertSafe(variant_bool);
            BOOST_CHECK_EQUAL( bool_value, value_bool );

            string str_value = NCBI_CONVERT_TO(ConvertSafe(variant_bool), string);
            BOOST_CHECK_EQUAL( str_value, NStr::IntToString(value_bool) );
        }

        {
            const CVariant variant_string( value_string );

            string_value = ConvertSafe(variant_string).operator string();
            BOOST_CHECK_EQUAL( string_value, value_string );
        }

        {
            const CVariant variant_char( value_char );

            string_value = ConvertSafe(variant_char).operator string();
            BOOST_CHECK_EQUAL( string_value, value_char );
        }

        {
            const CVariant variant_CTimeShort( value_CTime, eShort );

            const CTime& CTime_value = ConvertSafe(variant_CTimeShort);
            BOOST_CHECK_EQUAL( CTime_value, value_CTime );
        }

        {
            const CVariant variant_CTimeLong( value_CTime, eLong );

            const CTime& CTime_value = ConvertSafe(variant_CTimeLong);
            BOOST_CHECK_EQUAL( CTime_value, value_CTime );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBResultConvert)
{
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
                    bool_value = Convert(*rs);
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
                    Uint1_value = Convert(*rs);
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
                    Int2_value = Convert(*rs);
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
                    Int4_value = Convert(*rs);
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
                        string string_value = Convert(*rs).operator string();
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
                        Uint4 value = Convert(*rs);
                        BOOST_CHECK_EQUAL(value, 2843113322u);
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
                    string string_value = Convert(*rs).operator string();
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
                    float_value = Convert(*rs);
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
                    double_value = Convert(*rs);
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
                    float_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs);
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
                    string string_value = Convert(*rs).operator string();
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
                    string string_value = Convert(*rs).operator string();
                    BOOST_CHECK_EQUAL(string_value, string("12345"));
                }
            }
        }
    }

    // Second test ...
    {
        if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
        {
            sql = "select 1, 2.0, '3'";

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
                    string value1 = Convert(*rs);
                    string value2 = Convert(*rs);
                    string value3 = Convert(*rs);
                }
            }
        }

        if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
        {
            sql = "select 1, 2.0, '3'";

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
                    int value1 = Convert(*rs);
                    float value2 = Convert(*rs);
                    string value3 = Convert(*rs);
                    value1 += value1;
                    value2 += value2;
                }
            }
        }

        if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
        {
            sql = "select 1, 2.0, '3'";

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
                    float value1 = Convert(*rs);
                    string value2 = Convert(*rs);
                    int value3 = Convert(*rs);
                    value1 += value1;
                    value3 += value3;
                }
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DoTest_CDBResultConvertSafe(const string& sql, const T& v)
{
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
            T value = ConvertSafe(*rs);
            BOOST_CHECK_EQUAL(value, v);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBResultConvertSafe)
{
    string sql;

    // First test ...
    {
        DoTest_CDBResultConvertSafe<bool>("select Convert(bit, 1)", true);
        DoTest_CDBResultConvertSafe<Uint1>("select Convert(tinyint, 1)", 1);
        DoTest_CDBResultConvertSafe<Int2>("select Convert(smallint, 1)", 1);
        DoTest_CDBResultConvertSafe<Int4>("select Convert(int, 1)", 1);
        DoTest_CDBResultConvertSafe<float>("select Convert(float(4), 1)", 1);
        DoTest_CDBResultConvertSafe<float>("select Convert(real, 1)", 1);
        DoTest_CDBResultConvertSafe<double>("select Convert(double precision, 1)", 1);

        // numeric
        /* Drivers do not support conversion from *nymeric* to string ...
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
                        string string_value = ConvertSafe(*rs).operator string();
                        BOOST_CHECK_EQUAL(string_value, "1");
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
                        string string_value = ConvertSafe(*rs).operator string();
                        BOOST_CHECK_EQUAL(string_value, "2843113322");
                    }
                }
            }
        }

        // decimal
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
                    string string_value = ConvertSafe(*rs).operator string();
                    BOOST_CHECK_EQUAL(string_value, "1");
                }
            }
        }
        */

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
                    const CTime& CTime_value = ConvertSafe(*rs);
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
                    const CTime& CTime_value = ConvertSafe(*rs);
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
                    string string_value = ConvertSafe(*rs);
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
                    string string_value = ConvertSafe(*rs);
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
                    string string_value = ConvertSafe(*rs);
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
                    string string_value = ConvertSafe(*rs);
                    BOOST_CHECK_EQUAL(string_value, string("12345"));
                }
            }
        }

        // binary
        if (GetArgs().GetDriverName() != odbc_driver)
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
                    string string_value = ConvertSafe(*rs);

                    if (GetArgs().GetDriverName() == dblib_driver) {
                        BOOST_CHECK_EQUAL(NStr::TruncateSpaces(string_value), string("12345"));
                    } else {
                        BOOST_CHECK_EQUAL(string_value, string("12345"));
                    }
                }
            }
        }

        // varbinary
        if (GetArgs().GetDriverName() != odbc_driver)
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
                    string string_value = ConvertSafe(*rs);
                    BOOST_CHECK_EQUAL(string_value, string("12345"));
                }
            }
        }

        // text
        /* Drivers do not allow convert TEXT and IMAGE into string ...
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
                    string string_value = ConvertSafe(*rs).operator string();
                    BOOST_CHECK_EQUAL(string_value, "12345");
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
                    string string_value = ConvertSafe(*rs).operator string();
                    BOOST_CHECK_EQUAL(string_value, "12345");
                }
            }
        }
        */
    }

    // Second test ...
    {
        if (GetArgs().GetDriverName() != dblib_driver)
        {
            sql = "select 1, 2.0, '3'";

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
                    int value1 = ConvertSafe(*rs);
                    Int8 value2 = ConvertSafe(*rs);
                    string value3 = ConvertSafe(*rs);
                    value1 += value1;
                    value2 += value2;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DoTest_CDBCmdConvertSafe(const string& sql, const T& v)
{
    // By pointer (acquire ownership) ...
    {
        T value = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
        BOOST_CHECK_EQUAL(value, v);
    }

    // By reference ...
    {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().GetCDB_Connection()->LangCmd(sql));

        T value = ConvertSafe(*cmd);
        BOOST_CHECK_EQUAL(value, v);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBCmdConvertSafe)
{
    if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer)) {
        DoTest_CDBCmdConvertSafe<Uint1>("select Convert(tinyint, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvertSafe<Int2>("select Convert(smallint, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvertSafe<Int4>("select 1, 2.0, '3'", 1);
        if (!((GetArgs().GetDriverName() == dblib_driver ||
                GetArgs().GetDriverName() == ftds_driver ||
                GetArgs().GetDriverName() == ctlib_driver)
            && GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            )
        {
            DoTest_CDBCmdConvertSafe<Int8>("select Convert(bigint, 1), 2.0, '3'", 1);
        }
        DoTest_CDBCmdConvertSafe<float>("select Convert(real, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvertSafe<double>("select Convert(float, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvertSafe<double>("select Convert(double precision, 1), 2.0, '3'", 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBCmdConvertSafe2)
{
    string sql;

    // LangCmd ...
    {
        // pair ...
        {
            sql = "select Convert(tinyint, 1), Convert(real, 2.0), '3'";

            pair<Uint1, float> value = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));

            BOOST_CHECK_EQUAL(value.first, 1);
            BOOST_CHECK_EQUAL(value.second, 2);
        }

        // vector ...
        {
            sql = "select * from sysusers";

            vector<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // set ...
        {
            sql = "select * from sysusers";

            set<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // stack ...
        {
            sql = "select * from sysusers";

            stack<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // map ...
        {
            sql = "select * from sysusers";

            map<Int4, Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);
        }
    }

    // RPC
    {
        // pair ...
        {
            sql = "sp_server_info";

            pair<Int4, string> value = ConvertSafe(GetConnection().GetCDB_Connection()->RPC(sql));

            BOOST_CHECK_EQUAL(value.first, 1);
            BOOST_CHECK_EQUAL(value.second, string("DBMS_NAME"));
        }

        // vector ...
        {
            sql = "sp_server_info";

            vector<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // set ...
        {
            sql = "sp_server_info";

            set<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // stack ...
        {
            sql = "sp_server_info";

            stack<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // map ...
        {
            sql = "sp_server_info";

            map<Int4, string> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }
    }


    // CursorCMD
    {
        // pair ...
        {
            sql = "select Convert(tinyint, 1), Convert(real, 2.0), '3'";

            pair<Uint1, float> value = ConvertSafe(GetConnection().GetCDB_Connection()->Cursor("c1", sql));

            BOOST_CHECK_EQUAL(value.first, 1);
            BOOST_CHECK_EQUAL(value.second, 2);
        }

        // vector ...
        {
            sql = "select * from sysusers";

            vector<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->Cursor("c2", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // set ...
        {
            sql = "select * from sysusers";

            set<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->Cursor("c3", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // stack ...
        {
            sql = "select * from sysusers";

            stack<Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->Cursor("c4", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // map ...
        {
            sql = "select * from sysusers";

            map<Int4, Int4> value_int = ConvertSafe(GetConnection().GetCDB_Connection()->Cursor("c5", sql));
            BOOST_CHECK(value_int.size() > 0);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBCmdConvert2)
{
    string sql;

#if !(defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550)
// WorkShop 5.5 seems to have problems with destroying of temporary objects.

    // LangCmd ...
    {
        // pair ...
        if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
        {
            sql = "select 1, 2.0, '3'";

            pair<Uint1, float> value1 = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK_EQUAL(value1.first, 1);
            BOOST_CHECK_EQUAL(value1.second, 2);

            pair<Int1, Int1> value2 = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK_EQUAL(value2.first, 1);
            BOOST_CHECK_EQUAL(value2.second, 2);
        }

        // vector ...
        {
            sql = "select * from sysusers";

            vector<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);

            vector<string> value_str = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_str.size() > 0);

            vector<float> value_float = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_float.size() > 0);
        }

        // set ...
        {
            sql = "select * from sysusers";

            set<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);

            set<string> value_str = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_str.size() > 0);

            set<float> value_float = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_float.size() > 0);
        }

        // stack ...
        {
            sql = "select * from sysusers";

            stack<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);

            stack<string> value_str = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_str.size() > 0);

            stack<float> value_float = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_float.size() > 0);
        }

        // map ...
        {
            sql = "select * from sysusers";

            map<Int4, string> value_int = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_int.size() > 0);

            map<string, string> value_str = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_str.size() > 0);

            map<float, string> value_float = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
            BOOST_CHECK(value_float.size() > 0);
        }
    }
#endif


    // RPC
    {
        // pair ...
        {
            sql = "sp_server_info";

            pair<Int4, string> value = Convert(GetConnection().GetCDB_Connection()->RPC(sql));

            BOOST_CHECK_EQUAL(value.first, 1);
            BOOST_CHECK_EQUAL(value.second, string("DBMS_NAME"));
        }

        // vector ...
        {
            sql = "sp_server_info";

            vector<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // set ...
        {
            sql = "sp_server_info";

            set<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // stack ...
        {
            sql = "sp_server_info";

            stack<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // map ...
        {
            sql = "sp_server_info";

            map<Int4, string> value_int = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_int.size() > 0);
        }
    }

    // CursorCMD
    {
        // pair ...
        {
            sql = "select Convert(tinyint, 1), Convert(real, 2.0), '3'";

            pair<Uint1, float> value = Convert(GetConnection().GetCDB_Connection()->Cursor("c11", sql));

            BOOST_CHECK_EQUAL(value.first, 1);
            BOOST_CHECK_EQUAL(value.second, 2);
        }

        // vector ...
        {
            sql = "select * from sysusers";

            vector<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->Cursor("c21", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // set ...
        {
            sql = "select * from sysusers";

            set<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->Cursor("c31", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // stack ...
        {
            sql = "select * from sysusers";

            stack<Int4> value_int = Convert(GetConnection().GetCDB_Connection()->Cursor("c41", sql));
            BOOST_CHECK(value_int.size() > 0);
        }

        // map ...
        {
            sql = "select * from sysusers";

            map<Int4, string> value_int = Convert(GetConnection().GetCDB_Connection()->Cursor("c51", sql));
            BOOST_CHECK(value_int.size() > 0);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
class CServerInfo2
{
public:
    CServerInfo2(const string& name, const string value)
    : m_Name(name)
    , m_Value(value)
    {
    }

private:
    string m_Name;
    string m_Value;
};

struct SServerInfo3
{
    Uint4 id;
    string name;
    string value;
};

namespace value_slice
{

template <typename CP>
class CMakeObject<CP, CServerInfo2, CDB_Result>
{
public:
    static CServerInfo2 Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;

        TResult result(source);

        return CServerInfo2(result, result);
    }
};

template <typename CP>
class CMakeObject<CP, SServerInfo3, CDB_Result>
{
public:
    static SServerInfo3 Make(CDB_Result& source)
    {
        typedef CValueConvert<CP, CDB_Result> TResult;
        SServerInfo3 res_val;

        TResult result(source);
        res_val.id = result;
        res_val.name = static_cast<string>(result);
        res_val.value = static_cast<string>(result);

        return res_val;
    }
};

}

////////////////////////////////////////////////////////////////////////////////
#if !(defined(NCBI_COMPILER_WORKSHOP) && NCBI_COMPILER_VERSION <= 550)
// WorkShop 5.5 seems to have problems with destroying of temporary objects.

BOOST_AUTO_TEST_CASE(Test_CDBCmdConvert3)
{
    string sql;

    if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
    {
        // vector ...
        {
            sql = "sp_server_info";

            vector<SServerInfo3> value = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value.size() > 0);

            vector<pair<Uint4, string> > value_pair = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair.size() > 0);

            vector<pair<Uint4, pair<string, string> > > value_pair2 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair2.size() > 0);

            vector<pair<Uint4, CServerInfo2> > value_pair3 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair3.size() > 0);

            vector<vector<string> > value_pair4 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair4.size() > 0);

            // Error. Recordset has only three columns.
            // vector<map<string, string> > value_pair5 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            // BOOST_CHECK(value_pair5.size() > 0);

            vector<map<int, map<string, string> > > value_pair6 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair6.size() > 0);
        }

        // map ...
        {
            sql = "sp_server_info";

            map<Int4, CServerInfo2> value = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value.size() > 0);

            map<pair<Uint4, string>, string> value_pair = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair.size() > 0);

            map<Int4, pair<string, string> > value_pair2 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair2.size() > 0);

            map<Int4, vector<string> > value_pair3 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair3.size() > 0);

            map<Int4, map<string, string> > value_pair4 = Convert(GetConnection().GetCDB_Connection()->RPC(sql));
            BOOST_CHECK(value_pair4.size() > 0);
        }
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DoTest_CDBCmdConvert(const string& sql, const T& v)
{
    // By pointer (acquire ownership) ...
    {
        T value = Convert(GetConnection().GetCDB_Connection()->LangCmd(sql));
        BOOST_CHECK_EQUAL(value, v);
    }

    // By reference ...
    {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().GetCDB_Connection()->LangCmd(sql));

        T value = Convert(*cmd);
        BOOST_CHECK_EQUAL(value, v);
    }

}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBCmdConvert)
{
    if (!(GetArgs().GetDriverName() == dblib_driver && GetArgs().GetServerType() == CDBConnParams::eMSSqlServer))
    {
        DoTest_CDBCmdConvert<Uint1>("select Convert(tinyint, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvert<Int2>("select Convert(smallint, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvert<Int4>("select 1, 2.0, '3'", 1);
        if (!((GetArgs().GetDriverName() == dblib_driver ||
                GetArgs().GetDriverName() == ftds_driver ||
                GetArgs().GetDriverName() == ctlib_driver)
            && GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            )
        {
            DoTest_CDBCmdConvert<Int8>("select Convert(bigint, 1), 2.0, '3'", 1);
        }
        DoTest_CDBCmdConvert<float>("select Convert(real, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvert<double>("select Convert(float, 1), 2.0, '3'", 1);
        DoTest_CDBCmdConvert<double>("select Convert(double precision, 1), 2.0, '3'", 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
inline
void DoTest_CDBObjectConvertSql(const string& sql, const T& v)
{
    T value = NCBI_CONVERT_TO(ConvertSQL(GetConnection().GetCDB_Connection()->LangCmd(sql)), T);
    BOOST_CHECK_EQUAL(value, v);
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBObjectConvertSql)
{
    string sql;

    sql = "SELECT null";

    DoTest_CDBObjectConvertSql<bool>(sql, bool());
    DoTest_CDBObjectConvertSql<Int1>(sql, 0);
    DoTest_CDBObjectConvertSql<Uint1>(sql, 0);
    DoTest_CDBObjectConvertSql<Int2>(sql, 0);
    DoTest_CDBObjectConvertSql<Uint2>(sql, 0);
    DoTest_CDBObjectConvertSql<Int4>(sql, 0);
    DoTest_CDBObjectConvertSql<Uint4>(sql, 0);
    DoTest_CDBObjectConvertSql<Int8>(sql, 0);
    DoTest_CDBObjectConvertSql<Uint8>(sql, 0);
    DoTest_CDBObjectConvertSql<float>(sql, 0.0);
    DoTest_CDBObjectConvertSql<double>(sql, 0.0);
    DoTest_CDBObjectConvertSql<string>(sql, kEmptyStr);
    DoTest_CDBObjectConvertSql<CTime>(sql, CTime());
}

////////////////////////////////////////////////////////////////////////////////
template <typename T>
inline
void DoTest_CDBObjectConvertSqlSafe(const string& sql, const T& v)
{
    T value = NCBI_CONVERT_TO(ConvertSQLSafe(GetConnection().GetCDB_Connection()->LangCmd(sql)), T);
    BOOST_CHECK_EQUAL(value, v);
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CDBObjectConvertSqlSafe)
{
    string sql;

    sql = "SELECT null";

    DoTest_CDBObjectConvertSqlSafe<bool>(sql, bool());
    DoTest_CDBObjectConvertSqlSafe<Uint1>(sql, 0);
    DoTest_CDBObjectConvertSqlSafe<Int2>(sql, 0);
    DoTest_CDBObjectConvertSqlSafe<Int4>(sql, 0);
    DoTest_CDBObjectConvertSqlSafe<Int8>(sql, 0);
    DoTest_CDBObjectConvertSqlSafe<float>(sql, 0.0);
    DoTest_CDBObjectConvertSqlSafe<double>(sql, 0.0);
    DoTest_CDBObjectConvertSqlSafe<string>(sql, kEmptyStr);
    DoTest_CDBObjectConvertSqlSafe<CTime>(sql, CTime());
}

////////////////////////////////////////////////////////////////////////////////
// Safe (compile-time) conversion ...
/*
template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SSafeCP, FROM>
ConvertSafeSQL(const FROM& value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, FROM>(value);
}

template <typename FROM>
inline
const value_slice::CValueConvert<value_slice::SSafeCP, FROM>
ConvertSafeSQL(FROM& value)
{
    return value_slice::CValueConvert<value_slice::SSafeCP, FROM>(value);
}


////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DoTest_DbapiConvertSafe(const string& sql, const T& v)
{
    // By pointer (acquire ownership) ...
    {
        T value = ConvertSafe(GetConnection().GetCDB_Connection()->LangCmd(sql));
        BOOST_CHECK_EQUAL(value, v);
    }

    // By reference ...
    {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().GetCDB_Connection()->LangCmd(sql));

        T value = ConvertSafe(*cmd);
        BOOST_CHECK_EQUAL(value, v);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DbapiConvertSafe)
{
    DoTest_DbapiConvertSafe<Uint1>("select Convert(tinyint, 1), 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<Int2>("select Convert(smallint, 1), 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<Int4>("select 1, 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<Int8>("select Convert(bigint, 1), 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<float>("select Convert(real, 1), 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<double>("select Convert(float, 1), 2.0, '3'", 1);
    DoTest_DbapiConvertSafe<double>("select Convert(double precision, 1), 2.0, '3'", 1);
}
*/

///////////////////////////////////////////////////////////////////////////////
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
    NCBITEST_DEPENDS_ON(Test_CloneConnection,       Test_ConnParamsDatabase);
}


END_NCBI_SCOPE
