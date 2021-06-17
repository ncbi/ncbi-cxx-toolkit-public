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
*   Unit test suite to check pack/unpack cache operations not covered by other tests
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

class CPsgCachePackUnpackTest
    : public testing::Test
{
 public:
    CPsgCachePackUnpackTest() = default;

 protected:
    static void SetUpTestCase()
    {
        m_Cache = make_unique<CPubseqGatewayCache>("", "", "");
        m_Cache->Open({});
    }

    static void TearDownTestCase()
    {
        m_Cache = nullptr;
    }

    static unique_ptr<CPubseqGatewayCache> m_Cache;
};

unique_ptr<CPubseqGatewayCache> CPsgCachePackUnpackTest::m_Cache(nullptr);

string ToPrintableString(const string& rv)
{
    stringstream s;
    s << "Size: " << rv.size() << ", Data: [ ";
    s << hex;
    const char * r = rv.c_str();
    for (size_t i = 0; i < rv.size(); ++i) {
        s << setw(4) << setfill('0') << static_cast<uint16_t>(r[i]) << " ";
    }
    s << dec;
    s << "]";
    return s.str();

};

TEST_F(CPsgCachePackUnpackTest, BlobPropKey)
{
    int32_t sat_key = 2054006;
    int64_t last_modified = 823387172086;
    string result = m_Cache->PackBlobPropKey(sat_key, last_modified);
    EXPECT_EQ(
        "Size: 12, Data: [ 0000 001f 0057 0076 ffff ffff ffff 0040 004a 004c ffd3 000a ]",
        ToPrintableString(result)
    );

    string result2 = m_Cache->PackBlobPropKey(sat_key);
    EXPECT_EQ("Size: 4, Data: [ 0000 001f 0057 0076 ]", ToPrintableString(result2));

    int64_t actual_modified = 0;
    int32_t actual_key = 0;
    EXPECT_TRUE(m_Cache->UnpackBlobPropKey(result.c_str(), result.size(), actual_modified, actual_key));
    EXPECT_EQ(sat_key, actual_key);
    EXPECT_EQ(last_modified, actual_modified);

    actual_modified = 0;
    EXPECT_TRUE(m_Cache->UnpackBlobPropKey(result.c_str(), result.size(), actual_modified));
    EXPECT_EQ(last_modified, actual_modified);

    string broken = result;
    broken.resize(broken.size() - 1);
    EXPECT_FALSE(m_Cache->UnpackBlobPropKey(broken.c_str(), broken.size(), actual_modified, actual_key));
}

TEST_F(CPsgCachePackUnpackTest, Si2Csi)
{
    string sec_seq_id = "3643631";
    int sec_seq_id_type = 12;
    string result = m_Cache->PackSiKey(sec_seq_id, sec_seq_id_type);
    EXPECT_EQ(
        "Size: 10, Data: [ 0033 0036 0034 0033 0036 0033 0031 0000 0000 000c ]",
        ToPrintableString(result)
    );

    int actual_sec_seq_id_type = 0;
    EXPECT_TRUE(m_Cache->UnpackSiKey(result.c_str(), result.size(), actual_sec_seq_id_type));
    EXPECT_EQ(sec_seq_id_type, actual_sec_seq_id_type);
}

TEST_F(CPsgCachePackUnpackTest, BioseqInfo)
{
    string result = m_Cache->PackBioseqInfoKey("AC005299", 0x08070707);
    EXPECT_EQ(
        "Size: 12, Data: [ 0041 0043 0030 0030 0035 0032 0039 0039 0000 fff8 fff8 fff8 ]",
        ToPrintableString(result)
    );

    result = m_Cache->PackBioseqInfoKey("AC005299", 0x0807f707, 0x080805f5);
    EXPECT_EQ(
        "Size: 14, Data: [ 0041 0043 0030 0030 0035 0032 0039 0039 0000 fff8 0008 fff8 0005 fff5 ]",
        ToPrintableString(result)
    );

    result = m_Cache->PackBioseqInfoKey("AC005299", 0x0807f707, 0x080805f5, 1234567789);
    EXPECT_EQ(
        "Size: 22, Data: [ 0041 0043 0030 0030 0035 0032 0039 0039 0000 fff8 0008"
        " fff8 0005 fff5 ffff ffff ffff ffff ffb6 0069 fffd ff92 ]",
        ToPrintableString(result)
    );

    {
        string accession;
        int version, seq_id_type;
        int64_t gi;
        EXPECT_TRUE(m_Cache->UnpackBioseqInfoKey(result.c_str(), result.size(), accession, version, seq_id_type, gi));
        EXPECT_EQ("AC005299", accession);
        EXPECT_EQ(0x0007f707, version);
        EXPECT_EQ(0x000005f5, seq_id_type);
        EXPECT_EQ(1234567789, gi);
    }

    {
        int version, seq_id_type;
        int64_t gi;
        EXPECT_TRUE(m_Cache->UnpackBioseqInfoKey(result.c_str(), result.size(), version, seq_id_type, gi));
        EXPECT_EQ(0x0007f707, version);
        EXPECT_EQ(0x000005f5, seq_id_type);
        EXPECT_EQ(1234567789, gi);
    }
}

END_SCOPE()
