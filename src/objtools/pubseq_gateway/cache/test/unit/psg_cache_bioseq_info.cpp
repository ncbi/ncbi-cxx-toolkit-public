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

namespace {

USING_NCBI_SCOPE;

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
    string data;
    int version, seq_id_type;
    int64_t gi;

    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({});
    EXPECT_FALSE(cache->LookupBioseqInfoByAccession("AC005299", data, version, seq_id_type, gi));
}

TEST_F(CPsgCacheBioseqInfoTest, LookupByAccession)
{
    string data;
    int version, seq_id_type;
    int64_t gi;

    bool result = m_Cache->LookupBioseqInfoByAccession("FAKE", data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccession("", data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccession("AC005299", data, version, seq_id_type, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(1, version);
    EXPECT_EQ(5, seq_id_type);
    EXPECT_EQ(3786039, gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersion)
{
    string data;
    int seq_id_type;
    int64_t gi;

    bool result = m_Cache->LookupBioseqInfoByAccessionVersion("FAKE", 1, data, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionVersion("", 1, data, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionVersion("AC005299", -1, data, seq_id_type, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(5, seq_id_type);
    EXPECT_EQ(3786039, gi);

    result = m_Cache->LookupBioseqInfoByAccessionVersion("AC005299", 0, data, seq_id_type, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(5, seq_id_type);
    EXPECT_EQ(3746100, gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdType)
{
    string data;
    int seq_id_type, version;
    int64_t gi;

    bool result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("FAKE", 1, 5, data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("", 1, 5, data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("AC005299", 0, 3, data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("AC005299", 77, 5, data, version, seq_id_type, gi);
    EXPECT_FALSE(result);

    // version >= 0 && seq_id_type <= 0
    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("AC005299", 0, -1, data, version, seq_id_type, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(0, version);
    EXPECT_EQ(5, seq_id_type);
    EXPECT_EQ(3746100, gi);

    // version < 0
    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("AC005299", -1, 5, data, version, seq_id_type, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(1, version);
    EXPECT_EQ(5, seq_id_type);
    EXPECT_EQ(3786039, gi);

    // version >= 0 && seq_id_type > 0
    result = m_Cache->LookupBioseqInfoByAccessionVersionSeqIdType("AC005299", 0, 5, data, gi);
    ASSERT_TRUE(result);
    EXPECT_EQ(3746100, gi);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionGi)
{
    string data;
    int seq_id_type, version;

    bool result = m_Cache->LookupBioseqInfoByAccessionGi("FAKE", 1, data, version, seq_id_type);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionGi("", 1, data, version, seq_id_type);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionGi("AC005299", -1, data, version, seq_id_type);
    EXPECT_FALSE(result);

    result = m_Cache->LookupBioseqInfoByAccessionGi("AC005299", 3643631, data, version, seq_id_type);
    ASSERT_TRUE(result);
    EXPECT_EQ(0, version);
    EXPECT_EQ(5, seq_id_type);
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionSeqIdTypeGi)
{
    string data;
    EXPECT_FALSE(m_Cache->LookupBioseqInfoByAccessionVersionSeqIdTypeGi("FAKE", 0, 5, 3643631,  data));
    EXPECT_FALSE(m_Cache->LookupBioseqInfoByAccessionVersionSeqIdTypeGi("", 0, 5, 3643631, data));
    EXPECT_FALSE(m_Cache->LookupBioseqInfoByAccessionVersionSeqIdTypeGi("AC005299", -1, 5, 3643631, data));
    EXPECT_FALSE(m_Cache->LookupBioseqInfoByAccessionVersionSeqIdTypeGi("AC005299", 0, 5, 888, data));
    ASSERT_TRUE(m_Cache->LookupBioseqInfoByAccessionVersionSeqIdTypeGi("AC005299", 0, 5, 3643631, data));

    ::psg::retrieval::BioseqInfoValue value;
    EXPECT_TRUE(value.ParseFromString(data));
    EXPECT_EQ(907538716500, value.date_changed());
    EXPECT_EQ(1310387125, value.hash());
    EXPECT_EQ(40756, value.length());
    EXPECT_EQ(1, value.mol());
    EXPECT_EQ(0, value.blob_key().sat());
    EXPECT_EQ(5985907, value.blob_key().sat_key());
    EXPECT_EQ(0, value.state());
    EXPECT_EQ(5072UL, value.tax_id());
}

TEST_F(CPsgCacheBioseqInfoTest, LookupBioseqInfoByAccessionVersionWithSeqIdsInheritance)
{
    string data;
    int found_seq_id_type{0};
    int64_t found_gi{0};
    ASSERT_TRUE(m_Cache->LookupBioseqInfoByAccessionVersion("AH015101", 1, data, found_seq_id_type, found_gi));

    ::psg::retrieval::BioseqInfoValue value;
    EXPECT_TRUE(value.ParseFromString(data));
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

}  // namespace

