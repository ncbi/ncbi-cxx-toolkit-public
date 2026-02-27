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
*   Unit test for blob retrieval performance
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include "test_environment.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
USING_SCOPE(chrono);

class CBlobRetrievalPerformanceTest
    : public testing::Test
{
public:
    CBlobRetrievalPerformanceTest() = default;

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

    vector<int32_t> m_BlobIds{
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
        360033825,360033825,360033825,360033825,360033825,360033825,360033825,
    };

    /*vector<int32_t> m_BlobIds{
        360033825,374157883,360033821,374157879,376280253,376280245,
        376280247,376280233,376280231,376280248,376280229,376280235,376280252,
        376280227,376280225,369683454,369683513,369683506,376280236,374157881,
        360033827,369683531,369683429,376280262,369683136,369683113,374157880,
        369683091,360033709,369683526,372729640,374157876,360033822,376280260,
        376280261,372729646,369683404,369683090,372729650,372729641,374157882,
        372729651,372729648,369683356,372729649,369683532,374157884,376280259,
        360033826,372729642,372729643,381524182,381524180,376280249,369683533,
        372729644,372729645,369683089,376280258,369335827,381524176,381524177,
        376280246,369683078,369683069,369335826,372729638,372729633,374157877,
        374157878,372729630,360033819,360033820,373898376,346278097,373898377,
        377630826,369335825,373898374,373898382,373898379,369335823,381524175,
        369335822,372729634,369335824,381524173,377628687,376280234,381524174,
        351414227,369683056,373898381,381524172,369683042,377625914,377623330,
        369335828,372729639,377619154,377621108,372729636,373898378,351414226,
    };*/

    string m_KeyspaceName{"blob_repartition"};
};

struct SBlobCallbackReceiver
    : public CCassDataCallbackReceiver
{
 public:
    void OnData() override
    {
        signalled = true;
    }
    atomic_bool signalled{false};
};

struct SBlobRequest
{
    int32_t blob_size{0};
    steady_clock::time_point started{};
    steady_clock::time_point finished{};
    atomic<int32_t> blob_size_fetched{0};
    atomic_bool done{false};
    shared_ptr<SBlobCallbackReceiver> receiver;
    unique_ptr<CCassBlobTaskLoadBlob> request;

    void PropsCallback(CBlobRecord const & blob, bool isFound)
    {
        ASSERT_TRUE(isFound) << "Blob record is not found: " << request->GetKey();
        blob_size = blob.GetSize();
    }
    void ChunkCallback(CBlobRecord const &, const unsigned char *, unsigned int size, int chunk_no)
    {
        if (chunk_no == -1) {
            done = true;
            finished = steady_clock::now();
            ASSERT_EQ(blob_size, blob_size_fetched) << "Blob record size is wrong: " << request->GetKey();
        } else {
            blob_size_fetched += size;
        }
    }
};

const char* CBlobRetrievalPerformanceTest::m_TestClusterName = "ID_CASS_TEST";
CSafeStatic<STestEnvironment> CBlobRetrievalPerformanceTest::sm_Env;

static auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

TEST_F(CBlobRetrievalPerformanceTest, DISABLED_LoadBlobs)
{
    vector<unique_ptr<SBlobRequest>> requests;
    int32_t running{0}, max_running{5};
    auto itr = m_BlobIds.begin();
    auto started = steady_clock::now();
    while (itr != m_BlobIds.end() || running > 0) {
        while (running < max_running && itr != m_BlobIds.end()) {
            auto request = make_unique<SBlobRequest>();
            request->started = steady_clock::now();
            request->request = make_unique<CCassBlobTaskLoadBlob>(sm_Env.Get().connection, m_KeyspaceName, *itr, true, error_function);
            request->receiver = make_shared<SBlobCallbackReceiver>();
            request->request->SetDataReadyCB(request->receiver);
            request->request->SetChunkCallback(
                [target = request.get()]
                (CBlobRecord const &blob, const unsigned char *d, unsigned int size, int chunk_no)
                {
                    target->ChunkCallback(blob, d, size, chunk_no);
                }
            );
            request->request->SetPropsCallback(
                [target = request.get()]
                (CBlobRecord const & blob, bool isFound)
                {
                    target->PropsCallback(blob, isFound);
                }
            );
            cout << *itr << " started " << endl;
            request->request->Wait();
            requests.push_back(std::move(request));
            ++running;
            ++itr;
        }
        for (auto & request : requests) {
            if (!request->done && request->receiver->signalled) {
                request->receiver->signalled = false;
                request->request->Wait();
                if (request->done) {
                    --running;
                    cout << request->request->GetKey() << " finished " << endl;
                }
            }
        }
    }

    auto latency_percentile = [](auto &requests, float percentile)
    {
        auto item = requests.begin() + (percentile * requests.size()) / 100;
        nth_element(requests.begin(), item, requests.end());
        return duration_cast<seconds>((*item)->finished - (*item)->started).count();
    };

    duration<double> diff = steady_clock::now() - started;
    cout << "Total time: " << duration_cast<seconds>(diff).count() << "s" << endl;
    int64_t total_size{0};
    for(auto& request : requests) {
        total_size += request->blob_size;
    }
    sort(requests.begin(), requests.end(),
        [](auto const & a, auto const & b) -> bool {
            return (a->finished - a->started) > (b->finished - b->started);
        }
    );
    cout << "Total throughput: " << static_cast<float>(total_size) / duration_cast<seconds>(diff).count() << " bytes/second" << endl;
    cout << "Latency 50perc: " << latency_percentile(requests, 0.5) << " seconds" << endl;
    cout << "Latency 75perc: " << latency_percentile(requests, 0.75) << " seconds" << endl;
    cout << "Latency 90perc: " << latency_percentile(requests, 0.9) << " seconds" << endl;
    cout << "Latency 95perc: " << latency_percentile(requests, 0.95) << " seconds" << endl;
}

}  // namespace
