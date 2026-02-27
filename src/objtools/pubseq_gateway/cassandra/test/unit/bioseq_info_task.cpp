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
*   Unit test suite to check CCassBioseqInfoTaskFetch
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include "test_environment.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CBioseqInfoTaskFetchTest
    : public testing::Test
{
public:
    CBioseqInfoTaskFetchTest() = default;

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
    string m_KeyspaceName{"idmain2"};
};

const char* CBioseqInfoTaskFetchTest::m_TestClusterName = "ID_CASS_TEST";
CSafeStatic<STestEnvironment> CBioseqInfoTaskFetchTest::sm_Env;

static auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

static auto wait_function = [](CCassBioseqInfoTaskFetch& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

TEST_F(CBioseqInfoTaskFetchTest, AccessionNotFound) {
    vector<CBioseqInfoRecord> actual_records;
    size_t call_count{0};
    CBioseqInfoFetchRequest request;
    request.SetAccession("FAKEACCESSION");
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(0UL, actual_records.size());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionMultiple) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("H82419");
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> && records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    EXPECT_EQ("DC1", fetch.GetConnectionDatacenterName());
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(2UL, actual_records.size());

    EXPECT_EQ("H82419", actual_records[0].GetAccession());
    EXPECT_EQ(1, actual_records[0].GetVersion());
    EXPECT_EQ(5, actual_records[0].GetSeqIdType());
    EXPECT_EQ(1060508, actual_records[0].GetGI());

    EXPECT_EQ("H82419", actual_records[1].GetAccession());
    EXPECT_EQ(0, actual_records[1].GetVersion());
    EXPECT_EQ(7, actual_records[1].GetSeqIdType());
    EXPECT_EQ(11278869, actual_records[1].GetGI());
}

// ADD SatOLD12 and SatOLD14 to replication list to ENABLE
TEST_F(CBioseqInfoTaskFetchTest, DISABLED_AccessionVersionMultiple) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("AC005299").SetVersion(0);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> && records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(5UL, actual_records.size());

    EXPECT_EQ("AC005299", actual_records[0].GetAccession());
    EXPECT_EQ(0, actual_records[0].GetVersion());
    EXPECT_EQ(5, actual_records[0].GetSeqIdType());
    EXPECT_EQ(3746100, actual_records[0].GetGI());

    EXPECT_EQ("AC005299", actual_records[4].GetAccession());
    EXPECT_EQ(0, actual_records[4].GetVersion());
    EXPECT_EQ(5, actual_records[4].GetSeqIdType());
    EXPECT_EQ(3327928, actual_records[4].GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionVersionSingle) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("H82419").SetVersion(1);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, actual_records.size());

    EXPECT_EQ("H82419", actual_records[0].GetAccession());
    EXPECT_EQ(1, actual_records[0].GetVersion());
    EXPECT_EQ(5, actual_records[0].GetSeqIdType());
    EXPECT_EQ(1060508, actual_records[0].GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionSeqIdType) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("H82419").SetSeqIdType(7);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, actual_records.size());

    EXPECT_EQ("H82419", actual_records[0].GetAccession());
    EXPECT_EQ(0, actual_records[0].GetVersion());
    EXPECT_EQ(7, actual_records[0].GetSeqIdType());
    EXPECT_EQ(11278869, actual_records[0].GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionGISingle) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("H82419").SetGI(11278869);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, actual_records.size());

    EXPECT_EQ("H82419", actual_records[0].GetAccession());
    EXPECT_EQ(0, actual_records[0].GetVersion());
    EXPECT_EQ(7, actual_records[0].GetSeqIdType());
    EXPECT_EQ(11278869, actual_records[0].GetGI());
}

// ADD SatOLD12 and SatOLD14 to replication list to ENABLE
TEST_F(CBioseqInfoTaskFetchTest, DISABLED_SeqIdsInheritance) {
    size_t call_count{0};
    vector<CBioseqInfoRecord> actual_records;
    CBioseqInfoFetchRequest request;
    request.SetAccession("NC_000001").SetVersion(5);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count, &actual_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            actual_records = std::move(records);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, actual_records.size());

    EXPECT_EQ("NC_000001", actual_records[0].GetAccession());
    EXPECT_EQ(5, actual_records[0].GetVersion());
    EXPECT_EQ(10, actual_records[0].GetSeqIdType());
    EXPECT_EQ(37623929, actual_records[0].GetGI());
    EXPECT_EQ(0, actual_records[0].GetState());
    EXPECT_EQ(10, actual_records[0].GetSeqState());
    EXPECT_EQ(4UL, actual_records[0].GetSeqIds().size());
    set<tuple<int16_t, string>> expected_seq_ids;
    // Own
    expected_seq_ids.insert(make_tuple<int16_t, string>(12, "37623929"));

    // Inherited
    expected_seq_ids.insert(make_tuple<int16_t, string>(11, "ASM:GCF_000001305|1"));
    expected_seq_ids.insert(make_tuple<int16_t, string>(11, "NCBI_GENOMES|1"));
    expected_seq_ids.insert(make_tuple<int16_t, string>(19, "GPC_000001293.1"));
    EXPECT_EQ(expected_seq_ids, actual_records[0].GetSeqIds());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionGIWrongVersion) {
    size_t call_count{0};
    CBioseqInfoFetchRequest request;
    request.SetAccession("AC005299").SetVersion(5).SetGI(3643631);
    CCassBioseqInfoTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName, request,
        [&call_count](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            EXPECT_EQ(0UL, records.size());
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
}

}  // namespace
