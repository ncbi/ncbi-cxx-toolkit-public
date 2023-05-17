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
 * Original author:     Alexandra Soboleva (public algorithm)
 * Toolkit adaptation:  Anton Lavrentiev
 *
 * File Description:
 *   NCBI crypting module.
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <connect/ext/ncbi_crypt.h>
#ifdef HAVE_LIBNCBICRYPT
#  include <ncbicrypt.h>
#endif /*HAVE_LIBNCBICRYPT*/
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NCBI_USE_ERRCODE_X   Connect_Crypt


/*--------------------------------------------------------------*/
/*ARGSUSED*/
int CRYPT_Version(int version)
{
#ifdef HAVE_LIBNCBICRYPT
    return NCBICRYPT_Version(version);
#else
    return -1;
#endif /*HAVE_LIBNCBICRYPT*/
}


/*--------------------------------------------------------------*/
CRYPT_Key CRYPT_Init(const char* key)
{
#ifdef HAVE_LIBNCBICRYPT
    assert((void*) NCBICRYPT_BAD_KEY == (void*) CRYPT_BAD_KEY);
    return (CRYPT_Key) NCBICRYPT_Init(key);
#else
    CRYPT_Key res;
    return !key  ||  !*key
        ? 0 : !(res = (CRYPT_Key) strdup(key)) ? CRYPT_BAD_KEY : res;
#endif /*HAVE_LIBNCBICRYPT*/
}


/*--------------------------------------------------------------*/
void CRYPT_Free(CRYPT_Key key)
{
    if (key  &&  key != CRYPT_BAD_KEY) {
#ifdef HAVE_LIBNCBICRYPT
        int res = NCBICRYPT_Free((NCBICRYPT_Key) key);
        if (res != 0)
            CORE_LOG_X(1, eLOG_Critical, "[CRYPT_Free]  Corrupt key");
#else
        free((void*) key);
#endif /*HAVE_LIBNCBICRYPT*/
    }
}


/*--------------------------------------------------------------*/
extern char* CRYPT_EncodeString(CRYPT_Key key, const char* str)
{
    char* res;
    if (key == CRYPT_BAD_KEY) {
        CORE_LOG_X(2, eLOG_Error, "[CRYPT_Encode]  Bad key");
        errno = EFAULT;
        return 0;
    }
#ifdef HAVE_LIBNCBICRYPT
    if (!(res = NCBICRYPT_EncodeString((NCBICRYPT_Key) key, str))) {
        int error = errno;
        if (error == EINVAL) {
            CORE_LOG_X(3, eLOG_Critical, "[CRYPT_Encode]  Corrupt key");
            errno = error;
        }
    }
#else
    res = NcbiCrypt(str, (const char*) key);
#endif /*HAVE_LIBNCBICRYPT*/
    return res;
}


/*--------------------------------------------------------------*/
extern char* CRYPT_DecodeString(CRYPT_Key key, const char* str)
{
    char* res;
    if (key == CRYPT_BAD_KEY) {
        CORE_LOG_X(4, eLOG_Error, "[CRYPT_Decode]  Bad key");
        errno = EFAULT;
        return 0;
    }
#ifdef HAVE_LIBNCBICRYPT
    if (!(res = NCBICRYPT_DecodeString((NCBICRYPT_Key) key, str))) {
        int error = errno;
        switch (error) {
        case EINVAL:
            CORE_LOG_X(5, eLOG_Critical, "[CRYPT_Decode]  Corrupt key");
            break;
        case ERANGE:
            CORE_LOGF(eLOG_Trace,
                      ("[CRYPT_Decode]  Unknown ciphertext version `%c'",
                       isalnum((unsigned char)(*str)) ? *str : '?'));
            break;
        case EDOM:
            CORE_LOG(eLOG_Trace, "[CRYPT_Decode]  Bad ciphertext");
            break;
        default:
            /* some other error */
            return 0;
        }
        errno = error;
    }
#else
    res = NcbiDecrypt(str, (const char*) key);
#endif /*HAVE_LIBNCBICRYPT*/
    return res;
}


#ifdef HAVE_LIBNCBICRYPT


/*--------------------------------------------------------------*/
extern char* NcbiCrypt(const char* str, const char* key)
{
    NCBICRYPT_Key tmp = NCBICRYPT_Init(key);
    char* res
        = tmp == NCBICRYPT_BAD_KEY ? 0 : NCBICRYPT_EncodeString(tmp, str);
    NCBICRYPT_Free(tmp);
    return res;
}


/*--------------------------------------------------------------*/
extern char* NcbiDecrypt(const char* str, const char* key)
{
    NCBICRYPT_Key tmp = NCBICRYPT_Init(key);
    char* res
        = tmp == NCBICRYPT_BAD_KEY ? 0 : NCBICRYPT_DecodeString(tmp, str);
    NCBICRYPT_Free(tmp);
    return res;
}


#else


/*--------------------------------------------------------------*/
extern char* NcbiCrypt(const char* s, const char* k)
{
    static const char kHex[] = "0123456789ABCDEF";
    size_t slen, klen, i, j;
    char *d, *pd;

    if (!s)
        return 0;
    if (!k  ||  !*k)
        return strdup(s);
    slen = strlen(s);
    if (!(d = (char*) malloc((slen << 1) + 2)))
        return 0;
    pd = d;
    *pd++ = 'H';
    klen = strlen(k);
    for (i = 0, j = 0;  i < slen;  ++i, ++j) {
        unsigned char c;
        if (j == klen)
            j  = 0;
        c = *s++ ^ k[j];
        *pd++ = kHex[c >> 4];
        *pd++ = kHex[c & 0x0F];
    }
    *pd = '\0';
    assert(((size_t)(pd - d)) & 1);
    return d;
}


/*--------------------------------------------------------------*/
static unsigned char s_FromHex(char c)
{
    unsigned char b = c - '0';
    if (b < 10)
        return b;
    b = (c | ' ') - 'a';
    return b < 6 ? b + 10 : '\xFF';
}


extern char* NcbiDecrypt(const char* s, const char* k)
{
    size_t slen, klen, i, j;
    char *d, *pd;

    if (!s)
        return 0;
    if (!k  ||  !*k)
        return strdup(s);
    slen = strlen(s);
    if (!(slen-- & 1)) {
        CORE_LOG(eLOG_Trace, "[CRYPT_Decode]  Bad ciphertext");
        errno = EDOM;
        return 0;
    }
    if (*s != 'H') {
        CORE_LOGF(eLOG_Trace,
                  ("[CRYPT_Decode]  Unknown ciphertext version `%c'",
                   isalnum((unsigned char)(*s)) ? *s : '?'));
        errno = ERANGE;
        return 0;
    }
    slen >>= 1;
    if (!(d = (char*) malloc(slen + 1)))
        return 0;
    pd = d;
    klen = strlen(k);
    for (i = 0, j = 0;  i < slen;  ++i, ++j) {
        unsigned char hi, lo;
        if (((hi = s_FromHex(*++s)) & 0xF0)  ||
            ((lo = s_FromHex(*++s)) & 0xF0)) {
            CORE_LOG(eLOG_Trace, "[CRYPT_Decode]  Corrupt ciphertext");
            break;
        }
        if (j == klen)
            j  = 0;
        *pd++ = ((hi << 4) | lo) ^ k[j];
    }
    if (i < slen/*user error*/) {
        free(d), d = 0;
        errno = EDOM;
    } else {
        *pd = '\0';
        assert((size_t)(pd - d) == slen);
    }
    return d;
}


#endif /*HAVE_LIBNCBICRYPT*/
