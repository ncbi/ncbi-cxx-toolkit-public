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
 * Authors:  Aaron Ucko
 *
 * File Description:
 *   Test for threadsafe variants of BOOST_* and NCBITEST_* macros.
 *
 *   Does not (yet) try to confirm thread-safety, just identical
 *   behavior in sequential mode (apart from invisible locking).
 *   Warning-level tests are all *expected* to fail, to allow for
 *   (manual) comparision of both pass and fail messages without
 *   having any serious failures.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <common/test_assert.h>  /* This header must go last */

#ifndef BOOST_TEST
#  define BOOST_TEST_WARN(x)  BOOST_WARN(x)
#  define BOOST_TEST_CHECK(x) BOOST_CHECK(x)
#endif

USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(Test_NcbiTest)
{
    BOOST_TEST_WARN(8 + 9 != 9 + 8);
    NCBITEST_WARN(8 + 9 != 9 + 8);
    NCBITEST_WARN_MT_SAFE(8 + 9 != 9 + 8);

    BOOST_TEST_CHECK(8 + 9 == 9 + 8);
    NCBITEST_CHECK(8 + 9 == 9 + 8);
    NCBITEST_CHECK_MT_SAFE(8 + 9 == 9 + 8);

    BOOST_WARN_EQUAL(8 + 9, 9 * 8);
    NCBITEST_WARN_EQUAL(8 + 9, 9 * 8);
    NCBITEST_WARN_EQUAL_MT_SAFE(8 + 9, 9 * 8);

    BOOST_REQUIRE_EQUAL(8 + 9, 9 + 8);
    NCBITEST_REQUIRE_EQUAL(8 + 9, 9 + 8);
    NCBITEST_REQUIRE_EQUAL_MT_SAFE(8 + 9, 9 + 8);

    BOOST_WARN_NE(8 + 9, 9 + 8);
    NCBITEST_WARN_NE(8 + 9, 9 + 8);
    NCBITEST_WARN_NE_MT_SAFE(8 + 9, 9 + 8);

    BOOST_CHECK_NE(8 + 9, 9 * 8);
    NCBITEST_CHECK_NE(8 + 9, 9 * 8);
    NCBITEST_CHECK_NE_MT_SAFE(8 + 9, 9 * 8);
}

BOOST_AUTO_TEST_CASE(Test_Throw_NoThrow)
{
    BOOST_WARN_NO_THROW(throw runtime_error("Aieee!"));
    BOOST_WARN_NO_THROW_MT_SAFE(throw runtime_error("Aieee!"));

    BOOST_CHECK_NO_THROW(;);
    BOOST_CHECK_NO_THROW_MT_SAFE(;);

    BOOST_WARN_THROW(;, runtime_error);
    BOOST_WARN_THROW_MT_SAFE(;, runtime_error);

    BOOST_REQUIRE_THROW(throw runtime_error("Aieee!"), runtime_error);
    BOOST_REQUIRE_THROW_MT_SAFE(throw runtime_error("Aieee!"), runtime_error);

    auto pred = [](const exception& e) { return !strcmp(e.what(), "foo"); };
    
    BOOST_WARN_EXCEPTION(throw runtime_error("bar"), runtime_error, pred);
    BOOST_WARN_EXCEPTION_MT_SAFE(throw runtime_error("bar"), runtime_error,
                                 pred);

    BOOST_CHECK_EXCEPTION(throw runtime_error("foo"), runtime_error, pred);
    BOOST_CHECK_EXCEPTION_MT_SAFE(throw runtime_error("foo"), runtime_error,
                                  pred);
}

BOOST_AUTO_TEST_CASE(Test_Plain)
{
    BOOST_WARN(8 + 9 != 9 + 8);
    BOOST_WARN_MT_SAFE(8 + 9 != 9 + 8);

    BOOST_CHECK(8 + 9 == 9 + 8);
    BOOST_CHECK_MT_SAFE(8 + 9 == 9 + 8);

    BOOST_CHECK(8 + 9 == 9 + 8);
    BOOST_REQUIRE_MT_SAFE(8 + 9 == 9 + 8);
}

BOOST_AUTO_TEST_CASE(Test_Messages)
{
    BOOST_WARN_MESSAGE(false, "foo");
    BOOST_WARN_MESSAGE_MT_SAFE(false, "foo");

    BOOST_CHECK_MESSAGE(true, "foo");
    BOOST_CHECK_MESSAGE_MT_SAFE(true, "foo");

    BOOST_REQUIRE_MESSAGE(true, "foo");
    BOOST_REQUIRE_MESSAGE_MT_SAFE(true, "foo");

    BOOST_TEST_MESSAGE("foo");
    BOOST_TEST_MESSAGE_MT_SAFE("foo");
}

BOOST_AUTO_TEST_CASE(Test_Equal_NE)
{
    BOOST_WARN_EQUAL(8 + 9, 8 * 9);
    BOOST_WARN_EQUAL_MT_SAFE(8 + 9, 8 * 9);

    BOOST_CHECK_EQUAL(8 + 9, 8 + 9);
    BOOST_CHECK_EQUAL_MT_SAFE(8 + 9, 8 + 9);

    BOOST_WARN_NE(8 * 9, 8 * 9);
    BOOST_WARN_NE_MT_SAFE(8 * 9, 8 * 9);

    BOOST_REQUIRE_NE(8 + 9, 8 * 9);
    BOOST_REQUIRE_NE_MT_SAFE(8 + 9, 8 * 9);
}

BOOST_AUTO_TEST_CASE(Test_Close)
{
    BOOST_WARN_CLOSE(3.0, 4.0, 10);
    BOOST_WARN_CLOSE_MT_SAFE(3.0, 4.0, 10);

    BOOST_CHECK_CLOSE(3.0, 3.2, 10);
    BOOST_CHECK_CLOSE_MT_SAFE(3.0, 3.2, 10);

    BOOST_REQUIRE_CLOSE(3.0, 3.2, 10);
    BOOST_REQUIRE_CLOSE_MT_SAFE(3.0, 3.2, 10);
}

BOOST_AUTO_TEST_CASE(Test_Collections)
{
    vector<int> x{2, 3, 6};
    vector<int> y{2, 4, 6};

    BOOST_WARN_EQUAL_COLLECTIONS(x.begin(), x.end(), y.begin(), y.end());
    BOOST_WARN_EQUAL_COLLECTIONS_MT_SAFE(x.begin(), x.end(),
                                         y.begin(), y.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(x.begin(), x.end(), x.begin(), x.end());
    BOOST_CHECK_EQUAL_COLLECTIONS_MT_SAFE(x.begin(), x.end(),
                                          x.begin(), x.end());

    BOOST_REQUIRE_EQUAL_COLLECTIONS(y.begin(), y.end(), y.begin(), y.end());
    BOOST_REQUIRE_EQUAL_COLLECTIONS_MT_SAFE(y.begin(), y.end(),
                                            y.begin(), y.end());
}
