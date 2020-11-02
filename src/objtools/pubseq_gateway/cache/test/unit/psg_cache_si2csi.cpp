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
*   Unit test suite to check si2csi cache operations
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <memory>
#include <string>

#include <unistd.h>
#include <libgen.h>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbistre.hpp>

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

BEGIN_SCOPE()

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CPsgCacheSi2CsiTest
    : public testing::Test
{
 public:
    CPsgCacheSi2CsiTest() = default;

 protected:
    static void SetUpTestCase()
    {
        char buf[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
        string config_path = "psg_cache_unit.ini";
        if (count != -1) {
            config_path = string(dirname(buf)) + "/" + config_path;
        }

        CNcbiIfstream i(config_path, ifstream::in | ifstream::binary);
        CNcbiRegistry r(i);
        string file_name = r.GetString("LMDB_CACHE", "si2csi", "");
        m_Cache = make_unique<CPubseqGatewayCache>("", file_name, "");
        m_Cache->Open({});
    }

    static void TearDownTestCase()
    {
        m_Cache = nullptr;
    }

    static unique_ptr<CPubseqGatewayCache> m_Cache;
};

unique_ptr<CPubseqGatewayCache> CPsgCacheSi2CsiTest::m_Cache(nullptr);

TEST_F(CPsgCacheSi2CsiTest, LookupUninitialized)
{
    CPubseqGatewayCache::TSi2CsiRequest request;

    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({});
    request.SetSecSeqId("3643631");
    EXPECT_TRUE(cache->FetchSi2Csi(request).empty());
}

TEST_F(CPsgCacheSi2CsiTest, LookupCsiBySeqId)
{
    CPubseqGatewayCache::TSi2CsiRequest request;

    request.SetSecSeqId("FAKE");
    EXPECT_TRUE(m_Cache->FetchSi2Csi(request).empty());

    request.Reset().SetSecSeqId("");
    EXPECT_TRUE(m_Cache->FetchSi2Csi(request).empty());

    request.Reset().SetSecSeqId("3643631");
    auto response = m_Cache->FetchSi2Csi(request);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(12, response[0].GetSecSeqIdType());

    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3643631, response[0].GetGI());
}

TEST_F(CPsgCacheSi2CsiTest, LookupCsiBySeqIdSeqIdType)
{
    CPubseqGatewayCache::TSi2CsiRequest request;

    request.SetSecSeqId("FAKE").SetSecSeqIdType(0);
    EXPECT_TRUE(m_Cache->FetchSi2Csi(request).empty());

    request.Reset().SetSecSeqId("").SetSecSeqIdType(0);
    EXPECT_TRUE(m_Cache->FetchSi2Csi(request).empty());

    request.Reset().SetSecSeqId("3643631").SetSecSeqIdType(0);
    EXPECT_TRUE(m_Cache->FetchSi2Csi(request).empty());

    request.Reset().SetSecSeqId("3643631").SetSecSeqIdType(12);
    auto response = m_Cache->FetchSi2Csi(request);
    ASSERT_EQ(1UL, response.size());

    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3643631, response[0].GetGI());
}

TEST_F(CPsgCacheSi2CsiTest, LookupCsiForLastRecord)
{
    auto response = m_Cache->FetchSi2CsiLast();
    ASSERT_FALSE(response.empty());
    auto last = response[response.size() - 1];

    CPubseqGatewayCache::TSi2CsiRequest request;
    request.SetSecSeqId(last.GetSecSeqId()).SetSecSeqIdType(last.GetSecSeqIdType());
    response = m_Cache->FetchSi2Csi(request);

    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(last.GetSecSeqId(), response[0].GetSecSeqId());
    EXPECT_EQ(last.GetSecSeqIdType(), response[0].GetSecSeqIdType());
    EXPECT_EQ(last.GetAccession(), response[0].GetAccession());
    EXPECT_EQ(last.GetVersion(), response[0].GetVersion());
    EXPECT_EQ(last.GetSeqIdType(), response[0].GetSeqIdType());
    EXPECT_EQ(last.GetGI(), response[0].GetGI());
}

END_SCOPE()

