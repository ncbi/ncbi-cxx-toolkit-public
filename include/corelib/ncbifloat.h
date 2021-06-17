#ifndef CORELIB___NCBIFLOAT__H
#define CORELIB___NCBIFLOAT__H

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
 * Author:  Andrei Gourianov
 *
 *      
 */

/**
 * @file ncbifloat.h
 *
 * Floating-point support routines.
 *
 */

/** @addtogroup Exception
 *
 * @{
 */


#include <ncbiconf.h>

#if defined(NCBI_OS_MSWIN)
#   include <float.h>
#elif defined(HAVE_IEEEFP_H)
#   if defined(NCBI_OS_CYGWIN) && defined(__cplusplus)
/* At least some versions of Cygwin's ieeefp.h fail to use extern "C",
 * requiring us to. :-/ */
extern "C" {
#      include <ieeefp.h>
}
#   else
#      include <ieeefp.h>
#   endif
#endif

#if defined(NCBI_HAVE_CXX11)  && \
    (!defined(__GLIBCXX__)  ||  defined(_GLIBCXX_USE_C99_MATH))
// Kludge to avoid formal ambiguity in C++ '11 between ::isnan, which
// returns int, and std::isnan, which returns bool.
#  include <cmath>
#  ifdef isnan
#    undef isnan
#  endif
#  if __cplusplus >= 201103L  &&  defined(_GLIBCXX_CONSTEXPR)  \
    &&  !defined(NCBI_COMPILER_ICC)  &&  !defined(__MIC__)  \
    &&  !defined(NCBI_COMPILER_ANY_CLANG)
#    define ISNAN_CONSTEXPR _GLIBCXX_CONSTEXPR
#  else
#    define ISNAN_CONSTEXPR
#  endif
namespace std {
    inline ISNAN_CONSTEXPR bool bool_isnan(float x)       { return isnan(x); }
    inline ISNAN_CONSTEXPR bool bool_isnan(double x)      { return isnan(x); }
    inline ISNAN_CONSTEXPR bool bool_isnan(long double x) { return isnan(x); }
}
// Unbreak <boost/math/special_functions/fpclassify.hpp>.
namespace boost {
    namespace math_detail {
        using std::bool_isnan;
    }
}
#  define isnan bool_isnan
#elif defined(NCBI_OS_DARWIN)
#  include <cmath>
#endif

#if defined(NCBI_OS_MSWIN)  &&  !defined(isnan)
/**
 * Define value of isnan (Is Not A Number).
 *
 * Checks double-precision value for not a number (NaN).
 */
#   define isnan _isnan
#endif

#if defined(NCBI_OS_MSWIN)
/**
 * Define value of finite (Is Finite).
 *
 * Checks whether given double-precision floating point value is finite
 */
#   define finite _finite
#elif defined(NCBI_OS_DARWIN)
/**
 * Define value of finite (Is Finite).
 *
 * Checks whether given double-precision floating point value is finite
 */
#   ifdef finite
#     undef finite
#   endif
#   define finite isfinite
#endif


#endif  /* CORELIB___NCBIFLOAT__H */


/* @} */
