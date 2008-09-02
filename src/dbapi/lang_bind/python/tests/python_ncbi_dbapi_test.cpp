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
#include <corelib/ncbifile.hpp>
#include <corelib/plugin_manager.hpp>

#include "python_ncbi_dbapi_test.hpp"
#include <dbapi/driver/impl/dbapi_impl_context.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
CPythonDBAPITest::CPythonDBAPITest(const CTestArguments& args)
: m_Engine(new pythonpp::CEngine)
, m_args(args)
{
}

CPythonDBAPITest::~CPythonDBAPITest(void)
{
    // delete m_Engine;
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
        CPluginManager_DllResolver::EnableGlobally(true);
        string connection_args( m_args.GetDriverName() + "', '" +
                                m_args.GetServerTypeStr() + "', '" +
                                m_args.GetServerName() + "', '" +
                                m_args.GetDatabaseName() + "', '" +
                                m_args.GetUserName() + "', '" +
                                m_args.GetUserPassword() );

        string connection_str( "connection = dbapi.connect('" +
                                connection_args +
                                "', True)\n");
        string conn_simple_str( "conn_simple = dbapi.connect('" +
                                connection_args +
                                "')\n");

        ExecuteStr("import python_ncbi_dbapi as dbapi\n");
        ExecuteStr( connection_str.c_str() );
        ExecuteStr( conn_simple_str.c_str() );

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
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestBasic(void)
{
    try {
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
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void
CPythonDBAPITest::TestConnection(void)
{
    const string connection_args( m_args.GetDriverName() + "', '" +
            m_args.GetServerTypeStr() + "', '" +
            m_args.GetServerName() + "', '" +
            m_args.GetDatabaseName() + "', '" +
            m_args.GetUserName() + "', '" +
            m_args.GetUserPassword() );

    try {
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
                ExecuteStr( connection_str.c_str() );
                ExecuteStr( "tmp_connection.close()\n" );
            }

            // Open connection, run a query, close connection ...
            {
                ExecuteStr( connection_str.c_str() );
                ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
                ExecuteStr( "tmp_cursor.execute('SET NOCOUNT ON')\n" );
                ExecuteStr( "tmp_connection.close()\n" );
            }

            // Open multiple connection simulteniously ...
            {
                ExecuteStr( connection_str.c_str() );
                ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
                ExecuteStr( "tmp_cursor.execute('SELECT @@version')\n" );

                ExecuteStr( connection2_str.c_str() );
                ExecuteStr( "tmp_cursor2 = tmp_connection2.cursor()\n");
                ExecuteStr( "tmp_cursor2.execute('SELECT @@version')\n" );

                ExecuteStr( connection3_str.c_str() );
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
                ExecuteStr( connection_str.c_str() );
                ExecuteStr( "tmp_cursor = tmp_connection.cursor()\n");
                ExecuteStr( "tmp_cursor.execute('SET NOCOUNT ON')\n" );
                ExecuteStr( "tmp_connection.close()\n" );
            }
        }
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

            sql  = "cursor.execute('INSERT INTO #t2(vc1900_field, text_val, image_val) VALUES(@vcv, @tv, @iv)', ";
            sql += " {'@vcv':long_str, '@tv':long_str, '@iv':dbapi.Binary(long_str)} ) \n";
            ExecuteStr( sql.c_str() );
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
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }

}

void 
CPythonDBAPITest::Test_RaiseError(void)
{
    try {
        ExecuteStr("cursor = conn_simple.cursor()\n");
        ExecuteSQL("select @@servername");
        ExecuteStr("cursor.fetchall()\n");

        try {
            ExecuteSQL( "raiserror 99999 'error msg'\n" );
            BOOST_CHECK(false);
        } catch (const string&) {
            ;
        }

        ExecuteStr("cursor.fetchall()\n");
    }
    catch ( const string& ex ) {
        BOOST_FAIL( ex );
    }
}

void 
CPythonDBAPITest::Test_Exception(void)
{
    try {
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
    catch ( const string& ex ) {
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


// From example4.py
void
CPythonDBAPITest::TestScenario_2(void)
{
    string sql;

    try {
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
                        "        conn = dbapi.connect('ftds', 'MSSQL', 'MSDEV1', 'DBAPI_Sample', 'anyone', 'allowed', True) \n"
                        "        CreateSchema() \n"
                        "        CreateCustomers() \n"
                        "        GetCustomers() \n"
                        "        DeleteCustomers() \n"
                );

            ExecuteStr("TestScenario_2()");
    }
    catch( const string& ex ) {
        BOOST_FAIL( ex );
    }
}


static
string GetSybaseClientVersion(void)
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


///////////////////////////////////////////////////////////////////////////
CPythonDBAPITestSuite::CPythonDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
//     bool Solaris = false;
    bool sybase_client_v125 = false;

// #if defined(NCBI_OS_SOLARIS)
//     Solaris = true;
// #endif

    const string sybase_version = GetSybaseClientVersion();
    if (NStr::CompareNocase(sybase_version, 0, 4, "12.5") == 0
        || NStr::CompareNocase(sybase_version, "current") == 0) {
        sybase_client_v125 = true;
    }

    // add member function test cases to a test suite
    boost::shared_ptr<CPythonDBAPITest> DBAPIInstance( new CPythonDBAPITest( args ) );
    boost::unit_test::test_case* tc = NULL;

    boost::unit_test::test_case* tc_init =
        BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::MakeTestPreparation, DBAPIInstance);

    add(tc_init);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestConnection, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::Test_RaiseError, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::Test_Exception, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    if ( ( args.GetDriverName() == "ctlib" && sybase_client_v125) ||
         ( (args.GetDriverName() == "ftds"
            // || args.GetDriverName() == "ftds8" // 05/22/08 // Stoped working
            || args.GetDriverName() == "ftds63"
            || args.GetDriverName() == "ftds_odbc"
            ) &&
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
    if ( server_name.compare("ODBC") != 0
         && server_name.compare("ODBCW") != 0
         && server_name.compare("FTDS_ODBC") != 0
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestExecuteStoredProc, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestScenario_1, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CPythonDBAPITest::TestScenario_2, DBAPIInstance);
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
                              "python_ncbi_dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MSDEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "msdblib", "odbc", \
                      "odbcw", "gateway", "ftds8", "ftds_odbc"
#else
#define DEF_SERVER    "THALBERG"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "gateway", \
                      "ftds8", "ftds_odbc"
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
    if ( NStr::CompareNocase(GetServerName(), "STRAUSS") == 0
         || NStr::CompareNocase(GetServerName(), "MOZART") == 0
         || NStr::CompareNocase(GetServerName(), "OBERON") == 0
         || NStr::CompareNocase(GetServerName(), "TAPER") == 0
         || NStr::CompareNocase(GetServerName(), "THALBERG") == 0
         || NStr::CompareNocase(GetServerName(), "SCHUMANN") == 0
         || NStr::CompareNocase(GetServerName(), "CLEMENTI") == 0
         || NStr::CompareNocase(GetServerName(), "BARTOK") == 0
         || NStr::CompareNocase(GetServerName(), "SYB_TEST") == 0
	 || NStr::StartsWith(GetServerName(), "DBAPI_DEV")
         ) {
        return eSybase;
    } else if ( NStr::StartsWith(GetServerName(), "MS_DEV")
                || NStr::StartsWith(GetServerName(), "MSSQL")
                || NStr::StartsWith(GetServerName(), "MSDEV")
                || NStr::StartsWith(GetServerName(), "OAMSDEV")
                || NStr::StartsWith(GetServerName(), "QMSSQL")
		|| NStr::CompareNocase(GetServerName(), "MS_TEST") == 0
                ) {
        return eMsSql;
    }

    return eUnknown;
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
		} else if ( (GetDriverName() == "ftds8")) {
				m_DatabaseParameters["version"] = "42";
		} else if (GetDriverName() == "ftds63" || GetDriverName() == "ftds64_dblib") {
				// ftds work with Sybase databases using protocol v42 only ...
				m_DatabaseParameters["version"] = "100";
		} else if (GetDriverName() == "ftds_odbc") {
				m_DatabaseParameters["version"] = "50";
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

