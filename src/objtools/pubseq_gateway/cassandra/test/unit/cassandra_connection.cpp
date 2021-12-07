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
    connection->Connect();
    string dc = connection->GetDatacenterName();
    EXPECT_FALSE(dc.empty());
    auto peer_list = connection->GetLocalPeersAddressList(dc);
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
        query->WaitAsync(query->Timeout() * 1000 + 10);
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
            1000'000, // not used
            1, //retry
            connection, "satold01", sat_key1,
            true, // chunks for timeout
            timeout_error
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
            1000'000, // not used
            2, //retry
            connection, "satold01", sat_key2,
            true, // no chunks
            timeout_error
        );
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

}  // namespace
