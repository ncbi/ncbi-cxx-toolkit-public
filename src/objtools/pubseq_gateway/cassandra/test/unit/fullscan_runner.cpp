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
#include <thread>
#include <utility>
#include <set>
#include <map>

#include "fullscan_plan_mock.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

using ::testing::Return;

class CCassandraFullscanRunnerTest
    : public testing::Test
{
public:
    CCassandraFullscanRunnerTest()
        : m_KeyspaceName("test_ipg_storage_entrez")
        , m_TableName("ipg_report")
    {}

protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        //r.Set(config_section, "numthreadsio", "1", IRegistry::fPersistent);
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

const char* CCassandraFullscanRunnerTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CCassandraFullscanRunnerTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CCassandraFullscanRunnerTest::m_Connection(nullptr);

struct SConsumeContext {
    CFastMutex mutex;
    bool tick_result{true};
    bool read_result{true};
    bool restartable{false};
    size_t tick_called{0};
    size_t finalize_called{0};
    size_t read_called{0};
    size_t consumer_created{0};
    size_t query_restart_called{0};
    bool finalize_should_throw{false};
    bool read_should_throw{false};
    set<thread::id> thread_ids;
    map<int64_t, size_t> ids_returned;

    size_t max_consecutive_rows{0};
    size_t consecutive_rows{0};
};

class CSimpleRowConsumer
    : public ICassandraFullscanConsumer
{
public:
    explicit CSimpleRowConsumer(SConsumeContext * context)
        : m_Context(context)
    {
        if (m_Context) {
            CFastMutexGuard _(m_Context->mutex);
            ++m_Context->consumer_created;
        }
    }

    bool Tick() override
    {
        if (m_Context) {
            CFastMutexGuard _(m_Context->mutex);
            m_Context->thread_ids.insert(this_thread::get_id());
            ++m_Context->tick_called;
            if (m_Context->consecutive_rows > m_Context->max_consecutive_rows) {
                m_Context->max_consecutive_rows = m_Context->consecutive_rows;
            }
            m_Context->consecutive_rows = 0;
        }
        EXPECT_EQ(true, ICassandraFullscanConsumer::Tick())
                << "Base Tick funcion should return true always";
        return m_Context ? m_Context->tick_result : true;
    }

    bool ReadRow(CCassQuery const & query) override
    {
        if (m_Context) {
            CFastMutexGuard _(m_Context->mutex);
            m_Context->thread_ids.insert(this_thread::get_id());
            ++m_Context->ids_returned[query.FieldGetInt64Value(0)];
            ++m_ConsumerIdsReturned[query.FieldGetInt64Value(0)];
            ++m_Context->consecutive_rows;
            ++m_Context->read_called;
            if (m_Context->read_should_throw) {
                NCBI_THROW(CCassandraException, eFatal, "ReadRow may throw");
            }
        }
        return m_Context ? m_Context->read_result : true;
    }

    void OnQueryRestart() override
    {
        cout << "QUERY RESTARTED" << endl;
        // Simulate reset of internal container on Query failure
        if (m_Context) {
            CFastMutexGuard _(m_Context->mutex);
            if (m_Context->restartable) {
                for (auto item : m_ConsumerIdsReturned) {
                    m_Context->ids_returned[item.first] -= item.second;
                }
                m_ConsumerIdsReturned.clear();
            }
            ++m_Context->query_restart_called;
        }
    }

    void Finalize() override
    {
        if (m_Context) {
            CFastMutexGuard _(m_Context->mutex);
            ++m_Context->finalize_called;

            if (m_Context->finalize_should_throw) {
                NCBI_THROW(CCassandraException, eFatal, "Finalize may throw");
            }
        }
    }

    SConsumeContext* m_Context;
    map<int64_t, size_t> m_ConsumerIdsReturned;
};

class CCassandraFullscanPlanCaching
    : public CCassandraFullscanPlan
{
public:
    TQueryPtr GetNextQuery() override
    {
        auto query = CCassandraFullscanPlan::GetNextQuery();
        if (query) {
            query->UsePerRequestTimeout(true);
            query->SetTimeout(1);
        }
        return query;
    }

    void Generate() override
    {
        if (!m_Generated) {
            CCassandraFullscanPlan::Generate();
            m_Generated = true;
        }
    }
private:
    bool m_Generated{false};
};

unique_ptr<MockCassandraFullscanPlan> make_default_plan_mock()
{
    auto plan_mock = make_unique<MockCassandraFullscanPlan>();
    EXPECT_CALL(*plan_mock, Generate())
        .Times(1);
    EXPECT_CALL(*plan_mock, GetQueryCount())
        .WillOnce(Return(1UL));
    return plan_mock;
}

TEST_F(CCassandraFullscanRunnerTest, NonConfiguredRunnerTest) {
    CCassandraFullscanRunner runner;
    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw without configuration";
}

TEST_F(CCassandraFullscanRunnerTest, BrokenQueryRunnerTest) {
    auto query = m_Connection->NewQuery();
    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query));

    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [] { return make_unique<CSimpleRowConsumer>(nullptr);}
        );

    EXPECT_THROW(runner.SetExecutionPlan(nullptr), CCassandraException)
        << "SetExecutionPlan should throw when called second time";

    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw with broken query";
}

TEST_F(CCassandraFullscanRunnerTest, AtLeastOneActiveStatementPerThread) {
    auto plan_mock = make_unique<MockCassandraFullscanPlan>();
    EXPECT_CALL(*plan_mock, Generate())
        .Times(1);
    EXPECT_CALL(*plan_mock, GetQueryCount())
        .WillOnce(Return(4UL));
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .Times(0);

    SConsumeContext context;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetMaxActiveStatements(1)
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw on wrong max_active_statements";
}

TEST_F(CCassandraFullscanRunnerTest, OneThreadRunnerTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);

    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillRepeatedly(Return(nullptr));

    SConsumeContext context;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_TRUE(runner.Execute());
    EXPECT_EQ(1UL, context.thread_ids.size());
    EXPECT_EQ(1UL, context.thread_ids.count(this_thread::get_id()));
    EXPECT_EQ(1UL, context.ids_returned.size());
    EXPECT_EQ(1UL, context.ids_returned[160755002]);
    EXPECT_EQ(1UL, context.finalize_called);
}

TEST_F(CCassandraFullscanRunnerTest, MultiThreadRunnerTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);
    auto query1 = m_Connection->NewQuery();
    query1->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 103317145", 0);
    auto query2 = m_Connection->NewQuery();
    query2->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 15724717", 0);

    auto plan_mock = make_unique<MockCassandraFullscanPlan>();
    EXPECT_CALL(*plan_mock, Generate())
        .Times(1);
    EXPECT_CALL(*plan_mock, GetQueryCount())
        .WillOnce(Return(3UL));
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .Times(6)
        .WillOnce(Return(query))
        .WillOnce(Return(query1))
        .WillOnce(Return(query2))
        .WillRepeatedly(Return(nullptr));

    SConsumeContext context;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_TRUE(runner.Execute());
    EXPECT_EQ(3UL, context.thread_ids.size());
    EXPECT_EQ(1UL, context.thread_ids.count(this_thread::get_id()))
        << "One worker should be executed in current thread";
    EXPECT_EQ(3UL, context.ids_returned.size());
    EXPECT_EQ(1UL, context.ids_returned[160755002]);
    EXPECT_EQ(1UL, context.ids_returned[103317145]);
    EXPECT_EQ(3UL, context.ids_returned[15724717]);
    EXPECT_EQ(3UL, context.finalize_called);
}

TEST_F(CCassandraFullscanRunnerTest, FinalizeMayThrowTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);
    auto query1 = m_Connection->NewQuery();
    query1->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);

    unique_ptr<MockCassandraFullscanPlan> plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillOnce(Return(query1))
        .WillOnce(Return(nullptr));

    SConsumeContext context;
    context.finalize_should_throw = true;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw with finalize throw";
}

TEST_F(CCassandraFullscanRunnerTest, ReadMayThrowTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);

    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillOnce(Return(nullptr));

    SConsumeContext context;
    context.read_should_throw = true;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_THROW(runner.Execute(), CCassandraException)
        << "Execute should throw after read throw";
}

TEST_F(CCassandraFullscanRunnerTest, ReadMayReturnFalseTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 15724717", 0);

    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillOnce(Return(nullptr));

    SConsumeContext context;
    context.read_result = false;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_FALSE(runner.Execute())
        << "Execute should return false after any read call returns false";
    EXPECT_EQ(1UL, context.read_called);
}

TEST_F(CCassandraFullscanRunnerTest, TickMayReturnFalseTest) {
    auto query = m_Connection->NewQuery();
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 15724717", 0);
    auto query1 = m_Connection->NewQuery();
    query1->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 160755002", 0);

    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillOnce(Return(query1))
        .WillOnce(Return(nullptr));

    SConsumeContext context;
    context.tick_result = false;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(1)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_FALSE(runner.Execute())
        << "Execute should return false after any read call returns false";
    EXPECT_EQ(1UL, context.tick_called);
}

TEST_F(CCassandraFullscanRunnerTest, ResultPagingTest) {
    auto query = m_Connection->NewQuery();
    // this group has 3 records
    query->SetSQL("select ipg from test_ipg_storage_entrez.ipg_report WHERE ipg = 15724717", 0);

    auto plan_mock = make_default_plan_mock();
    EXPECT_CALL(*plan_mock, GetNextQuery())
        .WillOnce(Return(query))
        .WillOnce(Return(nullptr));

    SConsumeContext context;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(1)
        .SetConsistency(CASS_CONSISTENCY_LOCAL_ONE)
        .SetPageSize(2)
        .SetExecutionPlan(std::move(plan_mock))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );

    EXPECT_TRUE(runner.Execute());
    EXPECT_EQ(2UL, context.max_consecutive_rows);
    EXPECT_EQ(3UL, context.ids_returned[15724717]);
}

TEST_F(CCassandraFullscanRunnerTest, ConsumerPerQueryTest)
{
    auto connection = m_Factory->CreateInstance();
    connection->Connect();
    auto plan = make_unique<CCassandraFullscanPlanCaching>();
    plan
        ->SetConnection(connection)
        .SetFieldList({"ipg", "accession"})
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName)
        .SetMinPartitionsForSubrangeScan(0);

    plan->Generate();
    auto plan_query_count = plan->GetQueryCount();

    // To allow Query succeed when restarted
    connection->SetQueryTimeoutRetry(1000);

    SConsumeContext context;
    context.restartable = true;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        )
        .SetConsumerCreationPolicy(ECassandraFullscanConsumerPolicy::eOnePerQuery);
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    auto execution_result = runner.Execute();
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_TRUE(execution_result);
    size_t total_rows{0};
    for (auto ipg_block : context.ids_returned) {
        total_rows += ipg_block.second;
    }
    EXPECT_EQ(35425UL, total_rows);
    EXPECT_EQ(plan_query_count, context.consumer_created);
    EXPECT_LT(10UL, context.query_restart_called);
    EXPECT_LT(1000UL, context.consumer_created);
}

TEST_F(CCassandraFullscanRunnerTest, SmokeTest) {
    auto plan = make_unique<CCassandraFullscanPlan>();
    plan
        ->SetConnection(m_Connection)
        .SetFieldList({"ipg", "accession"})
        .SetKeyspace(m_KeyspaceName)
        .SetTable(m_TableName)
        .SetMinPartitionsForSubrangeScan(0);

    SConsumeContext context;
    CCassandraFullscanRunner runner;
    runner
        .SetThreadCount(4)
        .SetExecutionPlan(std::move(plan))
        .SetConsumerFactory(
            [&context] {
                return make_unique<CSimpleRowConsumer>(&context);
            }
        );
    EXPECT_TRUE(runner.Execute());
    size_t total_rows{0};
    for (auto ipg_block : context.ids_returned) {
        total_rows += ipg_block.second;
    }
    EXPECT_EQ(35425UL, total_rows);
}

TEST_F(CCassandraFullscanRunnerTest, CheckRegistrySettings)
{
    string section = "RUNNER_CONFIG";
    CNcbiRegistry r;
    r.Set(section, "runner_thread_count", "15");
    r.Set(section, "runner_consistency", "ANY");
    r.Set(section, "runner_page_size", "45");
    r.Set(section, "runner_max_active_statements", "450");
    r.Set(section, "runner_max_retry_count", "8");

    class CTestRunner : public CCassandraFullscanRunner
    {
    public:
        int64_t ThreadCount() const
        {
            return GetThreadCount();
        }
        TCassConsistency Consistency() const
        {
            return GetConsistency();
        }
        int64_t PageSize() const
        {
            return GetPageSize();
        }
        int64_t MaxActiveStatements() const
        {
            return GetMaxActiveStatements();
        }
        int64_t MaxRetryCount() const
        {
            return GetMaxRetryCount();
        }
    };

    CTestRunner runner;
    runner.ApplyConfiguration(&r, section);
    EXPECT_EQ(15, runner.ThreadCount());
    EXPECT_EQ(TCassConsistency::CASS_CONSISTENCY_ANY, runner.Consistency());
    EXPECT_EQ(45, runner.PageSize());
    EXPECT_EQ(450, runner.MaxActiveStatements());
    EXPECT_EQ(8, runner.MaxRetryCount());
}

}  // namespace
