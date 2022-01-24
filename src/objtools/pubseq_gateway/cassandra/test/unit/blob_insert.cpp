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
*   Unit test for blob loading test
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/insert_extended.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

// @todo Migrate to CCM to prevent data races and blob data leftovers
class CBlobInsertTest
    : public testing::Test
{
 public:
    CBlobInsertTest() = default;

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

    int32_t m_SourceBlobId{9965740};
    string m_SourceKeyspaceName{"psg_test_sat_4"};
    string m_TargetKeyspaceName{"blob_repartition"};
};

static auto wait_function = [](CCassBlobTaskLoadBlob& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

const char* CBlobInsertTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CBlobInsertTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CBlobInsertTest::m_Connection(nullptr);

static auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

TEST_F(CBlobInsertTest, CheckBigBlobLoading)
{
    CCassBlobTaskLoadBlob fetch(m_Connection, m_SourceKeyspaceName, m_SourceBlobId, true, error_function);
    wait_function(fetch);
    EXPECT_TRUE(fetch.IsBlobPropsFound());
    auto blob = fetch.ConsumeBlobRecord();
    blob->VerifyBlobSize();

    // Lets take 24 bits from time for fake_key
    int64_t fake_modified = time(0);
    int32_t fake_key = static_cast<int32_t>(fake_modified & 0xFFFFFF);
    blob->SetKey(fake_key);
    blob->SetModified(fake_modified);
    blob->SetBigBlobSchema(true);

    CCassBlobTaskInsertExtended insert(m_Connection, m_TargetKeyspaceName, blob.get(), false, error_function);
    insert.Wait();

    CCassBlobTaskLoadBlob fetch_loaded(m_Connection, m_TargetKeyspaceName, fake_key, true, error_function);
    wait_function(fetch_loaded);
    EXPECT_TRUE(fetch_loaded.IsBlobPropsFound());
    auto blob_loaded = fetch_loaded.ConsumeBlobRecord();
    blob_loaded->VerifyBlobSize();
    EXPECT_TRUE(blob_loaded->GetFlag(EBlobFlags::eBigBlobSchema));

    auto query = m_Connection->NewQuery();
    query->SetSQL("SELECT last_modified FROM " + m_TargetKeyspaceName + ".big_blob_chunk WHERE sat_key = ? AND last_modified = ? AND chunk_no = ?", 3);
    query->BindInt32(0, fake_key);
    query->BindInt64(1, fake_modified);
    query->BindInt32(2, 0);
    query->Query();
    query->NextRow();
    EXPECT_EQ(fake_modified, query->FieldGetInt64Value(0));

    CCassBlobTaskDelete delete_loaded(m_Connection, m_TargetKeyspaceName, fake_key, false, error_function);
    delete_loaded.Wait();

    query = m_Connection->NewQuery();
    query->SetSQL("SELECT last_modified FROM " + m_TargetKeyspaceName + ".big_blob_chunk WHERE sat_key = ? AND last_modified = ? AND chunk_no = ?", 3);
    query->BindInt32(0, fake_key);
    query->BindInt64(1, fake_modified);
    query->BindInt32(2, 0);
    query->Query();
    query->NextRow();
    EXPECT_TRUE(query->IsEOF());
}

}  // namespace
