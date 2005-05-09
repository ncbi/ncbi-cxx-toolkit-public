#ifndef NCBIFLOAT__H
#define NCBIFLOAT__H

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

/// @file ncbifloat.h
/// Floating-point support routines.


/** @addtogroup Exception
 *
 * @{
 */


#include <ncbiconf.h>

#if defined(NCBI_OS_MSWIN)
#   include <float.h>
#elif defined(HAVE_IEEEFP_H)
#   include <ieeefp.h>
#endif


#if defined(NCBI_OS_MSWIN)
/// Define value of isnan (Is Not A Number).
///
/// Checks double-precision value for not a number (NaN).
#   define isnan _isnan
#elif defined(NCBI_OS_DARWIN)  &&  defined(MATH_ERRNO)  &&  !defined(isnan)
/// Define value of isnan (Is Not A Number).
///
/// <math.h> changed a lot between 10.1 and 10.2; the presence of
/// MATH_ERRNO indicates 10.2, which needs this hack, thanks to
/// <cmath>'s obnoxious removal of <math.h>'s isnan macro.
#   define isnan __isnand
#endif


#if defined(NCBI_OS_MSWIN)
/// Define value of finite (Is Finite).
///
/// Checks whether given double-precision floating point value is finite
#   define finite _finite
#endif

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/05/09 14:17:32  ucko
 * Update Darwin hack -- __isnan seems to be unavailable in the latest
 * 10.3 version of libSystem, and __isnand is more appropriate anyway.
 *
 * Revision 1.8  2004/05/04 18:07:39  ucko
 * Use configure's test for ieeefp.h rather than harcoding an incomplete
 * list of platforms.
 *
 * Revision 1.7  2004/05/04 17:58:16  gouriano
 * Not include ieeefp.h on Linux
 *
 * Revision 1.6  2004/05/04 17:05:58  gouriano
 * Added definition of finite
 *
 * Revision 1.5  2003/08/23 14:53:52  siyan
 * Documentation changes.
 *
 * Revision 1.4  2003/04/03 17:52:31  ucko
 * Properly close doxygen end-group comment.
 *
 * Revision 1.3  2003/04/01 14:19:42  siyan
 * Added doxygen support
 *
 * Revision 1.2  2003/02/14 15:48:44  ucko
 * Add a workaround for Mac OS X 10.2, where <cmath> clobbers <math.h>'s
 * #define of isnan but doesn't replace it with anything.
 *
 * Revision 1.1  2003/02/04 17:02:07  gouriano
 * initial revision
 *
 *
 * ==========================================================================
 */

#endif /* NCBIFLOAT__H */
