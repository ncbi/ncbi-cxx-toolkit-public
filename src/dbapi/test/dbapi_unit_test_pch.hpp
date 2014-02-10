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

#include <corelib/test_boost.hpp>

#include <dbapi/dbapi.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>

#include <dbapi/driver/dbapi_driver_conn_params.hpp>


#define DBAPI_BOOST_FAIL(ex)                                                \
    do {                                                                    \
        const CDB_TimeoutEx* to = dynamic_cast<const CDB_TimeoutEx*>(&ex);  \
        if (to)                                                             \
            BOOST_TIMEOUT(ex.what());                                       \
        else                                                                \
            BOOST_FAIL(ex.what());                                          \
    } while (0)                                                             \
/**/

#define CONN_OWNERSHIP  eTakeOwnership

BEGIN_NCBI_SCOPE

// Forward declaration
class CDriverManager;
class IDataSource;
class IConnection;
class IStatement;
class IDBConnectionFactory;

enum ETransBehavior { eNoTrans, eTransCommit, eTransRollback };
enum { max_text_size = 8000 };

class CTestTransaction
{
public:
    CTestTransaction(IConnection& conn, ETransBehavior tb = eTransCommit);
    ~CTestTransaction(void);

public:
    IConnection& GetConnection(void)
    {
        _ASSERT(m_Conn);
        return *m_Conn;
    }

private:
    IConnection* m_Conn;
    ETransBehavior  m_TransBehavior;
};

/////////////////////////////////////////////////////////////////////////////
class CUnitTestParams : public CDBConnParamsDelegate
{
public:
    CUnitTestParams(const CDBConnParams& other);
    virtual ~CUnitTestParams(void);

public:
    virtual string GetServerName(void) const;
    virtual EServerType GetServerType(void) const;

private:
    // Non-copyable.
    CUnitTestParams(const CUnitTestParams& other);
    CUnitTestParams& operator =(const CUnitTestParams& other);
};


///////////////////////////////////////////////////////////////////////////
class CTestArguments : public CObject
{
public:
    CTestArguments(void);

public:
    typedef map<string, string> TDatabaseParameters;

    enum ETestConfiguration {
        eWithExceptions,
        eWithoutExceptions,
        eFast
    };

    string GetDriverName(void) const
    {
        return GetTestParams().GetDriverName();
    }

    string GetServerName(void) const
    {
        return GetTestParams().GetServerName();
    }

    string GetUserName(void) const
    {
        return GetTestParams().GetUserName();
    }

    string GetUserPassword(void) const
    {
        return GetTestParams().GetPassword();
    }

    string GetDatabaseName(void) const
    {
        return GetTestParams().GetDatabaseName();
    }

    CDBConnParams::EServerType GetServerType(void) const
    {
        return GetTestParams().GetServerType();
    }

    string GetProgramBasename(void) const;

    bool IsBCPAvailable(void) const;

    bool IsODBCBased(void) const;
    bool DriverAllowsMultipleContexts(void) const;

    bool UseGateway(void) const
    {
        return !m_GatewayHost.empty();
    }

    ETestConfiguration GetTestConfiguration(void) const
    {
        return m_TestConfiguration;
    }

    void PutMsgDisabled(const char* msg) const;
    void PutMsgExpected(const char* msg, const char* replacement) const;

    const CDBConnParams& GetConnParams(void) const
    {
        return m_ConnParams;
    }

private:
    void SetDatabaseParameters(void);

    const CDBConnParams& GetTestParams(void) const
    {
        return m_ConnParams2;
    }

private:
    string m_GatewayHost;
    string m_GatewayPort;

    ETestConfiguration  m_TestConfiguration;

    mutable unsigned int m_NumOfDisabled;
    bool m_ReportDisabled;
    bool m_ReportExpected;

    CDBConnParamsBase m_ParamBase;
    CDBConnParamsBase m_ParamBase2;
    CUnitTestParams m_ConnParams;
    CCPPToolkitConnParams m_CPPParams;
    CUnitTestParams m_ConnParams2;
    // CCPPToolkitConnParams m_CPPParams;
    // CUnitTestParams m_ConnParams;
};


const string& GetTableName(void);
IConnection& GetConnection(void);
IDataSource& GetDS(void);
const CTestArguments& GetArgs(void);
int GetMaxVarcharSize(void);

inline
CDriverManager& GetDM(void)
{
    return CDriverManager::GetInstance();
}

void DumpResults(IStatement* const stmt);
size_t GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
                       const string& table_name);
size_t GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt);
Int8 GetIdentity(const auto_ptr<IStatement>& auto_stmt);


bool CommonInit(void);
void CommonFini(void);


///////////////////////////////////////////////////////////////////////////
class CDBSetConnParams : public CDBConnParamsDelegate
{
public:
    CDBSetConnParams(
        const string& server_name,
        const string& user_name,
        const string& password,
        Uint4 tds_version,
        const CDBConnParams& other);
    virtual ~CDBSetConnParams(void);

public:
    virtual Uint4  GetProtocolVersion(void) const;

    virtual string GetServerName(void) const;
    virtual string GetUserName(void) const;
    virtual string GetPassword(void) const;

private:
    // Non-copyable.
    CDBSetConnParams(const CDBSetConnParams& other);
    CDBSetConnParams& operator =(const CDBSetConnParams& other);

private:
    const Uint4     m_ProtocolVersion;
    const string    m_ServerName;
    const string    m_UserName;
    const string    m_Password;
};


///////////////////////////////////////////////////////////////////////////
extern const char* ftds64_driver;
extern const char* ftds_driver;

extern const char* odbc_driver;
extern const char* ctlib_driver;
extern const char* dblib_driver;

extern const char* msg_record_expected;

static const TBlobOStreamFlags kBOSFlags = (fBOS_UseTransaction |
                                            fBOS_SkipLogging);

string GetSybaseClientVersion(void);

END_NCBI_SCOPE

#endif  // DBAPI_UNIT_TEST_H


