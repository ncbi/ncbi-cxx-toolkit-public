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
*   Unit test suite to check CCassStatusHistoryTaskGetPublicComment
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
#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CGetPublicCommentTest
    : public testing::Test
{
 public:
    CGetPublicCommentTest()
     : m_KeyspaceName("satncbi_extended")
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

const char* CGetPublicCommentTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CGetPublicCommentTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CGetPublicCommentTest::m_Connection(nullptr);

static auto comment_wait_function = [](CCassStatusHistoryTaskGetPublicComment& task){
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

static auto blob_wait_function = [](CCassBlobTaskLoadBlob& task){
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

TEST_F(CGetPublicCommentTest, BasicSuppressed) {
    CCassBlobTaskLoadBlob fetch_blob(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        130921029, false,
        error_function
    );
    blob_wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        *blob, error_function
    );
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ("This RefSeq genome was suppressed because "
                "updated RefSeq validation criteria identified problems with the assembly or annotation.",
                comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, BasicSuppressedFromDifferencesTask) {
    CCassBlobTaskLoadBlob fetch_blob(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        130921029, false,
        error_function
    );
    blob_wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        *blob, error_function
    );
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ("This RefSeq genome was suppressed because updated RefSeq validation"
                " criteria identified problems with the assembly or annotation.", comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, BasicWithdrawn) {
    CCassBlobTaskLoadBlob fetch_blob(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        1888383, false,
        error_function
    );
    blob_wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        *blob, error_function
    );
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ("This record was removed at the submitter's request. "
                "Please contact info@ncbi.nlm.nih.gov for further details.", comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, WithdrawnWithDefaultCommentFromSatInfo) {

    CPSGMessages messages;
    string messages_error;
    EXPECT_TRUE(FetchMessages("sat_info", m_Connection, messages, messages_error));
    EXPECT_EQ("", messages_error);

    CCassBlobTaskLoadBlob fetch_blob(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        4317, false,
        error_function
    );
    blob_wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        *blob, error_function
    );
    get_comment.SetMessages(&messages);
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ("This record was removed at the submitter's request. "
                "Contact info@ncbi.nlm.nih.gov for further information", comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultComment) {
    CPSGMessages messages;
    string suppressed_value = "BLOB_STATUS_SUPPRESSED";
    messages.Set(suppressed_value, suppressed_value);
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        blob, error_function
    );
    get_comment.SetMessages(&messages);
    get_comment.SetCommentCallback(
        [&call_count, suppressed_value]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ(suppressed_value, comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentNotConfigured) {
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0}, error_call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName, blob,
        [&error_call_count]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
        {
            ++error_call_count;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eMissData, code);
            EXPECT_EQ(eDiag_Error, severity);
        }
    );
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(0UL, call_count);
    EXPECT_EQ(1UL, error_call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentWrongMessage) {
    CPSGMessages messages;
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0}, error_call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName, blob,
        [&error_call_count]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
        {
            ++error_call_count;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eMissData, code);
            EXPECT_EQ(eDiag_Error, severity);
        }
    );
    get_comment.SetMessages(&messages);
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(0UL, call_count);
    EXPECT_EQ(1UL, error_call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentFromSatInfo) {
    CPSGMessages messages;
    string messages_error;
    EXPECT_TRUE(FetchMessages("sat_info", m_Connection, messages, messages_error));
    EXPECT_EQ("", messages_error);
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        blob, error_function
    );
    get_comment.SetMessages(&messages);
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ("This record was removed from further distribution at the submitter's request. "
                "Contact info@ncbi.nlm.nih.gov for further information", comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, AliveBlob) {
    CCassBlobTaskLoadBlob fetch_blob(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        358006939, false,
        error_function
    );
    blob_wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        *blob, error_function
    );
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(false, isFound);
            EXPECT_EQ("", comment);
        }
    );
    comment_wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

}  // namespace
