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
#include <connect/ncbi_core_cxx.hpp>

#include <ncbi_source_ver.h>
#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/util/blobstore.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>

#include "dbapi_unit_test.hpp"
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>

#include <common/test_assert.h>  /* This header must go last */


////////////////////////////////////////////////////////////////////////////////
static const char* ftds8_driver = "ftds8";
static const char* ftds64_driver = "ftds";
static const char* ftds_driver = ftds64_driver;

static const char* ftds_odbc_driver = "ftds_odbc";
static const char* ftds_dblib_driver = "ftds_dblib";

static const char* odbc_driver = "odbc";
static const char* ctlib_driver = "ctlib";
static const char* dblib_driver = "dblib";


// #define DBAPI_BOOST_FAIL(ex)
//     ERR_POST(ex);
//     BOOST_FAIL(ex.GetMsg())

#define DBAPI_BOOST_FAIL(ex) \
    BOOST_FAIL(ex.GetMsg())

BEGIN_NCBI_SCOPE

static
string GetSybaseClientVersion(void);


/////////////////////////////////////////////////////////////////////////////
inline
CDiagCompileInfo GetBlankCompileInfo(void)
{
    return CDiagCompileInfo();
}

////////////////////////////////////////////////////////////////////////////////
enum { max_text_size = 8000 };
#define CONN_OWNERSHIP  eTakeOwnership
static const char* msg_record_expected = "Record expected";

////////////////////////////////////////////////////////////////////////////////
class CDB_UserHandler_Exception_NoThrow :
    public CDB_UserHandler
{
public:
    virtual ~CDB_UserHandler_Exception_NoThrow(void);

    virtual bool HandleIt(CDB_Exception* ex);
};


CDB_UserHandler_Exception_NoThrow::~CDB_UserHandler_Exception_NoThrow(void)
{
}

bool
CDB_UserHandler_Exception_NoThrow::HandleIt(CDB_Exception* ex)
{
    if (!ex)
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    // Do not throw exceptions ...
    // ex->Throw();

    return true;
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
    if (m_args.DriverAllowsMultipleContexts()) {
        CTestArguments::TDatabaseParameters params = m_args.GetDBParameters();
        params["version"] = "125";
        m_AutoDC.reset(Get_I_DriverContext(m_args.GetDriverName(), &params));
    }
    
    // SetDiagFilter(eDiagFilter_Trace, "!/dbapi/driver/ctlib");
}

CDBAPIUnitTest::~CDBAPIUnitTest(void)
{
//     I_DriverContext* drv_context = GetDS().GetDriverContext();
//
//     drv_context->PopDefConnMsgHandler( m_ErrHandler.get() );
//     drv_context->PopCntxMsgHandler( m_ErrHandler.get() );

    m_Conn.reset(NULL);
    m_DM.DestroyDs(m_DS);
    m_DS = NULL;
}


///////////////////////////////////////////////////////////////////////////////
I_DriverContext& CDBAPIUnitTest::GetDriverContext(void)
{
    I_DriverContext* cntx = NULL;

    if (m_args.DriverAllowsMultipleContexts()) {
        cntx = m_AutoDC.get();
    } else {
        cntx = GetDS().GetDriverContext();
    }

    _ASSERT(cntx);

    return *cntx;
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

        I_DriverContext* drv_context = GetDS().GetDriverContext();

        if (m_args.GetTestConfiguration() != CTestArguments::eWithoutExceptions) {
            if (m_args.IsODBCBased()) {
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
        }

        m_Conn.reset(GetDS().CreateConnection( CONN_OWNERSHIP ));
        BOOST_CHECK(m_Conn.get() != NULL);

        Connect(m_Conn);

        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

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

        sql  = " CREATE TABLE #test_unicode_table ( \n";
        sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
        sql += "    nvc255_field NVARCHAR(255) NULL \n";
//        sql += "    nvc255_field VARCHAR(255) NULL \n";
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
    EDiagSev getMaxSeverity() {
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
    EDiagSev m_max_severity;
    bool     m_Succeed;
};


CTestErrHandler::CTestErrHandler()
: m_max_severity(eDiag_Info)
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

    if (ex && ex->GetSeverity() > m_max_severity)
    {
        m_max_severity = ex->GetSeverity();
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
void CDBAPIUnitTest::Test_UnicodeNB(void)
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
void CDBAPIUnitTest::Test_Unicode(void)
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
void CDBAPIUnitTest::Test_VARCHAR_MAX(void)
{
    string sql;
    const string table_name = "#test_varchar_max_table";
    // const string table_name = "DBAPI_Sample..test_varchar_max_table";
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

        // SQL value injection technique ... 
        {
            // Clean table ...
            {
                sql = "DELETE FROM " + table_name;

                auto_stmt->ExecuteUpdate( sql );
            }

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
        if (false) {
            const CVariant vc_max_value(msg);

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

    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_VARCHAR_MAX_BCP(void)
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
                col2.Append(msg.c_str(), msg.size());

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

                        rs->BindBlobToVariant(true);
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
void CDBAPIUnitTest::Test_CHAR(void)
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
void CDBAPIUnitTest::Test_NVARCHAR(void)
{
    string sql;

    try {
        if (false) {
            string Title69;

            auto_ptr<IConnection> conn(
                GetDS().CreateConnection(CONN_OWNERSHIP)
                );

            conn->Connect("anyone","allowed","MSSQL69", "PMC3");
//             conn->Connect("anyone","allowed","MSSQL42", "PMC3");
            auto_ptr<IStatement> auto_stmt(conn->GetStatement());

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
void CDBAPIUnitTest::Test_NTEXT(void)
{
    string sql;

    try {
        string ins_value = "asdfghjkl";
        char buffer[20];

        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        sql = "SET TEXTSIZE 2147483647";
        auto_stmt->ExecuteUpdate(sql);

        sql = "create table #test_ntext (txt_fld ntext)";
        auto_stmt->ExecuteUpdate(sql);

        sql = "insert into #test_ntext values ('" + ins_value + "')";
        auto_stmt->ExecuteUpdate(sql);

        sql = "SELECT txt_fld from #test_ntext";
        auto_stmt->SendSql(sql);
        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                rs->BindBlobToVariant(true);
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


// Based on Soussov's API.
void CDBAPIUnitTest::Test_Iskhakov(void)
{
    string sql;

    try {
        auto_ptr<CDB_Connection> auto_conn(
            GetDriverContext().Connect(
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
                GetDS().CreateConnection(eTakeOwnership)
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
                );
            Connect(local_conn);

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
                GetDS().CreateConnection(eTakeOwnership)
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
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
void
CDBAPIUnitTest::Test_DateTime(void)
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
void
CDBAPIUnitTest::Test_DateTimeBCP(void)
{
    string table_name("#test_bcp_datetime");
    // string table_name("DBAPI_Sample..test_bcp_datetime");
    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
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
                    GetConnection().GetBulkInsert(table_name)
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
                    GetConnection().GetBulkInsert(table_name)
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
CDBAPIUnitTest::Test_LOB_Replication(void)
{
    string sql, sql2;
    CDB_Text txt;

    try {
        auto_ptr<IConnection> auto_conn(
            GetDS().CreateConnection(CONN_OWNERSHIP)
            );
        auto_conn->Connect(
                "anyone",
                "allowed",
                "MSSQL67"
                );
        CDB_Connection* conn = auto_conn->GetCDB_Connection();
        auto_ptr<CDB_LangCmd> auto_stmt;

        /* DBAPI_Sample..blobRepl is replicated from MSSQL67 to MSSQL8 */
        sql = "SELECT blob, TEXTPTR(blob) FROM DBAPI_Sample..blobRepl";

        {
            auto_stmt.reset(conn->LangCmd("BEGIN TRAN"));
            auto_stmt->Send();
            auto_stmt->DumpResults();

            auto_stmt.reset(conn->LangCmd(sql));
            auto_stmt->Send();

            txt.Append(NStr::Int8ToString(CTime(CTime::eCurrent).GetTimeT()));

            auto_ptr<I_ITDescriptor> descr[2];
            int i = 0;
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
                    descr[i].reset(rs->GetImageOrTextDescriptor());
                    ++i;

                }

                BOOST_CHECK(descr[0].get() != NULL);
                BOOST_CHECK(descr[1].get() != NULL);
            }

            conn->SendData(*descr[0], txt);
            txt.MoveTo(0);
            conn->SendData(*descr[1], txt);

            auto_stmt.reset(conn->LangCmd("COMMIT TRAN"));
            auto_stmt->Send();
            auto_stmt->DumpResults();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_LOB(void)
{
    // static char clob_value[] = "1234567890";
    static string clob_value("1234567890");
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& out = auto_cursor->GetBlobOStream(1,
                                                           clob_value.size(),
                                                           eDisableLog);
                out.write(clob_value.c_str(), clob_value.size());
                out.flush();
            }

    //         auto_stmt->SendSql( sql );
    //         while( auto_stmt->HasMoreResults() ) {
    //             if( auto_stmt->HasRows() ) {
    //                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //                 while ( rs->Next() ) {
    //                     ostream& out = rs->GetBlobOStream(clob_value.size(), eDisableLog);
    //                     out.write(clob_value.c_str(), clob_value.size());
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
                        const string str_value = value.GetString();

                        BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                        BOOST_CHECK_EQUAL(clob_value, str_value);
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
                        while ((read_bytes = value.Read(buff, sizeof(buff)))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK_EQUAL(result, clob_value);
                    }
                }
            }
        }

        // Read Blob
        // There is a bug in implementation of ReadItem in ftds8 driver.
        if (m_args.GetDriverName() != ftds8_driver) {
            string result;
            char buff[3];

            sql = "SELECT text_field FROM "+ GetTableName();

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                    while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, clob_value);
                }

            }
        } else {
            m_args.PutMsgDisabled("Test_LOB - Read Blob");
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

                    if (false) {
                        // Use cursor with parameters.
                        // Unfortunately that doesn't work with the new ftds
                        // driver ....
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = @int_field ";

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));
                            auto_cursor->SetParam(CVariant(Int4(ind)), "@int_field");

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        clob_value.size(),
                                        eDisableLog);
                                out.write(clob_value.c_str(), clob_value.size());
                                out.flush();
                            }
                        }
                    } else {
                        sql  = " SELECT text_field FROM " + GetTableName();
                        sql += " WHERE int_field = " + NStr::IntToString(ind);

                        if (ind % 2 == 0) {
                            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

                            // blobRs should be destroyed before auto_cursor ...
                            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                            while(blobRs->Next()) {
                                ostream& out = auto_cursor->GetBlobOStream(1,
                                        clob_value.size(),
                                        eDisableLog);
                                out.write(clob_value.c_str(), clob_value.size());
                                out.flush();
                            }
                        }
                    }
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, GetTableName())
                                  );
            }

            // Read blob via Read method ...
            // ftds_odbc is hanging up ...
            if (m_args.GetDriverName() != ftds_odbc_driver) {
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
                                while ((read_bytes = value.Read(buff, sizeof(buff)))) {
                                    result += string(buff, read_bytes);
                                }

                                BOOST_CHECK_EQUAL(result, clob_value);
                            } else {
                                BOOST_CHECK(value.IsNull());
                            }
                        }
                    }
                }
            } else {
                m_args.PutMsgDisabled("Test_LOB Read blob via Read method");
            }

            // Read Blob
            if (m_args.GetDriverName() != ftds8_driver) {
                char buff[3];

                sql = "SELECT text_field FROM "+ GetTableName();

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                            while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(!is_null);
                            BOOST_CHECK_EQUAL(result, clob_value);
                        } else {
                            size_t read_bytes = 0;
                            bool is_null = true;

                            while ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                                result += string(buff, read_bytes);
                            }

                            BOOST_CHECK(is_null);
                        }
                    }

                }
            } else {
                m_args.PutMsgDisabled("Test_LOB Read Read Blob");
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

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


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_LOB_Multiple(void)
{
    const string table_name = "#test_lob_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Prepare data ...
        {
            // Create table ...
            if (table_name[0] =='#') {
                sql =
                    "CREATE TABLE " + table_name + " ( \n"
                    "   id NUMERIC IDENTITY NOT NULL, \n"
                    "   text01  TEXT NULL, \n" 
                    "   text02  TEXT NULL, \n" 
                    "   image01 IMAGE NULL, \n" 
                    "   image02 IMAGE NULL \n" 
                    ") \n";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                auto_stmt->ExecuteUpdate( sql );
                
                /*
                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
                */
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test_lob_multiple", sql));

                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                while(blobRs->Next()) {
                    for (int pos = 1; pos <= 4; ++pos) {
                        ostream& out =
                            auto_cursor->GetBlobOStream(pos,
                                                        clob_value.size(),
                                                        eDisableLog
                                                        );
                        out.write(clob_value.c_str(), clob_value.size());
                        out.flush();
                        BOOST_CHECK(out.good());
                    }
                }
            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    rs->BindBlobToVariant(true);

                    while ( rs->Next() ) {
                        for (int pos = 1; pos <= 4; ++pos) {
                            const CVariant& value = rs->GetVariant(pos);

                            BOOST_CHECK( !value.IsNull() );

                            size_t blob_size = value.GetBlobSize();
                            BOOST_CHECK_EQUAL(clob_value.size(), blob_size);
                            BOOST_CHECK_EQUAL(value.GetString(), clob_value);
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
CDBAPIUnitTest::Test_LOB_LowLevel(void)
{
    string sql;
    CDB_Text txt;

    try {
        bool rc = false;
        auto_ptr<CDB_LangCmd> auto_stmt;
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
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


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_LOB_Multiple_LowLevel(void)
{
    const string table_name = "#test_lob_ll_multiple";
    static string clob_value("1234567890");
    string sql;
    // enum {num_of_records = 10};

    try {
        bool rc = false;
        auto_ptr<CDB_LangCmd> auto_stmt;
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
        BOOST_CHECK(conn != NULL);

        // Prepare data ...
        {
            // Create table ...
            if (table_name[0] =='#') {
                sql =
                    "CREATE TABLE " + table_name + " ( \n"
                    "   id NUMERIC IDENTITY NOT NULL, \n"
                    "   text01  TEXT NULL, \n" 
                    "   text02  TEXT NULL, \n" 
                    "   image01 IMAGE NULL, \n" 
                    "   image02 IMAGE NULL \n" 
                    ") \n";

                auto_stmt.reset(conn->LangCmd(sql));
                rc = auto_stmt->Send();
                BOOST_CHECK(rc);
                auto_stmt->DumpResults();
            }

            // Insert data ...

            // Insert empty CLOB.
            {
                sql  = " INSERT INTO " + table_name + "(text01, text02, image01, image02)";
                sql += " VALUES('', '', '', '')";

                auto_stmt.reset(conn->LangCmd(sql));
                rc = auto_stmt->Send();
                BOOST_CHECK(rc);
                auto_stmt->DumpResults();
                
                /*
                for (int i = 0; i < num_of_records; ++i) {
                    auto_stmt->ExecuteUpdate( sql );
                }
                */
            }

            // Update LOB value.
            {
                sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
                // With next line MS SQL returns incorrect blob descriptors in select
                //sql += " ORDER BY id";

                auto_ptr<CDB_CursorCmd> auto_cursor;
                auto_cursor.reset(conn->Cursor("test_lob_multiple_ll", sql));

                auto_ptr<CDB_Result> rs(auto_cursor->Open());
                BOOST_CHECK (rs.get() != NULL);

                auto_ptr<I_ITDescriptor> text01;
                auto_ptr<I_ITDescriptor> text02;
                auto_ptr<I_ITDescriptor> image01;
                auto_ptr<I_ITDescriptor> image02;

                while (rs->Fetch()) {
                    // ReadItem must not be called here
                    //rs->ReadItem(NULL, 0);
                    text01.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(text01.get());
                    rs->SkipItem();

                    text02.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(text02.get());
                    rs->SkipItem();

                    image01.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(image01.get());
                    rs->SkipItem();

                    image02.reset(rs->GetImageOrTextDescriptor());
                    BOOST_CHECK(image02.get());
                    rs->SkipItem();
                }

                // Send data ...
                {
                    CDB_Text    obj_text;
                    CDB_Image   obj_image;

                    obj_text.Append(clob_value);
                    obj_image.Append(clob_value.c_str(), clob_value.size());

                    conn->SendData(*text01, obj_text);
                    obj_text.MoveTo(0);
                    conn->SendData(*text02, obj_text);
                    conn->SendData(*image01, obj_image);
                    obj_image.MoveTo(0);
                    conn->SendData(*image02, obj_image);
                }

            }
        }

        // Retrieve data ...
        {
            sql  = " SELECT text01, text02, image01, image02 FROM " + table_name;
            sql += " ORDER BY id";

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

                CDB_Stream* obj_lob;
                CDB_Text    obj_text;
                CDB_Image   obj_image;
                char        buffer[128];

                while (rs->Fetch()) {
                    for (int pos = 0; pos < 4; ++pos) {
                        switch (rs->ItemDataType(pos)) {
                            case eDB_Image:
                                obj_lob = &obj_image;
                                break;
                            case eDB_Text:
                                obj_lob = &obj_text;
                            default:
                                break;
                        };

                        // You have to call either 
                        // obj_lob->Truncate();
                        // rs->GetItem(obj_lob);
                        // or
                        // rs->GetItem(obj_lob, I_Result::eAssignLOB);
                        rs->GetItem(obj_lob, I_Result::eAssignLOB);

                        BOOST_CHECK( !obj_lob->IsNULL() );

                        size_t blob_size = obj_lob->Size();
                        BOOST_CHECK_EQUAL(clob_value.size(), blob_size);

                        size_t size_read = obj_lob->Read(buffer, sizeof(buffer));

                        BOOST_CHECK_EQUAL(clob_value, string(buffer, size_read));
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
CDBAPIUnitTest::Test_BulkInsertBlob(void)
{
    string sql;
    enum { record_num = 100 };
    // string table_name = GetTableName();
    string table_name = "#dbapi_bcp_table2";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            // Prepare data ...
            {
                // Clean table ...
                auto_stmt->ExecuteUpdate("DELETE FROM "+ table_name);
            }

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(GetConnection().CreateBulkInsert(table_name));
                // auto_ptr<IBulkInsert> bi(GetConnection().CreateBulkInsert(table_name, 1));
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
                auto_ptr<IBulkInsert> bi(GetConnection().CreateBulkInsert(table_name));
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
                BOOST_CHECK_EQUAL(rec_num, int(record_num) * int(batch_num));
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
        CDB_Connection* conn = GetConnection().GetCDB_Connection();

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    geneId INT NOT NULL, \n";
            sql += "    dataImage IMAGE NULL, \n";
            sql += "    dataText TEXT NULL \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert not-null value ...
        {
            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

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

                sql = "SELECT dataText, dataImage FROM "+ table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(!is_null);
                        BOOST_CHECK_EQUAL(result, data);

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(is_null);
                    }
                    BOOST_CHECK_EQUAL(rec_num, 1);

                }
            }
        }


        // Insert null-value ...
        {
            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                geneIdVal = 2;

                dataTextVal.AssignNULL();

                bcp->SendRow();
                bcp->CompleteBCP();
            }

            // Retrieve data back ...
            {
                string result;
                char buff[64];
                int rec_num = 0;

                sql = "SELECT dataText FROM "+ table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                            result += string(buff, read_bytes);
                        }

                        BOOST_CHECK(is_null);
                    }
                    BOOST_CHECK_EQUAL(rec_num, 1);

                }
            }
        }

        // Insert two values ...
        {
            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

                // First row ...
                geneIdVal = 2;

                dataTextVal.AssignNULL();

                bcp->SendRow();

                // Second row ...
                geneIdVal = 1;

                dataTextVal.Append(data);
                dataTextVal.MoveTo(0);

                bcp->SendRow();

                // Complete transaction ...
                bcp->CompleteBCP();
            }

            // Retrieve data back ...
            {
                string result;
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name + " ORDER BY geneId";

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                    size_t read_bytes = 0;
                    bool is_null = true;

                    // First record ...
                    BOOST_CHECK(rs->Fetch());
                    if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(!is_null);
                    BOOST_CHECK_EQUAL(result, data);

                    // Second record ...
                    read_bytes = 0;
                    is_null = true;

                    BOOST_CHECK(rs->Fetch());
                    if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
                        result += string(buff, read_bytes);
                    }

                    BOOST_CHECK(is_null);
                }
            }
        }


        // Check NULL-values ...
        {
            int num_of_tests = 10;

            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(2, &dataTextVal);
                bcp->Bind(1, &dataImageVal);

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

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
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

        // Check NULL-values in a different way ...
        if (false) {
            int num_of_tests = 10;

            // Delete previously inserted data ...
            {
                sql = "DELETE FROM " + table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

                auto_stmt->Send();
                auto_stmt->DumpResults();
            }

            // Insert data ...
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                CDB_Int geneIdVal;
                CDB_Text dataTextVal;
                CDB_Image dataImageVal;

                bcp->Bind(0, &geneIdVal);
                bcp->Bind(1, &dataTextVal);
                bcp->Bind(2, &dataImageVal);

                for(int i = 0; i < num_of_tests; ++i ) {
                    geneIdVal = i;

                    if (i % 2 == 0) {
                        dataTextVal.Append(data);
                        dataTextVal.MoveTo(0);

                        dataImageVal.AssignNULL();
                    } else {
                        dataTextVal.AssignNULL();

                        dataImageVal.Append((void*)data.c_str(), data.size());
                        dataImageVal.MoveTo(0);
                    }

                    bcp->SendRow();
                }

                bcp->CompleteBCP();
            }

            // Check inserted data ...
            if (false) {
                char buff[64];

                sql = "SELECT dataText FROM "+ table_name;

                auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

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

                        if ((read_bytes = rs->ReadItem(buff, sizeof(buff), &is_null))) {
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
    string data;

    data =
        "9606    2       GO:0019899      IPI     -       enzyme binding  11435418        Function"
        "9606    2       GO:0019966      IDA     -       interleukin-1 binding   9714181 Function"
        "9606    2       GO:0019959      IPI     -       interleukin-8 binding   10880251        Function"
        "9606    2       GO:0008320      NR      -       protein carrier activity        -       Function"
        "9606    2       GO:0004867      IEA     -       serine-type endopeptidase inhibitor activity    -       Function"
        "9606    2       GO:0043120      IDA     -       tumor necrosis factor binding   9714181 Function"
        "9606    2       GO:0017114      IEA     -       wide-spectrum protease inhibitor activity       -       Function"
        "9606    2       GO:0006886      NR      -       intracellular protein transport -       Process"
        "9606    2       GO:0051260      NAS     -       protein homooligomerization     -       Process"
        "9606    2       GO:0005576      NAS     -       extracellular region    14718574        Component";
    // data = "testing";

    try {
        CDB_Connection* conn = GetConnection().GetCDB_Connection();
        const size_t first_ind = 9;
        // const size_t first_ind = 0;

        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";

            if(first_ind) {
                sql += "    vkey int NOT NULL , \n";
                sql += "    geneId int NOT NULL , \n";
                sql += "    modate datetime NOT NULL , \n";
                sql += "    dtype int NOT NULL , \n";
                sql += "    dsize int NOT NULL , \n";
                sql += "    dataStr varchar (255) NULL , \n";
                sql += "    dataInt int NULL , \n";
                sql += "    dataBin varbinary (255) NULL , \n";
                sql += "    cnt int NOT NULL , \n";
            }

            sql += "    dataText text NULL , \n";
            sql += "    dataImg image NULL  \n";
            sql += " )";

            auto_ptr<CDB_LangCmd> auto_stmt(conn->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert data ...
        if (true) {

            auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

            // Declare data-holders ...
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = data.size();

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;

            CDB_Image dataImgVal;


            // Bind ...
            if (first_ind) {
                bcp->Bind(0, &vkeyVal);
                bcp->Bind(1, &geneIdVal);
                bcp->Bind(2, &modateVal);
                bcp->Bind(3, &dtypeVal);
                bcp->Bind(4, &dsizeVal);
                bcp->Bind(5, &dataStrVal);
                bcp->Bind(6, &dataIntVal);
                bcp->Bind(7, &dataBinVal);
                bcp->Bind(8, &cntVal);
            }

            dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            //    dataTextVal.Append(data);
            //    dataTextVal.MoveTo(0);
            bcp->Bind(first_ind, &dataTextVal);

            dataImgVal.AssignNULL();
            bcp->Bind(first_ind + 1, &dataImgVal);

            bcp->SendRow();

            bcp->CompleteBCP();
        }

        // Delete previously inserted data ...
        {
            sql = "DELETE FROM " + table_name;

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Insert data again ...
        if (true) {

            auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

            // Declare data-holders ...
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = data.size();

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;

            CDB_Image dataImgVal;


            // Bind ...
            if (first_ind) {
                bcp->Bind(0, &vkeyVal);
                bcp->Bind(1, &geneIdVal);
                bcp->Bind(2, &modateVal);
                bcp->Bind(3, &dtypeVal);
                bcp->Bind(4, &dsizeVal);
                bcp->Bind(5, &dataStrVal);
                bcp->Bind(6, &dataIntVal);
                bcp->Bind(7, &dataBinVal);
                bcp->Bind(8, &cntVal);
            }

            // dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            dataTextVal.Append(data);
            dataTextVal.MoveTo(0);
            bcp->Bind(first_ind, &dataTextVal);

            dataImgVal.AssignNULL();
            bcp->Bind(first_ind + 1, &dataImgVal);

            /*
            CDB_Text dataTextVal;
            // dataTextVal.AssignNULL();
            // doesn't matter, null or not null data both fail
            dataTextVal.Append(data);
            dataTextVal.MoveTo(0);
            bcp->Bind(0, &dataTextVal);

            CDB_Image dataImgVal;
            dataImgVal.AssignNULL();
            bcp->Bind(1, &dataImgVal);
            */

            bcp->SendRow();

            bcp->CompleteBCP();
        }

        // Delete previously inserted data ...
        {
            sql = "DELETE FROM " + table_name;

            auto_ptr<CDB_LangCmd> auto_stmt(GetConnection().GetCDB_Connection()->LangCmd(sql));

            auto_stmt->Send();
            auto_stmt->DumpResults();
        }

        // Third scenario ...
        if (m_args.GetDriverName() != ctlib_driver) {
            CDB_Int vkeyVal;
            vkeyVal = 1;

            CDB_Int geneIdVal;
            geneIdVal = 2;

            CTime curr_time(CTime::eCurrent);
            CDB_DateTime modateVal(curr_time);

            CDB_Int dtypeVal;
            dtypeVal = 106;

            CDB_Int dsizeVal;
            dsizeVal = data.size();

            CDB_VarChar dataStrVal;
            dataStrVal.AssignNULL();

            CDB_Int dataIntVal;
            dataIntVal = 0;

            CDB_VarBinary dataBinVal;
            dataBinVal.AssignNULL();

            CDB_Int cntVal;
            cntVal = 1;

            CDB_Text dataTextVal;
            dataTextVal.AssignNULL();

            CDB_Image dataImgVal;
            dataImgVal.AssignNULL();

            bool have_img = true;

            // case 1: text data is null;
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                if ( have_img ) {
                    bcp->Bind(first_ind + 1, &dataImgVal);
                }

                bcp->SendRow();

                bcp->CompleteBCP();
            }

            // case 2: text data is not null
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                if ( have_img ) {
                    bcp->Bind(first_ind + 1, &dataImgVal);
                }


                dataTextVal.Append(data);
                dataTextVal.MoveTo(0);

                bcp->SendRow();

                bcp->CompleteBCP();
            }


            // case 3: text data is null but img is not null
            {
                auto_ptr<CDB_BCPInCmd> bcp(conn->BCPIn(table_name));

                if (first_ind) {
                    bcp->Bind(0, &vkeyVal);
                    bcp->Bind(1, &geneIdVal);
                    bcp->Bind(2, &modateVal);
                    bcp->Bind(3, &dtypeVal);
                    bcp->Bind(4, &dsizeVal);
                    bcp->Bind(5, &dataStrVal);
                    bcp->Bind(6, &dataIntVal);
                    bcp->Bind(7, &dataBinVal);
                    bcp->Bind(8, &cntVal);
                }

                bcp->Bind(first_ind, &dataTextVal);
                bcp->Bind(first_ind + 1, &dataImgVal);

                dataTextVal.AssignNULL();
                dataImgVal.Append(data.c_str(), data.size()); // pretend this data is bi
                dataImgVal.MoveTo(0);

                bcp->SendRow();

                bcp->CompleteBCP();
            }

        } else {
            m_args.PutMsgDisabled("Test_BulkInsertBlob_LowLevel2 - third scenario");
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test03", sql));

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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
CDBAPIUnitTest::Test_BCP_Cancel(void)
{
    string sql;

    try {
        auto_ptr<IConnection> conn(
                GetDS().CreateConnection(CONN_OWNERSHIP)
                );
        Connect(conn);

        auto_ptr<IStatement> auto_stmt( conn->GetStatement() );

        // Initialize ...
        // Block 1 ...
        {
            sql =
                "CREATE TABLE #test_bcp_cancel ( \n"
                "   int_field int \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        // Block 2 ...
        {
            auto_ptr<IBulkInsert> bi(
                conn->GetBulkInsert("#test_bcp_cancel")
                );

            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->StoreBatch();
            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->Cancel();
        }

        // Retrieve data ...
        // Block 3 ...
        if (true) {
            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt, "#test_bcp_cancel"), size_t(10) );
        }

        // Initialize ...
        // Block 4 ...
        if (true) {
            sql = "CREATE INDEX test_bcp_cancel_ind ON #test_bcp_cancel (int_field)";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        // Block 5 ...
        if (true) {
            auto_ptr<IBulkInsert> bi(
                conn->GetBulkInsert("#test_bcp_cancel")
                );

            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->StoreBatch();
            for (int i = 0; i < 10; ++i) {
                col1 = i;
                bi->AddRow();
            }
            bi->Cancel();
        }

        // Retrieve data ...
        // Block 6 ...
        if (true) {
            BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt, "#test_bcp_cancel"), size_t(20) );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Overflow(void)
{
    string sql;
    enum {column_size = 32, data_size = 64};

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Initialize ...
        {
            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vc32_field VARCHAR(32) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        string str_data = string(data_size, 'O');

        // Insert data ...
        {
            bool exception_catched = false;
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert("#test_bulk_overflow")
                );

            CVariant col1(eDB_VarChar, data_size);

            bi->Bind(1, &col1);

            col1 = str_data;

            // Neither AddRow() nor Complete() should throw an exception.
            try
            {
                bi->AddRow();
                bi->Complete();
            } catch(const CDB_Exception&)
            {
                exception_catched = true;
            }

            if (m_args.GetDriverName() == dblib_driver) {
                m_args.PutMsgDisabled("Unexceptional overflow of varchar in bulk-insert");

                if ( !exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was expected.");
                }
            }
            else {
                if ( exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was not expected.");
                }
            }
        }

        // Retrieve data ...
        if (m_args.GetDriverName() != dblib_driver)
        {
            sql = "SELECT * FROM #test_bulk_overflow";

            auto_stmt->SendSql( sql );
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

            BOOST_CHECK( rs.get() );
            BOOST_CHECK( rs->Next() );

            const CVariant& value = rs->GetVariant(1);

            BOOST_CHECK( !value.IsNull() );

            string str_value = value.GetString();
            BOOST_CHECK_EQUAL( string::size_type(column_size), str_value.size() );
        }

        // Initialize ...
        {
            sql = "DROP TABLE #test_bulk_overflow";

            auto_stmt->ExecuteUpdate( sql );

            sql =
                "CREATE TABLE #test_bulk_overflow ( \n"
                "   vb32_field VARBINARY(32) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data ...
        {
            bool exception_catched = false;
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert("#test_bulk_overflow")
                );

            CVariant col1(eDB_VarBinary, data_size);

            bi->Bind(1, &col1);

            col1 = CVariant::VarBinary(str_data.c_str(), str_data.size());

            // Here either AddRow() or Complete() should throw an exception.
            try
            {
                bi->AddRow();
                bi->Complete();
            } catch(const CDB_Exception&)
            {
                exception_catched = true;
            }

            if (m_args.GetDriverName() == ctlib_driver) {
                m_args.PutMsgDisabled("Exception when overflow of varbinary in bulk-insert");

                if ( exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was not expected.");
                }
            }
            else {
                if ( !exception_catched ) {
                    BOOST_FAIL("Exception CDB_ClientEx was expected.");
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing(void)
{
    string sql;
    string table_name = "#bin_bulk_insert_table";
    // string table_name = "DBAPI_Sample..bin_bulk_insert_table";

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        if (table_name[0] == '#') {
            // Table for bulk insert ...
            if ( m_args.GetServerType() == CTestArguments::eMsSql
                || m_args.GetServerType() == CTestArguments::eMsSql2005) {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id INT PRIMARY KEY, \n";
                sql += "    vc8000_field VARCHAR(8000) NULL, \n";
                sql += "    vb8000_field VARBINARY(8000) NULL, \n";
                sql += "    int_field INT NULL, \n";
                sql += "    bigint_field BIGINT NULL \n";
                sql += " )";

                // Create the table
                auto_stmt->ExecuteUpdate(sql);
            } else
            {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id INT PRIMARY KEY, \n";
                sql += "    vc8000_field VARCHAR(1900) NULL, \n";
                sql += "    vb8000_field VARBINARY(1900) NULL, \n";
                sql += "    int_field INT NULL \n";
                sql += " )";

                // Create the table
                auto_stmt->ExecuteUpdate(sql);
            }
        }


        // VARBINARY ...
        {
            enum { num_of_tests = 10 };
            const char char_val('2');

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

                CVariant col1(eDB_Int);
                CVariant col2(eDB_VarChar);
                CVariant col3(eDB_LongBinary, m_max_varchar_size);

                bi->Bind(1, &col1);
                bi->Bind(2, &col2);
                bi->Bind(3, &col3);

                for(int i = 0; i < num_of_tests; ++i ) {
                    int int_value = m_max_varchar_size / num_of_tests * i;
                    string str_val(int_value , char_val);

                    col1 = int_value;
                    col3 = CVariant::LongBinary(m_max_varchar_size,
                                                str_val.c_str(),
                                                str_val.size()
                                                );
                    bi->AddRow();
                }
                bi->Complete();
            }

            // Retrieve data ...
            // Some drivers limit size of text/binary to 255 bytes ...
            if ( m_args.GetDriverName() != dblib_driver
                ) {
                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

                sql  = " SELECT id, vb8000_field FROM " + table_name;
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
                // m_args.PutMsgDisabled("Test_Bulk_Writing Retrieve data");
            }
        }

        // INT, BIGINT
        {

            // Clean table ...
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // INT collumn ...
            {
                enum { num_of_tests = 8 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        GetConnection().GetBulkInsert(table_name)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_Int);

                    bi->Bind(1, &col1);
                    bi->Bind(4, &col2);

                    for(int i = 0; i < num_of_tests; ++i ) {
                        col1 = i;
                        Int4 value = Int4( 1 ) << (i * 4);
                        col2 = value;
                        bi->AddRow();
                    }
                    bi->Complete();
                }

                // Retrieve data ...
                {
                    sql  = " SELECT int_field FROM " + table_name;
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
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // BIGINT collumn ...
            // Sybase doesn't have BIGINT data type ...
            if (m_args.GetServerType() != CTestArguments::eSybase)
            {
                enum { num_of_tests = 16 };

                // Insert data ...
                {
                    auto_ptr<IBulkInsert> bi(
                        GetConnection().GetBulkInsert(table_name)
                        );

                    CVariant col1(eDB_Int);
                    CVariant col2(eDB_BigInt);
                    //CVariant col_tmp2(eDB_VarChar);
                    //CVariant col_tmp3(eDB_Int);

                    bi->Bind(1, &col1);
                    //bi->Bind(2, &col_tmp2);
                    //bi->Bind(3, &col_tmp3);
                    bi->Bind(5, &col2);

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
                    sql  = " SELECT bigint_field FROM " + table_name;
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
            else {
                m_args.PutMsgDisabled("Bigint in Sybase");
            }
        }

        // Yet another BIGINT test (and more) ...
        // Sybase doesn't have BIGINT data type ...
        if (m_args.GetServerType() != CTestArguments::eSybase)
        {
            auto_ptr<IStatement> stmt( GetConnection().CreateStatement() );

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
                    GetConnection().CreateBulkInsert("#__blki_test")
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
    //             auto_ptr<IBulkInsert> blki( GetConnection().CreateBulkInsert("#__blki_test", 2) );
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
        else {
            m_args.PutMsgDisabled("Bigint in Sybase");
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
            auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
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
            if (m_args.GetDriverName() != dblib_driver
                ) {
                sql  = " SELECT id, vc8000_field FROM " + table_name;
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
                m_args.PutMsgDisabled("Test_Bulk_Writing Retrieve data");
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
    const string table_name("#SbSubs");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);
            CVariant col4(eDB_Int);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            bi->Bind(4, &col4);

            col1 = 15001;
            col2 = 1;
            col4 = 0;

            bi->AddRow();
            bi->Complete();
        }

        // Retrieve data ...
        {
            sql  = " SELECT live, date, rlsdate, depdate FROM " + table_name;

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->GetVariant(1).IsNull() );
            BOOST_CHECK( rs->GetVariant(1).GetBit() );
            BOOST_CHECK( !rs->GetVariant(2).IsNull() );
            BOOST_CHECK( !rs->GetVariant(2).GetCTime().IsEmpty() );
            BOOST_CHECK( rs->GetVariant(3).IsNull() );
            BOOST_CHECK( !rs->GetVariant(4).IsNull() );
            BOOST_CHECK( !rs->GetVariant(4).GetCTime().IsEmpty() );

            // Dump results ...
            DumpResults( auto_stmt.get() );
        }

        // Second test
        {
            sql = "DELETE FROM " + table_name;
            auto_stmt->ExecuteUpdate(sql);

            auto_ptr<IBulkInsert> bi(
                    GetConnection().GetBulkInsert(table_name)
                    );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);
            CVariant col3(eDB_Int);
            CVariant col4(eDB_Int);
            CVariant col5(eDB_DateTime);
            CVariant col6(eDB_DateTime);
            CVariant col7(eDB_DateTime);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);
            bi->Bind(3, &col3);
            bi->Bind(4, &col4);
            bi->Bind(5, &col5);
            bi->Bind(6, &col6);
            bi->Bind(7, &col7);

            col1 = 15001;
            col2 = 1;
            col3 = 2;
            if (m_args.IsODBCBased()
                || m_args.GetDriverName() == dblib_driver
                )
            {
                m_args.PutMsgDisabled("Bulk-insert NULLs when there are defaults");

                bi->AddRow();
                bi->Complete();

                // Retrieve data ...
                {
                    sql  = " SELECT live, date, rlsdate, depdate FROM " + table_name;

                    auto_stmt->SendSql( sql );

                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );

                    BOOST_CHECK( rs->Next() );
                    BOOST_CHECK( !rs->GetVariant(1).IsNull() );
                    BOOST_CHECK( rs->GetVariant(1).GetBit() );
                    BOOST_CHECK( !rs->GetVariant(2).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(2).GetCTime().IsEmpty() );
                    BOOST_CHECK( rs->GetVariant(3).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(4).IsNull() );
                    BOOST_CHECK( !rs->GetVariant(4).GetCTime().IsEmpty() );

                    // Dump results ...
                    DumpResults( auto_stmt.get() );
                }
            }
            else if (m_args.GetDriverName() != ctlib_driver) {
                try {
                    bi->AddRow();
                    bi->Complete();

                    BOOST_FAIL("Exception was not thrown");
                }
                catch (CDB_ClientEx&) {
                    // exception must be thrown
                }
            }
            else {
                // driver crushes after trying to insert
                m_args.PutMsgDisabled("Bulk-insert NULL into NOT-NULL column");
            }
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing3(void)
{
    string sql;
    string table_name("#blk_table3");
    string table_name2 = "#dbapi_bcp_table2";
    const int test_num = 10;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Side-effect ...
        // Tmp table ...
        auto_ptr<IBulkInsert> bi_tmp(
            GetConnection().GetBulkInsert(table_name2)
            );

        CVariant col_tmp(eDB_Int);
        bi_tmp->Bind(1, &col_tmp);

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    uid int NOT NULL , \n"
                "    info_id int NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                GetConnection().GetBulkInsert(table_name)
                );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_Int);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            for (int j = 0; j < test_num; ++j) {
                for (int i = 0; i < test_num; ++i) {
                    col1 = i;
                    col2 = i * 2;

                    bi->AddRow();
                }
                bi->StoreBatch();
            }

            bi->Complete();
        }


    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing4(void)
{
    string sql;
    string table_name("#blk_table4");
    const int test_num = 10;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
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

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            //
            auto_ptr<IBulkInsert> bi(
                GetConnection().CreateBulkInsert(table_name)
                );

            CVariant b_idnwparams(eDB_Int);
            CVariant b_gi1(eDB_Int);
            CVariant b_gi2(eDB_Int);
            CVariant b_idty(eDB_Float);
            CVariant b_idty2(eDB_Float);
            CVariant b_idty4(eDB_Float);
            CVariant b_trans(eDB_Text);

            Uint2 pos = 0;
            bi->Bind(++pos, &b_idnwparams);
            bi->Bind(++pos, &b_gi1);
            bi->Bind(++pos, &b_gi2);
            bi->Bind(++pos, &b_idty);
            bi->Bind(++pos, &b_trans);
            bi->Bind(++pos, &b_idty2);
            bi->Bind(++pos, &b_idty4);

            b_idnwparams = 123456;

            for (int j = 0; j < test_num; ++j) {

                b_gi1 = j + 1;
                b_gi2 = j + 2;
                b_idty = float(j + 1.1);
                b_idty2 = float(j + 2.2);
                b_idty4 = float(j + 3.3);

                b_trans.Truncate();
                b_trans.Append(
                        test_data.c_str(),
                        test_data.size()
                        );

                bi->AddRow();
            }

            bi->Complete();
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing5(void)
{
    string sql;
    const string table_name("#blk_table5");
    // const string table_name("DBAPI_Sample..blk_table5");

    try {
        CDB_Connection* conn(GetConnection().GetCDB_Connection());


        // Create table ...
        if (true) {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    file_name varchar(512) NULL, \n"
                "    line_num int NULL, \n"
                "    pid int NULL, \n"
                "    tid int NULL, \n"
                "    iteration int NULL, \n"
                "    proc_sn int NULL, \n"
                "    thread_sn int NULL, \n"
                "    host varchar(128) NULL, \n"
                "    session_id varchar(128) NULL, \n"
                "    app_name varchar(128) NULL, \n"
                "    req_type varchar(128) NULL, \n"
                "    client_ip int NULL, \n"
                "    date datetime NULL, \n"
                "    exec_time_sec float NULL, \n"
                "    params_unparsed varchar(8000) NULL, \n"
                "    extra varchar(8000) NULL, \n"
                "    error varchar(8000) NULL, \n"
                "    warning varchar(8000) NULL \n"
                ") \n"
                ;

            auto_ptr<CDB_LangCmd> cmd(conn->LangCmd(sql));
            cmd->Send();
            cmd->DumpResults();
        }

        // Insert data ...
        {
            auto_ptr<CDB_BCPInCmd> vBcp(conn->BCPIn(table_name));

            CDB_VarChar     s_file_name;
            CDB_Int         n_line_num;
            CDB_Int         n_pid;
            CDB_Int         n_tid;
            CDB_Int         n_iteration;
            CDB_Int         n_proc_sn;
            CDB_Int         n_thread_sn;
            CDB_VarChar     s_host;
            CDB_VarChar     s_session_id;
            CDB_VarChar     s_app_name;
            CDB_VarChar     s_req_type;
            CDB_Int         n_client_ip;
            CDB_DateTime    dt_date;
            CDB_Float       f_exec_time_secs;
            CDB_VarChar     s_params_unparsed;
            CDB_VarChar     s_extra;
            CDB_VarChar     s_error;
            CDB_VarChar     s_warning;

            Uint2 pos = 0;
            vBcp->Bind(pos++, &s_file_name);
            vBcp->Bind(pos++, &n_line_num);
            vBcp->Bind(pos++, &n_pid);
            vBcp->Bind(pos++, &n_tid);
            vBcp->Bind(pos++, &n_iteration);
            vBcp->Bind(pos++, &n_proc_sn);
            vBcp->Bind(pos++, &n_thread_sn);
            vBcp->Bind(pos++, &s_host);
            vBcp->Bind(pos++, &s_session_id);
            vBcp->Bind(pos++, &s_app_name);
            vBcp->Bind(pos++, &s_req_type);
            vBcp->Bind(pos++, &n_client_ip);
            vBcp->Bind(pos++, &dt_date);
            vBcp->Bind(pos++, &f_exec_time_secs);
            vBcp->Bind(pos++, &s_params_unparsed);
            vBcp->Bind(pos++, &s_extra);
            vBcp->Bind(pos++, &s_error);
            vBcp->Bind(pos++, &s_warning);

            int i = 2;
            while (i-- > 0) {
                if (i % 2 == 1)
                    s_file_name = "";
                else
                    s_file_name = "some name";
                n_line_num = 12;
                n_pid = 12345;
                n_tid = 67890;
                n_iteration = 23;
                n_proc_sn = 34;
                n_thread_sn = 45;
                s_host = "some host";
                s_session_id = "some id";
                s_app_name = "some name";
                s_req_type = "some type";
                n_client_ip = 12345;
                dt_date = CTime();
                f_exec_time_secs = 0;
                if (i % 2 == 1)
                    s_params_unparsed = "";
                else
                    s_params_unparsed = "some params";
                s_extra = "";
                s_error = "";
                s_warning = "";

                vBcp->SendRow();
            }

            vBcp->CompleteBCP();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

void
CDBAPIUnitTest::Test_Bulk_Writing6(void)
{
    string sql;
    string table_name("#blk_table6");
    const string& str_value("Oops ...");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
        const string test_data("Test, test, tEST.");

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "[ctg_id] INT NOT NULL, \n"
                "[ctg_name] VARCHAR(64) NOT NULL, \n"
                "[ctg_gi] INT NOT NULL, \n"
                "[ctg_acc] VARCHAR(20) NULL, \n"
                "[ctg_ver] TINYINT NOT NULL, \n"
                "[chrm] VARCHAR(64) NOT NULL, \n"
                "[start] INT NOT NULL, \n"
                "[stop] INT NOT NULL, \n"
                "[orient] CHAR(1) NOT NULL, \n"
                "[group_term] VARCHAR(64) NOT NULL, \n"
                "[group_label] VARCHAR(64) NOT NULL, \n"
                "[ctg_label] VARCHAR(64) NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            //
            auto_ptr<IBulkInsert> stmt(
                GetConnection().GetBulkInsert(table_name)
                );

            // Declare variables ...
            CVariant ctg_id(eDB_Int);
            CVariant ctg_name(eDB_VarChar);
            CVariant ctg_gi(eDB_Int);
            CVariant ctg_acc(eDB_VarChar);
            CVariant ctg_ver(eDB_TinyInt);
            CVariant chrm(eDB_VarChar);
            CVariant ctg_start(eDB_Int);
            CVariant ctg_stop(eDB_Int);
            CVariant ctg_orient(eDB_VarChar);
            CVariant group_term(eDB_VarChar);
            CVariant group_label(eDB_VarChar);
            CVariant ctg_label(eDB_VarChar);

            // Bind ...
            int idx(0);
            stmt->Bind(++idx, &ctg_id);
            stmt->Bind(++idx, &ctg_name);
            stmt->Bind(++idx, &ctg_gi);
            stmt->Bind(++idx, &ctg_acc);
            stmt->Bind(++idx, &ctg_ver);
            stmt->Bind(++idx, &chrm);
            stmt->Bind(++idx, &ctg_start);
            stmt->Bind(++idx, &ctg_stop);
            stmt->Bind(++idx, &ctg_orient);
            stmt->Bind(++idx, &group_term);
            stmt->Bind(++idx, &group_label);
            stmt->Bind(++idx, &ctg_label);

            // Assign values ...
            ctg_id = 1;
            ctg_name = str_value;
            ctg_gi = 2;
            ctg_acc = str_value;
            ctg_ver = Uint1(3);
            chrm = str_value;
            ctg_start = 4;
            ctg_stop = 5;
            ctg_orient = "V";
            group_term = str_value;
            group_label = str_value;
            ctg_label = str_value;

            stmt->AddRow();
            stmt->Complete();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


void
CDBAPIUnitTest::Test_Bulk_Late_Bind(void)
{
    string sql;
    const string table_name("#blk_late_bind_table");
    bool exception_thrown = false;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "    id int NULL, \n"
                "    name varchar(200) NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Check that data cannot be binded after rows sending
        {
            auto_ptr<IBulkInsert> bi(
                GetConnection().CreateBulkInsert(table_name)
                );

            CVariant  id(eDB_Int);
            CVariant  name(eDB_VarChar);

            bi->Bind(1, &id);

            id = 5;
            bi->AddRow();

            try {
                bi->Bind(2, &name);
                BOOST_FAIL("Exception after late binding wasn't thrown");
            }
            catch (CDB_Exception&) {
                exception_thrown = true;
                // ok
            }

            BOOST_CHECK(exception_thrown);
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
                    CVariant::LongChar(str_val.c_str(), str_val.size()),
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
void CDBAPIUnitTest::Test_Numeric(void)
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

                if (m_args.IsODBCBased()) {
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

                    if (m_args.IsODBCBased()) {
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

                    if (!m_args.IsODBCBased()) {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), string("98.79"));
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), string("98.79"));
                    } else {
                        m_args.PutMsgDisabled("Test_Numeric - part 2");
                    }
                }

                //
                {
                    BOOST_CHECK( rs->Next() );

                    const CVariant& value1 = rs->GetVariant(1);
                    const CVariant& value2 = rs->GetVariant(2);

                    BOOST_CHECK( !value1.IsNull() );
                    BOOST_CHECK( !value2.IsNull() );

                    if (!m_args.IsODBCBased()) {
                        BOOST_CHECK_EQUAL(value1.GetNumeric(), string("1.21"));
                        BOOST_CHECK_EQUAL(value2.GetNumeric(), string("1.21"));
                    } else {
                        m_args.PutMsgDisabled("Test_Numeric - part 3");
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
void CDBAPIUnitTest::Test_CDB_Exception(void)
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


///////////////////////////////////////////////////////////////////////////////
size_t
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
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
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt)
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
Int8
CDBAPIUnitTest::GetIdentity(const auto_ptr<IStatement>& auto_stmt)
{
    Int8 identity = 0;

    DumpResults(auto_stmt.get());
    // ClearParamList is necessary here ...
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
    const size_t rec_num = 2;
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

            for (size_t i = 0; i < rec_num; ++i) {
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
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Open a cursor for the second time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
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
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }
        }

        // Second test ...
        if (m_args.GetServerType() != CTestArguments::eSybase)
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
                //"  WHERE obj.rm_file_id=0x7FFFFFF AND obj.submit_id = 1 \n"
                "  WHERE obj.rm_file_id=0x7FFFFFF \n"
                "  ORDER BY obj.submit_id \n"
                "    FOR UPDATE OF obj.chr, obj.is_scaffold, obj.is_unlinked"
                ;

            // Insert something ...
            {
                sql  = "INSERT INTO #AgpFiles(submit_id, file_num, path, name, notes, file_size) ";
                sql += "VALUES(1, 0, '', '', '', 0)";

                auto_stmt->ExecuteUpdate(sql);

                Int8 agp_files_id = GetIdentity(auto_stmt);

                // Make 2 different inserts
                sql  = "INSERT INTO #Objects(submit_id, file_id, name, blob_ofs) ";
                sql += "VALUES(1, " + NStr::Int8ToString(agp_files_id) + ", '', 0)";

                auto_stmt->ExecuteUpdate(sql);

                sql  = "INSERT INTO #Objects(submit_id, file_id, name, blob_ofs) ";
                sql += "VALUES(2, " + NStr::Int8ToString(agp_files_id) + ", '', 0)";

                auto_stmt->ExecuteUpdate(sql);
            }

            // Just read data ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    ;
                }
            }

            // Update something ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                // Make 2 different updates
                sql = "UPDATE #Objects SET chr=123, is_scaffold=234, is_unlinked=345";

                BOOST_CHECK(rs->Next());
                auto_cursor->Update("#Objects", sql);

                sql = "UPDATE #Objects SET chr=456, is_scaffold=234, is_unlinked=345";

                rs->Next();
                auto_cursor->Update("#Objects", sql);
            }

            // Check that update was successful
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                int check_val = 123;
                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(3).GetInt4() == check_val);
                    check_val = 456;
                }
            }

            // Delete something
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", cursor_sql));
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                auto_cursor->Delete("#Objects");

                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
                BOOST_CHECK_EQUAL(size_t(1), GetNumOfRecords(auto_stmt, "#Objects"));
            }
        } else {
            m_args.PutMsgDisabled("Test_Cursor Second test");
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
    const size_t rec_num = 2;
    string sql;

    try {
        // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
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
            auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test01", sql));

            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());

                if (blobRs->Next()) {
                    ostream& out = auto_cursor->GetBlobOStream(1, strlen(clob), eDisableLog);
                    out.write(clob, strlen(clob));
                    out.flush();
                } else {
                    BOOST_FAIL( msg_record_expected );
                }
            }

            // Check record number ...
            {
                auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
                BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
            }

            // Another cursor ...
            sql  = " select text_field from " + GetTableName();
            sql += " where int_field = 1 for update of text_field";

            auto_cursor.reset(GetConnection().GetCursor("test02", sql));
            {
                // blobRs should be destroyed before auto_cursor ...
                auto_ptr<IResultSet> blobRs(auto_cursor->Open());
                if ( !blobRs->Next() ) {
                    BOOST_FAIL( msg_record_expected );
                }

                ostream& out = auto_cursor->GetBlobOStream(1, strlen(clob), eDisableLog);
                out.write(clob, strlen(clob));
                out.flush();
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


void
CDBAPIUnitTest::Test_Cursor_Param(void)
{
    string sql;
    const size_t rec_num = 2;

    try {
         // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        {
            sql = "select int_field from " + GetTableName() + " WHERE int_field = @int_value";

            // Open a cursor for the first time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test10", sql));

                auto_cursor->SetParam(CVariant(Int4(0)), "@int_value" );

                // First Open ...
                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 0);

                ///////////////
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // No ClearParamList in ICursor at the moment ...
                // auto_cursor->ClearParamList();

                auto_cursor->SetParam(CVariant(Int4(1)), "@int_value" );

                //  Second Open ...
                rs.reset(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 1);
            }

            // Open a cursor for the second time ...
            {
                auto_ptr<ICursor> auto_cursor(GetConnection().GetCursor("test10", sql));

                auto_cursor->SetParam(CVariant(Int4(1)), "@int_value" );

                auto_ptr<IResultSet> rs(auto_cursor->Open());
                BOOST_CHECK(rs.get() != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK_EQUAL(rs->GetVariant(1).GetInt4(), 1);
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Cursor_Multiple(void)
{
    string sql;
    const size_t rec_num = 2;

    try {
         // Initialize a test table ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            // Insert new LOB records ...
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, text_field) VALUES(@id, '') \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }
            // Check record number ...
            BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
        }

        // Open ...
        {
            sql = "select int_field from " + GetTableName() + " WHERE int_field = @int_value";

            auto_ptr<ICursor> auto_cursor1(GetConnection().GetCursor("test10", sql));
            auto_ptr<ICursor> auto_cursor2(GetConnection().GetCursor("test11", sql));

            auto_cursor1->SetParam(CVariant(Int4(0)), "@int_value" );
            auto_cursor2->SetParam(CVariant(Int4(0)), "@int_value" );

            auto_ptr<IResultSet> rs1(auto_cursor1->Open());
            BOOST_CHECK(rs1.get() != NULL);
            auto_ptr<IResultSet> rs2(auto_cursor2->Open());
            BOOST_CHECK(rs2.get() != NULL);

            BOOST_CHECK(rs1->Next());
            BOOST_CHECK_EQUAL(rs1->GetVariant(1).GetInt4(), 0);
            BOOST_CHECK(rs2->Next());
            BOOST_CHECK_EQUAL(rs2->GetVariant(1).GetInt4(), 0);
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
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
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
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            IResultSet* rs;

            // 1) Select recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57 + 33" );
            BOOST_CHECK( rs != NULL );

            // 2) Retrive only one record.
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );

            // 3) Select another recordset with just one record
            auto_ptr<IStatement> auto_stmt2( GetConnection().CreateStatement() );
            rs = auto_stmt2->ExecuteQuery( "select qq = 57.55 + 0.0033" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );
        }

        // Check column name ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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

            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
            m_args.PutMsgDisabled("Check sequent call of ExecuteQuery");
        }

        // Select NULL values and empty strings ... 
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            auto_ptr<IResultSet> rs;

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT '', NULL, NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, '', NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, '', NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, NULL, ''" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT '', '', NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, '', '', NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, '', ''" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );
        }


    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_SelectStmt2(void)
{
    string sql;
    string table_name("#blk_table7");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Running parametrized statement ...
        {
            int result_num = 0;

            auto_stmt->SetParam( CVariant( "master" ), "@dbname" );

            auto_stmt->SendSql("exec sp_helpdb @dbname");

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    ++result_num;
                }
            }

            auto_stmt->ClearParamList();
        }

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "id INT NOT NULL PRIMARY KEY, \n"
                "name VARCHAR(255) NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            sql  = "insert into ";
            sql += table_name + "( id, name ) values ( 1, 'one' )";

            auto_stmt->ExecuteUpdate(sql);
        }

        // Query data ...
        {
            sql  = "select id, name from ";
            sql += table_name;

            auto_stmt->SendSql(sql);
            while( auto_stmt->HasMoreResults() ) { 
                if( auto_stmt->HasRows() ) { 
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet()); 
                    while( rs->Next() ) { 
                        ;
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
CDBAPIUnitTest::Test_SelectStmtXML(void)
{
    try {
        // SQL + XML
        {
            string sql;
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
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
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());
        IResultSet* rs;

        // First test ...
        {
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
            if (m_args.GetDriverName() != dblib_driver  ||  m_args.GetServerType() == CTestArguments::eSybase)
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    BOOST_CHECK(!rs->Next());

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    BOOST_CHECK(!rs->Next());

                    DumpResults(auto_stmt.get());
                }
            }

            // decimal
            if (m_args.GetDriverName() != dblib_driver  ||  m_args.GetServerType() == CTestArguments::eSybase)
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

        // Second test ...
        {
            // bit
            {
                rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Bit* data = dynamic_cast<CDB_Bit*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // tinyint
            {
                rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_TinyInt* data = dynamic_cast<CDB_TinyInt*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // smallint
            {
                rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_SmallInt* data = dynamic_cast<CDB_SmallInt*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // int
            {
                rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Int* data = dynamic_cast<CDB_Int*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // numeric
            if (m_args.GetDriverName() != dblib_driver  ||  m_args.GetServerType() == CTestArguments::eSybase)
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    switch(variant.GetData()->GetType()) {
                        case eDB_Numeric:
                            {
                                CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), string("1"));
                            }
                            break;
                        case eDB_Double:
                            {
                                m_args.PutMsgExpected("CDB_Numeric", "CDB_Double");

                                CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), 1);
                            }
                            break;
                        default:
                            BOOST_FAIL("Invalid data type.");
                    }

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    switch(variant.GetData()->GetType()) {
                        case eDB_Numeric:
                            {
                                CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());

                                if (m_args.IsODBCBased()) {
                                    BOOST_CHECK_EQUAL(data->Value(), string("2843113322"));
                                } else {
                                    BOOST_CHECK_EQUAL(data->Value(), string("2843113322.00"));
                                }
                            }
                            break;
                        case eDB_Double:
                            {
                                m_args.PutMsgExpected("CDB_Numeric", "CDB_Double");

                                CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), 2843113322U);
                            }
                            break;
                        default:
                            BOOST_FAIL("Invalid data type.");
                    }

                    DumpResults(auto_stmt.get());
                }
            }

            // decimal
            if (m_args.GetDriverName() != dblib_driver  ||  m_args.GetServerType() == CTestArguments::eSybase)
            {
                rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                switch(variant.GetData()->GetType()) {
                    case eDB_Numeric:
                        {
                            CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());
                            BOOST_CHECK_EQUAL(data->Value(), string("1"));
                        }
                        break;
                    case eDB_Double:
                        {
                            m_args.PutMsgExpected("CDB_Numeric", "CDB_Double");

                            CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                            BOOST_CHECK_EQUAL(data->Value(), 1);
                        }
                        break;
                    default:
                        BOOST_FAIL("Invalid data type.");
                }

                // CDB_Numeric* data = dynamic_cast<CDB_Numeric*>(variant.GetData());
                // BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), string("1"));

                DumpResults(auto_stmt.get());
            }

            // float
            {
                rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Float* float_data = NULL;
                float_data = dynamic_cast<CDB_Float*>(variant.GetData());

                if (float_data) {
                    // m_args.PutMsgExpected("CDB_Double", "CDB_Float");
                    BOOST_CHECK_EQUAL(float_data->Value(), 1);
                }

                //
                CDB_Double* double_data = NULL;
                double_data = dynamic_cast<CDB_Double*>(variant.GetData());

                if (double_data) {
                    m_args.PutMsgExpected("CDB_Float", "CDB_Double");
                    BOOST_CHECK_EQUAL(double_data->Value(), 1);
                }

                DumpResults(auto_stmt.get());
            }

            // double
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    CDB_Double* data = dynamic_cast<CDB_Double*>(variant.GetData());
                    BOOST_CHECK(data);

                    BOOST_CHECK_EQUAL(data->Value(), 1);

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(double precision, 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    CDB_Double* data = dynamic_cast<CDB_Double*>(variant.GetData());
                    BOOST_CHECK(data);

                    BOOST_CHECK_EQUAL(data->Value(), 2843113322U);

                    DumpResults(auto_stmt.get());
                }
            }

            // real
            {
                rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Float* data = dynamic_cast<CDB_Float*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

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
                const CVariant& variant = rs->GetVariant(1);

                CDB_SmallDateTime* data = dynamic_cast<CDB_SmallDateTime*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // datetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_DateTime* dt_data = NULL;
                dt_data = dynamic_cast<CDB_DateTime*>(variant.GetData());

                if (!dt_data){
                    m_args.PutMsgExpected("CDB_DateTime", "CDB_SmallDateTime");

                    CDB_SmallDateTime* data = dynamic_cast<CDB_SmallDateTime*>(variant.GetData());
                    BOOST_CHECK(data);
                }

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->Value(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if(varchar_data) {
                    m_args.PutMsgExpected("CDB_Char", "CDB_VarChar");

                    BOOST_CHECK_EQUAL(string(varchar_data->Value()), string("12345     "));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_Char", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }


                DumpResults(auto_stmt.get());
            }

            // varchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(varchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    m_args.PutMsgExpected("CDB_VarChar", "CDB_Char");

                    BOOST_CHECK_EQUAL(char_data->Value(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if (varchar_data) {
                    BOOST_CHECK_EQUAL(varchar_data->Value(), string("12345"));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_VarChar", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // nchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->Value(),
                                      string("12345     "));
                }

                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if(varchar_data) {
                    m_args.PutMsgExpected("CDB_Char", "CDB_VarChar");

                    BOOST_CHECK_EQUAL(string(varchar_data->Value()), string("12345     "));
                }

                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_Char", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // nvarchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nvarchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    m_args.PutMsgExpected("CDB_VarChar", "CDB_Char");

                    BOOST_CHECK_EQUAL(char_data->Value(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if (varchar_data) {
                    BOOST_CHECK_EQUAL(varchar_data->Value(), string("12345"));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_VarChar", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->Size(), size_t(10));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                    //             char_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            char_data->Value(),
                            "12345\0\0\0\0\0",
                            char_data->Size()
                            ),
                        0
                        );
                }

                //
                CDB_VarBinary* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarBinary*>(variant.GetData());

                if(varchar_data) {
                    m_args.PutMsgExpected("CDB_Binary", "CDB_VarBinary");

                    BOOST_CHECK_EQUAL(varchar_data->Size(), size_t(10));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(varchar_data->Value()),
                    //             varchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            varchar_data->Value(),
                            "12345\0\0\0\0\0",
                            varchar_data->Size()
                            ),
                        0
                        );
                }

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_Binary", "CDB_LongBinary");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // long binary
            // dblib cannot transfer more than 255 bytes ...
            if (m_args.GetDriverName() != dblib_driver
                ) {
                rs = auto_stmt->ExecuteQuery("select convert(binary(1000), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    m_args.PutMsgExpected("CDB_LongBinary", "CDB_Binary");

                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                                char_data->Size()),
                            string("12345")
                            );
                }

                //
                CDB_VarBinary* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarBinary*>(variant.GetData());

                if(varchar_data) {
                    m_args.PutMsgExpected("CDB_LongBinary", "CDB_VarBinary");

                    BOOST_CHECK_EQUAL(varchar_data->Size(), size_t(1000));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(varchar_data->Value()),
                    //             varchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            varchar_data->Value(),
                            "12345\0\0\0\0\0",
                            10
                            ),
                        0
                        );
                }

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(1000));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                    //             longchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            longchar_data->Value(),
                            "12345\0\0\0\0\0",
                            10
                            ),
                        0
                        );
                }

                DumpResults(auto_stmt.get());
            } else {
                m_args.PutMsgDisabled("Test_Recordset: Second test - long binary");
            }

            // varbinary
            {
                rs = auto_stmt->ExecuteQuery("select convert(varbinary(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    m_args.PutMsgExpected("CDB_VarBinary", "CDB_Binary");

                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                                char_data->Size()),
                            string("12345")
                            );
                }

                //
                CDB_VarBinary* data = dynamic_cast<CDB_VarBinary*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Size(), size_t(5));
                BOOST_CHECK_EQUAL(string(static_cast<const char*>(data->Value()),
                            data->Size()),
                        string("12345")
                        );

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    m_args.PutMsgExpected("CDB_VarBinary", "CDB_LongBinary");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // text
            if (m_args.GetDriverName() != dblib_driver) {
                rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Text* data = dynamic_cast<CDB_Text*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            } else {
                m_args.PutMsgDisabled("Test_Recordset: Second test - text");
            }

            // image
            if (m_args.GetDriverName() != dblib_driver) {
                rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Image* data = dynamic_cast<CDB_Image*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            } else {
                m_args.PutMsgDisabled("Test_Recordset: Second test - image");
            }
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_ResultsetMetaData(void)
{
    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());
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
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);
                    md = rs->GetMetaData();
                    BOOST_CHECK(md);

                    EDB_Type curr_type = md->GetType(1);
                    BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);
                    md = rs->GetMetaData();
                    BOOST_CHECK(md);

                    EDB_Type curr_type = md->GetType(1);
                    BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                    DumpResults(auto_stmt.get());
                }
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

            // long char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(8000), '12345')");
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

            // long binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(8000), '12345')");
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
CDBAPIUnitTest::Test_StmtMetaData(void)
{
    try {
        auto_ptr<ICallableStatement> auto_stmt;
        unsigned int col_num = 0;

        if (false) {
            auto_ptr<IStatement> auto_stmt01(GetConnection().GetStatement());
            auto_stmt01->ExecuteUpdate("USE DBAPI_Sample");
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement(".sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("..sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_sproc_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            switch (m_args.GetServerType()) {
                case CTestArguments::eSybase:
                    BOOST_CHECK_EQUAL(col_num, 5U);
                    break;
                case CTestArguments::eMsSql2005:
                    BOOST_CHECK_EQUAL(col_num, 7U);
                    break;
                case CTestArguments::eMsSql:
                    BOOST_CHECK_EQUAL(col_num, 6U);
                    break;
                default:
                    break;
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            BOOST_CHECK_EQUAL(col_num, 2U);
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_tables")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eMsSql2005) {
                BOOST_CHECK_EQUAL(col_num, 6U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 5U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_stored_procedures")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eMsSql2005) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            if (m_args.GetServerType() == CTestArguments::eSybase) {
		auto_stmt.reset(
			GetConnection().GetCallableStatement("sybsystemprocs..sp_stored_procedures")
			);
            } else {
		auto_stmt.reset(
			GetConnection().GetCallableStatement("master..sp_stored_procedures")
			);
            }

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eMsSql2005) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            if (m_args.GetServerType() == CTestArguments::eSybase) {
		auto_stmt.reset(
			GetConnection().GetCallableStatement("sybsystemprocs.dbo.sp_stored_procedures")
			);
            } else {
		auto_stmt.reset(
			GetConnection().GetCallableStatement("master.dbo.sp_stored_procedures")
			);
            }

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (m_args.GetServerType() == CTestArguments::eMsSql2005) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
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
        I_DriverContext* drv_context = GetDS().GetDriverContext();

        // Check PushCntxMsgHandler ...
        // PushCntxMsgHandler - Add message handler "h" to process 'context-wide' (not bound
        // to any particular connection) error messages.
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            auto_ptr<IConnection> local_conn( GetDS().CreateConnection() );
            Connect(local_conn);

            drv_context->PushCntxMsgHandler( drv_err_handler.get() );

            // Connection process should be affected ...
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );

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
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
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
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
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


            auto_ptr<IConnection> local_conn( GetDS().CreateConnection() );
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
            auto_ptr<IConnection> new_conn( GetDS().CreateConnection() );
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
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
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
                GetDS().SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = GetDS().CreateConnection();
                } catch(...)
                {
                    delete conn;
                }

                GetDS().SetLogStream(&cerr);
            }

            {
                IConnection* conn = NULL;

                // Enable multiexception ...
                GetDS().SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = GetDS().CreateConnection();

                    GetDS().SetLogStream(&cerr);
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
    I_DriverContext* drv_context = GetDS().GetDriverContext();

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
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
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
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());

                Connect(conn1);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> conn2(GetDS().CreateConnection());

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
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
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
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());

                Connect(conn1);

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> conn2(GetDS().CreateConnection());

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
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
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
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());

                Connect(new_conn);

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());

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
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
                GetConnection().GetCallableStatement("sp_databases")
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
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
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
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
        }

        // Temporary test ...
        // !!! This is a bug ...
        if (false) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
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
            // It is not a bug. It is a difference in setiings for MS SQL
            // 2005.
            if (m_args.GetServerType() != CTestArguments::eMsSql2005) {
                int num = 0;
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_databases")
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
                    GetConnection().GetCallableStatement("sp_server_info")
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
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

                // Set parameter to NULL ...
                auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
                auto_stmt->Execute();

                if (m_args.GetServerType() == CTestArguments::eSybase) {
                    BOOST_CHECK_EQUAL( size_t(30), GetNumOfRecords(auto_stmt) );
                } else {
                    BOOST_CHECK_EQUAL( size_t(29), GetNumOfRecords(auto_stmt) );
                }

                // Set parameter to 1 ...
                auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
                auto_stmt->Execute();

                BOOST_CHECK_EQUAL( size_t(1), GetNumOfRecords(auto_stmt) );
            }

            // NULL value with CVariant ...
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_statistics")
                    );

                auto_stmt->SetParam(CVariant((const char*) NULL), "@table_name");
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());
            }

            // Doesn't work for some reason ...
            if (false) {
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_statistics")
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
                    GetConnection().GetCallableStatement("DBAPI_Sample..TestBigIntProc")
                    );

                auto_stmt->SetParam(CVariant(Int8(1234567890)), "@num");
                auto_stmt->ExecuteUpdate();
            }
        }

        // Test output parameters ...
        if (false) {
            CRef<CDB_UserHandler_Diag> handler(new  CDB_UserHandler_Diag());
            I_DriverContext* drv_context = GetDS().GetDriverContext();

            drv_context->PushDefConnMsgHandler(handler);

            auto_ptr<ICallableStatement> auto_stmt(
                GetConnection().GetCallableStatement("DBAPI_Sample..SampleProc3")
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
                     case eDB_ComputeResult:
                         break;
                     case eDB_StatusResult:
                         break;
                     case eDB_CursorResult:
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

            drv_context->PopDefConnMsgHandler(handler);
        }

        // Temporary test ...
        if (false) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
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

        // Temporary test ...
        if (false && m_args.GetServerType() != CTestArguments::eSybase) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "pmcupdate",
                "*******",
                "PMC3QA",
                "PMC3QA"
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->PrepareCall("id_new_id")
                );
            auto_stmt->SetParam(CVariant("tsub2"), "@IdName");

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
CDBAPIUnitTest::Test_Procedure2(void)
{
    try {
        {
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            auto_stmt->Execute();

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            int value_int = rs->GetVariant(1).GetInt4();
                            BOOST_CHECK(value_int != 0);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // Mismatched types of INT parameters ...
        {
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            // Set parameter to 2 ...
            // sp_server_info takes an INT parameter ...
            auto_stmt->SetParam( CVariant( Int8(2) ), "@attribute_id" );
            auto_stmt->Execute();

            BOOST_CHECK_EQUAL( size_t(1), GetNumOfRecords(auto_stmt) );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Procedure3(void)
{
    try {
        // Reading multiple result-sets from a stored procedure ...
        {
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_helpdb")
                    );

            auto_stmt->SetParam( CVariant( "master" ), "@dbname" );
            auto_stmt->Execute();

            int result_num = 0;

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while (rs->Next()) {
                        ;
                    }

                    ++result_num;
                }
            }

            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL(result_num, 3);
            } else {
                BOOST_CHECK_EQUAL(result_num, 2);
            }
        }

        // The same as above, but using IStatement ...
        {
            auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

            int result_num = 0;

            auto_stmt->SendSql("exec sp_helpdb 'master'");
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while (rs->Next()) {
                        ;
                    }

                    ++result_num;
                }
            }

            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL(result_num, 4);
            } else {
                BOOST_CHECK_EQUAL(result_num, 3);
            }
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
    string sql;
    string table_name = "#exception_table";
    // string table_name = "exception_table";

    // Very first test ...
    {
        // Try to catch a base class ...
        BOOST_CHECK_THROW(ES_01_Internal(*m_Conn), CDB_Exception);
    }

    // Second test ...
    // Restore after invalid statement ...
    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        if (table_name[0] == '#') {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id INT PRIMARY KEY \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }


        auto_stmt->ExecuteUpdate("INSERT " + table_name + "(id) VALUES(17)");

        try {
            // Try to insert duplicate value ...
            auto_stmt->ExecuteUpdate("INSERT " + table_name + "(id) VALUES(17)");
        } catch (const CDB_Exception&) {
            // ignore it ...
        }

        try {
            // execute invalid statement ...
            auto_stmt->ExecuteUpdate("ISERT " + table_name + "(id) VALUES(17)");
        } catch (const CDB_Exception&) {
            // ignore it ...
        }

        // Check status of the statement ...
        if (false) {
            auto_stmt->ExecuteUpdate("SELECT max(id) FROM " + table_name);
        } else {
            auto_stmt->SendSql("SELECT max(id) FROM " + table_name);
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
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_MsgToEx(void)
{
    // First test ...
    {
        int msg_num = 0;
        CDB_Exception* dbex = NULL;

        auto_ptr<IConnection> local_conn(
            GetDS().CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        // 1.
        {
            msg_num = 0;

            BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);

            while((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 1a.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 2.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(true);

            BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 1);
        }

        // 3.
        if (true) {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 4.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(true);
            // Disable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }
    }

    // Second test ...
    {
        auto_ptr<IConnection> local_conn(
            GetDS().CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        local_conn->GetCDB_Connection()->PushMsgHandler(
            new CDB_UserHandler_Exception_NoThrow,
            eTakeOwnership
            );

        // Enable MsgToEx ...
        // local_conn->MsgToEx(true);
        // Disable MsgToEx ...
        // local_conn->MsgToEx(false);

        BOOST_CHECK_THROW(ES_01_Internal(*local_conn), CDB_Exception);
    }
}


///////////////////////////////////////////////////////////////////////////////
// This code is based on a problem reported by Baoshan Gu.


class BaoshanGu_handler : public CDB_UserHandler
{
public:
     virtual ~BaoshanGu_handler(void)
     {
     };

     virtual bool HandleIt(CDB_Exception* ex)
     {
         if (!ex)  return false;

         // Ignore errors with ErrorCode set to 0
         // (this is related mostly to the FreeTDS driver)
         if (ex->GetDBErrCode() == 0
             // || ex->Message().find("ERROR") == string::npos
            ) {
             return true;
         }

         NcbiCout << "BaoshanGu_handler called." << endl;
         throw *ex;
         //ex->Throw();

         return true;
     }
};


void
CDBAPIUnitTest::Test_MsgToEx2(void)
{
    CRef<BaoshanGu_handler> handler(new BaoshanGu_handler);
    I_DriverContext* drv_context = GetDS().GetDriverContext();

    drv_context->PushCntxMsgHandler(handler);
    drv_context->PushDefConnMsgHandler(handler);

    auto_ptr<IConnection> conn(GetDS().CreateConnection());
    conn->Connect("anyone","allowed", "REFTRACK_DEV", "x_locus");

    // conn->MsgToEx(true);
    conn->MsgToEx(false);

    auto_ptr<IStatement> stmt(conn->GetStatement());

    try {
        // string sql = " EXEC locusXref..undelete_locus_id @locus_id = 135896, @login='guba'";
        string sql = "EXEC locusXref..dbapitest";

        stmt->ExecuteUpdate(sql); //or SendSql
    } catch (const CDB_Exception& e) {
        NcbiCout << "CDB_Exception:" << e.Message() << endl;
    }

    CDB_Exception* dbex = NULL;
    while ((dbex = conn->GetErrorAsEx()->Pop())){
        cout << "MSG:" << dbex->Message() << endl;
    }

    drv_context->PopCntxMsgHandler(handler);
    drv_context->PopDefConnMsgHandler(handler);
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
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
                // ClearParamList is necessary here ...
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
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(0), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 0 );
                DumpResults(auto_stmt.get());
            }

            // Read second inserted value back ...
            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(1), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 1 );
                DumpResults(auto_stmt.get());
            }

            // Clean previously inserted data ...
            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

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
    const string table_name = GetTableName();
    // const string table_name("DBAPI_Sample..dbapi_unit_test");

    enum {rec_num = 10};
    string sql;

    try {
        // Initialize data (strings are NOT empty) ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + table_name +
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
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, table_name)
                                  );
            }

            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

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
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, "#test_unicode_table"));
            }
        }

        // Check ...
        if (true) {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
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
#ifdef NCBI_OS_SOLARIS
        // On Solaris GetNumOfRecords gives an error with MS SQL database
        if (m_args.GetDriverName() != dblib_driver
            ||  m_args.GetServerType() == CTestArguments::eSybase)
#endif
        {
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

                // Set parameter to NULL ...
                auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
                auto_stmt->Execute();

                if (m_args.GetServerType() == CTestArguments::eSybase) {
                    BOOST_CHECK_EQUAL( size_t(30), GetNumOfRecords(auto_stmt) );
                } else {
                    BOOST_CHECK_EQUAL( size_t(29), GetNumOfRecords(auto_stmt) );
                }

                // Set parameter to 1 ...
                auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
                auto_stmt->Execute();

                BOOST_CHECK_EQUAL( size_t(1), GetNumOfRecords(auto_stmt) );
            }

            {
            }
        }


        // Special case: empty strings and strings with spaces.
        if (m_args.GetDriverName() != ftds_odbc_driver) {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Initialize data (strings are EMPTY) ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + table_name +
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
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                                                                table_name));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
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

#ifdef NCBI_OS_SOLARIS
                        // Another version of Sybase client is used on Solaris,
                        // ctlib driver works there differently
                        if (m_args.GetDriverName() == ctlib_driver) {
                            const string sybase_version = GetSybaseClientVersion();
                            if (NStr::CompareNocase(sybase_version, 0, 4, "12.0") == 0) {
                                BOOST_CHECK( vc1000_field.IsNull() );
                                continue;
                            }
                        }
#endif

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        // Old protocol version has this strange feature
                        if (m_args.GetServerType() == CTestArguments::eSybase
                            || m_args.GetDriverName() == dblib_driver
                           )
                        {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string(" ") );
                        }
                        else {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string() );
                        }
                    }
                }

                DumpResults(auto_stmt.get());
            }

            // Initialize data (strings are full of spaces) ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(string("    ")), "@vc1000_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
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
                        // Old protocol version has this strange feature
                        if (m_args.GetServerType() == CTestArguments::eSybase
                            || m_args.GetDriverName() == dblib_driver
                           )
                        {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string(" ") );
                        }
                        else {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string("    ") );
                        }
                    }
                }

                DumpResults(auto_stmt.get());
            }
        } else {
            m_args.PutMsgDisabled("Testing of NULL-value + empty string is disabled.");
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
                auto_ptr<IStatement> stmt( GetConnection().GetStatement() );

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
                auto_ptr<IStatement> stmt( GetConnection().GetStatement() );

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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
    // auto_ptr<IStatement> stmt( GetConnection().CreateStatement() );
    // _ASSERT(curr_stmt->get());
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

    // Insert row_count records into the table ...
    for ( Int4 i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
            auto_stmt.reset( GetConnection().GetStatement() );
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
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

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
                GetConnection().GetCDB_Connection(),
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
        CMsgHandlerGuard guard(GetDriverContext());

        sql = "sleep 0 kill";
        const unsigned orig_conn_num = GetDriverContext().NofConnections();

        for (unsigned i = 0; i < num_of_tests; ++i) {
            // Start a connection scope ...
            {
                bool conn_killed = false;

                auto_ptr<CDB_Connection> auto_conn;

                auto_conn.reset(GetDriverContext().Connect(
                        "LINK_OS_INTERNAL",
                        "anyone",
                        "allowed",
                        0
                    ));

                // kill connection ...
                try {
                    auto_ptr<CDB_LangCmd> auto_stmt(auto_conn->LangCmd(sql));
                    auto_stmt->Send();
                    auto_stmt->DumpResults();
                } catch (const CDB_Exception&) {
                    // Ignore it ...
                }

                try {
                    auto_ptr<CDB_RPCCmd> auto_stmt(auto_conn->RPC("sp_who"));
                    auto_stmt->Send();
                    auto_stmt->DumpResults();
                } catch (const CDB_Exception&) {
                    conn_killed = true;
                }

                BOOST_CHECK(conn_killed);
            }

            BOOST_CHECK_EQUAL(
                orig_conn_num,
                GetDriverContext().NofConnections()
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
    {
        enum {
            eN = 5
        };

        auto_ptr<IConnection> auto_conns[eN + 1];

        // There are already 2 connections - so +2
        CDriverManager::GetInstance().SetMaxConnect(eN + 2);
        for (int i = 0; i < eN; ++i) {
            auto_ptr<IConnection> auto_conn(GetDS().CreateConnection(CONN_OWNERSHIP));
            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            auto_conns[i] = auto_conn;
        }

        try {
            auto_conns[eN].reset(GetDS().CreateConnection(CONN_OWNERSHIP));
            Connect(auto_conns[eN]);
            BOOST_FAIL("Connection above limit is created");
        }
        catch (CDB_Exception&) {
            // exception thrown - it's ok
            LOG_POST("Connection above limit is rejected - that is ok");
        }
    }

    // This test is not supposed to be run every day.
    if (false) {
        enum {
            eN = 50
        };

        auto_ptr<IConnection> auto_conns[eN];

        // There are already 2 connections - so +2
        CDriverManager::GetInstance().SetMaxConnect(eN + 2);
        for (int i = 0; i < eN; ++i) {
            auto_ptr<IConnection> auto_conn(GetDS().CreateConnection(CONN_OWNERSHIP));
            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            auto_conns[i] = auto_conn;
        }
    }
    else {
        m_args.PutMsgDisabled("Large connections amount");
    }
}


////////////////////////////////////////////////////////////////////////////////
static
IDBServiceMapper*
MakeCDBUDPriorityMapper01(const IRegistry* registry)
{
    TSvrRef server01(new CDBServer("MSDEV1"));
    TSvrRef server02(new CDBServer("MSDEV2"));
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
    TSvrRef server01(new CDBServer("MSDEV1"));
    const string service_name("TEST_SERVICE_01");
    auto_ptr<IConnection> conn;

    // Install new mapper ...
    ncbi::CDbapiConnMgr::Instance().SetConnectionFactory(
        factory(MakeCDBUDPriorityMapper01)
    );

    // Create connection ...
    conn.reset(GetDS().CreateConnection(CONN_OWNERSHIP));
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
    const string db_name("AGP"); // This database should exist in MSDEV1, and not in MSDEV2

    // CTrivialConnValidator ...
    {
        CTrivialConnValidator validator(db_name);

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
        CValidator validator(db_name);

        Check_Validator(factory_factory, validator);
    }

    // Another customized validator ...
    {

        // Connection validator to check against DBAPI_Sample ...
        CValidator01 validator(db_name);

        Check_Validator(factory_factory, validator);
    }

    // One more ...
    {
        // Connection validator to check against DBAPI_Sample ...
        CValidator02 validator(db_name);

        Check_Validator(factory_factory, validator);
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_ConnFactory(void)
{
    enum {num_of_tests = 128};

    try {
        TSvrRef server01(new CDBServer("mssql51.ncbi.nlm.nih.gov"));
        TSvrRef server02(new CDBServer("mssql51.ncbi.nlm.nih.gov"));
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
                GetDS().GetDriverContext()->Connect(
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
                GetDS().GetDriverContext()->Connect(
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
            BOOST_CHECK(value_VarChar.Value() == NULL);
            BOOST_CHECK(value_Char.Value() == NULL);
            BOOST_CHECK(value_LongChar.Value() == NULL);
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
            BOOST_CHECK(value_VarChar.Value() != NULL);
            BOOST_CHECK(value_VarChar2.Value() != NULL);
            // !!!
            BOOST_CHECK(value_VarChar3.Value() == NULL);
            BOOST_CHECK(value_VarChar4.Value() != NULL);
            BOOST_CHECK(value_Char.Value() != NULL);
            BOOST_CHECK(value_Char2.Value() != NULL);
            // !!!
            BOOST_CHECK(value_Char3.Value() == NULL);
            BOOST_CHECK(value_Char4.Value() != NULL);
            BOOST_CHECK(value_LongChar.Value() != NULL);
            BOOST_CHECK(value_LongChar2.Value() != NULL);
            // !!!
            BOOST_CHECK(value_LongChar3.Value() == NULL);
            BOOST_CHECK(value_LongChar4.Value() != NULL);
            BOOST_CHECK(value_VarBinary.Value() != NULL);
            BOOST_CHECK(value_Binary.Value() != NULL);
            BOOST_CHECK(value_LongBinary.Value() != NULL);
            BOOST_CHECK(value_Float.Value() != 0);
            BOOST_CHECK(value_Double.Value() != 0);
            BOOST_CHECK(value_SmallDateTime.Value() != CTime());
            BOOST_CHECK(value_DateTime.Value() != CTime());
            BOOST_CHECK(value_Numeric.Value() != string("0"));
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
            static unsigned int s_tTimeOut = 30; // in seconds
            CDB_Connection* pDbLink = NULL;

            string sDbDriver(m_args.GetDriverName());
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


void
CDBAPIUnitTest::Test_Authentication(void)
{
    try {
        // MSSQL10 has SSL certificate.
        // There is no MSSQL10 any more ...
    //     {
    //         auto_ptr<IConnection> auto_conn( GetDS().CreateConnection() );
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
            auto_ptr<IConnection> auto_conn( GetDS().CreateConnection() );

            auto_conn->Connect(
                "NCBI_NT\\anyone",
                "Perm1tted",
                "MSDEV2"
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


void
CDBAPIUnitTest::Test_Query_Cancelation(void)
{
    string sql;

    try {
        // Cancel without a previously thrown exception.
        {
            // IStatement
            {
                auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

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

                const char *proc_name =
                    m_args.GetServerType() == CTestArguments::eMsSql2005 ?
                                                     "sp_oledb_database" :
                                                     "sp_databases";

                // 1
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
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
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 4
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
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
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
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
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
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
                auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

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
                    auto_stmt.reset(GetConnection().GetCallableStatement("sp_wrong"));
                    auto_stmt->ExecuteUpdate();
                } catch(const CDB_Exception&)
                {
                    auto_stmt->Cancel();
                }

                try {
                    auto_stmt->ExecuteUpdate();
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
        auto_ptr<IConnection> auto_conn;
        auto_ptr<IStatement> auto_stmt;

        // Alter DriverContext ...
        {
            I_DriverContext* dc = GetDS().GetDriverContext();
            unsigned int timeout = dc->GetTimeout();

            dc->SetTimeout(2);

            // Create connection ...
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            Test_WaitForDelay(auto_conn);

            // Crete new connection because some drivers (ftds8 for example)
            // can close connection in the Test_WaitForDelay test.
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            //
            // Check selecting from a huge table ...
            Test_HugeTableSelect(auto_conn);

            dc->SetTimeout(timeout);
        } // Alter DriverContext ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


void CDBAPIUnitTest::Test_Timeout2(void)
{
    try {
        auto_ptr<IConnection> auto_conn;
        auto_ptr<IStatement> auto_stmt;


        // Alter connection ...
        {
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_conn->SetTimeout(2);

            Test_WaitForDelay(auto_conn);

            //
            // Check selecting from a huge table ...
            Test_HugeTableSelect(auto_conn);

        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_WaitForDelay(const auto_ptr<IConnection>& auto_conn)
{
    auto_ptr<IStatement> auto_stmt;
    bool timeout_was_reported = false;

    auto_stmt.reset(auto_conn->GetStatement());

    if(m_args.GetDriverName() == ftds8_driver) {
        try {
            auto_stmt->SendSql("waitfor delay '0:00:04'");
            BOOST_CHECK(auto_stmt->HasMoreResults());
        } catch(const CDB_Exception&) {
            timeout_was_reported = true;
            auto_stmt->Cancel();
        } catch(...) {
            BOOST_CHECK(false);
        }
    } else {
        try {
            auto_stmt->SendSql("waitfor delay '0:00:04'");
            BOOST_CHECK(auto_stmt->HasMoreResults());
        } catch(const CDB_TimeoutEx&) {
            timeout_was_reported = true;
            auto_stmt->Cancel();
        }
    }


    // Check if connection is alive ...
    if (m_args.GetDriverName() != ftds8_driver) {
        auto_stmt->SendSql("SELECT name FROM sysobjects");
        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );
    } else {
        m_args.PutMsgDisabled("Test_Timeout. Check if connection is alive.");
    }

    BOOST_CHECK(timeout_was_reported);
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_HugeTableSelect(const auto_ptr<IConnection>& auto_conn)
{
    const string table_name("#huge_table");


    // Check selecting from a huge table ...
    const char* str_value = "Oops ...";
    size_t rec_num = 200000;
    string sql;
    auto_ptr<IStatement> auto_stmt;

    auto_stmt.reset(auto_conn->GetStatement());

    if (!m_args.IsBCPAvailable()) {
        rec_num = 15000;
    }

    // Preparation ...
    {
        string sql;
        
        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(1500) NOT NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }
    }

    // Populate table with data ...
    {
        CStopWatch timer(CStopWatch::eStart);

        if (m_args.IsBCPAvailable()) {
            CVariant col1(eDB_Int);
            CVariant col2(eDB_VarChar);

            auto_ptr<IBulkInsert> bi(
                    auto_conn->GetBulkInsert(table_name)
                    );

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            col2 = str_value;

            for (size_t i = 0; i < rec_num; ++i) {
                col1 = Int4(i);

                bi->AddRow();

                if (i % 1000 == 0) {
                    bi->StoreBatch();
                }
            }
            bi->Complete();
        } else {
            sql = "INSERT INTO " + table_name +
                "(id, vc8000_field) VALUES(@id, @vc_val)";

            //auto_stmt->ExecuteUpdate("BEGIN TRANSACTION");

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                auto_stmt->SetParam( CVariant::VarChar(str_value), "@vc_val");

                auto_stmt->ExecuteUpdate( sql );

                /*if (i % 100 == 0) {
                    auto_stmt->ExecuteUpdate("COMMIT TRANSACTION");
                    auto_stmt->ExecuteUpdate("BEGIN TRANSACTION");
                    }*/
            }

            //auto_stmt->ExecuteUpdate("COMMIT TRANSACTION");
        }

        LOG_POST( "Huge table inserted in " << timer.Elapsed() << " sec." );
    }

    BOOST_CHECK(GetNumOfRecords(auto_stmt, table_name) == rec_num);

    // Read data ...
    {
        size_t num = 0;

        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());

        CStopWatch timer(CStopWatch::eStart);

        auto_stmt->SendSql("SELECT * FROM " + table_name);

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                while (rs->Next()) {
                    ++num;
                }
            }
        }

        LOG_POST( "Huge table selected in " << timer.Elapsed() << " sec." );

        BOOST_CHECK(num == rec_num);
    }

    {
        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
        auto_stmt->ExecuteUpdate("drop table " + table_name);
    }
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_SetLogStream(void)
{
    try {
        CNcbiOfstream logfile("dbapi_unit_test.log");

        GetDS().SetLogStream(&logfile);
        GetDS().SetLogStream(&logfile);

        // Test block ...
        {
            // No errors ...
            {
                auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                BOOST_CHECK(auto_conn.get() != NULL);

                Connect(auto_conn);

                auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK(auto_stmt->HasMoreResults());
                DumpResults(auto_stmt.get());
            }

            GetDS().SetLogStream(&logfile);

            // Force errors ...
            {
                auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
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

            GetDS().SetLogStream(&logfile);
        }

        // Install user-defined error handler (eTakeOwnership)
        {
            {
                I_DriverContext* drv_context = GetDS().GetDriverContext();

                if (m_args.IsODBCBased()) {
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
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                GetDS().SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
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

                GetDS().SetLogStream(&logfile);
            }
        }

        // Install user-defined error handler (eNoOwnership)
        {
            CMsgHandlerGuard handler_guard(*GetDS().GetDriverContext());

            // Test block ...
            {
                // No errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    Connect(auto_conn);

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                GetDS().SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
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

                GetDS().SetLogStream(&logfile);
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
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

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
CDBAPIUnitTest::Test_ClearParamList(void)
{
    const long rec_num  = 10;
    const string table_name = GetTableName();
    const string str_value = "asdfghjkl";
    string sql;

    // Initialize data (strings are full of spaces) ...
    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);
            }

            {
                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(string(str_value)), "@vc1000_field" );

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                            GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    // const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL(vc1000_field.GetString(), str_value);
                    /*
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
                    */
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Truncation(void)
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
void CDBAPIUnitTest::Test_ConnParams(void)
{
    // Checking parser ...
    if (true) {

        // CDBUriConnParams ...
        {
            CDBUriConnParams params("dbapi:ctlib://myuser:mypswd@MAPVIEW_LOAD:1433/AlignModel?tds_version=42&client_charset=UTF8");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("ctlib"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("mypswd"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi:ftds://myuser@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("ftds"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser:allowed@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser:allowed@MAPVIEW_LOAD");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string(""));
        }

        // CDB_ODBC_ConnParams ..
        {
            CDB_ODBC_ConnParams params("Driver={SQLServer};Server=MS_TEST;Database=DBAPI_Sample;Uid=anyone;Pwd=allowed;");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("{SQLServer}"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("anyone"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MS_TEST"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("DBAPI_Sample"));
        }
    }


    // Checking ability to connect using different connection-parameter classes ...
    if (true) {
        // CDBUriConnParams ...
        {
            {
                CDBUriConnParams params(
                        "dbapi:" + m_args.GetDriverName() +
                        "://" + m_args.GetUserName() +
                        ":" + m_args.GetUserPassword() +
                        "@" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDBUriConnParams params(
                        "dbapi:" // No driver name ...
                        "//" + m_args.GetUserName() +
                        ":" + m_args.GetUserPassword() +
                        "@" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDBUriConnParams params(
                        "dbapi:" // No driver name ...
                        "//" + m_args.GetUserName() +
                        // No password ...
                        "@" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

        }

        // CDB_ODBC_ConnParams ...
        {
            {
                CDB_ODBC_ConnParams params(
                        "DRIVER=" + m_args.GetDriverName() +
                        ";UID=" + m_args.GetUserName() +
                        ";PWD=" + m_args.GetUserPassword() +
                        ";SERVER=" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDB_ODBC_ConnParams params(
                        // No driver ...
                        ";UID=" + m_args.GetUserName() +
                        ";PWD=" + m_args.GetUserPassword() +
                        ";SERVER=" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDB_ODBC_ConnParams params(
                        // No driver ...
                        ";UID=" + m_args.GetUserName() +
                        // No password ...
                        ";SERVER=" + m_args.GetServerName()
                        );

                IDataSource* ds = m_DM.MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

        }
    }

    // Check CDBEnvConnParams ...
    {
        CDBUriConnParams uri_params("dbapi://wrong_user:wrong_pswd@wrong_server/wrong_db");
        CDBEnvConnParams params(uri_params);
        CNcbiEnvironment env;

        env.Set("DBAPI_SERVER", m_args.GetServerName());
        env.Set("DBAPI_DATABASE", "DBAPI_Sample");
        env.Set("DBAPI_USER", m_args.GetUserName());
        env.Set("DBAPI_PASSWORD", m_args.GetUserPassword());

        IDataSource* ds = m_DM.MakeDs(params);
        auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
        conn->Connect(params);
        auto_ptr<IStatement> auto_stmt(conn->GetStatement());
        auto_stmt->ExecuteUpdate("SELECT @@version");

        env.Reset();
    }

    // CDBInterfacesFileConnParams ...
    {
        CDBUriConnParams uri_params(
                "dbapi:" // No driver name ...
                "//" + m_args.GetUserName() +
                ":" + m_args.GetUserPassword() +
                "@" + m_args.GetServerName()
                );

        CDBInterfacesFileConnParams params(uri_params);

        IDataSource* ds = m_DM.MakeDs(params);
        auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
        conn->Connect(params);
        auto_ptr<IStatement> auto_stmt(conn->GetStatement());
        auto_stmt->ExecuteUpdate("SELECT @@version");
    }

}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_BindByPos(void)
{
    const long rec_num  = 10;
    const string table_name = GetTableName();
    const string str_value = "asdfghjkl";
    string sql;

    // Initialize data (strings are full of spaces) ...
    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);
            }

            {
                sql  = " INSERT INTO " + table_name +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(string(str_value)), 2);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), 1);
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), 1);
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                            GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    // const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL(vc1000_field.GetString(), str_value);
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }

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


void
CDBAPIUnitTest::Test_Heavy_Load(void)
{
    try {
        // Heavy bulk-insert
        Test_HugeTableSelect(m_Conn);

        string table_name = "#test_heavy_load";
        enum {num_tests = 30000};
        {
            string sql = "create table " + table_name + " ("
                         "int_field int,"
                         "flt_field float,"
                         "date_field datetime,"
                         "vc200_field varchar(200),"
                         "vc2000_field varchar(2000),"
                         "txt_field text"
                         ")";

            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
            auto_stmt->ExecuteUpdate( sql );
        }

        // Heavy insert with parameters
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
            auto_ptr<IStatement> auto_stmt2( m_Conn->GetStatement() );

            string sql = "INSERT INTO " + table_name +
                         " VALUES(@int_field, @flt_field, @date_field, "
                                 "@vc200_field, @vc2000_field, @txt_field)";

            string vc200_val = string(190, 'a');
            string vc2000_val = string(1500, 'z');
            string txt_val = string(2000, 'q');

            CStopWatch timer(CStopWatch::eStart);

            auto_stmt->SetParam( CVariant( Int4(123456) ), "@int_field" );
            auto_stmt->SetParam( CVariant(654.321), "@flt_field" );
            auto_stmt->SetParam( CVariant(CTime(CTime::eCurrent)),
                                 "@date_field" );
            auto_stmt->SetParam( CVariant::VarChar(vc200_val.c_str(),
                                                   vc200_val.size()),
                                 "@vc200_field" );
            auto_stmt->SetParam( CVariant::LongChar(vc2000_val.c_str(),
                                                    vc2000_val.size()),
                                 "@vc2000_field"
                                 );
            auto_stmt->SetParam( CVariant::VarChar(txt_val.c_str(),
                                                   txt_val.size()),
                                 "@txt_field" );

            auto_stmt2->ExecuteUpdate("BEGIN TRAN");
            for (int i = 0; i < num_tests; ++i) {
                auto_stmt->ExecuteUpdate( sql );

                if (i % 1000 == 0) {
                    auto_stmt2->ExecuteUpdate("COMMIT TRAN");
                    auto_stmt2->ExecuteUpdate("BEGIN TRAN");
                }
            }
            auto_stmt2->ExecuteUpdate("COMMIT TRAN");
            LOG_POST( "Inserts made in " << timer.Elapsed() << " sec." );
        }

        // Heavy select
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            string sql = "select * from " + table_name;

            IResultSet* rs;

            CStopWatch timer(CStopWatch::eStart);

            rs = auto_stmt->ExecuteQuery(sql);
            rs->BindBlobToVariant(true);

            while (rs->Next()) {
                /*int int_val =*/ rs->GetVariant(1).GetInt4();
                /*double flt_val =*/ rs->GetVariant(2).GetDouble();
                CTime date_val = rs->GetVariant(3).GetCTime();
                string vc1_val = rs->GetVariant(4).GetString();
                string vc2_val = rs->GetVariant(5).GetString();
                const CVariant& txt_var = rs->GetVariant(6);
                string txt_val;
                txt_val.resize(txt_var.GetBlobSize());
                txt_var.Read(&txt_val[0], txt_val.size());
            }

            LOG_POST( "Select finished in " << timer.Elapsed() << " sec." );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

static
string GetSybaseClientVersion(void)
{
    CNcbiEnvironment env;
    string sybase_version = env.Get("SYBASE");
    CDirEntry dir_entry(sybase_version);
    dir_entry.DereferenceLink();
    sybase_version = dir_entry.GetPath();

    sybase_version = sybase_version.substr(
        sybase_version.find_last_of('/') + 1
        );

    return sybase_version;
}

////////////////////////////////////////////////////////////////////////////////
CDBAPITestSuite::CDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
    enum EOsType {eOsUnknown, eOsSolaris, eOsWindows, eOsIrix};
    enum ECompilerType {eCompilerUnknown, eCompilerWorkShop, eCompilerGCC};
    
    EOsType os_type = eOsUnknown;
    ECompilerType compiler_type = eCompilerUnknown;
    
    bool sybase_client_v125 = false;
    bool sybase_client_v120_solaris = false;

#if defined(NCBI_OS_SOLARIS)
    os_type = eOsSolaris;
#endif

#if defined(NCBI_COMPILER_WORKSHOP)
    compiler_type = eCompilerWorkShop;
#endif

#if defined(NCBI_COMPILER_GCC)
    compiler_type = eCompilerGCC;
#endif

#if defined(NCBI_OS_IRIX)
    os_type = eOsIrix;
#endif

#if defined(NCBI_OS_MSWIN)
    os_type = eOsWindows;
#endif

    // Get Sybase client version number (at least try to do that).
    const string sybase_version = GetSybaseClientVersion();
    if (NStr::CompareNocase(sybase_version, 0, 4, "12.5") == 0
        || NStr::CompareNocase(sybase_version, "current") == 0) {
        sybase_client_v125 = true;
    }

    if (os_type == eOsSolaris && NStr::CompareNocase(sybase_version, 0, 4, "12.0") == 0) {
        sybase_client_v120_solaris = true;
    }

    // add member function test cases to a test suite
    boost::shared_ptr<CDBAPIUnitTest> DBAPIInstance(new CDBAPIUnitTest(args));
    boost::unit_test::test_case* tc = NULL;

    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        if (args.DriverAllowsMultipleContexts()) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_Many,
                                    DBAPIInstance);
            add(tc);
        } else if (args.GetDriverName() != dblib_driver) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_One,
                    DBAPIInstance);
            add(tc);
        }
    }


    boost::unit_test::test_case* tc_init =
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::TestInit, DBAPIInstance);

    add(tc_init);

    // if (args.GetServerType() == CTestArguments::eMsSql2005) {
    if (false) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_VARCHAR_MAX,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (args.IsBCPAvailable()
        && args.GetServerType() == CTestArguments::eMsSql2005
        && args.GetDriverName() != dblib_driver
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_VARCHAR_MAX_BCP,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (!(args.GetDriverName() == ctlib_driver && sybase_client_v120_solaris)
        // We probably have an old version of Sybase Client installed on
        // Windows ...
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsWindows) // 04/08/08
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CHAR,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    // Under development ...
    if (false) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Truncation,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (false && !(args.GetDriverName() == dblib_driver && args.GetServerType() == CTestArguments::eSybase)) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ConnParams,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (args.GetTestConfiguration() != CTestArguments::eFast) 
    {
        if (args.GetServerType() != CTestArguments::eSybase
            && args.GetDriverName() != dblib_driver) 
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ConnFactory,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_ConnFactory");
        }
    }

    if (args.GetTestConfiguration() != CTestArguments::eFast) 
    {
        if (true) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ConnPool,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_ConnPool");
        }
    }


    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        if (args.GetServerType() == CTestArguments::eSybase && os_type != eOsSolaris) 
        {
            // Solaris has an outdated version of Sybase client installed ...
            if (args.GetDriverName() != dblib_driver // Cannot connect to the server ...
                    && args.GetDriverName() != ftds_dblib_driver // Cannot connect to the server ...
                    && args.GetDriverName() != ftds8_driver
               ) 
            {
                tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DropConnection,
                        DBAPIInstance);
                tc->depends_on(tc_init);
                add(tc);
            } else {
                args.PutMsgDisabled("Test_DropConnection");
            }
        }
    }

    if ((args.GetServerType() == CTestArguments::eMsSql
        || args.GetServerType() == CTestArguments::eMsSql2005)
        && args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != dblib_driver
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BlobStore,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_BlobStore");
    }

    if (args.GetTestConfiguration() != CTestArguments::eWithoutExceptions
        && args.GetTestConfiguration() != CTestArguments::eFast
        ) 
    {
        // It looks like ftds on WorkShop55_550-DebugMT64 doesn't work ...
        if ( (args.GetDriverName() == ftds8_driver
            && !(os_type == eOsSolaris && compiler_type == eCompilerWorkShop) && os_type != eOsIrix
            )
            || (args.GetDriverName() == dblib_driver && args.GetServerType() == CTestArguments::eSybase)
            || (args.GetDriverName() == ctlib_driver && os_type != eOsSolaris)
            || args.GetDriverName() == ftds64_driver
            || args.IsODBCBased()
            ) 
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Timeout,
                                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_Timeout");
        }
    }

    if (args.GetTestConfiguration() != CTestArguments::eWithoutExceptions
        && args.GetTestConfiguration() != CTestArguments::eFast
        ) 
    {
        if (args.GetDriverName() != ctlib_driver // SetTimeout is not supported. API doesn't support that.
            && args.GetDriverName() != dblib_driver // SetTimeout is not supported. API doesn't support that.
            && args.GetDriverName() != ftds_dblib_driver // SetTimeout is not supported. API doesn't support that.
            && args.GetDriverName() != ftds8_driver // 04/09/08 Doesn't report timeout ...
            ) 
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Timeout2,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_Timeout2");
        }
    }

    // There is a problem with ftds driver
    // on GCC_340-ReleaseMT--i686-pc-linux2.4.23
    if (args.GetDriverName() != ftds8_driver
        && args.GetDriverName() != ftds_dblib_driver
        && !(args.GetDriverName() == ftds8_driver && os_type == eOsSolaris)
        && !(args.GetDriverName() == ftds8_driver
             && args.GetServerType() == CTestArguments::eSybase)
        && (args.GetDriverName() != dblib_driver
             || args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Query_Cancelation,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Query_Cancelation");
    }


    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        if ((args.GetServerType() == CTestArguments::eMsSql
            || args.GetServerType() == CTestArguments::eMsSql2005) &&
            (args.GetDriverName() == ftds_odbc_driver
            || args.GetDriverName() == ftds64_driver)
            ) 
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Authentication,
                                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_Authentication");
        }
    }

    //
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CDB_Object, DBAPIInstance));
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant, DBAPIInstance));
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CDB_Exception,
                              DBAPIInstance));

    if (args.GetDriverName() != ftds8_driver // Doesn't work 02/28/08 
        && args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != dblib_driver
       ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Numeric, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Numeric");
    }

    if (!(args.GetDriverName() == ftds8_driver
             && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == ftds_dblib_driver
             && args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Create_Destroy,
                                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Create_Destroy");
    }

    //
    if (!(args.GetDriverName() == ftds8_driver
          && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == ftds_dblib_driver
          && args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Multiple_Close,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Multiple_Close");
    }

    if (args.IsBCPAvailable()
        && os_type != eOsSolaris // Requires Sybase client 12.5
        // We probably have an old version of Sybase Client installed on
        // Windows ...
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsWindows) // 04/08/08
        && args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != odbc_driver // 04/04/08
        && !(args.GetDriverName() == dblib_driver && args.GetServerType() != CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing");
    }


    if (args.IsBCPAvailable()
        && args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != dblib_driver
        && args.GetDriverName() != odbc_driver // 04/04/08
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris)
        // We probably have an old version of Sybase Client installed on
        // Windows ...
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsWindows) // 04/08/08
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing2, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing2");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != dblib_driver
        && !(args.GetDriverName() == ftds_dblib_driver && args.GetServerType() == CTestArguments::eSybase)
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing3, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing3");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != ftds_dblib_driver
        && !(args.GetDriverName() == dblib_driver && args.GetServerType() != CTestArguments::eSybase)
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris)
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing4, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing4");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != ftds_dblib_driver
        && !(args.GetDriverName() == dblib_driver && args.GetServerType() != CTestArguments::eSybase)
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris)
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing5, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing5");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != odbc_driver // 04/04/08
        && !(args.GetDriverName() == dblib_driver && args.GetServerType() != CTestArguments::eSybase)
        && !(args.GetDriverName() == ftds_dblib_driver && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris) // 04/09/08
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Late_Bind, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Late_Bind");
    }

    if (args.IsBCPAvailable()
        && !(args.GetDriverName() == dblib_driver && args.GetServerType() != CTestArguments::eSybase)
        && !(args.GetDriverName() == ftds_dblib_driver && args.GetServerType() == CTestArguments::eSybase)
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing6, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Bulk_Writing6");
    }

    boost::unit_test::test_case* tc_parameters = NULL;

    if (args.GetDriverName() != ftds_dblib_driver) {
        tc_parameters =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_StatementParameters,
                                  DBAPIInstance);
        tc_parameters->depends_on(tc_init);
        add(tc_parameters);
    } else {
        args.PutMsgDisabled("Test_StatementParameters");
    }

    if (tc_parameters
        && (args.GetDriverName() != dblib_driver
            || args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetRowCount,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_GetRowCount");
    }

    // Doesn't work at the moment ...
    if (args.GetDriverName() != ftds8_driver // memory access violation 03/03/08 
        && !args.IsODBCBased() // 03/24/08 Statement(s) could not be prepared.
        && args.GetDriverName() != ftds_dblib_driver // 04/01/08 Results pending.
        && (args.GetDriverName() != dblib_driver || args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB_LowLevel,
                                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_LOB_LowLevel");
    }

    boost::unit_test::test_case* tc_cursor = NULL;

    if (tc_parameters
        && !(args.GetDriverName() == ftds8_driver
            && args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != dblib_driver // Code will hang up with dblib for some reason ...
        ) 
    {
        //
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
        tc_cursor = tc;
    } else {
        args.PutMsgDisabled("Test_Cursor");
    }

    if (tc_parameters
        && args.GetDriverName() != dblib_driver // 04/01/08
        && !(args.GetDriverName() == ftds8_driver
            && args.GetServerType() == CTestArguments::eSybase) // 04/01/08
        )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor2,
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Cursor2");
    }

    if (tc_cursor 
        && args.GetDriverName() != ctlib_driver) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor_Param,
            DBAPIInstance);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Cursor_Param");
    }

    // It looks like only ctlib driver can pass this test at the moment. // 04/09/08
    if (tc_cursor
        && args.GetDriverName() != ftds8_driver
        && args.GetDriverName() != ftds_driver
        && args.GetDriverName() != ftds_odbc_driver
        && args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != odbc_driver
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor_Multiple,
            DBAPIInstance);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Cursor_Multiple");
    }

    if (tc_cursor) 
    {
        // Does not work with all databases and drivers currently ...
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB, 
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_LOB");
    }

    if (tc_cursor) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB2, 
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_LOB2");
    }

    if (tc_cursor
        && args.GetDriverName() != ftds64_driver // 04/22/08  This result is not available anymore
        && args.GetDriverName() != ftds8_driver // 04/21/08  "Invalid text, ntext, or image pointer value"
        && args.GetDriverName() != ftds_odbc_driver // 04/21/08 Memory access violation
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB_Multiple, 
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_LOB_Multiple");
    }

    if (tc_cursor
        && args.GetDriverName() != ftds_odbc_driver // 04/21/08 "Statement(s) could not be prepared"
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB_Multiple_LowLevel, 
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_LOB_Multiple_LowLevel");
    }

    if (tc_cursor) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BlobStream,
            DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_cursor);
        tc->depends_on(tc_cursor);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_BlobStream");
    }

    // Not completed yet ...
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NVARCHAR,
//                                    DBAPIInstance);
//         tc->depends_on(tc_init);
//         tc->depends_on(tc_parameters);
//         add(tc);

    if (tc_parameters && 
        (args.GetDriverName() == odbc_driver
        // || args.GetDriverName() == ftds_odbc_driver
        || args.GetDriverName() == ftds64_driver
        // || args.GetDriverName() == ftds8_driver
        )
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UnicodeNB,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_UnicodeNB");
    }


    if (tc_parameters && 
        (args.GetDriverName() == odbc_driver
        // || args.GetDriverName() == ftds_odbc_driver
        || args.GetDriverName() == ftds64_driver
        )
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Unicode,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Unicode");
    }


    if (tc_parameters
        && args.GetDriverName() != ftds_dblib_driver
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_StmtMetaData,
                DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_StmtMetaData");
    }

    if (tc_parameters) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ClearParamList,
                DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_ClearParamList");
    }

    if (tc_parameters) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmt2, DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_SelectStmt2");
    }

    if (tc_parameters
        // ftds_dblib don't work with Sybase and gives very strange results with MS SQL
        && args.GetDriverName() != ftds_dblib_driver
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NULL, DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(tc_parameters);
        tc->depends_on(tc_parameters);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_NULL");
    }


    if (args.IsBCPAvailable()
        && args.GetDriverName() != ftds_dblib_driver
        && (args.GetDriverName() != dblib_driver || args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_BulkInsertBlob");
    }

    if(args.IsBCPAvailable()
        && args.GetDriverName() != odbc_driver // 04/04/08
        && args.GetDriverName() != ftds_dblib_driver
        && (args.GetDriverName() != dblib_driver || args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris && !sybase_client_v125)
        )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob_LowLevel,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_BulkInsertBlob_LowLevel");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != dblib_driver // Invalid parameters to bcp_bind ...
        && args.GetDriverName() != ftds_dblib_driver // Invalid parameters to bcp_bind ...
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob_LowLevel2,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_BulkInsertBlob_LowLevel2");
    }

    if (true) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_MsgToEx,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_MsgToEx");
    }

    /*
    if (args.GetServerType() == CTestArguments::eSybase
        && args.GetDriverName() != dblib_driver) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_MsgToEx2,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }
    */

    boost::unit_test::test_case* except_safety_tc = NULL;

    if (args.GetTestConfiguration() != CTestArguments::eFast) 
    {
        except_safety_tc =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Exception_Safety,
                    DBAPIInstance);
        except_safety_tc->depends_on(tc_init);
        add(except_safety_tc);
    }

    if (except_safety_tc)
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler,
                DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(except_safety_tc);
        tc->depends_on(except_safety_tc);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_UserErrorHandler");
    }

    boost::unit_test::test_case* select_stmt_tc = NULL;

    if (!(args.GetDriverName() == ftds8_driver
                && args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != dblib_driver // Because of "select qq = 57.55 + 0.0033" ...
        ) 
    {
        select_stmt_tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmt,
            DBAPIInstance);
        select_stmt_tc->depends_on(tc_init);
        add(select_stmt_tc);
    } else {
        args.PutMsgDisabled("Test_SelectStmt");
    }

    // There is a problem with the ftds8 driver and Sybase ...
    if (select_stmt_tc
        && !(args.GetDriverName() == ftds8_driver
            && args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != ftds_dblib_driver
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris && !sybase_client_v125)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Recordset,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(select_stmt_tc);
        tc->depends_on(select_stmt_tc);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Recordset");
    }

    //
    if (false
        && select_stmt_tc) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_ResultsetMetaData,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(select_stmt_tc);
        tc->depends_on(select_stmt_tc);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_ResultsetMetaData");
    }


    boost::unit_test::test_case* select_xml_stmt_tc = NULL;

    //
    if (select_stmt_tc
        && (args.GetServerType() == CTestArguments::eMsSql
            || args.GetServerType() == CTestArguments::eMsSql2005)
            && args.GetDriverName() != dblib_driver
            ) 
    {
        //
        select_xml_stmt_tc =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmtXML,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        _ASSERT(select_stmt_tc);
        tc->depends_on(select_stmt_tc);
        add(select_xml_stmt_tc);
    } else {
        args.PutMsgDisabled("Test_SelectStmtXML");
    }


    //
    if (select_xml_stmt_tc) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Unicode_Simple,
                                DBAPIInstance);
        tc->depends_on(tc_init);
        tc->depends_on(select_xml_stmt_tc);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Unicode_Simple");
    }

    // ctlib/ftds64 will work in case of protocol version 12.5 only
    // ftds + Sybase and dblib won't work because of the early
    // protocol versions.
    if ((((args.GetDriverName() == ftds8_driver
            && (args.GetServerType() == CTestArguments::eMsSql
                || args.GetServerType() == CTestArguments::eMsSql2005))
                || args.IsODBCBased()
            // !!! This driver won't work with Sybase because it supports
            // CS_VERSION_110 only. !!!
            // || args.GetDriverName() == ftds_dblib_driver
            ))
            // args.GetDriverName() == ctlib_driver ||
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Insert,
                                    DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Insert");
    }

    if (!(args.GetDriverName() == ftds8_driver
          && args.GetServerType() == CTestArguments::eSybase)
        && (args.GetDriverName() != dblib_driver
          || args.GetServerType() == CTestArguments::eSybase)
        && args.GetDriverName() != ftds_dblib_driver
        ) 
    {

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

    } else {
        args.PutMsgDisabled("Test_Procedure");
    }


    if (args.GetDriverName() != dblib_driver
        && !(args.GetDriverName() == ctlib_driver && os_type == eOsSolaris && !sybase_client_v125)
        && !(args.GetDriverName() == ftds_dblib_driver && args.GetServerType() == CTestArguments::eSybase)
        && !(args.GetDriverName() == ftds8_driver && args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant2, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Variant2");
    }


    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetTotalColumns,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);


    if (args.GetDriverName() != ftds_dblib_driver // 03/13/08
        && args.GetDriverName() != dblib_driver // 03/13/08
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure2,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Procedure2");
    }

    if (!args.IsODBCBased() // 04/04/08 
        && !(args.GetDriverName() == ftds_dblib_driver
            && args.GetServerType() == CTestArguments::eSybase) // 03/27/08
        && !(args.GetDriverName() == dblib_driver
            && args.GetServerType() == CTestArguments::eMsSql
            && os_type == eOsSolaris) // 04/01/08
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure3,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if ( (args.GetDriverName() == ftds8_driver
          || args.GetDriverName() == ftds_dblib_driver
          || args.GetDriverName() == ftds64_driver
          // || args.GetDriverName() == ftds_odbc_driver  // 03/25/08 // This is a big problem ....
          ) &&
         (args.GetServerType() == CTestArguments::eMsSql
          || args.GetServerType() == CTestArguments::eMsSql2005) ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UNIQUE, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_UNIQUE");
    }

    if (args.IsBCPAvailable()
        && args.GetDriverName() != ftds_dblib_driver
        && (args.GetDriverName() != dblib_driver
             ||  args.GetServerType() == CTestArguments::eSybase)
        ) 
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTimeBCP,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_DateTimeBCP");
    }

    // !!! There are still problems ...
    if (args.IsBCPAvailable()) 
    {
        if (!(args.GetDriverName() == ftds_dblib_driver
                 &&  args.GetServerType() == CTestArguments::eSybase)
            && !(args.GetDriverName() == dblib_driver
                 &&  args.GetServerType() != CTestArguments::eSybase)
           )
        {
            if ( !( args.GetTestConfiguration() == CTestArguments::eWithoutExceptions
                    && args.GetDriverName() == dblib_driver)
                    ) {
                tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Overflow,
                        DBAPIInstance);
                tc->depends_on(tc_init);
                add(tc);
            }
        } else {
            args.PutMsgDisabled("Test_Bulk_Overflow");
        }
    }

    // The only problem with this test is that the LINK_OS server is not
    // available from time to time.
    if(args.GetServerType() == CTestArguments::eSybase)
    {
        if (args.GetDriverName() != ftds8_driver
            && args.GetDriverName() != ftds_dblib_driver
           ) 
        {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Iskhakov,
                    DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        } else {
            args.PutMsgDisabled("Test_Iskhakov");
        }
    }


    if ( args.GetDriverName() != ftds_odbc_driver  // 03/25/08 // Strange ....
         && !(args.GetDriverName() == ftds8_driver
              && args.GetServerType() == CTestArguments::eSybase)
         && !(args.GetDriverName() == dblib_driver
              && args.GetServerType() == CTestArguments::eSybase)
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTime,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_DateTime");
    }

    // Valid for all drivers ...
    if (args.GetDriverName() != ftds_dblib_driver
        && args.GetDriverName() != dblib_driver
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Decimal,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

    } else {
        args.PutMsgDisabled("Test_Decimal");
    }


    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SetLogStream,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    if (!(((args.GetDriverName() == ftds8_driver
                || args.GetDriverName() == ftds_dblib_driver
              )
           && args.GetServerType() == CTestArguments::eSybase) // Something is wrong ...
          )
         && args.GetDriverName() != dblib_driver
        )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Identity,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    } else {
        args.PutMsgDisabled("Test_Identity");
    }


    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler_LT,
                                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    if (args.GetTestConfiguration() != CTestArguments::eFast) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_N_Connections,
                DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

//     if (args.GetServerType() == CTestArguments::eMsSql
//         && args.GetDriverName() != odbc_driver // Doesn't work ...
//         // && args.GetDriverName() != ftds_odbc_driver
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
        args.PutMsgDisabled("Test_HasMoreResults");
    }


    // DBLIB has no cancel command, so this test doesn't work with DBLIB API
    if (args.IsBCPAvailable()
        &&  args.GetDriverName() != dblib_driver  // dblib driver series have no bcp cancellation
        &&  args.GetDriverName() != ftds_dblib_driver
        &&  args.GetDriverName() != ctlib_driver  // Cancel is not working in Sybase ctlib driver
        )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BCP_Cancel,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (args.GetServerType() != CTestArguments::eSybase
        &&  args.GetDriverName() != odbc_driver
        &&  args.GetDriverName() != dblib_driver
        &&  args.GetDriverName() != ftds_dblib_driver
       )
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NTEXT,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }
/*
    It's not supposed to be included in DBAPI unit tests.
    It's just example of code that will force replication of updated blob.

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB_Replication,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);
*/
/*
    This is not supposed to be included in DBAPI unit tests.
    This is just example of heavy-load test.

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Heavy_Load,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);
*/

    cout << args.GetNumOfDisabledTests() << " tests are disabled..." << endl;
}

CDBAPITestSuite::~CDBAPITestSuite(void)
{
}


////////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(int argc, char * argv[])
: m_Arguments(argc, argv)
, m_TestConfiguration(eWithExceptions)
, m_NumOfDisabled(0)
, m_ReportDisabled(false)
, m_ReportExpected(false)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    // Specify USAGE context
    arg_desc->SetUsageContext(m_Arguments.GetProgramBasename(),
                              "dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MSDEV1"
#define DEF_DRIVER    ftds_driver
#define ALL_DRIVERS   ctlib_driver, dblib_driver, ftds64_driver, odbc_driver, \
                      ftds_dblib_driver, ftds_odbc_driver, ftds8_driver

#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "SCHUMANN"
#define DEF_DRIVER    ctlib_driver
#define ALL_DRIVERS   ctlib_driver, dblib_driver, ftds64_driver, ftds_dblib_driver, \
                      ftds_odbc_driver, ftds8_driver
#else
#define DEF_SERVER    "MSDEV1"
#define DEF_DRIVER    ftds_driver
#define ALL_DRIVERS   ftds64_driver, ftds_dblib_driver, ftds_odbc_driver, \
                      ftds8_driver
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

    arg_desc->AddDefaultKey("report_disabled", "report_disabled",
                            "Report disabled tests",
                            CArgDescriptions::eBoolean,
                            "false");

    arg_desc->AddDefaultKey("report_expected", "report_expected",
                            "Report expected tests",
                            CArgDescriptions::eBoolean,
                            "false");

    arg_desc->AddDefaultKey("conf", "configuration",
                            "Configuration for testing",
                            CArgDescriptions::eString, "with-exceptions");
    arg_desc->SetConstraint("conf", &(*new CArgAllow_Strings,
                            "with-exceptions", "without-exceptions", "fast"));

    auto_ptr<CArgs> args_ptr(arg_desc->CreateArgs(m_Arguments));
    const CArgs& args = *args_ptr;


    // Get command-line arguments ...
    m_DriverName    = args["d"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();
    m_ReportDisabled = args["report_disabled"].AsBoolean();

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

    if (args["conf"].AsString() == "with-exceptions") {
        m_TestConfiguration = CTestArguments::eWithExceptions;
    } else if (args["conf"].AsString() == "without-exceptions") {
        m_TestConfiguration = CTestArguments::eWithoutExceptions;
    } else if (args["conf"].AsString() == "fast") {
        m_TestConfiguration = CTestArguments::eFast;
    }

    SetDatabaseParameters();
}


CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    if ( NStr::CompareNocase(GetServerName(), "STRAUSS") == 0
         || NStr::CompareNocase(GetServerName(), "MOZART") == 0
         || NStr::CompareNocase(GetServerName(), "OBERON") == 0
         || NStr::CompareNocase(GetServerName(), "TAPER") == 0
         || NStr::CompareNocase(GetServerName(), "THALBERG") == 0
         || NStr::CompareNocase(GetServerName(), "LINK_OS") == 0
         || NStr::CompareNocase(GetServerName(), "SCHUMANN") == 0
         || NStr::CompareNocase(GetServerName(), "BARTOK") == 0
         || NStr::CompareNocase(GetServerName(), "SYB_TEST") == 0
         || NStr::CompareNocase(GetServerName(), 0, 9, "DBAPI_DEV") == 0
         ) {
        return eSybase;
    } else if ( NStr::CompareNocase(GetServerName(), 0, 7, "MSSQL67") == 0
                || NStr::CompareNocase(GetServerName(), 0, 7, "MSSQL79") == 0
                || NStr::CompareNocase(GetServerName(), 0, 7, "MSSQL82") == 0
                ) {
        return eMsSql2005;
    } else if (NStr::CompareNocase(GetServerName(), 0, 6, "MS_DEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 5, "MSSQL") == 0
                || NStr::CompareNocase(GetServerName(), 0, 5, "MSDEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 7, "OAMSDEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 6, "QMSSQL") == 0
                || NStr::CompareNocase(GetServerName(), "MS_TEST") == 0
                ) {
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
        (GetDriverName() == dblib_driver || GetDriverName() == ctlib_driver)
        ) {
        // Solaris Intel native Sybase drivers ...
        // There is no apropriate client
        return false;
    } else if ( GetDriverName() == ftds_odbc_driver
         || (GetDriverName() == ftds8_driver
             && GetServerType() == CTestArguments::eSybase)
         ) {
        return false;
    }

    return true;
}

bool
CTestArguments::IsODBCBased(void) const
{
    return GetDriverName() == odbc_driver ||
           GetDriverName() == ftds_odbc_driver;
}

bool CTestArguments::DriverAllowsMultipleContexts(void) const
{
    return GetDriverName() == ctlib_driver
        || GetDriverName() == ftds64_driver
        || IsODBCBased();
}

void
CTestArguments::SetDatabaseParameters(void)
{
    if ( m_TDSVersion.empty() ) {
        if ( GetDriverName() == ctlib_driver ) {
            // m_DatabaseParameters["version"] = "125";
            // Set max_connect to let open more than 30 connections with ctlib.
            // m_DatabaseParameters["max_connect"] = "100";
        } else if ( GetDriverName() == dblib_driver  &&
                GetServerType() == eSybase ) {
            // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
            // BcpIn to it using protocol version other than "100".
            m_DatabaseParameters["version"] = "100";
        } else if (GetDriverName() == ftds8_driver) {
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
        } else if ( GetDriverName() == ftds_dblib_driver &&
                GetServerType() == eSybase ) {
            // ftds8 works with Sybase databases using protocol v42 only ...
            m_DatabaseParameters["version"] = "42";
        } else if (GetDriverName() == ftds_odbc_driver) {
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
        }
        /* These parameters settings are commented out on purpose.
         * ftds64 driver is supposed to autodetect TDS version.
        } else if (GetDriverName() == ftds64_driver) {
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
        */
    } else {
        m_DatabaseParameters["version"] = m_TDSVersion;
    }

    if ( (GetDriverName() == ftds8_driver ||
                GetDriverName() == ftds64_driver ||
                // GetDriverName() == odbc_driver ||
                // GetDriverName() == ftds_odbc_driver ||
                GetDriverName() == ftds_dblib_driver)
            && (GetServerType() == eMsSql || GetServerType() == eMsSql2005)) {
        m_DatabaseParameters["client_charset"] = "UTF-8";
    }

    if (!m_GatewayHost.empty()) {
        m_DatabaseParameters["host_name"] = m_GatewayHost;
        m_DatabaseParameters["port_num"] = m_GatewayPort;
        m_DatabaseParameters["driver_name"] = GetDriverName();
    }
}

void CTestArguments::PutMsgDisabled(const char* msg) const
{
    ++m_NumOfDisabled;

    if (m_ReportDisabled) {
        LOG_POST(Warning << "- " << msg << " is disabled !!!");
    }
}

void CTestArguments::PutMsgExpected(const char* msg, const char* replacement) const
{
    if (m_ReportExpected) {
        LOG_POST(Warning << "? " << msg << " is expected instead of " << replacement);
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

    // Disable old log format ...
    // ncbi::GetDiagContext().SetOldPostFormat(false);

    // Using old log format ...
    // Show time (no msec.) ...
    ncbi::SetDiagPostFlag(ncbi::eDPF_DateTime);
    ncbi::CONNECT_Init();

    test_suite* test = BOOST_TEST_SUITE("DBAPI Unit Test.");

    test->add(new ncbi::CDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test;
}

