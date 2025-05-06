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
*   Unit test suite to check CBlobRecord class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <corelib/ncbitime.hpp>

#include <gtest/gtest.h>

namespace {
    USING_NCBI_SCOPE;
    USING_IDBLOB_SCOPE;

    int16_t GetSybaseWithdrawn(CBlobRecord const& a)
    {
        static constexpr uint64_t withdrawn_offset = 7;
        static constexpr uint64_t withdrawn_mask = 0b1111;
        return static_cast<int16_t>((a.GetFlags() >> withdrawn_offset) & withdrawn_mask);
    }

    int16_t GetSybaseSuppress(CBlobRecord const& a)
    {
        static constexpr uint64_t suppress_offset = 11;
        static constexpr uint64_t suppress_mask = 0b111'1110;
        auto flags = static_cast<uint64_t>(a.GetFlags());
        return
            (flags & static_cast<TBlobFlagBase>(EBlobFlags::eSuppress) ? 1 : 0)
            | static_cast<int16_t>(flags >> (suppress_offset - 1) & suppress_mask);
    }

    TEST(BlobRecordTest, DataComparison) {
        {
            CBlobRecord a, b;
            EXPECT_TRUE(a.IsDataEqual(b));
        }
        {
            CBlobRecord a, b;
            a.AppendBlobChunk({'a'});
            EXPECT_FALSE(a.IsDataEqual(b));
            EXPECT_FALSE(b.IsDataEqual(a));
        }

        {
            CBlobRecord a, b;
            a.AppendBlobChunk({'a'});
            a.AppendBlobChunk({'b'});
            a.AppendBlobChunk({'c', 'd'});
            b.AppendBlobChunk({'a', 'b', 'c', 'd'});
            EXPECT_TRUE(a.IsDataEqual(b));
            EXPECT_TRUE(b.IsDataEqual(a));
        }

        {
            CBlobRecord a, b;
            a.AppendBlobChunk({'a', 'b', 'c', 'd'});
            b.AppendBlobChunk({'a', 'b', 'c', 'd'});
            a.AppendBlobChunk({'e', 'f', 'g', 'h'});
            b.AppendBlobChunk({'e', 'f', 'g', 'z'});
            EXPECT_FALSE(a.IsDataEqual(b));
            EXPECT_FALSE(b.IsDataEqual(a));
        }
        {
            CBlobRecord a;
            CBlobRecord::TTimestamp tm(CCurrentTime().GetTimeT()+1000);
            a.SetHupDate(tm);
            EXPECT_TRUE(a.IsConfidential());
            tm = tm - 1000;
            a.SetHupDate(tm);
            EXPECT_FALSE(a.IsConfidential());
        }
    }

    TEST(BlobRecordTest, BlobProperties) {
        CBlobRecord a;
        CBlobRecord::TTimestamp tm(CCurrentTime().GetTimeT() + 1000);
        a.SetHupDate(tm);
        EXPECT_TRUE(a.IsConfidential());
        tm = tm - 1000;
        a.SetHupDate(tm);
        EXPECT_FALSE(a.IsConfidential());
    }

    TEST(BlobRecordTest, SuppressFlags) {
        {
            CBlobRecord a;
            a.SetFlag(true, EBlobFlags::eSuppress);
            EXPECT_EQ(1, GetSybaseSuppress(a));
            a.SetFlag(true, EBlobFlags::eSuppressTemporary);
            EXPECT_EQ(5, GetSybaseSuppress(a));
            a.SetFlag(true, EBlobFlags::eSuppressEditBlocked);
            EXPECT_EQ(7, GetSybaseSuppress(a));
        }
        {
            CBlobRecord a;
            a.SetFlag(true, EBlobFlags::eNoIncrementalProcessing);
            EXPECT_EQ(16, GetSybaseSuppress(a));
        }
        {
            CBlobRecord a;
            a.SetFlags(4150);
            EXPECT_EQ(5, GetSybaseSuppress(a));
            EXPECT_EQ(0, GetSybaseWithdrawn(a));
        }
    }

    TEST(BlobRecordTest, WithdrawFlags) {
        {
            CBlobRecord a;
            a.SetFlag(true, EBlobFlags::eWithdrawnBase);
            EXPECT_EQ(1, GetSybaseWithdrawn(a));
        }
        {
            CBlobRecord a;
            a.SetFlag(true, EBlobFlags::eWithdrawnPermanently);
            EXPECT_EQ(2, GetSybaseWithdrawn(a));
        }
    }

    TEST(BlobRecordTest, FlagsExamplesFromProduction) {
        CBlobRecord a;
        a.SetFlags(4150);
        EXPECT_EQ(5, GetSybaseSuppress(a));
        EXPECT_EQ(0, GetSybaseWithdrawn(a));

        a.SetFlags(2082);
        EXPECT_EQ(2, GetSybaseSuppress(a));
        EXPECT_EQ(0, GetSybaseWithdrawn(a));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eDead));

        a.SetFlags(184);
        EXPECT_EQ(1, GetSybaseSuppress(a));
        EXPECT_EQ(1, GetSybaseWithdrawn(a));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eDead));

        a.SetFlags(16390);
        EXPECT_EQ(16, GetSybaseSuppress(a));
        EXPECT_EQ(0, GetSybaseWithdrawn(a));
        EXPECT_FALSE(a.GetFlag(EBlobFlags::eDead));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eNot4Gbu));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eGzip));

        a.SetFlags(20530);
        EXPECT_EQ(21, GetSybaseSuppress(a));
        EXPECT_EQ(0, GetSybaseWithdrawn(a));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eDead));
        EXPECT_FALSE(a.GetFlag(EBlobFlags::eNot4Gbu));
        EXPECT_TRUE(a.GetFlag(EBlobFlags::eGzip));

    }
}  // namespace
