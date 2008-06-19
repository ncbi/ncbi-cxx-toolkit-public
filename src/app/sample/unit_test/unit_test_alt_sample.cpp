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
*   Sample unit test without auto-registration of test cases.
*
* If you don't like concept of init_unit_test_suite() function and want
* to use auto-registration of test cases look into another sample
* unit_test_sample.cpp.
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

// This macro should be defined in all unit tests that do not use
// auto-registration facility of Boost.Test
#define NCBI_BOOST_NO_AUTO_TEST_MAIN

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

// For use in init_unit_test_suite()
#include <boost/shared_ptr.hpp>

USING_NCBI_SCOPE;


void TestSimpleTools(void)
{
    int i = 1;
    double d = 0.123;
    string s1 = "qwerty";
    string s2 = "qwerty";

    // If this check fails, test will continue its execution
    BOOST_CHECK_EQUAL(i,  1);
    // If this check fails, test will stop its execution at this point
    BOOST_REQUIRE_EQUAL(d, 0.123);

    BOOST_CHECK_EQUAL(s1, s2);

    // Never use it this way, because it will not compile on WorkShop
    // BOOST_CHECK_EQUAL(s1, "qwerty");
    // Instead of that use it this way
    BOOST_CHECK(s1 == "qwerty");
    // Or this way
    BOOST_CHECK_EQUAL(s1, string("qwerty"));
}


class CExceptionTests
{
public:
    void TestWithException(void);
    void TestWithoutException(void);

private:
    void ThrowSomeException(void);
    void FuncWithoutException(void);
};

void CExceptionTests::ThrowSomeException(void)
{
    NCBI_THROW(CException, eUnknown, "Some exception message");
}

void CExceptionTests::FuncWithoutException(void)
{
    printf("Here is some dummy message");
}

void CExceptionTests::TestWithException(void)
{
    BOOST_CHECK_THROW( ThrowSomeException(), CException );
}

void CExceptionTests::TestWithoutException(void)
{
    BOOST_CHECK_NO_THROW( FuncWithoutException() );
}


using namespace boost::unit_test;

// Initialization function - it must be in global namespace, i.e. outside of
// any BEGIN_NCBI_SCOPE-END_NCBI_SCOPE structures.
test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    test_suite* ts = BOOST_TEST_SUITE("Main Sample Test Suite");

    test_case* tc = BOOST_TEST_CASE(TestSimpleTools);
    ts->add(tc);

    test_suite* sub_ts = BOOST_TEST_SUITE("Test Suite for exceptions");
    ts->add(sub_ts);
    // all tests in sub_ts should not be called if tc fails
    sub_ts->depends_on(tc);

    // Object for 2 other test cases. It will be destroyed when last
    // shared_ptr for it will be destroyed.
    boost::shared_ptr<CExceptionTests> testObj(new CExceptionTests());

    test_case* tc_class =
        BOOST_CLASS_TEST_CASE(&CExceptionTests::TestWithException, testObj);
    sub_ts->add(tc_class);

    tc_class =
        BOOST_CLASS_TEST_CASE(&CExceptionTests::TestWithoutException, testObj);
    sub_ts->add(tc_class);


    // Here (or anywhere else in this function) you can call any other
    // initialization code which you want to execute before all tests.


    return ts;
}
