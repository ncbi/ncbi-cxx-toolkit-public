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
* Author:  Aaron Ucko, Pavel Ivanov, NCBI
*
* File Description:
*   Sample unit test with auto-registration of test cases.
*
* NOTE:
*   Boost.Test reports some memory leaks when compiled in MSVC even for this
*   simple code. Maybe it's related to some toolkit static variables.
*   To avoid annoying messages about memory leaks run this program with
*   parameter --detect_memory_leak=0
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(TestSimpleTools)
{
    int i = 1;
    double d = 0.123;
    string s1 = "qwerty";
    string s2 = "qwerty";

    BOOST_CHECK_EQUAL(i, 1);
    BOOST_CHECK_EQUAL(d, 0.123);
    BOOST_CHECK_EQUAL(s1, s2);
    // Never use it this way, because it will not compile on WorkShop
    // BOOST_CHECK_EQUAL(s1, "qwerty");
    // Instead of that use it this way
    BOOST_CHECK(s1 == "qwerty");
    // Or this way
    BOOST_CHECK_EQUAL(s1, string("qwerty"));
}

void s_ThrowSomeException(void)
{
    NCBI_THROW(CException, eUnknown, "Some exception message");
}

BOOST_AUTO_TEST_CASE(TestWithException)
{
    try {
        s_ThrowSomeException();
        BOOST_FAIL("Exception wasn't thrown");
    }
    catch (CException&) {
        // Exception thrown - all is ok
    }

    // Or you can make it this way
    bool exception_thrown = false;
    try {
        s_ThrowSomeException();
    }
    catch (CException&) {
        exception_thrown = true;
    }
    BOOST_CHECK_MESSAGE(exception_thrown, "Exception wasn't thrown");
}

BOOST_AUTO_TEST_CASE(TestWithoutException)
{
    try {
        // Put some code that have not to throw exception
    }
    catch (exception&) {
        BOOST_FAIL("std::exception was thrown");
    }
}
