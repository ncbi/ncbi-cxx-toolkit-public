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

#include "python_ncbi_dbapi_test.hpp"

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <corelib/test_boost.hpp>
#include <corelib/plugin_manager.hpp>

#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>

BEGIN_NCBI_SCOPE


static pythonpp::CEngine* s_Engine;


static const CTestArguments&
GetArgs(void)
{
    static CTestArguments s_Args;
    return s_Args;
}

static string
GetSybaseClientVersion(void)
{
    string sybase_version;

#if defined(NCBI_OS_MSWIN)
    sybase_version = "12.5";
#else
    impl::CDriverContext::ResetEnvSybase();

    CNcbiEnvironment env;
    sybase_version = env.Get("SYBASE");
    CDirEntry dir_entry(sybase_version);
    dir_entry.DereferenceLink();
    sybase_version = dir_entry.GetPath();

    sybase_version = sybase_version.substr(
        sybase_version.find_last_of('/') + 1
        );
#endif

    return sybase_version;
}


static void
ExecuteStr(const char* cmd)
{
    pythonpp::CEngine::ExecuteStr(cmd);
}

static void
ExecuteStr(const string& cmd)
{
    pythonpp::CEngine::ExecuteStr(cmd.c_str());
}

static void
ExecuteSQL(const string& sql)
{
    string cmd = string("cursor.execute('''") + sql + "''') \n";
    ExecuteStr(cmd);
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "odbc"
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MSDEV1"
#define DEF_DRIVER    "ftds"
#else
#define DEF_SERVER    "THALBERG"
#define DEF_DRIVER    "ftds"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("dr", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString,
                            DEF_DRIVER);
    arg_desc->SetConstraint("dr", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "DBAPI_test");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddDefaultKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString,
                            "DBAPI_Sample");
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

    parser->AddSymbol("SERVER_SybaseSQL", GetArgs().GetServerType() == CTestArguments::eSybase);
    parser->AddSymbol("SERVER_MicrosoftSQL", GetArgs().GetServerType() == CTestArguments::eMsSql);

    parser->AddSymbol("DRIVER_ftds", GetArgs().GetDriverName() == "ftds");
    parser->AddSymbol("DRIVER_odbc", GetArgs().GetDriverName() == "odbc");
    parser->AddSymbol("DRIVER_ctlib", GetArgs().GetDriverName() == "ctlib");
    parser->AddSymbol("DRIVER_dblib", GetArgs().GetDriverName() == "dblib");
}

NCBITEST_AUTO_INIT()
{
    CPluginManager_DllResolver::EnableGlobally(true);

    CNcbiEnvironment env;
#ifdef NCBI_OS_MSWIN
    env.Set("PYTHONPATH", env.Get("CFG_BIN"));
#else
    string lib_dir = env.Get("CFG_LIB");
    env.Set("PYTHONPATH", lib_dir);
    env.Set("LD_LIBRARY_PATH", lib_dir + ":" + env.Get("LD_LIBRARY_PATH"));
#endif

    s_Engine = new pythonpp::CEngine;

    string connection_args( GetArgs().GetDriverName() + "', '" +
                            GetArgs().GetServerTypeStr() + "', '" +
                            GetArgs().GetServerName() + "', '" +
                            GetArgs().GetDatabaseName() + "', '" +
                            GetArgs().GetUserName() + "', '" +
                            GetArgs().GetUserPassword() );

    string connection_str( "connection = dbapi.connect('" +
                            connection_args +
                            "', True)\n");
    string conn_simple_str( "conn_simple = dbapi.connect('" +
                            connection_args +
                            "')\n");

    ExecuteStr("import python_ncbi_dbapi as dbapi\n");
    ExecuteStr("import datetime\n");
    ExecuteStr("dbapi.release_global_lock(True)");
    ExecuteStr( connection_str );
    ExecuteStr( conn_simple_str );

    ExecuteStr("cursor_simple = conn_simple.cursor() \n");
    ExecuteStr("cursor_simple.execute('CREATE TABLE #t ( vkey int )') \n");

    ExecuteStr("cursor_simple.execute("
        "'CREATE TABLE #t2 ( "
        "   int_val int null, "
        // "   vc1900_field varchar(255) null, "
        "   vc1900_field varchar(1900) null, "
        "   text_val text null, "
        "   image_val image null)') \n"
        );
}

NCBITEST_AUTO_FINI()
{
    // delete m_Engine;
}


BOOST_AUTO_TEST_CASE(TestBasic)
{
    ExecuteStr("version = dbapi.__version__ \n");
    ExecuteStr("apilevel = dbapi.apilevel \n");
    ExecuteStr("threadsafety = dbapi.threadsafety \n");
    ExecuteStr("paramstyle = dbapi.paramstyle \n");

    ExecuteStr("connection.commit()\n");
    ExecuteStr("connection.rollback()\n");

    ExecuteStr("cursor = connection.cursor()\n");
    ExecuteStr("cursor2 = conn_simple.cursor()\n");

#if PY_VERSION_HEX >= 0x02040000
    ExecuteStr("date_val = dbapi.Date(1, 1, 1)\n");
    ExecuteStr("time_val = dbapi.Time(1, 1, 1)\n");
    ExecuteStr("timestamp_val = dbapi.Timestamp(1, 1, 1, 1, 1, 1)\n");
#endif
    ExecuteStr("binary_val = dbapi.Binary('Binary test')\n");

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

BOOST_AUTO_TEST_CASE(TestConnection)
{
    const string connection_args( GetArgs().GetDriverName() + "', '" +
            GetArgs().GetServerTypeStr() + "', '" +
            GetArgs().GetServerName() + "', '" +
            GetArgs().GetDatabaseName() + "', '" +
            GetArgs().GetUserName() + "', '" +
            GetArgs().GetUserPassword() );

    // First test ...
    {
        const string connection_str( "tmp_connection = dbapi.connect('" +
            connection_args +
            "', True)\n");
        const string connection2_str( "tmp_connection2 = dbapi.connect('" +
            connection_args +
            "', True)\n");
        const string connection3_str( "tmp_connection3 = dbapi.connect('" +
            connection_args +
            "', True)\n");


        // Open and close connection ...
        {
            ExecuteStr( connection_str );
            ExecuteStr( "tmp_connection.close()\n" );
        }

        // Open connection, run a query, close connection ...
        {
            ExecuteStr( connection_str );
            ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
            ExecuteStr( "tmp_cursor.execute('SET NOCOUNT ON')\n" );
            ExecuteStr( "tmp_connection.close()\n" );
        }

        // Open multiple connection simulteniously ...
        {
            ExecuteStr( connection_str );
            ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
            ExecuteStr( "tmp_cursor.execute('SELECT @@version')\n" );

            ExecuteStr( connection2_str );
            ExecuteStr( "tmp_cursor2 = tmp_connection2.cursor()\n");
            ExecuteStr( "tmp_cursor2.execute('SELECT @@version')\n" );

            ExecuteStr( connection3_str );
            ExecuteStr( "tmp_cursor3 = tmp_connection3.cursor()\n");
            ExecuteStr( "tmp_cursor3.execute('SELECT @@version')\n" );

            // Execute all statements again ...
            {
                ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
                ExecuteStr( "tmp_cursor.execute('SELECT @@version')\n" );

                ExecuteStr( "tmp_cursor2 = tmp_connection2.cursor()\n");
                ExecuteStr( "tmp_cursor2.execute('SELECT @@version')\n" );

                ExecuteStr( "tmp_cursor3 = tmp_connection3.cursor()\n");
                ExecuteStr( "tmp_cursor3.execute('SELECT @@version')\n" );
            }

            // Close all open connections ...
            ExecuteStr( "tmp_connection.close()\n" );
            ExecuteStr( "tmp_connection2.close()\n" );
            ExecuteStr( "tmp_connection3.close()\n" );
        }
    }

    // Second test ...
    // Test extra-parameters with connection ...
    if (false) {
        const string connection_str( "tmp_connection = dbapi.connect('" +
            connection_args +
            "', {'client_charset':'UTF-8'})\n");

        // Open connection, run a query, close connection ...
        {
            ExecuteStr( connection_str );
            ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
            ExecuteStr( "tmp_cursor.execute('SET NOCOUNT ON')\n" );
            ExecuteStr( "tmp_connection.close()\n" );
        }
    }
}

BOOST_AUTO_TEST_CASE(TestExecute)
{
    // Simple test
    {
        ExecuteStr("cursor = connection.cursor()\n");
        ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
        ExecuteStr("cursor.fetchone()\n");
    }
}

BOOST_AUTO_TEST_CASE(TestFetch)
{
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

BOOST_AUTO_TEST_CASE(TestParameters)
{
    // Prepare ...
    {
        ExecuteStr("cursor = conn_simple.cursor()\n");
    }

    // Very first test ...
    {
        // ExecuteStr("cursor.execute('SELECT name FROM syscolumns WHERE id = 4 and printfmt = @printfmt_par', {'@printfmt_par':None})\n");
        ExecuteStr("cursor.execute('SELECT name FROM syscolumns WHERE id = @id', {'@id':None})\n");

        // fetchall
        ExecuteStr("print cursor.fetchall()\n");
    }

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

    // Test for datetime
    {
        ExecuteStr("cursor.execute('select cast(@dt as datetime)', (datetime.datetime(2010,1,1,12,1,2,50000),)) \n");
        ExecuteStr("dt = cursor.fetchone()[0] \n");
        ExecuteStr("print dt \n");
        ExecuteStr("if dt.year != 2010 or dt.month != 1 or dt.day != 1 "
                   "or dt.hour != 12 or dt.minute != 1 or dt.second != 2 "
                   "or dt.microsecond != 50000: "
                   "raise StandardError('Invalid datetime returned: ' + str(dt)) \n");
    }
}

BOOST_AUTO_TEST_CASE(TestExecuteMany)
{
    // Excute with empty parameter list
    {
        ExecuteStr("sql_ins = 'INSERT INTO #t(vkey) VALUES(@value)' \n");
        ExecuteStr("cursor = conn_simple.cursor()\n");
        ExecuteStr("cursor.executemany(sql_ins, [ {'@value':value} for value in range(1, 11) ]) \n");
        ExecuteStr("cursor.executemany(sql_ins, [ {'value':value} for value in range(1, 11) ]) \n");
    }
}


BOOST_AUTO_TEST_CASE(TestTransaction)
{
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


BOOST_AUTO_TEST_CASE(TestFromFile)
{
    s_Engine->ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample9.py");
}

BOOST_AUTO_TEST_CASE(Test_callproc)
{
    // Prepare ...
    {
        // ExecuteStr("cursor = connection.cursor()\n");
        ExecuteStr("cursor = conn_simple.cursor()\n");
    }

    // Check output parameters ...
    if (false) {
        // ExecuteStr("cursor.callproc('sp_server_info', {'@attribute_id':1}) \n");
        ExecuteStr("print cursor.callproc('SampleProc3', {'@id':1, '@f':2.0, '@o':0}) \n");
        ExecuteStr("print cursor.fetchall()\n");
        ExecuteStr("while cursor.nextset() : \n"
                   "    print cursor.fetchall() "
                );
    }

    if (false) {
        ExecuteStr("print cursor.callproc('DBAPI_Sample..SampleProc3', {'@id':1, '@f':2.0, '@o':0}) \n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("while cursor.nextset() : \n"
                   "    print cursor.fetchall() "
                );
    }

    if (false) {
        ExecuteStr("print cursor.callproc('DBAPI_Sample.dbo.SampleProc3', {'@id':1, '@f':2.0, '@o':0}) \n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("while cursor.nextset() : \n"
                   "    print cursor.fetchall() "
                );
    }

    if (false) {
        ExecuteStr("db_pipe = dbapi.connect('ftds', 'MSSQL','GPIPE_META', 'GPIPE_META', 'anyone', 'allowed') \n");
        ExecuteStr("cursor_pipe = db_pipe.cursor()\n");
        // ExecuteStr("print cursor_pipe.callproc('GPIPE_META.dbo.test', {'@myparam':1}) \n");
        ExecuteStr("print cursor_pipe.callproc('test', {'@myparam':1}) \n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("while cursor.nextset() : \n"
                   "    print cursor.fetchall() "
                );
    }

    {
        ExecuteStr("cursor_test = conn_simple.cursor()\n");
        ExecuteStr("cursor_test.fetchall()\n");
        ExecuteStr("while cursor_test.nextset() : \n"
                   "    print cursor_test.fetchall() "
                );
    }

    // CALL stored procedure ...
    ExecuteStr("print cursor.callproc('sp_databases')\n");
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

    ExecuteStr("print cursor.callproc('sp_server_info')\n");
    ExecuteStr("cursor.fetchall()\n");
    ExecuteStr("rc = cursor.get_proc_return_status()\n");

    // Check output parameters ...
    if (false) {
        // ExecuteStr("cursor.callproc('sp_server_info', {'@attribute_id':1}) \n");
        ExecuteStr("print cursor.callproc('SampleProc3', {'@id':1, '@f':2.0, '@o':0}) \n");
        ExecuteStr("cursor.fetchall()\n");
        ExecuteStr("while cursor.nextset() : \n"
                   "    print cursor.fetchall() "
                );
    }
}


BOOST_AUTO_TEST_CASE(TestExecuteStoredProc)
{
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
    ExecuteStr("cursor.execute('exec sp_server_info 1')\n");
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
    ExecuteStr("cursor.execute('exec sp_server_info 2')\n");
    ExecuteStr("cursor.fetchall()\n");
    ExecuteStr("cursor.fetchall()\n");
    ExecuteStr("cursor.fetchone()\n");
    ExecuteStr("cursor.fetchmany(5)\n");
    ExecuteStr("rc = cursor.get_proc_return_status()\n");
    ExecuteStr("rc = cursor.get_proc_return_status()\n");
}


BOOST_AUTO_TEST_CASE(TestStoredProcByPos)
{
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
    ExecuteStr("cursor.callproc('sp_server_info', [1])\n");
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
    ExecuteStr("cursor.callproc('sp_server_info', [2])\n");
    ExecuteStr("cursor.fetchall()\n");
    ExecuteStr("cursor.fetchall()\n");
    ExecuteStr("cursor.fetchone()\n");
    ExecuteStr("cursor.fetchmany(5)\n");
    ExecuteStr("rc = cursor.get_proc_return_status()\n");
    ExecuteStr("rc = cursor.get_proc_return_status()\n");
}


BOOST_AUTO_TEST_CASE(Test_SelectStmt)
{
    string sql;

    // Prepare ...
    {
        ExecuteStr("cursor = conn_simple.cursor()\n");

        sql =
            "CREATE TABLE #Overlaps ( \n"
            "   pairId int NOT NULL , \n"
            "   overlapNum smallint NOT NULL , \n"
            "   start1 int NOT NULL , \n"
            "   start2 int NOT NULL , \n"
            "   stop1 int NOT NULL , \n"
            "   stop2 int NOT NULL , \n"
            "   orient char (2) NOT NULL , \n"
            "   gaps int NOT NULL , \n"
            "   mismatches int NOT NULL , \n"
            "   adjustedLen int NOT NULL , \n"
            "   length int NOT NULL , \n"
            "   contained tinyint NOT NULL , \n"
            "   seq_align text  NULL , \n"
            "   merged_sa char (1) NOT NULL , \n"
            "   PRIMARY KEY \n"
            "   ( \n"
            "       pairId, \n"
            "       overlapNum \n"
            "   ) \n"
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
        ExecuteStr( sql );
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

BOOST_AUTO_TEST_CASE(Test_LOB)
{
    string sql;

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
        ExecuteStr( sql );

        sql  = "cursor.execute('INSERT INTO #t2(vc1900_field, text_val, image_val) VALUES(@vcv, @tv, @iv)', ";
        sql += " {'@vcv':long_str, '@tv':long_str, '@iv':dbapi.Binary(long_str)} ) \n";
        ExecuteStr( sql );
    }

    // Check ...
    {
        sql = "SELECT vc1900_field, text_val, image_val FROM #t2";
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
        ExecuteStr("if len(record[2]) != len(long_str) : "
                   "raise StandardError('Invalid string size: ') \n"
        );
    }

}

BOOST_AUTO_TEST_CASE(Test_RaiseError)
{
    ExecuteStr("cursor = conn_simple.cursor()\n");
    ExecuteSQL("select @@servername");
    ExecuteStr("cursor.fetchall()\n");
    BOOST_CHECK_THROW(ExecuteSQL( "raiserror 99999 'error msg'\n" ), string);
    ExecuteStr("cursor.fetchall()\n");
}

BOOST_AUTO_TEST_CASE(Test_Exception)
{
//    StandardError
//    |__Warning
//    |__Error
//        |__InterfaceError
//        |__DatabaseError
//            |__DataError
//            |__OperationalError
//            |__IntegrityError
//            |__InternalError
//            |__ProgrammingError
//            |__NotSupportedError


    // Catch exception ...
    {
        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.Warning(\"Oops ...\") \n"
            "except dbapi.Warning, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.Warning(\"Oops ...\") \n"
            "except StandardError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.Error(\"Oops ...\") \n"
            "except dbapi.Error, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.Error(\"Oops ...\") \n"
            "except StandardError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.InterfaceError(\"Oops ...\") \n"
            "except dbapi.InterfaceError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.InterfaceError(\"Oops ...\") \n"
            "except dbapi.Error, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.DatabaseError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.DatabaseError(\"Oops ...\") \n"
            "except dbapi.Error, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   cursor.execute('SELECT * FROM wrong_table') \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), inst.srv_errno, inst.srv_msg \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.DataError(\"Oops ...\") \n"
            "except dbapi.DataError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.DataError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.OperationalError(\"Oops ...\") \n"
            "except dbapi.OperationalError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.OperationalError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.IntegrityError(\"Oops ...\") \n"
            "except dbapi.IntegrityError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.IntegrityError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.InternalError(\"Oops ...\") \n"
            "except dbapi.InternalError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.InternalError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.ProgrammingError(\"Oops ...\") \n"
            "except dbapi.ProgrammingError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.ProgrammingError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );

        ///////////////////////////////
        ExecuteStr(
            "try: \n"
            "   raise dbapi.NotSupportedError(\"Oops ...\") \n"
            "except dbapi.NotSupportedError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
        ExecuteStr(
            "try: \n"
            "   raise dbapi.NotSupportedError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), \"is OK!\" \n"
            "else: \n"
            "   raise \n"
            );
    }

    // Print exception ...
    {
        ExecuteStr(
            "try: \n"
            "   raise dbapi.Warning(\"Oops ...\") \n"
            "except dbapi.Warning, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.Error(\"Oops ...\") \n"
            "except dbapi.Error, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.InterfaceError(\"Oops ...\") \n"
            "except dbapi.InterfaceError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.DatabaseError(\"Oops ...\") \n"
            "except dbapi.DatabaseError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.DataError(\"Oops ...\") \n"
            "except dbapi.DataError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.OperationalError(\"Oops ...\") \n"
            "except dbapi.OperationalError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.IntegrityError(\"Oops ...\") \n"
            "except dbapi.IntegrityError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.InternalError(\"Oops ...\") \n"
            "except dbapi.InternalError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.ProgrammingError(\"Oops ...\") \n"
            "except dbapi.ProgrammingError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );

        ExecuteStr(
            "try: \n"
            "   raise dbapi.NotSupportedError(\"Oops ...\") \n"
            "except dbapi.NotSupportedError, inst: \n"
            "   print type(inst), inst \n"
            "else: \n"
            "   raise \n"
            );
    }
}

// From example8.py
BOOST_AUTO_TEST_CASE(TestScenario_1)
{
    string sql;

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
        ExecuteStr("umonth_list = [u'January', u'February', u'March', u'April', u'May', u'June', u'July', u'August', u'September', u'October', u'November', u'December']");
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
        ExecuteStr("cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in umonth_list])");
        ExecuteSQL("select * from #sale_stat");
        ExecuteStr("print \"We have inserted\", len( cursor.fetchall() ), \"records\"");
        ExecuteSQL("COMMIT TRANSACTION");
        ExecuteSQL("select * from #sale_stat");
        ExecuteStr("print \"After a 'manual' commit command the table contains\", len( cursor.fetchall() ), \"records\"");
        ExecuteSQL("select month from #sale_stat");
        ExecuteStr("month = cursor.fetchone()[0]");
        ExecuteStr("if not isinstance(month, str) : "
                    "raise StandardError('Invalid data type: ') \n");
        ExecuteStr("dbapi.return_strs_as_unicode(True)");
        ExecuteSQL("select month from #sale_stat");
        ExecuteStr("month = cursor.fetchone()[0]");
        ExecuteStr("if not isinstance(month, unicode) : "
            "raise StandardError('Invalid data type: ') \n");
        ExecuteStr("dbapi.return_strs_as_unicode(False)");
        ExecuteSQL("select month from #sale_stat");
        ExecuteStr("month = cursor.fetchone()[0]");
        ExecuteStr("if not isinstance(month, str) : "
            "raise StandardError('Invalid data type: ') \n");
        // Test checking that nulls are bound to output parameters as values
        // with actual type with which that parameter is declared.
        // Disabled because it needs procedure to be created separately.
        // Procedure should be as following:
        //   CREATE PROCEDURE testing (@p1 int, @p2 int OUTPUT) AS
        //   begin
        //     set @p1 = 123
        //     set @p2 = 123
        //   end
        //ExecuteStr("out = cursor.callproc('testing', [None, None])");
        //ExecuteStr("print out");
        //ExecuteStr("if isinstance(out[1], str) : "
        //    "raise StandardError('Invalid data type: ') \n");
        //ExecuteStr("if not isinstance(out[0], None.__class__) : "
        //    "raise StandardError('Invalid data type: ') \n");
        ExecuteStr("from __future__ import with_statement \n"
                   "with conn_simple.cursor() as cursor: \n"
                   "  for row in cursor.execute('exec sp_spaceused'): \n"
                   "    print row \n"
                   "  cursor.nextset() \n"
                   "  for row in cursor: \n"
                   "    print row \n"
                   "try: \n"
                   "  cursor.execute('select * from sysobjects') \n"
                   "  raise Exception('DatabseError was not thrown') \n"
                   "except dbapi.DatabaseError: \n"
                   "  pass \n");
        ExecuteStr("cursor = conn_simple.cursor()\n" );
        ExecuteStr("for row in cursor.execute('exec sp_spaceused'): \n"
                   "  print row \n");
    }
}


BOOST_AUTO_TEST_CASE(TestScenario_1_ByPos)
{
    string sql;

    // Prepare ...
    {
        ExecuteStr( "cursor = conn_simple.cursor()\n" );
    }

    // Create a table ...
    {
        sql = " CREATE TABLE #sale_stat2 ( \n"
            " year INT NOT NULL, \n"
            " month VARCHAR(255) NOT NULL, \n"
            " stat INT NOT NULL \n"
            " ) ";
        ExecuteSQL(sql);
    }

    // Insert data ..
    {
        ExecuteStr("month_list = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']");
        ExecuteStr("umonth_list = [u'January', u'February', u'March', u'April', u'May', u'June', u'July', u'August', u'September', u'October', u'November', u'December']");
        ExecuteStr("sql = \"insert into #sale_stat2(year, month, stat) values (@year, @month, @stat)\"");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"Empty table contains\", len( cursor.fetchall() ), \"records\"");
        ExecuteSQL("BEGIN TRANSACTION");
        ExecuteStr("cursor.executemany(sql, [[year, month, stat] for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"We have inserted\", len( cursor.fetchall() ), \"records\"");
        ExecuteStr("conn_simple.rollback();");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"After a 'standard' rollback command the table contains\", len( cursor.fetchall() ), \"records\"");
        ExecuteSQL("ROLLBACK TRANSACTION");
        ExecuteSQL("BEGIN TRANSACTION");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"After a 'manual' rollback command the table contains\", len( cursor.fetchall() ), \"records\"");
        ExecuteStr("cursor.executemany(sql, [[year, month, stat] for stat in range(1, 3) for year in range(2004, 2006) for month in umonth_list])");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"We have inserted\", len( cursor.fetchall() ), \"records\"");
        ExecuteSQL("COMMIT TRANSACTION");
        ExecuteSQL("select * from #sale_stat2");
        ExecuteStr("print \"After a 'manual' commit command the table contains\", len( cursor.fetchall() ), \"records\"");
    }
}


// From example4.py
BOOST_AUTO_TEST_CASE(TestScenario_2)
{
    ExecuteStr("conn = None");

    ExecuteStr( "def getCon(): \n"
                "    global conn \n"
                "    return conn"
                );

    ExecuteStr( "def CreateSchema(): \n"
                "        cu = getCon().cursor() \n"
                "        cu.execute(\"SELECT name from sysobjects "
                "           WHERE name = 'customers' AND type = 'U'\") \n"
                "        if len(cu.fetchall()) > 0: \n"
                "                cu.execute(\"DROP TABLE customers\") \n"
                "        cu.execute(\"\"\" \n"
                "               CREATE TABLE customers ( \n"
                "                       cust_name VARCHAR(255) NOT NULL \n"
                "               ) \n"
                "               \"\"\") \n"
                "        getCon().commit() \n"
            );

    ExecuteStr("def GetCustomers(): \n"
                "        cu = getCon().cursor() \n"
                "        cu.execute(\"select * from customers\") \n"
                "        print cu.fetchall() \n"
        );

    ExecuteStr("def DeleteCustomers(): \n"
                "        cu = getCon().cursor() \n"
                "        cu.execute(\"delete from customers\") \n"
                "        getCon().commit() \n"
        );

    ExecuteStr("def CreateCustomers(): \n"
                "        cu = getCon().cursor() \n"
                "        sql = \"insert into customers(cust_name) values (@name)\" \n"
                "        cu.execute(sql, {'@name':'1111'}) \n"
                "        cu.execute(sql, {'@name':'2222'}) \n"
                "        cu.execute(sql, {'@name':'3333'}) \n"
                "        getCon().rollback() \n"
                "        cu.execute(sql, {'@name':'Jane'}) \n"
                "        cu.execute(sql, {'@name':'Doe'}) \n"
                "        cu.execute(sql, {'@name':'Scott'}) \n"
                "        getCon().commit() \n"
        );

    ExecuteStr("def TestScenario_2(): \n"
                "        global conn \n"
                "        conn = dbapi.connect('ftds', 'MSSQL', 'MSDEV1', 'DBAPI_Sample', 'DBAPI_test', 'allowed', True) \n"
                "        CreateSchema() \n"
                "        CreateCustomers() \n"
                "        GetCustomers() \n"
                "        DeleteCustomers() \n"
        );

    ExecuteStr("TestScenario_2()");
}


///////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(void)
{
    const CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app) {
        return;
    }

    const CArgs& args = app->GetArgs();
    // Get command-line arguments ...
    m_DriverName    = args["dr"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();

    SetDatabaseParameters();
}

CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    switch (CCPPToolkitConnParams::GetServerType(GetServerName())) {
    case CDBConnParams::eSybaseSQLServer:
    case CDBConnParams::eSybaseOpenServer:
        return eSybase;
    case CDBConnParams::eMSSqlServer:
        return eMsSql;
    default:
        return eUnknown;
    }
}

void
CTestArguments::SetDatabaseParameters(void)
{
    if (GetServerType() == eSybase) {
        if ( GetDriverName() == "dblib") {
                // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
                // BcpIn to it using protocol version other than "100".
                m_DatabaseParameters["version"] = "100";
        } else if ( (GetDriverName() == "ftds")) {
                m_DatabaseParameters["version"] = "42";
        }
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
    default :
        return "none";
    }
}

END_NCBI_SCOPE
