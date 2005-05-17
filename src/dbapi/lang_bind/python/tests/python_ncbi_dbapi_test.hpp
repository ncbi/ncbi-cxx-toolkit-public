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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <boost/test/unit_test.hpp>

#include "../pythonpp/pythonpp_emb.hpp"

using boost::unit_test_framework::test_suite;

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////
class CTestArguments
{
public:
    CTestArguments(int argc, char * argv[]);

public:
    typedef map<string, string> TDatabaseParameters;

    enum EServerType {
        eUnknown,   //< Server type is not known
        eSybase,    //< Sybase server
        eMsSql,     //< Microsoft SQL server
        eMySql,     //< MySql server
        eSqlite,    //< Sqlite server
        eOracle     //< ORACLE server
    };

    string GetDriverName(void) const
    {
        return m_DriverName;
    }
    
    string GetServerName(void) const
    {
        return m_ServerName;
    }
    
    string GetUserName(void) const
    {
        return m_UserName;
    }
    
    string GetUserPassword(void) const
    {
        return m_UserPassword;
    }

    const TDatabaseParameters& GetDBParameters(void) const
    {
        return m_DatabaseParameters;
    }

    string GetDatabaseName(void) const
    {
        return m_DatabaseName;
    }
    
    string GetServerTypeStr(void) const;

private:
    EServerType GetServerType(void) const;
    void SetDatabaseParameters(void);

private:

    string m_DriverName;
    string m_ServerName;
    string m_UserName;
    string m_UserPassword;
    string m_DatabaseName;
    TDatabaseParameters m_DatabaseParameters;
};


class CPythonDBAPITest 
{
public:
    CPythonDBAPITest(const CTestArguments& args);

public:
    // Test IStatement interface.

    // Test particular methods.
    void MakeTestPreparation();
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
    const CTestArguments m_args;
};

///////////////////////////////////////////////////////////////////////////
struct CPythonDBAPITestSuite : public test_suite
{
    CPythonDBAPITestSuite(const CTestArguments& args);
    ~CPythonDBAPITestSuite(void);
};

END_NCBI_SCOPE

#endif  // PYTHON_NCBI_DBAPI_TEST_H

/* ===========================================================================
 *
 * $Log$
 * Revision 1.6  2005/05/17 16:42:34  ssikorsk
 * Moved on Boost.Test
 *
 * Revision 1.5  2005/03/01 15:22:58  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.4  2005/02/17 14:03:36  ssikorsk
 * Added database parameters to the unit-test (handled via CNcbiApplication)
 *
 * Revision 1.3  2005/02/10 20:13:48  ssikorsk
 * Improved 'simple mode' test
 *
 * Revision 1.2  2005/02/08 19:21:18  ssikorsk
 * + Test "rowcount" attribute and support hte "simple mode" interface
 *
 * Revision 1.1  2005/02/03 16:11:16  ssikorsk
 * python_ncbi_dbapi_test was adapted to the cppunit testing framework
 *
 *
 * ===========================================================================
 */
