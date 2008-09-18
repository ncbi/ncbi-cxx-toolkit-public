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
#include <dbapi/driver/impl/dbapi_impl_context.hpp>

#include "dbapi_unit_test.hpp"


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
CDBSetConnParams::CDBSetConnParams(
        const string& server_name,
        const string& user_name,
        const string& password,
        Uint4 tds_version,
        const CDBConnParams& other)
: CDBConnParamsDelegate(other)
, m_ProtocolVersion(tds_version)
, m_ServerName(server_name)
, m_UserName(user_name)
, m_Password(password)
{
}

CDBSetConnParams::~CDBSetConnParams(void)
{
}

Uint4 
CDBSetConnParams::GetProtocolVersion(void) const
{
    return m_ProtocolVersion;
}

string 
CDBSetConnParams::GetServerName(void) const
{
    return m_ServerName;
}

string 
CDBSetConnParams::GetUserName(void) const
{
    return m_UserName;
}

string 
CDBSetConnParams::GetPassword(void) const
{
    return m_Password;
}


////////////////////////////////////////////////////////////////////////////////
const char* ftds8_driver = "ftds8";
const char* ftds64_driver = "ftds";
const char* ftds_driver = ftds64_driver;

const char* ftds_odbc_driver = "ftds_odbc";
const char* ftds_dblib_driver = "ftds_dblib";

const char* odbc_driver = "odbc";
const char* ctlib_driver = "ctlib";
const char* dblib_driver = "dblib";

////////////////////////////////////////////////////////////////////////////////
const char* msg_record_expected = "Record expected";

/////////////////////////////////////////////////////////////////////////////
const string&
GetTableName(void)
{
    static const string s_TableName = "#dbapi_unit_table";
    return s_TableName;
}

const CTestArguments&
GetArgs(void)
{
    static CRef<CTestArguments> s_Args(new CTestArguments());
    return *s_Args;
}

int
GetMaxVarcharSize(void)
{
    static int s_max_varchar_size = 0;

    if (s_max_varchar_size == 0) {
        if ( GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
            s_max_varchar_size = 8000;
        } else {
            // Sybase
            s_max_varchar_size = 1900;
        }
    }

    return s_max_varchar_size;
}


///////////////////////////////////////////////////////////////////////////////
static IDataSource* s_DS = NULL;

IDataSource&
GetDS(void)
{
    _ASSERT(s_DS);
    return *s_DS;
}

bool CommonInit(void)
{
    // Using old log format ...
    // Show time (no msec.) ...
    SetDiagPostFlag(eDPF_DateTime);
    CONNECT_Init();

    DBLB_INSTALL_DEFAULT();

#ifndef NCBI_DLL_SUPPORT

#ifdef HAVE_LIBSYBASE
    DBAPI_RegisterDriver_CTLIB();
#endif

#ifdef HAVE_LIBSQLITE3
    DBAPI_RegisterDriver_SQLITE3();
#endif

#ifdef HAVE_ODBC
    DBAPI_RegisterDriver_ODBC();
#endif

    DBAPI_RegisterDriver_FTDS();
    DBAPI_RegisterDriver_FTDS_ODBC();

#else
    CPluginManager_DllResolver::EnableGlobally(true);
#endif // NCBI_DLL_SUPPORT

    if (false) {
        // Two calls below will cause problems with the Sybase 12.5.1 client
        // (No problems with the Sybase 12.5.0 client)
        IDataSource* ds = GetDM().MakeDs(GetArgs().GetConnParams());
        GetDM().DestroyDs(ds);
    }

    try {
        s_DS = GetDM().MakeDs(GetArgs().GetConnParams());
    }
    catch (CException& ex) {
        LOG_POST(Warning << "Error loading database driver: " << ex.what());
        NcbiTestSetGlobalDisabled();
        return false;
    }

    I_DriverContext* drv_context = GetDS().GetDriverContext();
    drv_context->SetLoginTimeout(4);
    drv_context->SetTimeout(4);

    if (GetArgs().GetTestConfiguration() != CTestArguments::eWithoutExceptions) {
        if (GetArgs().IsODBCBased()) {
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

    return true;
}

void
CommonFini(void)
{
    if (s_DS) {
        GetDM().DestroyDs(s_DS);
    }
}

///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////

string GetSybaseClientVersion(void)
{
    string sybase_version;

#if defined(NCBI_OS_MSWIN)
    sybase_version = "12.5";
#else
    impl::CDriverContext::ResetEnvSybase();

    CNcbiEnvironment env;
    sybase_version = env.Get("SYBASE");

	if (!sybase_version.empty()) {
		CDirEntry dir_entry(sybase_version);
		dir_entry.DereferenceLink();
		sybase_version = dir_entry.GetPath();

		sybase_version = sybase_version.substr(
				sybase_version.find_last_of('/') + 1
				);
	} else {
		sybase_version = "0.0";
	}
#endif

    return sybase_version;
}


NCBITEST_INIT_VARIABLES(parser)
{
    ////////////////////////
    // Sybase ...
	{
		double syb_client_ver = 0.0;
		const string syb_client_ver_str = GetSybaseClientVersion();

		if (!syb_client_ver_str.empty()) {
			try {
				syb_client_ver = NStr::StringToDouble(syb_client_ver_str.substr(0, 4));
			} catch (const CStringException&) {
				// Conversion error
			}
		}

		parser->AddSymbol("SYBASE_ClientVersion", syb_client_ver);

	}

    ///////////////////////
    // Configuration-related ...
#ifdef HAVE_LIBCONNEXT
    parser->AddSymbol("HAVE_LibConnExt", CRYPT_Version(-1) != -1);
#else
    parser->AddSymbol("HAVE_LibConnExt", false);
#endif

#ifdef HAVE_LIBSYBASE
    parser->AddSymbol("HAVE_Sybase", true);
#else
    parser->AddSymbol("HAVE_Sybase", false);
#endif

#ifdef HAVE_ODBC
    parser->AddSymbol("HAVE_ODBC", true);
#else
    parser->AddSymbol("HAVE_ODBC", false);
#endif

#ifdef HAVE_MYSQL
    parser->AddSymbol("HAVE_MYSQL", true);
#else
    parser->AddSymbol("HAVE_MYSQL", false);
#endif

#ifdef HAVE_LIBSQLITE3
    parser->AddSymbol("HAVE_SQLITE3", true);
#else
    parser->AddSymbol("HAVE_SQLITE3", false);
#endif

    parser->AddSymbol("DRIVER_AllowsMultipleContexts", GetArgs().DriverAllowsMultipleContexts());

    parser->AddSymbol("SERVER_MySQL", GetArgs().GetServerType() == CDBConnParams::eMySQL);
    parser->AddSymbol("SERVER_SybaseOS", GetArgs().GetServerType() == CDBConnParams::eSybaseOpenServer);
    parser->AddSymbol("SERVER_SybaseSQL", GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer);
    parser->AddSymbol("SERVER_MicrosoftSQL", GetArgs().GetServerType() == CDBConnParams::eMSSqlServer);
    parser->AddSymbol("SERVER_SQLite", GetArgs().GetServerType() == CDBConnParams::eSqlite);

    parser->AddSymbol("DRIVER_ftds", GetArgs().GetDriverName() == ftds_driver);
    parser->AddSymbol("DRIVER_ftds8", GetArgs().GetDriverName() == ftds8_driver);
    parser->AddSymbol("DRIVER_ftds64", GetArgs().GetDriverName() == ftds64_driver);
    parser->AddSymbol("DRIVER_ftds_odbc", GetArgs().GetDriverName() == ftds_odbc_driver);
    parser->AddSymbol("DRIVER_ftds_dblib", GetArgs().GetDriverName() == ftds_dblib_driver);
    parser->AddSymbol("DRIVER_odbc", GetArgs().GetDriverName() == odbc_driver);
    parser->AddSymbol("DRIVER_ctlib", GetArgs().GetDriverName() == ctlib_driver);
    parser->AddSymbol("DRIVER_dblib", GetArgs().GetDriverName() == dblib_driver);
    parser->AddSymbol("DRIVER_mysql", GetArgs().GetDriverName() == "mysql");

    parser->AddSymbol("DRIVER_IsBcpAvailable", GetArgs().IsBCPAvailable());
    parser->AddSymbol("DRIVER_IsOdbcBased", GetArgs().GetDriverName() == ftds_odbc_driver ||
            GetArgs().GetDriverName() == odbc_driver);
}


////////////////////////////////////////////////////////////////////////////////
CUnitTestParams::CUnitTestParams(const CDBConnParams& other)
: CDBConnParamsDelegate(other)
{
}

CUnitTestParams::~CUnitTestParams(void)
{
}

string CUnitTestParams::GetServerName(void) const
{
    const string server_name = CDBConnParamsDelegate::GetServerName();

    if (NStr::CompareNocase(server_name, "MsSql") == 0) {
#ifdef HAVE_LIBCONNEXT
        return "MS_TEST";
#else
        return "MSDEV1";
#endif
    } else if (NStr::CompareNocase(server_name, "Sybase") == 0) {
#ifdef HAVE_LIBCONNEXT
        return "SYB_TEST";
#else
        return "CLEMENTI";
#endif
    }

    return server_name;
}

CDBConnParams::EServerType 
CUnitTestParams::GetServerType(void) const
{
    const string server_name = GetThis().GetServerName();
    const string driver_name = GetThis().GetDriverName();

    if (driver_name == "dblib" 
        || driver_name == "ftds_dblib"
        || driver_name == "ftds8"
        ) {
        if (NStr::CompareNocase(server_name, 0, 8, "CLEMENTI") == 0
            || NStr::CompareNocase(server_name, 0, 8, "SYB_TEST") == 0
            ) 
        {
            return eSybaseSQLServer;
        }
    }

    return CDBConnParamsDelegate::GetServerType();
}

////////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(void)
: m_TestConfiguration(eWithExceptions)
, m_NumOfDisabled(0)
, m_ReportDisabled(false)
, m_ReportExpected(false)
, m_ConnParams(m_ParamBase)
, m_CPPParams(m_ParamBase2)
, m_ConnParams2(m_CPPParams)
// , m_CPPParams(m_ParamBase)
// , m_ConnParams(m_CPPParams)
{
    const CNcbiApplication* app = CNcbiApplication::Instance();

    if (!app) {
        return;
    }

    const CArgs& args = app->GetArgs();

    // Get command-line arguments ...
    m_ParamBase.SetServerName(args["S"].AsString());
    m_ParamBase2.SetServerName(args["S"].AsString());
    m_ParamBase.SetUserName(args["U"].AsString());
    m_ParamBase2.SetUserName(args["U"].AsString());
    m_ParamBase.SetPassword(args["P"].AsString());
    m_ParamBase2.SetPassword(args["P"].AsString());

    if (args["d"].HasValue()) {
        m_ParamBase.SetDriverName(args["d"].AsString());
        m_ParamBase2.SetDriverName(args["d"].AsString());
    }

    if (args["D"].HasValue()) {
        m_ParamBase.SetDatabaseName(args["D"].AsString());
        m_ParamBase2.SetDatabaseName(args["D"].AsString());
    }

    if ( args["v"].HasValue() ) {
        m_ParamBase.SetProtocolVersion(args["v"].AsInteger());
        m_ParamBase2.SetProtocolVersion(args["v"].AsInteger());
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
             && GetServerType() == CDBConnParams::eSybaseSQLServer)
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
    if ((GetDriverName() == ftds8_driver ||
         GetDriverName() == ftds64_driver ||
         // GetDriverName() == odbc_driver ||
         // GetDriverName() == ftds_odbc_driver ||
         GetDriverName() == ftds_dblib_driver) && GetServerType() == CDBConnParams::eMSSqlServer
        ) 
    {
        m_ParamBase.SetEncoding(eEncoding_UTF8);
        m_ParamBase2.SetEncoding(eEncoding_UTF8);
    }

    if (!m_GatewayHost.empty()) {
        m_ParamBase.SetParam("host_name", m_GatewayHost);
        m_ParamBase2.SetParam("host_name", m_GatewayHost);
        m_ParamBase.SetParam("port_num", m_GatewayPort);
        m_ParamBase2.SetParam("port_num", m_GatewayPort);
        m_ParamBase.SetParam("driver_name", GetDriverName());
        m_ParamBase2.SetParam("driver_name", GetDriverName());
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

////////////////////////////////////////////////////////////////////////////////
NCBITEST_INIT_CMDLINE(arg_desc)
{
// Describe the expected command-line arguments
#define ALL_DRIVERS   ctlib_driver, dblib_driver, ftds64_driver, odbc_driver, \
                    ftds_dblib_driver, ftds_odbc_driver, ftds8_driver

#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MSSQL"
//#define DEF_DRIVER    ftds_driver
//#define ALL_DRIVERS   ctlib_driver, dblib_driver, ftds64_driver, odbc_driver,
//                    ftds_dblib_driver, ftds_odbc_driver, ftds8_driver

#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "Sybase"
//#define DEF_DRIVER    ctlib_driver
//#define ALL_DRIVERS   ctlib_driver, dblib_driver, ftds64_driver, ftds_dblib_driver,
//                    ftds_odbc_driver, ftds8_driver
#else
#define DEF_SERVER    "MSSQL"
//#define DEF_DRIVER    ftds_driver
//#define ALL_DRIVERS   ftds64_driver, ftds_dblib_driver, ftds_odbc_driver,
//                    ftds8_driver
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddOptionalKey("d", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString);
    arg_desc->SetConstraint("d", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddOptionalKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString);

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

    arg_desc->AddDefaultKey("conf", "configuration",
                            "Configuration for testing",
                            CArgDescriptions::eString, "with-exceptions");
    arg_desc->SetConstraint("conf", &(*new CArgAllow_Strings,
                            "with-exceptions", "without-exceptions", "fast"));
}



END_NCBI_SCOPE
