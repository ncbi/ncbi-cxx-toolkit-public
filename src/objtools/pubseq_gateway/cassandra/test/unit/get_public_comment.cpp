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
#include <objtools/pubseq_gateway/impl/cassandra/status_history/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include "test_environment.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CGetPublicCommentTest
    : public testing::Test
{
public:
    CGetPublicCommentTest() = default;

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

    string m_KeyspaceName{"satncbi_extended"};
};

const char* CGetPublicCommentTest::m_TestClusterName = "ID_CASS_TEST";
CSafeStatic<STestEnvironment> CGetPublicCommentTest::sm_Env;

auto wait_function = [](auto& task)
{
    bool done = task.Wait();
    while (!done) {
        usleep(100);
        done = task.Wait();
    }
    return done;
};

auto get_messages_fn =
[] (string const& cluster_name, shared_ptr<CCassConnection> connection) -> auto
{
    const string config_section = "TEST";
    auto r = make_shared<CNcbiRegistry>();
    r->Set(config_section, "service", string(cluster_name), IRegistry::fPersistent);
    CSatInfoSchemaProvider schema_provider("sat_info3", "PSG_TEST", std::move(connection), r, config_section);
    EXPECT_EQ(ESatInfoRefreshMessagesResult::eMessagesUpdated, schema_provider.RefreshMessages(true));
    return schema_provider.GetMessages();
};

static auto error_function =
[](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
{
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

TEST_F(CGetPublicCommentTest, BasicSuppressed)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 130921029, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, BasicSuppressedFromDifferencesTask)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 130921029, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, BasicWithdrawn)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 1888383, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, WithdrawnWithDefaultCommentFromSatInfo)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 4317, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
    get_comment.SetMessages(get_messages_fn("PSG_TEST", sm_Env.Get().connection));
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, WithdrawnWithDefaultCommentFromSatInfo2)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 4317, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_TRUE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
    get_comment.SetMessages(get_messages_fn("PSG_TEST", sm_Env.Get().connection));
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultComment)
{
    auto messages = make_shared<CPSGMessages>();
    string suppressed_value = "BLOB_STATUS_SUPPRESSED";
    messages->Set(suppressed_value, suppressed_value);
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, blob, error_function);
    get_comment.SetMessages(messages);
    get_comment.SetCommentCallback(
        [&call_count, suppressed_value]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(true, isFound);
            EXPECT_EQ(suppressed_value, comment);
        }
    );
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentNotConfigured)
{
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0}, error_call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        sm_Env.Get().connection, m_KeyspaceName, blob,
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
    wait_function(get_comment);
    EXPECT_EQ(0UL, call_count);
    EXPECT_EQ(1UL, error_call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentWrongMessage)
{
    auto messages = make_shared<CPSGMessages>();
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0}, error_call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(
        sm_Env.Get().connection, m_KeyspaceName, blob,
        [&error_call_count]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
        {
            ++error_call_count;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eMissData, code);
            EXPECT_EQ(eDiag_Error, severity);
        }
    );
    get_comment.SetMessages(messages);
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
        }
    );
    wait_function(get_comment);
    EXPECT_EQ(0UL, call_count);
    EXPECT_EQ(1UL, error_call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentFromSatInfo)
{
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);
    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, blob, error_function);
    get_comment.SetMessages(get_messages_fn("PSG_TEST", sm_Env.Get().connection));
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, SuppressedWithDefaultCommentFromSatInfo2)
{
    const string config_section = "TEST";
    auto r = make_shared<CNcbiRegistry>();
    r->Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
    CSatInfoSchemaProvider schema_provider("sat_info3", "PSG_TEST", sm_Env.Get().connection, r, config_section);
    EXPECT_EQ(ESatInfoRefreshMessagesResult::eMessagesUpdated, schema_provider.RefreshMessages(true));
    CBlobRecord blob(numeric_limits<CBlobRecord::TSatKey>::max());
    blob.SetSuppress(true);

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, blob, error_function);
    get_comment.SetMessages(schema_provider.GetMessages());
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
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, AliveBlob)
{
    CCassBlobTaskLoadBlob fetch_blob(sm_Env.Get().connection, m_KeyspaceName, 538172168, false, error_function);
    wait_function(fetch_blob);
    ASSERT_TRUE(fetch_blob.IsBlobPropsFound());

    auto blob = fetch_blob.ConsumeBlobRecord();
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eSuppress));
    EXPECT_FALSE(blob->GetFlag(EBlobFlags::eWithdrawn));

    size_t call_count{0};
    CCassStatusHistoryTaskGetPublicComment get_comment(sm_Env.Get().connection, m_KeyspaceName, *blob, error_function);
    get_comment.SetCommentCallback(
        [&call_count]
        (string comment, bool isFound)
        {
            ++call_count;
            EXPECT_EQ(false, isFound);
            EXPECT_EQ("", comment);
        }
    );
    wait_function(get_comment);
    EXPECT_EQ(1UL, call_count);
}

TEST_F(CGetPublicCommentTest, FetchBlobStatusHistory)
{
    CCassStatusHistoryTaskFetch fetch_status_history(sm_Env.Get().connection, m_KeyspaceName, 4317, error_function);
    wait_function(fetch_status_history);
    auto status_history_ptr = fetch_status_history.Consume();
    ASSERT_NE(nullptr, status_history_ptr);
    EXPECT_EQ(4317, status_history_ptr->GetSatKey());
    EXPECT_EQ(773965176963, status_history_ptr->GetDoneWhen());
    EXPECT_EQ(
        "{sat_key: 4317, done_when: '1994-07-11 18:19:36.963000', flags: 2, user: 'cavanaug', "
        "comment: 'Suppress: L15527 : submittor Ron Swanstrom reports that this sequence erroneously"
        " attributed to wrong patient in HIV study : sequences will be resubmitted in the future"
        " : per D. Airozo (GenBank)', public comment: '',"
        " replaces_ids: [], replaces: 0, replaced_by_ids: []}",
        status_history_ptr->ToString()
    );
}

}  // namespace
