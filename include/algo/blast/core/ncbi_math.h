#ifndef ALGO_BLAST_CORE__NCBIMATH
#define ALGO_BLAST_CORE__NCBIMATH

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
 */

#include <algo/blast/core/ncbi_std.h> 
#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Natural logarithm with shifted input
 *  @param x input operand (x > -1)
 *  @return log(x+1)
 */
NCBI_XBLAST_EXPORT 
double BLAST_Log1p (double x);

/** Exponentional with base e 
 *  @param x input operand
 *  @return exp(x) - 1
 */
NCBI_XBLAST_EXPORT 
double BLAST_Expm1 (double x);

/** Factorial function
 *  @param n input operand
 *  @return (double)(1 * 2 * 3 * ... * n)
 */
NCBI_XBLAST_EXPORT 
double BLAST_Factorial(Int4 n);

/** Logarithm of the factorial 
 *  @param x input operand
 *  @return log(1 * 2 * 3 * ... * x)
 */
NCBI_XBLAST_EXPORT 
double BLAST_LnFactorial (double x);

/** log(gamma(n)), integral n 
 *  @param n input operand
 *  @return log(1 * 2 * 3 * ... (n-1))
 */
NCBI_XBLAST_EXPORT 
double BLAST_LnGammaInt (Int4 n);

/** Romberg numerical integrator 
 *  @param f Pointer to the function to integrate; the first argument
 *               is the variable to integrate over, the second is a pointer
 *               to a list of additional arguments that f may need
 *  @param fargs Pointer to an array of extra arguments or parameters
 *               needed to compute the function to be integrated. None
 *               of the items in this list may vary over the region
 *               of integration
 *  @param p Left-hand endpoint of the integration interval
 *  @param q Right-hand endpoint of the integration interval
 *           (q is assumed > p)
 *  @param eps The relative error tolerance that indicates convergence
 *  @param epsit The number of consecutive diagonal entries in the 
 *               Romberg array whose relative difference must be less than
 *               eps before convergence is assumed. This is presently 
 *               limited to 1, 2, or 3
 *  @param itmin The minimum number of diagnonal Romberg entries that
 *               will be computed
 *  @return The computed integral of f() between p and q
 */
NCBI_XBLAST_EXPORT 
double BLAST_RombergIntegrate (double (*f) (double, void*), 
                               void* fargs, double p, double q, 
                               double eps, Int4 epsit, Int4 itmin);

/** Greatest common divisor 
 *  @param a First operand (any integer)
 *  @param b Second operand (any integer)
 *  @return The largest integer that evenly divides a and b
 */
NCBI_XBLAST_EXPORT 
Int4 BLAST_Gcd (Int4 a, Int4 b);

/** Divide 3 numbers by their greatest common divisor
 * @param a First integer [in] [out]
 * @param b Second integer [in] [out]
 * @param c Third integer [in] [out]
 * @return The greatest common divisor
 */
NCBI_XBLAST_EXPORT 
Int4 BLAST_Gdb3(Int4* a, Int4* b, Int4* c);

/** Nearest integer 
 *  @param x Input to round (rounded value must be representable
 *           as a 32-bit signed integer)
 *  @return floor(x + 0.5);
 */
NCBI_XBLAST_EXPORT 
long BLAST_Nint (double x);

/** Integral power of x 
 * @param x floating-point base of the exponential
 * @param n (integer) exponent
 * @return x multiplied by itself n times
 */
NCBI_XBLAST_EXPORT 
double BLAST_Powi (double x, Int4 n);

/** Number of derivatives of log(x) to carry in gamma-related 
    computations */
#define LOGDERIV_ORDER_MAX	4  
/** Number of derivatives of polygamma(x) to carry in gamma-related 
    computations for non-integral values of x */
#define POLYGAMMA_ORDER_MAX	LOGDERIV_ORDER_MAX

/** value of pi is only used in gamma-related computations */
#define NCBIMATH_PI	3.1415926535897932384626433832795

/** Natural log(2) */
#define NCBIMATH_LN2	0.69314718055994530941723212145818
/** Natural log(PI) */
#define NCBIMATH_LNPI	1.1447298858494001741434273513531

#ifdef __cplusplus
}
#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.11  2005/03/10 16:12:59  papadopo
 * doxygen fixes
 *
 * Revision 1.10  2004/11/18 21:22:10  dondosha
 * Added BLAST_Gdb3, used in greedy alignment; removed extern and added NCBI_XBLAST_EXPORT to all prototypes
 *
 * Revision 1.9  2004/11/02 13:54:33  papadopo
 * small doxygen fixes
 *
 * Revision 1.8  2004/11/01 16:37:57  papadopo
 * Add doxygen tags, remove unused constants
 *
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


#endif /* !ALGO_BLAST_CORE__NCBIMATH */

