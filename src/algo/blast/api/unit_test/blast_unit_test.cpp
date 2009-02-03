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
 * Authors: Christiam Camacho
 *
 */

/** @file blast_unit_test.cpp
 * Miscellaneous BLAST unit tests
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <algo/blast/core/blast_def.h>
#include <vector>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN    // this should only be defined here!
#include <boost/test/auto_unit_test.hpp>

#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
//USING_SCOPE(blast);
//USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(SSeqRangeIntersect)
{
    SSeqRange a = SSeqRangeNew(0, 0);
    SSeqRange b = SSeqRangeNew(30, 67);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b) == FALSE);

    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, NULL) == FALSE);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(NULL, &b) == FALSE);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(NULL, NULL) == FALSE);

    a = SSeqRangeNew(0, 0);
    b = SSeqRangeNew(0, 67);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(0, 0);
    b = SSeqRangeNew(0, 0);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(10, 40);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(20, 30);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(30, 67);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(4, 32);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(0, 10);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(40, 100);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b));

    a = SSeqRangeNew(10, 40);
    b = SSeqRangeNew(80, 142);
    BOOST_REQUIRE(SSeqRangeIntersectsWith(&a, &b) == FALSE);
}

BOOST_AUTO_TEST_CASE(SSeqRange_TestLowerBound)
{
    vector<SSeqRange> ranges;
    ranges.push_back(SSeqRangeNew(30, 46));
    ranges.push_back(SSeqRangeNew(50, 77));
    ranges.push_back(SSeqRangeNew(80, 100));
    ranges.push_back(SSeqRangeNew(102, 300));

    Int4 idx;
    idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(), 0);
    BOOST_REQUIRE(idx == 0);

    idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(), 41);
    BOOST_REQUIRE(idx == 0);

    idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(), 50);
    BOOST_REQUIRE(idx == 1);

    idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(), 78);
    BOOST_REQUIRE(idx == 2);

    idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(), 100);
    BOOST_REQUIRE(idx == 2);
}

BOOST_AUTO_TEST_CASE(SSeqRange_RangeSelection)
{
    vector<SSeqRange> ranges;
    ranges.push_back(SSeqRangeNew(30, 46));
    ranges.push_back(SSeqRangeNew(50, 77));
    ranges.push_back(SSeqRangeNew(80, 100));
    ranges.push_back(SSeqRangeNew(105, 110));
    ranges.push_back(SSeqRangeNew(120, 134));
    ranges.push_back(SSeqRangeNew(140, 148));

    SSeqRange target = SSeqRangeNew(20, 100);

    Int4 starting_idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(),
                                            target.left);
    BOOST_REQUIRE(starting_idx == 0);

    Int4 ending_idx = SSeqRangeArrayLessThanOrEqual(&ranges[0], ranges.size(),
                                          target.right);
    BOOST_REQUIRE(ending_idx == 2);

    for (Int4 i = starting_idx; i <= ending_idx; i++) {
        BOOST_REQUIRE(SSeqRangeIntersectsWith(&ranges[i], &target));
    }
}

#endif /* SKIP_DOXYGEN_PROCESSING */
