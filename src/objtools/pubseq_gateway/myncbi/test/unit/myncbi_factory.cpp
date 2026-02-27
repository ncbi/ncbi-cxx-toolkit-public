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

        static int UvLoopClose (uv_loop_t* loop)
        {
            int rc = uv_loop_close(loop);
            if (rc != 0) {
                uv_run(loop, UV_RUN_DEFAULT);
                rc = uv_loop_close(loop);
                if (rc != 0) {
                    uv_print_all_handles(loop, stderr);
                }
            }
            return rc;
        }

        static char const* const m_Username;
        static char const* const m_Password;
    };

    char const* const  CMyNCBIFactoryTest::m_Username{"mntest4"};
    char const* const  CMyNCBIFactoryTest::m_Password{"mntest11"};

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
        uv_loop_t loop;
        ASSERT_EQ(0, uv_loop_init(&loop));
        auto factory = make_shared<CPSG_MyNCBIFactory>();
        factory->SetMyNCBIURL("http://txproxy.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount");
        factory->SetHttpProxy("linkerd:4140");
        factory->SetVerboseCURL(false);
        factory->SetRequestTimeout(chrono::milliseconds(100));

        auto signin = factory->CreateSignIn(&loop, m_Username, m_Password, error_fn);
        uv_run(&loop, UV_RUN_DEFAULT);
        ASSERT_EQ(signin->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eSuccess);
        ASSERT_EQ(error_cb_called, 0UL);
        auto cookie = signin->GetCookie();
        signin.reset();
        {
            auto whoami = factory->CreateWhoAmI(&loop, cookie, user_fn, error_fn);
            uv_run(&loop, UV_RUN_DEFAULT);
            ASSERT_EQ(whoami->GetResponseStatus(), EPSG_MyNCBIResponseStatus::eSuccess);
            ASSERT_EQ(error_cb_called, 0UL);
            EXPECT_EQ(whoami->GetUserId(), 3500004);
            EXPECT_EQ(whoami->GetUserName(), m_Username);
            EXPECT_EQ(whoami->GetEmailAddress(), "qa@ncbi.nlm.nih.gov");
            EXPECT_EQ(whoami->GetUserId(), user_info.user_id);
            EXPECT_EQ(whoami->GetUserName(), user_info.username);
            EXPECT_EQ(whoami->GetEmailAddress(), user_info.email_address);
            whoami.reset();
        }
        {
            auto whoami_response = factory->ExecuteWhoAmI(&loop, cookie);
            ASSERT_EQ(whoami_response.response_status, EPSG_MyNCBIResponseStatus::eSuccess);
            EXPECT_EQ(whoami_response.response.user_id, 3500004);
            EXPECT_EQ(whoami_response.response.username, m_Username);
            EXPECT_EQ(whoami_response.response.email_address, "qa@ncbi.nlm.nih.gov");
        }
        {
            auto whoami_response = factory->ExecuteWhoAmI(&loop, "cookie");
            EXPECT_EQ(whoami_response.response_status, EPSG_MyNCBIResponseStatus::eError);
            EXPECT_EQ(whoami_response.response.user_id, 0);
            EXPECT_EQ(whoami_response.response.username, string());
            EXPECT_EQ(whoami_response.response.email_address, string());
            EXPECT_EQ(whoami_response.error.status, CRequestStatus::e404_NotFound);
            EXPECT_EQ(whoami_response.error.code, 200);
            EXPECT_EQ(whoami_response.error.severity, eDiag_Error);
            EXPECT_EQ(whoami_response.error.message, "MyNCBIUser data not found: MyNCBI response status - 200; "
                 "MyNCBI response text - '<?xml version=\"1.0\"?><MyNcbiResponse><ERROR>Not signed in</ERROR>"
                 "<SignInURL>https://account.ncbi.nlm.nih.gov/?</SignInURL>"
                 "<SignOutURL>http://www.ncbi.nlm.nih.gov/account/signout/?</SignOutURL>"
                 "<RegisterURL>https://account.ncbi.nlm.nih.gov/signup/?</RegisterURL>"
                 "<HomePageURL>http://www.ncbi.nlm.nih.gov/myncbi/?</HomePageURL></MyNcbiResponse>'");
        }
        factory.reset();
        EXPECT_EQ(0, UvLoopClose(&loop)) << "Failed to close uv_loop properly";
    }

    TEST_F(CMyNCBIFactoryTest, WrongCookie)
    {
        size_t error_cb_called{0};
        auto error_fn =
            [&error_cb_called]
            (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
            {
                EXPECT_EQ(status, CRequestStatus::e404_NotFound);
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
        EXPECT_EQ(0, UvLoopClose(loop)) << "Failed to close uv_loop properly";
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
                EXPECT_EQ(message, "Request failed: MyNCBI response status - 502; MyNCBI response text - ''");
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
        EXPECT_EQ(0, UvLoopClose(loop)) << "Failed to close uv_loop properly";
    }

    TEST_F(CMyNCBIFactoryTest, ResolveAccessPoint)
    {
        string myncbi_url = "http://txproxy.linkerd.ncbi.nlm.nih.gov/v1/service/MyNCBIAccount?txsvc=MyNCBIAccount";
        auto factory = make_shared<CPSG_MyNCBIFactory>();
        factory->SetVerboseCURL(false);
        // good: configuration
        {
            factory->SetResolveTimeout(chrono::milliseconds(400));
            EXPECT_EQ(chrono::milliseconds(400), factory->GetResolveTimeout());
        }
        // good: standard case
        {
            factory->SetHttpProxy("linkerd:4140");
            factory->SetMyNCBIURL(myncbi_url);
            factory->ResolveAccessPoint();
        }
        // good: IP instead of hostname
        {
            factory->SetHttpProxy("130.14.191.21:4140");
            factory->SetMyNCBIURL(myncbi_url);
            factory->ResolveAccessPoint();
        }
        // good: with scheme
        {
            factory->SetMyNCBIURL("http://linkerd:4140");
            factory->SetHttpProxy("");
            factory->ResolveAccessPoint();
        }
        // bad: bad proxy host name
        try {
            factory->SetHttpProxy("wrong_host_value,:80");
            factory->SetMyNCBIURL(myncbi_url);
            factory->ResolveAccessPoint();
        }
        catch(CPSG_MyNCBIException const& ex) {
            EXPECT_EQ("CURL resolution error: 'Couldn't resolve proxy name'", ex.GetMsg());
            EXPECT_EQ(CPSG_MyNCBIException::eMyNCBIResolveError, ex.GetErrCode());
        }
        // bad: non existent host for proxy
        try {
            factory->SetHttpProxy("non.existent.host:80");
            factory->SetMyNCBIURL(myncbi_url);
            factory->ResolveAccessPoint();
        }
        catch(CPSG_MyNCBIException const& ex) {
            EXPECT_EQ("CURL resolution error: 'Couldn't resolve proxy name'", ex.GetMsg());
            EXPECT_EQ(CPSG_MyNCBIException::eMyNCBIResolveError, ex.GetErrCode());
        }
        // bad: wrong scheme
        try {
            factory->SetHttpProxy("ftp://linkerd:80");
            factory->SetMyNCBIURL(myncbi_url);
            factory->ResolveAccessPoint();
        }
        catch(CPSG_MyNCBIException const& ex) {
            EXPECT_EQ("CURL resolution error: 'Couldn't connect to server'", ex.GetMsg());
            EXPECT_EQ(CPSG_MyNCBIException::eMyNCBIResolveError, ex.GetErrCode());
        }
    }

}  // namespace
