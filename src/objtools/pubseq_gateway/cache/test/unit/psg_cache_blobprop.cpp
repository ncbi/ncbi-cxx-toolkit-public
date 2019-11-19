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
*   Unit test suite to check blob_prop cache operations
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
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

BEGIN_SCOPE()

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CPsgCacheBlobPropTest
    : public testing::Test
{
 public:
    CPsgCacheBlobPropTest() = default;

 protected:
    static void SetUpTestCase()
    {
        CNcbiIfstream i(GetConfigPath(), ifstream::in | ifstream::binary);
        CNcbiRegistry r(i);
        string file_name = r.GetString("LMDB_CACHE", "blob_prop", "");
        m_Cache = make_unique<CPubseqGatewayCache>("", "", file_name);
        m_Cache->Open({0, 4});
    }

    static string GetConfigPath()
    {
        char buf[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
        string config_path = "psg_cache_unit.ini";
        if (count != -1) {
            config_path = string(dirname(buf)) + "/" + config_path;
        }
        return config_path;
    }

    static void TearDownTestCase()
    {
        m_Cache = nullptr;
    }

    static unique_ptr<CPubseqGatewayCache> m_Cache;
};

unique_ptr<CPubseqGatewayCache> CPsgCacheBlobPropTest::m_Cache(nullptr);

TEST_F(CPsgCacheBlobPropTest, LookupUninitialized)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    CPubseqGatewayCache::TBlobPropResponse response;
    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({0, 4});

    request.SetSat(0).SetSatKey(2054006);
    cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());
}

TEST_F(CPsgCacheBlobPropTest, LookupBlobPropBySatKey)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    CPubseqGatewayCache::TBlobPropResponse response;

    request.SetSat(-10).SetSatKey(2054006);
    m_Cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(-500);
    m_Cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(4).SetSatKey(9965740);
    m_Cache->FetchBlobProp(request, response);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(1114019083516, response[0].last_modified);

    request.Reset().SetSat(0).SetSatKey(2054006);
    m_Cache->FetchBlobProp(request, response);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(823387172086, response[0].last_modified);

    ::psg::retrieval::BlobPropValue value;
    EXPECT_TRUE(value.ParseFromString(response[0].data));
    EXPECT_EQ(0, value.class_());
    EXPECT_EQ(823387172086L, value.date_asn1());
    EXPECT_EQ("SPT", value.div());
    EXPECT_EQ(36UL, value.flags());
    EXPECT_EQ(-2208970800000L, value.hup_date());
    EXPECT_EQ("", value.id2_info());
    EXPECT_EQ(1, value.n_chunks());
    EXPECT_EQ(6, value.owner());
    EXPECT_EQ(1145L, value.size());
    EXPECT_EQ(1145L, value.size_unpacked());
    EXPECT_EQ("cavanaug", value.username());
}

TEST_F(CPsgCacheBlobPropTest, LookupBlobPropBySatKeyLastModified)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    CPubseqGatewayCache::TBlobPropResponse response;

    request.SetSat(-10).SetSatKey(2054006).SetLastModified(823387172086);
    m_Cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(-500).SetLastModified(823387172086);
    m_Cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(2054006).SetLastModified(20);
    m_Cache->FetchBlobProp(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(2054006).SetLastModified(823387172086);
    m_Cache->FetchBlobProp(request, response);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(823387172086, response[0].last_modified);
    EXPECT_EQ(0, response[0].sat);
    EXPECT_EQ(2054006, response[0].sat_key);

    ::psg::retrieval::BlobPropValue value;
    EXPECT_TRUE(value.ParseFromString(response[0].data));
    EXPECT_EQ(823387172086L, value.date_asn1());
    EXPECT_EQ("SPT", value.div());
    EXPECT_EQ("", value.id2_info());
    EXPECT_EQ(1, value.n_chunks());
    EXPECT_EQ(1145L, value.size());
    EXPECT_EQ("cavanaug", value.username());
}

END_SCOPE()

