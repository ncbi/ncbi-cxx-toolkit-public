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

#include <connect/ncbi_ansi_ext.h>
#include <ctype.h>
#include <stdlib.h>


extern char* strdup(const char* str)
{
    size_t size = strlen(str) + 1;
    return (char *)memcpy(malloc(size), str, size);
}


extern int strcasecmp(const char* s1, const char* s2)
{
    const unsigned char* p1 = (const unsigned char*) s1;
    const unsigned char* p2 = (const unsigned char*) s2;
    unsigned char c1, c2;

    if (p1 == p2)
        return 0;

    do {
        c1 = toupper(*p1++);
        c2 = toupper(*p2++);
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
        c1 = toupper(*p1++);
        c2 = toupper(*p2++);
    } while (--n > 0  &&  c1  &&  c1 == c2);

    return c1 - c2;
}


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
