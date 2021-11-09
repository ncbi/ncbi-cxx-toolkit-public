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
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

TEST(CCassNameResolverTest, NamerdResolverTest) {
    const string config_section = "TEST";
    {
        auto factory = CCassConnectionFactory::s_Create();
        CNcbiRegistry r;
        r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
        r.Set(config_section, "name_resolver", "NAMERD", IRegistry::fPersistent);
        factory->LoadConfig(r, config_section);
        string host;
        short port;
        factory->GetHostPort(host, port);
        EXPECT_EQ(9042, port);
        vector<string> items;
        NStr::Split(host, ",", items, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        for (const auto & item : items) {
            string prefix, suffix;
            EXPECT_TRUE(NStr::SplitInTwo(item, ".", prefix, suffix));
            EXPECT_EQ("be-md.ncbi.nlm.nih.gov", suffix);
            EXPECT_EQ(0UL, prefix.find("idtest"));
            string host_number = prefix.substr(string("idtest").size());
            int host_id = NStr::StringToNumeric<int>(host_number);
            EXPECT_TRUE((host_id >= 111 && host_id <= 114) || (host_id >= 211 && host_id <= 214));
        }
    }
}

TEST(CCassNameResolverTest, LBSMResolverTest) {
    const string config_section = "TEST";
    {
        auto factory = CCassConnectionFactory::s_Create();
        CNcbiRegistry r;
        r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
        r.Set(config_section, "name_resolver", "LBSM", IRegistry::fPersistent);
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
    }
}

TEST(CCassNameResolverTest, DefaultResolverTest) {
    const string config_section = "TEST";
    {
        auto factory = CCassConnectionFactory::s_Create();
        CNcbiRegistry r;
        r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
        factory->LoadConfig(r, config_section);
        string host;
        short port;
        factory->GetHostPort(host, port);
        EXPECT_EQ(9042, port);
        EXPECT_EQ(0UL, host.find("130.14."));
    }
}

TEST(CCassNameResolverTest, WrongResolverTest) {
    const string config_section = "TEST";
    {
        auto factory = CCassConnectionFactory::s_Create();
        CNcbiRegistry r;
        r.Set(config_section, "service", "SNPS_CASS_TEST", IRegistry::fPersistent);
        r.Set(config_section, "name_resolver", "NONEXISTENTRESOLVER", IRegistry::fPersistent);
        EXPECT_THROW(factory->LoadConfig(r, config_section), CCassandraException);
    }
}

TEST(CCassNameResolverTest, HostListTest) {
    const string config_section = "TEST";
    {
        auto factory = CCassConnectionFactory::s_Create();
        CNcbiRegistry r;
        r.Set(config_section, "service", "idtest111:9999", IRegistry::fPersistent);
        r.Set(config_section, "name_resolver", "LBSM", IRegistry::fPersistent);
        factory->LoadConfig(r, config_section);
        string host;
        short port;
        factory->GetHostPort(host, port);
        EXPECT_EQ(9999, port);
        EXPECT_EQ("idtest111", host);
    }
}

}  // namespace
