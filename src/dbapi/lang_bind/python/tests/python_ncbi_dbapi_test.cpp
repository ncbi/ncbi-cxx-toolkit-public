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

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( CPythonDBAPITest );


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
CPythonDBAPITest::TestBasic()
{
    // m_Engine.ExecuteStr("import python_ncbi_dbapi\n");

    m_Engine.ExecuteStr("version = python_ncbi_dbapi.__version__ \n");
    m_Engine.ExecuteStr("apilevel = python_ncbi_dbapi.apilevel \n");
    m_Engine.ExecuteStr("threadsafety = python_ncbi_dbapi.threadsafety \n");
    m_Engine.ExecuteStr("paramstyle = python_ncbi_dbapi.paramstyle \n");

    m_Engine.ExecuteStr("connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed', True)\n");
    m_Engine.ExecuteStr("conn_simple = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV2', '', 'anyone', 'allowed')\n");

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
        m_Engine.ExecuteStr("cursor = connection.cursor()\n");
        m_Engine.ExecuteStr("cursor.executemany('select qq = 57 + 33', [])\n");
        m_Engine.ExecuteStr("cursor.fetchone()\n");
    }
}


void 
CPythonDBAPITest::TestTransaction()
{
    m_Engine.ExecuteStr("cursor = conn_simple.cursor()\n");
    m_Engine.ExecuteStr("cursor.execute('CREATE TABLE #t ( vkey int)')\n");
    m_Engine.ExecuteStr("cursor.execute('SELECT * FROM #t')\n");
    m_Engine.ExecuteStr("cursor.fetchall()\n");
}


void 
CPythonDBAPITest::TestFromFile()
{
}


void 
CPythonDBAPITest::CreateTestTable()
{
}


END_NCBI_SCOPE

int
main(int argc, char *argv[])
{
    try {
        // Execute some Python statements (in module __main__)

        // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample1.py");
        // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample5.py");
        // m_Engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\tests\\python_ncbi_dbapi_test.py");

    } catch(...)
    {
	    return 1;
    }

	// return 0;




  // Get the top level suite from the registry
  CPPUNIT_NS::Test *suite = CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest();

  // Adds the test to the list of test to run
  CPPUNIT_NS::TextUi::TestRunner runner;
  runner.addTest( suite );

  // Change the default outputter to a compiler error format outputter
  runner.setOutputter( new CPPUNIT_NS::CompilerOutputter( &runner.result(),
                                                       std::cerr ) );
  // Run the test.
  bool wasSucessful = runner.run();

  // Return error code 1 if the one of test failed.
  return wasSucessful ? 0 : 1;
}

/* ===========================================================================
*
* $Log$
* Revision 1.4  2005/02/08 19:21:18  ssikorsk
* + Test "rowcount" attribute and support hte "simple mode" interface
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
