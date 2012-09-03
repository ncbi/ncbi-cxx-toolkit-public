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


#ifndef HAVE_STRDUP

char* strdup(const char* str)
{
    size_t size = strlen(str) + 1;
    char*   res = (char*) malloc(size);
    if (res)
        memcpy(res, str, size);
    return res;
}

#endif /*HAVE_STRDUP*/


#ifndef HAVE_STRNDUP

char* strndup(const char* str, size_t n)
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
        c1 = c1 >= 'A' && c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = c2 >= 'A' && c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
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
        c1 = c1 >= 'A' && c1 <= 'Z' ? c1 + ('a' - 'A') : tolower(c1);
        c2 = c2 >= 'A' && c2 <= 'Z' ? c2 + ('a' - 'A') : tolower(c2);
    } while (--n > 0  &&  c1  &&  c1 == c2);

    return c1 - c2;
}

#endif /*HAVE_STRCASECMP*/


char* strupr(char* s)
{
    unsigned char* t = (unsigned char*) s;

    while ( *t ) {
        *t = toupper(*t);
        t++;
    }
    return s;
}


char* strlwr(char* s)
{
    unsigned char* t = (unsigned char*) s;

    while ( *t ) {
        *t = tolower(*t);
        t++;
    }
    return s;
}


char* strncpy0(char* s1, const char* s2, size_t n)
{
    *s1 = '\0';
    return strncat(s1, s2, n);
}


#ifndef HAVE_MEMRCHR
/* suboptimal but working implementation */
void* memrchr(const void* s, int c, size_t n)
{
    unsigned char* e = (unsigned char*) s + n;
    size_t i;
    for (i = 0;  i < n;  i++) {
        if (*--e == (unsigned char) c)
            return e;
    }
    return 0;
}
#endif/*!HAVE_MEMRCHR*/


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
    v = (f < 0.0 ? -f : f) + 0.5 / w;
    x = (long)  v;
    y = (long)((v - x) * w);
    return s + sprintf(s, "-%ld.%0*lu" + !(f < 0.0), x, p, y);
}


double NCBI_simple_atof(const char* s, char** e)
{
    long x;
    errno = 0;
    x = strtol(s, e, 10);
    if (**e == '.') {
        if (isdigit((unsigned char)((*e)[1]))) {
            double w;
            long   y;
            int    p;
            errno = 0/*can be EINVAL here for ".NNN"*/;
            y = strtoul(s = ++(*e), e, 10);
            assert(*e > s);
            p = (int)(*e - s);
            if (p >= (int)(sizeof(x_pow10)/sizeof(x_pow10[0]))) {
                w  = 10.0;
                do {
                    w *= x_pow10[(int)(sizeof(x_pow10)/sizeof(x_pow10[0]))-1];
                    p -=         (int)(sizeof(x_pow10)/sizeof(x_pow10[0]))-1;
                } while (p >=    (int)(sizeof(x_pow10)/sizeof(x_pow10[0])));
                if (errno == ERANGE)
                    errno  = 0;
                w *= x_pow10[p];
            } else
                w  = x_pow10[p];
            return x < 0 ? x - y / w : x + y / w;
        } else if (*e > s)
            ++(*e);
    }
    return x;
}
