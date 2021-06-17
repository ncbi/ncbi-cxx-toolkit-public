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
 * Authors:  Sergey satskiy
 *
 * File Description:
 *   TEST for uncaught exceptions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


bool    flag = false;


class ExceptionInDestructor
{
    public:
        ExceptionInDestructor()
        {}

        ~ExceptionInDestructor()
        #if !(defined(_MSC_VER) && _MSC_VER < 1900)
            // Visual studio 2013 does not support this while works as needed.
            // The other compilers must have noexcept(false) to work properly.
            noexcept(false)
        #endif
        {
            if (std::uncaught_exception()) {
                flag = true;
                return;
            }

            // Throw an exception if we are not handling another exception
            throw std::runtime_error("From ExceptionInDestructor");
        }
};


BOOST_AUTO_TEST_CASE(ScopeEnd)
{
    flag = false;

    try {
        ExceptionInDestructor   e;
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        BOOST_CHECK(what == std::string("From ExceptionInDestructor"));
    } catch (...) {
        BOOST_FAIL("Expected exception from a destructor");
    }

    BOOST_CHECK(flag == false);
}


BOOST_AUTO_TEST_CASE(OuterException)
{
    flag = false;
    try {
        ExceptionInDestructor   e;
        throw std::runtime_error("Outer exception");
    } catch (const std::exception &  exc) {
        string  what = exc.what();
        BOOST_CHECK(what == std::string("Outer exception"));
    } catch (...) {
        BOOST_FAIL("Expected outer exception");
    }

    BOOST_CHECK(flag == true);
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("Uncaught exceptions test");
    //SetDiagPostLevel(eDiag_Info);
}
