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
}  // namespace
