#ifndef FLOATING_POINT__HPP
#define FLOATING_POINT__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description: Wrappers around boost floating point comparison
 *                   functionality
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiexpt.hpp>
#include <util/impl/floating_point_comparison.hpp>


BEGIN_NCBI_SCOPE


/// Floating point supported operations
enum EFloatingPointOperation {
    eFP_LessThan,       ///< '<' operation
    eFP_EqualTo,        ///< '==' operation
    eFP_GreaterThan     ///< '>' operation
};

/// Supported types of tolerances
enum EFloatingPointTolerance {
    eFP_WithFraction,   ///< fraction tolerance
    eFP_WithPercent     ///< percent tolerance
};


/// Floating point comparisons
///
/// It uses boost implementation of the comparison so the details are available
/// here:
/// http://www.boost.org/doc/libs/1_39_0/libs/test/doc/html/utf/testing-tools/floating_point_comparison.html
///
/// @param lhs
///   Left hand side value to compare
/// @param operation
///   Required comparison
/// @param rhs
///   Right hand side value to compare
/// @param tolerance
///   Tolerance type to be used
/// @param threshold
///   Tolerance value
/// @return
///   Evaluation of the required operation
template< typename TFloatingPoint>
bool g_FloatingPoint_Compare(TFloatingPoint          lhs,
                             EFloatingPointOperation operation,
                             TFloatingPoint          rhs,
                             EFloatingPointTolerance tolerance,
                             TFloatingPoint          threshold)
{
    if (tolerance == eFP_WithFraction) {
        switch (operation) {
        case eFP_LessThan:
            return (lhs < rhs) &&
                   !boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::fraction_tolerance(threshold));
        case eFP_EqualTo:
            return boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::fraction_tolerance(threshold));
        case eFP_GreaterThan:
            return (lhs > rhs) &&
                   !boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::fraction_tolerance(threshold));
        default:
            _TROUBLE;   // Unknown operation
        }
    } else if (tolerance == eFP_WithPercent) {
        switch (operation) {
        case eFP_LessThan:
            return (lhs < rhs) &&
                   !boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::percent_tolerance(threshold));
        case eFP_EqualTo:
            return boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::percent_tolerance(threshold));
        case eFP_GreaterThan:
            return (lhs > rhs) &&
                   !boost_fp_impl::check_is_close(lhs, rhs,
                            boost_fp_impl::percent_tolerance(threshold));
        default:
            _TROUBLE;   // Unknown operation
        }
    } else {
        _TROUBLE;       // Unknown tolerance
    }
    return false;
}

END_NCBI_SCOPE

#endif // FLOATING_POINT__HPP

