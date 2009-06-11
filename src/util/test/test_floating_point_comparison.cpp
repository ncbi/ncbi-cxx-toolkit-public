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
 * Author:  Sergey Satskiy
 *
 * File Description:
 *   Test of floating point comparison
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <ncbiconf.h>

#include <util/floating_point.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE( FPComparison01 )
{
    double lhs = 1.111e-10;
    double rhs = 1.112e-10;
    double fraction = 0.0008999;

    if (g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                eFP_WithFraction, fraction)) {
        BOOST_FAIL( "Fraction comparison (double) failed. "
                    "Expected: false. Received: true." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison02 )
{
    double lhs = 1.23456e-10;
    double rhs = 1.23457e-10;
    double percent = 0.0001;

    if (g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                eFP_WithPercent, percent)) {
        BOOST_FAIL( "Percent comparison (double) failed. "
                    "Expected: false. Received: true." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison03 )
{
    double lhs = 1.111e-10;
    double rhs = 1.112e-10;
    double fraction = 0.009;

    if (!g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                 eFP_WithFraction, fraction)) {
        BOOST_FAIL( "Fraction comparison (double) failed. "
                    "Expected: true. Received: false." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison04 )
{
    double lhs = 1.23456e-10;
    double rhs = 1.23457e-10;
    double percent = 0.001;

    if (!g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                 eFP_WithPercent, percent)) {
        BOOST_FAIL( "Percent comparison (double) failed. "
                    "Expected: true. Received: false." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison05 )
{
    float lhs = 1.111e-10F;
    float rhs = 1.112e-10F;
    float fraction = 0.0008999F;

    if (g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                eFP_WithFraction, fraction)) {
        BOOST_FAIL( "Fraction comparison (float) failed. "
                    "Expected: false. Received: true." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison06 )
{
    float lhs = 1.23456e-10F;
    float rhs = 1.23457e-10F;
    float percent = 0.0001F;

    if (g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                eFP_WithPercent, percent)) {
        BOOST_FAIL( "Percent comparison (float) failed. "
                    "Expected: false. Received: true." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison07 )
{
    float lhs = 1.111e-10F;
    float rhs = 1.112e-10F;
    float fraction = 0.009F;

    if (!g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                 eFP_WithFraction, fraction)) {
        BOOST_FAIL( "Fraction comparison (float) failed. "
                    "Expected: true. Received: false." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison08 )
{
    float lhs = 1.23456e-10F;
    float rhs = 1.23457e-10F;
    float percent = 0.001F;

    if (!g_FloatingPoint_Compare(lhs, eFP_EqualTo, rhs,
                                 eFP_WithFraction, percent)) {
        BOOST_FAIL( "Percent comparison (float) failed. "
                    "Expected: true. Received: false." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison09 )
{
    double lhs = 1.23456e-10;
    double rhs = 1.23457e-10;
    double percent = 0.0001;

    if (!g_FloatingPoint_Compare(lhs, eFP_LessThan, rhs,
                                 eFP_WithPercent, percent)) {
        BOOST_FAIL( "Percent less double failed. "
                    "Expected: false. Received: true." );
    }
    return;
}


BOOST_AUTO_TEST_CASE( FPComparison10 )
{
    double lhs = 1.23457e-10;
    double rhs = 1.23456e-10;
    double percent = 0.0001;

    if (!g_FloatingPoint_Compare(lhs, eFP_GreaterThan, rhs,
                                 eFP_WithPercent, percent)) {
        BOOST_FAIL( "Percent greater double failed. "
                    "Expected: false. Received: true." );
    }
    return;
}

