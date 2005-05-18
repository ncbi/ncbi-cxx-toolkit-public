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

#include <boost/test/unit_test_result.hpp>

#include "python_ncbi_dbapi_test.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
CPythonDBAPITest::CPythonDBAPITest(const CTestArguments& args)
: m_args(args)
{
}

void
CPythonDBAPITest::MakeTestPreparation(void)
{
    try {
        string connection_args( m_args.GetDriverName() + "', '" +
                                m_args.GetServerTypeStr() + "', '" +
                                m_args.GetServerName() + "', '" +
                                m_args.GetDatabaseName() + "', '" +
                                m_args.GetUserName() + "', '" +
                                m_args.GetUserPassword() );

        string connection_str( "connection = python_ncbi_dbapi.connect('" +
                                connection_args + 
                                "', True)\n");
        string conn_simple_str( "conn_simple = python_ncbi_dbapi.connect('" + 
                                connection_args + 
                                "')\n");

        m_Engine.ExecuteStr("import python_ncbi_dbapi\n");
        m_Engine.ExecuteStr( connection_str.c_str() );
        m_Engine.ExecuteStr( conn_simple_str.c_str() );

        m_Engine.ExecuteStr("cursor_simple = conn_simple.cursor() \n");
        m_Engine.ExecuteStr("cursor_simple.execute('CREATE TABLE #t ( vkey int)') \n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestBasic(void)
{
    try {
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

        m_Engine.ExecuteStr("cursor2.execute('select qq = 57 + 33')\n");
        m_Engine.ExecuteStr("cursor2.fetchone()\n");

        m_Engine.ExecuteStr("cursor.close()\n");
        m_Engine.ExecuteStr("cursor2.close()\n");

        m_Engine.ExecuteStr("cursor_simple.close()\n");
        m_Engine.ExecuteStr("connection.close()\n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestExecute(void)
{
    try {
        // Simple test
        {
            m_Engine.ExecuteStr("cursor = connection.cursor()\n");
            m_Engine.ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
            m_Engine.ExecuteStr("cursor.fetchone()\n");
        }
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestFetch(void)
{
    try {
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
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestParameters(void)
{
    try {
        // Prepare ...
        m_Engine.ExecuteStr("cursor = connection.cursor()\n");
        m_Engine.ExecuteStr("cursor.execute('select name, type from sysobjects where type = @type_par', {'@type_par':'S'})\n");

        // fetchall
        m_Engine.ExecuteStr("cursor.fetchall()\n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestExecuteMany(void)
{
    try {
        // Excute with empty parameter list
        {
            m_Engine.ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
            m_Engine.ExecuteStr("cursor = conn_simple.cursor()\n");
            m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
            m_Engine.ExecuteStr("cursor.executemany(sql_ins, [ {'value':value} for value in range(1, 11) ]) \n");
        }
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


void
CPythonDBAPITest::TestTransaction(void)
{
    try {
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
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


void
CPythonDBAPITest::TestFromFile(void)
{
    try {
        m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample9.py");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void 
CPythonDBAPITest::TestStoredProcedures(void)
{
    try {
        m_Engine.ExecuteStr("cursor.callproc('sp_databases')\n");
        m_Engine.ExecuteStr("rc = cursor.get_proc_return_status()\n");
        m_Engine.ExecuteStr("cursor.fetchall()\n");

        // m_Engine.ExecuteStr("cursor.callproc('sp_server_info')\n");
        // m_Engine.ExecuteStr("print cursor.fetchall()\n");
        m_Engine.ExecuteStr("cursor.callproc('sp_server_info 1')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");
        m_Engine.ExecuteStr("cursor.callproc('sp_server_info 2')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");

        m_Engine.ExecuteStr("cursor.execute('execute sp_databases')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");

        m_Engine.ExecuteStr("cursor.callproc('execute sp_server_info 1')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");
        m_Engine.ExecuteStr("cursor.callproc('execute sp_server_info 2')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");

        // EXECUTE stored procedure with parameters ...
        m_Engine.ExecuteStr("cursor.execute('execute sp_server_info 1')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");
        m_Engine.ExecuteStr("cursor.execute('execute sp_server_info 2')\n");
        m_Engine.ExecuteStr("print cursor.fetchall()\n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


///////////////////////////////////////////////////////////////////////////
CPythonDBAPITestSuite::CPythonDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
    // add member function test cases to a test suite
    boost::shared_ptr<CPythonDBAPITest> DBAPIInstance( new CPythonDBAPITest( args ) );
    boost::unit_test::test_case* tc = NULL;
    boost::unit_test::test_case* tc_init = 
        BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::MakeTestPreparation, DBAPIInstance);

    add(tc_init);

    // tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestBasic, DBAPIInstance);
    // tc->depends_on(tc_init);
    // add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecute, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestFetch, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestParameters, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecuteMany, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestStoredProcedures, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

//     tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestFromFile, DBAPIInstance);
//     tc->depends_on(tc_init);
//     add(tc);

}

CPythonDBAPITestSuite::~CPythonDBAPITestSuite(void)
{
}


///////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(int argc, char * argv[])
{
    CNcbiArguments arguments(argc, argv);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    // Specify USAGE context
    arg_desc->SetUsageContext(arguments.GetProgramBasename(),
                              "dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "msdblib", "odbc", "gateway"
#else
#define DEF_SERVER    "STRAUSS"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "gateway"
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

    auto_ptr<CArgs> args_ptr(arg_desc->CreateArgs(arguments));
    const CArgs& args = *args_ptr;


    // Get command-line arguments ...
    m_DriverName    = args["d"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();

    SetDatabaseParameters();
}

CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    if ( GetServerName() == "STRAUSS"  ||  GetServerName() == "MOZART" ) {
        return eSybase;
    } else if ( GetServerName().substr(0, 6) == "MS_DEV" ) {
        return eMsSql;
    }

    return eUnknown;
}

void
CTestArguments::SetDatabaseParameters(void)
{
    if ( GetDriverName() == "dblib" && GetServerType() == eSybase ) {
        // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
        // BcpIn to it using protocol version other than "100".
        m_DatabaseParameters["version"] = "100";
    } else if ( GetDriverName() == "ftds" && GetServerType() == eSybase ) {
        // ftds forks with Sybase databases using protocol v42 only ...
        m_DatabaseParameters["version"] = "42";
    }
}

string
CTestArguments::GetServerTypeStr(void) const
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
    default :
        return "none";
    }

    return "none";
}

END_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////
test_suite*
init_unit_test_suite( int argc, char * argv[] )
{
    // Configure UTF ...
    // boost::unit_test_framework::unit_test_log::instance().set_log_format( "XML" );
    // boost::unit_test_framework::unit_test_result::set_report_format( "XML" );

    std::auto_ptr<test_suite> test(BOOST_TEST_SUITE( "DBAPI Unit Test." ));

    test->add(new ncbi::CPythonDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test.release();
}

/* ===========================================================================
*
* $Log$
* Revision 1.10  2005/05/18 18:41:07  ssikorsk
* Small refactoring
*
* Revision 1.9  2005/05/17 16:42:34  ssikorsk
* Moved on Boost.Test
*
* Revision 1.8  2005/03/10 17:22:33  ssikorsk
* Fixed a compilation warning
*
* Revision 1.7  2005/03/01 15:22:58  ssikorsk
* Database driver manager revamp to use "core" CPluginManager
*
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
