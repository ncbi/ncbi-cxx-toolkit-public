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
#include <corelib/ncbi_safe_static.hpp>

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>
#include <util/lmdbxx/lmdb++.h>

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
        sm_CacheFilePath.Get() = r.GetString("LMDB_CACHE", "blob_prop", "");
        sm_Cache = new CPubseqGatewayCache("", "", sm_CacheFilePath.Get());
        sm_Cache->Open({0, 4});
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
        if (sm_Cache) {
            delete sm_Cache;
            sm_Cache = nullptr;
        }
    }

    static CPubseqGatewayCache* sm_Cache;
    static CSafeStatic<string> sm_CacheFilePath;
};

CPubseqGatewayCache* CPsgCacheBlobPropTest::sm_Cache{nullptr};
CSafeStatic<string> CPsgCacheBlobPropTest::sm_CacheFilePath;

TEST_F(CPsgCacheBlobPropTest, LookupUninitialized)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({0, 4});
    EXPECT_EQ(0U, cache->GetBlobPropFlags());

    request.SetSat(0).SetSatKey(2054006);
    auto response = cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    unsigned int expected_flags = MDB_RDONLY | MDB_NOSUBDIR | MDB_NOSYNC | MDB_NOMETASYNC;
    EXPECT_EQ(expected_flags, sm_Cache->GetBlobPropFlags());
}

TEST_F(CPsgCacheBlobPropTest, LookupBlobPropBySatKey)
{
    CPubseqGatewayCache::TBlobPropRequest request;

    request.SetSat(-10).SetSatKey(2054006);
    auto response = sm_Cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(-500);
    response = sm_Cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(4).SetSatKey(9965740);
    response = sm_Cache->FetchBlobProp(request);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(1114019083516, response[0].GetModified());

    request.Reset().SetSat(0).SetSatKey(2054006);
    response = sm_Cache->FetchBlobProp(request);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(823387172086, response[0].GetModified());

    EXPECT_EQ(0, response[0].GetClass());
    EXPECT_EQ(823387172086L, response[0].GetDateAsn1());
    EXPECT_EQ("SPT", response[0].GetDiv());
    EXPECT_EQ(36, response[0].GetFlags());
    EXPECT_EQ(-2208970800000L, response[0].GetHupDate());
    EXPECT_EQ("", response[0].GetId2Info());
    EXPECT_EQ(1, response[0].GetNChunks());
    EXPECT_EQ(6, response[0].GetOwner());
    EXPECT_EQ(1145L, response[0].GetSize());
    EXPECT_EQ(1145L, response[0].GetSizeUnpacked());
    EXPECT_EQ("cavanaug", response[0].GetUserName());
}

TEST_F(CPsgCacheBlobPropTest, LookupBlobPropBySatKeyLastModified)
{
    CPubseqGatewayCache::TBlobPropRequest request;

    request.SetSat(-10).SetSatKey(2054006).SetLastModified(823387172086);
    auto response = sm_Cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(-500).SetLastModified(823387172086);
    response = sm_Cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(2054006).SetLastModified(20);
    response = sm_Cache->FetchBlobProp(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetSat(0).SetSatKey(2054006).SetLastModified(823387172086);
    response = sm_Cache->FetchBlobProp(request);
    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(823387172086, response[0].GetModified());
    EXPECT_EQ(2054006, response[0].GetKey());

    EXPECT_EQ(823387172086L, response[0].GetDateAsn1());
    EXPECT_EQ("SPT", response[0].GetDiv());
    EXPECT_EQ("", response[0].GetId2Info());
    EXPECT_EQ(1, response[0].GetNChunks());
    EXPECT_EQ(1145L, response[0].GetSize());
    EXPECT_EQ("cavanaug", response[0].GetUserName());
}

TEST_F(CPsgCacheBlobPropTest, LookupBlobPropForLastRecord)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    request.SetSat(0);
    auto response = sm_Cache->FetchBlobPropLast(request);
    ASSERT_FALSE(response.empty());
    auto last = response[response.size() - 1];

    request.Reset().SetSat(0).SetSatKey(last.GetKey()).SetLastModified(last.GetModified());
    response = sm_Cache->FetchBlobProp(request);

    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(last.GetModified(), response[0].GetModified());
    EXPECT_EQ(last.GetModified(), response[0].GetModified());
    EXPECT_EQ(last.GetDiv(), response[0].GetDiv());
    EXPECT_EQ(last.GetSize(), response[0].GetSize());
}

TEST_F(CPsgCacheBlobPropTest, EnumerateBlobProp)
{
    int rows{0};
    sm_Cache->EnumerateBlobProp(4,
        [&rows] (int32_t sat_key, int64_t modified)
        {
            if (rows == 0) {
                EXPECT_EQ(4317, sat_key);
                EXPECT_EQ(1009491218503, modified);
            }
            ++rows;
            return true;
        }
    );
    EXPECT_EQ(1020, rows);
}

TEST_F(CPsgCacheBlobPropTest, DisableSatDatabase)
{
    {
        auto cache = make_unique<CPubseqGatewayCache>("", "", sm_CacheFilePath.Get());
        testing::internal::CaptureStderr();
        cache->Open({0, 111});
        auto stderr = testing::internal::GetCapturedStderr();
        EXPECT_EQ(
            "Warning: BlobProp cache: database disabled (#STATUS[111][\"DISABLED\"] == \"yes\" OR #STATUS[111] does not exist)"
            " for #DATA[111], cache for sat = 111 will not be used.\n",
            stderr
        );
    }
    {
        auto cache = make_unique<CPubseqGatewayCache>("", "", sm_CacheFilePath.Get());
        testing::internal::CaptureStderr();
        cache->Open({0, 112});
        auto stderr = testing::internal::GetCapturedStderr();
        EXPECT_EQ(
            "Warning: BlobProp cache: database disabled (#STATUS[112][\"DISABLED\"] == \"yes\" OR #STATUS[112] does not exist)"
            " for #DATA[112], cache for sat = 112 will not be used.\n",
            stderr
        );
        CPubseqGatewayCache::TBlobPropRequest request;
        request.SetSat(112).SetSatKey(2054006);
        auto response = cache->FetchBlobProp(request);
        EXPECT_TRUE(response.empty());
    }
}

END_SCOPE()

