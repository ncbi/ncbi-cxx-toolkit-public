#ifndef CONNECT___NCBI_ANSI_EXT__H
#define CONNECT___NCBI_ANSI_EXT__H

/* $Id$
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
///@file ncbi_ansi_ext.h
 *   Non-ANSI, yet widely used string/memory functions
 *
 */

#include <connect/connect_export.h>
#include "ncbi_config.h"
#include <stddef.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_STRNLEN

#  ifdef   strnlen
#    undef strnlen
#  endif
#  define  strnlen     NCBI_strnlen

/** Return the number of characters in the string pointed to by "str" (not
 *  including the terminating '\0' character but no more than "maxlen" (if the
 *  '\0' character hasn't been found within the first "maxlen" characters).
 */
NCBI_XCONNECT_EXPORT
size_t     strnlen(const char* str, size_t maxlen);

#endif /*HAVE_STRNLEN*/ 


#ifndef HAVE_STRDUP

#  ifdef   strdup
#    undef strdup
#  endif
#  define  strdup       NCBI_strdup

/** Create a copy of string "str".  Return an identical malloc()'ed string,
 *  which must be explicitly deallocated by free() when no longer needed.
 *  Return NULL if the memory allocation failed.
 */
NCBI_XCONNECT_EXPORT
char*      strdup(const char* str);

#elif defined(NCBI_COMPILER_MSVC)
#  define  strdup       _strdup
#endif /*HAVE_STRDUP*/


#ifndef HAVE_STRNDUP

#  ifdef   strndup
#    undef strndup
#  endif
#  define  strndup      NCBI_strndup

/** Create a copy of up to "n" first characters of string "str".  Return a
 *  malloc()'ed and '\0'-terminated string, which must be explicitly
 *  deallocated by free() when no longer needed.  Return NULL if the memory
 *  allocation failed.
 *  @note "str" is ignored (and, thus, may be NULL) when "n" is passed as 0,
 *  which results in creating an empty string ("") upon return.
 */
NCBI_XCONNECT_EXPORT
char*      strndup(const char* str, size_t n);

#endif /*HAVE_STRNDUP*/


#ifndef HAVE_STRCASECMP

#  ifdef   strcasecmp
#    undef strcasecmp
#    undef strncasecmp
#  endif
#  define  strcasecmp   NCBI_strcasecmp
#  define  strncasecmp  NCBI_strncasecmp

/** Compare "s1" and "s2", ignoring case.  Return less than, equal to or
 *  greater than zero if "s1" is lexicographically less than, equal to or
 *  greater than "s2", respectively.
 */
NCBI_XCONNECT_EXPORT
int        strcasecmp(const char* s1, const char* s2);

/** Compare no more than "n" characters of "s1" and "s2", ignoring case.
 *  Return less than, equal to or greater than zero if the initial fragment of
 *  "s1" is lexicographically less than, equal to or greater than the same
 *  fragment of "s2", respectively.
 */
NCBI_XCONNECT_EXPORT
int        strncasecmp(const char* s1, const char* s2, size_t n);

#endif /*HAVE_STRCASECMP*/


#ifdef   strupr
#  undef strupr
#  undef strlwr
#endif
#define  strupr         NCBI_strupr
#define  strlwr         NCBI_strlwr

/** Convert a string to all uppercase, then return pointer to the altered
 *  string.  Because the conversion is made in place, the returned pointer is
 *  the same as the passed one.
 */
char*    strupr(char* s);

/** Convert a string to all lowercase, then return pointer to the altered
 *  string.  Because the conversion is made in place, the returned pointer is
 *  the same as the passed one.
 */
char*    strlwr(char* s);


/** Copy not more than "n" characters from string "s2" into "s1", and return
 *  the result, which is always '\0'-terminated.
 *  NOTE:  The difference of this function from standard strncpy() is in that
 *  the result is _always_ '\0'-terminated and that the function does _not_ pad
 *  "s1" with null bytes should "s2" be shorter than "n" characters.
 */
NCBI_XCONNECT_EXPORT
char*    strncpy0(char* s1, const char* s2, size_t n);


#ifndef HAVE_MEMCCHR

#ifdef   memcchr
#  undef memcchr
#endif
#define  memcchr        NCBI_memcchr

/** Find the address of the first occurrence of a byte that is NOT char "c"
 *  within the "n" bytes of memory at the address "s".  Return NULL if all
 *  bytes are "c".
 */
void*    memcchr(const void* s, int c, size_t n);

#endif /*!HAVE_MEMCCHR*/


#ifndef HAVE_MEMRCHR

#ifdef   memrchr
#  undef memrchr
#endif
#define  memrchr        NCBI_memrchr

/** Find the address of the last occurrence of char "c" within "n" bytes of a
 *  memory block of size "n" beginning at the address "s".  Return NULL if no
 *  such byte is found.
 */
void*    memrchr(const void* s, int c, size_t n);

#endif /*!HAVE_MEMRCHR*/


/** Locale-independent double-to-ASCII conversion of value "f" into a character
 *  buffer pointed to by "s", with a specified precision (mantissa digits) "p".
 *  There is an internal limit on precision (so larger values of "p" will be
 *  silently truncated).  The maximal representable whole part corresponds to
 *  the maximal signed long integer.  Otherwise, the behavior is undefined.
 *  Return the pointer past the output string (points to the terminating '\0').
 */
NCBI_XCONNECT_EXPORT
char*    NCBI_simple_ftoa(char* s, double f, int p);


/** Locale-independent ASCII-to-double conversion of string "s".  Does not work
 *  for scientific notation (values including exponent).  Sets "e" to point to
 *  the character that stopped conversion.  Clears "errno" but sets it non-zero
 *  in case of conversion errors.  Maximal value for the whole part may not
 *  exceed the maximal signed long integer, and for mantissa -- unsigned long
 *  integer.
 *  Returns result of the conversion (on error sets errno, returns 0.0).
 *  @note e == s upon return if no valid input was found and consumed.
 */
NCBI_XCONNECT_EXPORT
double   NCBI_simple_atof(const char* s, char** e);


/** Return non-zero(true) if a block of memory based at "s" and of size "n" has
 *  any space characters (as defined by isspace(c) of <ctype.h>).  Return zero
 *  (false) if no such characters were found.  Note that "s" is not considered
 *  to be '\0'-terminated;  that is, all "n" positions will be checked.
 *  @note If "n" is 0, then "s" is not getting accessed (and can be anything,
 *  including NULL), and the return value is always 0(false).
 */
int/*bool*/ NCBI_HasSpaces(const char* s, size_t n);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONNECT___NCBI_ANSI_EXT__H */
