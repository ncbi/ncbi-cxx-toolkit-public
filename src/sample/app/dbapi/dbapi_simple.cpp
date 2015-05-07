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
 * Authors: David McElhany, Pavel Ivanov, Michael Kholodov
 *
 * File Description:
 *
 *  This program demonstrates connecting to a database and performing several
 *  simple operations, in order of preference:
 *      -  executing a stored procedure
 *      -  executing a static SQL statement
 *      -  executing a parameterized SQL statement
 *      -  executing a dynamic SQL statement
 *
 *  Here's how to choose which form of SQL statement creation to use:
 *   1. If stored procuedures can be used, then use them.  This increases both
 *      security and performance.  Plus, this practice could facilitate testing
 *      and documentation.
 *   2. Otherwise, if the SQL statement does not require construction, then use
 *      static SQL.
 *   3. Otherwise, if parameterized SQL can be used, then use it.
 *   4. Otherwise, as a last resort, use dynamic SQL.  NOTE: If user-supplied
 *      data is used to construct the statement, then you MUST sanitize the
 *      user-supplied data.
 *
 *  For educational purposes, this program also shows how constructing SQL
 *  statements from unsanitized user input can result in SQL injection - and
 *  therefore why user input must always be sanitized when using dynamic SQL.
 *
 *  The program also demonstrates using a custom error handler.
 *
 *  The program is written assuming static linkage and a connection to
 *  SQL Server.
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>

// "new" DBAPI
#include <dbapi/dbapi.hpp>

// "old" DBAPI
#include <dbapi/driver/dbapi_conn_factory.hpp>  // CTrivialConnValidator
#include <dbapi/driver/dbapi_svc_mapper.hpp>    // DBLB_INSTALL_DEFAULT
#include <dbapi/driver/drivers.hpp>             // DBAPI_RegisterDriver_FTDS
#include <dbapi/driver/exception.hpp>           // CDB_UserHandler

#define NCBI_USE_ERRCODE_X   DbapiSimpleSproc

BEGIN_NCBI_SCOPE
NCBI_DEFINE_ERRCODE_X(DbapiSimpleSproc, 2500, 1);
END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CDbapiSimpleApp::

class CDbapiSimpleApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void ParseArgs(void);
    void SetupDb(void);

    void DemoStoredProc(void);
    void DemoParamerizedSql(void);
    void DemoStaticSql(void);
    void DemoDynamicSql(void);

    void DemoSqlInjection(void);

    void RetrieveData(IStatement* stmt);

    // Command-line options
    string  m_UserString1;
    string  m_UserString2;
    string  m_UserString3;
    string  m_Service;
    string  m_DbName;
    string  m_User;
    string  m_Password;

    // Application data
    IDataSource*    m_Ds;
    IConnection*    m_Conn;
    CNcbiOfstream   m_Logstream;
    string          m_LogFileName;

    CDB_UserHandler_Stream  m_ExHandler;
};


/////////////////////////////////////////////////////////////////////////////
//  CErrHandler - custom error handler

class CErrHandler : public CDB_UserHandler
{
public:
    CErrHandler(const string& type, CNcbiOfstream* log_stream)
        : m_Type(type), m_LogStream(log_stream)
    {
    }

    virtual bool HandleIt(CDB_Exception* ex)
    {
        // Use the default handler when no exception info is available.
        if (!ex) {
            return false;
        }

        // Log all exceptions coming from the server.
        *m_LogStream << m_Type << " " << ex->SeverityString() << ": ";
        *m_LogStream << ex->ReportThis() << NcbiEndl;

        // NOTE: DBAPI converts server exceptions into C++ exceptions with
        // severity eDiag_Error, and less severe server errors into C++
        // exceptions with severity eDiag_Info.

        // Only throw for the more severe server exceptions.
        if (ex->GetSeverity() >= eDiag_Error) {
            throw *ex; // Handle in the main client code.
        }

        // No need for further processing by other handlers.
        return true;
    }

private:
    string          m_Type;
    CNcbiOstream*   m_LogStream;
};


/////////////////////////////////////////////////////////////////////////////
//  Init - setup command-line arguments

void CDbapiSimpleApp::Init(void)
{
    CArgDescriptions* argdesc = new CArgDescriptions();
    argdesc->SetUsageContext(GetArguments().GetProgramBasename(),
                             "DBAPI simple operations demo");


    argdesc->AddDefaultKey("user_string1", "UserString1", "A user-supplied "
                           "string to be used in one of the demonstrations "
                           "(could contain a SQL injection attempt)",
                           CArgDescriptions::eString, "");

    argdesc->AddDefaultKey("user_string2", "UserString2", "Another "
                           "user-supplied string to be used in one of the "
                           "demonstrations (could contain a SQL injection "
                           "attempt)",
                           CArgDescriptions::eString, "");

    argdesc->AddDefaultKey("user_string3", "UserString3", "Yet another "
                           "user-supplied string to be used in one of the "
                           "demonstrations (could contain a SQL injection "
                           "attempt)",
                           CArgDescriptions::eString, "");

    argdesc->AddPositional("service", "Service name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("db_name", "Database name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("user", "User name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("password", "User password",
                           CArgDescriptions::eString);


    SetupArgDescriptions(argdesc);
}


/////////////////////////////////////////////////////////////////////////////
//  Run - i.e. parse command-line arguments and demo simple operations.


int CDbapiSimpleApp::Run(void)
{
    // Get user parameters and connect to database.
    ParseArgs();
    SetupDb();

    // Demo several simple operations - in order of preference.
    DemoStoredProc();
    DemoStaticSql();
    DemoParamerizedSql();
    DemoDynamicSql();

    // Show how SQL injection can happen.
    DemoSqlInjection();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Exit - perform any needed cleanup

void CDbapiSimpleApp::Exit(void)
{
    CDriverManager& dm(CDriverManager::GetInstance());
    dm.DestroyDs(m_Ds);

    m_Logstream.flush();
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  ParseArgs - parse the command-line arguments.

void CDbapiSimpleApp::ParseArgs(void)
{
    const CArgs& args = GetArgs();

    m_UserString1 = args["user_string1"].AsString();
    m_UserString2 = args["user_string2"].AsString();
    m_UserString3 = args["user_string3"].AsString();
    m_Service = args["service"].AsString();
    m_DbName = args["db_name"].AsString();
    m_User = args["user"].AsString();
    m_Password = args["password"].AsString();

    if (args["logfile"].HasValue()) {
        m_LogFileName = args["logfile"].AsString();
    } else {
        m_LogFileName = GetAppName() + ".log";
    }
}


/////////////////////////////////////////////////////////////////////////////
//  SetupDb - connect to the database.
//
//  NOTE: Please be aware that your choice of options (or use of default
//  options) may cause/prevent failures or affect the execution plan.
//
//  There are many ways for a user to control DBAPI, including the following
//  (not in any particular order):
//
//  1.  Passing a validator, server, driver, username, password, and database
//      name to IConnection::ConnectValidated().
//
//  2.  Setting protocol version, encoding, server type, host, port, server,
//      driver, username, password, database name, and / or any other custom
//      key/value pairs in a CDBConnParams-derived object and passing it to
//      IConnection::Connect().
//
//  3.  Passing a URI containing name/value pairs to CDBUriConnParams and
//      passing that to IConnection::Connect().
//
//  4.  Using the I_DriverContext methods SetLoginTimeout(), SetTimeout(),
//      SetHostName(), etc.
//
//  5.  Using the IConnection methods SetMode(), ForceSingle(), SetTimeout(),
//      and SetCancelTimeout().
//
//  6.  Passing a connection pool name via I_DriverContext::Connect().
//
//  7.  Using CTLibContext::SetClientCharset().
//
//  8.  Writing and using your own custom connection validator, derived from
//      IConnValidator.  Validators can be chained using CConnValidatorCoR.
//
//  9.  Setting NCBI configuration parameters, such as FTDS_TDS_VERSION,
//      NCBI_CONFIG__DB_CONNECTION_FACTORY__MAX_CONN_ATTEMPTS, and others
//      (see http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_libconfig/).
//
//  10. Setting environment variables, such as FREEBCP_DATEFMT, TDSQUERY,
//      DATA_TABLE, and others (to see how these are used, see
//      http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lxr/ident?i=_bcp_exec_out).
//
//  11. Setting parameters in an interfaces file.
//
//  For more information on setting connection options, please see the NCBI C++
//  Toolkit book chapter on DBAPI:
//      http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_dbapi/

void CDbapiSimpleApp::SetupDb(void)
{
    // Use load balancing if available.
    DBLB_INSTALL_DEFAULT();

    // Explicitly register a driver.
    DBAPI_RegisterDriver_FTDS();

    try {
        CDriverManager& dm(CDriverManager::GetInstance());

        // Create a data source - the root object for all other
        // objects in the library.
        m_Ds = dm.CreateDs("ftds");

        // Setup diagnostics.
        m_Logstream.open(m_LogFileName.c_str());
        m_Ds->SetLogStream(&m_Logstream);

        // Add a message handler for 'context-wide' error messages (not bound
        // to any particular connection); let DBAPI own it.
        I_DriverContext* drv_context = m_Ds->GetDriverContext();
        drv_context->PushCntxMsgHandler(
            new CErrHandler("General", &m_Logstream),
            eTakeOwnership);

        // Add a 'per-connection' message handler to the stack of default
        // handlers which are inherited by all newly created connections;
        // let DBAPI own it.
        drv_context->PushDefConnMsgHandler(
            new CErrHandler("Connection", &m_Logstream),
            eTakeOwnership);

        // Configure this context.
        drv_context->SetLoginTimeout(10);
        // default query timeout for client/server comm for all connections
        drv_context->SetTimeout(15);

        // Create connection.
        m_Conn = m_Ds->CreateConnection();
        if (m_Conn == NULL) {
            ERR_POST_X(1, Fatal << "Could not create connection.");
        }

        // Validate connection.  When using load balancing, this will interpret
        // the "server" name as a service, then use the load balancer to find
        // servers, then try in succession until a successful login to the
        // given database is successful.
        CTrivialConnValidator val(m_DbName);
        m_Conn->ConnectValidated(val, m_User, m_Password, m_Service, m_DbName);

    } catch (...) { // try/catch here just to facilitate debugging
        throw;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  DemoStoredProc - demonstrate executing a stored procedure.
//
//  See the File Description comment to determine if a stored procedure
//  should be used.
//
//  This demo uses a stored procedure that:
//  1.  Sets recommended settings.
//  2.  Selects some data per parameters.
//  3.  Assigns an output parameter.
//  4.  Generates a simple message, a warning, and an exception.
//
//  The server errors and exceptions will be caught and processed so that you
//  can see how the user handling works.  In case you don't have access to the
//  stored procedure, please see the companion file dbapi_simple_sproc.sql.
//
//  NOTE: It is recommended that you create your stored procedures using the
//  following settings because these settings are generally advisable
//  (and they cannot be changed at run-time):
//      SET ANSI_NULLS ON
//      SET QUOTED_IDENTIFIER ON
//
//  Also, executing stored procedures using the following settings is
//  recommended unless there is a specific reason to use other settings:
//      SET ANSI_PADDING ON
//      SET ANSI_WARNINGS ON
//      SET ARITHABORT ON
//      SET CONCAT_NULL_YIELDS_NULL ON
//      SET NUMERIC_ROUNDABORT OFF

void CDbapiSimpleApp::DemoStoredProc(void)
{
    try {
        // Pick a stored procedure.
        string proc_name("dbapi_simple_sproc");

        // Create an integer input parameter "@max_id", a float input
        // parameter "@max_fl", and an integer output parameter "@num_rows".
        auto_ptr<ICallableStatement> cstmt(m_Conn->GetCallableStatement(
            proc_name));
        cstmt->SetParam(CVariant(5), "@max_id");
        cstmt->SetParam(CVariant(5.1f), "@max_fl");
        cstmt->SetOutputParam(CVariant(eDB_Int), "@num_rows");

        // Execute the stored procedure.
        NcbiCout << "\nExecuting stored procedure \"" << proc_name << "\":"
            << NcbiEndl;
        cstmt->Execute();

        // Retrieve and display the data.
        RetrieveData(&*cstmt);

        // The stored procedure will return a status.
        NcbiCout << "\nStored procedure returned status: "
            << cstmt->GetReturnStatus() << NcbiEndl;
        string msgs = m_Ds->GetErrorInfo();
        if ( ! msgs.empty() ) {
            NcbiCout << "    Errors:" << NcbiEndl;
            NcbiCout << "        " << msgs << NcbiEndl;
        }
    } catch (...) {
        NcbiCout << "*** Caught exception; see logfile \"" << m_LogFileName
            << "\"." << NcbiEndl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  DemoStaticSql - demonstrate executing static SQL.
//
//  See the File Description comment to determine if static SQL
//  should be used.
//
//  While stored procedures are preferable to static SQL, there is no chance
//  of SQL injection with static SQL.  Certainly you can write destructive
//  static SQL yourself (e.g. DROP my_table), but that wouldn't be
//  SQL injection.

void CDbapiSimpleApp::DemoStaticSql(void)
{
    try {
        // Create a static SQL statement.
        string sql("SELECT [title] FROM [Journal]");

        // Execute the static SQL.
        NcbiCout << "\nExecuting static SQL \"" << sql << "\":" << NcbiEndl;
        auto_ptr<IStatement> stmt(m_Conn->CreateStatement());
        stmt->Execute(sql);

        // Retrieve the data.
        RetrieveData(&*stmt);
    } catch (...) {
        NcbiCout << "*** Caught exception; see logfile \"" << m_LogFileName
            << "\"." << NcbiEndl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  DemoParamerizedSql - demonstrate executing parameterized SQL.
//
//  See the File Description comment to determine if parameterized SQL
//  should be used.
//
//  This function allows users to enter _any_ string (possibly containing
//  SQL injection attempts) but doesn't require sanitizing the string.
//  That's because the string is passed as a parameter and SQL Server
//  guarantees that it is not susceptible to SQL injection when parameters are
//  used.

void CDbapiSimpleApp::DemoParamerizedSql(void)
{
    try {
        // Get user-supplied strings.
        const CArgs& args = GetArgs();
        string user_last   = args["user_string1"].AsString();
        string user_salary = args["user_string2"].AsString();
        string user_hire   = args["user_string3"].AsString();

        // Create a parameterized SQL statement.
        string sql(" SELECT [id], [last], [first], [salary], [hiredate]"
                   " FROM [Employee] WHERE [last] LIKE @last"
                   " AND [salary] > @salary"
                   " AND [hiredate] > @hire");

        // Assign parameters.
        auto_ptr<IStatement> cstmt(m_Conn->GetStatement());
        cstmt->SetParam(CVariant(user_last),   "@last");
        cstmt->SetParam(CVariant(user_salary), "@salary");
        cstmt->SetParam(CVariant(user_hire),   "@hire");

        // Execute the parameterized SQL.
        NcbiCout << "\nExecuting parameterized SQL \"" << sql << "\":"
            << NcbiEndl;
        cstmt->SendSql(sql);

        // Retrieve the data.
        RetrieveData(&*cstmt);
    } catch (...) {
        NcbiCout << "*** Caught exception; see logfile \"" << m_LogFileName
            << "\"." << NcbiEndl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  DemoDynamicSql - demonstrate executing dynamic SQL.
//
//  See the File Description comment to determine if dynamic SQL
//  should be used.
//
//  If you work at NCBI and you believe your application requires dynamic SQL,
//  please email cpp-core so that we'll be aware of any internal applications
//  that require dynamic SQL.
//
//  NOTE: If user-supplied data is used to construct the statement, then you
//      MUST sanitize the user-supplied data, otherwise your code will be
//      vulnerable to SQL injection attacks.
//
//  If you are inside NCBI and using SQL Server or Sybase, sanitizing requires
//  calling NStr::SQLEncode() for strings and round-tripping scalar types.
//  For other database systems, contact cpp-core.

void CDbapiSimpleApp::DemoDynamicSql(void)
{
    try {
        // Get user-supplied strings.
        const CArgs& args = GetArgs();
        string user_last   = args["user_string1"].AsString();
        string user_salary = args["user_string2"].AsString();
        string user_hire   = args["user_string3"].AsString();

        // Sanitize.
        // For strings, use SQLEncode().  For scalars, first convert to an
        // appropriate scalar type, then convert back to a string.
        user_last   = NStr::SQLEncode(CUtf8::AsUTF8(user_last,eEncoding_ISO8859_1));
        user_salary = NStr::UIntToString(NStr::StringToUInt(user_salary));
        user_hire   = "'" + CTime(user_hire).AsString() + "'";

        // Dynamically create a SQL statement.
        string sql(" SELECT [id], [last], [first], [salary], [hiredate]"
                   " FROM [Employee] WHERE [last] LIKE " + user_last +
                   " AND [salary] > " + user_salary +
                   " AND [hiredate] > " + user_hire);

        // Execute the dynamic SQL.
        NcbiCout << "\nExecuting dynamic SQL \"" << sql << "\":" << NcbiEndl;
        auto_ptr<IStatement> stmt(m_Conn->CreateStatement());
        stmt->Execute(sql);

        // Retrieve the data.
        RetrieveData(&*stmt);
    } catch (...) {
        NcbiCout << "*** Caught exception; see logfile \"" << m_LogFileName
            << "\"." << NcbiEndl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  DemoSqlInjection - demonstrate how SQL injection can happen.
//
//  This function is simply for educational purposes -- to show how code that
//  constructs a SQL statement from unsanitized user input is vulnerable to
//  SQL injection.

void CDbapiSimpleApp::DemoSqlInjection(void)
{
#if 0
    try {
        // Get user-supplied string.
        const CArgs& args = GetArgs();
        string user_input = args["user_string1"].AsString();

        // DO NOT DO THIS -- IT ALLOWS SQL INJECTION.
        // Dynamically create a SQL statement directly from unsanitized
        // user input.
        string sql(" SELECT [id], [last], [first], [salary], [hiredate]"
                   " FROM [Employee] WHERE [last] LIKE '" + user_input + "'");

        // If you did the above, then the 'sql' string could contain malicious
        // SQL.  For example, the user input:
        //      a' OR 1=1; DROP TABLE [Employee]; --
        // would result in the SQL statement:
        //      SELECT [id], [last], [first], [salary], [hiredate]
        //      FROM [Employee]
        //      WHERE [last] LIKE 'a' OR 1=1; DROP TABLE [emp]; --'
    } catch (...) {
        NcbiCout << "*** Caught exception; see logfile \"" << m_LogFileName
            << "\"." << NcbiEndl;
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
//  RetrieveData - retrieve data from the SQL server.

void CDbapiSimpleApp::RetrieveData(IStatement* stmt)
{
    while (stmt->HasMoreResults()) {
        // Use an auto_ptr to manage resultset lifetime.
        // NOTE: Use it with caution. When the wrapped parent object
        // goes out of scope, all child objects are destroyed
        // (which isn't an issue for this demo but could be for
        // other applications).
        auto_ptr<IResultSet> rs(stmt->GetResultSet());

        // Sometimes the results have no rows - and that's ok.
        if ( ! stmt->HasRows() ) {
            ERR_POST_X(1, Info << Note << "No rows.");
            continue;
        }

        switch (rs->GetResultType()) {

        case eDB_StatusResult:
            NcbiCout << "\nStatus results:" << NcbiEndl;
            while (rs->Next()) {
                NcbiCout << "    Status: " << rs->GetVariant(1).GetInt4()
                    << NcbiEndl;
            }
            break;

        case eDB_ParamResult:
            NcbiCout << "\nParameter results:" << NcbiEndl;
            while (rs->Next()) {
                NcbiCout << "    Parameter: "
                    << rs->GetVariant(1).GetInt4() << NcbiEndl;
            }
            break;

        case eDB_RowResult: {
            NcbiCout << "\nRow results:" << NcbiEndl;

            const IResultSetMetaData* rsMeta = rs->GetMetaData();

            // Print column headers.
            for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                NcbiCout << "    " << rsMeta->GetName(i);
            }
            NcbiCout << NcbiEndl;
            for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                NcbiCout << "    " << string(rsMeta->GetName(i).size(), '=');
            }
            NcbiCout << NcbiEndl;

            while (rs->Next()) {
                // Print a row of data.
                for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                    NcbiCout << "    " << rs->GetVariant(i).GetString();
                }
                NcbiCout << NcbiEndl;
            }
            NcbiCout << "    ---------------" << NcbiEndl;
            NcbiCout << "    Row count: " << stmt->GetRowCount()
                << NcbiEndl;
            break;
        }

        // These types aren't used in this demo, but might be in
        // your code.
        case eDB_ComputeResult:
        case eDB_CursorResult:
            ERR_POST_X(1, Warning << Note << "Unhandled results type:"
                << rs->GetResultType());
            break;

        // Any other type means this code is out-of-date.
        default:
            ERR_POST_X(1, Critical << "Unexpected results type:"
                << rs->GetResultType());
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CDbapiSimpleApp().AppMain(argc, argv);
}
