#ifndef NCBI_ANSI_EXT__H
#define NCBI_ANSI_EXT__H

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
 *    Non-ANSI, yet widely used functions
 *
 * --------------------------------------------------------------------------
 * $Log$
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

#include <ncbiconf.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_STRDUP
#  define strdup      NCBI_strdup

/* Create a copy of string "str".
 * Return an identical malloc'ed string, which must be explicitly freed 
 * by free() when no longer needed.
 */
char *strdup(const char* str);
#endif /* HAVE_STRDUP */


#ifndef HAVE_STRCASECMP
#  define strcasecmp  NCBI_strcasecmp
#  define strncasecmp NCBI_strncasecmp

/* Compare "s1" and "s2", ignoring case.
 * Return less than, equal to or greater than zero if
 * "s1" is lexicographically less than, equal to or greater than "s2".
 */
int strcasecmp(const char* s1, const char* s2);

/* Compare not more than "n" characters of "s1" and "s2", ignoring case.
 * Return less than, equal to or greater than zero if
 * "s1" is lexicographically less than, equal to or greater than "s2".
 */
int strncasecmp(const char* s1, const char* s2, size_t n);
#endif /* HAVE_STRCASECMP */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NCBI_ANSI_EXT__H */
