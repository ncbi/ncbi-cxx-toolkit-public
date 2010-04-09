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

#include <corelib/ncbiapp.hpp>

#include "sdbapi_unit_test.hpp"

BEGIN_NCBI_SCOPE

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


///////////////////////////////////////////////////////////////////////////////
static AutoPtr<CDatabase> s_Conn = NULL;

CDatabase&
GetDatabase(void)
{
    _ASSERT(s_Conn.get());
    return *s_Conn;
}


///////////////////////////////////////////////////////////////////////////////
NCBITEST_AUTO_INIT()
{
    // Using old log format ...
    // Show time (no msec.) ...
    SetDiagPostFlag(eDPF_DateTime);

    try {
        CSDB_ConnectionParam params;
        params.Set(CSDB_ConnectionParam::eService,  GetArgs().GetServerName());
        params.Set(CSDB_ConnectionParam::eUsername, GetArgs().GetUserName());
        params.Set(CSDB_ConnectionParam::ePassword, GetArgs().GetUserPassword());
        params.Set(CSDB_ConnectionParam::eDatabase, GetArgs().GetDatabaseName());
        s_Conn.reset(new CDatabase(params));
        s_Conn->Connect();
    }
    catch (CSDB_Exception& ex) {
        LOG_POST(Warning << "Error connecting to database: " << ex);
        NcbiTestSetGlobalDisabled();
        return;
    }
    _ASSERT(s_Conn.get());

    CQuery query(GetDatabase().NewQuery());

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
    query.SetSql(sql);
    query.Execute();

    sql  = " CREATE UNIQUE INDEX #ind01 ON " + GetTableName() + "( id ) \n";

    // Create an index
    query.SetSql(sql);
    query.Execute();

    sql  = " CREATE TABLE #dbapi_bcp_table2 ( \n";
    sql += "    id INT NULL, \n";
    // Identity won't work with bulk insert ...
    // sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    int_field INT NULL, \n";
    sql += "    vc1000_field VARCHAR(1000) NULL, \n";
    sql += "    text_field TEXT NULL \n";
    sql += " )";

    query.SetSql(sql);
    query.Execute();

    sql  = " CREATE TABLE #test_unicode_table ( \n";
    sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
    sql += "    nvc255_field NVARCHAR(255) NULL \n";
//        sql += "    nvc255_field VARCHAR(255) NULL \n";
    sql += " )";

    // Create table
    query.SetSql(sql);
    query.Execute();
}

NCBITEST_AUTO_FINI()
{
    s_Conn.reset(NULL);
}


////////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(void)
{
    const CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app) {
        return;
    }
    const CArgs& args = app->GetArgs();

    // Get command-line arguments ...
    m_ServerName = args["S"].AsString();
    m_UserName = args["U"].AsString();
    m_Password = args["P"].AsString();
    m_DatabaseName = args["D"].AsString();
}

////////////////////////////////////////////////////////////////////////////////
NCBITEST_INIT_CMDLINE(arg_desc)
{
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MSSQL"

#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "Sybase"
#else
#define DEF_SERVER    "MSSQL"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddDefaultKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString, "DBAPI_Sample");
}

END_NCBI_SCOPE
