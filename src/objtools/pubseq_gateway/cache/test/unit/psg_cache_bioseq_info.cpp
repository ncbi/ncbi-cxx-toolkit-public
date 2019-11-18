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
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

BEGIN_SCOPE()

USING_NCBI_SCOPE;
USING_PSG_SCOPE;

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
    CPubseqGatewayCache::TBioseqInfoResponse response;
    cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupWithoutResult)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("FAKE");
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset();
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetVersion(0);
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupByAccession)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.Reset().SetAccession("AC005299");
    m_Cache->FetchBioseqInfo(request, response);
    ASSERT_EQ(response.size(), 6UL);
    EXPECT_EQ("AC005299", response[0].accession);
    EXPECT_EQ(1, response[0].version);
    EXPECT_EQ(5, response[0].seq_id_type);
    EXPECT_EQ(3786039, response[0].gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersion)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("AC005299").SetVersion(0);
    m_Cache->FetchBioseqInfo(request, response);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].accession);
    EXPECT_EQ(0, response[0].version);
    EXPECT_EQ(5, response[0].seq_id_type);
    EXPECT_EQ(3746100, response[0].gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdType)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("AC005299").SetVersion(0).SetSeqIdType(3);
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetAccession("AC005299").SetVersion(77).SetSeqIdType(5);
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());

    // version >= 0 && no seq_id_type
    request.Reset().SetAccession("AC005299").SetVersion(0);
    m_Cache->FetchBioseqInfo(request, response);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].accession);
    EXPECT_EQ(0, response[0].version);
    EXPECT_EQ(5, response[0].seq_id_type);
    EXPECT_EQ(3746100, response[0].gi);

    // version >= 0 && seq_id_type > 0
    request.Reset().SetAccession("AC005299").SetVersion(0).SetSeqIdType(5);
    m_Cache->FetchBioseqInfo(request, response);
    ASSERT_EQ(response.size(), 5UL);
    EXPECT_EQ("AC005299", response[0].accession);
    EXPECT_EQ(0, response[0].version);
    EXPECT_EQ(5, response[0].seq_id_type);
    EXPECT_EQ(3746100, response[0].gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionGi)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.Reset().SetAccession("AC005299").SetGI(3643631);
    m_Cache->FetchBioseqInfo(request, response);
    ASSERT_EQ(response.size(), 1UL);
    EXPECT_EQ("AC005299", response[0].accession);
    EXPECT_EQ(0, response[0].version);
    EXPECT_EQ(5, response[0].seq_id_type);
    EXPECT_EQ(3643631, response[0].gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdTypeGi)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("AC005299").SetVersion(0).SetSeqIdType(5).SetGI(888);
    m_Cache->FetchBioseqInfo(request, response);
    EXPECT_TRUE(response.empty());

    request.Reset().SetAccession("AC005299").SetVersion(0).SetSeqIdType(5).SetGI(3643631);
    m_Cache->FetchBioseqInfo(request, response);

    ASSERT_EQ(response.size(), 1UL);
    ::psg::retrieval::BioseqInfoValue value;
    EXPECT_TRUE(value.ParseFromString(response[0].data));
    EXPECT_EQ(907538716500, value.date_changed());
    EXPECT_EQ(1310387125, value.hash());
    EXPECT_EQ(40756, value.length());
    EXPECT_EQ(1, value.mol());
    EXPECT_EQ(0, value.blob_key().sat());
    EXPECT_EQ(5985907, value.blob_key().sat_key());
    EXPECT_EQ(0, value.state());
    EXPECT_EQ(5072UL, value.tax_id());
    EXPECT_EQ("", value.name());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionWithSeqIdsInheritance)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    CPubseqGatewayCache::TBioseqInfoResponse response;

    request.SetAccession("AH015101").SetVersion(1);
    m_Cache->FetchBioseqInfo(request, response);

    ASSERT_EQ(response.size(), 1UL);
    ::psg::retrieval::BioseqInfoValue value;
    EXPECT_TRUE(value.ParseFromString(response[0].data));
    EXPECT_EQ(-1984866248, value.hash());
    EXPECT_EQ(821, value.length());
    EXPECT_EQ(1, value.mol());
    EXPECT_EQ(4, value.blob_key().sat());
    EXPECT_EQ(10756063, value.blob_key().sat_key());
    EXPECT_EQ(0, value.state());
    EXPECT_EQ(9606UL, value.tax_id());
    EXPECT_EQ(2, value.seq_ids_size());

    set<tuple<int16_t, string>> expected_seq_ids, actual_seq_ids;

    // Inherited
    expected_seq_ids.insert(make_tuple<int16_t, string>(5, "SEG_DQ123855S"));

    // Own
    expected_seq_ids.insert(make_tuple<int16_t, string>(12, "71493118"));

    for (auto const & item : value.seq_ids()) {
        actual_seq_ids.insert(make_tuple(static_cast<int16_t>(item.sec_seq_id_type()), item.sec_seq_id()));
    }
    EXPECT_EQ(expected_seq_ids, actual_seq_ids);
}

END_SCOPE()

