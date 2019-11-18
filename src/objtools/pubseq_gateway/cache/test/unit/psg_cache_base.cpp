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
*   Unit test suite to check basic functionality for CPubseqGatewayCache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include <gtest/gtest.h>

BEGIN_IDBLOB_SCOPE

bool operator==(const CPubseqGatewayCache::TRuntimeError a, const CPubseqGatewayCache::TRuntimeError b)
{
    return a.message == b.message;
}

std::ostream& operator<<(std::ostream& os, const CPubseqGatewayCache::TRuntimeError& error) {
  return os << "'" << error.message << "'";  // whatever needed to print bar to os
}

END_IDBLOB_SCOPE

BEGIN_SCOPE()
USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

TEST(PubseqGatewayCacheRuntimeErrors, EmptyOnStart) {
    CPubseqGatewayCache cache("", "", "");
    EXPECT_TRUE(cache.GetErrors().empty());
}

TEST(PubseqGatewayCacheRuntimeErrors, WrongFileNames) {
    CPubseqGatewayCache::TRuntimeErrorList expected_error_list{
        CPubseqGatewayCache::TRuntimeError("Failed to open 'wrong si2sci' cache: No such file or directory: "
            "No such file or directory, si2csi cache will not be used."),
        CPubseqGatewayCache::TRuntimeError("Failed to open '/really_wrong_blob_prop' cache: "
            "List of satellites is empty: Successful return: 0, blob prop cache will not be used."),
    };
    CPubseqGatewayCache cache("", "wrong si2sci", "/really_wrong_blob_prop");
    cache.Open({});
    EXPECT_EQ(2UL, cache.GetErrors().size());
    EXPECT_EQ(expected_error_list, cache.GetErrors());
    cache.ResetErrors();
    EXPECT_TRUE(cache.GetErrors().empty());
}

TEST(PubseqGatewayCacheRuntimeErrors, ErrorLimit) {
    CPubseqGatewayCache cache("1", "2", "3");
    for (size_t i = 0; i < CPubseqGatewayCache::kRuntimeErrorLimit; ++i) {
        cache.Open({});
    }
    EXPECT_EQ(CPubseqGatewayCache::kRuntimeErrorLimit, cache.GetErrors().size());
    cache.ResetErrors();
    EXPECT_TRUE(cache.GetErrors().empty());
}

END_SCOPE()
