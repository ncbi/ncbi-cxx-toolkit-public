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
}

}  // namespace
