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

#ifndef PYTHON_NCBI_DBAPI_TEST_H
#define PYTHON_NCBI_DBAPI_TEST_H

#include <cppunit/extensions/HelperMacros.h>
#include "../pythonpp/pythonpp_emb.hpp"

BEGIN_NCBI_SCOPE

class CPythonDBAPITest : public CPPUNIT_NS::TestFixture
{
  CPPUNIT_TEST_SUITE( CPythonDBAPITest );
  CPPUNIT_TEST( TestBasic );
  CPPUNIT_TEST( TestExecute );
  CPPUNIT_TEST( TestFetch );
  CPPUNIT_TEST( TestParameters );
  // CPPUNIT_TEST( TestExecuteMany );
  CPPUNIT_TEST( TestTransaction );
  CPPUNIT_TEST( TestFromFile );
  CPPUNIT_TEST_SUITE_END();

public:
    CPythonDBAPITest();

public:
  void setUp();
  void tearDown();

public:
    // Test IStatement interface.

    // Test particular methods.
    void TestBasic();
    void TestExecute();
    void TestFetch();
    void TestParameters();
    void TestExecuteMany();

    // Test scenarios.
    void TestTransaction();

    // Run a Python script from a file
    void TestFromFile();

private:
    void CreateTestTable();

private:
    pythonpp::CEngine m_Engine;
};

END_NCBI_SCOPE

#endif  // PYTHON_NCBI_DBAPI_TEST_H

/* ===========================================================================
 *
 * $Log$
 * Revision 1.2  2005/02/08 19:21:18  ssikorsk
 * + Test "rowcount" attribute and support hte "simple mode" interface
 *
 * Revision 1.1  2005/02/03 16:11:16  ssikorsk
 * python_ncbi_dbapi_test was adapted to the cppunit testing framework
 *
 *
 * ===========================================================================
 */
