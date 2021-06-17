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
*   Unit test suite to check bioseq_info cache operations
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include <unistd.h>
#include <libgen.h>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbistre.hpp>

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

BEGIN_SCOPE()

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CPsgCacheBioseqInfoTest
    : public testing::Test
{
 public:
    CPsgCacheBioseqInfoTest() = default;

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
        string file_name = r.GetString("LMDB_CACHE", "bioseq_info", "");
        m_Cache = make_unique<CPubseqGatewayCache>(file_name, "", "");
        m_Cache->Open({});
    }

    static void TearDownTestCase()
    {
        m_Cache = nullptr;
    }

    static unique_ptr<CPubseqGatewayCache> m_Cache;
};

unique_ptr<CPubseqGatewayCache> CPsgCacheBioseqInfoTest::m_Cache(nullptr);

TEST_F(CPsgCacheBioseqInfoTest, LookupUninitialized)
{
    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({});
    CPubseqGatewayCache::TBioseqInfoRequest request;
    auto response = cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupWithoutResult)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.SetAccession("FAKE");
    auto response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());

    request.Reset();
    response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetVersion(0);
    response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupByAccession)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.Reset().SetAccession("AC005299");
    auto response = m_Cache->FetchBioseqInfo(request);
    ASSERT_EQ(response.size(), 6UL);
    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(1, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3786039, response[0].GetGI());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersion)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.SetAccession("AC005299").SetVersion(0);
    auto response = m_Cache->FetchBioseqInfo(request);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3746100, response[0].GetGI());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdType)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.SetAccession("AC005299").SetVersion(0).SetSeqIdType(3);
    auto response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetAccession("AC005299").SetVersion(77).SetSeqIdType(5);
    response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());

    // version >= 0 && no seq_id_type
    request.Reset().SetAccession("AC005299").SetVersion(0);
    response = m_Cache->FetchBioseqInfo(request);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3746100, response[0].GetGI());

    // version >= 0 && seq_id_type > 0
    request.Reset().SetAccession("AC005299").SetVersion(0).SetSeqIdType(5);
    response = m_Cache->FetchBioseqInfo(request);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3746100, response[0].GetGI());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionGi)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.Reset().SetAccession("AC005299").SetGI(3643631);
    auto response = m_Cache->FetchBioseqInfo(request);
    ASSERT_EQ(response.size(), 1UL);
    EXPECT_EQ("AC005299", response[0].GetAccession());
    EXPECT_EQ(0, response[0].GetVersion());
    EXPECT_EQ(5, response[0].GetSeqIdType());
    EXPECT_EQ(3643631, response[0].GetGI());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdTypeGi)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("AC005299").SetVersion(0).SetSeqIdType(5).SetGI(888);
    response = m_Cache->FetchBioseqInfo(request);
    EXPECT_TRUE(response.empty());

    request.Reset().SetAccession("AC005299").SetVersion(0).SetSeqIdType(5).SetGI(3643631);
    response = m_Cache->FetchBioseqInfo(request);

    ASSERT_EQ(response.size(), 1UL);
    EXPECT_EQ(907538716500, response[0].GetDateChanged());
    EXPECT_EQ(1310387125, response[0].GetHash());
    EXPECT_EQ(40756, response[0].GetLength());
    EXPECT_EQ(1, response[0].GetMol());
    EXPECT_EQ(0, response[0].GetSat());
    EXPECT_EQ(5985907, response[0].GetSatKey());
    EXPECT_EQ(0, response[0].GetState());
    EXPECT_EQ(5072, response[0].GetTaxId());
    EXPECT_EQ("", response[0].GetName());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoWithSeqIdsInheritance)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;

    request.SetAccession("NC_000001").SetVersion(5);
    auto response = m_Cache->FetchBioseqInfo(request);

    ASSERT_EQ(response.size(), 1UL);
    EXPECT_EQ(-785904429, response[0].GetHash());
    EXPECT_EQ(246127941, response[0].GetLength());
    EXPECT_EQ(1, response[0].GetMol());
    EXPECT_EQ(24, response[0].GetSat());
    EXPECT_EQ(5622604, response[0].GetSatKey());
    EXPECT_EQ(0, response[0].GetState());
    EXPECT_EQ(10, response[0].GetSeqState());
    EXPECT_EQ(9606, response[0].GetTaxId());
    EXPECT_EQ(4UL, response[0].GetSeqIds().size());

    set<tuple<int16_t, string>> expected_seq_ids;

    // Inherited
    expected_seq_ids.insert(make_tuple<int16_t, string>(11, "ASM:GCF_000001305|1"));
    expected_seq_ids.insert(make_tuple<int16_t, string>(11, "NCBI_GENOMES|1"));
    expected_seq_ids.insert(make_tuple<int16_t, string>(19, "GPC_000001293.1"));

    // Own
    expected_seq_ids.insert(make_tuple<int16_t, string>(12, "37623929"));

    EXPECT_EQ(expected_seq_ids, response[0].GetSeqIds());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoForLastRecord)
{
    auto response = m_Cache->FetchBioseqInfoLast();
    ASSERT_FALSE(response.empty());
    auto last = response[response.size() - 1];

    CPubseqGatewayCache::TBioseqInfoRequest request;
    request
        .SetAccession(last.GetAccession())
        .SetVersion(last.GetVersion())
        .SetSeqIdType(last.GetSeqIdType())
        .SetGI(last.GetGI());
    response = m_Cache->FetchBioseqInfo(request);

    ASSERT_EQ(1UL, response.size());
    EXPECT_EQ(last.GetAccession(), response[0].GetAccession());
    EXPECT_EQ(last.GetVersion(), response[0].GetVersion());
    EXPECT_EQ(last.GetSeqIdType(), response[0].GetSeqIdType());
    EXPECT_EQ(last.GetGI(), response[0].GetGI());
}

END_SCOPE()

