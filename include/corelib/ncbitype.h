#ifndef NCBITYPE__H
#define NCBITYPE__H

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
* Author:  Denis Vakatov
*
* File Description:
*
*   NCBI C/C++ fixed-size types:
*      Char, Uchar
*      Int1, Uint1
*      Int2, Uint2
*      Int4, Uint4
*      Int8, Uint8
*      Ncbi_BigScalar
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.7  2001/04/16 18:51:05  vakatov
* "Char" to become "char" rather than "signed char".
* Introduced "Schar" for "signed char".
*
* Revision 1.6  2001/01/03 17:41:49  vakatov
* All type limits moved to "ncbi_limits.h"
*
* Revision 1.5  1999/04/14 19:49:22  vakatov
* Fixed suffixes of the 8-byte int constants
* Introduced limits for native signed and unsigned integer types
* (char, short, int)
*
* Revision 1.4  1999/03/11 16:32:02  vakatov
* BigScalar --> Ncbi_BigScalar
*
* Revision 1.3  1998/11/06 22:42:39  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.2  1998/10/30 20:08:34  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.1  1998/10/20 22:58:55  vakatov
* Initial revision
*
* ==========================================================================
*/


#include <ncbiconf.h>



/* Char, Uchar, Int[1,2,4], Uint[1,2,4]
 */

#if (SIZEOF_CHAR != 1)
#  error "Unsupported size of char(must be 1 byte)"
#endif
#if (SIZEOF_SHORT != 2)
#  error "Unsupported size of short int(must be 2 bytes)"
#endif
#if (SIZEOF_INT != 4)
#  error "Unsupported size of int(must be 4 bytes)"
#endif


typedef          char  Char;
typedef signed   char  Schar;
typedef unsigned char  Uchar;
typedef signed   char  Int1;
typedef unsigned char  Uint1;
typedef signed   short Int2;
typedef unsigned short Uint2;
typedef signed   int   Int4;
typedef unsigned int   Uint4;


/* Int8, Uint8
 */

#if   (SIZEOF_LONG == 8)
#  define INT8_TYPE long
#elif (SIZEOF_LONG_LONG == 8)
#  define INT8_TYPE long long
#elif (SIZEOF___INT64 == 8)
#  define INT8_TYPE __int64
#else
#  error "This platform does not support 8-byte integer"
#endif

typedef signed   INT8_TYPE Int8;
typedef unsigned INT8_TYPE Uint8;


/* BigScalar
 */

#define BIG_TYPE INT8_TYPE
#define BIG_SIZE 8
#if (SIZEOF_LONG_DOUBLE > BIG_SIZE)
#  undef  BIG_TYPE
#  undef  BIG_SIZE
#  define BIG_TYPE long double 
#  define BIG_SIZE SIZEOF_LONG_DOUBLE
#endif
#if (SIZEOF_DOUBLE > BIG_SIZE)
#  undef  BIG_TYPE
#  undef  BIG_SIZE
#  define BIG_TYPE double
#  define BIG_SIZE SIZEOF_DOUBLE
#endif
#if (SIZEOF_VOIDP > BIG_SIZE)
#  undef  BIG_TYPE
#  undef  BIG_SIZE
#  define BIG_TYPE void*
#  define BIG_SIZE SIZEOF_VOIDP
#endif

typedef BIG_TYPE Ncbi_BigScalar;


/* Undef auxiliaries
 */

#undef BIG_SIZE
#undef BIG_TYPE
#undef INT8_TYPE


#endif /* NCBITYPE__H */
