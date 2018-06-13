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
*   Unit test suite to check fullscan runner operations
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <utility>

#include "fullscan_plan_mock.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassandraFullscanRunnerTest
    : public testing::Test
{
 public:
    CCassandraFullscanRunnerTest()
     : m_TestClusterName("ID_CASS_TEST")
     , m_Factory(nullptr)
     , m_Connection(nullptr)
     , m_KeyspaceName("test_ipg_storage_entrez")
     , m_TableName("ipg_report")
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

class CSimpleRowConsumer
    : public ICassandraFullscanConsumer
{
 public:
    virtual bool Tick()
    {
        EXPECT_EQ(true, ICassandraFullscanConsumer::Tick())
                << "Base Tick funcion should return true always";
        return true;
    }
    virtual bool ReadRow(CCassQuery const & query)
    {
        return true;
    }
    virtual void Finalize() {
        m_FinalizeCalled = true;
    }

    bool m_FinalizeCalled = false;
};

TEST_F(CCassandraFullscanRunnerTest, NonConfiguredRunnerTest) {
    CCassandraFullscanRunner runner;
    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw without configuration";
}

TEST_F(CCassandraFullscanRunnerTest, OneThreadRunnerTest) {

    /*shared_ptr<MockCassandraFullscanPlan> plan_mock = make_shared<MockCassandraFullscanPlan>();

    CCassandraFullscanRunner runner;
    runner
        .SetConnection(m_Connection)
        .SetFieldList({"ipg"})
        .SetThreadCount(1)
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName)*/
}

TEST_F(CCassandraFullscanRunnerTest, SmokeTest) {
    unique_ptr<CCassandraFullscanPlan> plan(new CCassandraFullscanPlan());
    plan
        ->SetConnection(m_Connection)
        .SetFieldList({"ipg", "accession"})
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName);

    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(move(plan))
        .SetConsumerFactory(
            []() -> unique_ptr<CSimpleRowConsumer> {
                return unique_ptr<CSimpleRowConsumer>(new CSimpleRowConsumer());
            }
        );
    runner.Execute();
}

}  // namespace
