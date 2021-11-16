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
*   Unit test suite to check name resolver usage
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

auto populate_environment =
[](map<string, string> const& env_data) {
    for (auto const& item : env_data) {
        setenv(item.first.c_str(), item.second.c_str(), 1);
    }
};

auto clear_environment =
[](map<string, string> const& env_data) {
    for (auto const& item : env_data) {
        unsetenv(item.first.c_str());
    }
};

TEST(CCassNameResolverTest, NamerdResolverTest) {
    const string config_section = "TEST";
    CNcbiRegistry r;
    r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
    map<string, string> env{
        {"CONN_LBSMD_DISABLE", "1"},
        {"CONN_NAMERD_ENABLE", "1"},
        //{"http_proxy", "linkerd:4140"},
        //{"DIAG_POST_LEVEL", "Info"},
        {"DIAG_TRACE", "1"},
        {"CONN_DEBUG_PRINTOUT", "ALL"},
    };
    populate_environment(env);
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(r, config_section);
    string host;
    short port;
    string stderr_content;
    //string stdout_content;
    testing::internal::CaptureStderr();
    //testing::internal::CaptureStdout();
    try {
        factory->GetHostPort(host, port);
        testing::internal::GetCapturedStderr();
        //testing::internal::GetCapturedStdout();
    }
    catch (CCassandraException& exception) {
        stderr_content = testing::internal::GetCapturedStderr();
        //stdout_content = testing::internal::GetCapturedStdout();
    }
    EXPECT_EQ("", stderr_content) << "Stderr content should be empty";
    EXPECT_EQ(9042, port);
    vector<string> items;
    NStr::Split(host, ",", items, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    for (const auto & item : items) {
        EXPECT_EQ(0UL, item.find("130.14."));
        string host_number = item.substr(string("130.14.XX.").size());
        int host_id = NStr::StringToNumeric<int>(host_number);
        EXPECT_TRUE(host_id >= 1 && host_id <= 254);
    }
    clear_environment(env);
}

TEST(CCassNameResolverTest, LBSMDResolverTest) {
    const string config_section = "TEST";
    CNcbiRegistry r;
    r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
    map<string, string> env{
        {"CONN_LBSMD_DISABLE", "0"},
        {"CONN_NAMERD_ENABLE", "0"}};
    populate_environment(env);
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(r, config_section);
    string host;
    short port;
    factory->GetHostPort(host, port);
    EXPECT_EQ(9042, port);
    vector<string> items;
    NStr::Split(host, ",", items, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    for (const auto & item : items) {
        EXPECT_EQ(0UL, item.find("130.14."));
        string host_number = item.substr(string("130.14.XX.").size());
        int host_id = NStr::StringToNumeric<int>(host_number);
        EXPECT_TRUE(host_id >= 1 && host_id <= 254);
    }
    clear_environment(env);
}

TEST(CCassNameResolverTest, NoneResolverTest) {
    const string config_section = "TEST";
    CNcbiRegistry r;
    r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
    map<string, string> env{
        {"CONN_LBSMD_DISABLE", "1"},
        {"CONN_NAMERD_ENABLE", "0"},
        {"CONN_LBDNS_ENABLE", "0"},
        {"CONN_LBOS_ENABLE", "0"},
        {"CONN_LINKERD_ENABLE", "0"},
        {"CONN_LOCAL_ENABLE", "0"},
        {"CONN_DISPD_DISABLE", "1"}};
    populate_environment(env);
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(r, config_section);
    string host;
    short port{0};
    try {
        factory->GetHostPort(host, port);
        FAIL() << "Name resolution without name resolver should throw";
    } catch (CCassandraException& exception) {
        ASSERT_EQ(exception.GetErrCode(), CCassandraException::eGeneric) << "Exception code should be CCassandraException::eGeneric";
    }

    EXPECT_EQ(0, port);
    EXPECT_EQ("", host);
    clear_environment(env);
}

TEST(CCassNameResolverTest, DefaultResolverTest) {
    const string config_section = "TEST";
    CNcbiRegistry r;
    r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(r, config_section);
    string host;
    short port;
    factory->GetHostPort(host, port);
    EXPECT_EQ(9042, port);
    EXPECT_EQ(0UL, host.find("130.14."));
}

TEST(CCassNameResolverTest, HostListTest) {
    const string config_section = "TEST";
    CNcbiRegistry r;
    r.Set(config_section, "service", "idtest111:9999", IRegistry::fPersistent);
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(r, config_section);
    string host;
    short port;
    factory->GetHostPort(host, port);
    EXPECT_EQ(9999, port);
    EXPECT_EQ("idtest111", host);
}

}  // namespace
