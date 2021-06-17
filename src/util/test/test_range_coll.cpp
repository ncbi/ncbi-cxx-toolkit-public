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
* Author:  Pavel Ivanov, Aleksey Grichenko, NCBI
*
* File Description:
*   Unit test for CFormatGuess class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <util/range_coll.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(TestRangeAddition)
{
    CRangeCollection<TSeqPos> coll;

    coll += TSeqRange(10, 20);
    BOOST_CHECK_EQUAL(coll.size(), 1U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(10, 20));
     }}

    coll += TSeqRange(30, 40);
    BOOST_CHECK_EQUAL(coll.size(), 2U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(10, 20));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(30, 40));
     }}

    coll += TSeqRange(50, 60);
    BOOST_CHECK_EQUAL(coll.size(), 3U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(10, 20));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(30, 40));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(50, 60));
     }}

    coll += TSeqRange(55, 70);
    BOOST_CHECK_EQUAL(coll.size(), 3U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(10, 20));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(30, 40));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(50, 70));
     }}

    coll += TSeqRange(35, 50);
    BOOST_CHECK_EQUAL(coll.size(), 2U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(10, 20));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(30, 70));
     }}
}



BOOST_AUTO_TEST_CASE(TestRangeSubtraction)
{
    CRangeCollection<TSeqPos> coll;

    coll += TSeqRange(0, 100);
    BOOST_CHECK_EQUAL(coll.size(), 1U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(0, 100));
     }}

    coll -= TSeqRange(50, 60);
    BOOST_CHECK_EQUAL(coll.size(), 2U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(0, 49));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(61, 100));
     }}

    coll -= TSeqRange(40, 70);
    BOOST_CHECK_EQUAL(coll.size(), 2U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(0, 39));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(71, 100));
     }}

    coll -= TSeqRange(10, 20);
    BOOST_CHECK_EQUAL(coll.size(), 3U);
    {{
         CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(0, 9));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(21, 39));

         ++it;
         BOOST_CHECK(it != coll.end());
         BOOST_CHECK(*it == TSeqRange(71, 100));
     }}
}



BOOST_AUTO_TEST_CASE(TestRangeIntersection)
{
    CRangeCollection<TSeqPos> coll;

    for (TSeqPos p = 10;  p < 100;  p += 20) {
        coll += TSeqRange(p, p + 10);
    }
    BOOST_CHECK_EQUAL(coll.size(), 5U);

    coll &= TSeqRange(35, 75);
    BOOST_CHECK_EQUAL(coll.size(), 3U);

    CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(35, 40));

    ++it;
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(50, 60));

    ++it;
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(70, 75));

    BOOST_CHECK(coll.IntersectingWith(TSeqRange(42, 48)) == false);
    BOOST_CHECK(coll.IntersectingWith(TSeqRange(52, 58)) == true);
    BOOST_CHECK(coll.IntersectingWith(TSeqRange(58, 65)) == true);
    BOOST_CHECK(coll.IntersectingWith(TSeqRange(75, 80)) == true);
    BOOST_CHECK(coll.IntersectingWith(TSeqRange(85, 90)) == false);

    CRangeCollection<TSeqPos> coll2 = coll;

    coll &= TSeqRange(80,90);
    BOOST_CHECK_EQUAL(coll.size(), 0U);

    it = coll.begin();
    BOOST_CHECK(it == coll.end());

    BOOST_CHECK_EQUAL(coll2.size(), 3U);

    coll2 &= TSeqRange(10,20);
    BOOST_CHECK_EQUAL(coll2.size(), 0U);

    it = coll2.begin();
    BOOST_CHECK(it == coll2.end());
}

BOOST_AUTO_TEST_CASE(TestRangeCollIntersection)
{
    CRangeCollection<TSeqPos> coll, coll2;

    for (TSeqPos p = 10;  p < 100;  p += 20) {
        coll += TSeqRange(p, p + 10);
    }

    for (TSeqPos p = 45;  p < 150;  p += 20) {
        coll2 += TSeqRange(p, p + 10);
    }
    BOOST_CHECK_EQUAL(coll.size(), 5U);

    coll &= coll2;
    BOOST_CHECK_EQUAL(coll.size(), 3U);

    CRangeCollection<TSeqPos>::const_iterator it = coll.begin();
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(50, 55));

    ++it;
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(70, 75));

    ++it;
    BOOST_CHECK(it != coll.end());
    BOOST_CHECK(*it == TSeqRange(90, 95));
}
