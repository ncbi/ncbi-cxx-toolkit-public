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

// #include <boost/test/unit_test_result.hpp>

#include "python_ncbi_dbapi_test.hpp"
#include <test/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
CPythonDBAPITest::CPythonDBAPITest(const CTestArguments& args)
: m_Engine(new pythonpp::CEngine)
, m_args(args)
{
}

CPythonDBAPITest::~CPythonDBAPITest(void)
{
}

void
CPythonDBAPITest::ExecuteStr(const char* cmd)
{
    pythonpp::CEngine::ExecuteStr(cmd);
}

void
CPythonDBAPITest::ExecuteSQL(const string& sql)
{
    string cmd = string("cursor.execute('''") + sql + "''') \n";
    ExecuteStr(cmd.c_str());
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

        ExecuteStr("import python_ncbi_dbapi\n");
        ExecuteStr( connection_str.c_str() );
        ExecuteStr( conn_simple_str.c_str() );

        ExecuteStr("cursor_simple = conn_simple.cursor() \n");
        ExecuteStr("cursor_simple.execute('CREATE TABLE #t ( vkey int )') \n");

        ExecuteStr("cursor_simple.execute("
            "'CREATE TABLE #t2 ( "
            "   int_val int null, "
            // "   vc1900_field varchar(255) null, "
            "   vc1900_field varchar(1900) null, "
            "   text_val text null)') \n"
            );
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestBasic(void)
{
    try {
        ExecuteStr("version = python_ncbi_dbapi.__version__ \n");
        ExecuteStr("apilevel = python_ncbi_dbapi.apilevel \n");
        ExecuteStr("threadsafety = python_ncbi_dbapi.threadsafety \n");
        ExecuteStr("paramstyle = python_ncbi_dbapi.paramstyle \n");

        ExecuteStr("connection.commit()\n");
        ExecuteStr("connection.rollback()\n");

        ExecuteStr("cursor = connection.cursor()\n");
        ExecuteStr("cursor2 = conn_simple.cursor()\n");

        ExecuteStr("date_val = python_ncbi_dbapi.Date(1, 1, 1)\n");
        ExecuteStr("time_val = python_ncbi_dbapi.Time(1, 1, 1)\n");
        ExecuteStr("timestamp_val = python_ncbi_dbapi.Timestamp(1, 1, 1, 1, 1, 1)\n");
        ExecuteStr("binary_val = python_ncbi_dbapi.Binary('Binary test')\n");

        ExecuteStr("cursor.execute('select qq = 57 + 33') \n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.execute('select qq = 57.55 + 0.0033')\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.execute('select qq = GETDATE()')\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.execute('select name, type from sysobjects')\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("rowcount = cursor.rowcount \n");
        ExecuteStr("cursor.execute('select name, type from sysobjects where type = @type_par', {'type_par':'S'})\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.executemany('select name, type from sysobjects where type = @type_par', [{'type_par':'S'}, {'type_par':'D'}])\n");
        ExecuteStr("cursor.fetchmany()\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("cursor.nextset()\n");
        ExecuteStr("cursor.setinputsizes()\n");
        ExecuteStr("cursor.setoutputsize()\n");

        ExecuteStr("cursor2.execute('select qq = 57 + 33')\n");
        ExecuteStr("cursor2.fetchone()\n");

        ExecuteStr("cursor.close()\n");
        ExecuteStr("cursor2.close()\n");

        ExecuteStr("cursor_simple.close()\n");
        ExecuteStr("connection.close()\n");
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
            ExecuteStr("cursor = connection.cursor()\n");
            ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
            ExecuteStr("cursor.fetchone()\n");
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
        ExecuteStr("cursor = connection.cursor()\n");
        ExecuteStr("cursor.execute('select name, type from sysobjects')\n");

        // fetchone
        ExecuteStr("cursor.fetchone()\n");
        // fetchmany
        ExecuteStr("cursor.fetchmany(1)\n");
        ExecuteStr("cursor.fetchmany(2)\n");
        ExecuteStr("cursor.fetchmany(3)\n");
        // fetchall
        ExecuteStr("cursor.fetchall()\n");
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
        {
            ExecuteStr("cursor = conn_simple.cursor()\n");
        }

        // Very first test ...
        {
            ExecuteStr("cursor.execute('SELECT name, type FROM sysobjects WHERE type = @type_par', {'@type_par':'S'})\n");

            // fetchall
            ExecuteStr("cursor.fetchall()\n");
        }

        // Test for varchar strings ...
        {
            ExecuteStr("cursor.execute('DELETE FROM #t2')\n");
            ExecuteStr("seq_align = 254 * '-' + 'X' \n");
            ExecuteStr("cursor.execute('INSERT INTO #t2(vc1900_field) VALUES(@tv)', {'@tv':seq_align})\n");
            ExecuteStr("cursor.execute('SELECT vc1900_field FROM #t2') \n");
            ExecuteStr("if len(cursor.fetchone()[0]) != 255 : raise StandardError('Invalid string length.') \n");
        }

        // Test for text strings ...
        {
            ExecuteSQL("DELETE FROM #t2");
            ExecuteStr("seq_align = 254 * '-' + 'X' + 100 * '-'\n");
            ExecuteStr("if len(seq_align) != 355 : raise StandardError('Invalid string length.') \n");
            ExecuteStr("cursor.execute('INSERT INTO #t2(vc1900_field) VALUES(@tv)', {'@tv':seq_align})\n");
            ExecuteSQL("SELECT vc1900_field FROM #t2");
            ExecuteStr("record = cursor.fetchone()");
//             ExecuteStr("print record");
//             ExecuteStr("print len(record[0])");
            ExecuteStr("if len(record[0]) != 355 : raise StandardError('Invalid string length.') \n");

            ExecuteStr("cursor.execute('DELETE FROM #t2')\n");
            ExecuteStr("seq_align = 254 * '-' + 'X' + 100 * '-'\n");
            ExecuteStr("if len(seq_align) != 355 : raise StandardError('Invalid string length.') \n");
            ExecuteStr("cursor.execute('INSERT INTO #t2(text_val) VALUES(@tv)', {'@tv':seq_align})\n");
            ExecuteStr("cursor.execute('SELECT text_val FROM #t2') \n");
            ExecuteStr("record = cursor.fetchone() \n");
//             ExecuteStr("print record \n");
            ExecuteStr("if len(record[0]) != 355 : raise StandardError('Invalid string length.') \n");
        }
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
            ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
            ExecuteStr("cursor = conn_simple.cursor()\n");
            ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
            ExecuteStr("cursor.executemany(sql_ins, [ {'value':value} for value in range(1, 11) ]) \n");
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
            ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
            ExecuteStr("sql_sel = 'SELECT * FROM #t' \n");
            ExecuteStr("cursor = conn_simple.cursor() \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
            ExecuteStr("cursor.execute('BEGIN TRANSACTION') \n");
            ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
            ExecuteStr("conn_simple.commit() \n");
            ExecuteStr("conn_simple.rollback() \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
            ExecuteStr("cursor.execute('ROLLBACK TRANSACTION') \n");
            ExecuteStr("cursor.execute('BEGIN TRANSACTION') \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
            ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
            ExecuteStr("cursor.execute('COMMIT TRANSACTION') \n");
            ExecuteStr("cursor.execute(sql_sel) \n");
            ExecuteStr("cursor.fetchall() \n");
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
        m_Engine->ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample9.py");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::Test_callproc(void)
{
    try {
        // Prepare ...
        {
            // ExecuteStr("cursor = connection.cursor()\n");
            ExecuteStr("cursor = conn_simple.cursor()\n");
        }

        // CALL stored procedure ...
        ExecuteStr("cursor.callproc('sp_databases')\n");
        BOOST_CHECK_THROW(
            ExecuteStr("rc = cursor.get_proc_return_status()\n"),
            string
            );
        BOOST_CHECK_THROW(
            ExecuteStr("rc = cursor.get_proc_return_status()\n"),
            string
            );

        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.fetchmany(5)\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");

        ExecuteStr("cursor.callproc('sp_server_info')\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


void
CPythonDBAPITest::TestExecuteStoredProc(void)
{
    try {
        // Prepare ...
        {
            // ExecuteStr("cursor = connection.cursor()\n");
            ExecuteStr("cursor = conn_simple.cursor()\n");
        }

        // EXECUTE stored procedure without parameters ...
        ExecuteStr("cursor.execute('execute sp_databases')\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");

        // EXECUTE stored procedure with parameters ...
        ExecuteStr("cursor.execute('execute sp_server_info 1')\n");
        BOOST_CHECK_THROW(
            ExecuteStr("rc = cursor.get_proc_return_status()\n"),
            string
            );
        BOOST_CHECK_THROW(
            ExecuteStr("rc = cursor.get_proc_return_status()\n"),
            string
            );
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");
        ExecuteStr("cursor.execute('execute sp_server_info 2')\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("cursor.fetchone()\n");
        ExecuteStr("cursor.fetchmany(5)\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");
        ExecuteStr("rc = cursor.get_proc_return_status()\n");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


void
CPythonDBAPITest::Test_SelectStmt(void)
{
    string sql;

    try {
        // Prepare ...
        {
            ExecuteStr("cursor = conn_simple.cursor()\n");

            sql =
            "CREATE TABLE #Overlaps ( \n"
            "	pairId int NOT NULL , \n"
            "	overlapNum smallint NOT NULL , \n"
            "	start1 int NOT NULL , \n"
            "	start2 int NOT NULL , \n"
            "	stop1 int NOT NULL , \n"
            "	stop2 int NOT NULL , \n"
            "	orient char (2) NOT NULL , \n"
            "	gaps int NOT NULL , \n"
            "	mismatches int NOT NULL , \n"
            "	adjustedLen int NOT NULL , \n"
            "	length int NOT NULL , \n"
            "	contained tinyint NOT NULL , \n"
            "	seq_align text  NULL , \n"
            "	merged_sa char (1) NOT NULL , \n"
            "	PRIMARY KEY \n"
            "	( \n"
            "		pairId, \n"
            "		overlapNum \n"
            "	) \n"
            ") \n";

            ExecuteSQL(sql);

            // Insert data into the table ...
            string long_string =
                "Seq-align ::= { type partial, dim 2, score "
                "{ { id str \"score\", value int 6771 }, { id str "
                "\"e_value\", value real { 0, 10, 0 } }, { id str "
                "\"bit_score\", value real { 134230121751674, 10, -10 } }, "
                "{ id str \"num_ident\", value int 7017 } }, segs denseg "
                "{ dim 2, numseg 3, ids { gi 3021694, gi 3924652 }, starts "
                "{ 6767, 32557, 6763, -1, 0, 25794 }, lens { 360, 4, 6763 }, "
                "strands { minus, minus, minus, minus, minus, minus } } }";

            sql  = "long_str = '"+ long_string + "' \n";
            ExecuteStr( sql.c_str() );
            sql  =
                "INSERT INTO #Overlaps VALUES( \n"
                "1, 1, 0, 25794, 7126, 32916, '--', 1, 21, 7124, 7127, 0, \n";
            sql += "'" + long_string + "', 'n')";

            ExecuteSQL(sql);
        }

        // Check ...
        {
            sql = "SELECT * FROM #Overlaps";
            ExecuteSQL(sql);
            ExecuteStr("record = cursor.fetchone() \n");
            // ExecuteStr("print record \n");
            ExecuteStr("if len(record) != 14 : "
                       "raise StandardError('Invalid number of columns.') \n"
            );
            // ExecuteStr("print len(record[12]) \n");
            ExecuteStr("if len(record[12]) != len(long_str) : "
                       "raise StandardError('Invalid string size: ') \n"
            );
        }
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::Test_LOB(void)
{
    string sql;

    try {
        // Prepare ...
        {
            ExecuteStr( "cursor = conn_simple.cursor()\n" );
            ExecuteSQL( "DELETE FROM #t2" );
        }

        // Insert data ...
        {
            string long_string =
                "Seq-align ::= { type partial, dim 2, score "
                "{ { id str \"score\", value int 6771 }, { id str "
                "\"e_value\", value real { 0, 10, 0 } }, { id str "
                "\"bit_score\", value real { 134230121751674, 10, -10 } }, "
                "{ id str \"num_ident\", value int 7017 } }, segs denseg "
                "{ dim 2, numseg 3, ids { gi 3021694, gi 3924652 }, starts "
                "{ 6767, 32557, 6763, -1, 0, 25794 }, lens { 360, 4, 6763 }, "
                "strands { minus, minus, minus, minus, minus, minus } } }";

            sql  = "long_str = '"+ long_string + "' \n";
            ExecuteStr( sql.c_str() );

            sql  = "cursor.execute('INSERT INTO #t2(vc1900_field, text_val) VALUES(@vcv, @tv)', ";
            sql += " {'@vcv':long_str, '@tv':long_str} ) \n";
            ExecuteStr( sql.c_str() );
        }

        // Check ...
        {
            sql = "SELECT vc1900_field, text_val FROM #t2";
            ExecuteSQL(sql);
            ExecuteStr("record = cursor.fetchone() \n");
            ExecuteStr("print record[0] \n");
//             ExecuteStr("print len(record[0]) \n");
//             ExecuteStr("print long_str \n");
            ExecuteStr("if len(record[0]) != len(long_str) : "
                       "raise StandardError('Invalid string size: ') \n"
            );
            ExecuteStr("if len(record[1]) != len(long_str) : "
                       "raise StandardError('Invalid string size: ') \n"
            );
        }
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }

}

// From example8.py
void
CPythonDBAPITest::TestScenario_1(void)
{
    string sql;

    try {
        // Prepare ...
        {
            ExecuteStr( "cursor = conn_simple.cursor()\n" );
        }

        // Create a table ...
        {
            sql = " CREATE TABLE #sale_stat ( \n"
                        " year INT NOT NULL, \n"
                        " month VARCHAR(255) NOT NULL, \n"
                        " stat INT NOT NULL \n"
                " ) ";
            ExecuteSQL(sql);
        }

        // Insert data ..
        {
            ExecuteStr("month_list = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']");
            ExecuteStr("sql = \"insert into #sale_stat(year, month, stat) values (@year, @month, @stat)\"");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"Empty table contains\", len( cursor.fetchall() ), \"records\"");
            ExecuteSQL("BEGIN TRANSACTION");
            ExecuteStr("cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"We have inserted\", len( cursor.fetchall() ), \"records\"");
            ExecuteStr("conn_simple.rollback();");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"After a 'standard' rollback command the table contains\", len( cursor.fetchall() ), \"records\"");
            ExecuteSQL("ROLLBACK TRANSACTION");
            ExecuteSQL("BEGIN TRANSACTION");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"After a 'manual' rollback command the table contains\", len( cursor.fetchall() ), \"records\"");
            ExecuteStr("cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"We have inserted\", len( cursor.fetchall() ), \"records\"");
            ExecuteSQL("COMMIT TRANSACTION");
            ExecuteSQL("select * from #sale_stat");
            ExecuteStr("print \"After a 'manual' commit command the table contains\", len( cursor.fetchall() ), \"records\"");
        }
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

    if ( ( args.GetDriverName() == "ctlib") ||
         ( (args.GetDriverName() == "ftds" || args.GetDriverName() == "ftds63") &&
           args.GetServerType() == CTestArguments::eMsSql )
         ) {
        // This test doen't work with the dblib driver currently ...
        tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::Test_LOB, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);

        tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestParameters, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::Test_SelectStmt, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestBasic, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecute, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestFetch, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecuteMany, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::Test_callproc, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    string server_name = args.GetDriverName();
    NStr::ToUpper( server_name );
    // Do not run this test case for the ODBC driver.
    if ( server_name.compare("ODBC") != 0 ) {
        tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecuteStoredProc, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestScenario_1, DBAPIInstance);
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
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "msdblib", "odbc", "gateway"
#else
#define DEF_SERVER    "OBERON"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "gateway"
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
    if ( GetServerName() == "STRAUSS" ||
         GetServerName() == "MOZART" ||
         GetServerName() == "OBERON" ||
         GetServerName().compare(0, 6, "BARTOK") == 0) {
        return eSybase;
    } else if ( NStr::EqualNocase( GetServerName(), 0, sizeof("MS_DEV") - 1, "MS_DEV") ) {
        return eMsSql;
    }

    return eUnknown;
}

void
CTestArguments::SetDatabaseParameters(void)
{
    if ( GetDriverName() == "ctlib" &&
         GetServerName().compare(0, 6, "BARTOK") != 0 ) {
      m_DatabaseParameters["version"] = "125";
    } else if ( GetDriverName() == "dblib" && GetServerType() == eSybase ) {
        // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
        // BcpIn to it using protocol version other than "100".
        m_DatabaseParameters["version"] = "125";
    } else if ( (GetDriverName() == "ftds" || GetDriverName() == "ftds63") &&
                GetServerType() == eSybase ) {
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

    test_suite* test = BOOST_TEST_SUITE("DBAPI Unit Test.");

    test->add(new ncbi::CPythonDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test;
}

/* ===========================================================================
*
* $Log$
* Revision 1.25  2006/12/12 15:43:56  ssikorsk
* Replaced STRAUSS with OBERON
*
* Revision 1.24  2006/02/28 12:09:46  ivanov
* #include <test/test_assert.h>
*
* Revision 1.23  2005/12/05 17:04:31  ucko
* Add support for Boost 1.33.x, which is now installed on Solaris 10.
*
* Revision 1.22  2005/11/16 16:34:53  ssikorsk
* Handle Sybase server 'BARTOK'
*
* Revision 1.21  2005/10/24 13:07:51  ssikorsk
* Added support for the ftds63 driver
*
* Revision 1.20  2005/10/20 13:11:19  ssikorsk
* Use TDS protocol 12.5 with the dblib driver
*
* Revision 1.19  2005/10/19 15:53:33  ssikorsk
* Implemented the TestScenario_1 test
*
* Revision 1.18  2005/09/14 17:50:55  ssikorsk
* 	  Do not use named constraints with a temporary table
*
* Revision 1.17  2005/09/13 14:46:39  ssikorsk
* Added a Test_LOB implementation
*
* Revision 1.16  2005/09/07 11:15:36  ssikorsk
* Added an implementation of the Test_SelectStmt method
*
* Revision 1.15  2005/08/09 14:55:54  ssikorsk
* Added explicit cursor creation.
*
* Revision 1.14  2005/06/02 22:12:20  ssikorsk
* Added new tests. Simplified the code.
*
* Revision 1.13  2005/06/02 18:42:00  ssikorsk
* Added new test for passing long strings as a parameter
*
* Revision 1.12  2005/06/01 18:41:12  ssikorsk
* Do not check the "get_proc_return_status" method after "execute"
* stored procedute with the ODBC driver.
*
* Revision 1.11  2005/05/31 14:56:27  ssikorsk
* Added get_proc_return_status to the cursor class in the Python DBAPI
*
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
