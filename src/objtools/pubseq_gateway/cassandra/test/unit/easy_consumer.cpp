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
*   Unit test suite to check templated fullscan runner helpers
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/runner.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/easy_consumer.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <set>
#include <map>

#include "fullscan_plan_mock.hpp"
#include "test_environment.hpp"

namespace {

    USING_NCBI_SCOPE;
    USING_IDBLOB_SCOPE;

    using ::testing::Return;

    class CCassandraEasyConsumerTest
        : public testing::Test
    {
    public:
        CCassandraEasyConsumerTest()
            : m_KeyspaceName("test_ipg_storage_entrez")
            , m_TableName("ipg_report")
        {}

    protected:
        static void SetUpTestCase() {
            const string config_section = "TEST";
            CNcbiRegistry r;
            r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
            sm_Env.Get().SetUp(&r, config_section);
        }

        static void TearDownTestCase() {
            sm_Env.Get().TearDown();
        }

        static const char* m_TestClusterName;
        static CSafeStatic<STestEnvironment> sm_Env;
        static CSafeStatic<atomic_int> sm_InstanceCounter;

        string m_KeyspaceName;
        string m_TableName;
    };

    const char* CCassandraEasyConsumerTest::m_TestClusterName = "ID_CASS_TEST";
    CSafeStatic<STestEnvironment> CCassandraEasyConsumerTest::sm_Env;
    CSafeStatic<atomic_int> CCassandraEasyConsumerTest::sm_InstanceCounter;

    TEST_F(CCassandraEasyConsumerTest, SimpleFullScan)
    {
        string section = "RUNNER_CONFIG";
        CNcbiRegistry r;
        r.Set(section, "runner_thread_count", "15");
        r.Set(section, "runner_consistency", "ONE");
        r.Set(section, "runner_page_size", "45");
        r.Set(section, "runner_max_active_statements", "450");
        r.Set(section, "runner_max_retry_count", "8");

        struct SIpgRow {
            int64_t ipg{};
            string accession;
            string nuc_accession;
            tuple<int, int, int> cds;
            int32_t length;
            list<int> pubmedids;
            vector<string> src_refseq;
            int64_t updated;
        };
        using TFields = TFullscanFieldList<
            CFullScanField<"ipg", SIpgRow, int64_t, &SIpgRow::ipg>,
            CFullScanField<"accession", SIpgRow, string, &SIpgRow::accession>,
            CFullScanField<"nuc_accession", SIpgRow, string, &SIpgRow::nuc_accession>,
            CFullScanField<"cds", SIpgRow, decltype(SIpgRow::cds), &SIpgRow::cds>,
            CFullScanField<"length", SIpgRow, decltype(SIpgRow::length), &SIpgRow::length>,
            CFullScanField<"pubmedids", SIpgRow, decltype(SIpgRow::pubmedids), &SIpgRow::pubmedids>,
            CFullScanField<"src_refseq as refseq", SIpgRow, vector<string>, &SIpgRow::src_refseq>,
            CFullScanField<"updated", SIpgRow, int64_t, &SIpgRow::updated>
        >;
        using TContainer = vector<SIpgRow>;
        using TConsumer = CFullscanEasyConsumer<TContainer, TFields>;
        auto plan = make_unique<CCassandraFullscanPlan>();
        plan
            ->SetConnection(sm_Env.Get().connection)
            .SetFieldList(TConsumer::GetFieldList())
            .SetKeyspace(m_KeyspaceName)
            .SetTable(m_TableName);

        CCassandraFullscanRunner runner;
        runner.ApplyConfiguration(&r, section);
        runner.SetExecutionPlan(std::move(plan));
        TContainer data_vector;
        EXPECT_TRUE(CFullscanEasyRunner::Execute<TConsumer>(runner, data_vector));
        bool found_test_record{false};
        for (auto const& row : data_vector) {
            EXPECT_LT(0, row.ipg);
            EXPECT_FALSE(row.accession.empty());
            if (row.ipg == 56998991 && row.accession == "WP_028954042.1" and row.nuc_accession == "NZ_ALVU02000001.1") {
                found_test_record = true;
                EXPECT_EQ(make_tuple(327945, 329060, 1), row.cds);
                EXPECT_EQ(371, row.length);
                EXPECT_EQ(list<int>{23277585}, row.pubmedids);
                EXPECT_EQ(vector<string>{"Known"}, row.src_refseq);
                EXPECT_EQ(1492401600000, row.updated);
            }
        }
        EXPECT_TRUE(found_test_record);
    }

    TEST_F(CCassandraEasyConsumerTest, FieldFunctionFullScan)
    {
        struct SIpgRow
        {
            int64_t ipg{};
            string accession;
            void SetAccession(const string& value) {
                accession = value + "_Z";
            }
        };
        using TFields = TFullscanFieldList<
            CFullScanField<"ipg", SIpgRow, int64_t, &SIpgRow::ipg>,
            CFullScanFieldMethod<"accession", SIpgRow, string, const string&, &SIpgRow::SetAccession>
        >;
        using TContainer = vector<SIpgRow>;
        using TConsumer = CFullscanEasyConsumer<TContainer, TFields>;
        auto plan = make_unique<CCassandraFullscanPlan>();
        plan
            ->SetConnection(sm_Env.Get().connection)
            .SetFieldList(TConsumer::GetFieldList())
            .SetKeyspace(m_KeyspaceName)
            .SetTable(m_TableName);

        vector<string> expected_fields{"ipg", "accession"};
        EXPECT_EQ(expected_fields, TConsumer::GetFieldList());

        CCassandraFullscanRunner runner;
        runner.SetExecutionPlan(std::move(plan));
        TContainer data_vector;
        EXPECT_TRUE(CFullscanEasyRunner::Execute<TConsumer>(runner, data_vector));
        for (auto const& row : data_vector) {
            EXPECT_LT(0, row.ipg);
            EXPECT_FALSE(row.accession.empty());
            EXPECT_TRUE(row.accession.ends_with("_Z"));
        }
    }

    TEST_F(CCassandraEasyConsumerTest, DequeContainterFullScan)
    {
        struct SIpgRow
        {
            int64_t ipg{};
            string accession;
        };
        using TFields = TFullscanFieldList<
            CFullScanField<"ipg", SIpgRow, int64_t, &SIpgRow::ipg>,
            CFullScanField<"accession", SIpgRow, string, &SIpgRow::accession>
        >;
        using TContainer = deque<SIpgRow>;
        using TConsumer = CFullscanEasyConsumer<TContainer, TFields>;
        auto plan = make_unique<CCassandraFullscanPlan>();
        plan
            ->SetConnection(sm_Env.Get().connection)
            .SetFieldList(TConsumer::GetFieldList())
            .SetKeyspace(m_KeyspaceName)
            .SetTable(m_TableName);

        CCassandraFullscanRunner runner;
        runner.SetExecutionPlan(std::move(plan));
        TContainer data_vector;
        EXPECT_TRUE(CFullscanEasyRunner::Execute<TConsumer>(runner, data_vector));
        for (auto const& row : data_vector) {
            EXPECT_LT(0, row.ipg);
            EXPECT_FALSE(row.accession.empty());
        }
    }

    TEST_F(CCassandraEasyConsumerTest, CustomConsumerFullScan)
    {
        struct SIpgRow {
            int64_t ipg{};
            string accession;
        };
        using TFields = TFullscanFieldList<
            CFullScanField<"ipg", SIpgRow, int64_t, &SIpgRow::ipg>,
            CFullScanField<"accession", SIpgRow, string, &SIpgRow::accession>
        >;
        using TContainer = deque<SIpgRow>;
        sm_InstanceCounter.Get() = 0;
        struct STestConsumer
            : public CFullscanEasyConsumer<TContainer, TFields>
        {
            using TBaseClass = CFullscanEasyConsumer<TContainer, TFields>;
            STestConsumer() = delete;
            STestConsumer(mutex *mtx, TContainer *data)
                : TBaseClass(mtx, data)
                , m_InstanceId(++sm_InstanceCounter.Get())
            {}

            void Finalize() override
            {
                auto buffer_size = BufferSize();
                auto container_size = ContainerSize();
                EXPECT_LE(0UL, buffer_size);
                EXPECT_LE(0UL, container_size);
                TBaseClass::Finalize();
                if (buffer_size > 0) {
                    auto new_buffer_size = BufferSize();
                    auto new_container_size = ContainerSize();
                    EXPECT_EQ(0UL, new_buffer_size);
                    EXPECT_LT(container_size, new_container_size);
                }
            }
        private:
            int m_InstanceId{0};
        };
        auto plan = make_unique<CCassandraFullscanPlan>();
        plan
            ->SetConnection(sm_Env.Get().connection)
            .SetFieldList(STestConsumer::GetFieldList())
            .SetKeyspace(m_KeyspaceName)
            .SetTable(m_TableName);

        CCassandraFullscanRunner runner;
        runner.SetExecutionPlan(std::move(plan));
        TContainer data_vector;
        EXPECT_TRUE(CFullscanEasyRunner::Execute<STestConsumer>(runner, data_vector));
        for (auto const& row : data_vector) {
            EXPECT_LT(0, row.ipg);
            EXPECT_FALSE(row.accession.empty());
        }
    }
}  // namespace
