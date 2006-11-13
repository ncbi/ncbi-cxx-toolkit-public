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

#ifndef DBAPI_UNIT_TEST_H
#define DBAPI_UNIT_TEST_H

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;


BEGIN_NCBI_SCOPE

// Forward declaration
class CDriverManager;
class IDataSource;
class IConnection;
class IStatement;

enum ETransBehavior { eNoTrans, eTransCommit, eTransRollback };

class CTestTransaction
{
public:
    CTestTransaction(IConnection& conn, ETransBehavior tb = eTransCommit);
    ~CTestTransaction(void);

private:
    IConnection* m_Conn;
    ETransBehavior  m_TransBehavior;
};

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

    EServerType GetServerType(void) const;

    string GetProgramBasename(void) const;


private:
    void SetDatabaseParameters(void);

private:
    CNcbiArguments m_Arguments;
    string m_DriverName;
    string m_ServerName;
    string m_UserName;
    string m_UserPassword;
    string m_DatabaseName;
    string m_TDSVersion;
    TDatabaseParameters m_DatabaseParameters;
};



///////////////////////////////////////////////////////////////////////////
class CDBAPIUnitTest
{
public:
    CDBAPIUnitTest(const CTestArguments& args);
    ~CDBAPIUnitTest(void);

public:
    void TestInit(void);

public:
    // Test IStatement interface.

    // Testing Approach for value wrappers
    void Test_Variant(void);
    void Test_Variant2(void);

    void Test_CDB_Exception(void);

    // Testing Approach for Members
    // Test particular methods.
    void Test_GetRowCount();
    void CheckGetRowCount(
        int row_count,
        ETransBehavior tb = eNoTrans,
        IStatement* stmt = NULL);
    void CheckGetRowCount2(
        int row_count,
        ETransBehavior tb = eNoTrans,
        IStatement* stmt = NULL);

    void Test_StatementParameters(void);
    void Test_UserErrorHandler(void);
    void Test_NULL();

    void Test_SelectStmt(void);
    void Test_SelectStmtXML(void);
    void Test_Cursor(void);
    void Test_Cursor2(void);
    void Test_Procedure(void);
    void Test_Bulk_Writing(void);
    void Test_Bulk_Overflow(void);
    void Test_GetTotalColumns(void);
    void Test_LOB(void);
    void Test_LOB2(void);
    void Test_BlobStream(void);
    void Test_BulkInsertBlob(void);
    void Test_UNIQUE(void);
    void Test_DateTime(void);
    void Test_Insert(void);
    void Test_HasMoreResults(void);
    void Test_Create_Destroy(void);
    void Multiple_Close(void);
    void Test_Unicode(void);
    void Test_Iskhakov(void);
    void Test_NCBI_LS(void);
    void Test_Authentication(void);

public:
    void Test_Exception_Safety(void);
    void Test_ES_01(IConnection& conn);

public:
    // Not implemented yet ...
    void Test_Bind(void);
    void Test_Execute(void);

    void Test_Exception(void);

    // Test scenarios.
    void Repeated_Usage(void);
    void Single_Value_Writing(void);
    void Single_Value_Reading(void);
    void Bulk_Reading(void);
    void Multiple_Resultset(void);
    void Query_Cancelation(void);
    void Error_Conditions(void);
    void Transactional_Behavior(void);

protected:
    const string& GetTableName(void) const
    {
        return m_TableName;
    }
    static void DumpResults(IStatement* const stmt);
    static int GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
                               const string& table_name);
    static int GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt);
    void Connect(const auto_ptr<IConnection>& conn) const;

private:
    const CTestArguments    m_args;
    CDriverManager&         m_DM;
    IDataSource*            m_DS;
    auto_ptr<IConnection>   m_Conn;

    const string            m_TableName;
    unsigned int            m_max_varchar_size;
};

///////////////////////////////////////////////////////////////////////////
struct CDBAPITestSuite : public test_suite
{
    CDBAPITestSuite(const CTestArguments& args);
    ~CDBAPITestSuite(void);
};

END_NCBI_SCOPE

#endif  // DBAPI_UNIT_TEST_H

/* ===========================================================================
 *
 * $Log$
 * Revision 1.47  2006/11/13 19:56:28  ssikorsk
 * Added Test_Authentication.
 *
 * Revision 1.46  2006/10/18 18:38:36  ssikorsk
 * + Test_NCBI_LS
 *
 * Revision 1.45  2006/10/12 19:24:42  ssikorsk
 * + Test_Iskhakov
 *
 * Revision 1.44  2006/10/11 14:36:55  ssikorsk
 * 			       Added Test_NULL() to the test-suite.
 *
 * Revision 1.43  2006/09/13 20:01:00  ssikorsk
 * Added method Connect to CDBAPIUnitTest.
 *
 * Revision 1.42  2006/09/12 15:02:57  ssikorsk
 * Added Test_CDB_Exception.
 *
 * Revision 1.41  2006/08/24 17:00:24  ssikorsk
 * + Test_Unicode
 *
 * Revision 1.40  2006/08/21 18:19:35  ssikorsk
 * Added class CODBCErrHandler;
 * Added tests Test_Cursor2 and Test_LOB2;
 *
 * Revision 1.39  2006/06/14 19:35:39  ssikorsk
 * Added Create_Destroy and Multiple_Close tests.
 *
 * Revision 1.38  2006/05/30 16:32:33  ssikorsk
 *  Made CNcbiArguments a member of CTestArguments.
 *
 * Revision 1.37  2006/05/10 16:20:05  ssikorsk
 * Added class CErrHandler
 *
 * Revision 1.36  2006/04/28 15:34:37  ssikorsk
 * Added Test_BulkInsertBlob
 *
 * Revision 1.35  2006/02/22 17:11:11  ssikorsk
 * Added Test_HasMoreResults to the test-suite.
 *
 * Revision 1.34  2006/02/22 15:29:29  ssikorsk
 * Renamed Test_GetColumnNo to Test_GetTotalColumns.
 *
 * Revision 1.33  2006/02/14 18:43:41  ucko
 * Declare ~CDBAPIUnitTest, which has acquired a (trivial) definition.
 *
 * Revision 1.32  2006/02/07 16:30:52  ssikorsk
 * Added Test_BlobStream to the test-suite.
 *
 * Revision 1.31  2006/01/26 17:51:12  ssikorsk
 * Added method GetNumOfRecords.
 *
 * Revision 1.30  2005/10/26 11:31:08  ssikorsk
 * Added CTestArguments::m_TDSVersion
 *
 * Revision 1.29  2005/10/03 12:19:06  ssikorsk
 * Added a Test_Insert test to the test-suite.
 *
 * Revision 1.28  2005/09/22 10:48:59  ssikorsk
 * Added Test_Bulk_Overflow test to the test-suite
 *
 * Revision 1.27  2005/09/21 13:48:17  ssikorsk
 * Added Test_DateTime test to the test-suite.
 *
 * Revision 1.26  2005/09/16 16:59:11  ssikorsk
 * + Test_UNIQUE test
 *
 * Revision 1.25  2005/09/13 14:47:35  ssikorsk
 * Added a Test_LOB test to the test-suite
 *
 * Revision 1.24  2005/09/07 11:08:19  ssikorsk
 * Added Test_GetColumnNo to the test-suite
 *
 * Revision 1.23  2005/08/29 16:07:23  ssikorsk
 * Adapted Bulk_Writing for Sybase.
 *
 * Revision 1.22  2005/08/17 18:05:47  ssikorsk
 * Added initial tests for BulkInsert with INT and BIGINT datatypes
 *
 * Revision 1.21  2005/08/15 18:56:56  ssikorsk
 * Added Test_SelectStmtXML to the test-suite
 *
 * Revision 1.20  2005/08/12 15:46:43  ssikorsk
 * Added an initial bulk test to the test suite.
 *
 * Revision 1.19  2005/08/10 16:56:50  ssikorsk
 * Added Test_Variant2 to the test-suite
 *
 * Revision 1.18  2005/08/09 16:09:40  ssikorsk
 * Added the 'Test_Cursor' test to the test-suite
 *
 * Revision 1.17  2005/08/09 13:14:42  ssikorsk
 * Added a 'Test_Procedure' test to the test-suite
 *
 * Revision 1.16  2005/07/11 11:13:02  ssikorsk
 * Added a 'TestSelect' test to the test-suite
 *
 * Revision 1.15  2005/05/12 18:42:57  ssikorsk
 * Improved the "Test_UserErrorHandler" test-case
 *
 * Revision 1.14  2005/05/12 15:33:34  ssikorsk
 * initial version of Test_UserErrorHandler
 *
 * Revision 1.13  2005/05/05 20:28:34  ucko
 * Remove redundant CTestArguments:: when declaring SetDatabaseParameters.
 *
 * Revision 1.12  2005/05/05 15:09:21  ssikorsk
 * Moved from CPPUNIT to Boost.Test
 *
 * Revision 1.11  2005/04/12 18:12:10  ssikorsk
 * Added SetAutoClearInParams and IsAutoClearInParams functions to IStatement
 *
 * Revision 1.10  2005/04/11 14:13:15  ssikorsk
 * Explicitly clean a parameter list after Execute (because of the ctlib driver)
 *
 * Revision 1.9  2005/04/07 20:29:12  ssikorsk
 * Added more dedicated statements to each test
 *
 * Revision 1.8  2005/04/07 19:16:03  ssikorsk
 *
 * Added two different test patterns:
 * 1) New/dedicated statement for each test;
 * 2) Reusable statement for all tests;
 *
 * Revision 1.7  2005/04/07 14:07:16  ssikorsk
 * Added CheckGetRowCount2
 *
 * Revision 1.6  2005/02/16 21:46:40  ssikorsk
 * Improved CVariant test
 *
 * Revision 1.5  2005/02/16 20:01:20  ssikorsk
 * Added CVariant test
 *
 * Revision 1.4  2005/02/15 17:32:29  ssikorsk
 * Added  TDS "version" parameter with database connection
 *
 * Revision 1.3  2005/02/15 16:06:24  ssikorsk
 * Added driver and server parameters to the test-suite (handled via CNcbiApplication)
 *
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
 * ===========================================================================
 */
