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
*   Sample unit tests file for the mainstream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework (which
* is based on Boost.Test framework. For more advanced techniques look into
* another sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default in"
         "order to avoid unwanted failure during the daily automated builds.");
}


NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    cout << "Finalization function executed" << endl;
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
    // If something during checking can throw an exception and you don't want
    // it to do that you can use different macros:
    NCBITEST_CHECK_EQUAL(s1, s2);
    // ... or
    NCBITEST_CHECK(s1 == s2);
    // ... or
    NCBITEST_CHECK_MESSAGE(s1 == s2, "Object s1 not equal object s2");

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
    cout << "Here is some dummy message" << endl;
}

BOOST_AUTO_TEST_CASE(TestWithoutException)
{
    BOOST_CHECK_NO_THROW( s_FuncWithoutException() );
}


BOOST_AUTO_TEST_CASE_TIMEOUT(TestTimeout, 2);
BOOST_AUTO_TEST_CASE(TestTimeout)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if ( args["enable_TestTimeout"] ) {
        // This test will always fail due to timeout
        SleepSec(4);
    }
    else {
        cout << "To run TestTimeout pass flag '-enable_TestTimeout'" << endl;
    }
}
