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
*   Sample unit tests file with advanced developing techniques.
*
* This file represents advanced developing techniques in usage of Ncbi.Test
* framework based on Boost.Test framework. For more simple example look into
* another sample - unit_test_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    //
    // Useful function that can be used here:
    // if (some condition) {
    //     NcbiTestSetGlobalDisabled();
    //     return;
    // }

    printf("Initialization function executed\n");
}

NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)

    printf("Finalization function executed\n");
}

NCBITEST_INIT_CMDLINE(descrs)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    descrs->AddOptionalPositional("some_arg",
                                  "This is custom sample command line "
                                  "argument that will be distinguished by "
                                  "test application",
                                  CArgDescriptions::eString);
}

NCBITEST_INIT_VARIABLES(parser)
{
    // Here we are initializing variables that will be used in conditions
    // in unit_test_alt_sample.ini

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    parser->AddSymbol("some_arg_passed", args["some_arg"].HasValue());
}

NCBITEST_INIT_TREE()
{
    // Here we can set some dependencies between tests (if one disabled or
    // failed other shouldn't execute) and hard-coded disablings. Note though
    // that more preferable way to make always disabled test is add to
    // ini-file in UNITTESTS_DISABLE section this line:
    // TestName = true

    NCBITEST_DEPENDS_ON(DependentOnArg, UsingArg);

    NCBITEST_DISABLE(AlwaysDisabled);
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
    printf("Here is some dummy message\n");
}

BOOST_AUTO_TEST_CASE(TestWithoutException)
{
    BOOST_CHECK_NO_THROW( s_FuncWithoutException() );
}

BOOST_AUTO_TEST_CASE_TIMEOUT(TestTimeout, 1);
BOOST_AUTO_TEST_CASE(TestTimeout)
{
    // This test will always fail due to timeout
    SleepSec(2);
}

BOOST_AUTO_TEST_CASE(TestDependentOnArg)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    string arg_value = args["some_arg"].AsString();

    LOG_POST("Argument value is " << arg_value);
}

BOOST_AUTO_TEST_CASE(TestAlwaysDisabled)
{
    printf("This message will never be printed.\n");
}

BOOST_AUTO_TEST_CASE(TestDisabledInConfig)
{
    printf("This message will be printed only after "
           "proper editing of *.ini file or after adding command line "
           "argument --run_test='*DisabledInConfig'.\n");
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    string arg_value = args["some_arg"].AsString();

    BOOST_CHECK( !arg_value.empty() );
}
