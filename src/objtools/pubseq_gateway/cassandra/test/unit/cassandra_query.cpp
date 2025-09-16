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
* Author:  Dmitrii Saprykin, NCBI
*
* File Description:
*   Unit test suite to check CCassQuery class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <string>
#include <memory>
#include <map>
#include <thread>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassQueryTest
    : public testing::Test
{
 public:
    CCassQueryTest()
     : m_KeyspaceName("test_cassandra_driver")
     , m_TableName("test_retrieval")
    {}

 protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    static void TearDownTestCase() {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static shared_ptr<CCassConnectionFactory> m_Factory;
    static shared_ptr<CCassConnection> m_Connection;

    string m_KeyspaceName;
    string m_TableName;
};

const char* CCassQueryTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CCassQueryTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CCassQueryTest::m_Connection(nullptr);

TEST_F(CCassQueryTest, FieldGetMapValue) {
    auto query = m_Connection->NewQuery();
    query->SetSQL(
        "SELECT map_field, int_field FROM test_cassandra_driver.test_retrieval "
        " WHERE id = 'row_with_null_map_column'", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
    ASSERT_EQ(query->NextRow(), ar_dataready) << "Failed to find row for null map value test";

    map<int, string> result;
    EXPECT_TRUE(query->FieldIsNull(0)) << "Null cell is reported as non null";
    EXPECT_NO_THROW(query->FieldGetMapValue(0, result));
    EXPECT_TRUE(result.empty()) << "Field containing null valued map should return empty on fetch";
    EXPECT_THROW(query->FieldGetMapValue(1, result), CCassandraException)
            << "FieldGetMapValue should throw on wrong column type fetch attempt";

    map<int, string> expected = {{1, "a"}, {2, "b"}};
    query = m_Connection->NewQuery();
    query->SetSQL(
        "SELECT map_field FROM test_cassandra_driver.test_retrieval WHERE id = 'row_with_2_item_map'", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
    ASSERT_EQ(query->NextRow(), ar_dataready) << "Failed to find row for map value test";
    EXPECT_FALSE(query->FieldIsNull(0)) << "Not null cell is reported as null";
    EXPECT_NO_THROW(query->FieldGetMapValue(0, result));
    EXPECT_EQ(expected, result) << "FieldGetMapValue failed to return valid map value";
}

TEST_F(CCassQueryTest, ParamAsStrForDebug) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("NOQUERY", 1);

    EXPECT_THROW(query->ParamAsStrForDebug(0), CCassandraException);
    EXPECT_THROW(query->ParamAsStrForDebug(1), CCassandraException);

    string bytes = "AbCdEfGhIJkLmN";
    query->BindBytes(0, reinterpret_cast<const unsigned char *>(bytes.c_str()), bytes.size());
    EXPECT_EQ("'0x4162436445664768494a...'", query->ParamAsStrForDebug(0));
    query->BindBytes(0, reinterpret_cast<const unsigned char *>(bytes.c_str()), 5);
    EXPECT_EQ("'0x4162436445'", query->ParamAsStrForDebug(0));
    query->BindStr(0, bytes);
    EXPECT_EQ("'AbCdEfGhIJ...'", query->ParamAsStrForDebug(0));
    query->BindStr(0, bytes.substr(0, 10));
    EXPECT_EQ("'AbCdEfGhIJ'", query->ParamAsStrForDebug(0));

    set<int> s = {0, 1};
    query->BindSet(0, s);
    EXPECT_EQ("set(...)", query->ParamAsStrForDebug(0));

    query->BindInt64(0, 867);
    EXPECT_EQ("867", query->ParamAsStrForDebug(0));

    query->BindNull(0);
    EXPECT_EQ("Null", query->ParamAsStrForDebug(0));

    query->BindDate(0, 1720396800);
    EXPECT_EQ("1720396800", query->ParamAsStrForDebug(0));
}

TEST_F(CCassQueryTest, CassandraExceptionFormat)
{
    auto query = m_Connection->NewQuery();
    {
        query->SetSQL("NOQUERY", 1);
        try {
            query->Query();
        }
        catch (CCassandraException const &ex) {
            EXPECT_EQ(CCassandraException::eQueryFailed, ex.GetErrCode());
            EXPECT_EQ("CassandraErrorMessage - \"line 1:0 no viable alternative at input 'NOQUERY' ([NOQUERY])\";"
                      " CassandraErrorCode - 0x2002000; SQL: \"NOQUERY\"", ex.GetMsg());
        }
    }

    {
        query->SetSQL("SELECT * FROM system.peers WHERE data_center = ? ALLOW FILTERING", 1);
        query->BindStr(0, "DC1");
        //auto timeout = query->GetRequestTimeoutMs();
        query->SetTimeout(1);
        query->UsePerRequestTimeout(true);
        try {
            query->Query(CCassConsistency::kSerial, true, false);
            while(!query->IsReady()) {
                this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        catch (CCassandraException const &ex) {
            EXPECT_EQ(CCassandraException::eQueryTimeout, ex.GetErrCode());
            EXPECT_EQ("CassandraErrorMessage - \"Request timed out\"; CassandraErrorCode - 0x100000E;"
                      " SQL: \"SELECT * FROM system.peers WHERE data_center = ? ALLOW FILTERING\";"
                      " Params - ('DC1'); timeout - 1ms", ex.GetMsg());
        }
        query->UsePerRequestTimeout(false);
    }

    query = m_Connection->NewQuery();
    {
        query->SetSQL("SELECT * FROM system.size_estimates WHEREE table_name = ? ALLOW FILTERING", 1);
        query->BindStr(0, "allele");
        try {
            query->Query();
        }
        catch (CCassandraException const &ex) {
            EXPECT_EQ(CCassandraException::eQueryFailed, ex.GetErrCode());
            EXPECT_EQ("CassandraErrorMessage - \"line 1:36 mismatched input 'WHEREE' expecting EOF "
                      "(SELECT * FROM system.size_estimates [WHEREE]...)\"; CassandraErrorCode - 0x2002000;"
                      " SQL: \"SELECT * FROM system.size_estimates WHEREE table_name = ? ALLOW FILTERING\"", ex.GetMsg());
        }
    }

    {
        query->SetHost("127.0.0.23");
        query->SetSQL("SELECT * FROM system.peers", 0);
        try {
            query->Query();
        }
        catch (CCassandraException const &ex) {
            EXPECT_EQ(CCassandraException::eQueryFailedRestartable, ex.GetErrCode());
            EXPECT_EQ(
                    "CassandraErrorMessage - \"All hosts in current policy attempted and were either unavailable or failed\";"
                    " CassandraErrorCode - 0x100000A", ex.GetMsg());
        }
    }
}

TEST_F(CCassQueryTest, FieldGetDateValue) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("SELECT date_field FROM test_cassandra_driver.test_retrieval "
        " WHERE id = 'row_with_null_date_column'", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
    ASSERT_EQ(query->NextRow(), ar_dataready) << "Failed to find row for null date value test";
    EXPECT_TRUE(query->FieldIsNull(0)) << "Null cell is reported as non null";
    const int64_t default_val = 1720396800;
    EXPECT_EQ(query->FieldGetInt64Value(0, default_val), default_val);
    EXPECT_EQ(query->FieldGetInt32Value(0, default_val), default_val);
    query = m_Connection->NewQuery();
    query->SetSQL("SELECT date_field FROM test_cassandra_driver.test_retrieval WHERE id = 'row_with_date_value'", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
    ASSERT_EQ(query->NextRow(), ar_dataready) << "Failed to find row for date value test";
    EXPECT_FALSE(query->FieldIsNull(0)) << "Not null cell is reported as null";
    EXPECT_EQ(dtDate, query->FieldType(0)) << "FieldType failed to return correct type";
    const int64_t expected = 1720137600;
    EXPECT_EQ(expected, query->FieldGetInt32Value(0)) << "FieldGetInt32Value failed to return valid date value";
    EXPECT_EQ(expected, query->FieldGetInt64Value(0)) << "FieldGetInt64Value failed to return valid date value";
    EXPECT_EQ("2024-07-05", query->FieldGetStrValue(0)) << "FieldGetStrValue failed to return valid date value";
}

TEST_F(CCassQueryTest, DateValueRoundTrip) {
    const int32_t int_val = getpid();
    const string id = "rewrite_date_value_row_" + NStr::NumericToString(int_val);
    const string date_str = "2024-07-03";
    const CTime t(date_str, "Y-M-D", CTime::ETimeZone::eUTC);
    const int64_t date_val = t.GetTimeT();
    auto query = m_Connection->NewQuery();
    query->SetSQL("INSERT INTO test_cassandra_driver.test_retrieval (id, int_field, date_field) VALUES (?,?,?) USING TTL 3600", 3);
    query->BindStr(0, id);
    query->BindInt32(1, int_val);
    query->BindDate(2, date_val);
    EXPECT_EQ(query->ParamAsInt32(2), date_val);
    EXPECT_EQ(query->ParamAsInt64(2), date_val);
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, false, true);
    query = m_Connection->NewQuery();
    query->SetSQL("SELECT date_field FROM test_cassandra_driver.test_retrieval WHERE id = ?", 1);
    query->BindStr(0, id);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM, false, true);
    ASSERT_EQ(query->NextRow(), ar_dataready) << "Failed to find row for null date value test";
    EXPECT_FALSE(query->FieldIsNull(0)) << "Not null cell is reported as null";
    EXPECT_EQ(query->FieldType(0), dtDate) << "FieldType failed to return correct type";
    EXPECT_EQ(query->FieldGetInt32Value(0), date_val) << "FieldGetInt32Value failed to return valid date value";
    EXPECT_EQ(query->FieldGetInt64Value(0), date_val) << "FieldGetInt64Value failed to return valid date value";
    EXPECT_EQ(query->FieldGetStrValue(0), date_str) << "FieldGetStrValue failed to return valid date value";
}

}  // namespace
