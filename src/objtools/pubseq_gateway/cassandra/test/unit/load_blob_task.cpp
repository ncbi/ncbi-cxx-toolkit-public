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
#include <condition_variable>
#include <mutex>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>
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
    string m_BigBlobKeyspace{"psg_test_sat_33"};
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
    CCassBlobTaskLoadBlob fetch(m_Connection, m_KeyspaceName, std::move(cache_blob), true, fn404);
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

TEST_F(CBlobTaskLoadBlobTest, ReadConsistencyAnyShouldFail)
{
    bool errorCallbackCalled{false};
    auto error_fn =
        [&errorCallbackCalled]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
    {
        EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
        EXPECT_EQ(CCassandraException::eQueryFailed, code);
        EXPECT_EQ(eDiag_Error, severity);
        string message_x = regex_replace(message, regex(" line \\d+: Error"), " line N: Error");
        message_x = regex_replace(message_x, regex("[^\"]+cass_driver\\.cpp"), "cass_driver.cpp");
        EXPECT_EQ("NCBI C++ Exception:\n    T0 \"cass_driver.cpp\","
                  " line N: Error: (CCassandraException::eQueryFailed) "
                  "idblob::CCassQuery::ProcessFutureResult() - "
                  "CassandraErrorMessage - \"ANY ConsistencyLevel is only supported for writes\";"
                  " CassandraErrorCode - 0x2002200; SQL: \"SELECT    last_modified,   class,   date_asn1,   div,"
                  "   flags,   hup_date,   id2_info,   n_chunks,   owner,   size,   size_unpacked,   username"
                  " FROM psg_test_sat_4.blob_prop WHERE sat_key = ? LIMIT 1\"; Params - (2155365)\n", message_x);
        errorCallbackCalled = true;
    };
    CCassBlobTaskLoadBlob fetch(m_Connection, m_BlobChunkKeyspace, 2155365, true, error_fn);
    fetch.SetReadConsistency(CCassConsistency::kAny);
    wait_function(fetch);
    EXPECT_TRUE(errorCallbackCalled) << "Error is expected to happen when reading with ANY consistency";
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
    CCassBlobTaskLoadBlob fetch1(m_Connection, m_KeyspaceName, std::move(blob), true, error_function);
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
    CCassBlobTaskLoadBlob fetch(m_Connection, "fake_keyspace", std::move(blob_prop), true, error_function);
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

    CCassBlobTaskLoadBlob fetch(m_Connection, m_BigBlobKeyspace, 32435367, true, timeout_function);
    fetch.SetQueryTimeout(chrono::milliseconds(1));
    wait_function(fetch);
    EXPECT_TRUE(timeout_function_called) << "Timeout should happen";
    EXPECT_TRUE(fetch.HasError()) << "Request should be in failed state";
}

TEST_F(CBlobTaskLoadBlobTest, BlobChunkTimeOutFailWithComment)
{
    bool blob_finished{false}, comment_finished{false}, timeout_called{false}, comment_called{false};
    bool false_negative{false};
    auto timeout_function =
        [&timeout_called, &blob_finished]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            timeout_called = true;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eQueryTimeout, code);
            EXPECT_EQ(eDiag_Error, severity);
            blob_finished = true;
        };
    auto chunk_callback =
        [&blob_finished, &false_negative]
        (CBlobRecord const &, const unsigned char *, unsigned int size, int chunk_no) {
            if (chunk_no == -1) {
                blob_finished = true;
                false_negative = true;
                cout << "CHUNKS RECEIVED: TEST RESULT IS FALSE NEGATIVE" << endl;
            }
        };
    auto comment_callback =
        [&comment_called]
        (string comment, bool isFound) {
            comment_called = true;
            EXPECT_TRUE(isFound) << "Should be found";
            EXPECT_EQ(comment, "BLOB_STATUS_SUPPRESSED") << "Should be expected comment text";
        };


    auto blob = make_unique<CBlobRecord>();
    (*blob).SetKey(8091).SetModified(1000412801493).SetNChunks(1).SetSize(1114);
    CCassBlobTaskLoadBlob blob_fetch(m_Connection, "psg_test_sat_4", std::move(blob), true, timeout_function);
    blob_fetch.SetQueryTimeout(chrono::milliseconds(1));
    blob_fetch.SetUsePrepared(false);
    blob_fetch.SetChunkCallback(chunk_callback);

    CBlobRecord comment_blob;
    comment_blob.SetKey(8091).SetModified(1000412801493).SetFlags(22);
    auto messages_provider = make_shared<CPSGMessages>();
    messages_provider->Set("BLOB_STATUS_SUPPRESSED", "BLOB_STATUS_SUPPRESSED");
    CCassStatusHistoryTaskGetPublicComment comment_fetch(m_Connection, "psg_test_sat_4", comment_blob, error_function);
    comment_fetch.SetCommentCallback(comment_callback);
    comment_fetch.SetMessages(messages_provider);

    atomic_bool has_events{false};
    mutex wait_mutex;
    condition_variable wait_condition;

    struct STestCallbackReceiver
        : public CCassDataCallbackReceiver
    {
        void OnData() override {
            {
                unique_lock<mutex> wait_lck(*cb_wait_mutex);
                *cb_has_events = true;
            }
            ++times_called;
            cb_wait_condition->notify_all();
        }

        atomic_bool* cb_has_events{nullptr};
        mutex* cb_wait_mutex{nullptr};
        condition_variable* cb_wait_condition{nullptr};
        atomic_int times_called{0};
    };

    auto event_callback = make_shared<STestCallbackReceiver>();
    event_callback->cb_has_events = &has_events;
    event_callback->cb_wait_mutex = &wait_mutex;
    event_callback->cb_wait_condition = &wait_condition;
    blob_fetch.SetDataReadyCB(event_callback);
    comment_fetch.SetDataReadyCB(event_callback);

    auto wait_function =
    [&has_events, &wait_condition, &wait_mutex, &blob_finished, &comment_finished, &blob_fetch, &comment_fetch, &false_negative]() {
        while (!blob_finished || !comment_finished) {
            unique_lock<mutex> wait_lck(wait_mutex);
            auto predicate = [&has_events]() -> bool
            {
                return has_events;
            };
            while (!wait_condition.wait_for(wait_lck, chrono::seconds(1), predicate));
            if (has_events) {
                has_events = false;
                if (!comment_finished) {
                    if (comment_fetch.Wait()) {
                        EXPECT_TRUE(blob_finished) << "Blob request should be in finished state when Comment->Wait()==true called";
                        comment_finished = true;
                        EXPECT_FALSE(comment_fetch.HasError()) << "Comment request should be in success state";
                    }
                }
                if (!blob_finished) {
                    if (blob_fetch.Wait()) {
                        blob_finished = true;
                        EXPECT_TRUE(blob_fetch.HasError() || false_negative) << "Blob request should be in failed state";
                    }
                }

            }
        }
    };

    EXPECT_FALSE(blob_fetch.Wait()) << "Blob fetch should not finish on Init()";
    // ??? this_thread::sleep_for(chrono::milliseconds(100));
    EXPECT_FALSE(comment_fetch.Wait()) << "Comment fetch should not finish on Init()";

    wait_function();

    EXPECT_TRUE(comment_called) << "Comment callback should be called";
    EXPECT_TRUE(timeout_called || false_negative) << "Timeout should happen";
    EXPECT_TRUE(blob_finished) << "Blob request should finish";
    EXPECT_TRUE(comment_finished) << "Public comment request should finish";
    EXPECT_EQ(2, event_callback->times_called) << "Error and Data callbacks should be called 2 times in total (Blob (1 data or 1 timeout) and Comment (1 data))";
    event_callback.reset();
}

TEST_F(CBlobTaskLoadBlobTest, LoadBlobRetryLogging)
{
    const string config_section = "TEST";
    auto factory = CCassConnectionFactory::s_Create();
    CNcbiRegistry r;
    r.Set(config_section, "service", "ID_CASS_TEST", IRegistry::fPersistent);
    // Too small value here prevents prepare operation
    r.Set(config_section, "qtimeout", "1", IRegistry::fPersistent);
    r.Set(config_section, "qtimeout_retry", "1", IRegistry::fPersistent);
    r.Set(config_section, "maxretries", "3", IRegistry::fPersistent);
    factory->LoadConfig(r, config_section);
    auto connection = factory->CreateInstance();
    connection->Connect();
    bool timeout_function_called{false};
    auto timeout_function =
        [&timeout_function_called]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            timeout_function_called = true;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eQueryTimeout, code);
            EXPECT_EQ(eDiag_Error, severity);
        };
    bool logging_function_called{false};
    auto logging_function =
        [&logging_function_called]
        (EDiagSev severity, const string & message) -> void {
            regex error_regex(R"(^CassandraQueryRetry: SQL.+; params.+; previous_retries=(\d+); max_retries=3; )"
                              R"(error_code=eQueryTimeout; driver_error=[^;]+; decision=([^\)]+)$)");
            smatch match;
            if (regex_match(message, match, error_regex)) {
                logging_function_called = true;
                auto retries = NStr::StringToNumeric<int>(match[1].str());
                EXPECT_GT(3, retries) << "There should be less than 3 retries";
                EXPECT_EQ(3UL, match.size());
                if (retries < 2) {
                    EXPECT_EQ(eDiag_Warning, severity);
                    EXPECT_EQ("retry_allowed", match[2].str());
                }
                else {
                    EXPECT_EQ(eDiag_Error, severity);
                    EXPECT_EQ("retry_forbidden", match[2].str());
                }
                //cout << "Full match: " << match[0] << endl;
            }
        };
    CCassBlobTaskLoadBlob fetch(connection, m_BigBlobKeyspace, 32435367, true, timeout_function);
    fetch.SetQueryTimeout(chrono::milliseconds(1));
    fetch.SetUsePrepared(false);
    fetch.SetLoggingCB(logging_function);
    testing::internal::CaptureStderr();
    wait_function(fetch);
    testing::internal::GetCapturedStderr();
    EXPECT_TRUE(timeout_function_called) << "Timeout should happen";
    EXPECT_TRUE(logging_function_called) << "Retries are expected to happen";
    EXPECT_TRUE(fetch.HasError()) << "Request should be in failed state";
}

}  // namespace
