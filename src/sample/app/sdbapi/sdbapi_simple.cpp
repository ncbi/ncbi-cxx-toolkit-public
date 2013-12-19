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
 * Authors: David McElhany
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
 *  The program is written assuming static linkage and a connection to
 *  SQL Server.
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <dbapi/simple/sdbapi.hpp>


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CSdbapiSimpleApp::

class CSdbapiSimpleApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    void ParseArgs(void);
    void SetupDb(void);

    void DemoStoredProc(void);
    void DemoParamerizedSql(void);
    void DemoStaticSql(void);
    void DemoDynamicSql(void);

    void DemoSqlInjection(void);

    // Command-line options
    string  m_UserString1;
    string  m_UserString2;
    string  m_UserString3;
    string  m_Service;
    string  m_DbName;
    string  m_User;
    string  m_Password;

    // Application data
    CDatabase   m_Db;
};


/////////////////////////////////////////////////////////////////////////////
//  Init - setup command-line arguments

void CSdbapiSimpleApp::Init(void)
{
    CArgDescriptions* argdesc = new CArgDescriptions();
    argdesc->SetUsageContext(GetArguments().GetProgramBasename(),
                             "SDBAPI simple operations demo");


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


int CSdbapiSimpleApp::Run(void)
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
//  ParseArgs - parse the command-line arguments.

void CSdbapiSimpleApp::ParseArgs(void)
{
    CArgs args = GetArgs();

    m_UserString1 = args["user_string1"].AsString();
    m_UserString2 = args["user_string2"].AsString();
    m_UserString3 = args["user_string3"].AsString();
    m_Service = args["service"].AsString();
    m_DbName = args["db_name"].AsString();
    m_User = args["user"].AsString();
    m_Password = args["password"].AsString();
}


/////////////////////////////////////////////////////////////////////////////
//  SetupDb - connect to the database.

void CSdbapiSimpleApp::SetupDb(void)
{
    // Specify connection parameters.
    // If you are inside NCBI and need to know what credentials to use,
    // email cpp-core.
    string uri = "dbapi://" + m_User + ":" + m_Password + "@/" + m_DbName;
    CSDB_ConnectionParam db_params(uri);

    // Specify the service.
    db_params.Set(CSDB_ConnectionParam::eService, m_Service);

    // Connect to the database.
    m_Db = CDatabase(db_params);
    m_Db.Connect();
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
//
//  In case you don't have access to the stored procedure, please see the
//  companion file sdbapi_simple_sproc.sql.
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

void CSdbapiSimpleApp::DemoStoredProc(void)
{
    // Pick a stored procedure.
    string proc_name("sdbapi_simple_sproc");

    // Create an integer input parameter "@max_id", a float input
    // parameter "@max_fl", and an integer output parameter "@num_rows".
    CQuery query = m_Db.NewQuery();
    query.SetParameter("@max_id", 5);
    query.SetParameter("@max_fl", 5.1f);
    query.SetParameter("@num_rows", 0, eSDB_Int4, eSP_InOut);

    // Execute the stored procedure.
    NcbiCout << "\nExecuting stored proc \"" << proc_name << "\":" << NcbiEndl;
    query.ExecuteSP(proc_name);
    // query.RequireRowCount(...);

    // Print the results.
    //
    // NOTE: For database APIs, array-like indexes are 1-based, not 0-based!
    //
    NcbiCout << "int_val    fl_val" << NcbiEndl;
    ITERATE(CQuery, row, query.SingleSet()) {
        NcbiCout
            << row[1].AsInt4() << "    "
            << row[2].AsFloat() << NcbiEndl;
    }

    // Confirm having read all results.
    query.VerifyDone();

    // Print the number of result rows.
    NcbiCout
        << "Number of rows: "
        << query.GetParameter("@num_rows").AsInt4() << NcbiEndl;
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

void CSdbapiSimpleApp::DemoStaticSql(void)
{
    // Create a static SQL statement.
    string sql("SELECT [title] FROM [Journal]");

    // Execute the static SQL.
    NcbiCout << "\nExecuting static SQL \"" << sql << "\":" << NcbiEndl;
    CQuery query = m_Db.NewQuery(sql);
    query.Execute();
    // query.RequireRowCount(...);

    // Print the results.
    //
    // NOTE: For database APIs, array-like indexes are 1-based, not 0-based!
    //
    NcbiCout << "title" << NcbiEndl;
    ITERATE(CQuery, row, query.SingleSet()) {
        NcbiCout << row[1].AsString() << NcbiEndl;
    }
    query.VerifyDone();
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

void CSdbapiSimpleApp::DemoParamerizedSql(void)
{
    // Get user-supplied strings.
    CArgs args = GetArgs();
    string user_last   = args["user_string1"].AsString();
    string user_salary = args["user_string2"].AsString();
    string user_hire   = args["user_string3"].AsString();

    // Create a parameterized SQL statement.
    string sql(" SELECT [id], [last], [first], [salary], [hiredate]"
               " FROM [Employee] WHERE [last] LIKE @last"
               " AND [salary] > @salary"
               " AND [hiredate] > @hire");

    // Assign parameters.
    CQuery query = m_Db.NewQuery();
    query.SetParameter("@last",   user_last);
    query.SetParameter("@salary", user_salary);
    query.SetParameter("@hire",   user_hire);

    // Execute the parameterized SQL.
    NcbiCout << "\nExecuting parameterized SQL \"" << sql << "\":" << NcbiEndl;
    query.SetSql(sql);
    query.Execute();
    // query.RequireRowCount(...);

    // Print the results.
    //
    // NOTE: For database APIs, array-like indexes are 1-based, not 0-based!
    //
    NcbiCout << "id    last    first    salary    hiredate" << NcbiEndl;
    ITERATE(CQuery, row, query.SingleSet()) {
        NcbiCout
            << row[1].AsInt4() << "    "
            << row[2].AsString() << "    "
            << row[3].AsString() << "    "
            << row[4].AsInt4() << "    "
            << row[5].AsString() << NcbiEndl;
    }
    query.VerifyDone();
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

void CSdbapiSimpleApp::DemoDynamicSql(void)
{
    // Get user-supplied strings.
    CArgs args = GetArgs();
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
    CQuery query = m_Db.NewQuery();
    query.SetSql(sql);
    query.Execute();
    // query.RequireRowCount(...);

    // Print the results.
    //
    // NOTE: For database APIs, array-like indexes are 1-based, not 0-based!
    //
    NcbiCout << "id    last    first    salary    hiredate" << NcbiEndl;
    ITERATE(CQuery, row, query.SingleSet()) {
        NcbiCout
            << row[1].AsInt4() << "    "
            << row[2].AsString() << "    "
            << row[3].AsString() << "    "
            << row[4].AsInt4() << "    "
            << row[5].AsString() << NcbiEndl;
    }
    query.VerifyDone();
}


/////////////////////////////////////////////////////////////////////////////
//  DemoSqlInjection - demonstrate how SQL injection can happen.
//
//  This function is simply for educational purposes -- to show how code that
//  constructs a SQL statement from unsanitized user input is vulnerable to
//  SQL injection.

void CSdbapiSimpleApp::DemoSqlInjection(void)
{
#if 0
    // Get user-supplied string.
    CArgs args = GetArgs();
    string user_input = args["user_string1"].AsString();

    // DO NOT DO THIS -- IT ALLOWS SQL INJECTION.
    // Dynamically create a SQL statement directly from unsanitized user input.
    string sql(" SELECT [id], [last], [first], [salary], [hiredate]"
               " FROM [Employee] WHERE [last] LIKE '" + user_input + "'");

    // If you did the above, then the 'sql' string could contain malicious SQL.
    // For example, the user input:
    //      a' OR 1=1; DROP TABLE [Employee]; --
    // would result in the SQL statement:
    //      SELECT [id], [last], [first], [salary], [hiredate]
    //      FROM [Employee] WHERE [last] LIKE 'a' OR 1=1; DROP TABLE [emp]; --'
#endif
}


/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
    return CSdbapiSimpleApp().AppMain(argc, argv);
}
