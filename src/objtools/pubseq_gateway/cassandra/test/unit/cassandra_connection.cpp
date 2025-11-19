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
*   Unit test suite to check CCassConnection class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassConnectionTest
    : public testing::Test
{
 public:
    CCassConnectionTest()
    {}

 protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
    }

    static void TearDownTestCase() {
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static shared_ptr<CCassConnectionFactory> m_Factory;
};

const char* CCassConnectionTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CCassConnectionTest::m_Factory(nullptr);

TEST_F(CCassConnectionTest, ReconnectShouldNotFail) {
    auto connection = m_Factory->CreateInstance();
    ASSERT_FALSE(connection->IsConnected()) << "Connection should be DOWN before running test";

    // Set impossible hostname to fail connection
    connection->SetConnectionPoint("192.168.111.111");
    try {
        connection->Connect();
        FAIL() << "Connect should fail for impossible hostname";
    } catch (CCassandraException& exception) {
        ASSERT_EQ(exception.GetErrCode(), 2002) << "Exception code should be CCassandraException::eFailedToConn";
    }

    try {
        connection->Connect();
        FAIL() << "Subsequent connection attempt should fail";
    } catch (CCassandraException& exception) {
        ASSERT_EQ(exception.GetErrCode(), 2002) << "Exception code should be CCassandraException::eFailedToConn";
    }

    ASSERT_FALSE(connection->IsConnected()) << "Connection should be DOWN after 2 failed attempts";

    string host_list;
    int16_t port;
    m_Factory->GetHostPort(host_list, port);
    connection->SetConnectionPoint(host_list, port);
    connection->Connect();

    ASSERT_TRUE(connection->IsConnected()) << "Connection should be UP after the reason of failure is fixed";
}

TEST_F(CCassConnectionTest, LocalPeersList) {
    auto connection = m_Factory->CreateInstance();
    string dc = connection->GetDatacenterName();
    EXPECT_TRUE(dc.empty());
    connection->Connect();
    dc = connection->GetDatacenterName();
    EXPECT_FALSE(dc.empty());
    auto peer_list = connection->GetLocalPeersAddressList(dc);
    EXPECT_EQ(8UL, peer_list.size());
    for (auto const& peer : peer_list) {
        EXPECT_EQ(0UL, peer.find("130.14."));
    }
    peer_list = connection->GetLocalPeersAddressList("");
    EXPECT_EQ(8UL, peer_list.size());
    for (auto const& peer : peer_list) {
        EXPECT_EQ(0UL, peer.find("130.14."));
    }
}

TEST_F(CCassConnectionTest, GetSizeEstimates) {
    auto connection = m_Factory->CreateInstance();
    connection->Connect();
    auto estimates = connection->GetSizeEstimates("DC1", "idmain2", "si2csi");
    ASSERT_FALSE(estimates.empty());
    auto current_start = estimates.begin()->range_start;
    for (auto const& estimate : estimates) {
        EXPECT_GE(estimate.range_start, current_start);
        EXPECT_GT(estimate.range_end, estimate.range_start);
        EXPECT_GE(estimate.partitions_count, 0);
        EXPECT_GE(estimate.mean_partition_size, 0);
        current_start = estimate.range_end;
    }

    EXPECT_TRUE(connection->GetSizeEstimates("WRONGDC", "idmain2", "si2csi").empty());
    EXPECT_TRUE(connection->GetSizeEstimates("DC1", "idmain2", "").empty());
}

TEST_F(CCassConnectionTest, GetSizeEstimatesOneNodeDown) {
    CCassConnection::TTokenRanges true_estimates_ranges;
    int64_t min_size{numeric_limits<int64_t>::max()}, max_size{numeric_limits<int64_t>::min()};
    int64_t min_count{numeric_limits<int64_t>::max()}, max_count{numeric_limits<int64_t>::min()};
    vector<string> peers;
    {
        // Need a separate connection or black list policy will not work otherwise
        auto connection = m_Factory->CreateInstance();
        connection->Connect();
        peers = connection->GetLocalPeersAddressList("");
        auto true_estimates = connection->GetSizeEstimates("DC1", "idmain2", "si2csi");
        for (auto const& range : true_estimates) {
            true_estimates_ranges.push_back(make_pair(range.range_start, range.range_end));
            min_size = min(range.mean_partition_size, min_size);
            max_size = max(range.mean_partition_size, max_size);
            min_count = min(range.partitions_count, min_count);
            max_count = max(range.partitions_count, max_count);
        }
    }
    ASSERT_FALSE(peers.empty());
    for (auto peer : peers) {
        auto connection = m_Factory->CreateInstance();
        connection->SetBlackList(peer);
        connection->Connect();
        auto estimates = connection->GetSizeEstimates("DC1", "idmain2", "si2csi");
        ASSERT_EQ(estimates.size(), true_estimates_ranges.size());
        auto current_start = estimates.begin()->range_start;
        for (auto const &estimate: estimates) {
            EXPECT_GE(estimate.range_start, current_start);
            EXPECT_GT(estimate.range_end, estimate.range_start);
            EXPECT_GE(estimate.partitions_count, 0);
            EXPECT_GE(estimate.mean_partition_size, 0);
            current_start = estimate.range_end;
        }
        CCassConnection::TTokenRanges estimates_ranges;
        for (auto const& range : estimates) {
            estimates_ranges.push_back(make_pair(range.range_start, range.range_end));
            EXPECT_GE(range.partitions_count, min_count);
            EXPECT_GE(range.mean_partition_size, min_size);
            EXPECT_LE(range.partitions_count, max_count);
            EXPECT_LE(range.mean_partition_size, max_count);
        }
        EXPECT_EQ(estimates_ranges, true_estimates_ranges);
    }
}

TEST_F(CCassConnectionTest, GetSizeEstimatesTwoNodesDown) {
    vector<string> peers;
    {
        // Need a separate connection or black list policy will not work otherwise
        auto connection = m_Factory->CreateInstance();
        connection->Connect();
        peers = connection->GetLocalPeersAddressList("");
    }
    ASSERT_GE(peers.size(), 2UL);
    auto connection = m_Factory->CreateInstance();
    connection->SetBlackList(peers[0] + "," + peers[1]);
    connection->Connect();
    EXPECT_THROW(connection->GetSizeEstimates("DC1", "idmain2", "si2csi"), CCassandraException);
}

TEST_F(CCassConnectionTest, QueryRetryTimeout) {
    const string config_section = "TEST";
    auto factory = CCassConnectionFactory::s_Create();
    CNcbiRegistry r;
    r.Set(config_section, "service", "ID_CASS_TEST", IRegistry::fPersistent);
    r.Set(config_section, "qtimeout", "1", IRegistry::fPersistent);
    r.Set(config_section, "qtimeout_retry", "1000", IRegistry::fPersistent);
    factory->LoadConfig(r, config_section);
    auto connection = factory->CreateInstance();
    connection->Connect();
    EXPECT_EQ(1UL, connection->QryTimeoutMs());
    EXPECT_EQ(1000UL, connection->QryTimeoutRetryMs());
    auto query = connection->NewQuery();
    EXPECT_EQ(1UL, query->Timeout());
    query->SetSQL("select * from test_ipg_storage_entrez.ipg_report LIMIT 10000", 0);
    query->Query(CASS_CONSISTENCY_ALL, true, false, 10000);
    try {
        query->WaitAsync(query->Timeout() * 2000);
        ASSERT_TRUE(false) << "Query with 1ms timeout should throw.";
    }
    catch (CCassandraException const& ex) {
        EXPECT_EQ(CCassandraException::eQueryTimeout, ex.GetErrCode());
    }
    auto start = chrono::steady_clock::now();
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(query->Restart(CASS_CONSISTENCY_ONE)) << "Query with 1000ms timeout should not throw.";
    auto warning = testing::internal::GetCapturedStderr();
    EXPECT_EQ("Warning: Cassandra query restarted: SQL - 'select * from test_ipg_storage_entrez.ipg_report LIMIT 10000'\n", warning);
    EXPECT_EQ(1000UL, query->Timeout());
    EXPECT_EQ(ar_dataready, query->WaitAsync(query->Timeout() * 1000));

    // Operation should take longer than (timeout_before_retry * 3)
    EXPECT_LT(3 * 1, chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count());
}

TEST_F(CCassConnectionTest, LoadBlobRetryTimeout)
{
    int32_t sat_key1{6592654}, sat_key2{5179655};

    const string config_section = "TEST";
    auto factory = CCassConnectionFactory::s_Create();
    CNcbiRegistry r;
    r.Set(config_section, "service", "ID_CASS_TEST", IRegistry::fPersistent);
    // Too small value here prevents prepare operation
    r.Set(config_section, "qtimeout", "1", IRegistry::fPersistent);
    r.Set(config_section, "qtimeout_retry", "1000", IRegistry::fPersistent);
    r.Set(config_section, "maxretries", "1", IRegistry::fPersistent);
    factory->LoadConfig(r, config_section);

    auto connection = factory->CreateInstance();
    connection->Connect();

    int call_count{0};
    auto timeout_error =
    [&call_count]
    (CRequestStatus::ECode status, int code, EDiagSev, const string & message)
    {
        EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
        EXPECT_EQ(2007, code);
        ++call_count;
    };

    // Failing task without retries
    {
        CCassBlobTaskLoadBlob task(
            connection, "satold01", sat_key1, true, timeout_error
        );
        task.SetUsePrepared(false);
        while(!task.Wait()) {
            this_thread::sleep_for(chrono::milliseconds(2));
        }
        EXPECT_GT(call_count, 0);
        EXPECT_TRUE(task.HasError());
    }

    // Task with 2 retries with changed timeout
    {
        call_count = 0;
        CCassBlobTaskLoadBlob task(
            connection, "satold01", sat_key2, true, timeout_error
        );
        task.SetMaxRetries(2);
        task.SetUsePrepared(false);
        testing::internal::CaptureStderr();
        while(!task.Wait()) {
            this_thread::sleep_for(chrono::milliseconds(2));
        }
        testing::internal::GetCapturedStderr();
        EXPECT_EQ(0, call_count);
        EXPECT_FALSE(task.HasError());
    }
}

TEST_F(CCassConnectionTest, MaxRetriesPropagation)
{
    const string config_section = "TEST";
    auto factory = CCassConnectionFactory::s_Create();
    CNcbiRegistry r;
    r.Set(config_section, "service", "ID_CASS_TEST", IRegistry::fPersistent);
    // Too small value here prevents prepare operation
    r.Set(config_section, "maxretries", "7", IRegistry::fPersistent);
    factory->LoadConfig(r, config_section);

    auto connection = factory->CreateInstance();
    EXPECT_EQ(7, connection->GetMaxRetries());
    CCassBlobTaskLoadBlob task(connection, "stub", 0, true, nullptr);
    EXPECT_EQ(7, task.GetMaxRetries());
    task.SetMaxRetries(11);
    EXPECT_EQ(11, task.GetMaxRetries());
    EXPECT_EQ(7, connection->GetMaxRetries());
    task.SetMaxRetries(-1);
    EXPECT_EQ(7, task.GetMaxRetries());
}

TEST_F(CCassConnectionTest, GetTokenRanges)
{
    using CC = CCassConnection;
    auto connection = m_Factory->CreateInstance();
    connection->Connect();
    CC::TTokenRanges ranges;
    connection->GetTokenRanges(ranges);
    ASSERT_FALSE(ranges.empty());
    {
        EXPECT_EQ(ranges.begin()->first, numeric_limits<CC::TTokenValue>::min());
        EXPECT_EQ(ranges.rbegin()->second, numeric_limits<CC::TTokenValue>::max());
        auto previous_end = ranges.begin()->first;
        for (auto range : ranges) {
            EXPECT_LT(range.first, range.second);
            EXPECT_EQ(previous_end, range.first);
            previous_end = range.second;
        }
    }
    {
        set<CC::TTokenValue> start_tokens, end_tokens;
        for (auto itr = ranges.begin(); itr != ranges.end(); ++itr) {
            start_tokens.insert(itr->first);
            end_tokens.insert(itr->second);
        }
        vector<string> tokens;
        auto query = connection->NewQuery();
        query->SetSQL("SELECT tokens FROM system.local", 0);
        query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);
        query->NextRow();
        query->FieldGetSetValues(0, tokens);
        for(const auto & item: tokens) {
            CC::TTokenValue value = strtol(item.c_str(), nullptr, 10);
            EXPECT_FALSE(start_tokens.find(value) == start_tokens.end());
            EXPECT_FALSE(end_tokens.find(value) == end_tokens.end());
        }
    }
}

}  // namespace
