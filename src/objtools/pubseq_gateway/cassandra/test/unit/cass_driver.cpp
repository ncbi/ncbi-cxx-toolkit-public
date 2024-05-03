/*
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
*   Unit test suite to check common Cassandra driver source
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

#include <gtest/gtest.h>

namespace {
    USING_NCBI_SCOPE;
    USING_IDBLOB_SCOPE;

    TEST(CCassConsistencyTest, FromString)
    {
        EXPECT_EQ(CCassConsistency::FromString("ALL"), CCassConsistency::kAll);
        EXPECT_EQ(CCassConsistency::FromString("ONE"), CCassConsistency::kOne);
        EXPECT_EQ(CCassConsistency::FromString("TWO"), CCassConsistency::kTwo);
        EXPECT_EQ(CCassConsistency::FromString("THREE"), CCassConsistency::kThree);
        EXPECT_EQ(CCassConsistency::FromString("QUORUM"), CCassConsistency::kQuorum);
        EXPECT_EQ(CCassConsistency::FromString("EACH_QUORUM"), CCassConsistency::kEachQuorum);
        EXPECT_EQ(CCassConsistency::FromString("LOCAL_QUORUM"), CCassConsistency::kLocalQuorum);
        EXPECT_EQ(CCassConsistency::FromString("SERIAL"), CCassConsistency::kSerial);
        EXPECT_EQ(CCassConsistency::FromString("LOCAL_SERIAL"), CCassConsistency::kLocalSerial);
        EXPECT_EQ(CCassConsistency::FromString("LOCAL_ONE"), CCassConsistency::kLocalOne);

        EXPECT_EQ(CCassConsistency::FromString(""), CCassConsistency::kUnknown);
        EXPECT_EQ(CCassConsistency::FromString("FFFFFFFFFFFFF"), CCassConsistency::kUnknown);
        EXPECT_EQ(CCassConsistency::FromString("LocalQuorum"), CCassConsistency::kUnknown);
    }
}  // namespace
