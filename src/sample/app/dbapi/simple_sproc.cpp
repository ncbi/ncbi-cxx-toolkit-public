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
 *  This program demonstrates connecting to a database, calling a stored
 *  procedure, and using a custom error handler.  The program is written
 *  assuming static linkage and a connection to SQL Server.
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

#include <map>

#define NCBI_USE_ERRCODE_X   DbapiSimpleQuery

BEGIN_NCBI_SCOPE
NCBI_DEFINE_ERRCODE_X(DbapiSimpleQuery, 2525, 1);
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

    void DemoProcExec(void);
    void ParseArgs(void);
    void SetupDb(void);

    // Command-line options
    string  m_Server;
    string  m_User;
    string  m_Password;

    // Application data
    IDataSource*    m_Ds;
    IConnection*    m_Conn;
    CNcbiOfstream   m_Logstream;
    string          m_LogFileName;

    CDB_UserHandler_Stream  m_ExHandler;

    static const string     sm_kDriver;
    static const string     sm_kDbName;
    static const string     sm_kSpName;
};
const string    CDbapiSimpleApp::sm_kDriver = "ftds";
const string    CDbapiSimpleApp::sm_kDbName = "DBAPI_Sample";
const string    CDbapiSimpleApp::sm_kSpName = "simple_sproc";


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
                             "DBAPI simple query demo");


    argdesc->AddPositional("server", "Server name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("user", "User name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("password", "User password",
                           CArgDescriptions::eString);


    SetupArgDescriptions(argdesc);
}


/////////////////////////////////////////////////////////////////////////////
//  Run - i.e. parse command-line arguments, demo executing a stored proc
//  and a parameterized query.


int CDbapiSimpleApp::Run(void)
{
    // Get user parameters and connect to database.
    ParseArgs();
    SetupDb();

    // Do something useful with the database.
    DemoProcExec();

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
//  DemoProcExec - demonstrate executing a stored proc.
//
//  This demo uses a stored proc that:
//  1.  Checks for recommended settings.
//  2.  Selects some data per parameters.
//  3.  Assigns an output parameter.
//  4.  Generates a simple message, a warning, and an exception.
//
//  The warning will be exception will be caught and processed so that you can see how the user handling works.  In case you
//  don't have access to the stored procedure, please see the companion
//  file simple_sproc.sql.
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

void CDbapiSimpleApp::DemoProcExec(void)
{
    try {
        // Get access to an SQL statement.
        auto_ptr<ICallableStatement> cstmt(m_Conn->GetCallableStatement(
            sm_kSpName));
        NcbiCout << "\n\n" << string(60, '=') << NcbiEndl;
        NcbiCout << "Executing stored procedure \"" << sm_kSpName << "\":"
            << NcbiEndl;

        // Create an integer input parameter "@max_id", a float input
        // parameter "@max_fl", and an integer output parameter "@outp".
        cstmt->SetParam(CVariant(5), "@max_id");
        cstmt->SetParam(CVariant(5.1f), "@max_fl");
        cstmt->SetOutputParam(CVariant(eDB_Int), "@outp");

        // Execute the stored procedure.
        cstmt->Execute();

        // Retrieve the data.
        while (cstmt->HasMoreResults()) {
            // Use an auto_ptr to manage resultset lifetime.
            // NOTE: Use it with caution. When the wrapped parent object
            // goes out of scope, all child objects are destroyed
            // (which isn't an issue for this demo but could be for
            // other applications).
            auto_ptr<IResultSet> rs(cstmt->GetResultSet());

            // Sometimes the results have no rows - and that's ok.
            if ( ! cstmt->HasRows() ) {
                LOG_POST_X(1, Info << "No rows.");
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

                // Print column names.
                for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                    NcbiCout << "    " << rsMeta->GetName(i);
                }
                NcbiCout << NcbiEndl;

                while (rs->Next()) {
                    // Print a row of data.
                    for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                        NcbiCout << "    " << rs->GetVariant(i).GetString();
                    }
                    NcbiCout << NcbiEndl;
                }
                NcbiCout << "    Row count: " << cstmt->GetRowCount()
                    << NcbiEndl;
                break;
            }

            // These types aren't used in this demo, but might be in
            // your code.
            case eDB_ComputeResult:
            case eDB_CursorResult:
                LOG_POST_X(1, Warning << "Unhandled results type:"
                    << rs->GetResultType());
                break;

            // Any other type means this code is out-of-date.
            default:
                ERR_POST_X(1, Critical << "Unexpected results type:"
                    << rs->GetResultType());
            }
        }

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
//  ParseArgs - parse the command-line arguments.

void CDbapiSimpleApp::ParseArgs(void)
{
    CArgs args = GetArgs();

    m_Server = args["server"].AsString();
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
//      (see http://www.ncbi.nlm.nih.gov/books/NBK7164/).
//
//  10. Setting environment variables, such as FREEBCP_DATEFMT, TDSQUERY,
//      DATA_TABLE, and others (to see how these are used, see
//      http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lxr/ident?i=_bcp_exec_out).
//
//  11. Setting parameters in an interfaces file.
//
//  For more information on setting connection options, please see the NCBI C++
//  Toolkit book chapter on DBAPI: http://www.ncbi.nlm.nih.gov/books/NBK7176/

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
        m_Ds = dm.CreateDs(sm_kDriver);

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
        CTrivialConnValidator val(sm_kDbName);
        m_Conn->ConnectValidated(val, m_User, m_Password, m_Server, sm_kDbName);
        //m_Conn = drv_context->ConnectValidated(m_Server, m_User, m_Password, val);

    } catch (...) { // try/catch here just to facilitate debugging
        throw;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CDbapiSimpleApp().AppMain(argc, argv);
}
