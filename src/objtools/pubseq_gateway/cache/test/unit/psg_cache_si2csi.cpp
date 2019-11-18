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
#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

BEGIN_SCOPE()

USING_NCBI_SCOPE;
USING_PSG_SCOPE;

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
    string data;
    int sec_seq_id_type;

    unique_ptr<CPubseqGatewayCache> cache = make_unique<CPubseqGatewayCache>("", "", "");
    cache->Open({});
    EXPECT_FALSE(cache->LookupCsiBySeqId("3643631", sec_seq_id_type, data));
}

TEST_F(CPsgCacheSi2CsiTest, LookupCsiBySeqId)
{
    string data;
    int sec_seq_id_type;

    bool result = m_Cache->LookupCsiBySeqId("FAKE", sec_seq_id_type, data);
    EXPECT_FALSE(result);

    result = m_Cache->LookupCsiBySeqId("", sec_seq_id_type, data);
    EXPECT_FALSE(result);

    result = m_Cache->LookupCsiBySeqId("3643631", sec_seq_id_type, data);
    ASSERT_TRUE(result);
    EXPECT_EQ(12, sec_seq_id_type);

    ::psg::retrieval::BioseqInfoKey value;
    EXPECT_TRUE(value.ParseFromString(data));
    EXPECT_EQ("AC005299", value.accession());
    EXPECT_EQ(0, value.version());
    EXPECT_EQ(5, value.seq_id_type());
    EXPECT_EQ(3643631, value.gi());
}

TEST_F(CPsgCacheSi2CsiTest, LookupCsiBySeqIdSeqIdType)
{
    string data;
    bool result = m_Cache->LookupCsiBySeqIdSeqIdType("FAKE", 0, data);
    EXPECT_FALSE(result);

    result = m_Cache->LookupCsiBySeqIdSeqIdType("", 0, data);
    EXPECT_FALSE(result);

    result = m_Cache->LookupCsiBySeqIdSeqIdType("3643631", 0, data);
    EXPECT_FALSE(result);

    result = m_Cache->LookupCsiBySeqIdSeqIdType("3643631", 12, data);
    ASSERT_TRUE(result);

    ::psg::retrieval::BioseqInfoKey value;
    EXPECT_TRUE(value.ParseFromString(data));
    EXPECT_EQ("AC005299", value.accession());
    EXPECT_EQ(0, value.version());
    EXPECT_EQ(5, value.seq_id_type());
    EXPECT_EQ(3643631, value.gi());
}

END_SCOPE()

