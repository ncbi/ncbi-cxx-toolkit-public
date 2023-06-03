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
 * Authors:  Anton Lavrentiev
 *           Leonid Boitsov   (random and regression tests)
 *
 * File Description:
 *   Test suite for "ncbi_crypt.[ch]"
 *
 */

#include <ncbiconf.h>
#include "../../ncbi_ansi_ext.h"
#include "../../ncbi_priv.h"
#include <connect/ext/ncbi_crypt.h>
#ifdef HAVE_LIBCONNEXT2
#  include <signon3/id.h>
#  define KEY  WEBENV_CRYPTKEY
#else
/* Note that in the key it is allowed to use spaces and any other
   special chars (e.g. alarm) but '\0' (which terminates the key) */
#  define KEY  "!\"#$%&'()*+,-./0123456789:;<=>?@" \
               "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`" \
               "abcdefghijklmnopqrstuvwxyz{|}~ \a"
#endif /*HAVE_LIBCONNEXT2*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* This header must go last */
#include <common/test_assert.h>

#define NTESTS  1000
#define NSEEDS  3
#define NKEYS   2


#define CORE_DATAF_ERRNO(level, error, data, size, fmt_args)    \
    DO_CORE_LOG_X(0, 0, level, g_CORE_Sprintf fmt_args, 1,      \
                  error, 0, data, size)


static int/*bool*/  s_CanChangeSeed = 0/*false*/;


static unsigned int s_Version(int version)
{
    return version < 0 ? 0 : (unsigned int) version;
}


typedef enum {
    eEncode = 0,
    eDecode
} EAction;

static void s_PrintStr(EAction action, const char* what,
                       const char* result, unsigned int seed)
{
    char x_seed[40];
    const char* x_action = action == eEncode ? "Encode" : "Decode";
    if (s_CanChangeSeed  &&  action == eEncode)
        sprintf(x_seed, " (seed %u)", seed);
    else
        *x_seed = '\0';
    if (!result  ||  !*result) {
        int/*bool*/ dynamic = 0/*false*/;
        const char* x_result = result ? "\"\"" :
            NcbiMessagePlusError(&dynamic, "<NULL>", errno, 0);
        CORE_LOGF(eLOG_Note,
                  ("%s%s%s%s%s: %s", x_action, x_seed,
                   what ? " \"" : "", what ? what : "", &"\""[!what],
                   x_result));
        if (dynamic)
            free((void*) x_result);
    } else {
        CORE_DATAF(eLOG_Note, result, strlen(result),
                   ("%s%s%s%s%s", x_action, x_seed,
                    what ? " \"" : "", what ? what : "", &"\""[!what]));
    }
}


static void s_PrintKey(CRYPT_Key k, const char* key, unsigned int seed)
{
    char header[80];

    if (!k)
        strcpy (header, "Key:     <NULL>");
    else if (k == CRYPT_BAD_KEY)
        strcpy (header, "Key:     <BAD>");
    else if (s_CanChangeSeed)
        sprintf(header, "Key:     Seed %u", seed);
    else
        *header = '\0';
    if (!*key) {
        if (*header)
            strcat(header, ", \"\"");
        else
            strcpy(header, "Key:     \"\"");
        key = 0;
    }
    CORE_DATA(eLOG_Note, key, key ? strlen(key) : 0, header);
}


/* NB: This call does not return */
#ifdef __GNUC__
__attribute__((noreturn))
#endif /*__GNUC__*/
static void s_ReportError(const char* where,   const char* original,
                          const char* encoded, const char* decoded,
                          const char* key, CRYPT_Key k, unsigned int seed,
                          int/*bool*/ print)
{
    int error = errno;
    if (print) {
        s_PrintStr(eEncode, 0, encoded, seed);
        s_PrintStr(eDecode, 0, decoded, 0/*irrelevant*/);
    }
    s_PrintKey(k, key, seed);
    if (!original  ||  !*original) {
        CORE_LOGF_ERRNO(eLOG_Fatal, error,
                        ("%s: Test failed for %s", where,
                         original ? "\"\"" : "<NULL>"));
    } else {
        CORE_DATAF_ERRNO(eLOG_Fatal, error, original, strlen(original),
                         ("%s: Test failed", where));
    }
    /*NOTREACHED*/
    abort();
}


/*
 * Perform encode-decode cycle and compare the result with the original.
 * Should it fail, print all the info necessary to reproduce the test.
 */
static void s_CheckEncodeDecode(const char* where, const char* key,
                                const char *str,   const char* arg,
                                CRYPT_Key   k)
{
    const char* encoded;
    const char* decoded;
    int/*bool*/ print;
    unsigned int seed;

    if (arg) {
        if (!*arg)
            arg = 0;
        print = 1/*true*/;
    } else
        print = 0/*false*/;
    if (k  &&  k != CRYPT_BAD_KEY  &&  s_CanChangeSeed) {
        /* HACK * HACK * HACK */
        seed = *((unsigned int*) k);
    } else
        seed = 0/*irrelevant*/;

    errno = 0;
    encoded = CRYPT_EncodeString(k, str);
    if (print) {
        s_PrintStr(eEncode, str, encoded, seed);
        if (encoded  &&  arg) {
            free((char*) encoded);
            encoded = arg;
        }
    }
    decoded = CRYPT_DecodeString(k, encoded);
    if (print)
        s_PrintStr(eDecode, encoded, decoded, 0/*irrelevant*/);

    if (!encoded  ||  !decoded  ||  (!arg  &&  strcmp(str, decoded) != 0)) {
        s_ReportError(where, arg  &&  encoded ? arg : str,
                      encoded, decoded, key, k, seed, !arg);
    }
    if (encoded != arg)
         free((char*) encoded);
    free((char*) decoded);
}


/*
 * Generate random string.  "buf" must have room for "len" + 1 characters.
 */
static void s_RandomStr(char buf[], int len,
                        unsigned char cmin,
                        unsigned char cmax)
{
    int i;

    assert(cmin <= cmax);

    if (cmin < cmax) {
        for (i = 0;  i < len;  ++i) {
            buf[i] = (char)
                (rand() % ((int) cmax - (int) cmin + 1) + (int) cmin);
        }
    } else
        memset(buf, cmin, (size_t) len);
    buf[len] = '\0';
}


/*
 * Check that lower version still can be decoded even if higher version is set.
 */
static void s_CompatibilityTest(int n, const char* key)
{
    char  buf[256];
    char* encoded;
    char* decoded;
    int   i, j, v = 0;

    assert(key);

    if (CRYPT_Version(-1) < 0  ||
        (CRYPT_Version(1) < 0  &&  CRYPT_Version(1) != 1)) {
        return;  /* unable to change version, nothing to check */
    }

    CORE_LOG(eLOG_Note, "CompatibilityTest test started");
    for (i = 0;  i < n;  ++i) {
        s_RandomStr(buf, rand() % (int) sizeof(buf), '\x01', '\x7F');
        for (j = 0;  j < NSEEDS;  ++j) {
            verify(CRYPT_Version(v = 0) >= 0);
            do {
                int w = v + 1;
                verify(CRYPT_Version(w) >= 0);
                if (CRYPT_Version(w) == w) {
                    CRYPT_Version(v);
                    errno = 0;
                    encoded = NcbiCrypt(buf, key);
                    do {
                        CRYPT_Version(w);
                        decoded = NcbiDecrypt(encoded, key);
                        if (!encoded  ||  !decoded  ||  strcmp(buf, decoded)) {
                            char message[80];
                            sprintf(message, "Compatibility test of"
                                    " version `%u' with version `%u'",
                                    s_Version(v), s_Version(w));
                            s_ReportError(message, buf, encoded, decoded,
                                          key, 0, 0, 1/*true*/);
                        }
                        free(decoded);
                        CRYPT_Version(++w);
                    } while (CRYPT_Version(w) == w);
                    free(encoded);
                }
                CRYPT_Version(++v);
            } while (CRYPT_Version(v) == v);
        }
    }
    CORE_LOGF(eLOG_Note, ("CompatibilityTest test completed for %u version%s",
                          v, &"s"[v == 1]));
}

/* 
 * Most of one-, two-, and three-character combinations,
 * with a few different seeds for each combination.
 */
static void s_GenericEncodeDecodeTriplesTest(const char* key, CRYPT_Key k)
{
    int  c1, c2, c3, i, m, v;
    char buf[10];

    assert(key);

    if ((v = CRYPT_Version(-1)) >= 0)
        verify(CRYPT_Version(v) >= 0);

    CORE_LOGF(eLOG_Note,
              ("GenericEncodeDecodeTriples test started for version `%u'",
               s_Version(v)));
    m = v == 0 ? 128 : 256;
    for (c1 = 0;  c1 <= 'Z' + 10;  c1 = c1 ? c1 + 1 : ' ') {
        for (c2 = 0;  c2 < m;  ++c2) {
            for (c3 = 0;  c3 < m;  ++c3) {
                for (i = 0;  i < NSEEDS;  ++i) {
                    buf[0] = (char)(c1 <= 'Z' ? c1 : rand() & (m - 1));
                    buf[1] = (char) c2;
                    buf[2] = (char) c3;
                    buf[3] = '\0';
                    s_CheckEncodeDecode("GenericEncodeDecodeTriples",
                                        key, buf, 0, k);
                }
                if (!c2)
                    break;
            }
            if (!c1)
                break;
        }
    }
    CORE_LOGF(eLOG_Note,
              ("GenericEncodeDecodeTriples test completed for version `%u'",
               s_Version(v)));
}


/* 
 * Encode random strings then decode them, checking whether the result
 * is the same as the original.
 */   
static void s_GenericEncodeDecodeRandomTest(int n,
                                            const char* key, CRYPT_Key k)
{
    char buf[256];
    int  i, j, v;

    assert(key);

    if ((v = CRYPT_Version(-1)) >= 0)
        verify(CRYPT_Version(v) >= 0);

    CORE_LOGF(eLOG_Note,
              ("GenericEncodeDecodeRandom test started for version `%u'",
               s_Version(v)));
    for (i = 0;  i < n;  ++i) {
        s_RandomStr(buf, rand() % (int) sizeof(buf),
                    '\x01', (unsigned char)(v == 0 ? '\x7F' : '\xFF'));
        for (j = 0;  j < NSEEDS;  ++j)
            s_CheckEncodeDecode("GenericEncodeDecodeRandom", key, buf, 0, k);
    }
    CORE_LOGF(eLOG_Note,
              ("GenericEncodeDecodeRandom test completed for version `%u'",
               s_Version(v)));
}


static void s_Usage(const char* prog)
{
    const char *s;
    if (!prog  ||  !*prog)
        prog = "test_ncbi_crypt";
    if (!(s = strrchr(prog, '/')))
        s = prog;
    else
        s++;

    fprintf(stderr,
            "Usage:\n"
            "%s [-{0|1}]"
            " [string_to_encode [{-|key} [string_to_decode [seed]]]]\n\n"
            "  -0 = Sets CRYPT version 0 (Only 7-bit clean)\n"
            "  -1 = Sets CRYPT version 1 (8-bit clean)\n"
            "       Default CRYPT version = %d\n\n"
            "Without command-line parameters, randomized unit tests start.\n\n"
            "Otherwise, \"string_to_encode\" is encoded using either\n"
            "some default key (-) or \"key\" as provided;  after that\n"
            "either the result of encode operation or \"string_to_decode\"\n"
            "(if provided) is decoded using the same key.\n"
            "Last parameter may specify random seed value as an integer.\n\n",
            s, CRYPT_Version(-1));
    exit(-1);
}


/* Usage:
 *    [-{0|1}] [string_to_encode [{key|-} [string_to_decode [seed]]]]
 *
 * Perform an automatic test if no command-line arguments provided.
 */

int main(int argc, char* argv[])
{
    TLOG_FormatFlags flags = fLOG_Short | fLOG_FullOctal | fLOG_OmitNoteLevel;
    int/*nool*/ seed = 0/*false*/;
    const char* key = KEY;
    const char* arg;

    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);
    CORE_SetLOGFormatFlags(fLOG_None | flags);

    if (CRYPT_Version(-1) != -1)
        s_CanChangeSeed = 1/*true*/;

    if (argc > 1) {
        int n = 1;
        if (strcmp(argv[1], "-0") == 0) {
            if (CRYPT_Version(0) == -1)
                CORE_LOG(eLOG_Fatal, "Crypt version 0 not supported");
            --argc;
            ++n;
        } else if (strcmp(argv[1], "-1") == 0) {
            if (CRYPT_Version(1) == -1)
                CORE_LOG(eLOG_Fatal, "Crypt version 1 not supported");
            --argc;
            ++n;
        }
        if (argc > 5  ||  (n == 1  &&  UTIL_HelpRequested(argc, argv))) {
            s_Usage(argv[0]);
            return -1;
        }
        if (argv[1] != argv[n])
            memmove(&argv[1], &argv[n], (size_t) argc * sizeof(argv[0]));
        arg = argv[1];
    } else
        arg = 0;
    if (argc > 2  &&  strcmp(argv[2], "-") != 0)
        key = argv[2];

    if (argc > 4) {
        seed = 1/*true*/;
        g_NCBI_ConnectRandomSeed = (unsigned int) atol(argv[4]);
    } else if (g_NCBI_ConnectRandomSeed == 0) {
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    }
    srand(g_NCBI_ConnectRandomSeed);

    if (!arg) {
        char buf[128];
        int  i, v;

        CORE_SetLOGFormatFlags(flags | fLOG_DateTime);
        CORE_LOGF(eLOG_Note, ("Random seed = %d", g_NCBI_ConnectRandomSeed));
        for (i = 0;  i < NKEYS;  ++i) {
            CRYPT_Key k;
            if (i) {
                CORE_LOGF(eLOG_Note, ("TEST %d:  Generating random key", i+1));
                s_RandomStr(buf, rand() % (int) sizeof(buf),
                            '\x01', (unsigned char)'\xFF');
            } else {
                CORE_LOGF(eLOG_Note, ("TEST %d:  Using static key", i+1));
                strncpy0(buf, KEY, sizeof(buf) - 1);
            }

            s_CompatibilityTest(NTESTS, buf);

            k = CRYPT_Init(buf);
            CRYPT_Version(v = 0);
            do {
                s_GenericEncodeDecodeTriplesTest(buf, k);
                s_GenericEncodeDecodeRandomTest(NTESTS, buf, k);
                CRYPT_Version(++v);
            } while (CRYPT_Version(v) == v);
            CRYPT_Free(k);
        }
        CORE_LOG(eLOG_Note, "TEST completed successfully");
    } else {
        CRYPT_Key k = CRYPT_Init(key);
        if (k  &&  k != CRYPT_BAD_KEY  &&  s_CanChangeSeed  &&  seed) {
            /* HACK * HACK * HACK */
            *((unsigned int*) k) = g_NCBI_ConnectRandomSeed;
        }
        s_CheckEncodeDecode("main", key, arg, argc > 3 ? argv[3] : "", k);
        CRYPT_Free(k);
        CORE_LOG(eLOG_Note, "Done.");
    }

    CORE_SetLOG(0);
    return 0;
}
