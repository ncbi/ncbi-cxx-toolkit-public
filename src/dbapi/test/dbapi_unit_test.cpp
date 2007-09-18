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
#include <corelib/ncbithr.hpp>

#include <ncbi_source_ver.h>
#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/util/blobstore.hpp>

#include "dbapi_unit_test.hpp"
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include <common/test_assert.h>  /* This header must go last */


// #define DBAPI_BOOST_FAIL(ex)
//     ERR_POST(ex);
//     BOOST_FAIL(ex.GetMsg())

#define DBAPI_BOOST_FAIL(ex) \
    BOOST_FAIL(ex.GetMsg())

BEGIN_NCBI_SCOPE

enum { max_text_size = 8000 };
#define CONN_OWNERSHIP  eTakeOwnership
static const char* msg_record_expected = "Record expected";

inline
void PutMsgDisabled(const char* msg)
{
    LOG_POST(Warning << "- " << msg << " is disabled !!!");
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
        auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
        stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}

///////////////////////////////////////////////////////////////////////////////
class CIConnectionIFPolicy
{
protected:
    static void Destroy(IConnection* obj)
    {
        delete obj;
    }
    static IConnection* Init(IDataSource* ds)
    {
        return ds->CreateConnection( CONN_OWNERSHIP );
    }
};


///////////////////////////////////////////////////////////////////////////////
// void*
// GetAppSymbol(const char* symbol)
// {
// #ifdef WIN32
//     return ::GetProcAddress(::GetModuleHandle(NULL), symbol);
// #else
//     return ::dlsym(::dlopen(NULL, RTLD_LAZY), symbol);
// #endif
// }


///////////////////////////////////////////////////////////////////////////////
CDBAPIUnitTest::CDBAPIUnitTest(const CTestArguments& args)
: m_args(args)
, m_DM( CDriverManager::GetInstance() )
, m_DS( NULL )
, m_TableName( "#dbapi_unit_table" )
{
    // SetDiagFilter(eDiagFilter_Trace, "!/dbapi/driver/ctlib");
}

CDBAPIUnitTest::~CDBAPIUnitTest(void)
{
//     I_DriverContext* drv_context = m_DS->GetDriverContext();
//
//     drv_context->PopDefConnMsgHandler( m_ErrHandler.get() );
//     drv_context->PopCntxMsgHandler( m_ErrHandler.get() );

    m_Conn.reset(NULL);
    m_DM.DestroyDs(m_DS);
    m_DS = NULL;
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Connect(const auto_ptr<IConnection>& conn) const
{
    conn->Connect(
        m_args.GetUserName(),
        m_args.GetUserPassword(),
        m_args.GetServerName()
//             m_args.GetDatabaseName()
        );
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::TestInit(void)
{
    try {
        DBLB_INSTALL_DEFAULT();

        if ( m_args.GetServerType() == CTestArguments::eMsSql
             || m_args.GetServerType() == CTestArguments::eMsSql2005) {
            m_max_varchar_size = 8000;
        } else {
            // Sybase
            m_max_varchar_size = 1900;
        }

#ifdef USE_STATICALLY_LINKED_DRIVERS

#ifdef HAVE_LIBSYBASE
        DBAPI_RegisterDriver_CTLIB();
        DBAPI_RegisterDriver_DBLIB();
#endif
        DBAPI_RegisterDriver_FTDS();

#endif

        if (false) {
            // Two calls below will cause problems with the Sybase 12.5.1 client
            // (No problems with the Sybase 12.5.0 client)
            IDataSource* ds = m_DM.CreateDs(
                m_args.GetDriverName(),
                &m_args.GetDBParameters()
                );
            m_DM.DestroyDs(ds);
        }

        if (m_args.UseGateway()) {
            m_DS = m_DM.CreateDs(
                "gateway",
                &m_args.GetDBParameters()
                );
        } else {
            m_DS = m_DM.CreateDs(
                m_args.GetDriverName(),
                &m_args.GetDBParameters()
                );
        }

        I_DriverContext* drv_context = m_DS->GetDriverContext();

        if (m_args.GetDriverName() == "odbc" ||
            m_args.GetDriverName() == "odbcw" ||
            m_args.GetDriverName() == "ftds_odbc"
            ) {
            drv_context->PushCntxMsgHandler(
                new CDB_UserHandler_Exception_ODBC,
                eTakeOwnership
                );
            drv_context->PushDefConnMsgHandler(
                new CDB_UserHandler_Exception_ODBC,
                eTakeOwnership
                );
        } else {
            drv_context->PushCntxMsgHandler(
                new CDB_UserHandler_Exception,
                eTakeOwnership
                );
            drv_context->PushDefConnMsgHandler(
                new CDB_UserHandler_Exception,
                eTakeOwnership
                );
        }

        m_Conn.reset(m_DS->CreateConnection( CONN_OWNERSHIP ));
        BOOST_CHECK(m_Conn.get() != NULL);

        m_Conn->SetMode(IConnection::eBulkInsert);

        Connect(m_Conn);

        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

        // Create a test table ...
        string sql;

        sql  = " CREATE TABLE " + GetTableName() + "( \n";
        sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
        sql += "    int_field INT NULL, \n";
        sql += "    vc1000_field VARCHAR(1000) NULL, \n";
        sql += "    text_field TEXT NULL \n";
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

        // Table for bulk insert ...
        if ( m_args.GetServerType() == CTestArguments::eMsSql
             || m_args.GetServerType() == CTestArguments::eMsSql2005) {
            sql  = " CREATE TABLE #bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(8000) NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    bigint_field BIGINT NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);

            sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vb8000_field VARBINARY(8000) NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        } else
        {
            sql  = " CREATE TABLE #bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(1900) NULL, \n";
            sql += "    int_field INT NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);

            sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vb8000_field VARBINARY(1900) NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }

        sql  = " CREATE TABLE #test_unicode_table ( \n";
        sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
//         sql += "    nvc255_field NVARCHAR(255) NULL \n";
        sql += "    nvc255_field VARCHAR(255) NULL \n";
        sql += " )";

        // Create table
        auto_stmt->ExecuteUpdate(sql);
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CPluginManagerException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch (...) {
        BOOST_FAIL("Couldn't initialize the test-suite.");
    }
}

class CTestErrHandler : public CDB_UserHandler
{
public:
    // c-tor
    CTestErrHandler();
    // d-tor
    virtual ~CTestErrHandler();

public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);

    // Get current global "last-resort" error handler.
    // If not set, then the default will be "CDB_UserHandler_Default".
    // This handler is guaranteed to be valid up to the program termination,
    // and it will call the user-defined hs_Procandler last set by SetDefault().
    // NOTE:  never pass it to SetDefault, like:  "SetDefault(&GetDefault())"!
    static CDB_UserHandler& GetDefault(void);

    // Alternate the default global "last-resort" error handler.
    // Passing NULL will mean to ignore all errors that reach it.
    // Return previously set (or default-default if not set yet) handler.
    // The returned handler should be delete'd by the caller; the last set
    // handler will be delete'd automagically on the program termination.
    static CDB_UserHandler* SetDefault(CDB_UserHandler* h);

public:
    EDB_Severity getMaxSeverity() {
        return m_max_severity;
    }
    bool GetSucceed(void) const
    {
        return m_Succeed;
    }
    void Init(void)
    {
        m_Succeed = false;
    }

private:
    EDB_Severity    m_max_severity;
    bool            m_Succeed;
};

CTestErrHandler::CTestErrHandler()
: m_max_severity(eDB_Info)
, m_Succeed(false)
{
}

CTestErrHandler::~CTestErrHandler()
{
   ;
}

bool CTestErrHandler::HandleIt(CDB_Exception* ex)
{
    m_Succeed = true;

    if (ex && ex->Severity() > m_max_severity)
    {
        m_max_severity = ex->Severity();
    }

    // return false to find the next handler on the stack.
    // There is always one default stack

    return false;
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Unicode_Simple(void)
{
    string sql;
    string str_value;

    try {
        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());
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
void CDBAPIUnitTest::Test_Unicode(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    string str_rus("坚坚 假结 俞家");
    string str_ger("Au遝rdem k鰊nen Sie einzelne Eintr鋑e aus Ihrem "
                   "Suchprotokoll entfernen");

//     string str_utf8("\320\237\321\203\320\277\320\272\320\270\320\275");
    string str_utf8("\xD0\x9F\xD1\x83\xD0\xBF\xD0\xBA\xD0\xB8\xD0\xBD");
//     string str_utf8("HELLO");

    CStringUTF8 utf8_str_1252_rus(str_rus, eEncoding_Windows_1252);
    CStringUTF8 utf8_str_1252_ger(str_ger, eEncoding_Windows_1252);
    CStringUTF8 utf8_str_utf8(str_utf8, eEncoding_UTF8);

    BOOST_CHECK( str_utf8.size() > 0 );
    BOOST_CHECK_EQUAL( str_utf8, utf8_str_utf8 );

    BOOST_CHECK( utf8_str_1252_rus.IsValid() );
    BOOST_CHECK( utf8_str_1252_ger.IsValid() );
    BOOST_CHECK( utf8_str_utf8.IsValid() );

    try {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #test_unicode_table" );

        // Insert data ...
        {
            sql = "INSERT INTO #test_unicode_table(nvc255_field) "
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
        }

        // Retrieve data ...
        {
            string nvc255_value;

            sql  = " SELECT nvc255_field FROM #test_unicode_table";
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
            CStringUTF8 utf8_utf8(nvc255_value, eEncoding_UTF8);

            // Read utf8_str_1252_rus ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_1252_rus, nvc255_value );
            CStringUTF8 utf8_rus(nvc255_value, eEncoding_UTF8);
            string value_rus =
                utf8_rus.AsSingleByteString(eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( str_rus, value_rus );

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

        // Use permanent table ...
        if (false) {
            auto_stmt->ExecuteUpdate( "DELETE FROM DBAPI_Sample.."
                                      "test_nstring_table" );

            // Insert data ...
            {
                sql = "INSERT INTO DBAPI_Sample..test_nstring_table"
                    "(first_field) VALUES(@val)";

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_rus.c_str(),
                                                       utf8_str_1252_rus.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_ger.c_str(),
                                                       utf8_str_1252_ger.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_utf8.c_str(),
                                                       utf8_str_utf8.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);
            }

            // Retrieve data ....
            {
                string nvc255_value;

                sql  = " SELECT first_field FROM DBAPI_Sample..test_nstring_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                // Read utf8_str_1252_rus ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL(utf8_str_1252_rus.size(),
                                  nvc255_value.size()
                                  );
                BOOST_CHECK_EQUAL(utf8_str_1252_rus, nvc255_value);
                CStringUTF8 utf8_rus(nvc255_value, eEncoding_UTF8);
                string value_rus =
                    utf8_rus.AsSingleByteString(eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( str_rus, value_rus );

                // Read utf8_str_1252_ger ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(),
                                   nvc255_value.size()
                                   );
                BOOST_CHECK_EQUAL( utf8_str_1252_ger, nvc255_value );
                CStringUTF8 utf8_ger(nvc255_value, eEncoding_UTF8);
                string value_ger =
                    utf8_ger.AsSingleByteString(eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( str_ger, value_ger );

                // Read utf8_str_utf8 ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL( utf8_str_utf8.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( utf8_str_utf8, nvc255_value );
                CStringUTF8 utf8_utf8(nvc255_value, eEncoding_UTF8);

                DumpResults(auto_stmt.get());
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
void CDBAPIUnitTest::Test_NVARCHAR(void)
{
    string sql;

    try {
        if (false) {
            string Title69;

            auto_ptr<IConnection> conn(
                m_DS->CreateConnection(CONN_OWNERSHIP)
                );

            conn->Connect("anyone","allowed","MSSQL69", "PMC3");
//             conn->Connect("anyone","allowed","MSSQL42", "PMC3");
            auto_ptr<IStatement> auto_stmt(conn->GetStatement());

            sql = "SELECT Title from Subject where SubjectId=7";
            auto_stmt->Execute(sql);
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


// Based on Soussov's API.
void CDBAPIUnitTest::Test_Iskhakov(void)
{
    string sql;

    try {
        auto_ptr<CDB_Connection> auto_conn(
            m_DS->GetDriverContext()->Connect(
                "LINK_OS",
                "anyone",
                "allowed",
                0)
            );
        BOOST_CHECK( auto_conn.get() != NULL );

        sql  = "get_iodesc_for_link pubmed_pubmed";

        auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
        BOOST_CHECK( auto_stmt.get() != NULL );

        bool rc = auto_stmt->Send();
        BOOST_CHECK( rc );

        auto_ptr<I_ITDescriptor> descr;

        while(auto_stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(auto_stmt->Result());

            if (rs.get() == NULL) {
                continue;
            }

            if (rs->ResultType() != eDB_RowResult) {
                continue;
            }

            while(rs->Fetch()) {
                rs->ReadItem(NULL, 0);

                descr.reset(rs->GetImageOrTextDescriptor());
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Create_Destroy(void)
{
    try {
        ///////////////////////
        // CreateStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);
        }

        ///////////////////////
        // GetStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void CDBAPIUnitTest::Test_Multiple_Close(void)
{
    try {
        ///////////////////////
        // CreateStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }

        // Do not destroy a statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }

        ///////////////////////
        // GetStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }

        // Do not destroy a statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                m_DS->CreateConnection( CONN_OWNERSHIP )
                );
            Connect(local_conn);

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_HasMoreResults(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // First test ...
    // This test shouldn't throw.
    // Sergey Koshelkov complained that this code throws an exception with
    // info-level severity.
    // ftds (windows):  HasMoreResults() never returns;
    // odbs (windows):  HasMoreResults() throws exception:
    try {
        auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            }
        }
    }
    catch( const CDB_Exception& ex ) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(...) {
        BOOST_FAIL("Unknown error in Test_HasMoreResults");
    }

    // Second test ...
    {
        try {
            auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                }
            }
        }
        catch( const CDB_Exception& ex ) {
            DBAPI_BOOST_FAIL(ex);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Insert(void)
{
    string sql;
    const string small_msg("%");
    const string test_msg(300, 'A');
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

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
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

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
void
CDBAPIUnitTest::Test_DateTime(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
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

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
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

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
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
                        m_Conn->GetCallableStatement("sp_test_datetime", 1)
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

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
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

                    // Failed for some reason ...
                    // BOOST_CHECK(dt_value2 == dt_value);
                }

                // Insert NULL data using parameters ...
                {
                    auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                    auto_ptr<ICallableStatement> call_auto_stmt(
                        m_Conn->GetCallableStatement("sp_test_datetime", 1)
                        );

                    call_auto_stmt->SetParam( null_date, "@dt_val" );
                    call_auto_stmt->Execute();
                    DumpResults(call_auto_stmt.get());
                }

                // Retrieve data ...
                {
                    sql = "SELECT * FROM #test_datetime";

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
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
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_DateTimeBCP(void)
{
    string table_name("#test_bcp_datetime");
    // string table_name("DBAPI_Sample..test_bcp_datetime");
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    CVariant value(eDB_DateTime);
    CTime t;
    CVariant null_date(t, eLong);
    CTime dt_value;

    try {
        // Initialization ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "   id INT, \n"
                "   dt_field DATETIME NULL \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data using BCP ...
        {
            value = CTime(CTime::eCurrent);

            // Insert data using parameters ...
            {
                CVariant col1(eDB_Int);
                CVariant col2(eDB_DateTime);

                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name);

                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert(table_name, 2)
                    );

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                col1 = 1;
                col2 = value;

                bi->AddRow();
                bi->Complete();

                auto_stmt->ExecuteUpdate("SELECT * FROM " + table_name);
                int nRows = auto_stmt->GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
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

                // Failed for some reason ...
                // BOOST_CHECK(dt_value2 == dt_value);
            }

            // Insert NULL data using parameters ...
            {
                CVariant col1(eDB_Int);
                CVariant col2(eDB_DateTime);

                auto_stmt->ExecuteUpdate("DELETE FROM " + table_name);

                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert(table_name, 2)
                    );

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                col1 = 1;

                bi->AddRow();
                bi->Complete();

                auto_stmt->ExecuteUpdate("SELECT * FROM " + table_name);
                int nRows = auto_stmt->GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM " + table_name;

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
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
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_UNIQUE(void)
{
    string sql;
    CVariant value(eDB_VarBinary, 16);

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

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
CDBAPIUnitTest::Test_LOB(void)
{
    static char clob_value[] = "1234567890";
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Prepare data ...
        {
            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            auto_stmt->ExecuteUpdate( sql );

            sql  = " SELECT text_field FROM " + GetTableName();
            // sql += " FOR UPDATE OF text_field";

            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& out = auto_cursor->GetBlobOStream(1,
                                                           sizeof(clob_value) - 1,
                                                           eDisableLog);
                out.write(clob_value, sizeof(clob_value) - 1);
                out.flush();
            }

    //         auto_stmt->SendSql( sql );
    //         while( auto_stmt->HasMoreResults() ) {
    //             if( auto_stmt->HasRows() ) {
    //                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //                 while ( rs->Next() ) {
    //                     ostream& out = rs->GetBlobOStream(sizeof(clob_value) - 1, eDisableLog);
    //                     out.write(clob_value, sizeof(clob_value) - 1);
    //                     out.flush();
    //                 }
    //             }
    //         }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    rs->BindBlobToVariant(true);

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);


                        BOOST_CHECK( !value.IsNull() );

                        size_t blob_size = value.GetBlobSize();
                        BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
                        // Int8 value = rs->GetVariant(1).GetInt8();
                    }
                }
            }
        }
        
        // Read blob via Read method ...
        {
            string result;
            char buff[3];

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    rs->BindBlobToVariant(true);

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);


                        BOOST_CHECK( !value.IsNull() );
                        size_t read_bytes = 0;
                        while (read_bytes = value.Read(buff, sizeof(buff))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK_EQUAL(result, string(clob_value));
                    }
                }
            }
        }
        
        // Read Blob
        // There is a bug in implementation of ReadItem in ftds8 driver.
        if (m_args.GetDriverName() != "ftds8") {
            string result;
            char buff[3];

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_ptr<CDB_LangCmd> auto_stmt(m_Conn->GetCDB_Connection()->LangCmd(sql));

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );

            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    size_t read_bytes = 0;
                    bool is_null = true;

                    while (read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null)) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, string(clob_value));
                }

            }
        } else {
            PutMsgDisabled("Test_LOB - Read Blob");
        }

        // Test NULL values ...
        {
            enum {rec_num = 10};

            // Insert records ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    auto_stmt->SetParam(CVariant( Int4(ind) ), "@int_field");

                    if (ind % 2 == 0) {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, '')";
                    } else {
                        sql  = " INSERT INTO " + GetTableName() +
                            "(int_field, text_field) VALUES(@int_field, NULL)";
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate(sql);

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
                    auto_stmt->ClearParamList();

                    if (false) {
                        // Use cursor with parameters.
                        // Unfortunately that doesn't work with the new ftds
                        // driver ....
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = @int_field ";

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));
                            auto_cursor->SetParam(CVariant(Int4(ind)), "@int_field");

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        sizeof(clob_value) - 1,
                                        eDisableLog);
                                out.write(clob_value, sizeof(clob_value) - 1);
                                out.flush();
                            }
                        }
                    } else {
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = " + NStr::IntToString(ind);

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        sizeof(clob_value) - 1,
                                        eDisableLog);
                                out.write(clob_value, sizeof(clob_value) - 1);
                                out.flush();
                            }
                        }
                    }
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(int(rec_num),
                                  GetNumOfRecords(auto_stmt, GetTableName())
                                  );
            }
            
            // Read blob via Read method ...
            // ftds_odbc is hanging up ...
            if (m_args.GetDriverName() != "ftds_odbc") {
                char buff[3];

                sql = "SELECT text_field FROM "+ GetTableName();
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                        rs->BindBlobToVariant(true);

                        for (long ind = 0; ind < rec_num; ++ind) {
                            BOOST_CHECK(rs->Next());

                            const CVariant& value = rs->GetVariant(1);
                            string result;

                            if (ind % 2 == 0) {
                                BOOST_CHECK(!value.IsNull());
                                size_t read_bytes = 0;
                                while (read_bytes = value.Read(buff, sizeof(buff))) {
                                    result += string(buff, read_bytes);
                                }

                                BOOST_CHECK_EQUAL(result, string(clob_value));
                            } else {
                                BOOST_CHECK(value.IsNull());
                            }
                        }
                    }
                }
            } else {
                PutMsgDisabled("Test_LOB Read blob via Read method");
            }

            // Read Blob
            if (m_args.GetDriverName() != "ftds8") {
                char buff[3];

                sql = "SELECT text_field FROM "+ GetTableName();

                auto_ptr<CDB_LangCmd> auto_stmt(m_Conn->GetCDB_Connection()->LangCmd(sql));

                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while(auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL) {
                        continue;
                    }

                    if (rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    for (long ind = 0; ind < rec_num; ++ind) {
                        BOOST_CHECK(rs->Fetch());
                        string result;

                        if (ind % 2 == 0) {
                            size_t read_bytes = 0;
                            bool is_null = true;

                            while (read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null)) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, string(clob_value));
                        } else {
                            size_t read_bytes = 0;
                            bool is_null = true;

                            while (read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null)) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(is_null);
                        }
                    }

                }
            } else {
                PutMsgDisabled("Test_LOB Read Read Blob");
            } // Read Blob
        } // Test NULL values ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_LOB2(void)
{
    static char clob_value[] = "1234567890";
    string sql;
    enum {num_of_records = 10};

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Prepare data ...
        {
            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
                sql += " VALUES(0, '')";

                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
            }

            // Update CLOB value.
            {
                sql  = " SELECT text_field FROM " + GetTableName();
                // sql += " FOR UPDATE OF text_field";

                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                while(blobRs->Next()) {
                    ostream& out =
                        auto_cursor->GetBlobOStream(1,
                                                    sizeof(clob_value) - 1,
                                                    eDisableLog
                                                    );
                    out.write(clob_value, sizeof(clob_value) - 1);
                    out.flush();
                }
            }
        }

        // Retrieve data ...
        {
            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    rs->BindBlobToVariant(true);

                    while ( rs->Next() ) {
                        const CVariant& value = rs->GetVariant(1);

                        BOOST_CHECK( !value.IsNull() );

                        size_t blob_size = value.GetBlobSize();
                        BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
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
void
CDBAPIUnitTest::Test_LOB_LowLevel(void)
{
    string sql;
    CDB_Text txt;

    try {
        bool rc = false;
        auto_ptr<CDB_LangCmd> auto_stmt;
        CDB_Connection* conn = m_Conn->GetCDB_Connection();
        BOOST_CHECK(conn != NULL);

        // Clean table ...
        {
            sql = "DELETE FROM " + GetTableName();
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);
            auto_stmt->DumpResults();
        }

        // Create descriptor ...
        {
            sql = "INSERT INTO " + GetTableName() + "(text_field, int_field) VALUES('', 1)";
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);
            auto_stmt->DumpResults();
        }

        // Retrieve descriptor ...
        auto_ptr<I_ITDescriptor> descr;
        {
            sql = "SELECT text_field FROM " + GetTableName();
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);

            // Retrieve descriptor ...
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    rs->ReadItem(NULL, 0);
                    descr.reset(rs->GetImageOrTextDescriptor());
                }
            }

            BOOST_CHECK(descr.get() != NULL);
        }

        // Send data ...
        {
            txt.Append("test clob data ...");
            conn->SendData(*descr, txt);
        }

        // Use CDB_ITDescriptor explicitely ..,
        {
            CDB_ITDescriptor descriptor(GetTableName(), "text_field", "int_field = 1");
            CDB_Text text;

            text.Append("test clob data ...");
            conn->SendData(descriptor, text);
        }

        // Create relatively big CLOB ...
        {
            CDB_ITDescriptor descriptor(GetTableName(), "text_field", "int_field = 1");
            CDB_Text text;

            for(int i = 0; i < 1000; ++i) {
                string str_value;

                str_value += "A quick brown fox jumps over the lazy dog, message #";
                str_value += NStr::IntToString(i);
                str_value += "\n";
                text.Append(str_value);
            }
        }

        // Read CLOB value back ...
        {
            sql = "SELECT text_field FROM " + GetTableName() + " WHERE int_field = 1";
            auto_stmt.reset(conn->LangCmd(sql));
            rc = auto_stmt->Send();
            BOOST_CHECK(rc);

            // Retrieve descriptor ...
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                char buff[4096];
                while(rs->Fetch()) {
                    if (rs->ReadItem(buff, sizeof(buff)) < sizeof(buff)) {
                        break;
                    };
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_BulkInsertBlob(void)
{
    string sql;
    enum { record_num = 100 };
    // string table_name = GetTableName();
    string table_name = "#dbapi_bcp_table2";

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // First test ...
        {
            // Prepare data ...
            {
                // Clean table ...
                auto_stmt->ExecuteUpdate("DELETE FROM "+ table_name);
            }

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(m_Conn->CreateBulkInsert(table_name, 4));
                // auto_ptr<IBulkInsert> bi(m_Conn->CreateBulkInsert(table_name, 1));
                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);
                CVariant col3(eDB_VarChar);
                CVariant col4(eDB_Text);
                // CVariant col4(eDB_VarChar);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);
                bi->Bind(4, &col4);

                for( int i = 0; i < record_num; ++i ) {
                    string im = NStr::IntToString(i);
                    col1 = i;
                    col2 = i;
                    col3 = im;
                    // col4 = im;
                    col4.Truncate();
                    col4.Append(im.c_str(), im.size());

                    bi->AddRow();
                }

                // bi->StoreBatch();
                bi->Complete();
            }

            // Check inserted data ...
            {
                int rec_num = GetNumOfRecords(auto_stmt, table_name);
                BOOST_CHECK_EQUAL(rec_num, (int)record_num);
            }
        }

        // Second test ...
        {
            enum { batch_num = 10 };
            // Prepare data ...
            {
                // Clean table ...
                auto_stmt->ExecuteUpdate("DELETE FROM "+ table_name);
            }

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(m_Conn->CreateBulkInsert(table_name, 4));
                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);
                CVariant col3(eDB_VarChar);
                CVariant col4(eDB_Text);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);
                bi->Bind(4, &col4);

                for (int j = 0; j < batch_num; ++j) {
                    for( int i = 0; i < record_num; ++i ) {
                        string im = NStr::IntToString(i);
                        col1 = i;
                        col2 = i;
                        col3 = im;
                        col4.Truncate();
                        col4.Append(im.c_str(), im.size());
                        bi->AddRow();
                    }

                    bi->StoreBatch();
                }

                bi->Complete();
            }

            // Check inserted data ...
            {
                int rec_num = GetNumOfRecords(auto_stmt, table_name);
                BOOST_CHECK_EQUAL(int(rec_num), int(record_num * batch_num));
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_BulkInsertBlob_LowLevel(void)
{
    string sql;
    // enum { record_num = 10 };
    string table_name = "#bcp_table1";
    const string data = "testing";

    try {
        CDB_Connection* conn = m_Conn->GetCDB_Connection();

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    geneId INT NOT NULL, \n";
            sql += "    dataText TEXT NULL \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert data ...
        {
            auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name, 2));

            CDB_Int geneIdVal;
            CDB_Text dataTextVal;

            bcp->Bind(0, &geneIdVal);
            bcp->Bind(1, &dataTextVal);

            geneIdVal = 2;

            dataTextVal.Append(data);
            dataTextVal.MoveTo(0);

            bcp->SendRow();
            bcp->CompleteBCP();
        }
        
        // Retrieve data back ...
        {
            string result;
            char buff[64];
            int rec_num = 0;

            sql = "SELECT dataText FROM "+ table_name;

            auto_ptr<CDB_LangCmd> auto_stmt(m_Conn->GetCDB_Connection()->LangCmd(sql));

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );

            while(auto_stmt->HasMoreResults()) {
                auto_ptr<CDB_Result> rs(auto_stmt->Result());

                if (rs.get() == NULL) {
                    continue;
                }

                if (rs->ResultType() != eDB_RowResult) {
                    continue;
                }

                while(rs->Fetch()) {
                    size_t read_bytes = 0;
                    bool is_null = true;
                    ++rec_num;

                    if (read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null)) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, data);
                }
                BOOST_CHECK_EQUAL(rec_num, 1);

            }
        }

        // Check NULL-values ...
        if (false) {
            int num_of_tests = 10;

            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(m_Conn->GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name, 2));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(1, &dataTextVal);

                for(int i = 0; i < num_of_tests; ++i ) {
                    geneIdVal = i;

                    if (i % 2 == 0) {
                        dataTextVal.Append(data);
                        dataTextVal.MoveTo(0);
                    } else {
                        dataTextVal.AssignNULL();
                    }

                    bcp->SendRow();
                }

                bcp->CompleteBCP();
            }

            // Check inserted data ...
            {
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(m_Conn->GetCDB_Connection()->LangCmd(sql));

                bool rc = auto_stmt->Send();
                BOOST_CHECK( rc );

                while(auto_stmt->HasMoreResults()) {
                    auto_ptr<CDB_Result> rs(auto_stmt->Result());

                    if (rs.get() == NULL) {
                        continue;
                    }

                    if (rs->ResultType() != eDB_RowResult) {
                        continue;
                    }

                    for(int i = 0; i < num_of_tests; ++i ) {
                        string result;
                        size_t read_bytes = 0;
                        bool is_null = true;

                        BOOST_CHECK(rs->Fetch());

                        if (read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null)) {
                            result += string(buff, read_bytes);
                        }


                        if (i % 2 == 0) {
                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, data);
                        } else {
                            BOOST_CHECK(is_null);
                        }
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
void
CDBAPIUnitTest::Test_BulkInsertBlob_LowLevel2(void)
{
    string sql;
    // enum { record_num = 10 };
    string table_name = "#bcp_table2";
    const string data = "testing";

    try {
        CDB_Connection* conn = m_Conn->GetCDB_Connection();

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    vkey bigint NOT NULL , \n";
            sql += "    geneId int NOT NULL , \n";
            sql += "    modate datetime NOT NULL , \n";
            sql += "    dtype int NOT NULL , \n";
            sql += "    dsize int NOT NULL , \n";
            sql += "    dataStr varchar (255) COLLATE Latin1_General_BIN NULL , \n";
            sql += "    dataInt int NULL , \n";
            sql += "    dataBin varbinary (255) NULL , \n";
            sql += "    cnt int NOT NULL , \n";
            sql += "    dataText text COLLATE Latin1_General_BIN NULL , \n";
            sql += "    dataImg image NULL  \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_BlobStream(void)
{
    string sql;
    enum {test_size = 10000};
    long data_len = 0;
    long write_data_len = 0;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Prepare data ...
        {
            ostrstream out;

            for (int i = 0; i < test_size; ++i) {
                out << i << " ";
            }

            data_len = out.pcount();
            BOOST_CHECK(data_len > 0);

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

            // Insert data ...
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";
            auto_stmt->ExecuteUpdate( sql );

            sql  = " SELECT text_field FROM " + GetTableName();

            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& ostrm = auto_cursor->GetBlobOStream(1,
                                                             data_len,
                                                             eDisableLog);

                ostrm.write(out.str(), data_len);
                out.freeze(false);

                BOOST_CHECK_EQUAL(ostrm.fail(), false);

                ostrm.flush();

                BOOST_CHECK_EQUAL(ostrm.fail(), false);
                BOOST_CHECK_EQUAL(ostrm.good(), true);
                // BOOST_CHECK_EQUAL(int(ostrm.tellp()), int(out.pcount()));
            }

            auto_stmt->SendSql( "SELECT datalength(text_field) FROM " +
                                GetTableName() );

            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );
                    write_data_len = rs->GetVariant(1).GetInt4();
                    BOOST_CHECK_EQUAL( data_len, write_data_len );
                    while (rs->Next()) {}
                }
            }
        }

        // Retrieve data ...
        {
            auto_stmt->ExecuteUpdate("set textsize 2000000");

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    try {
                        while (rs->Next()) {
                            istream& strm = rs->GetBlobIStream();
                            int j = 0;
                            for (int i = 0; i < test_size; ++i) {
                                strm >> j;

                                // if (i == 5000) {
                                //     DATABASE_DRIVER_ERROR( "Exception safety test.", 0 );
                                // }

                                BOOST_CHECK_EQUAL(strm.good(), true);
                                BOOST_CHECK_EQUAL(strm.eof(), false);
                                BOOST_CHECK_EQUAL(j, i);
                            }
                            long read_data_len = strm.tellg();
                            // Calculate a trailing space.
                            BOOST_CHECK_EQUAL(data_len, read_data_len + 1);
                        }
                    }
                    catch( const CDB_Exception& ) {
                        rs->Close();
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
CDBAPIUnitTest::Test_GetTotalColumns(void)
{
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE #Overlaps ( \n"
                "   pairId int NOT NULL , \n"
                "   overlapNum smallint NOT NULL , \n"
                "   start1 int NOT NULL , \n"
                "   start2 int NOT NULL , \n"
                "   stop1 int NOT NULL , \n"
                "   stop2 int NOT NULL , \n"
                "   orient char (2) NOT NULL , \n"
                "   gaps int NOT NULL , \n"
                "   mismatches int NOT NULL , \n"
                "   adjustedLen int NOT NULL , \n"
                "   length int NOT NULL , \n"
                "   contained tinyint NOT NULL , \n"
                "   seq_align text  NULL , \n"
                "   merged_sa char (1) NOT NULL , \n"
                "   PRIMARY KEY  \n"
                "   ( \n"
                "       pairId, \n"
                "       overlapNum \n"
                "   ) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data into the table ...
        {
            sql =
                "INSERT INTO #Overlaps VALUES( \n"
                "1, 1, 0, 25794, 7126, 32916, '--', 1, 21, 7124, 7127, 0, \n"
                "'Seq-align ::= { }', 'n')";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Actual check ...
        {
            sql = "SELECT * FROM #Overlaps";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    int col_num = -2;

    //                 switch ( rs->GetResultType() ) {
    //                 case eDB_RowResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_ParamResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_StatusResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_ComputeResult:
    //                 case eDB_CursorResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 }

                    col_num = rs->GetTotalColumns();
                    BOOST_CHECK_EQUAL( 14, col_num );
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
BulkAddRow(const auto_ptr<IBulkInsert>& bi, const CVariant& col)
{
    string msg(8000, 'A');
    CVariant col2(col);

    try {
        bi->Bind(2, &col2);
        col2 = msg;
        bi->AddRow();
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::DumpResults(IStatement* const stmt)
{
    while ( stmt->HasMoreResults() ) {
        if ( stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( stmt->GetResultSet() );
        }
    }
}

void
CDBAPIUnitTest::Test_Bulk_Overflow(void)
{
    string sql;
    enum {column_size = 32, data_size = 64};

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Initialize ...
        {
            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vc32_field VARCHAR(32) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        {
            bool exception_catched = false;
            auto_ptr<IBulkInsert> bi(
                m_Conn->GetBulkInsert("#test_bulk_overflow", 1)
                );

            CVariant col1(eDB_VarChar, data_size);

            bi->Bind(1, &col1);

            col1 = string(data_size, 'O');

            // Either AddRow() or Complete() should throw an exception.
            try
            {
                bi->AddRow();

            } catch(const CDB_Exception&)
            {
                exception_catched = true;
            }

            try
            {
                bi->Complete();
            } catch(const CDB_Exception&)
            {
                if ( exception_catched ) {
                    throw;
                } else {
                    exception_catched = true;
                }
            }

            if ( !exception_catched ) {
                BOOST_FAIL("Exception CDB_ClientEx expected.");
            }
        }

        // Retrieve data ...
    //     {
    //         sql = "SELECT * FROM #test_bulk_overflow";
    //
    //         auto_stmt->SendSql( sql );
    //         BOOST_CHECK( auto_stmt->HasMoreResults() );
    //         BOOST_CHECK( !auto_stmt->HasRows() );
    //         auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //
    //         BOOST_CHECK( rs.get() );
    //         BOOST_CHECK( rs->Next() );
    //
    //         const CVariant& value = rs->GetVariant(1);
    //
    //         BOOST_CHECK( !value.IsNull() );
    //
    //         string str_value = value.GetString();
    //         BOOST_CHECK_EQUAL( string::size_type(column_size), str_value.size() );
    //     }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing(void)
{
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );


        // VARBINARY ...
        {
            enum { num_of_tests = 10 };
            const char char_val('2');

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM #bin_bulk_insert_table" );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert("#bin_bulk_insert_table", 2)
                    );

                CVariant col1(eDB_Int);
                CVariant col2(eDB_LongBinary, m_max_varchar_size);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);

                for(int i = 0; i < num_of_tests; ++i ) {
                    int int_value = m_max_varchar_size / num_of_tests * i;
                    string str_val(int_value , char_val);

                    col1 = int_value;
                    col2 = CVariant::LongBinary(m_max_varchar_size,
                                                str_val.c_str(),
                                                str_val.size()
                                                );
                    bi->AddRow();
                }
                bi->Complete();
            }

            // Retrieve data ...
            // Some drivers limit size of text/binary to 255 bytes ...
            if ( m_args.GetDriverName() != "dblib"
                 && m_args.GetDriverName() != "msdblib"
                ) {
                auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

                sql  = " SELECT id, vb8000_field FROM #bin_bulk_insert_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );

                    int int_value = m_max_varchar_size / num_of_tests * i;
                    Int4 id = rs->GetVariant(1).GetInt4();
                    string vb8000_value = rs->GetVariant(2).GetString();

                    BOOST_CHECK_EQUAL( int_value, id );
                    BOOST_CHECK_EQUAL(string::size_type(int_value),
                                      vb8000_value.size()
                                      );
                }

                // Dump results ...
                DumpResults( auto_stmt.get() );
            } else {
                // PutMsgDisabled("Test_Bulk_Writing Retrieve data");
            }
        }

        // INT, BIGINT
        {

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

            // INT collumn ...
            {
                enum { num_of_tests = 8 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        m_Conn->GetBulkInsert("#bulk_insert_table", 3)
                        // m_Conn->GetBulkInsert("#bulk_insert_table", 4)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_Int);
                    CVariant col_tmp(eDB_VarChar);
                    CVariant col4(eDB_BigInt);

                    bi->Bind(1, &col1);
                    // bi->Bind(2, &col_tmp);
                    bi->Bind(3, &col2);
                    // bi->Bind(4, &col4);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        col1 = i;
                        Int4 value = Int4( 1 ) << (i * 4);
                        col2 = value;
                        // Int8 value8 = Int8( 1 ) << (i * 4);
                        // col4 = value8;
                        bi->AddRow();
                    }
                    bi->Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT int_field FROM #bulk_insert_table";
                    sql += " ORDER BY id";

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    for(int i = 0; i < num_of_tests; ++i ) {
                        BOOST_CHECK( rs->Next() );
                        Int4 value = rs->GetVariant(1).GetInt4();
                        Int4 expected_value = Int4( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

            // BIGINT collumn ...
            // Sybase doesn't have BIGINT data type ...
            if (m_args.GetServerType() != CTestArguments::eSybase) {
                enum { num_of_tests = 16 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        m_Conn->GetBulkInsert("#bulk_insert_table", 4)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_BigInt);

                    bi->Bind(1, &col1);
                    bi->Bind(4, &col2);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        col1 = i;
                        Int8 value = Int8( 1 ) << (i * 4);
                        col2 = value;
                        bi->AddRow();
                    }
                    bi->Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT bigint_field FROM #bulk_insert_table";
                    sql += " ORDER BY id";

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    for(int i = 0; i < num_of_tests; ++i ) {
                        BOOST_CHECK( rs->Next() );
                        Int8 value = rs->GetVariant(1).GetInt8();
                        Int8 expected_value = Int8( 1 ) << (i * 4);
                        BOOST_CHECK_EQUAL( expected_value, value );
                    }

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }
        }

        // Yet another BIGINT test (and more) ...
        // Sybase doesn't have BIGINT data type ...
        if (m_args.GetServerType() != CTestArguments::eSybase) {
            auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );

            // Create table ...
            {
                stmt->ExecuteUpdate(
                    // "create table #__blki_test ( name char(32) not null, value bigint null )" );
                    "create table #__blki_test ( name char(32), value bigint null )"
                    );
                stmt->Close();
            }

            // First test ...
            {
                auto_ptr<IBulkInsert> blki(
                    m_Conn->CreateBulkInsert("#__blki_test", 2)
                    );

                CVariant col1(eDB_Char,32);
                CVariant col2(eDB_BigInt);

                blki->Bind(1, &col1);
                blki->Bind(2, &col2);

                col1 = "Hello-1";
                col2 = Int8( 123 );
                blki->AddRow();

                col1 = "Hello-2";
                col2 = Int8( 1234 );
                blki->AddRow();

                col1 = "Hello-3";
                col2 = Int8( 12345 );
                blki->AddRow();

                col1 = "Hello-4";
                col2 = Int8( 123456 );
                blki->AddRow();

                blki->Complete();
                blki->Close();
            }

            // Second test ...
            // Overflow test.
            // !!! Current behavior is not defined properly and not consistent between drivers.
    //         {
    //             auto_ptr<IBulkInsert> blki( m_Conn->CreateBulkInsert("#__blki_test", 2) );
    //
    //             CVariant col1(eDB_Char,64);
    //             CVariant col2(eDB_BigInt);
    //
    //             blki->Bind(1, &col1);
    //             blki->Bind(2, &col2);
    //
    //             string name(8000, 'A');
    //             col1 = name;
    //             col2 = Int8( 123 );
    //             blki->AddRow();
    //         }
        }

        // VARCHAR ...
        {
            int num_of_tests;

            if ( m_args.GetServerType() == CTestArguments::eMsSql
                 || m_args.GetServerType() == CTestArguments::eMsSql2005) {
                num_of_tests = 7;
            } else {
                // Sybase
                num_of_tests = 3;
            }

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert("#bulk_insert_table", 2)
                    );

                CVariant col1(eDB_Int);

                bi->Bind(1, &col1);

                for(int i = 0; i < num_of_tests; ++i ) {
                    col1 = i;
                    switch (i) {
                    case 0:
                        BulkAddRow(bi, CVariant(eDB_VarChar));
                        break;
                    case 1:
                        BulkAddRow(bi, CVariant(eDB_LongChar, m_max_varchar_size));
                        break;
                    case 2:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 1024));
                        break;
                    case 3:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4112));
                        break;
                    case 4:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4113));
                        break;
                    case 5:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4138));
                        break;
                    case 6:
                        BulkAddRow(bi, CVariant(eDB_LongChar, 4139));
                        break;
                    };

                }
                bi->Complete();
            }

            // Retrieve data ...
            // Some drivers limit size of text/binary to 255 bytes ...
            if (m_args.GetDriverName() != "dblib"
                && m_args.GetDriverName() != "msdblib"
                ) {
                sql  = " SELECT id, vc8000_field FROM #bulk_insert_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                        // Retrieve results, if any
                        while( rs->Next() ) {
                            Int4 i = rs->GetVariant(1).GetInt4();
                            string col1 = rs->GetVariant(2).GetString();

                            switch (i) {
                            case 0:
                                BOOST_CHECK_EQUAL(
                                    col1.size(),
                                    string::size_type(m_max_varchar_size)
                                    );
                                break;
                            case 1:
                                BOOST_CHECK_EQUAL(
                                    col1.size(),
                                    string::size_type(m_max_varchar_size)
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
                    }
                }
            } else {
                // PutMsgDisabled("Test_Bulk_Writing Retrieve data");
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing2(void)
{
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE #SbSubs ( \n"
                "    [sacc] int NOT NULL , \n"
                "    [sid] int NOT NULL , \n"
                "    [ver] smallint NOT NULL , \n"
                "    [live] bit NOT NULL DEFAULT (1), \n"
                "    [date] datetime NOT NULL DEFAULT (getdate()), \n"
                "    [rlsdate] smalldatetime NULL, \n"
                "    [depdate] datetime NOT NULL DEFAULT (getdate()) \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert("#SbSubs", 3)
//                     m_Conn->GetBulkInsert("#SbSubs", 1)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);
            CVariant col3(eDB_Int);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            bi->Bind(3, &col3);

            col1 = 15001;
            col2 = 1;
            col3 = 2;
            bi->AddRow();
            bi->Complete();
        }


    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Variant2(void)
{
    const long rec_num = 3;
    const size_t size_step = 10;
    string sql;

    try {
        // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

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
                    CVariant::LongChar(str_val.c_str(), str_val.size()),
                    "@val"
                    );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
        }

        // Test VarChar ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

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
void CDBAPIUnitTest::Test_CDB_Exception(void)
{
    static const CDiagCompileInfo kBlankCompileInfo;
    const string message("Very dangerous message");
    const int msgnumber = 67890;

    try {
        {
            const string server_name("server_name");
            const string user_name("user_name");
            const int severity = 12345;

            CDB_Exception ex(
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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
                kBlankCompileInfo,
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


///////////////////////////////////////////////////////////////////////////////
int
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
                                const string& table_name)
{
    int cur_rec_num = 0;

    DumpResults(auto_stmt.get());
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

int
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt)
{
    int rec_num = 0;

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
Int8 
CDBAPIUnitTest::GetIdentity(const auto_ptr<IStatement>& auto_stmt)
{
    int identity = 0;

    DumpResults(auto_stmt.get());
    auto_stmt->ClearParamList();
    auto_stmt->SendSql( "select CONVERT(NUMERIC(18, 0), @@identity)");
    while (auto_stmt->HasMoreResults()) {
        if (auto_stmt->HasRows()) {
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            if (rs.get() != NULL) {
                while (rs->Next()) {
                    identity = rs->GetVariant(1).GetInt8();
                }
            }
        }
    }

    return identity;
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Cursor(void)
{
    const long rec_num = 2;
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Initialize a test table ...
        {
            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (long i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Check that cursor can be opened twice ...
        {
            sql = "select int_field from " + GetTableName();

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Open a cursor for the second time ...
            {
                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }
        }


        // Check cursor with "FOR UPDATE OF" clause ...
        // First test ...
        {
            sql  = "select int_field from " + GetTableName();
            sql += " FOR UPDATE OF int_field";

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }
        }

        // Second test ...
        if (m_args.GetDriverName() != "ctlib") 
        {
            sql  =
                " CREATE TABLE #AgpFiles ( \n"
                "     id   INT IDENTITY, \n"
                "     submit_id INT, \n"
                "     file_num  INT, \n"
                "     \n"
                "     cr_date   DATETIME DEFAULT getdate(), \n"
                "     path      VARCHAR(255), \n"
                "     name      VARCHAR(255), \n"
                "     notes     VARCHAR(255), \n"
                "     file_size INT, \n"
                "     objects INT default 0, \n"
                "     live_objects INT default 0, \n"
                "      \n"
                "     blob_num INT DEFAULT 0, \n"
                // "     data0 IMAGE NULL, \n"
                // "     data1 IMAGE NULL, \n"
                // "     data2 IMAGE NULL, \n"
                // "     data3 IMAGE NULL, \n"
                // "     data4 IMAGE NULL, \n"
                " )  \n"
                ;
            auto_stmt->ExecuteUpdate(sql);

            sql  =
                " CREATE TABLE #Objects ( \n"
                "     submit_id    INT, \n"
                "     file_id      INT, \n"
                "     rm_file_id   INT DEFAULT 0x7FFFFFF, \n"
                "     name         VARCHAR(80), \n"
                "     blob_ofs     INT NOT NULL, \n"
                // "     blob_len     INT NOT NULL, \n"
                // "     obj_len      INT NOT NULL, \n"
                // "      \n"
                // "     comps        INT NOT NULL, \n"
                // "     gaps         INT NOT NULL, \n"
                // "     scaffolds    INT NOT NULL, \n"
                // "     singletons   INT NOT NULL, \n"
                // "     checksum     INT NOT NULL, \n"
                "      \n"
                "     chr          SMALLINT DEFAULT -1, \n"
                "     is_scaffold  BIT, \n"
                "     is_unlinked  BIT, \n"
                "      \n"
                // "     line_num     INT, \n"
                "     PRIMARY KEY(submit_id, file_id, blob_ofs) \n"
                " ) \n"
                ;
            auto_stmt->ExecuteUpdate(sql);

            string cursor_sql  =
                " SELECT obj.name, f.name, obj.chr, obj.is_scaffold, obj.is_unlinked \n"
                "   FROM #Objects obj \n"
                "   JOIN #AgpFiles f ON obj.file_id = f.id \n"
                "  WHERE obj.rm_file_id=0x7FFFFFF AND obj.submit_id = 1 \n"
                "    FOR UPDATE OF obj.chr, obj.is_scaffold, obj.is_unlinked"
                ;

            // Insert something ...
            {
                sql  = "INSERT INTO #AgpFiles(submit_id, file_num, path, name, notes, file_size) ";
                sql += "VALUES(1, 0, '', '', '', 0)";

                auto_stmt->ExecuteUpdate(sql);

                Int8 agp_files_id = GetIdentity(auto_stmt);

                sql  = "INSERT INTO #Objects(submit_id, file_id, name, blob_ofs) ";
                sql += "VALUES(1, " + NStr::Int8ToString(agp_files_id) + ", '', 0)";

                auto_stmt->ExecuteUpdate(sql);
            }

            // Just read data ...
            {
                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Update something ...
            // Not ready yet !!!!
            if (false) {
                auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                sql = "UPDATE #Objects SET chr=123, is_scaffold=234, is_unlinked=345";

                while (rs->Next()) {
                    auto_cursor->Update("#Objects", sql);
                }
            }
        } else {
            PutMsgDisabled("Test_Cursor Second test");
        } // Second test ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Cursor2(void)
{
    const long rec_num = 2;
    string sql;

    try {
        // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (long i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Test CLOB field update ...
        // It doesn't work right now.
        {
            const char* clob = "abc";

            sql = "select text_field from " + GetTableName() + " for update of text_field \n";
            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));

            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
//                 while(blobRs->Next()) {
//                     ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//                     out.write(clob, sizeof(clob) - 1);
//                     out.flush();
//                 }

                if (blobRs->Next()) {
                    try {
                        /*ostream& out =*/ auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
                    } catch(...)
                    {
                    }
//                     out.write(clob, sizeof(clob) - 1);
//                     out.flush();
                } else {
                    BOOST_FAIL( msg_record_expected );
                }

                if (blobRs->Next()) {
//                     ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//                     out.write(clob, sizeof(clob) - 1);
//                     out.flush();
                } else {
                    BOOST_FAIL( msg_record_expected );
                }
            }

            // Check record number ...
            {
                auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
                BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
            }

            // Another cursor ...
            sql  = " select text_field from " + GetTableName();
            sql += " where int_field = 1 for update of text_field";

            auto_cursor.reset(m_Conn->GetCursor("test02", sql));
            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                if ( !blobRs->Next() ) {
                    BOOST_FAIL( msg_record_expected );
                }
                ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
                out.write(clob, sizeof(clob) - 1);
                out.flush();
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_SelectStmt(void)
{
    try {
        // Scenario:
        // 1) Select recordset with just one record
        // 2) Retrive only one record.
        // 3) Select another recordset with just one record
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
            IResultSet* rs;

            // 1) Select recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57 + 33" );
            BOOST_CHECK( rs != NULL );

            // 2) Retrive a record.
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );

            // 3) Select another recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57.55 + 0.0033" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );
        }

        // Same as before but uses two differenr connections ...
        if (false) {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
            IResultSet* rs;

            // 1) Select recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57 + 33" );
            BOOST_CHECK( rs != NULL );

            // 2) Retrive only one record.
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );

            // 3) Select another recordset with just one record
            auto_ptr<IStatement> auto_stmt2( m_Conn->CreateStatement() );
            rs = auto_stmt2->ExecuteQuery( "select qq = 57.55 + 0.0033" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );
        }

        // Check column name ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            IResultSet* rs(
                auto_stmt->ExecuteQuery( "select @@version as oops" )
                );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            auto_ptr<const IResultSetMetaData> col_metadata(rs->GetMetaData());
            BOOST_CHECK_EQUAL( string("oops"), col_metadata->GetName(1) );
        }

        // Check resultset ...
        {
            int num = 0;
            string sql = "select user_id(), convert(varchar(64), user_name()), "
                "convert(nvarchar(64), user_name())";

            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // 1) Select recordset with just one record
            IResultSet* rs( auto_stmt->ExecuteQuery( sql ) );
            BOOST_CHECK( rs != NULL );

            while (rs->Next()) {
                BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
                BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
                BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
                ++num;
            }

            BOOST_CHECK_EQUAL(num, 1);
        }

        // Check sequent call of ExecuteQuery ...
        if (true) {
            IResultSet* rs = NULL;
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // Run first time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );

            // Run second time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );

            // Run third time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
        } else {
            PutMsgDisabled("Check sequent call of ExecuteQuery");
        }



    //     // TMP
    //     {
    //         auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
    //         BOOST_CHECK( conn.get() != NULL );
    //
    //         conn->SetMode(IConnection::eBulkInsert);
    //
    //         conn->Connect(
    //             "anyone",
    //             "allowed",
    //             "MSSQL10",
    //             "gMapDB"
    //             );
    //
    //         auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
    //
    //         string sql;
    //
    //         sql  = "SELECT len(seq_loc) l, seq_loc FROM protein where ProtGi =56964519";
    //
    //         auto_stmt->SendSql( sql );
    //         while( auto_stmt->HasMoreResults() ) {
    //             if( auto_stmt->HasRows() ) {
    //                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //
    //                 // Retrieve results, if any
    //                 while( rs->Next() ) {
    //                     string col2 = rs->GetVariant(2).GetString();
    //                     BOOST_CHECK_EQUAL(col2.size(), 355);
    //                 }
    //             }
    //         }
    //     }

    //     // TMP
    //     {
    //         auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
    //         BOOST_CHECK( conn.get() != NULL );
    //
    //         conn->SetMode(IConnection::eBulkInsert);
    //
    //         conn->Connect(
    //             "mvread",
    //             "daervm",
    //             "MSSQL29",
    //             "MapViewHum36"
    //             );
    //
    //         auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
    //
    //         string sql;
    //
    //         sql  = "MM_GetData \'model\',\'1\',8335044,8800111,9606";
    //
    //         auto_stmt->SendSql( sql );
    //         while( auto_stmt->HasMoreResults() ) {
    //             if( auto_stmt->HasRows() ) {
    //                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //
    //                 // Retrieve results, if any
    //                 while( rs->Next() ) {
    //                 }
    //             }
    //         }
    //     }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_SelectStmtXML(void)
{
    try {
        // SQL + XML
        {
            string sql;
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
            IResultSet* rs;

            sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
            rs = auto_stmt->ExecuteQuery( sql );
            BOOST_CHECK( rs != NULL );

            if ( !rs->Next() ) {
                BOOST_FAIL( msg_record_expected );
            }

            // Same but call Execute instead of ExecuteQuery.
            auto_stmt->SendSql( sql );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Recordset(void)
{
    try {
        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());
        IResultSet* rs;

        // bit
        {
            rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // tinyint
        {
            rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // smallint
        {
            rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // int
        {
            rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // numeric
        {
            rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // decimal
        {
            rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // float
        {
            rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // double
        {
            rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // real
        {
            rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // smallmoney
        // Unsupported type ...
    //     {
    //         rs = auto_stmt->ExecuteQuery("select convert(smallmoney, 1)");
    //         BOOST_CHECK(rs != NULL);
    //
    //         BOOST_CHECK(rs->Next());
    //         BOOST_CHECK(!rs->Next());
    //
    //         DumpResults(auto_stmt.get());
    //     }

        // money
        // Unsupported type ...
    //     {
    //         rs = auto_stmt->ExecuteQuery("select convert(money, 1)");
    //         BOOST_CHECK(rs != NULL);
    //
    //         BOOST_CHECK(rs->Next());
    //         BOOST_CHECK(!rs->Next());
    //
    //         DumpResults(auto_stmt.get());
    //     }

        // smalldatetime
        {
            rs = auto_stmt->ExecuteQuery("select convert(smalldatetime, 'January 1, 1900')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // datetime
        {
            rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // char
        {
            rs = auto_stmt->ExecuteQuery("select convert(char(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // varchar
        {
            rs = auto_stmt->ExecuteQuery("select convert(varchar(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // nchar
        {
            rs = auto_stmt->ExecuteQuery("select convert(nchar(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // nvarchar
        {
            rs = auto_stmt->ExecuteQuery("select convert(nvarchar(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // binary
        {
            rs = auto_stmt->ExecuteQuery("select convert(binary(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // varbinary
        {
            rs = auto_stmt->ExecuteQuery("select convert(varbinary(32), '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // text
        {
            rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }

        // image
        {
            rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
            BOOST_CHECK(rs != NULL);

            BOOST_CHECK(rs->Next());
            BOOST_CHECK(!rs->Next());

            DumpResults(auto_stmt.get());
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_MetaData(void)
{
    try {
        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());
        IResultSet* rs;
        const IResultSetMetaData* md = NULL;

        // First test ...
        // Check different data types ...
        // Check non-empty results ...
        {
            // bit
            {
                rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Bit);

                DumpResults(auto_stmt.get());
            }

            // tinyint
            {
                rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_TinyInt);

                DumpResults(auto_stmt.get());
            }

            // smallint
            {
                rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_SmallInt);

                DumpResults(auto_stmt.get());
            }

            // int
            {
                rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Int);

                DumpResults(auto_stmt.get());
            }

            // numeric
            {
                rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                DumpResults(auto_stmt.get());
            }

            // decimal
            // There is no eDB_Decimal ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                DumpResults(auto_stmt.get());
            }

            // float
            {
                rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Float);

                DumpResults(auto_stmt.get());
            }

            // double
            {
                rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);

                DumpResults(auto_stmt.get());
            }

            // real
            {
                rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Float);

                DumpResults(auto_stmt.get());
            }

            // smallmoney
            // Unsupported type ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(smallmoney, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);

                DumpResults(auto_stmt.get());
            }

            // money
            // Unsupported type ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(money, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);

                DumpResults(auto_stmt.get());
            }

            // smalldatetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(smalldatetime, 'January 1, 1900')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_SmallDateTime);

                DumpResults(auto_stmt.get());
            }

            // datetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_DateTime);

                DumpResults(auto_stmt.get());
            }

            // char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Char);

                DumpResults(auto_stmt.get());
            }

            // varchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(varchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);

                DumpResults(auto_stmt.get());
            }

            // nchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Char);

                DumpResults(auto_stmt.get());
            }

            // nvarchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nvarchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);

                DumpResults(auto_stmt.get());
            }

            // binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Binary);

                DumpResults(auto_stmt.get());
            }

            // varbinary
            {
                rs = auto_stmt->ExecuteQuery("select convert(varbinary(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarBinary);

                DumpResults(auto_stmt.get());
            }

            // text
            {
                rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Text);

                DumpResults(auto_stmt.get());
            }

            // image
            {
                rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Image);

                DumpResults(auto_stmt.get());
            }
        }

        // Second test ...
        {
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_UserErrorHandler(void)
{
    try {
        // Set up an user-defined error handler ..

        // Push message handler into a context ...
        I_DriverContext* drv_context = m_DS->GetDriverContext();

        // Check PushCntxMsgHandler ...
        // PushCntxMsgHandler - Add message handler "h" to process 'context-wide' (not bound
        // to any particular connection) error messages.
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            auto_ptr<IConnection> local_conn( m_DS->CreateConnection() );
            Connect(local_conn);

            drv_context->PushCntxMsgHandler( drv_err_handler.get() );

            // Connection process should be affected ...
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( m_DS->CreateConnection() );

                try {
                    conn->Connect(
                        "unknown",
                        "invalid",
                        m_args.GetServerName()
                        );
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
                catch( const CException& ) {
                    BOOST_FAIL( "CException was catched instead of CDB_Exception." );
                }
                catch( ... ) {
                    BOOST_FAIL( "... was catched instead of CDB_Exception." );
                }

                drv_err_handler->Init();
            }

            // Current connection should not be affected ...
            {
                try {
                    ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // New connection should not be affected ...
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( m_DS->CreateConnection() );
                Connect(conn);

                // Reinit the errot handler because it can be affected during connection.
                drv_err_handler->Init();

                try {
                    ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

                try {
                    ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                local_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
            }

            // New connection should not be affected
            // after pushing a message handler into another connection
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( m_DS->CreateConnection() );
                Connect(conn);

                // Reinit the errot handler because it can be affected during connection.
                drv_err_handler->Init();

                try {
                    ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Remove all inserted handlers ...
            drv_context->PopCntxMsgHandler( drv_err_handler.get() );
        }


        // Check PushDefConnMsgHandler ...
        // PushDefConnMsgHandler - Add `per-connection' err.message handler "h" to the stack of default
        // handlers which are inherited by all newly created connections.
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());


            auto_ptr<IConnection> local_conn( m_DS->CreateConnection() );
            Connect(local_conn);

            drv_context->PushDefConnMsgHandler( drv_err_handler.get() );

            // Current connection should not be affected ...
            {
                try {
                    ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            // This is supposed to be okay.
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

                try {
                    ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                local_conn->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }


            ////////////////////////////////////////////////////////////////////////
            // Create a new connection.
            auto_ptr<IConnection> new_conn( m_DS->CreateConnection() );
            Connect(new_conn);

            // New connection should be affected ...
            {
                try {
                    ES_01_Internal(*new_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            // This is supposed to be okay.
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn->GetCDB_Connection()->PushMsgHandler( msg_handler.get() );

                try {
                    ES_01_Internal(*new_conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
            }

            // New connection should be affected
            // after pushing a message handler into another connection
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( m_DS->CreateConnection() );
                Connect(conn);

                try {
                    ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& ) {
                    // Ignore it
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
            }

            // Remove handlers ...
            drv_context->PopDefConnMsgHandler( drv_err_handler.get() );
        }

        // SetLogStream ...
        {
            {
                IConnection* conn = NULL;

                // Enable multiexception ...
                m_DS->SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = m_DS->CreateConnection();
                } catch(...)
                {
                    delete conn;
                }

                m_DS->SetLogStream(&cerr);
            }

            {
                IConnection* conn = NULL;

                // Enable multiexception ...
                m_DS->SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = m_DS->CreateConnection();

                    m_DS->SetLogStream(&cerr);
                } catch(...)
                {
                    delete conn;
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
// User error handler life-time ...
void CDBAPIUnitTest::Test_UserErrorHandler_LT(void)
{
    I_DriverContext* drv_context = m_DS->GetDriverContext();

    // Context. eNoOwnership ...
    {
        // auto_ptr
        // Use it once ...
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler.get());
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler.get());
        }

        // auto_ptr
        // Use it twice ...
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler.get());
            drv_context->PushDefConnMsgHandler(drv_err_handler.get());
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler.get());
            drv_context->PopDefConnMsgHandler(drv_err_handler.get());
        }

        // CRef
        // Use it once ...
        {
            CRef<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler);
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler);
        }

        // CRef
        // Use it twice ...
        {
            CRef<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler);
            drv_context->PushDefConnMsgHandler(drv_err_handler);
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler);
            drv_context->PopDefConnMsgHandler(drv_err_handler);
        }
    }

    // Connection. eNoOwnership ...
    {
        // auto_ptr
        {
            auto_ptr<CTestErrHandler> auto_msg_handler(new CTestErrHandler());

            // One connection ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn(m_DS->CreateConnection());
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }

            // Two connections ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> new_conn2(m_DS->CreateConnection());
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn1);
                Connect(new_conn2);

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                // Remove handler ...
                new_conn1->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
                new_conn2->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }

            // One connection ...
            {
                auto_ptr<IConnection> conn1(m_DS->CreateConnection());

                Connect(conn1);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> conn2(m_DS->CreateConnection());

                Connect(conn1);
                Connect(conn2);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                conn2->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
                conn2->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
            }
        }

        // CRef
        {
            CRef<CTestErrHandler> cref_msg_handler(new CTestErrHandler());

            // One connection ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn(m_DS->CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler);
                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler(msg_handler);
            }

            // Two connections ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> new_conn2(m_DS->CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn1);
                Connect(new_conn2);

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler);
                // Remove handler ...
                new_conn1->GetCDB_Connection()->PopMsgHandler(msg_handler);
                new_conn2->GetCDB_Connection()->PopMsgHandler(msg_handler);
            }

            // One connection ...
            {
                auto_ptr<IConnection> conn1(m_DS->CreateConnection());

                Connect(conn1);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> conn2(m_DS->CreateConnection());

                Connect(conn1);
                Connect(conn2);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                conn2->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
                conn2->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
            }
        }
    }

    // Connection. eTakeOwnership ...
    {
        CRef<CTestErrHandler> cref_msg_handler(new CTestErrHandler());

        // Raw pointer ...
        {
            // One connection ...
            {
                auto_ptr<IConnection> new_conn(m_DS->CreateConnection());
                CTestErrHandler* msg_handler = new CTestErrHandler();

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Raw pointer ...
            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> new_conn2(m_DS->CreateConnection());
                CTestErrHandler* msg_handler = new CTestErrHandler();

                Connect(new_conn1);
                Connect(new_conn2);

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }
        }

        // CRef
        {
            // One connection ...
            {
                auto_ptr<IConnection> new_conn(m_DS->CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> new_conn2(m_DS->CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                Connect(new_conn1);
                Connect(new_conn2);

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // One connection ...
            {
                auto_ptr<IConnection> new_conn(m_DS->CreateConnection());

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(m_DS->CreateConnection());
                auto_ptr<IConnection> new_conn2(m_DS->CreateConnection());

                Connect(new_conn1);
                Connect(new_conn2);

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }
        }
    }

    // Context. eTakeOwnership ...
    {
        // Raw pointer ...
        {
            CTestErrHandler* msg_handler = new CTestErrHandler();

            // Add handler ...
            drv_context->PushCntxMsgHandler(msg_handler,
                                            eTakeOwnership);
            drv_context->PushDefConnMsgHandler(msg_handler,
                                               eTakeOwnership);
            // We do not remove msg_handler here because it is supposed to be
            // deleted by drv_context ...
        }

        // CRef
        {
            CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(msg_handler,
                                            eTakeOwnership);
            drv_context->PushDefConnMsgHandler(msg_handler,
                                               eTakeOwnership);
            // We do not remove msg_handler here because it is supposed to be
            // deleted by drv_context ...
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Procedure(void)
{
    try {
        // Test a regular IStatement with "exec"
        // Parameters are not allowed with this construction.
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // Execute it first time ...
            auto_stmt->SendSql( "exec sp_databases" );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch ( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_ParamResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_StatusResult:
                        while( rs->Next() ) {
                            int status = rs->GetVariant(1).GetInt4();
                            status = status;
                        }
                        break;
                    case eDB_ComputeResult:
                    case eDB_CursorResult:
                        break;
                    }
                }
            }

            // Execute it second time ...
            auto_stmt->SendSql( "exec sp_databases" );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch ( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_ParamResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_StatusResult:
                        while( rs->Next() ) {
                            int status = rs->GetVariant(1).GetInt4();
                            status = status;
                        }
                        break;
                    case eDB_ComputeResult:
                    case eDB_CursorResult:
                        break;
                    }
                }
            }

            // Same as before but do not retrieve data ...
            auto_stmt->SendSql( "exec sp_databases" );
            auto_stmt->SendSql( "exec sp_databases" );
        }

        // Test ICallableStatement
        // No parameters at this time.
        {
            // Execute it first time ...
            auto_ptr<ICallableStatement> auto_stmt(
                m_Conn->GetCallableStatement("sp_databases")
                );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            int status = auto_stmt->GetReturnStatus();


            // Execute it second time ...
            auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            status = auto_stmt->GetReturnStatus();


            // Same as before but do not retrieve data ...
            auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
            auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
        }

        // Temporary test ...
        // !!! This is a bug ...
        if (false) {
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "anyone",
                "allowed",
                "PUBSEQ_OS_LXA",
                ""
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->GetCallableStatement("id_seqid4gi")
                );
            auto_stmt->SetParam( CVariant(1), "@gi" );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
        }


        // Test returned recordset ...
        {
            // In case of MS SQL 2005 sp_databases returns empty result set.
            // It is not a bug. It is a difference in implementation of MS SQL
            // 2005.
            if (m_args.GetServerType() != CTestArguments::eMsSql2005) {
                int num = 0;
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("sp_databases", 3)
                    );

                auto_stmt->Execute();

                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
                    BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
                    BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
                    ++num;
                }

                BOOST_CHECK(num > 0);

                DumpResults(auto_stmt.get());
            }

            {
                int num = 0;
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("sp_server_info", 3)
                    );

                auto_stmt->Execute();

                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
                    BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
                    BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
                    ++num;
                }

                BOOST_CHECK(num > 0);

                DumpResults(auto_stmt.get());
            }
        }

        // Test ICallableStatement
        // With parameters.
        {
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("sp_server_info", 1)
                    );

                // Set parameter to NULL ...
                auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
                auto_stmt->Execute();

                if (m_args.GetServerType() == CTestArguments::eSybase) {
                    BOOST_CHECK_EQUAL( 30, GetNumOfRecords(auto_stmt) );
                } else {
                    BOOST_CHECK_EQUAL( 29, GetNumOfRecords(auto_stmt) );
                }

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Set parameter to 1 ...
                auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
                auto_stmt->Execute();

                BOOST_CHECK_EQUAL( 1, GetNumOfRecords(auto_stmt) );
            }

            // Doesn't work for some reason ...
            if (false) {
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("sp_statistics")
                    );

                auto_stmt->SetParam(CVariant(GetTableName()), "@table_name");
                auto_stmt->Execute();

                {
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    BOOST_CHECK(auto_stmt->HasRows());
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);

                    BOOST_CHECK(rs->Next());
                    DumpResults(auto_stmt.get());
                }

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Execute it second time ...
                auto_stmt->SetParam(CVariant("#bulk_insert_table"), "@table_name");
                auto_stmt->Execute();

                {
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    BOOST_CHECK(auto_stmt->HasRows());
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);

                    BOOST_CHECK(rs->Next());
                    DumpResults(auto_stmt.get());
                }
            }

            if (false) {
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("DBAPI_Sample..TestBigIntProc")
                    );

                auto_stmt->SetParam(CVariant(Int8(1234567890)), "@num");
                auto_stmt->ExecuteUpdate();
            }
        }

        // Test output parameters ...
        if (false) {
            auto_ptr<ICallableStatement> auto_stmt(
                m_Conn->GetCallableStatement("DBAPI_Sample..SampleProc", 3)
                );
            auto_stmt->SetParam(CVariant(1), "@id");
            auto_stmt->SetParam(CVariant(2.0), "@f");
            auto_stmt->SetOutputParam(CVariant(eDB_Int), "@o");

            auto_stmt->Execute();
    //         auto_stmt->SendSql( "exec DBAPI_Sample..TestProc4 @test_out output" );

             while(auto_stmt->HasMoreResults()) {
                 if( auto_stmt->HasRows() ) {
                     auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                     switch( rs->GetResultType() ) {
                     case eDB_RowResult:
                         while(rs->Next()) {
                             // retrieve row results
                         }
                         break;
                     case eDB_ParamResult:
                         BOOST_CHECK(rs->Next());
                         NcbiCout << "Output param: "
                                  << rs->GetVariant(1).GetInt4()
                                  << endl;
                         break;
                     default:
                         break;
                     }
                 }
             }

    //         BOOST_CHECK(auto_stmt->HasMoreResults());
    //         BOOST_CHECK(auto_stmt->HasRows());
    //         auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //         BOOST_CHECK(rs.get() != NULL);
    //
    //         while (rs->Next()) {
    //             BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
    //             BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
    //             BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
    //             ++num;
    //         }
    //
    //         BOOST_CHECK(num > 0);

            DumpResults(auto_stmt.get());
        }

        // Temporary test ...
        if (false) {
            auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "anyone",
                "allowed",
                "mssql58.nac.ncbi.nlm.nih.gov",
                "GenomeHits"
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->GetCallableStatement("NewSub")
                );
            auto_stmt->SetParam(CVariant("tsub2"), "@name");
            auto_stmt->SetParam(CVariant("tst"), "@center");
            auto_stmt->SetParam(CVariant("9606"), "@taxid");
            auto_stmt->SetParam(CVariant("Homo sapiens"), "@organism");
            auto_stmt->SetParam(CVariant(""), "@notes");
            auto_stmt->Execute();

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Exception_Safety(void)
{
    // Very first test ...
    {
        // Try to catch a base class ...
        BOOST_CHECK_THROW(ES_01_Internal(*m_Conn), CDB_Exception);
    }

    // Second test ...
    // Restore after invalid statement ...
    try {
        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

        auto_stmt->ExecuteUpdate("INSERT #bulk_insert_table(id) VALUES(17)");

        try {
            // Try to insert duplicate value ...
            auto_stmt->ExecuteUpdate("INSERT #bulk_insert_table(id) VALUES(17)");
        } catch (const CDB_Exception&) {
            // ignore it ...
        }

        try {
            // execute invalid statement ...
            auto_stmt->ExecuteUpdate("ISERT #bulk_insert_table(id) VALUES(17)");
        } catch (const CDB_Exception&) {
            // ignore it ...
        }

        // Check status of the statement ...
        if (false) {
            auto_stmt->ExecuteUpdate("SELECT max(id) FROM #bulk_insert_table");
        } else {
            auto_stmt->SendSql("SELECT max(id) FROM #bulk_insert_table");
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }

    // Third test ...
    {
        // Enable MsgToEx ...
        m_Conn->MsgToEx(true);
        // Disable MsgToEx ...
        m_Conn->MsgToEx(false);

        BOOST_CHECK_THROW(ES_01_Internal(*m_Conn), CDB_Exception);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Throw CDB_Exception ...
void
CDBAPIUnitTest::ES_01_Internal(IConnection& conn)
{
    auto_ptr<IStatement> auto_stmt( conn.GetStatement() );

    auto_stmt->SendSql( "select name from wrong table" );
    while( auto_stmt->HasMoreResults() ) {
        if( auto_stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

            while( rs->Next() ) {
                // int col1 = rs->GetVariant(1).GetInt4();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_StatementParameters(void)
{
    try {
        // Very first test ...
        {
            const string table_name(GetTableName());
//             const string table_name("DBAPI_Sample..tmp_table_01");
            string sql;
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            if (false) {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
                sql += "    int_field INT NULL \n";
                sql += " )";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Clean previously inserted data ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );
            }

            {
                sql  = " INSERT INTO " + table_name +
                    "(int_field) VALUES( @value ) \n";

                auto_stmt->SetParam( CVariant(0), "@value" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                auto_stmt->SetParam( CVariant(1), "@value" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }


            {
                sql  = " SELECT int_field FROM " + table_name +
                    " ORDER BY int_field";
                // Execute a statement without parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }

            // Get number of records ...
            {
                auto_stmt->SendSql( "SELECT COUNT(*) FROM " + table_name );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 2 );
                DumpResults(auto_stmt.get());
            }

            // Read first inserted value back ...
            {
                auto_stmt->SetParam( CVariant(0), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 0 );
                DumpResults(auto_stmt.get());
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Read second inserted value back ...
            {
                auto_stmt->SetParam( CVariant(1), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 1 );
                DumpResults(auto_stmt.get());
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Clean previously inserted data ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_NULL(void)
{
    enum {rec_num = 10};
    string sql;

    try {
        // Initialize data (strings are NOT empty) ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                             "@vc1000_field"
                                             );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
                    auto_stmt->ClearParamList();
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(int(rec_num),
                                  GetNumOfRecords(auto_stmt, GetTableName())
                                  );
            }

            {
                // Drop all records ...
                sql  = " DELETE FROM #test_unicode_table";
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO #test_unicode_table"
                    "(nvc255_field) VALUES(@nvc255_field)";

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@nvc255_field");
                    } else {
                        auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                             "@nvc255_field"
                                             );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
                    auto_stmt->ClearParamList();
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(int(rec_num),
                                  GetNumOfRecords(auto_stmt, GetTableName()));
            }
        }

        if (false) {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM DBAPI_Sample..dbapi_unit_test";
            auto_stmt->ExecuteUpdate(sql);

            sql  = " INSERT INTO DBAPI_Sample..dbapi_unit_test"
                "(int_field, vc1000_field) "
                "VALUES(@int_field, @vc1000_field) \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            // Insert data ...
            for (long ind = 0; ind < rec_num; ++ind) {
                if (ind % 2 == 0) {
                    auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                    auto_stmt->SetParam( CVariant(eDB_VarChar), "@vc1000_field" );
                } else {
                    auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                    auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                         "@vc1000_field"
                                         );
                }

                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Check record number ...
            BOOST_CHECK_EQUAL(int(rec_num),
                              GetNumOfRecords(auto_stmt, GetTableName())
                              );
        }

        // Check ...
        if (true) {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            {
                sql = "SELECT int_field, vc1000_field FROM " + GetTableName() +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        BOOST_CHECK_EQUAL( vc1000_field.GetString(),
                                           NStr::IntToString(ind) );
                    }
                }

                DumpResults(auto_stmt.get());
            }

            {
                sql = "SELECT nvc255_field FROM #test_unicode_table ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& nvc255_field = rs->GetVariant(1);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( nvc255_field.IsNull() );
                    } else {
                        BOOST_CHECK( !nvc255_field.IsNull() );
                        BOOST_CHECK_EQUAL( NStr::TruncateSpaces(
                                                nvc255_field.GetString()
                                                ),
                                           NStr::IntToString(ind) );
                    }
                }

                DumpResults(auto_stmt.get());
            }
        }

        // Check NULL with stored procedures ...
        {
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    m_Conn->GetCallableStatement("sp_server_info", 1)
                    );

                // Set parameter to NULL ...
                auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
                auto_stmt->Execute();

                if (m_args.GetServerType() == CTestArguments::eSybase) {
                    BOOST_CHECK_EQUAL( 30, GetNumOfRecords(auto_stmt) );
                } else {
                    BOOST_CHECK_EQUAL( 29, GetNumOfRecords(auto_stmt) );
                }

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Set parameter to 1 ...
                auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
                auto_stmt->Execute();

                BOOST_CHECK_EQUAL( 1, GetNumOfRecords(auto_stmt) );
            }

            {
            }
        }


        // Special case: empty strings.
        if (false) {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            // Initialize data (strings are EMPTY) ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(string()), "@vc1000_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );

                    // !!! Do not forget to clear a parameter list ....
                    // Workaround for the ctlib driver ...
                    auto_stmt->ClearParamList();
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(int(rec_num), GetNumOfRecords(auto_stmt,
                                                                GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + GetTableName() +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        BOOST_CHECK_EQUAL( vc1000_field.GetString(), string() );
                    }
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_GetRowCount()
{
    try {
        enum {repeat_num = 5};
        for ( int i = 0; i < repeat_num; ++i ) {
            // Shared/Reusable statement
            {
                auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

                CheckGetRowCount( i, eNoTrans, stmt.get() );
                CheckGetRowCount( i, eTransCommit, stmt.get() );
                CheckGetRowCount( i, eTransRollback, stmt.get() );
            }
            // Dedicated statement
            {
                CheckGetRowCount( i, eNoTrans );
                CheckGetRowCount( i, eTransCommit );
                CheckGetRowCount( i, eTransRollback );
            }
        }

        for ( int i = 0; i < repeat_num; ++i ) {
            // Shared/Reusable statement
            {
                auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

                CheckGetRowCount2( i, eNoTrans, stmt.get() );
                CheckGetRowCount2( i, eTransCommit, stmt.get() );
                CheckGetRowCount2( i, eTransRollback, stmt.get() );
            }
            // Dedicated statement
            {
                CheckGetRowCount2( i, eNoTrans );
                CheckGetRowCount2( i, eTransCommit );
                CheckGetRowCount2( i, eTransRollback );
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::CheckGetRowCount(
    int row_count,
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( 1 ) \n";

    // Insert row_count records into the table ...
    for ( int i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ExecuteUpdate(sql);
        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName() + " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( 0, nRows );
    }

}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::CheckGetRowCount2(
    int row_count,
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    // auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );
    // _ASSERT(curr_stmt->get());
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

    // Insert row_count records into the table ...
    for ( Int4 i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->SetParam(CVariant(i), "@value");
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows2, 1 );
    }

    // Workaround for the CTLIB driver ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ClearParamList();
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() + " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_BlobStore(void)
{
    string sql;
    string table_name = "#TestBlobStore";

    try {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Create a table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "  id   INT, \n"
                "  cr_date   DATETIME DEFAULT getdate(), \n"
                " \n"
                "  -- columns needed to satisy blobstore.cpp classes \n"
                "  blob_num INT DEFAULT 0, -- always 0 (we need not accept data that does not fit into 1 row) \n"
                "  data0 IMAGE NULL, \n"
                "  data1 IMAGE NULL, \n"
                "  data2 IMAGE NULL, \n"
                "  data3 IMAGE NULL, \n"
                "  data4 IMAGE NULL \n"
//                 "  data0 TEXT NULL, \n"
//                 "  data1 TEXT NULL, \n"
//                 "  data2 TEXT NULL, \n"
//                 "  data3 TEXT NULL, \n"
//                 "  data4 TEXT NULL \n"
                ") "
                ;
            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert a row
        {
            auto_stmt->ExecuteUpdate("INSERT INTO " + table_name + " (id) values (66)");
        }

        {
            enum {
                IMAGE_BUFFER_SIZE = (64*1024*1024),
                // The number of dataN fields in the table
                BLOB_COL_COUNT = 5,
                // If you need to increase MAX_FILE_SIZE,
                // try increasing IMAGE_BUFFER_SIZE,
                // or alter table AgpFiles and BLOB_COL_COUNT.
                MAX_FILE_SIZE = (BLOB_COL_COUNT*IMAGE_BUFFER_SIZE)
                };

            string blob_cols[BLOB_COL_COUNT];

            for(int i=0; i<BLOB_COL_COUNT; i++) {
                blob_cols[i] = string("data") + NStr::IntToString(i);
            }

            CBlobStoreStatic blobrw(
                m_Conn->GetCDB_Connection(),
                table_name,     // tableName
                "id",           // keyColName
                "blob_num",     // numColName
                blob_cols,      // blobColNames
                BLOB_COL_COUNT, // nofBC - number of blob columns
                false,          // bool isText
                eNone, //eZLib, // ECompressMethod cm
                IMAGE_BUFFER_SIZE,
                false
            );

            // Write blob to the row ...
            {
                auto_ptr<ostream> pStream(blobrw.OpenForWrite( "66" ));
                BOOST_CHECK(pStream.get() != NULL);

                for(int i = 0; i < 10000; ++i) {
                    *pStream <<
                        "A quick brown fox jumps over the lazy dog, message #" <<
                        i << "\n";
                }

                pStream->flush();
            }

            // Read blob ...
            {
                auto_ptr<istream> pStream(blobrw.OpenForRead( "66" ));
                BOOST_CHECK(pStream.get() != NULL);

                string line;
                while (!pStream->eof()) {
                    NcbiGetline(*pStream, line, '\n');
                }
                // while (NcbiGetline(*pStream, line, "\r\n")) {
                //     ;
                // }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_DropConnection(void)
{
    string sql;
    enum {
        num_of_tests = 31
    };

    try {
        sql = "sleep 0 kill";
        const unsigned orig_conn_num =
            m_DS->GetDriverContext()->NofConnections();

        for (unsigned i = 0; i < num_of_tests; ++i) {
            // Start a connection scope ...
            {
                bool conn_killed = false;
                auto_ptr<IConnection> auto_conn(
                    m_DS->CreateConnection(CONN_OWNERSHIP)
                    );
                auto_conn->Connect("anyone","allowed","LINK_OS");

                // kill connection ...
                try {
                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->ExecuteUpdate(sql);
                } catch (const CDB_Exception&) {
                    // Ignore it ...
                }

                try {
                    auto_ptr<ICallableStatement> auto_stmt(
                        auto_conn->GetCallableStatement("sp_who")
                        );
                    auto_stmt->Execute();
                } catch (const CDB_Exception&) {
                    conn_killed = true;
                }

                BOOST_CHECK(conn_killed);
            }

            BOOST_CHECK_EQUAL(
                orig_conn_num,
                m_DS->GetDriverContext()->NofConnections()
                );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_N_Connections(void)
{
    enum {
        eN = 50
    };

    auto_ptr<IConnection> auto_conns[eN];

    for (int i = 0; i < eN; ++i) {
        auto_ptr<IConnection> auto_conn(m_DS->CreateConnection(CONN_OWNERSHIP));
        Connect(auto_conn);
        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());

        auto_stmt->ExecuteUpdate("SELECT @@version");
        auto_conns[i] = auto_conn;
    }
}


////////////////////////////////////////////////////////////////////////////////
static
IDBServiceMapper*
MakeCDBUDPriorityMapper01(const IRegistry* registry)
{
    TSvrRef server01(new CDBServer("MS_DEV1"));
    TSvrRef server02(new CDBServer("MS_DEV2"));
    const string service_name("TEST_SERVICE_01");

    auto_ptr<CDBUDPriorityMapper> mapper(new CDBUDPriorityMapper(registry));

    mapper->Add(service_name, server01);
    mapper->Add(service_name, server02);
    mapper->Add(service_name, server01);
    mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);

    return mapper.release();
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Check_Validator(TDBConnectionFactoryFactory factory,
                                IConnValidator& validator)
{
    TSvrRef server01(new CDBServer("MS_DEV1"));
    const string service_name("TEST_SERVICE_01");
    auto_ptr<IConnection> conn;

    // Install new mapper ...
    ncbi::CDbapiConnMgr::Instance().SetConnectionFactory(
        factory(MakeCDBUDPriorityMapper01)
    );

    // Create connection ...
    conn.reset(m_DS->CreateConnection(CONN_OWNERSHIP));
    BOOST_CHECK(conn.get() != NULL);

    // There are only 3 of server01 ...
    for (int i = 0; i < 12; ++i) {
        conn->ConnectValidated(
            validator,
            m_args.GetUserName(),
            m_args.GetUserPassword(),
            service_name
            );

        string server_name = conn->GetCDB_Connection()->ServerName();
//         LOG_POST(Warning << "Connected to: " << server_name);
        // server02 shouldn't be reported ...
        BOOST_CHECK_EQUAL(server_name, server01->GetName());
        conn->Close();
    }
}

////////////////////////////////////////////////////////////////////////////////
static
IDBConnectionFactory*
CDBConnectionFactoryFactory(IDBServiceMapper::TFactory svc_mapper_factory)
{
    return new CDBConnectionFactory(svc_mapper_factory);
}

static
IDBConnectionFactory*
CDBRedispatchFactoryFactory(IDBServiceMapper::TFactory svc_mapper_factory)
{
    return new CDBRedispatchFactory(svc_mapper_factory);
}


////////////////////////////////////////////////////////////////////////////////
class CValidator01 : public CTrivialConnValidator
{
public:
    CValidator01(const string& db_name,
                int attr = eDefaultValidateAttr)
    : CTrivialConnValidator(db_name, attr)
    {
    }

public:
    virtual IConnValidator::EConnStatus Validate(CDB_Connection& conn)
    {
        // Try to change a database ...
        try {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd(string("use ") + GetDBName()));
            set_cmd->Send();
            set_cmd->DumpResults();
        }
        catch(const CDB_Exception&) {
            // LOG_POST(Warning << "Db not accessible: " << GetDBName() <<
            //             ", Server name: " << conn.ServerName());
            return eTempInvalidConn;
            // return eInvalidConn;
        }

        if (GetAttr() & eCheckSysobjects) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // Go back to the original (master) database ...
        if (GetAttr() & eRestoreDefaultDB) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("use master"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // All exceptions are supposed to be caught and processed by
        // CDBConnectionFactory ...
        return eValidConn;
    }

    virtual EConnStatus ValidateException(const CDB_Exception& ex)
    {
        return eTempInvalidConn;
    }

    virtual string GetName(void) const
    {
        return "CValidator01" + GetDBName();
    }

};


////////////////////////////////////////////////////////////////////////////////
class CValidator02 : public CTrivialConnValidator
{
public:
    CValidator02(const string& db_name,
                int attr = eDefaultValidateAttr)
    : CTrivialConnValidator(db_name, attr)
    {
    }

public:
    virtual IConnValidator::EConnStatus Validate(CDB_Connection& conn)
    {
        // Try to change a database ...
        try {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("use " + GetDBName()));
            set_cmd->Send();
            set_cmd->DumpResults();
        }
        catch(const CDB_Exception&) {
            // LOG_POST(Warning << "Db not accessible: " << GetDBName() <<
            //             ", Server name: " << conn.ServerName());
            return eTempInvalidConn;
            // return eInvalidConn;
        }

        if (GetAttr() & eCheckSysobjects) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // Go back to the original (master) database ...
        if (GetAttr() & eRestoreDefaultDB) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("use master"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // All exceptions are supposed to be caught and processed by
        // CDBConnectionFactory ...
        return eValidConn;
    }

    virtual EConnStatus ValidateException(const CDB_Exception& ex)
    {
        return eInvalidConn;
    }

    virtual string GetName(void) const
    {
        return "CValidator02" + GetDBName();
    }
};


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::CheckConnFactory(TDBConnectionFactoryFactory factory_factory)
{
    // CTrivialConnValidator ...
    {
        // Connection validator to check against DBAPI_Sample ...
        CTrivialConnValidator validator("DBAPI_Sample");

        Check_Validator(factory_factory, validator);
    }

    // Same as before but with a customized validator ...
    {
        class CValidator : public CTrivialConnValidator
        {
        public:
            CValidator(const string& db_name,
                       int attr = eDefaultValidateAttr)
            : CTrivialConnValidator(db_name, attr)
            {
            }

            virtual EConnStatus ValidateException(const CDB_Exception& ex)
            {
                return eTempInvalidConn;
            }
        };

        // Connection validator to check against DBAPI_Sample ...
        CValidator validator("DBAPI_Sample");

        Check_Validator(factory_factory, validator);
    }

    // Another customized validator ...
    {

        // Connection validator to check against DBAPI_Sample ...
        CValidator01 validator("DBAPI_Sample");

        Check_Validator(factory_factory, validator);
    }

    // One more ...
    {
        // Connection validator to check against DBAPI_Sample ...
        CValidator02 validator("DBAPI_Sample");

        Check_Validator(factory_factory, validator);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_ConnFactory(void)
{
    enum {num_of_tests = 128};

    try {
        TSvrRef server01(new CDBServer("MS_DEV1"));
        TSvrRef server02(new CDBServer("MS_DEV2"));
        TSvrRef server03(new CDBServer("MSSQL67"));

        // Check CDBUDPriorityMapper ...
        {
            const string service_name("TEST_SERVICE_01");
            auto_ptr<CDBUDPriorityMapper> mapper(new CDBUDPriorityMapper());

            mapper->Add(service_name, server01);
            mapper->Add(service_name, server02);
            mapper->Add(service_name, server03);

            for (int i = 0; i < num_of_tests; ++i) {
                BOOST_CHECK(mapper->GetServer(service_name) == server01);
                BOOST_CHECK(mapper->GetServer(service_name) == server02);
                BOOST_CHECK(mapper->GetServer(service_name) == server03);

                BOOST_CHECK(mapper->GetServer(service_name) != server02);
                BOOST_CHECK(mapper->GetServer(service_name) != server03);
                BOOST_CHECK(mapper->GetServer(service_name) != server01);
            }
        }

        // Check CDBUDRandomMapper ...
        // DBUDRandomMapper is currently brocken ...
        if (false) {
            const string service_name("TEST_SERVICE_02");
            auto_ptr<CDBUDRandomMapper> mapper(new CDBUDRandomMapper());

            mapper->Add(service_name, server01);
            mapper->Add(service_name, server02);
            mapper->Add(service_name, server03);

            size_t num_server01 = 0;
            size_t num_server02 = 0;
            size_t num_server03 = 0;

            for (int i = 0; i < num_of_tests; ++i) {
                TSvrRef cur_server = mapper->GetServer(service_name);

                if (cur_server == server01) {
                    ++num_server01;
                } else if (cur_server == server02) {
                    ++num_server02;
                } else if (cur_server == server03) {
                    ++num_server03;
                } else {
                    BOOST_FAIL("Unknown server.");
                }
            }

            BOOST_CHECK_EQUAL(num_server01, num_server02);
            BOOST_CHECK_EQUAL(num_server02, num_server03);
            BOOST_CHECK_EQUAL(num_server03, num_server01);
        }

        // Check CDBConnectionFactory ...
        CheckConnFactory(CDBConnectionFactoryFactory);

        // Check CDBRedispatchFactory ...
        CheckConnFactory(CDBRedispatchFactoryFactory);

        // Future development ...
//         ncbi::CDbapiConnMgr::Instance().SetConnectionFactory(
//             new CDBConnectionFactory(
//                 mapper.release()
//             )
//         );

        // Restore original state ...
        DBLB_INSTALL_DEFAULT();
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_ConnPool(void)
{

    const string pool_name("test_pool");
    string sql;

    try {
        // Create pooled connection ...
        {
            auto_ptr<CDB_Connection> auto_conn(
                m_DS->GetDriverContext()->Connect(
                    m_args.GetServerName(),
                    m_args.GetUserName(),
                    m_args.GetUserPassword(),
                    0,
                    true, // reusable
                    pool_name
                    )
                );
            BOOST_CHECK( auto_conn.get() != NULL );

            sql  = "select @@version";

            auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
            BOOST_CHECK( auto_stmt.get() != NULL );

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );
            auto_stmt->DumpResults();
        }

        // Get pooled connection ...
        {
            auto_ptr<CDB_Connection> auto_conn(
                m_DS->GetDriverContext()->Connect(
                    "",
                    "",
                    "",
                    0,
                    true, // reusable
                    pool_name
                    )
                );
            BOOST_CHECK( auto_conn.get() != NULL );

            sql  = "select @@version";

            auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
            BOOST_CHECK( auto_stmt.get() != NULL );

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );
            auto_stmt->DumpResults();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_CDB_Object(void)
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
            CDB_Binary value_Binary;
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
        }

        // Check for NOT NULL a non-default constructor ...
        {
            CDB_Bit value_Bit(false);
            CDB_Int value_Int(1);
            CDB_SmallInt value_SmallInt(1);
            CDB_TinyInt value_TinyInt(1);
            CDB_BigInt value_BigInt(1);
            CDB_VarChar value_VarChar("ABC");
            CDB_Char value_Char(3, "ABC");
            CDB_LongChar value_LongChar(3, "ABC");
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
            CDB_Binary value_Binary;
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
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Variant(void)
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

            const CVariant variant_char( value_char );
            BOOST_CHECK( !variant_char.IsNull() );
            BOOST_CHECK( variant_char.GetString() == value_char );

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
    //        enum EDB_Type {
    //            eDB_Int,
    //            eDB_SmallInt,
    //            eDB_TinyInt,
    //            eDB_BigInt,
    //            eDB_VarChar,
    //            eDB_Char,
    //            eDB_VarBinary,
    //            eDB_Binary,
    //            eDB_Float,
    //            eDB_Double,
    //            eDB_DateTime,
    //            eDB_SmallDateTime,
    //            eDB_Text,
    //            eDB_Image,
    //            eDB_Bit,
    //            eDB_Numeric,

    //            eDB_UnsupportedType,
    //            eDB_LongChar,
    //            eDB_LongBinary
    //        };

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

            const CVariant variant_LongChar = CVariant::LongChar( value_char, strlen(value_char) );
            BOOST_CHECK( !variant_LongChar.IsNull() );
            BOOST_CHECK( variant_LongChar.GetString() == value_char );

            const CVariant variant_VarChar = CVariant::VarChar( value_char, strlen(value_char) );
            BOOST_CHECK( !variant_VarChar.IsNull() );
            BOOST_CHECK( variant_VarChar.GetString() == value_char );

            const CVariant variant_Char = CVariant::Char( strlen(value_char), const_cast<char*>(value_char) );
            BOOST_CHECK( !variant_Char.IsNull() );
            BOOST_CHECK( variant_Char.GetString() == value_char );

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

            CVariant variant_char( value_char );
            BOOST_CHECK( !variant_char.IsNull() );
            variant_char = CVariant( value_char );
            BOOST_CHECK( variant_char.GetString() == value_char );
            variant_char = value_char;
            BOOST_CHECK( variant_char.GetString() == value_char );

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
                new_time += 1;
                BOOST_CHECK( CVariant( value_CTime, eShort ) <
                             CVariant( new_time, eShort )
                             );
                BOOST_CHECK( CVariant( value_CTime, eLong ) <
                             CVariant( new_time, eLong )
                             );
            }

            // Check comparasion wit Uint1(0) ...
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


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_NCBI_LS(void)
{
    try {
        if (true) {
            static time_t s_tTimeOut = 30; // in seconds
            CDB_Connection* pDbLink = NULL;

            string sDbDriver(m_args.GetDriverName());
    //         string sDbServer("MSSQL57");
            string sDbServer("mssql57.nac.ncbi.nlm.nih.gov");
            string sDbName("NCBI_LS");
            string sDbUser("anyone");
            string sDbPasswd("allowed");
            string sErr;

            try {
                I_DriverContext* drv_context = m_DS->GetDriverContext();

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
                sErr = "CDB Exception from "+dbe.OriginatedFrom()+":"+
                    dbe.SeverityString(dbe.Severity())+": "+
                    dbe.Message();

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
            pRpc.reset( m_Connect->RPC("FindAccountMatchAll",20) );

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
                sError = "CDB Exception from "+dbe.OriginatedFrom()+":"+
                    dbe.SeverityString(dbe.Severity())+": "+
                    dbe.Message();
                bFetchErr = true;
            }

            BOOST_CHECK( acc_num > 0 );
            BOOST_CHECK_EQUAL( bFetchErr, false );
        }

        auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );
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
                auto_conn->GetCallableStatement("FindAccount", 20)
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
                auto_conn->GetCallableStatement("FindAccountMatchAll", 20)
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

            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt), 1 );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


void
CDBAPIUnitTest::Test_Authentication(void)
{
    try {
        // MSSQL10 has SSL certificate.
        // There is no MSSQL10 any more ...
    //     {
    //         auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );
    //
    //         auto_conn->Connect(
    //             "NCBI_NT\\anyone",
    //             "Perm1tted",
    //             "MSSQL10"
    //             );
    //
    //         auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );
    //
    //         auto_ptr<IResultSet> rs( auto_stmt->ExecuteQuery( "select @@version" ) );
    //         BOOST_CHECK( rs.get() != NULL );
    //         BOOST_CHECK( rs->Next() );
    //     }

        // No SSL certificate.
        if (true) {
            auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );

            auto_conn->Connect(
                "NAC\\anyone",
                "permitted",
                "MS_DEV2"
                );

            auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );

            IResultSet* rs = auto_stmt->ExecuteQuery("select @@version");
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
class CContextThread : public CThread
{
public:
    CContextThread(CDriverManager& dm, const CTestArguments& args);

protected:
    virtual ~CContextThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);

private:
    const CTestArguments    m_Args;
    CDriverManager&         m_DM;
    IDataSource*            m_DS;
};


CContextThread::CContextThread(CDriverManager& dm,
                               const CTestArguments& args) :
    m_Args(args),
    m_DM(dm),
    m_DS(NULL)
{
}


CContextThread::~CContextThread(void)
{
}


void*
CContextThread::Main(void)
{
    try {
        m_DS = m_DM.CreateDs(m_Args.GetDriverName(), &m_Args.GetDBParameters());
        return m_DS;
    } catch (const CDB_ClientEx&) {
        // Ignore it ...
    } catch (...) {
        _ASSERT(false);
    }

    return NULL;
}


void  CContextThread::OnExit(void)
{
    if (m_DS) {
        m_DM.DestroyDs(m_DS);
    }
}


void
CDBAPIUnitTest::Test_DriverContext_One(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK(succeeded_num >= 1);
}


void
CDBAPIUnitTest::Test_DriverContext_Many(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK_EQUAL(succeeded_num, int(eNumThreadsMax));
}


void CDBAPIUnitTest::Test_Decimal(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    sql = "declare @num decimal(6,2) set @num = 1.5 select @num as NumCol";
    auto_stmt->SendSql(sql);

    BOOST_CHECK( auto_stmt->HasMoreResults() );
    BOOST_CHECK( auto_stmt->HasRows() );
    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
    BOOST_CHECK( rs.get() != NULL );
    BOOST_CHECK( rs->Next() );

    DumpResults(auto_stmt.get());
}


void
CDBAPIUnitTest::Test_Query_Cancelation(void)
{
    string sql;

    try {
        // Cancel without a previously thrown exception.
        {
            // IStatement
            {
                auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

                // 1
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 2
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 3
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 4
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    int i = 0;
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();
                auto_stmt->Cancel();
            }

            // IColableStatement
            {
                auto_ptr<ICallableStatement> auto_stmt;

                // 1
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());

                // 2
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 3
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 4
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK(rs.get() != NULL);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 5
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK(rs.get() != NULL);
                    BOOST_CHECK(rs->Next());
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 6
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    int i = 0;
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();
                auto_stmt->Cancel();
            }

            // BCP
            {
            }
        }

        // Cancel with a previously thrown exception.
        {
            // IStatement
            {
                auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

                // 1
                try {
                    auto_stmt->SendSql("SELECT oops FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());

                // 2
                try {
                    sql = "SELECT name FROM sysobjects WHERE name = 'oops'";
                    auto_stmt->SendSql(sql);
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    rs->Next();
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());

                // 3
                try {
                    int i = 0;
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                    rs->Next();
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());
            }


            // IColableStatement
            {
                auto_ptr<ICallableStatement> auto_stmt;

                // 1
                try {
                    auto_stmt.reset(m_Conn->GetCallableStatement("sp_wrong"));
                    auto_stmt->Execute();
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }

                try {
                    auto_stmt->Execute();
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


void CDBAPIUnitTest::Test_Timeout(void)
{
    try {
        bool timeout_was_reported = false;
        auto_ptr<IConnection> auto_conn;
        I_DriverContext* dc = m_DS->GetDriverContext();
        unsigned int timeout = dc->GetTimeout();

        dc->SetTimeout(1);

        auto_conn.reset(m_DS->CreateConnection());
        BOOST_CHECK(auto_conn.get() != NULL);
        auto_conn->Connect(
            m_args.GetUserName(),
            m_args.GetUserPassword(),
            m_args.GetServerName()
            );

        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());

        try {
            auto_stmt->SendSql("waitfor delay '0:00:03'");
            BOOST_CHECK(auto_stmt->HasMoreResults());
        // } catch(const CDB_TimeoutEx&) {
        // We have to catch CDB_Exception here because of the ftds driver ...
        } catch(const CDB_Exception&) {
            timeout_was_reported = true;
            auto_stmt->Cancel();
        }

        // Check if connection is alive ...
        if (m_args.GetDriverName() != "ftds8"
            && !(m_args.GetDriverName() == "ftds" 
              && m_args.GetServerType() == CTestArguments::eSybase)    
            ) {
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );
        } else {
            PutMsgDisabled("CDBAPIUnitTest. Check if connection is alive.");
        }

        BOOST_CHECK(timeout_was_reported);

        dc->SetTimeout(timeout);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_SetLogStream(void)
{
    try {
        CNcbiOfstream logfile("dbapi_unit_test.log");

        m_DS->SetLogStream(&logfile);
        m_DS->SetLogStream(&logfile);

        // Test block ...
        {
            // No errors ...
            {
                auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                BOOST_CHECK(auto_conn.get() != NULL);

                Connect(auto_conn);

                auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK(auto_stmt->HasMoreResults());
                DumpResults(auto_stmt.get());
            }

            m_DS->SetLogStream(&logfile);

            // Force errors ...
            {
                auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                BOOST_CHECK(auto_conn.get() != NULL);

                Connect(auto_conn);

                auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                try {
                    auto_stmt->SendSql("SELECT oops FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                } catch(const CDB_Exception&) {
                    auto_stmt->Cancel();
                }
            }

            m_DS->SetLogStream(&logfile);
        }

        // Install user-defined error handler (eTakeOwnership)
        {
            {
                I_DriverContext* drv_context = m_DS->GetDriverContext();

                if (m_args.GetDriverName() == "odbc" ||
                    m_args.GetDriverName() == "odbcw" ||
                    m_args.GetDriverName() == "ftds_odbc"
                    ) {
                    drv_context->PushCntxMsgHandler(new CDB_UserHandler_Exception_ODBC,
                                                    eTakeOwnership
                                                    );
                    drv_context->PushDefConnMsgHandler(new CDB_UserHandler_Exception_ODBC,
                                                       eTakeOwnership
                                                       );
                } else {
                    CRef<CDB_UserHandler> hx(new CDB_UserHandler_Exception);
                    CDB_UserHandler::SetDefault(hx);
                    drv_context->PushCntxMsgHandler(new CDB_UserHandler_Exception,
                                                    eTakeOwnership);
                    drv_context->PushDefConnMsgHandler(new CDB_UserHandler_Exception,
                                                       eTakeOwnership);
                }
            }

            // Test block ...
            {
                // No errors ...
                {
                    auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                m_DS->SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    try {
                        auto_stmt->SendSql("SELECT oops FROM sysobjects");
                        BOOST_CHECK( auto_stmt->HasMoreResults() );
                    } catch(const CDB_Exception&) {
                        auto_stmt->Cancel();
                    }
                }

                m_DS->SetLogStream(&logfile);
            }
        }

        // Install user-defined error handler (eNoOwnership)
        {
            auto_ptr<CDB_UserHandler> h1;
            auto_ptr<CDB_UserHandler> h2;

            {
                I_DriverContext* drv_context = m_DS->GetDriverContext();

                if (m_args.GetDriverName() == "odbc" ||
                    m_args.GetDriverName() == "odbcw" ||
                    m_args.GetDriverName() == "ftds_odbc"
                    )
                {
                    h1.reset(new CDB_UserHandler_Exception_ODBC);
                    h2.reset(new CDB_UserHandler_Exception_ODBC);

                    drv_context->PushCntxMsgHandler(h1.get(),
                                                    eNoOwnership
                                                    );
                    drv_context->PushDefConnMsgHandler(h2.get(),
                                                       eNoOwnership
                                                       );
                } else {
                    h1.reset(new CDB_UserHandler_Exception);
                    h2.reset(new CDB_UserHandler_Exception);

                    drv_context->PushCntxMsgHandler(h1.get(),
                                                    eNoOwnership);
                    drv_context->PushDefConnMsgHandler(h2.get(),
                                                       eNoOwnership);
                }
            }

            // Test block ...
            {
                // No errors ...
                {
                    auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                m_DS->SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    try {
                        auto_stmt->SendSql("SELECT oops FROM sysobjects");
                        BOOST_CHECK( auto_stmt->HasMoreResults() );
                    } catch(const CDB_Exception&) {
                        auto_stmt->Cancel();
                    }
                }

                m_DS->SetLogStream(&logfile);
            }

            // Remove error-handlers ...
            {
                I_DriverContext* drv_context = m_DS->GetDriverContext();

                drv_context->PopCntxMsgHandler(h1.get());
                drv_context->PopDefConnMsgHandler(h2.get());
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Identity(void)
{
    string sql;
    Int8 table_id = 0;
    Int8 identity_id = 0;
    EDB_Type curr_type = eDB_UnsupportedType;
    const IResultSetMetaData* md = NULL;

    try {
        auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

        // Clean table ...
        auto_stmt->ExecuteUpdate("DELETE FROM " + GetTableName());

        // Insert data ...
        {
            // Pure SQL ...
            auto_stmt->ExecuteUpdate(
                "INSERT INTO " + GetTableName() + "(int_field) VALUES(1)");
        }

        // Retrieve data ...
        {
            sql  = " SELECT id, int_field FROM " + GetTableName();

            auto_stmt->SendSql( sql );

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());

            md = rs->GetMetaData();
            BOOST_CHECK(md);
            curr_type = md->GetType(1);
            BOOST_CHECK_EQUAL(curr_type, eDB_BigInt);

            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            table_id = id_value.GetInt8();
            BOOST_CHECK(!rs->Next());
        }

        // Retrieve identity ...
        {
            sql  = " SELECT CONVERT(NUMERIC(18, 0), @@identity) ";

            auto_stmt->SendSql(sql);

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());
            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            identity_id = id_value.GetInt8();
            BOOST_CHECK(!rs->Next());
        }

        BOOST_CHECK_EQUAL(table_id, identity_id);

        // Check identity type ...
        {
            sql  = " SELECT @@identity ";

            auto_stmt->SendSql(sql);

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());

            md = rs->GetMetaData();
            BOOST_CHECK(md);
            curr_type = md->GetType(1);
            BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            string id_str = rs->GetVariant(1).GetString();
            string id_num = rs->GetVariant(1).GetNumeric();
            BOOST_CHECK_EQUAL(id_str, id_num);

            BOOST_CHECK(!rs->Next());
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Bind(void)
{
}

void
CDBAPIUnitTest::Test_Execute(void)
{
}

void
CDBAPIUnitTest::Test_Exception(void)
{
}

void
CDBAPIUnitTest::Repeated_Usage(void)
{
}

void
CDBAPIUnitTest::Single_Value_Writing(void)
{
}

void
CDBAPIUnitTest::Single_Value_Reading(void)
{
}

void
CDBAPIUnitTest::Bulk_Reading(void)
{
}

void
CDBAPIUnitTest::Multiple_Resultset(void)
{
}

void
CDBAPIUnitTest::Error_Conditions(void)
{
}

void
CDBAPIUnitTest::Transactional_Behavior(void)
{
}


static
string GetSybaseClientVersion(void)
{
    CNcbiEnvironment env;
    string sybase_version = env.Get("SYBASE");

    sybase_version = sybase_version.substr(
        sybase_version.find_last_of('/') + 1
        );

    return sybase_version;
}

////////////////////////////////////////////////////////////////////////////////
CDBAPITestSuite::CDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
    bool Solaris = false;
    bool CompilerWorkShop = false;
    bool CompilerGCC = false;
    bool Irix = false;
    bool sybase_client_v125 = false;

#if defined(NCBI_OS_SOLARIS)
    Solaris = true;
#endif

#if defined(NCBI_COMPILER_WORKSHOP)
    CompilerWorkShop = true;
#endif

#if defined(NCBI_COMPILER_GCC)
    CompilerGCC = true;
#endif

#if defined(NCBI_OS_IRIX)
    Irix = true;
#endif

    const string sybase_version = GetSybaseClientVersion();
    if (NStr::CompareNocase(sybase_version, 0, 4, "12.5") == 0
        || NStr::CompareNocase(sybase_version, "current") == 0) {
        sybase_client_v125 = true;
    }

    // add member function test cases to a test suite
    boost::shared_ptr<CDBAPIUnitTest> DBAPIInstance(new CDBAPIUnitTest(args));
    boost::unit_test::test_case* tc = NULL;

    // Test DriverContext
    if (args.GetDriverName() == "ftds8" ||
        args.GetDriverName() == "ftds63" ||
        args.GetDriverName() == "ftds_dblib" ||
        // args.GetDriverName() == "dblib" ||
        args.GetDriverName() == "msdblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_One,
                                   DBAPIInstance);
        add(tc);
    } else {
        // PutMsgDisabled("Test_DriverContext_One");
    }

    if (args.GetDriverName() == "ctlib"
        || args.GetDriverName() == "ftds"
        || args.GetDriverName() == "odbc"
        || args.GetDriverName() == "ftds_odbc"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_Many,
                                   DBAPIInstance);
        add(tc);
    } else {
        // PutMsgDisabled("Test_DriverContext_Many");
    }

    boost::unit_test::test_case* tc_init =
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::TestInit, DBAPIInstance);

    add(tc_init);

    if (args.GetServerType() == CTestArguments::eMsSql ||
        args.GetServerType() == CTestArguments::eMsSql2005
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ConnFactory,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_ConnFactory");
    }

    if (!(args.GetDriverName() == "ftds" 
          && args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != "msdblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ConnPool,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_ConnPool");
    }


    if (args.GetServerType() == CTestArguments::eSybase) {
        if (args.GetDriverName() != "dblib"
            && args.GetDriverName() != "ftds_dblib"
            && args.GetDriverName() != "ftds8"
            && args.GetDriverName() != "ftds"
            // ctlib on x86_64 requires appropriate Sybase client ...
            && args.GetDriverName() != "ctlib"
           ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DropConnection,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_DropConnection");
        }
    }

    if ((args.GetServerType() == CTestArguments::eMsSql
        || args.GetServerType() == CTestArguments::eMsSql2005)
        && args.GetDriverName() != "msdblib"
        && args.GetDriverName() != "ftds_dblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BlobStore,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_BlobStore");
    }

    // It looks like ftds on WorkShop55_550-DebugMT64 doesn't work ...
    if ((args.GetDriverName() == "ftds8" &&
         !(Solaris && CompilerWorkShop) &&
         !Irix
         )
        || args.GetDriverName() == "dblib"
        || args.GetDriverName() == "msdblib"
        || (args.GetDriverName() == "ctlib" && !Solaris)
        || args.GetDriverName() == "ftds"
        || args.GetDriverName() == "ftds_odbc"
        || args.GetDriverName() == "odbc"
        || args.GetDriverName() == "odbcw"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Timeout,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Timeout");
    }


    // There is a problem with ftds driver
    // on GCC_340-ReleaseMT--i686-pc-linux2.4.23
    // There is a problem with ftds64 driver
    // on orkShop55_550-ReleaseMT
    if (args.GetDriverName() != "ftds8"
        && args.GetDriverName() != "ftds"
        && args.GetDriverName() != "ftds_dblib"
        && args.GetDriverName() != "msdblib"
        && !(args.GetDriverName() == "ftds8" && Solaris)
        && !(args.GetDriverName() == "ftds" && Solaris)
        && !(args.GetDriverName() == "ftds8"
             && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == "ftds"
             && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == "ftds_odbc"
             && args.GetServerType() == CTestArguments::eMsSql2005)
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Query_Cancelation,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Query_Cancelation");
    }


    if ((args.GetServerType() == CTestArguments::eMsSql
         || args.GetServerType() == CTestArguments::eMsSql2005) &&
        (args.GetDriverName() == "ftds_odbc"
         || args.GetDriverName() == "ftds")
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Authentication,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Authentication");
    }

    //
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CDB_Object, DBAPIInstance));
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant, DBAPIInstance));
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CDB_Exception,
                              DBAPIInstance));

    if (!(args.GetDriverName() == "ftds"
          && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
        && !(args.GetDriverName() == "ftds8"
          && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == "ftds_dblib"
          && args.GetServerType() == CTestArguments::eSybase)
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Create_Destroy,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Create_Destroy");
    }

    //
    if (!(args.GetDriverName() == "ftds"
          && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
        && !(args.GetDriverName() == "ftds8"
          && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == "ftds_dblib"
          && args.GetServerType() == CTestArguments::eSybase)
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Multiple_Close,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Multiple_Close");
    }

    if (args.IsBCPAvailable()
        && !Solaris // Requires Sybase client 12.5
        && args.GetDriverName() != "odbc"
        && args.GetDriverName() != "ftds_dblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        if (args.GetServerType() == CTestArguments::eMsSql
            || args.GetServerType() == CTestArguments::eMsSql2005
           ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing2, DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_Bulk_Writing2");
        }
    } else {
        PutMsgDisabled("Test_Bulk_Writing");
    }


    if (args.GetDriverName() != "ftds_dblib"
        && !(args.GetDriverName() == "ftds"
          && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == "ftds8"
          && args.GetServerType() == CTestArguments::eSybase)
        ) {
        boost::unit_test::test_case* tc_parameters =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_StatementParameters,
                                  DBAPIInstance);
        tc_parameters->depends_on(tc_init);
        add(tc_parameters);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetRowCount,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        tc->depends_on(tc_parameters);
        add(tc);

        // Doesn't work at the moment ...
        if (args.GetDriverName() != "ftds8"
            && args.GetDriverName() != "ftds_odbc"
            ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB_LowLevel,
                                   DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_LOB_LowLevel");
        }

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NULL, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        // Cursors work either with ftds + MSSQL or with ctlib at the moment ...
        if (!(args.GetDriverName() == "ftds8"
                && args.GetServerType() == CTestArguments::eSybase)
            && args.GetDriverName() != "dblib" // Code will hang up with dblib for some reason ...
            && args.GetDriverName() != "msdblib" // doesn't work ...
            ) {
            //
            boost::unit_test::test_case* tc_cursor =
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_parameters);
            add(tc);

            if (args.GetDriverName() != "ftds_odbc"
                && args.GetDriverName() != "odbc"
                && args.GetDriverName() != "odbcw"
                && args.GetDriverName() != "ftds8"
                ) {
                tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor2,
                                        DBAPIInstance);
                tc->depends_on(tc_init);
                tc->depends_on(tc_parameters);
                add(tc);
            } else {
                PutMsgDisabled("Test_Cursor2");
            }

            // Does not work with all databases and drivers currently ...
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_cursor);
            add(tc);

//             tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB2,
//             DBAPIInstance);
//             tc->depends_on(tc_init);
//             tc->depends_on(tc_cursor);
//             add(tc);

            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BlobStream,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_cursor);
            add(tc);
        } else {
            PutMsgDisabled("Test_Cursor");
            PutMsgDisabled("Test_LOB");
            PutMsgDisabled("Test_BlobStream");
        }

        // Not completed yet ...
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NVARCHAR,
//                                    DBAPIInstance);
//         tc->depends_on(tc_init);
//         tc->depends_on(tc_parameters);
//         add(tc);

        if (args.GetDriverName() == "odbcw"
    //         args.GetDriverName() == "ftds63" ||
    //         args.GetDriverName() == "ftds_odbc" ||
    //         || args.GetDriverName() == "ftds"
            ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Unicode,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_parameters);
            add(tc);
        } else {
            PutMsgDisabled("Test_Unicode");
        }
    } else {
        PutMsgDisabled("Test_StatementParameters");
        PutMsgDisabled("Test_NULL");
        PutMsgDisabled("Test_GetRowCount");
    }


    if (args.IsBCPAvailable()
        && args.GetDriverName() != "ftds_dblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_BulkInsertBlob");
    }

    if (args.IsBCPAvailable()) {
        if(args.GetDriverName() != "ftds_dblib"
          )
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob_LowLevel,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_BulkInsertBlob_LowLevel");
        }
    }

    // Test_UserErrorHandler depends on Test_Exception_Safety ...
    if (!(args.GetDriverName() == "ftds"
              && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
        ) {
        boost::unit_test::test_case* except_safety_tc =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Exception_Safety,
                                  DBAPIInstance);
        except_safety_tc->depends_on(tc_init);
        add(except_safety_tc);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        tc->depends_on(except_safety_tc);
        add(tc);
    } else {
        PutMsgDisabled("Test_Exception_Safety");
        PutMsgDisabled("Test_UserErrorHandler");
    }

    {
        if (!(args.GetDriverName() == "ftds"
              && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
            && !(args.GetDriverName() == "ftds8"
                 && args.GetServerType() == CTestArguments::eSybase)
          ) {
            boost::unit_test::test_case* select_stmt_tc =
                BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmt,
                                      DBAPIInstance);
            select_stmt_tc->depends_on(tc_init);
            add(select_stmt_tc);

            // There is a problem with the ftds8 driver and Sybase ...
            if (!(args.GetDriverName() == "ftds8"
                  && args.GetServerType() == CTestArguments::eSybase)
                && args.GetDriverName() != "ftds_dblib"
                ) {
                tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Recordset,
                                           DBAPIInstance);
                tc->depends_on(tc_init);
                tc->depends_on(select_stmt_tc);
                add(tc);
            } else {
                PutMsgDisabled("Test_Recordset");
            }

            //
            if (false) {
                tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_MetaData,
                                           DBAPIInstance);
                tc->depends_on(tc_init);
                tc->depends_on(select_stmt_tc);
                add(tc);
            } else {
                PutMsgDisabled("Test_MetaData");
            }

            //
            if ((args.GetServerType() == CTestArguments::eMsSql
                  || args.GetServerType() == CTestArguments::eMsSql2005)
                 && args.GetDriverName() != "msdblib"
                 && args.GetDriverName() != "dblib"
                 ) {
                //
                boost::unit_test::test_case* select_xml_stmt_tc =
                    BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmtXML,
                                           DBAPIInstance);
                tc->depends_on(tc_init);
                tc->depends_on(select_stmt_tc);
                add(select_xml_stmt_tc);

                //
                if (args.GetDriverName() != "odbc") {
                    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Unicode_Simple,
                                            DBAPIInstance);
                    tc->depends_on(tc_init);
                    tc->depends_on(select_xml_stmt_tc);
                    add(tc);
                } else {
                    PutMsgDisabled("Test_Unicode_Simple");
                }
            } else {
                PutMsgDisabled("Test_SelectStmtXML");
            }
        } else {
            PutMsgDisabled("Test_SelectStmt");
        }

        // ctlib/ftds64 will work in case of protocol version 12.5 only
        // ftds + Sybase and dblib won't work because of the early
        // protocol versions.
        if ((((args.GetDriverName() == "ftds8"
               && (args.GetServerType() == CTestArguments::eMsSql
                   || args.GetServerType() == CTestArguments::eMsSql2005))
              || args.GetDriverName() == "ftds63"
              || args.GetDriverName() == "odbc"
              || args.GetDriverName() == "odbcw"
              || args.GetDriverName() == "ftds_odbc"
              // !!! This driver won't work with Sybase because it supports
              // CS_VERSION_110 only. !!!
              || (args.GetDriverName() == "ftds"
                  && (args.GetServerType() == CTestArguments::eMsSql
                      || args.GetServerType() == CTestArguments::eMsSql2005))
              // || args.GetDriverName() == "ftds_dblib"
              ))
             // args.GetDriverName() == "ctlib" ||
            ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Insert,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_Insert");
        }
    }

    if (!(args.GetDriverName() == "ftds"
          && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
        && !(args.GetDriverName() == "ftds8"
          && args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != "ftds_dblib"
        ) {

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant2, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetTotalColumns,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Procedure");
        PutMsgDisabled("Test_Variant2");
        PutMsgDisabled("Test_GetTotalColumns");
    }

    if ( (args.GetDriverName() == "ftds8"
          || args.GetDriverName() == "ftds63"
          || args.GetDriverName() == "ftds_dblib"
          || args.GetDriverName() == "ftds"
          // || args.GetDriverName() == "ftds_odbc"  // This is a big problem ....
          ) &&
         (args.GetServerType() == CTestArguments::eMsSql
          || args.GetServerType() == CTestArguments::eMsSql2005) ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UNIQUE, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_UNIQUE");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != "odbc"
        && args.GetDriverName() != "msdblib"     // Just does'nt work for some reason
        && args.GetDriverName() != "ftds_dblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTimeBCP,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_DateTimeBCP");
    }

    // !!! There are still problems ...
    if (args.IsBCPAvailable()) {
        if (args.GetDriverName() != "odbc"
            && args.GetDriverName() != "ftds" // Something is completely wrong ...
            // && !(args.GetDriverName() == "ftds"
            //      && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
            && args.GetDriverName() != "ctlib"
            && args.GetDriverName() != "ftds_dblib"
           )
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Overflow,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_Bulk_Overflow");
        }
    }

    // The only problem with this test is that the LINK_OS server is not
    // available from time to time.
    if(args.GetServerType() == CTestArguments::eSybase)
    {
        if (args.GetDriverName() != "ftds8"
            && args.GetDriverName() != "ftds"
            && args.GetDriverName() != "ftds_dblib"
           ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Iskhakov,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            PutMsgDisabled("Test_Iskhakov");
        }
    }


    if ( args.GetDriverName() != "ftds_odbc"  // Strange ....
         && !(args.GetDriverName() == "ftds"
              && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
         && !(args.GetDriverName() == "ftds8"
              && args.GetServerType() == CTestArguments::eSybase)
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTime,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_DateTime");
    }

    // Valid for all drivers ...
    if (args.GetDriverName() != "ftds_dblib"
        && !(args.GetDriverName() == "ftds"
          && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Decimal,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SetLogStream,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Decimal");
        PutMsgDisabled("Test_SetLogStream");
    }

    if (!(((args.GetDriverName() == "ftds"
                || args.GetDriverName() == "ftds8"
                || args.GetDriverName() == "ftds_dblib"
              )
           && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
          || args.GetDriverName() == "msdblib"
          )
        )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Identity,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_Identity");
    }


    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler_LT,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);


    // This test is not supposed to be run every day.
    if (false) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_N_Connections,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        // PutMsgDisabled("Test_N_Connections");
    }

//     if (args.GetServerType() == CTestArguments::eMsSql
//         && args.GetDriverName() != "odbc" // Doesn't work ...
//         && args.GetDriverName() != "odbcw" // Doesn't work ...
//         // && args.GetDriverName() != "ftds_odbc"
//         && args.GetDriverName() != "msdblib"
//         ) {
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NCBI_LS, DBAPIInstance);
//         add(tc);
//     }

    // development ....
    if (false) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_HasMoreResults,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        PutMsgDisabled("Test_HasMoreResults");
    }

}

CDBAPITestSuite::~CDBAPITestSuite(void)
{
}


////////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(int argc, char * argv[]) :
    m_Arguments(argc, argv)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    // Specify USAGE context
    arg_desc->SetUsageContext(m_Arguments.GetProgramBasename(),
                              "dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "msdblib", "odbc", \
                      "ftds_dblib", "ftds_odbc", "ftds64", "odbcw", "ftds8"

#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "TAPER"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "ftds_dblib", \
                      "ftds_odbc", "ftds64", "ftds8"
#else
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ftds", "ftds63", "ftds_dblib", "ftds_odbc", \
                      "ftds64", "ftds8"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("d", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString,
                            DEF_DRIVER);
    arg_desc->SetConstraint("d", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddDefaultKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString,
                            "DBAPI_Sample");

    arg_desc->AddOptionalKey("v", "version",
                            "TDS protocol version",
                            CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("H", "gateway_host",
                            "DBAPI gateway host",
                            CArgDescriptions::eString);

    arg_desc->AddDefaultKey("p", "gateway_port",
                            "DBAPI gateway port",
                            CArgDescriptions::eInteger,
                            "65534");

    auto_ptr<CArgs> args_ptr(arg_desc->CreateArgs(m_Arguments));
    const CArgs& args = *args_ptr;


    // Get command-line arguments ...
    m_DriverName    = args["d"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();

    if ( args["v"].HasValue() ) {
        m_TDSVersion = args["v"].AsString();
    } else {
        m_TDSVersion.clear();
    }
    if ( args["H"].HasValue() ) {
        m_GatewayHost = args["H"].AsString();
    } else {
        m_GatewayHost.clear();
    }

    m_GatewayPort  = args["p"].AsString();

    SetDatabaseParameters();
}


CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    if ( NStr::CompareNocase(GetServerName(), "STRAUSS") == 0
         || NStr::CompareNocase(GetServerName(), "MOZART") == 0
         || NStr::CompareNocase(GetServerName(), "OBERON") == 0
         || NStr::CompareNocase(GetServerName(), "TAPER") == 0
         || NStr::CompareNocase(GetServerName(), "SCHUMANN") == 0
         || NStr::CompareNocase(GetServerName(), "BARTOK") == 0
         ) {
        return eSybase;
    } else if (NStr::CompareNocase(GetServerName(), 0, 7, "MSSQL67") == 0) {
        return eMsSql2005;
    } else if (NStr::CompareNocase(GetServerName(), 0, 6, "MS_DEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 5, "MSSQL") == 0
                || NStr::CompareNocase(GetServerName(), 0, 7, "OAMSDEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 6, "QMSSQL") == 0) {
        return eMsSql;
    }

    return eUnknown;
}


string
CTestArguments::GetProgramBasename(void) const
{
    return m_Arguments.GetProgramBasename();
}


bool
CTestArguments::IsBCPAvailable(void) const
{
#if defined(NCBI_OS_SOLARIS)
    const bool os_solaris = true;
#else
    const bool os_solaris = false;
#endif

    if (os_solaris && HOST_CPU[0] == 'i' &&
        (GetDriverName() == "dblib" || GetDriverName() == "ctlib")
        ) {
        // Solaris Intel native Sybase drivers ...
        // There is no apropriate client
        return false;
    } else if ( GetDriverName() == "ftds_odbc"
         || GetDriverName() == "odbcw"
         || GetDriverName() == "msdblib"
         || ((GetDriverName() == "ftds" || GetDriverName() == "ftds8") && GetServerType() == CTestArguments::eSybase)
         ) {
        return false;
    }

    return true;
}


void
CTestArguments::SetDatabaseParameters(void)
{
    if ( m_TDSVersion.empty() ) {
        if ( GetDriverName() == "ctlib" ) {
            // m_DatabaseParameters["version"] = "125";
            // Set max_connect to let open more than 30 connections with ctlib.
            // m_DatabaseParameters["max_connect"] = "100";
        } else if ( GetDriverName() == "dblib"  &&
                    GetServerType() == eSybase ) {
            // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
            // BcpIn to it using protocol version other than "100".
            m_DatabaseParameters["version"] = "100";
        } else if (GetDriverName() == "ftds8") {
            switch (GetServerType()) {
            case eSybase:
                m_DatabaseParameters["version"] = "42";
                break;
//             case eMsSql2005:
//                 m_DatabaseParameters["version"] = "70";
//                 break;
            default:
                break;
            }
        } else if ( (GetDriverName() == "ftds63" ||
                     GetDriverName() == "ftds_dblib") &&
                    GetServerType() == eSybase ) {
            // ftds8 work with Sybase databases using protocol v42 only ...
            m_DatabaseParameters["version"] = "42";
        } else if (GetDriverName() == "ftds_odbc") {
            switch (GetServerType()) {
            case eSybase:
                m_DatabaseParameters["version"] = "50";
                break;
            case eMsSql2005:
                m_DatabaseParameters["version"] = "70";
                break;
            default:
                break;
            }
        } else if (GetDriverName() == "ftds") {
            switch (GetServerType()) {
            case eSybase:
                m_DatabaseParameters["version"] = "125";
                break;
            case eMsSql2005:
                m_DatabaseParameters["version"] = "70";
                break;
            default:
                break;
            }
        }
    } else {
        m_DatabaseParameters["version"] = m_TDSVersion;
    }

    if ( (GetDriverName() == "ftds8" ||
          GetDriverName() == "ftds63" ||
          GetDriverName() == "ftds" ||
//           GetDriverName() == "ftds_odbc"  ||
          GetDriverName() == "ftds_dblib")
         && GetServerType() == eMsSql) {
        m_DatabaseParameters["client_charset"] = "UTF-8";
    }

    if (!m_GatewayHost.empty()) {
        m_DatabaseParameters["host_name"] = m_GatewayHost;
        m_DatabaseParameters["port_num"] = m_GatewayPort;
        m_DatabaseParameters["driver_name"] = GetDriverName();
    }
}


END_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////
test_suite*
init_unit_test_suite( int argc, char * argv[] )
{
//     ncbi::CException::SetStackTraceLevel(ncbi::eDiag_Warning);

    // Configure UTF ...
    // boost::unit_test_framework::unit_test_log::instance().set_log_format( "XML" );
    // boost::unit_test_framework::unit_test_result::set_report_format( "XML" );

    test_suite* test = BOOST_TEST_SUITE("DBAPI Unit Test.");

    test->add(new ncbi::CDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test;
}

