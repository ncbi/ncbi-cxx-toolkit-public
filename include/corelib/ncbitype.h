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

/// @file ncbitype.h
///
/// Defines NCBI C/C++ fixed-size types:
/// -  Char, Uchar
/// -  Int1, Uint1
/// -  Int2, Uint2
/// -  Int4, Uint4
/// -  Int8, Uint8
/// -  Ncbi_BigScalar


#include <ncbiconf.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif


/** @addtogroup Portability
 *
 * @{
 */


/*
 * threads configuration
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


typedef          char  Char;    ///< Alias for char
typedef signed   char  Schar;   ///< Alias for signed char
typedef unsigned char  Uchar;   ///< Alias for unsigned char
typedef signed   char  Int1;    ///< Alias for signed char
typedef unsigned char  Uint1;   ///< Alias for unsigned char
typedef signed   short Int2;    ///< Alias for signed short
typedef unsigned short Uint2;   ///< Alias for unsigned short
typedef signed   int   Int4;    ///< Alias for signed int
typedef unsigned int   Uint4;   ///< Alias for unsigned int


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

/// Signed 8 byte sized integer.
typedef signed   INT8_TYPE Int8;    

/// Unsigned 8 byte sized integer.
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

/// Define large scalar type.
///
/// This is platform dependent. It could be an Int8, long double, double
/// or void*.
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


/* Undef auxiliaries
 */

#undef BIG_SIZE
#undef BIG_TYPE
#undef INT8_TYPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/03/08 14:54:50  vasilche
 * Use C style comment in this file.
 *
 * Revision 1.14  2005/01/31 16:32:54  ucko
 * Ensure that intptr_t is available.
 *
 * Revision 1.13  2003/09/03 14:46:33  siyan
 * Minor doxygen related changes.
 *
 * Revision 1.12  2003/04/01 19:19:03  siyan
 * Added doxygen support
 *
 * Revision 1.11  2003/03/03 20:34:50  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.10  2002/09/20 14:13:51  vasilche
 * Fixed inconsistency of NCBI_*_THREADS macros
 *
 * Revision 1.9  2002/04/11 20:39:20  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.8  2001/05/30 16:17:23  vakatov
 * Introduced #NCBI_USE_INT64 -- in oreder to use "__int64" type
 * only when absolutely necessary (otherwise it conflicted with
 * "long long" for the Intel C++ compiler).
 *
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

#endif /* NCBITYPE__H */
