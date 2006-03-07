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

#include "ncbi_ansi_ext.h"
#include <ctype.h>
#include <stdlib.h>


#ifndef HAVE_STRDUP

extern char* strdup(const char* str)
{
    size_t size = strlen(str) + 1;
    char*   res = (char*) malloc(size);
    if (res)
        memcpy(res, str, size);
    return res;
}

#endif /*HAVE_STRDUP*/


#ifndef HAVE_STRNDUP

extern char* strndup(const char* str, size_t n)
{
    const char* end = n   ? memchr(str, '\0', n) : 0;
    size_t     size = end ? (size_t)(end - str)  : n;
    char*       res = (char*) malloc(size + 1);
    if (res) {
        memcpy(res, str, size);
        res[size] = '\0';
    }
    return res;
}

#endif /*HAVE_STRNDUP*/


#ifndef HAVE_STRCASECMP

/* We assume that we're using ASCII-based charsets */
extern int strcasecmp(const char* s1, const char* s2)
{
    const unsigned char* p1 = (const unsigned char*) s1;
    const unsigned char* p2 = (const unsigned char*) s2;
    unsigned char c1, c2;

    if (p1 == p2)
        return 0;

    do {
        c1 = *p1++;
        c2 = *p2++;
        c1 = c1 >= 'A' && c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = c2 >= 'A' && c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
    } while (c1  &&  c1 == c2);

    return c1 - c2;
}


extern int strncasecmp(const char* s1, const char* s2, size_t n)
{
    const unsigned char* p1 = (const unsigned char*) s1;
    const unsigned char* p2 = (const unsigned char*) s2;
    unsigned char c1, c2;

    if (p1 == p2  ||  n == 0)
        return 0;

    do {
        c1 = *p1++;
        c2 = *p2++;
        c1 = c1 >= 'A' && c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = c2 >= 'A' && c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
    } while (--n > 0  &&  c1  &&  c1 == c2);

    return c1 - c2;
}

#endif /*HAVE_STRCASECMP*/


extern char* strupr(char* s)
{
    unsigned char* t = (unsigned char*) s;

    while ( *t ) {
        *t = toupper(*t);
        t++;
    }
    return s;
}


extern char* strlwr(char* s)
{
    unsigned char* t = (unsigned char*) s;

    while ( *t ) {
        *t = tolower(*t);
        t++;
    }
    return s;
}


extern char* strncpy0(char* s1, const char* s2, size_t n)
{
    *s1 = '\0';
    return strncat(s1, s2, n);
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.19  2006/03/07 18:15:14  lavr
 * Optimize strndup()
 *
 * Revision 6.18  2006/03/07 17:54:44  ivanov
 * Fixed compilation error in strndup
 *
 * Revision 6.17  2006/03/07 17:18:51  lavr
 * +strndup
 *
 * Revision 6.16  2004/10/08 16:16:24  ivanov
 * Removed extra ")"
 *
 * Revision 6.15  2004/10/08 15:45:19  lavr
 * Make lower-case comparisons in no-case functions
 *
 * Revision 6.14  2002/10/29 22:18:29  lavr
 * Comply with man strdup(2) in the implementation of strdup() here
 *
 * Revision 6.13  2002/10/28 18:55:26  lavr
 * Fix change log to remove duplicate log entry for R6.12
 *
 * Revision 6.12  2002/10/28 18:52:07  lavr
 * Conditionalize definitions of strdup() and str[n]casecmp()
 *
 * Revision 6.11  2002/10/28 15:41:56  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.10  2002/09/24 15:05:45  lavr
 * Log moved to end
 *
 * Revision 6.9  2002/03/19 22:12:28  lavr
 * strcasecmp and strncasecmp are optimized (for ASCII range)
 *
 * Revision 6.8  2001/12/04 15:57:22  lavr
 * Tiny style adjustement
 *
 * Revision 6.7  2000/12/28 21:27:52  lavr
 * ANSI C++ compliant use of malloc (explicit casting of result)
 *
 * Revision 6.6  2000/11/07 21:45:16  lavr
 * Removed isupper/islower checking in strlwr/strupr
 *
 * Revision 6.5  2000/11/07 21:19:38  vakatov
 * Compilation warning fixed;  plus, some code beautification...
 *
 * Revision 6.4  2000/10/18 21:15:53  lavr
 * strupr and strlwr added
 *
 * Revision 6.3  2000/10/06 16:40:23  lavr
 * <string.h> included now in <connect/ncbi_ansi_ext.h>
 * conditional preprocessor statements removed
 *
 * Revision 6.2  2000/05/17 16:11:02  lavr
 * Reorganized for use of HAVE_* defines
 *
 * Revision 6.1  2000/05/15 19:03:41  lavr
 * Initial revision
 *
 * ==========================================================================
 */
