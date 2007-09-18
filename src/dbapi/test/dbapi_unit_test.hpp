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

#include <corelib/impl/ncbi_dbsvcmapper.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;


BEGIN_NCBI_SCOPE

// Forward declaration
class CDriverManager;
class IDataSource;
class IConnection;
class IStatement;
class IDBConnectionFactory;

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
        eMsSql2005, //< Microsoft SQL server 2005
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

    bool IsBCPAvailable(void) const;

    bool UseGateway(void) const
    {
        return !m_GatewayHost.empty();
    }


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
    string m_GatewayHost;
    string m_GatewayPort;
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
    void Test_CDB_Object(void);
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
    // User error handler life-time ...
    void Test_UserErrorHandler_LT(void);
    void Test_NULL();

    void Test_SelectStmt(void);
    void Test_SelectStmtXML(void);
    void Test_Recordset(void);
    void Test_MetaData(void);
    void Test_Cursor(void);
    void Test_Cursor2(void);
    void Test_Procedure(void);
    void Test_Bulk_Writing(void);
    void Test_Bulk_Writing2(void);
    void Test_Bulk_Overflow(void);
    void Test_GetTotalColumns(void);
    void Test_LOB(void);
    void Test_LOB_LowLevel(void);
    void Test_LOB2(void);
    void Test_BlobStream(void);
    void Test_BulkInsertBlob(void);
    void Test_BulkInsertBlob_LowLevel(void);
    void Test_BulkInsertBlob_LowLevel2(void);
    void Test_UNIQUE(void);
    void Test_DateTime(void);
    void Test_DateTimeBCP(void);
    void Test_Insert(void);
    void Test_HasMoreResults(void);
    void Test_Create_Destroy(void);
    void Test_Multiple_Close(void);
    void Test_Unicode_Simple(void);
    void Test_Unicode(void);
    void Test_NVARCHAR(void);
    void Test_Iskhakov(void);
    void Test_NCBI_LS(void);
    void Test_Authentication(void);
    void Test_DriverContext_One(void);
    void Test_DriverContext_Many(void);
    void Test_Decimal(void);
    void Test_Query_Cancelation(void);
    void Test_Timeout(void);
    void Test_SetLogStream(void);
    void Test_Identity(void);
    void Test_BlobStore(void);
    void Test_DropConnection(void);
    void Test_N_Connections(void);
    void Test_ConnFactory(void);
    void Test_ConnPool(void);

public:
    typedef IDBConnectionFactory* (*TDBConnectionFactoryFactory)
                (IDBServiceMapper::TFactory svc_mapper_factory);
    void Test_Exception_Safety(void);
    void ES_01_Internal(IConnection& conn);
    void Check_Validator(TDBConnectionFactoryFactory factory,
                         IConnValidator& validator);
    void CheckConnFactory(TDBConnectionFactoryFactory factory_factory);

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
    static Int8 GetIdentity(const auto_ptr<IStatement>& auto_stmt);
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


