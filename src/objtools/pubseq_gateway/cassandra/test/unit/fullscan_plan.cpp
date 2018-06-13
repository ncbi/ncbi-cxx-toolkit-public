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
     : m_TestClusterName("ID_CASS_TEST")
     , m_Factory(nullptr)
     , m_Connection(nullptr)
     , m_KeyspaceName("test_mlst_storage")
     , m_TableName("allele_data")
    {}

    virtual void SetUp()
    {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", m_TestClusterName, IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    virtual void TearDown()
    {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }
 protected:
    const string m_TestClusterName;
    shared_ptr<CCassConnectionFactory> m_Factory;
    shared_ptr<CCassConnection> m_Connection;

    string m_KeyspaceName;
    string m_TableName;
};

TEST_F(CCassandraFullscanPlanTest, CheckEmptyPlan) {
    CCassandraFullscanPlan plan;
    plan.SetConnection(m_Connection);
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
    m_Connection->GetTokenRanges(ranges);
    CCassandraFullscanPlan plan;
    plan
        .SetConnection(m_Connection)
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
        ASSERT_EQ(expected_template, query->ToString());
        ASSERT_LE(++i, ranges.size());
        EXPECT_EQ(ranges[ranges.size() - i].first, query->ParamAsInt64(0));
        EXPECT_EQ(ranges[ranges.size() - i].second, query->ParamAsInt64(1));
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
        ASSERT_EQ(expected_template, query->ToString());
        ASSERT_LE(++i, ranges.size());
        ASSERT_EQ(ranges[ranges.size() - i].first, query->ParamAsInt64(0));
        ASSERT_EQ(ranges[ranges.size() - i].second, query->ParamAsInt64(1));
        query = plan.GetNextQuery();
    }

    plan.SetMinPartitionsForSubrangeScan(numeric_limits<size_t>::max());
    plan.Generate();
    expected_template = "SELECT id, modified_time() as m FROM test_mlst_storage.allele_data";
    query = plan.GetNextQuery();
    ASSERT_EQ(expected_template, query->ToString())
            << "Short plan query template is wrong";
}

TEST_F(CCassandraFullscanPlanTest, CheckQueryCount) {
    CCassConnection::TTokenRanges ranges;
    m_Connection->GetTokenRanges(ranges);
    CCassandraFullscanPlan plan;
    plan
        .SetConnection(m_Connection)
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
