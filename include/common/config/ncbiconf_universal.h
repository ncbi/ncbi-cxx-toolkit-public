#ifndef NCBICONF_UNIVERSAL_H
#define NCBICONF_UNIVERSAL_H

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
 * Authors: Denis Vakatov, Aaron Ucko
 *
 */

/** @file ncbiconf_universal.h
 ** Architecture-specific settings for universal builds.
 **/

#ifdef NCBI_OS_DARWIN
#  include <machine/limits.h>
#  include <sys/cdefs.h>
#  ifndef LONG_BIT /* <machine/limits.h>'s definition is conditional */
#    ifdef __LP64__
#      define LONG_BIT         64
#    else
#      define LONG_BIT         32
#    endif
#  endif
#  define NCBI_PLATFORM_BITS   LONG_BIT
#  define SIZEOF_CHAR          1
#  define SIZEOF_DOUBLE        8
#  define SIZEOF_FLOAT         4
#  define SIZEOF_INT           4
#  if __DARWIN_LONG_DOUBLE_IS_DOUBLE
#    define SIZEOF_LONG_DOUBLE 8
#  else
#    define SIZEOF_LONG_DOUBLE 16
#  endif
#  define SIZEOF_LONG_LONG     8
#  define SIZEOF_SHORT         2
#  define SIZEOF_WCHAR_T       4
/* Define these macros to literal constants rather than calculating them,
 * to avoid redefinition warnings when using some third-party libraries. */
#  if LONG_BIT == 64
#    define SIZEOF_LONG   8
#    define SIZEOF_SIZE_T 8
#    define SIZEOF_VOIDP  8
#  else
#    define SIZEOF_LONG   4
#    define SIZEOF_SIZE_T 4
#    define SIZEOF_VOIDP  4
#  endif
#  define SIZEOF___INT64    0 /* no such type */
#  ifdef __BIG_ENDIAN__
#    define WORDS_BIGENDIAN 1
#  endif
/* __CHAR_UNSIGNED__: N/A -- Darwin uses signed characters regardless
 * of CPU type, and GCC would define the macro itself if appropriate. */
#else
#  error No universal-binary configuration settings defined for your OS.
#endif

#endif  /* NCBICONF_UNIVERSAL_H */
