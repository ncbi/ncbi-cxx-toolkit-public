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
*   Unit test suite to check CPSG_MyNCBIFactory
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <chrono>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>
#include <corelib/request_status.hpp>

#include <uv.h>

#include <objtools/pubseq_gateway/impl/myncbi/myncbi_factory.hpp>

namespace {

    USING_NCBI_SCOPE;

    class CMyNCBIFactoryTest
        : public testing::Test
    {
    public:
        CMyNCBIFactoryTest() = default;

    protected:
        static void SetUpTestCase ()
        {
            string error;
            ASSERT_TRUE(CPSG_MyNCBIFactory::InitGlobal(error));
        }

        static void TearDownTestCase ()
        {
            CPSG_MyNCBIFactory::CleanupGlobal();
        }

        static string m_Username;
        static string m_Password;
    };

    string CMyNCBIFactoryTest::m_Username{"mntest4"};
    string CMyNCBIFactoryTest::m_Password{"mntest11"};

    TEST_F(CMyNCBIFactoryTest, Basic)
    {
        size_t error_cb_called{0};
        auto error_fn =
            [&error_cb_called]
            (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
            {
                ++error_cb_called;
            };
        CPSG_MyNCBIRequest_WhoAmI::SUserInfo user_info;
        auto user_fn = [&user_info](CPSG_MyNCBIRequest_WhoAmI::SUserInfo info)
        {
            user_info = info;
        };
        auto loop = uv_default_loop();
        auto factory = make_shared<CPSG_MyNCBIFactory>();
        factory->SetMyNCBIURL("http://txproxy.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount");
        factory->SetHttpProxy("linkerd:4140");
        factory->SetVerboseCURL(false);
        factory->SetRequestTimeout(chrono::milliseconds(100));

        auto signin = factory->CreateSignIn(loop, m_Username, m_Password, error_fn);
        uv_run(loop, UV_RUN_DEFAULT);
        ASSERT_EQ(signin->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eSuccess);
        ASSERT_EQ(error_cb_called, 0UL);
        auto whoami = factory->CreateWhoAmI(loop, signin->GetCookie(), user_fn, error_fn);
        signin.reset();
        uv_run(loop, UV_RUN_DEFAULT);
        ASSERT_EQ(whoami->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eSuccess);
        ASSERT_EQ(error_cb_called, 0UL);
        EXPECT_EQ(whoami->GetUserId(), 3500004);
        EXPECT_EQ(whoami->GetUserName(), m_Username);
        EXPECT_EQ(whoami->GetEmailAddress(), "qa@ncbi.nlm.nih.gov");
        EXPECT_EQ(whoami->GetUserId(), user_info.user_id);
        EXPECT_EQ(whoami->GetUserName(), user_info.username);
        EXPECT_EQ(whoami->GetEmailAddress(), user_info.email_address);
        whoami.reset();
        factory.reset();
    }

    TEST_F(CMyNCBIFactoryTest, WrongCookie)
    {
        size_t error_cb_called{0};
        auto error_fn =
            [&error_cb_called]
            (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
            {
                EXPECT_EQ(status, CRequestStatus::e500_InternalServerError);
                EXPECT_EQ(code, 200);
                EXPECT_EQ(severity, eDiag_Error);
                EXPECT_FALSE(message.empty());
                ++error_cb_called;
            };
        CPSG_MyNCBIRequest_WhoAmI::SUserInfo user_info;
        auto user_fn = [&user_info](CPSG_MyNCBIRequest_WhoAmI::SUserInfo info)
        {
            user_info = info;
        };
        auto loop = uv_default_loop();
        auto factory = make_shared<CPSG_MyNCBIFactory>();
        factory->SetMyNCBIURL("http://txproxy.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount");
        factory->SetHttpProxy("linkerd:4140");
        factory->SetVerboseCURL(false);

        auto whoami = factory->CreateWhoAmI(loop, "XXXXXXXXXXX", user_fn, error_fn);
        uv_run(loop, UV_RUN_DEFAULT);
        ASSERT_EQ(whoami->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eError);
        ASSERT_EQ(error_cb_called, 1UL);
        EXPECT_EQ(whoami->GetUserId(), 0);
        EXPECT_EQ(whoami->GetUserName(), "");
        EXPECT_EQ(whoami->GetEmailAddress(), "");
        EXPECT_EQ(whoami->GetUserId(), user_info.user_id);
        EXPECT_EQ(whoami->GetUserName(), user_info.username);
        EXPECT_EQ(whoami->GetEmailAddress(), user_info.email_address);
        whoami.reset();
        factory.reset();
    }

    TEST_F(CMyNCBIFactoryTest, WrongURL)
    {
        size_t error_cb_called{0};
        auto error_fn =
            [&error_cb_called]
            (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
            {
                EXPECT_EQ(status, CRequestStatus::e500_InternalServerError);
                EXPECT_EQ(code, 502);
                EXPECT_EQ(severity, eDiag_Error);
                EXPECT_EQ(message, "Request failed: response status - 502; response text - ''");
                ++error_cb_called;
            };
        CPSG_MyNCBIRequest_WhoAmI::SUserInfo user_info;
        auto user_fn = [&user_info](CPSG_MyNCBIRequest_WhoAmI::SUserInfo info)
        {
            user_info = info;
        };
        auto loop = uv_default_loop();
        auto factory = make_shared<CPSG_MyNCBIFactory>();
        factory->SetMyNCBIURL("http://xxxxxx.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount");
        factory->SetHttpProxy("linkerd:4140");
        factory->SetVerboseCURL(false);

        auto whoami = factory->CreateWhoAmI(loop, "XXXXXXXXXXX", user_fn, error_fn);
        uv_run(loop, UV_RUN_DEFAULT);
        ASSERT_EQ(whoami->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eError);
        ASSERT_EQ(error_cb_called, 1UL);
        EXPECT_EQ(whoami->GetUserId(), 0);
        EXPECT_EQ(whoami->GetUserName(), "");
        EXPECT_EQ(whoami->GetEmailAddress(), "");
        EXPECT_EQ(whoami->GetUserId(), user_info.user_id);
        EXPECT_EQ(whoami->GetUserName(), user_info.username);
        EXPECT_EQ(whoami->GetEmailAddress(), user_info.email_address);
        whoami.reset();
        factory.reset();
    }

}  // namespace
