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
* Author:  Pavel Ivanov, NCBI
*
* File Description:
*   Sample unit test with auto-registration of test cases.
*
* If you don't want to use auto-registration of test cases or you want to use
* some features that are not available with auto-registration
* (e.g. initialization code before all tests or dependencies between tests)
* look into another sample - unit_test_alt_sample.cpp.
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

// This macro should be defined before inclusion of test_boost.hpp only in one
// "*.cpp" file inside executable. It is like function main() for
// non-Boost.Test executables.
//
// This macro is necessary only for unit tests with auto-registration.
// For the example of unit test without auto-registration look into
// "unit_test_alt_sample.cpp".
#define BOOST_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;



BOOST_AUTO_INITIALIZATION(MyTestInit)
{
    // Your application initialization code here (optional)
}


BOOST_AUTO_TEST_CASE(TestSimpleTools)
{
    int    i  = 1;
    double d  = 0.123;
    string s1 = "qwerty";
    string s2 = "qwerty";

    // If this check fails, test will continue its execution
    BOOST_CHECK_EQUAL(i,  1);
    // If this check fails, test will stop its execution at this point
    BOOST_REQUIRE_EQUAL(d,  0.123);

    BOOST_CHECK_EQUAL(s1, s2);

    // Never use it this way, because it will not compile on WorkShop:
    //    BOOST_CHECK_EQUAL(s1, "qwerty");
    // Instead, use it this way:
    BOOST_CHECK(s1 == "qwerty");
    // ...or this way:
    BOOST_CHECK_EQUAL(s1, string("qwerty"));
}


static void s_ThrowSomeException(void)
{
    NCBI_THROW(CException, eUnknown, "Some exception message");
}

BOOST_AUTO_TEST_CASE(TestWithException)
{
    BOOST_CHECK_THROW( s_ThrowSomeException(), CException );
}


static void s_FuncWithoutException(void)
{
    printf("Here is some dummy message\n");
}

BOOST_AUTO_TEST_CASE(TestWithoutException)
{
    BOOST_CHECK_NO_THROW( s_FuncWithoutException() );
}
