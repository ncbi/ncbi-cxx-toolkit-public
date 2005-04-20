#ifndef CONNECT___NCBI_ANSI_EXT__H
#define CONNECT___NCBI_ANSI_EXT__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Non-ANSI, yet widely used functions
 *
 */
#include <connect/connect_export.h>
#include "ncbi_config.h"
#include <stddef.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_STRDUP

#  ifdef strdup
#    undef strdup
#  endif
#  define strdup      NCBI_strdup

/* Create a copy of string "str".
 * Return an identical malloc'ed string, which must be explicitly freed 
 * by free() when no longer needed.
 */
extern NCBI_XCONNECT_EXPORT char* strdup(const char* str);

#endif /*HAVE_STRDUP*/


#ifndef HAVE_STRCASECMP

#  ifdef strcasecmp
#    undef strcasecmp
#    undef strncasecmp
#  endif
#  define strcasecmp  NCBI_strcasecmp
#  define strncasecmp NCBI_strncasecmp

/* Compare "s1" and "s2", ignoring case.
 * Return less than, equal to or greater than zero if
 * "s1" is lexicographically less than, equal to or greater than "s2".
 */
extern NCBI_XCONNECT_EXPORT int strcasecmp(const char* s1, const char* s2);

/* Compare not more than "n" characters of "s1" and "s2", ignoring case.
 * Return less than, equal to or greater than zero if
 * "s1" is lexicographically less than, equal to or greater than "s2".
 */
extern NCBI_XCONNECT_EXPORT int strncasecmp(const char* s1, const char* s2, size_t n);

#endif/*HAVE_STRCASECMP*/


#ifdef strupr
#  undef strupr
#  undef strlwr
#endif
#define strupr NCBI_strupr
#define strlwr NCBI_strlwr

/* Convert a string to uppercase, then return pointer to
 * the altered string. Because the conversion is made in place, the
 * returned pointer is the same as the passed one.
 */
extern NCBI_XCONNECT_EXPORT char* strupr(char* s);

/* Convert a string to lowercase, then return pointer to
 * the altered string. Because the conversion is made in place, the
 * returned pointer is the same as the passed one.
 */
extern NCBI_XCONNECT_EXPORT char* strlwr(char* s);


/* Copy not more than "n" characters from string "s2" into "s1"
 * and return the result, which is always null-terminated.
 * NOTE: The difference of this function from standard strncpy() is in
 * that the result is always null-terminated and that the function
 * does not pad "s1" with null bytes should "s2" be shorter than "n".
 */
extern NCBI_XCONNECT_EXPORT char* strncpy0(char* s1, const char* s2, size_t n);


#ifdef __cplusplus
} /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.13  2005/04/20 18:12:16  lavr
 * strdup() has got NCBI_XCONNECT_EXPORT
 *
 * Revision 6.12  2004/03/12 23:25:37  gorelenk
 * Added export prefixes.
 *
 * Revision 6.11  2002/10/28 15:41:25  lavr
 * Header made private (moved from include/connect to here)
 *
 * Revision 6.10  2002/09/24 15:01:17  lavr
 * File description indented uniformly
 *
 * Revision 6.9  2002/09/19 17:59:40  lavr
 * Header file guard macro changed; log moved to the end
 *
 * Revision 6.8  2000/11/07 21:19:35  vakatov
 * Compilation warning fixed;  plus, some code beautification...
 *
 * Revision 6.7  2000/10/18 21:15:19  lavr
 * strupr and strlwr added
 *
 * Revision 6.6  2000/10/06 16:39:22  lavr
 * <string.h> included and #defines now take care of functions declared
 * through macros (needed on Linux to prevent macro redefinitions)
 *
 * Revision 6.5  2000/10/05 21:26:07  lavr
 * ncbiconf.h removed
 *
 * Revision 6.4  2000/05/22 16:53:36  lavr
 * Minor change
 *
 * Revision 6.3  2000/05/17 18:51:28  vakatov
 * no HAVE_SIGACTION
 *
 * Revision 6.2  2000/05/17 16:09:57  lavr
 * Define prototypes of functions, usually defined but hidden in ANSI mode
 *
 * Revision 6.1  2000/05/15 19:03:07  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_ANSI_EXT__H */
