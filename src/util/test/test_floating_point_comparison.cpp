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
#include <corelib/ncbistd.hpp>
#include <util/floating_point.hpp>

USING_NCBI_SCOPE;

int main(int argc, char** argv)
{
    int         ret = 0;

    /*
     * Double tests
     */

    double      v1df = 1.111e-10;
    double      v2df = 1.112e-10;
    double      fraction_d = 0.0008999;

    if (g_FloatingPoint_EqualWithFractionTolerance(v1df, v2df, fraction_d)) {
        ret++;
        cerr << "Fraction comparison (double) failed. "
                "Expected: false. Received: true." << endl;
    }


    double      v1dp = 1.23456e-10;
    double      v2dp = 1.23457e-10;
    double      percent_d = 0.0001;

    if (g_FloatingPoint_EqualWithPercentTolerance(v1dp, v2dp, percent_d)) {
        ret++;
        cerr << "Percent comparison (double) failed. "
                "Expected: false. Received: true." << endl;
    }


    v1df = 1.111e-10;
    v2df = 1.112e-10;
    fraction_d = 0.009;

    if (!g_FloatingPoint_EqualWithFractionTolerance(v1df, v2df, fraction_d)) {
        ret++;
        cerr << "Fraction comparison (double) failed. "
                "Expected: true. Received: false." << endl;
    }

    v1dp = 1.23456e-10;
    v2dp = 1.23457e-10;
    percent_d = 0.001;

    if (!g_FloatingPoint_EqualWithPercentTolerance(v1dp, v2dp, percent_d)) {
        ret++;
        cerr << "Percent comparison (double) failed. "
                "Expected: true. Received: false." << endl;
    }

    /*
     * float tests
     */
    float       v1ff = 1.111e-10F;
    float       v2ff = 1.112e-10F;
    float       fraction_f = 0.0008999F;

    if (g_FloatingPoint_EqualWithFractionTolerance(v1ff, v2ff, fraction_f)) {
        ret++;
        cerr << "Fraction comparison (float) failed. "
                "Expected: false. Received: true." << endl;
    }


    float       v1fp = 1.23456e-10F;
    float       v2fp = 1.23457e-10F;
    float       percent_f = 0.0001F;

    if (g_FloatingPoint_EqualWithPercentTolerance(v1fp, v2fp, percent_f)) {
        ret++;
        cerr << "Percent comparison (float) failed. "
                "Expected: false. Received: true." << endl;
    }


    v1ff = 1.111e-10F;
    v2ff = 1.112e-10F;
    fraction_f = 0.009F;

    if (!g_FloatingPoint_EqualWithFractionTolerance(v1ff, v2ff, fraction_f)) {
        ret++;
        cerr << "Fraction comparison (float) failed. "
                "Expected: true. Received: false." << endl;
    }

    v1fp = 1.23456e-10F;
    v2fp = 1.23457e-10F;
    percent_f = 0.001F;

    if (!g_FloatingPoint_EqualWithPercentTolerance(v1fp, v2fp, percent_f)) {
        ret++;
        cerr << "Percent comparison (float) failed. "
                "Expected: true. Received: false." << endl;
    }

    /*
     * Mixing types for arguments is not allowed (not compiled)
     */

    return ret;
}

