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
 *  This program demonstrates connecting to a database and calling a stored
 *  procedure.  The program is written assuming static linkage and a
 *  connection to SQL Server.
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
//#include <corelib/ncbidiag.hpp>

#include <dbapi/simple/sdbapi.hpp>


#define NCBI_USE_ERRCODE_X   SdbapiSimpleSproc

BEGIN_NCBI_SCOPE
NCBI_DEFINE_ERRCODE_X(SdbapiSimpleSproc, 3300, 1);
END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CSdbapiSimpleApp::

class CSdbapiSimpleApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    void DemoProcExec(void);
    void ParseArgs(void);
    void SetupDb(void);

    // Command-line options
    string  m_Server;
    string  m_User;
    string  m_Password;

    // Application data
    CDatabase           m_Db;
    static const string sm_kDbName;
    static const string sm_kSpName;
};
const string    CSdbapiSimpleApp::sm_kDbName = "DBAPI_Sample";
const string    CSdbapiSimpleApp::sm_kSpName = "sdbapi_simple_sproc";


/////////////////////////////////////////////////////////////////////////////
//  Init - setup command-line arguments

void CSdbapiSimpleApp::Init(void)
{
    CArgDescriptions* argdesc = new CArgDescriptions();
    argdesc->SetUsageContext(GetArguments().GetProgramBasename(),
                             "SDBAPI simple stored procedure demo");


    argdesc->AddPositional("server", "Server name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("user", "User name",
                           CArgDescriptions::eString);

    argdesc->AddPositional("password", "User password",
                           CArgDescriptions::eString);


    SetupArgDescriptions(argdesc);
}


/////////////////////////////////////////////////////////////////////////////
//  Run - i.e. parse command-line arguments and demo executing a stored proc.


int CSdbapiSimpleApp::Run(void)
{
    // Get user parameters and connect to database.
    ParseArgs();
    SetupDb();

    // Do something useful with the database.
    DemoProcExec();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  DemoProcExec - demonstrate executing a stored proc.
//
//  This demo uses a stored proc that:
//  1.  Checks for recommended settings.
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

void CSdbapiSimpleApp::DemoProcExec(void)
{
    // Get access to a SQL statement.
    NcbiCout << "Executing stored procedure \"" << sm_kSpName << "\":"
        << NcbiEndl;
    CQuery query = m_Db.NewQuery();

    // Create an integer input parameter "@max_id", a float input
    // parameter "@max_fl", and an integer output parameter "@num_rows".
    query.SetParameter("@max_id", 5);
    query.SetParameter("@max_fl", 5.1f);
    query.SetParameter("@num_rows", 0, eSDB_Int4, eSP_InOut);

    // Execute the stored procedure.
    query.ExecuteSP(sm_kSpName);

    // Print the column names.
    NcbiCout << "    int_val    fl_val" << NcbiEndl;

    // Retrieve the data.
    //
    // NOTE: For database APIs, array-like indexes are 1-based, not 0-based!
    //
    ITERATE(CQuery, row, query.SingleSet()) {
        NcbiCout
            << "    " << row[1].AsInt4()
            << "    " << row[2].AsFloat() << NcbiEndl;
    }

    // Print the output parameter.
    NcbiCout
        << "Number of rows: "
        << query.GetParameter("@num_rows").AsInt4() << NcbiEndl;
}


/////////////////////////////////////////////////////////////////////////////
//  ParseArgs - parse the command-line arguments.

void CSdbapiSimpleApp::ParseArgs(void)
{
    CArgs args = GetArgs();

    m_Server = args["server"].AsString();
    m_User = args["user"].AsString();
    m_Password = args["password"].AsString();
}


/////////////////////////////////////////////////////////////////////////////
//  SetupDb - connect to the database.

void CSdbapiSimpleApp::SetupDb(void)
{
    // Specify connection parameters.
    string uri = "dbapi://" + m_User + ":" + m_Password + "@/" + sm_kDbName;
    CSDB_ConnectionParam db_params(uri);

    // Specify the server.
    db_params.Set(CSDB_ConnectionParam::eService, m_Server);

    // Connect to the database.
    m_Db = CDatabase(db_params);
    m_Db.Connect();
}


/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
    return CSdbapiSimpleApp().AppMain(argc, argv);
}
