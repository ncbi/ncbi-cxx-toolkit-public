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
 *   1. If stored procedures can be used, then use them.  This increases both
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
 * 
 *  See  'dbapi_simple_common.cpp' for an implementation detals.
 * 
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include "dbapi_simple_common.hpp"


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CDbapiSimpleApp::

class CDbapiSimpleApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void) { m_Sample.Exit(); };
    
    CNcbiSample_Dbapi_Simple m_Sample;

    void ParseArgs(CNcbiSample_Dbapi_Simple& sample);
};


/////////////////////////////////////////////////////////////////////////////
//  Init - setup command-line arguments

void CDbapiSimpleApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DBAPI simple operations demo");


    arg_desc->AddDefaultKey("user_string1", "UserString1", "A user-supplied "
                            "string to be used in one of the demonstrations "
                            "(could contain a SQL injection attempt)",
                            CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey("user_string2", "UserString2", "Another "
                            "user-supplied string to be used in one of the "
                            "demonstrations (could contain a SQL injection "
                            "attempt)",
                            CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey("user_string3", "UserString3", "Yet another "
                            "user-supplied string to be used in one of the "
                            "demonstrations (could contain a SQL injection "
                            "attempt)",
                            CArgDescriptions::eString, "");

    arg_desc->AddPositional("service", "Service name",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("db_name", "Database name",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("user", "User name",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("password", "User password",
                            CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
    
    // Init sample logic
    m_Sample.Init();
}


/////////////////////////////////////////////////////////////////////////////
//  Run - i.e. parse command-line arguments and demo simple operations.

int CDbapiSimpleApp::Run(void)
{
    // Get user parameters
    ParseArgs(m_Sample);

    // Execute sample logic
    int exit_code = m_Sample.Run();

    return exit_code;
}


/////////////////////////////////////////////////////////////////////////////
//  ParseArgs - parse the command-line arguments.

void CDbapiSimpleApp::ParseArgs(CNcbiSample_Dbapi_Simple& sample)
{
    const CArgs& args = GetArgs();

    sample.m_Service     = args["service"].AsString();
    sample.m_DbName      = args["db_name"].AsString();
    sample.m_User        = args["user"].AsString();
    sample.m_Password    = args["password"].AsString();
    sample.m_UserString1 = args["user_string1"].AsString();
    sample.m_UserString2 = args["user_string2"].AsString();
    sample.m_UserString3 = args["user_string3"].AsString();

    if (args["logfile"].HasValue()) {
        sample.m_LogFileName = args["logfile"].AsString();
    }
    else {
        sample.m_LogFileName = GetAppName() + ".log";
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CDbapiSimpleApp().AppMain(argc, argv);
}
