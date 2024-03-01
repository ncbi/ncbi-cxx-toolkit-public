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
*   Unit test suite to check CCassBlobTaskLoadBlob
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

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CBlobTaskLoadBlobTest
    : public testing::Test
{
 public:
    CBlobTaskLoadBlobTest() = default;

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

    string m_KeyspaceName{"satncbi_extended"};
    string m_BlobChunkKeyspace{"psg_test_sat_4"};
};

const char* CBlobTaskLoadBlobTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CBlobTaskLoadBlobTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CBlobTaskLoadBlobTest::m_Connection(nullptr);

static auto wait_function = [](CCassBlobTaskLoadBlob& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

static auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

TEST_F(CBlobTaskLoadBlobTest, StaleBlobRecordFromCache) {
    size_t call_count{0};
    auto fn404 = [&call_count](
        CRequestStatus::ECode status,
        int code, EDiagSev severity,
        const string & message
    ) {
        ++call_count;
        EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
        EXPECT_EQ(CCassandraException::eInconsistentData, code);
    };
    auto cache_blob = make_unique<CBlobRecord>();
    cache_blob->SetKey(2155365);
    cache_blob->SetModified(1342057137103);
    cache_blob->SetSize(12509);
    cache_blob->SetNChunks(1);
    CCassBlobTaskLoadBlob fetch(m_Connection, m_KeyspaceName, move(cache_blob), true, fn404);
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CBlobTaskLoadBlobTest, ExpiredLastModified) {
    size_t call_count{0};
    auto fn404 = [&call_count](
        CRequestStatus::ECode status,
        int code, EDiagSev severity,
        const string & message
    ) {
        ++call_count;
        EXPECT_EQ(CRequestStatus::e404_NotFound, status);
        EXPECT_EQ(CCassandraException::eNotFound, code);
    };
    CCassBlobTaskLoadBlob fetch(m_Connection, m_KeyspaceName, 2155365, 1342057137103, true, fn404);
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CBlobTaskLoadBlobTest, LatestBlobVersion) {
    CCassBlobTaskLoadBlob fetch(m_Connection, m_BlobChunkKeyspace, 2155365, true, error_function);
    wait_function(fetch);
    EXPECT_TRUE(fetch.IsBlobPropsFound());
    auto blob = fetch.ConsumeBlobRecord();
    EXPECT_EQ(2155365, blob->GetKey());
    EXPECT_EQ(12553LL, blob->GetSize());
    EXPECT_GE(1598181382370LL, blob->GetModified());
    EXPECT_EQ(1, blob->GetNChunks());
    blob->VerifyBlobSize();
}

TEST_F(CBlobTaskLoadBlobTest, ShouldFailOnWrongBigBlobFlag) {
    CCassBlobTaskLoadBlob fetch(m_Connection, m_KeyspaceName, 2155365, false, error_function);
    wait_function(fetch);
    EXPECT_TRUE(fetch.IsBlobPropsFound());
    auto blob = fetch.ConsumeBlobRecord();
    blob->SetBigBlobSchema(true);

    size_t call_count{0};
    static auto error_function =
    [&call_count]
    (CRequestStatus::ECode status, int, EDiagSev, const string &) {
        ++call_count;
        EXPECT_EQ(502, status);
    };
    CCassBlobTaskLoadBlob fetch1(m_Connection, m_KeyspaceName, move(blob), true, error_function);
    wait_function(fetch1);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CBlobTaskLoadBlobTest, ShouldFailOnNoConnection)
{
    EXPECT_THROW(
        CCassBlobTaskLoadBlob(
            nullptr, m_KeyspaceName, 2155365, false, error_function
        ),
        CCassandraException
    );
}

TEST_F(CBlobTaskLoadBlobTest, LoadBigBlobData) {
    CCassBlobTaskLoadBlob fetch(m_Connection, m_KeyspaceName, 422992879, true, error_function);
    wait_function(fetch);
    EXPECT_TRUE(fetch.IsBlobPropsFound());
    auto blob = fetch.ConsumeBlobRecord();
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eBigBlobSchema));
    EXPECT_EQ(blob->GetChunk(0)[0], 0x78);
}

// Fake explicit blob retrieval should fail on blob_chunk CQL not blob_prop
TEST_F(CBlobTaskLoadBlobTest, ExplicitBlobProperties) {
    int call_count{0};
    static auto error_function =
    [&call_count]
    (CRequestStatus::ECode status, int, EDiagSev, const string &message)
    {
        ++call_count;
        EXPECT_EQ(500, status);
        EXPECT_NE(message.find("SELECT data FROM fake_keyspace.blob_chunk WHERE sat_key"), string::npos);
    };
    auto blob_prop = make_unique<CBlobRecord>();
    blob_prop->SetKey(numeric_limits<CBlobRecord::TSatKey>::max());
    blob_prop->SetNChunks(1);
    CCassBlobTaskLoadBlob fetch(m_Connection, "fake_keyspace", move(blob_prop), true, error_function);
    wait_function(fetch);
    EXPECT_TRUE(fetch.IsBlobPropsFound());
    EXPECT_EQ(call_count, 1);
    EXPECT_TRUE(fetch.BlobPropsProvided());
    EXPECT_TRUE(fetch.HasError());
}

TEST_F(CBlobTaskLoadBlobTest, CancelFromBlobPropCallbackShouldStop) {
    bool props_callback_visited{false};
    CCassBlobTaskLoadBlob fetch(m_Connection, m_BlobChunkKeyspace, 2155365, true, error_function);
    fetch.SetPropsCallback(
        [&fetch, &props_callback_visited] (CBlobRecord const & blob, bool isFound)
        {
            EXPECT_TRUE(isFound) << "Blob props expected to be found.";
            EXPECT_EQ(2155365, blob.GetKey());
            EXPECT_EQ(12553LL, blob.GetSize());
            EXPECT_GE(1598181382370LL, blob.GetModified());
            EXPECT_EQ(1, blob.GetNChunks());
            fetch.Cancel();
            props_callback_visited = true;
        }
    );
    fetch.SetChunkCallback(
        [] (CBlobRecord const & blob, const unsigned char * /*data*/, unsigned int size, int chunk_no)
        {
            EXPECT_TRUE(false) << "Chunk callback should not be called if operation cancelled (blob - "
                << blob.GetKey() << ", chunk_no - " << chunk_no << ", size - " << size << ")";
        }
    );

    wait_function(fetch);
    EXPECT_TRUE(props_callback_visited) << "Blob props callback is expected to be visited";
    EXPECT_TRUE(fetch.Cancelled()) << "Operation should be Cancelled()";
    EXPECT_FALSE(fetch.HasError()) << "Cancel() should not look like error for now.";
}

TEST_F(CBlobTaskLoadBlobTest, QueryTimeoutOverride) {
    bool timeout_function_called{false};
    auto timeout_function =
    [&timeout_function_called]
    (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
        timeout_function_called = true;
        EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
        EXPECT_EQ(CCassandraException::eQueryTimeout, code);
        EXPECT_EQ(eDiag_Error, severity);
    };

    CCassBlobTaskLoadBlob fetch(m_Connection, m_BlobChunkKeyspace, 2155365, true, timeout_function);
    fetch.SetQueryTimeout(chrono::milliseconds(1));
    wait_function(fetch);
    EXPECT_TRUE(timeout_function_called) << "Timeout should happen";
    EXPECT_TRUE(fetch.HasError()) << "Request should be in failed state";
}

}  // namespace
