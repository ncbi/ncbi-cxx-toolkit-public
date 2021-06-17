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
*   Unit test suite to check fullscan plan generator operations
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>
#include <limits>

#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassandraFullscanPlanTest
    : public testing::Test
{
 public:
    CCassandraFullscanPlanTest()
     : m_KeyspaceName("test_mlst_storage")
     , m_TableName("allele_data")
    {}

 protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(s_TestClusterName), IRegistry::fPersistent);
        s_Factory = CCassConnectionFactory::s_Create();
        s_Factory->LoadConfig(r, config_section);
        s_Connection = s_Factory->CreateInstance();
        s_Connection->Connect();
    }

    static void TearDownTestCase() {
        s_Connection->Close();
        s_Connection = nullptr;
        s_Factory = nullptr;
    }

    static const char* s_TestClusterName;
    static shared_ptr<CCassConnectionFactory> s_Factory;
    static shared_ptr<CCassConnection> s_Connection;

    string m_KeyspaceName;
    string m_TableName;
};

const char* CCassandraFullscanPlanTest::s_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CCassandraFullscanPlanTest::s_Factory(nullptr);
shared_ptr<CCassConnection> CCassandraFullscanPlanTest::s_Connection(nullptr);

TEST_F(CCassandraFullscanPlanTest, CheckEmptyPlan) {
    CCassandraFullscanPlan plan;
    plan.SetConnection(s_Connection);
    EXPECT_THROW(
        plan.Generate(),
        CCassandraException
    ) << "Generate should throw without table and keyspace";
    plan
        .SetConnection(nullptr)
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName);
    EXPECT_EQ(nullptr, plan.GetNextQuery());
    EXPECT_THROW(
        plan.Generate(),
        CCassandraException
    ) << "Generate should throw without established connection";
}

TEST_F(CCassandraFullscanPlanTest, CheckQuery) {
    CCassConnection::TTokenRanges ranges;
    s_Connection->GetTokenRanges(ranges);
    CCassandraFullscanPlan plan;
    plan
        .SetConnection(s_Connection)
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName)
        .SetMinPartitionsForSubrangeScan(0);
    EXPECT_EQ(nullptr, plan.GetNextQuery())
        << "Plan sould be empty on creation";
    plan.Generate();
    shared_ptr<CCassQuery> query = plan.GetNextQuery();
    string expected_template =
        "SELECT * FROM test_mlst_storage.allele_data WHERE TOKEN(taxid,version) > ? AND TOKEN(taxid,version) <= ?";
    size_t i = 0;
    while(query) {
        ASSERT_LE(++i, ranges.size());
        string expected_query = expected_template + "\nparams: "
            + NStr::NumericToString(ranges[ranges.size() - i].first)
            + ", " + NStr::NumericToString(ranges[ranges.size() - i].second);
        ASSERT_EQ(expected_query, query->ToString());
        query = plan.GetNextQuery();
    }

    plan.SetFieldList({"id", "modified_time() as m"});
    plan.Generate();
    query = plan.GetNextQuery();
    expected_template =
            "SELECT id, modified_time() as m FROM test_mlst_storage.allele_data "
            "WHERE TOKEN(taxid,version) > ? AND TOKEN(taxid,version) <= ?";
    i = 0;
    while(query) {
        ASSERT_LE(++i, ranges.size());
        string expected_query = expected_template + "\nparams: "
            + NStr::NumericToString(ranges[ranges.size() - i].first)
            + ", " + NStr::NumericToString(ranges[ranges.size() - i].second);
        ASSERT_EQ(expected_query, query->ToString());
        query = plan.GetNextQuery();
    }

    plan.SetMinPartitionsForSubrangeScan(numeric_limits<size_t>::max());
    plan.Generate();
    expected_template = "SELECT id, modified_time() as m FROM test_mlst_storage.allele_data\nparams: ";
    query = plan.GetNextQuery();
    ASSERT_EQ(expected_template, query->ToString())
            << "Short plan query template is wrong";
}

TEST_F(CCassandraFullscanPlanTest, CheckQueryCount) {
    CCassConnection::TTokenRanges ranges;
    s_Connection->GetTokenRanges(ranges);
    CCassandraFullscanPlan plan;
    plan
        .SetConnection(s_Connection)
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName)
        .SetMinPartitionsForSubrangeScan(0);
    EXPECT_EQ(0UL, plan.GetMinPartitionsForSubrangeScan());
    EXPECT_EQ(nullptr, plan.GetNextQuery());
    plan.Generate();
    EXPECT_EQ(ranges.size(), plan.GetQueryCount())
        << "Subrange plan should have one query per token range";
    size_t query_count = 0;
    while(plan.GetNextQuery()) {
        query_count++;
    }
    EXPECT_EQ(ranges.size(), query_count);

    plan.SetMinPartitionsForSubrangeScan(numeric_limits<size_t>::max());
    plan.Generate();
    ASSERT_EQ(1UL, plan.GetQueryCount())
        << "Short plan should have one query";
    query_count = 0;
    while(plan.GetNextQuery()) {
        query_count++;
    }
    EXPECT_EQ(1UL, query_count)
        << "Short plan should have one query";
}

}  // namespace
