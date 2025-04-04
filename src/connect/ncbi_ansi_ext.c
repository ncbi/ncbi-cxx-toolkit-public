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
 *   Non-ANSI, yet widely used functions
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#ifndef HAVE_STRNLEN
size_t strnlen(const char* str, size_t maxlen)
{
    const char* end = (const char*) memchr(str, '\0', maxlen);
    return end ? (size_t)(end - str) : maxlen;
}
#endif /*HAVE_STRNLEN*/


#if !defined(HAVE_STRDUP)  &&  !defined(NCBI_COMPILER_MSVC)
char* strdup(const char* str)
{
    size_t size = strlen(str) + 1;
    char*   res = (char*) malloc(size);
    return res ? (char*) memcpy(res, str, size) : 0;
}
#endif /*!HAVE_STRDUP && !NCBI_COMPILER_MSVC*/


#ifndef HAVE_STRNDUP
char* strndup(const char* str, size_t n)
{
    size_t size = strnlen(str, n);
    char*   res = (char*) malloc(size + 1);
    if (res) {
        memcpy(res, str, size);
        res[size] = '\0';
    }
    return res;
}
#endif /*!HAVE_STRNDUP*/


#if !defined(NCBI_COMPILER_MSVC)  &&  !defined(HAVE_STRCASECMP)

/* We assume that we're using ASCII-based charsets */
int strcasecmp(const char* s1, const char* s2)
{
    const unsigned char* p1 = (const unsigned char*) s1;
    const unsigned char* p2 = (const unsigned char*) s2;
    unsigned char c1, c2;

    if (p1 == p2)
        return 0;

    do {
        c1 = *p1++;
        c2 = *p2++;
        c1 = 'A' <= c1  &&  c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = 'A' <= c2  &&  c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
    } while (c1  &&  c1 == c2);

    return c1 - c2;
}


int strncasecmp(const char* s1, const char* s2, size_t n)
{
    const unsigned char* p1 = (const unsigned char*) s1;
    const unsigned char* p2 = (const unsigned char*) s2;
    unsigned char c1, c2;

    if (p1 == p2  ||  n == 0)
        return 0;

    do {
        c1 = *p1++;
        c2 = *p2++;
        c1 = 'A' <= c1  &&  c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = 'A' <= c2  &&  c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
    } while (--n > 0  &&  c1  &&  c1 == c2);

    return c1 - c2;
}

#endif /*!NCBI_COMPILER_MSVC && !HAVE_STRCASECMP*/


char* strupr(char* s)
{
    unsigned char* t;
    for (t = (unsigned char*) s;  *t;  ++t)
        *t = (unsigned char) toupper(*t);
    return s;
}


char* strlwr(char* s)
{
    unsigned char* t;
    for (t = (unsigned char*) s;  *t;  ++t)
        *t = (unsigned char) tolower(*t);
    return s;
}


char* strncpy0(char* s1, const char* s2, size_t n)
{
    *s1 = '\0';
    return strncat(s1, s2, n);
}


#ifndef HAVE_MEMCCHR
void* memcchr(const void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*) s;
    size_t i;
    for (i = 0;  i < n;  ++i) {
        if (*p != (unsigned char) c)
            return p;
        ++p;
    }
    return 0;
}
#endif /*!HAVE_MEMCCHR*/


#ifndef HAVE_MEMRCHR
/* suboptimal but working implementation */
void* memrchr(const void* s, int c, size_t n)
{
    unsigned char* e = (unsigned char*) s + n;
    size_t i;
    for (i = 0;  i < n;  ++i) {
        if (*--e == (unsigned char) c)
            return e;
    }
    return 0;
}
#endif /*!HAVE_MEMRCHR*/


static const double x_pow10[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7 };


char* NCBI_simple_ftoa(char* s, double f, int p)
{
    double v, w;
    long   x, y;
    if (p < 0)
        p = 0;
    else {
        if (p >= (int)(sizeof(x_pow10)/sizeof(x_pow10[0])))
            p  = (int)(sizeof(x_pow10)/sizeof(x_pow10[0]))-1;
    }
    w = x_pow10[p];
    v = f < 0.0 ? -f : f;
    x = (long)(v + 0.5 / w);
    y = (long)(w * (v - (double) x) + 0.5);
    assert(p  ||  !y);  /* with precision 0, output of ".0" is empty */
    return s + sprintf(s, &"-%ld%s%.*lu"[!(f < 0.0)], x, &"."[!p], p, y);
}


double NCBI_simple_atof(const char* s, char** t)
{
    int/*bool*/ n;
    char*       e;
    long        x;

    if (t)
        *t = (char*) s;
    while (isspace((unsigned char)(*s)))
        s++;
    if ((*s == '-'  ||  *s == '+')
        &&  (s[1] == '.'  ||  isdigit((unsigned char) s[1]))) {
        n = *s++ == '-' ? 1/*true*/ : 0/*false*/;
    } else
        n = 0/*false*/;

    errno = 0;
    x = strtol(s, &e, 10);
    if (*e == '.') {
        if (isdigit((unsigned char) e[1])) {
            int error = errno;
            unsigned long y;
            double w;
            int p;
            s = ++e;
            errno = 0/*NB: maybe EINVAL for ".NNN"*/;
            y = strtoul(s, &e, 10);
            assert(e > s);
            p = (int)(e - s);
            if (p >= (int)(sizeof(x_pow10)/sizeof(x_pow10[0]))) {
                w  = 10.0;
                do {
                    w *= x_pow10[(int)(sizeof(x_pow10)/sizeof(x_pow10[0]))-1];
                    p -=         (int)(sizeof(x_pow10)/sizeof(x_pow10[0]))-1;
                } while (p >=    (int)(sizeof(x_pow10)/sizeof(x_pow10[0])));
                if (errno == ERANGE)
                    errno  = error == EINVAL ? 0 : error;
                w *= x_pow10[p];
            } else
                w  = x_pow10[p];
            if (t)
                *t = e;
            return n ? (double)(-x) - (double) y / w : 
                       (double)  x  + (double) y / w;
        } else if (t  &&  e > s)
            *t = ++e;
    } else if (t  &&  e > s)
        *t = e;
    return (double)(n ? -x : x);
}


#if 1

int/*bool*/ NCBI_HasSpaces(const char* s, size_t n)
{
    while (n--) {
        if (isspace((unsigned char) s[n]))
            return 1/*true*/;
    }
    return 0/*false*/;
}

#else

#ifdef __GNUC__
inline
#endif /*__GNUC__*/
/* Takes a <ctype.h>-like classification _function_ */
static int/*bool*/ x_HasClass(const char* s, size_t n, int (*isa)(int))
{
    while (n--) {
        if (isa((unsigned char) s[n]))
            return 1/*true*/;
    }
    return 0/*false*/;
}


/* undef to use a function variant */
#ifdef   isspace
#  undef isspace
#endif /*isspace*/
int/*bool*/ NCBI_HasSpaces(const char* s, size_t n)
{
    return x_HasClass(s, n, isspace);
}

#endif
