/*   ncbimath.h
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
* File Name:  ncbimath.h
*
* Author:  Gish, Kans, Ostell, Schuler
*
* Version Creation Date:   10/23/91
*
* $Revision$
*
* File Description:
*   	prototypes for portable math library
*
* Modifications:
* --------------------------------------------------------------------------
* Date     Name        Description of modification
* -------  ----------  -----------------------------------------------------
*
* $Log$
* Revision 1.3  2003/08/25 22:30:24  dondosha
* Added LnGammaInt definition and Nlm_Factorial prototype
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
* Revision 6.1  1999/11/24 17:29:16  sicotte
* Added LnFactorial function
*
* Revision 6.0  1997/08/25 18:16:37  madden
* Revision changed to 6.0
*
* Revision 5.2  1996/12/03 21:48:33  vakatov
* Adopted for 32-bit MS-Windows DLLs
*
 * Revision 5.1  1996/06/20  14:08:00  madden
 * Changed int to Int4, double to Nlm_FloatHi
 *
 * Revision 5.0  1996/05/28  13:18:57  ostell
 * Set to revision 5.0
 *
 * Revision 4.0  1995/07/26  13:46:50  ostell
 * force revision to 4.0
 *
 * Revision 2.4  1995/05/15  18:45:58  ostell
 * added Log line
 *
*
*
* ==========================================================================
*/

#include <algo/blast/core/ncbi_std.h> 

#ifndef _NCBIMATH_
#define _NCBIMATH_

#ifdef __cplusplus
extern "C" {
#endif

/* log(x+1) for all x > -1 */
extern double Nlm_Log1p (double);

/* exp(x)-1 for all x */
extern double Nlm_Expm1 (double);

/* Factorial function */
extern double Nlm_Factorial(Int4 n);

/* Logarithm of the factorial Fn */
extern double Nlm_LnFactorial (double x);

/* log(gamma(n)), integral n */
extern double Nlm_LnGammaInt (Int4);

/* Romberg numerical integrator */
extern double Nlm_RombergIntegrate (double (*f) (double, void*), void* fargs, double p, double q, double eps, Int4 epsit, Int4 itmin);

/* Greatest common divisor */
extern long Nlm_Gcd (long, long);

/* Nearest integer */
extern long Nlm_Nint (double);

/* Integral power of x */
extern double Nlm_Powi (double x, Int4 n);

#define Log1p	Nlm_Log1p
#define Expm1	Nlm_Expm1
#define LnFactorial	Nlm_LnFactorial
#define RombergIntegrate	Nlm_RombergIntegrate
#define Gcd	Nlm_Gcd
#define Nint	Nlm_Nint
#define Powi	Nlm_Powi
#define LnGammaInt Nlm_LnGammaInt

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


#endif /* !_NCBIMATH_ */

