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
* File Description: Python DBAPI unit-test
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "python_ncbi_dbapi_test.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( CPythonDBAPITest );

///////////////////////////////////////////////////////////////////////////////
enum EServerType {
    eUnknown,   //< Server type is not known
    eSybase,    //< Sybase server
    eMsSql,     //< Microsoft SQL server
    eOracle,    //< ORACLE server
    eMySql,     //< MySQL server
    eSqlite     //< SQLITE server
};

string DriverName;
string ServerName;
string UserName;
string UserPassword;

string GetDriverName(void)
{
    return DriverName;
}

string GetServerName(void)
{
    return ServerName;
}

string GetUserName(void)
{
    return UserName;
}

string GetUserPassword(void)
{
    return UserPassword;
}

EServerType
GetServerType(void)
{
    if ( GetServerName() == "STRAUSS"  ||  GetServerName() == "MOZART" ) {
        return eSybase;
    } else if ( GetServerName().substr(0, 6) == "MS_DEV" ) {
        return eMsSql;
    }

    return eUnknown;
}

string
GetServerTypeStr(void)
{
    switch ( GetServerType() ) {
    case eSybase :
        return "SYBASE";
    case eMsSql :
        return "MSSQL";
    case eOracle :
        return "ORACLE";
    case eMySql :
        return "MYSQL";
    case eSqlite :
        return "SQLITE";
    }

    return "none";
}

///////////////////////////////////////////////////////////////////////////////
CPythonDBAPITest::CPythonDBAPITest()
{
}

void
CPythonDBAPITest::setUp()
{
    m_Engine.ExecuteStr("import python_ncbi_dbapi\n");
}


void
CPythonDBAPITest::tearDown()
{
}


void
CPythonDBAPITest::MakeTestPreparation()
{
    string connection_str( "connection = python_ncbi_dbapi.connect('" + GetDriverName() + "', '" + GetServerTypeStr() + "', '" + GetServerName() + "', 'DBAPI_Sample', '" + GetUserName() + "', '" + GetUserPassword() + "', True)\n");
    string conn_simple_str( "conn_simple = python_ncbi_dbapi.connect('" + GetDriverName() + "', '" + GetServerTypeStr() + "', '" + GetServerName() + "', 'DBAPI_Sample', '" + GetUserName() + "', '" + GetUserPassword() + "')\n");

    m_Engine.ExecuteStr( connection_str.c_str() );
    m_Engine.ExecuteStr( conn_simple_str.c_str() );

    m_Engine.ExecuteStr("cursor_simple = conn_simple.cursor() \n");
    m_Engine.ExecuteStr("cursor_simple.execute('CREATE TABLE #t ( vkey int)') \n");
}

void
CPythonDBAPITest::TestBasic()
{
    m_Engine.ExecuteStr("version = python_ncbi_dbapi.__version__ \n");
    m_Engine.ExecuteStr("apilevel = python_ncbi_dbapi.apilevel \n");
    m_Engine.ExecuteStr("threadsafety = python_ncbi_dbapi.threadsafety \n");
    m_Engine.ExecuteStr("paramstyle = python_ncbi_dbapi.paramstyle \n");

    m_Engine.ExecuteStr("connection.commit()\n");
    m_Engine.ExecuteStr("connection.rollback()\n");

    m_Engine.ExecuteStr("cursor = connection.cursor()\n");
    m_Engine.ExecuteStr("cursor2 = conn_simple.cursor()\n");

    m_Engine.ExecuteStr("date_val = python_ncbi_dbapi.Date(1, 1, 1)\n");
    m_Engine.ExecuteStr("time_val = python_ncbi_dbapi.Time(1, 1, 1)\n");
    m_Engine.ExecuteStr("timestamp_val = python_ncbi_dbapi.Timestamp(1, 1, 1, 1, 1, 1)\n");
    m_Engine.ExecuteStr("binary_val = python_ncbi_dbapi.Binary('Binary test')\n");

    m_Engine.ExecuteStr("cursor.execute('select qq = 57 + 33') \n");
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    m_Engine.ExecuteStr("cursor.execute('select qq = 57.55 + 0.0033')\n");
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    m_Engine.ExecuteStr("cursor.execute('select qq = GETDATE()')\n");
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    m_Engine.ExecuteStr("cursor.execute('select name, type from sysobjects')\n");
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    m_Engine.ExecuteStr("cursor.fetchall()\n");
    m_Engine.ExecuteStr("rowcount = cursor.rowcount \n");
    m_Engine.ExecuteStr("cursor.execute('select name, type from sysobjects where type = @type_par', {'type_par':'S'})\n");
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    m_Engine.ExecuteStr("cursor.executemany('select name, type from sysobjects where type = @type_par', [{'type_par':'S'}, {'type_par':'D'}])\n");
    m_Engine.ExecuteStr("cursor.fetchmany()\n");
    m_Engine.ExecuteStr("cursor.fetchall()\n");
    m_Engine.ExecuteStr("cursor.nextset()\n");
    m_Engine.ExecuteStr("cursor.setinputsizes()\n");
    m_Engine.ExecuteStr("cursor.setoutputsize()\n");
    // m_Engine.ExecuteStr("cursor.callproc('test_sp')\n");
    // m_Engine.ExecuteStr("print cursor.fetchall()\n");

    m_Engine.ExecuteStr("cursor2.execute('select qq = 57 + 33')\n");
    m_Engine.ExecuteStr("cursor2.fetchone()\n");

    m_Engine.ExecuteStr("cursor.close()\n");
    m_Engine.ExecuteStr("cursor2.close()\n");

    // m_Engine.ExecuteStr("connection.close()\n");
    // m_Engine.ExecuteStr("connection2.close()\n");
}

void
CPythonDBAPITest::TestExecute()
{
    // Simple test
    {
        m_Engine.ExecuteStr("cursor = connection.cursor()\n");
        m_Engine.ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
        m_Engine.ExecuteStr("cursor.fetchone()\n");
    }
}

void
CPythonDBAPITest::TestFetch()
{
    // Prepare ...
    m_Engine.ExecuteStr("cursor = connection.cursor()\n");
    m_Engine.ExecuteStr("cursor.execute('select name, type from sysobjects')\n");

    // fetchone
    m_Engine.ExecuteStr("cursor.fetchone()\n");
    // fetchmany
    m_Engine.ExecuteStr("cursor.fetchmany(1)\n");
    m_Engine.ExecuteStr("cursor.fetchmany(2)\n");
    m_Engine.ExecuteStr("cursor.fetchmany(3)\n");
    // fetchall
    m_Engine.ExecuteStr("cursor.fetchall()\n");
}

void
CPythonDBAPITest::TestParameters()
{
    // Prepare ...
    m_Engine.ExecuteStr("cursor = connection.cursor()\n");
    m_Engine.ExecuteStr("cursor.execute('select name, type from sysobjects where type = @type_par', {'@type_par':'S'})\n");

    // fetchall
    m_Engine.ExecuteStr("cursor.fetchall()\n");
}

void
CPythonDBAPITest::TestExecuteMany()
{
    // Excute with empty parameter list
    {
        m_Engine.ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
        m_Engine.ExecuteStr("cursor = conn_simple.cursor()\n");
        m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
        m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'value':value} for value in range(1, 11) ]) \n");
    }
}


void
CPythonDBAPITest::TestTransaction()
{
    // "Simple mode" test ...
    {
        m_Engine.ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
        m_Engine.ExecuteStr("sql_sel = 'SELECT * FROM #t' \n");
        m_Engine.ExecuteStr("cursor = conn_simple.cursor() \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
        m_Engine.ExecuteStr("cursor.execute('BEGIN TRANSACTION') \n");
        m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
        m_Engine.ExecuteStr("conn_simple.commit() \n");
        m_Engine.ExecuteStr("conn_simple.rollback() \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
        m_Engine.ExecuteStr("cursor.execute('ROLLBACK TRANSACTION') \n");
        m_Engine.ExecuteStr("cursor.execute('BEGIN TRANSACTION') \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
        m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
        m_Engine.ExecuteStr("cursor.execute('COMMIT TRANSACTION') \n");
        m_Engine.ExecuteStr("cursor.execute(sql_sel) \n");
        m_Engine.ExecuteStr("cursor.fetchall() \n");
    }
}


void
CPythonDBAPITest::TestFromFile()
{
}


void
CPythonDBAPITest::CreateTestTable()
{
}


///////////////////////////////////////////////////////////////////////////////
CUnitTestApp::~CUnitTestApp(void)
{
    return ;
}

void
CUnitTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "python_ncbi_dbapi_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "odbc", "mysql", "oracle", "msdblib", "gateway"
#else
#define DEF_SERVER    "STRAUSS"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "odbc", "mysql", "oracle", "gateway"
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

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int
CUnitTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    DriverName      = args["d"].AsString();
    ServerName      = args["S"].AsString();
    UserName        = args["U"].AsString();
    UserPassword    = args["P"].AsString();

    // Get the top level suite from the registry
    CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest();

    // Adds the test to the list of test to run
    CPPUNIT_NS::TextUi::TestRunner runner;
    runner.addTest( suite );

    // Change the default outputter to a compiler error format outputter
    runner.setOutputter( new CPPUNIT_NS::CompilerOutputter( &runner.result(),   std::cerr ) );
    // Run the test.
    bool wasSucessful = runner.run();

    // Return error code 1 if the one of test failed.
    return wasSucessful ? 0 : 1;
}

void
CUnitTestApp::Exit(void)
{
    return ;
}

END_NCBI_SCOPE

//int
//main(int argc, char *argv[])
//{
//    // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample1.py");
//    // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample5.py");
//    // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\tests\\python_ncbi_dbapi_test.py");

//  // Get the top level suite from the registry
//  CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest();

//  // Adds the test to the list of test to run
//  CPPUNIT_NS::TextUi::TestRunner runner;
//  runner.addTest( suite );

//  // Change the default outputter to a compiler error format outputter
//  runner.setOutputter( new CPPUNIT_NS::CompilerOutputter( &runner.result(), std::cerr ) );
//  // Run the test.
//  bool wasSucessful = runner.run();

//  // Return error code 1 if the one of test failed.
//  return wasSucessful ? 0 : 1;
//}

int main(int argc, const char* argv[])
{
    return ncbi::CUnitTestApp().AppMain(argc, argv);
}

/* ===========================================================================
*
* $Log$
* Revision 1.6  2005/02/17 14:03:36  ssikorsk
* Added database parameters to the unit-test (handled via CNcbiApplication)
*
* Revision 1.5  2005/02/10 20:13:48  ssikorsk
* Improved 'simple mode' test
*
* Revision 1.4  2005/02/08 19:21:18  ssikorsk
* + Test "rowcount" attribute and support the "simple mode" interface
*
* Revision 1.3  2005/02/03 16:11:16  ssikorsk
* python_ncbi_dbapi_test was adapted to the cppunit testing framework
*
* Revision 1.2  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.1  2005/01/18 19:26:08  ssikorsk
* Initial version of a Python DBAPI module
*
* ===========================================================================
*/
