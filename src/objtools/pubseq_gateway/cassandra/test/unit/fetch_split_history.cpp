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
*   Unit test suite to check CCassBlobTaskFetchSplitHistory task
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/fetch_split_history.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CFetchSplitHistoryTest
    : public testing::Test
{
 public:
    CFetchSplitHistoryTest()
     : m_KeyspaceName("psg_test_sat_4")
     , m_Timeout(10000)
    {}

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

    string m_KeyspaceName;
    unsigned int m_Timeout;
};

const char* CFetchSplitHistoryTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CFetchSplitHistoryTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CFetchSplitHistoryTest::m_Connection(nullptr);

TEST_F(CFetchSplitHistoryTest, EmptyHistory) {
    size_t call_count{0};
    vector<SSplitHistoryRecord> actual_result;
    CCassBlobTaskFetchSplitHistory fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, 1,
        [&call_count, &actual_result](vector<SSplitHistoryRecord> && result) {
            ++call_count;
            swap(actual_result, result);
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(0UL, actual_result.size());
}

TEST_F(CFetchSplitHistoryTest, FetchAllVersions) {
    size_t call_count{0};
    CBlobRecord::TSatKey sat_key = 340865818;
    vector<SSplitHistoryRecord> actual_result;
    CCassBlobTaskFetchSplitHistory fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, sat_key,
        [&call_count, &actual_result](vector<SSplitHistoryRecord> && result) {
            ++call_count;
            swap(actual_result, result);
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(4UL, actual_result.size());

    EXPECT_EQ(sat_key, actual_result[0].sat_key);
    EXPECT_EQ(1591109641, actual_result[0].split_version);
    EXPECT_EQ(1565313318883LL, actual_result[0].modified);
    EXPECT_EQ("40.261500448.96.1591109641", actual_result[0].id2_info);
}

TEST_F(CFetchSplitHistoryTest, FetchOneVersion) {
    size_t call_count{0};
    CBlobRecord::TSatKey sat_key = 340865818;
    vector<SSplitHistoryRecord> actual_result;
    CCassBlobTaskFetchSplitHistory fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, sat_key, 1565300000,
        [&call_count, &actual_result](vector<SSplitHistoryRecord> && result) {
            ++call_count;
            swap(actual_result, result);
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, actual_result.size());

    EXPECT_EQ(sat_key, actual_result[0].sat_key);
    EXPECT_EQ(1565300000, actual_result[0].split_version);
    EXPECT_EQ(1565313318883LL, actual_result[0].modified);
    EXPECT_EQ("25.116773936.96.1565300000", actual_result[0].id2_info);
}

}  // namespace
