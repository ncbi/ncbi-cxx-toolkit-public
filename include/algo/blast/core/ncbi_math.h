/* $Id$
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
 * Authors:  Gish, Kans, Ostell, Schuler
 *
 * Version Creation Date:   10/23/91
 *
 * ==========================================================================
 */

/** @file ncbi_math.h
 * Prototypes for portable math library (ported from C Toolkit)
 * @todo FIXME doxygen comments
 */

#include <algo/blast/core/ncbi_std.h> 

#ifndef _NCBIMATH_
#define _NCBIMATH_

#ifdef __cplusplus
extern "C" {
#endif

/* log(x+1) for all x > -1 */
extern double BLAST_Log1p (double);

/* exp(x)-1 for all x */
extern double BLAST_Expm1 (double);

/* Factorial function */
extern double BLAST_Factorial(Int4 n);

/* Logarithm of the factorial Fn */
extern double BLAST_LnFactorial (double x);

/* log(gamma(n)), integral n */
extern double BLAST_LnGammaInt (Int4);

/* Romberg numerical integrator */
extern double BLAST_RombergIntegrate (double (*f) (double, void*), void* fargs, double p, double q, double eps, Int4 epsit, Int4 itmin);

/* Greatest common divisor */
extern long BLAST_Gcd (long, long);

/* Nearest integer */
extern long BLAST_Nint (double);

/* Integral power of x */
extern double BLAST_Powi (double x, Int4 n);

/* Error codes for the CTX_NCBIMATH context */
#define ERR_NCBIMATH_INVAL	1 /* invalid parameter */
#define ERR_NCBIMATH_DOMAIN	2 /* domain error */
#define ERR_NCBIMATH_RANGE	3 /* range error */
#define ERR_NCBIMATH_ITER	4 /* iteration limit exceeded */

#define LOGDERIV_ORDER_MAX	4
#define POLYGAMMA_ORDER_MAX	LOGDERIV_ORDER_MAX

#define NCBIMATH_PI	3.1415926535897932384626433832795
#define NCBIMATH_E	2.7182818284590452353602874713527
/* Euler's constant */
#define NCBIMATH_EULER 0.5772156649015328606065120900824
/* Catalan's constant */
#define NCBIMATH_CATALAN	0.9159655941772190150546035149324

/* sqrt(2) */
#define NCBIMATH_SQRT2	1.4142135623730950488016887242097
/* sqrt(3) */
#define NCBIMATH_SQRT3	1.7320508075688772935274463415059
/* sqrt(PI) */
#define NCBIMATH_SQRTPI 1.7724538509055160272981674833411
/* Natural log(2) */
#define NCBIMATH_LN2	0.69314718055994530941723212145818
/* Natural log(10) */
#define NCBIMATH_LN10	2.3025850929940456840179914546844
/* Natural log(PI) */
#define NCBIMATH_LNPI	1.1447298858494001741434273513531

#ifdef __cplusplus
}
#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.7  2004/05/19 14:52:01  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.6  2003/09/26 20:38:12  dondosha
 * Returned prototype for the factorial function (BLAST_Factorial)
 *
 * Revision 1.5  2003/09/26 19:02:31  madden
 * Prefix ncbimath functions with BLAST_
 *
 * Revision 1.4  2003/09/10 21:35:20  dondosha
 * Removed Nlm_ prefix from math functions
 *
 * Revision 1.3  2003/08/25 22:30:24  dondosha
 * Added LnGammaInt definition and Factorial prototype
 *
 * Revision 1.2  2003/08/11 14:57:16  dondosha
 * Added algo/blast/core path to all #included headers
 *
 * Revision 1.1  2003/08/02 16:32:11  camacho
 * Moved ncbimath.h -> ncbi_math.h
 *
 * Revision 1.2  2003/08/01 21:18:48  dondosha
 * Correction of a #include
 *
 * Revision 1.1  2003/08/01 21:03:40  madden
 * Cleaned up version of file for C++ toolkit
 *
 * ===========================================================================
 */


#endif /* !_NCBIMATH_ */

