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
#include <objtools/pubseq_gateway/impl/cassandra/si2csi_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CPsgApiTest
    : public testing::Test
{
 public:
    CPsgApiTest()
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

const char* CPsgApiTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CPsgApiTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CPsgApiTest::m_Connection(nullptr);

auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

auto bioseq_wait = [](CCassBioseqInfoTaskFetch& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

auto si2csi_wait = [](CCassSI2CSITaskFetch& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

TEST_F(CPsgApiTest, ResolutionByGi) {
    size_t call_count{0};
    vector<CSI2CSIRecord> si2csi_records;
    CSi2CsiFetchRequest si2csi_request;
    si2csi_request.SetSecSeqId("3643631");
    CCassSI2CSITaskFetch si2csi_fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, si2csi_request,
        [&call_count, &si2csi_records](vector<CSI2CSIRecord> &&records) {
            ++call_count;
            si2csi_records = move(records);
        },
        error_function
    );

    si2csi_wait(si2csi_fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, si2csi_records.size());

    CBioseqInfoFetchRequest bioseq_request;
    bioseq_request
        .SetAccession(si2csi_records[0].GetAccession())
        .SetVersion(si2csi_records[0].GetVersion())
        .SetSeqIdType(si2csi_records[0].GetSeqIdType())
        .SetGI(si2csi_records[0].GetGI());

    call_count = 0;
    vector<CBioseqInfoRecord> bioseq_records;
    CCassBioseqInfoTaskFetch bioseq_fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName, bioseq_request,
        [&call_count, &bioseq_records](vector<CBioseqInfoRecord> &&records) {
            ++call_count;
            bioseq_records = move(records);
        },
        error_function
    );
    bioseq_wait(bioseq_fetch);
    EXPECT_EQ(1UL, call_count);
    ASSERT_EQ(1UL, bioseq_records.size());

    EXPECT_EQ("AC005299", bioseq_records[0].GetAccession());
    EXPECT_EQ(0, bioseq_records[0].GetVersion());
    EXPECT_EQ(5, bioseq_records[0].GetSeqIdType());
    EXPECT_EQ(3643631, bioseq_records[0].GetGI());
    EXPECT_EQ(0, bioseq_records[0].GetState());
    EXPECT_EQ(10, bioseq_records[0].GetSeqState());
    EXPECT_EQ(2UL, bioseq_records[0].GetSeqIds().size());
    set<tuple<int16_t, string>> expected_seq_ids;
    // Own
    expected_seq_ids.insert(make_tuple<int16_t, string>(12, "3643631"));

    // Inherited
    expected_seq_ids.insert(make_tuple<int16_t, string>(11, "UOKNOR|2H2"));
    EXPECT_EQ(expected_seq_ids, bioseq_records[0].GetSeqIds());
}


}  // namespace
