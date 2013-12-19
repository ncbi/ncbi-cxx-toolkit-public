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

#include <corelib/test_boost.hpp>

#include <dbapi/simple/sdbapi.hpp>
#include <dbapi/driver/exception.hpp>


#define DBAPI_BOOST_FAIL(ex)                                                \
    do {                                                                    \
        const CDB_TimeoutEx* to = NULL;                                     \
        if (ex.GetPredecessor())                                            \
            to = dynamic_cast<const CDB_TimeoutEx*>(ex.GetPredecessor());   \
        if (to)                                                             \
            BOOST_TIMEOUT(ex.what());                                       \
        else                                                                \
            BOOST_FAIL(ex.what());                                          \
    } while (0)                                                             \
/**/

#define CONN_OWNERSHIP  eTakeOwnership

BEGIN_NCBI_SCOPE

enum ETransBehavior { eNoTrans, eTransCommit, eTransRollback };
enum { max_text_size = 8000 };

class CTestTransaction
{
public:
    CTestTransaction(const CDatabase& conn, ETransBehavior tb = eTransCommit);
    ~CTestTransaction(void);

private:
    CDatabase       m_Conn;
    ETransBehavior  m_TransBehavior;
};

enum ESqlSrvType {
    eSqlSrvMsSql,
    eSqlSrvSybase
};

///////////////////////////////////////////////////////////////////////////
class CTestArguments : public CObject
{
public:
    CTestArguments(void);

public:
    string GetServerName(void) const
    {
        if (NStr::CompareNocase(m_ServerName, "MsSql") == 0) {
#ifdef HAVE_LIBCONNEXT
            return "DBAPI_MS_TEST";
#else
            return "MSDEV1";
#endif
        } else if (NStr::CompareNocase(m_ServerName, "Sybase") == 0) {
#ifdef HAVE_LIBCONNEXT
            return "DBAPI_SYB_TEST";
#else
            return "DBAPI_DEV1";
#endif
        }
        return m_ServerName;
    }

    ESqlSrvType GetServerType(void) const
    {
        return m_ServerType;
    }

    string GetUserName(void) const
    {
        return m_UserName;
    }

    string GetUserPassword(void) const
    {
        return m_Password;
    }

    string GetDatabaseName(void) const
    {
        return m_DatabaseName;
    }

private:
    string m_GatewayHost;
    string m_GatewayPort;

    string m_ServerName;
    ESqlSrvType m_ServerType;
    string m_UserName;
    string m_Password;
    string m_DatabaseName;
};


const string& GetTableName(void);
CDatabase& GetDatabase(void);
const CTestArguments& GetArgs(void);

inline
static int s_ServerInfoRows(void)
{
    return (GetArgs().GetServerType() == eSqlSrvMsSql) ? 29 : 30;
}

size_t GetNumOfRecords(CQuery& query, const string& table_name);

extern const char* msg_record_expected;

END_NCBI_SCOPE

#endif  // DBAPI_UNIT_TEST_H


