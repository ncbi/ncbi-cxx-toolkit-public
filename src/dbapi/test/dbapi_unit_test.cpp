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

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <dbapi/dbapi.hpp>

#include "dbapi_unit_test.hpp"

BEGIN_NCBI_SCOPE

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( CDBAPIUnitTest );


CTestTransaction::CTestTransaction(
    IConnection& conn,
    ETransBehavior tb
    )
    : m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        m_Stmt.reset( conn.CreateStatement() );
        m_Stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            m_Stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            m_Stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}
///////////////////////////////////////////////////////////////////////////////
CDBAPIUnitTest::CDBAPIUnitTest()
: m_DM( CDriverManager::GetInstance() )
, m_DS( NULL )
, m_TableName( "#dbapi_unit_test" )
{
}

void
CDBAPIUnitTest::setUp()
{
    if ( m_DS == NULL ) {
        m_DS = m_DM.CreateDs( "ftds" );
    }
    m_Stmt.release();
    m_Conn.reset( m_DS->CreateConnection() );
    m_Conn->Connect( "anyone", "allowed", "MS_DEV1", "DBAPI_Sample" );
    m_Stmt.reset( m_Conn->CreateStatement() );

    // Create a test table ...
    string sql;

    sql  = " CREATE TABLE " + m_TableName + "( \n";
    sql += "    int_val INT NOT NULL \n";
    sql += " )";

    // Create the table
    m_Stmt->ExecuteUpdate(sql);
}


void
CDBAPIUnitTest::tearDown()
{
}


void
CDBAPIUnitTest::TestGetRowCount()
{
    for ( int i = 0; i < 5; ++i ) {
        CheckGetRowCount( i, eNoTrans );
        CheckGetRowCount( i, eTransCommit );
        CheckGetRowCount( i, eTransRollback );
    }
}

void
CDBAPIUnitTest::CheckGetRowCount(int row_count, ETransBehavior tb)
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    string sql;
    sql  = " INSERT INTO " + m_TableName + "(int_val) VALUES(1) \n";

    // Insert row_count records into the table ...
    for ( size_t i = 0; i < row_count; ++i ) {
        m_Stmt->ExecuteUpdate(sql);
        int nRows = m_Stmt->GetRowCount();
        CPPUNIT_ASSERT_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        sql  = " SELECT * FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        sql  = " UPDATE " + m_TableName + " SET int_val = 0 ";
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        sql  = " SELECT * FROM " + m_TableName + " WHERE int_val = 0";
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        sql  = " DELETE FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        sql  = " SELECT * FROM " + m_TableName;
        m_Stmt->ExecuteUpdate(sql);

        int nRows = m_Stmt->GetRowCount();

        CPPUNIT_ASSERT_EQUAL( 0, nRows );
    }

}

END_NCBI_SCOPE


int
main(int argc, char* argv[])
{
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
 * Revision 1.2  2005/02/11 16:12:02  ssikorsk
 * Improved GetRowCount test
 *
 * Revision 1.1  2005/02/04 17:25:02  ssikorsk
 * Renamed dbapi-unit-test to dbapi_unit_test.
 * Added dbapi_unit_test to the test suite.
 *
 * Revision 1.1  2005/02/03 16:06:46  ssikorsk
 * Added: initial version of a cppunit test for the DBAPI
 *
 *
 * ===========================================================================
 */
