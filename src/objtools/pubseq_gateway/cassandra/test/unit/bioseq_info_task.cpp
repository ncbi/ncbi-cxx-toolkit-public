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
#include <string>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CBioseqInfoTaskFetchTest
    : public testing::Test
{
 public:
    CBioseqInfoTaskFetchTest()
     : m_KeyspaceName("idmain2")
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

const char* CBioseqInfoTaskFetchTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CBioseqInfoTaskFetchTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CBioseqInfoTaskFetchTest::m_Connection(nullptr);

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
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "FAKEACCESSION", 1, false, 1, false, 0, false,
        [&call_count, &actual_code](CBioseqInfoRecord &&, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e404_NotFound, actual_code);
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionMultiple) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 1, false, 1, false, 0, false,
        [&call_count, &actual_code](CBioseqInfoRecord &&, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e300_MultipleChoices, actual_code);
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionVersionMultiple) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 0, true, 1, false, 0, false,
        [&call_count, &actual_code](CBioseqInfoRecord &&, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e300_MultipleChoices, actual_code);
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionVersionSingle) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CBioseqInfoRecord actual_record;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 1, true, 1, false, 0, false,
        [&call_count, &actual_code, &actual_record](CBioseqInfoRecord &&record, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
            actual_record = move(record);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e200_Ok, actual_code);
    EXPECT_EQ("AC005299", actual_record.GetAccession());
    EXPECT_EQ(1, actual_record.GetVersion());
    EXPECT_EQ(5, actual_record.GetSeqIdType());
    EXPECT_EQ(3786039, actual_record.GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionSeqIdType) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CBioseqInfoRecord actual_record;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 1, false, 5, true, 0, false,
        [&call_count, &actual_code, &actual_record](CBioseqInfoRecord &&record, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
            actual_record = move(record);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e200_Ok, actual_code);
    EXPECT_EQ("AC005299", actual_record.GetAccession());
    EXPECT_EQ(1, actual_record.GetVersion());
    EXPECT_EQ(5, actual_record.GetSeqIdType());
    EXPECT_EQ(3786039, actual_record.GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionGISingle) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CBioseqInfoRecord actual_record;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 0, false, 0, false, 3643631, true,
        [&call_count, &actual_code, &actual_record](CBioseqInfoRecord &&record, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
            actual_record = move(record);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e200_Ok, actual_code);
    EXPECT_EQ("AC005299", actual_record.GetAccession());
    EXPECT_EQ(0, actual_record.GetVersion());
    EXPECT_EQ(5, actual_record.GetSeqIdType());
    EXPECT_EQ(3643631, actual_record.GetGI());
}

TEST_F(CBioseqInfoTaskFetchTest, AccessionGIWrongVersion) {
    size_t call_count{0};
    CRequestStatus::ECode actual_code;
    CBioseqInfoRecord actual_record;
    CCassBioseqInfoTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, "AC005299", 5, true, 0, false, 3643631, true,
        [&call_count, &actual_code, &actual_record](CBioseqInfoRecord &&record, CRequestStatus::ECode code) {
            ++call_count;
            actual_code = code;
            actual_record = move(record);
        },
        error_function
    );
    wait_function(fetch);
    EXPECT_EQ(1UL, call_count);
    EXPECT_EQ(CRequestStatus::e404_NotFound, actual_code);
}

}  // namespace
