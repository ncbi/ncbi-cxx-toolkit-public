#ifndef _NCBILCL_
#define _NCBILCL_

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
 * Authors:  Vladimir Ivanov
 *
 * File Description:
 *    Redefine C Toolkit configuration macros using C++ Toolkit definitions
 *
 */


#include <corelib/ncbistl.hpp>

/** @addtogroup CToolsBridge
 *
 * @{
 */


/// Define ctransition namespace.

#define CTRANSITION_NS ctransition

#define BEGIN_CTRANSITION_SCOPE  BEGIN_SCOPE(CTRANSITION_NS)
#define END_CTRANSITION_SCOPE    END_SCOPE(CTRANSITION_NS)
#define USING_CTRANSITION_SCOPE  USING_SCOPE(CTRANSITION_NS)


/*----------------------------------------------------------------------*/
/*      OS                                                              */
/*----------------------------------------------------------------------*/

#ifdef NCBI_OS_UNIX
#  define OS_UNIX
#endif

#ifdef NCBI_OS_MSWIN
#  define OS_MSWIN
#endif

#ifdef NCBI_OS_BSD
#  define OS_UNIX_FREEBSD
#endif

#ifdef NCBI_OS_DARWIN
#  define OS_UNIX_DARWIN
#endif

#ifdef NCBI_OS_LINUX
#  define OS_UNIX_LINUX
#endif


/* Define to 1 on Cygwin. */
/* #undef NCBI_OS_CYGWIN */


/*----------------------------------------------------------------------*/
/*      Compiler                                                        */
/*----------------------------------------------------------------------*/


#ifdef NCBI_COMPILER_MSVC
#  define COMP_MSC
#endif

#ifdef NCBI_COMPILER_GCC
#  define COMP_GNU
#endif


/*----------------------------------------------------------------------*/
/*      Desired or available feature list                               */
/*----------------------------------------------------------------------*/

// C++ Toolkit have the same definitions
//#define HAVE_STRCASECMP 1
//#define HAVE_STRDUP 1


/*----------------------------------------------------------------------*/
/*      #includes                                                       */
/*----------------------------------------------------------------------*/

#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#  include <limits.h>
#  include <sys/stat.h>
#  include <stddef.h>
#  include <stdio.h>
#  include <ctype.h>
#  include <string.h>
//#  include <malloc.h>
#  include <memory.h>
#  include <stdlib.h>
#  include <math.h>
#  include <errno.h>
#  include <float.h>
#  include <unistd.h>
#endif

#if defined(NCBI_OS_MSWIN)
#  include <stddef.h>
#  include <sys/types.h>
#  include <limits.h>
#  include <sys/stat.h>
#  include <stdio.h>
#  include <ctype.h>
#  include <string.h>
#  include <stdlib.h>
#  include <math.h>
#  include <errno.h>
#  include <float.h>
#endif


/*----------------------------------------------------------------------*/
/*      Missing ANSI-isms                                               */
/*----------------------------------------------------------------------*/

#if 0

#ifndef offsetof
#define offsetof(__strctr,__fld) ((size_t)(&(((__strctr *)0)->__fld)))
#endif

#ifndef CLK_TCK
#  define CLK_TCK		60
#endif
#ifndef SEEK_SET
#  define SEEK_SET	0
#  define SEEK_CUR	1
#  define SEEK_END	2
#endif

#ifndef FILENAME_MAX
#  if defined(NCBI_OS_UNIX)
#    define FILENAME_MAX 1024
#  elif 
#    define FILENAME_MAX 63
#  endif
#endif
#ifndef PATH_MAX
#  if defined(NCBI_OS_UNIX)
#    define PATH_MAX 1024
#  elif 
#    define PATH_MAX 256
#  endif
#endif

#endif /* 0 */


/*----------------------------------------------------------------------*/
/*      Misc Macros                                                     */
/*----------------------------------------------------------------------*/

#define PROTO(x)	x
#define VPROTO(x)	x

#define INLINE inline

#if 0

#ifdef NCBI_OS_MSWIN
#  define DIRDELIMCHR	'\\'
#  define DIRDELIMSTR	"\\"
#else
#  define DIRDELIMCHR	'/'
#  define DIRDELIMSTR	"/"
#endif
#define CWDSTR	"."

#define KBYTE	(1024)
#define MBYTE	(1048576)

#define TEMPNAM_AVAIL
/*#define HAVE_MADVISE*/

#if defined(WORDS_BIGENDIAN)
#  define IS_BIG_ENDIAN
#elif
#  define IS_LITTLE_ENDIAN
#endif

#endif /* 0 */



/*----------------------------------------------------------------------*/
/*      Macros for Floating Point                                       */
/*----------------------------------------------------------------------*/

/*
#if defined(NCBI_OS_DARWIN) || defined(OS_UNIX_LINUX) || defined(NCBI_OS_MSWIN)
#  define EXP2(x)  exp((x)*LN2)
#  define LOG2(x)  (log(x)*(1./LN2))
#  define EXP10(x) exp((x)*LN10)
#  define LOG10(x) log10(x)
#endif

#if defined(???)
#  define EXP2(x)  exp2(x)
#  define LOG2(x)  log2(x)
#  define EXP10(x) exp10(x)
#  define LOG10(x) log10(x)
#endif
*/


/*----------------------------------------------------------------------*/
/*      Macros Defining Limits                                          */
/*----------------------------------------------------------------------*/

/* Largest permissible memory request */
/*
#ifdef NCBI_OS_MSWIN
#  define MAXALLOC  0x7F000000
#else
#  define MAXALLOC	0x40000000
#endif
*/



#endif  /* _NCBILCL_ */


/* @} */
