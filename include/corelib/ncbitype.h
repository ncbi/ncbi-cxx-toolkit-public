#ifndef CORELIB___NCBITYPE__H
#define CORELIB___NCBITYPE__H

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
 *
 */

/**
 * @file ncbitype.h
 *
 * Defines NCBI C/C++ fixed-size types.
 *
 *   -  Char, Uchar
 *   -  Int1, Uint1
 *   -  Int2, Uint2
 *   -  Int4, Uint4
 *   -  Int8, Uint8
 *   -  Ncbi_BigScalar
 *   -  Macros for constant values definition.
 *
 */

/** @addtogroup Portability
 *
 * @{
 */

#include <ncbiconf.h>

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif


/* Threads configuration
 */

#undef NCBI_NO_THREADS
#undef NCBI_THREADS
#undef NCBI_POSIX_THREADS
#undef NCBI_WIN32_THREADS

#if defined(_MT)  &&  !defined(NCBI_WITHOUT_MT)
#  if defined(NCBI_OS_MSWIN)
#    define NCBI_WIN32_THREADS
#  elif defined(NCBI_OS_UNIX)
#    define NCBI_POSIX_THREADS
#  else
#    define NCBI_NO_THREADS
#  endif
#else
#  define NCBI_NO_THREADS
#endif

#if !defined(NCBI_NO_THREADS)
#  define NCBI_THREADS
#endif


/* New/nonstandard keywords
 */
#if defined(__cplusplus)  &&  defined(NCBI_RESTRICT_CXX)
#  define NCBI_RESTRICT NCBI_RESTRICT_CXX
#elif !defined(__cplusplus)  &&  defined(NCBI_RESTRICT_C)
#  define NCBI_RESTRICT NCBI_RESTRICT_C
#elif __STDC_VERSION__ >= 199901 /* C99 specifies restrict */
#  define NCBI_RESTRICT restrict
#else
#  define NCBI_RESTRICT
#endif

#ifndef NCBI_FORCEINLINE
#  ifdef __cplusplus
#    define NCBI_FORCEINLINE inline
#  else
#    define NCBI_FORCEINLINE
#  endif
#endif

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


typedef          char  Char;    /**< Alias for char */
typedef signed   char  Schar;   /**< Alias for signed char */
typedef unsigned char  Uchar;   /**< Alias for unsigned char */
typedef signed   char  Int1;    /**< Alias for signed char */
typedef unsigned char  Uint1;   /**< Alias for unsigned char */
typedef signed   short Int2;    /**< Alias for signed short */
typedef unsigned short Uint2;   /**< Alias for unsigned short */
typedef signed   int   Int4;    /**< Alias for signed int */
typedef unsigned int   Uint4;   /**< Alias for unsigned int */


/* Int8, Uint8
 */

#if   (SIZEOF_LONG == 8)
#  define INT8_TYPE long
#elif (SIZEOF_LONG_LONG == 8)
#  define INT8_TYPE long long
#elif (SIZEOF___INT64 == 8)
#  define NCBI_USE_INT64 1
#  define INT8_TYPE __int64
#else
#  error "This platform does not support 8-byte integer"
#endif

/** Signed 8 byte sized integer */
typedef signed   INT8_TYPE Int8;    

/** Unsigned 8 byte sized integer */
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

/**
 * Define large scalar type.
 *
 * This is platform dependent. It could be an Int8, long double, double
 * or void*.
 */
typedef BIG_TYPE Ncbi_BigScalar;


#ifndef HAVE_INTPTR_T
#  if SIZEOF_INT == SIZEOF_VOIDP
typedef int intptr_t;
#  elif SIZEOF_LONG == SIZEOF_VOIDP
typedef long intptr_t;
#  elif SIZEOF_LONG_LONG == SIZEOF_VOIDP
typedef long long intptr_t;
#  else
#    error No integer type is the same size as a pointer!
#  endif
#endif

#ifndef HAVE_UINTPTR_T
#  if SIZEOF_INT == SIZEOF_VOIDP
typedef unsigned int uintptr_t;
#  elif SIZEOF_LONG == SIZEOF_VOIDP
typedef unsigned long uintptr_t;
#  elif SIZEOF_LONG_LONG == SIZEOF_VOIDP
typedef unsigned long long uintptr_t;
#  else
#    error No integer type is the same size as a pointer!
#  endif
#endif


/* Macros for constant values definition 
 */

#if (SIZEOF_LONG == 8)
#  define NCBI_CONST_INT8(v)   v##L
#  define NCBI_CONST_UINT8(v)  v##UL
#elif (SIZEOF_LONG_LONG == 8)
#  define NCBI_CONST_INT8(v)   v##LL
#  define NCBI_CONST_UINT8(v)  v##ULL
#elif defined(NCBI_USE_INT64)
#  define NCBI_CONST_INT8(v)   v##i64
#  define NCBI_CONST_UINT8(v)  v##ui64
#else
#  define NCBI_CONST_INT8(v)   v
#  define NCBI_CONST_UINT8(v)  v
#endif


/* Undef auxiliaries
 */

#undef BIG_SIZE
#undef BIG_TYPE
#undef INT8_TYPE


#endif  /* CORELIB___NCBITYPE__H */


/* @} */
