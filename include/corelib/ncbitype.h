#ifndef NCBITYPE__H
#define NCBITYPE__H

/*  $RCSfile$  $Revision$  $Date$
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
*   NCBI C/C++ fixed-size types and their limits
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/20 22:58:55  vakatov
* Initial revision
*
* ==========================================================================
*/

/* Import platform-specific definitions
 * (for all UNIX and MS-Win platforms they are automatically generated
 * by "autoconf" scripts)
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

typedef signed   char  Char;
typedef unsigned char  Uchar;
typedef signed   char  Int1;
typedef unsigned char  Uint1;
typedef signed   short Int2;
typedef unsigned short Uint2;
typedef signed   int   Int4;
typedef unsigned int   Uint4;


/* Int8, Uint8
 */
#if (SIZEOF_LONG == 8)
#  define INT8_TYPE long
#if (SIZEOF_LONG_LONG == 8)
#  define INT8_TYPE long long
#if (SIZEOF___INT64 == 8)
#  define INT8_TYPE long
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

typedef BIG_TYPE BigScalar;


/* Integer limits
 */
#ifdef __cplusplus
const Int1  kMin_I1  -128;
const Int1  kMax_I1   127;
const Uint1 kMax_UI1  255;
const Int2  kMin_I2  -32768;
const Int2  kMax_I2   32767;
const Uint2 kMax_UI2  65535;
const Int4  kMin_I4  -2147483648;
const Int4  kMax_I4   2147483647;
const Uint4 kMax_UI4  4294967295;
const Int8  kMin_I8  -9223372036854775808;
const Int8  kMax_I8   9223372036854775807;
const Uint8 kMax_UI8  18446744073709551615;
#else
/* TODO */
#endif


/* Undef auxiliaries
 */
#undef BIG_SIZE
#undef BIG_TYPE
#undef INT8_TYPE


#endif /* NCBITYPE__H */
