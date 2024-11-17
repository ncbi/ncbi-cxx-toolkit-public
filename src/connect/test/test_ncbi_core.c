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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Test suite for:
 *     ncbi_core.[ch]
 *     ncbi_util.[ch]
 *
 */

#include <connect/ncbi_base64.h>
#include <connect/ncbi_util.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_assert.h"
#include <stdlib.h>
#include <errno.h>

#include "test_assert.h"  /* This header must go last */

/* Aux. to printout a name of the next function to test
 */
#define DO_TEST(func)  do {                                             \
        printf("\n----- "#func"  ---------------------------------\n"); \
        func();                                                         \
    } while (0)


/* Some pre-declarations to avoid C++ compiler warnings
 */
#if defined(__cplusplus)
extern "C" {
    static int/*bool*/ TEST_CORE_LockHandler(void* data, EMT_Lock how);
    static void        TEST_CORE_LockCleanup(void* data);
    static void        TEST_CORE_LogHandler (void* data, const SLOG_Message* mess);
    static void        TEST_CORE_LogCleanup (void* data);
}
#endif /* __cplusplus */



/*****************************************************************************
 *  TEST_CORE -- test "ncbi_core.c"
 */

#define _STR(x)  #x
#define  STR(x)  _STR(x)
#define CHECK_IO_STATUS(x)                          \
    {                                               \
        const char* name = STR(x), *str, *ptr;      \
        size_t len;                                 \
        assert(memcmp(name, "eIO_", 4) == 0);       \
        assert((str = IO_StatusStr(x)) != 0);       \
        if (!(ptr = strchr(str, ' ')))              \
            len = strlen(str);                      \
        else                                        \
            len = (size_t)(ptr - str);              \
        assert(memcmp(str, name + 4, len) == 0);    \
    }

static void TEST_CORE_Io(void)
{
    /* EIO_Status, IO_StatusStr() */
    int x_status;
    for (x_status = 0;  x_status < EIO_N_STATUS;  ++x_status) {
        /* also checks uniqueness of values */
        switch ( (EIO_Status) x_status ) {
        case eIO_Success:
        case eIO_Timeout:
        case eIO_Interrupt:
        case eIO_InvalidArg:
        case eIO_NotSupported:
        case eIO_Unknown:
        case eIO_Closed:
            assert(IO_StatusStr((EIO_Status) x_status));
            printf("IO_StatusStr(status = %d): \"%s\"\n",
                   x_status, IO_StatusStr((EIO_Status) x_status));
            break;
        case eIO_Reserved:
            assert(!*IO_StatusStr((EIO_Status) x_status));
            break;
        default:
            assert(0);
        }
    }
    CHECK_IO_STATUS(eIO_Success);
    CHECK_IO_STATUS(eIO_Timeout); 
    CHECK_IO_STATUS(eIO_Interrupt);
    CHECK_IO_STATUS(eIO_InvalidArg);
    CHECK_IO_STATUS(eIO_NotSupported);
    CHECK_IO_STATUS(eIO_Unknown);
    CHECK_IO_STATUS(eIO_Closed);
}


static int TEST_CORE_LockData;

/* FMT_LOCK_Handler */
static int/*bool*/ TEST_CORE_LockHandler(void* data, EMT_Lock how)
{
    const char* str = 0;
    assert(data == &TEST_CORE_LockData);

    switch ( how ) {
    case eMT_Lock:
        str = "eMT_Lock";
        break;
    case eMT_LockRead:
        str = "eMT_LockRead";
        break;
    case eMT_Unlock:
        str = "eMT_Unlock";
        break;
    case eMT_TryLock:
        str = "eMT_TryLock";
        break;
    case eMT_TryLockRead:
        str = "eMT_TryLockRead";
        break;
    }
    assert(str);
    printf("TEST_CORE_LockHandler(%s)\n", str);
    return 1/*true*/;
}


/* FMT_LOCK_Cleanup */
static void TEST_CORE_LockCleanup(void* data)
{
    assert(data == &TEST_CORE_LockData);
    printf("TEST_CORE_LockCleanup()\n");
    TEST_CORE_LockData = 222;
}


static void TEST_CORE_Lock(void)
{
    /* MT_LOCK API */
    MT_LOCK x_lock;

    /* dummy */
    TEST_CORE_LockData = 111;
    x_lock = MT_LOCK_Create(&TEST_CORE_LockData, 0, TEST_CORE_LockCleanup);
    assert(x_lock);

    verify(MT_LOCK_AddRef(x_lock) == x_lock);
    verify(MT_LOCK_AddRef(x_lock) == x_lock);
    verify(MT_LOCK_Delete(x_lock) == x_lock);
    assert(TEST_CORE_LockData == 111);

    verify(MT_LOCK_Do(x_lock, eMT_LockRead) != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Lock)     != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)   != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)   != 0);

    verify(MT_LOCK_Delete(x_lock) == x_lock);
    assert(TEST_CORE_LockData == 111);
    verify(MT_LOCK_Delete(x_lock) == 0);
    assert(TEST_CORE_LockData == 222);

    /* real */
    x_lock = MT_LOCK_Create(&TEST_CORE_LockData,
                            TEST_CORE_LockHandler, TEST_CORE_LockCleanup);
    assert(x_lock);

    /* NB: Write after read is not usually an allowed lock nesting */
    verify(MT_LOCK_Do(x_lock, eMT_LockRead)    != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Lock)        != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);
    /* Read after write is usually okay */
    verify(MT_LOCK_Do(x_lock, eMT_Lock)        != 0);
    verify(MT_LOCK_Do(x_lock, eMT_LockRead)    != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);
    /* Try-locking sequence */
    verify(MT_LOCK_Do(x_lock, eMT_TryLock)     != 0);
    verify(MT_LOCK_Do(x_lock, eMT_TryLockRead) != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);
    verify(MT_LOCK_Do(x_lock, eMT_Unlock)      != 0);

    verify(MT_LOCK_Delete(x_lock) == 0);
}


static int TEST_CORE_LogData;

/* FLOG_Handler */
static void TEST_CORE_LogHandler(void* data, const SLOG_Message* mess)
{
    printf("TEST_CORE_LogHandler(round %d):\n", TEST_CORE_LogData);
    printf("   Message: %s\n", mess->message ? mess->message : "?");
    printf("   Level:   %s\n", LOG_LevelStr(mess->level));
    printf("   Module:  %s\n", mess->module ? mess->module : "?");
    printf("   Func:    %s\n", mess->func ? mess->func : "?");
    printf("   File:    %s\n", mess->file ? mess->file : "?");
    printf("   Line:    %d\n", mess->line);
}


/* FLOG_Cleanup */
static void TEST_CORE_LogCleanup(void* data)
{
    assert(data == &TEST_CORE_LogData);
    printf("TEST_CORE_LogCleanup(round %d)\n", TEST_CORE_LogData);
    TEST_CORE_LogData = 444;
}


static void TEST_CORE_Log(void)
{
    /* LOG */
    LOG x_log;

    /* protective MT-lock */
    MT_LOCK x_lock;

    /* ELOG_Level, LOG_LevelStr() */
    int x_level;
    for (x_level = 0;  x_level <= (int) eLOG_Fatal;  x_level++) {
        switch ( (ELOG_Level) x_level ) {
        case eLOG_Trace:
        case eLOG_Note:
        case eLOG_Warning:
        case eLOG_Error:
        case eLOG_Critical:
        case eLOG_Fatal:
            assert(LOG_LevelStr((ELOG_Level) x_level));
            printf("LOG_LevelStr(level = %d): \"%s\"\n",
                   x_level, LOG_LevelStr((ELOG_Level) x_level));
            break;
        default:
            assert(0);
        }
    }

    /* LOG API */

    /* MT-lock */
    x_lock = MT_LOCK_Create(&TEST_CORE_LockData,
                            TEST_CORE_LockHandler, TEST_CORE_LockCleanup);
    /* dummy */
    TEST_CORE_LogData = 1;
    x_log = LOG_Create(&TEST_CORE_LogData,
                       TEST_CORE_LogHandler, TEST_CORE_LogCleanup,
                       x_lock);
    verify(MT_LOCK_Delete(x_lock));
    assert(x_log);

    verify(LOG_AddRef(x_log) == x_log);
    verify(LOG_AddRef(x_log) == x_log);
    verify(LOG_Delete(x_log) == x_log);
    assert(TEST_CORE_LogData == 1);

    LOG_WRITE(0, 0, 0, eLOG_Trace, 0);
    LOG_Write(0, 0, 0, eLOG_Trace, 0, 0, 0, 0, 0, 0, 0);
    LOG_WRITE(x_log, 0, 0, eLOG_Trace, 0);
    LOG_Write(x_log, 0, 0, eLOG_Trace, 0, 0, 0, 0, 0, 0, 0);

    verify(LOG_Delete(x_log) == x_log);
    assert(TEST_CORE_LogData == 1);

    /* reset to "real" logging */
    LOG_Reset(x_log, &TEST_CORE_LogData,
              TEST_CORE_LogHandler, TEST_CORE_LogCleanup);
    assert(TEST_CORE_LogData == 444);
    TEST_CORE_LogData = 2;

    /* do the test logging */
    LOG_WRITE(x_log, 0, 0, eLOG_Trace, 0);
    LOG_Write(x_log, 0, 0, eLOG_Trace, 0, 0, 0, 0, 0, 0, 0);
    LOG_WRITE(x_log, 0, 0, eLOG_Warning, "");
    /* LOG_WRITE(x_log, eLOG_Fatal, "Something fatal"); */
#undef  THIS_MODULE
#define THIS_MODULE  "SomeModuleName"
    LOG_WRITE(x_log, 0, 0, eLOG_Error, "With changed module name");
#undef  THIS_MODULE
#define THIS_MODULE  0

    /* delete */
    verify(LOG_Delete(x_log) == 0);
    assert(TEST_CORE_LogData == 444);
}


static void TEST_CORE(void)
{
    /* Do all TEST_CORE_***() tests */
    DO_TEST(TEST_CORE_Io);
    DO_TEST(TEST_CORE_Lock);
    DO_TEST(TEST_CORE_Log);
}



/*****************************************************************************
 *  TEST_UTIL -- test "ncbi_util.c"
 */

static void TEST_UTIL_Log(void)
{
    /* create */
    LOG x_log = LOG_Create(0, 0, 0, 0);
    LOG_ToFILE(x_log, stdout, 0/*false*/);

    CORE_SetLOGFormatFlags(fLOG_Full | fLOG_Function);

    /* simple logging */
    LOG_WRITE(x_log, 0, 0, eLOG_Trace, 0);
    LOG_Write(x_log, 0, 0, eLOG_Trace, 0, 0, 0, 0, 0, 0, 0);
    LOG_WRITE(x_log, 0, 0, eLOG_Warning, "");
    /* LOG_WRITE(x_log, eLOG_Fatal, "Something fatal"); */
#undef  THIS_MODULE
#define THIS_MODULE  "SomeModuleName"
    LOG_WRITE(x_log, 0, 0, eLOG_Error, "With changed module name");
#undef  THIS_MODULE
#define THIS_MODULE  0

    /* data logging */
    {{
        size_t i;
        unsigned char data[300];
        for (i = 0;  i < sizeof(data);  ++i)
            data[i] = (unsigned char) i;
        LOG_DATA(x_log, 0, 0, eLOG_Note,
                 data, sizeof(data), "Data logging test");
    }}

    /* delete */
    verify(LOG_Delete(x_log) == 0);
}


static void TEST_UTIL_CRC32(void)
{
    unsigned int chksum = UTIL_CRC32_Update(0,
                                            "abcdefghijklmnopqrstuvwxyz", 26);
    verify(chksum == 0x3BC2A463);  /* see src/util/test/test_checksum.cpp */
}


#define TEST_PRINTABLE(str, flg, res)                               \
    *UTIL_PrintableString(str, sizeof(str) - 1, buf, flg) = '\0';   \
    assert(strcmp(buf, res) == 0)

static void TEST_UTIL_Printable(void)
{
    char buf[256];
    assert(!UTIL_PrintableString(0, 2, buf, 0));
    assert(!UTIL_PrintableString(buf, 2, 0, 0));
    /* cf. corelib/test/test_ncbistr.cpp */
    TEST_PRINTABLE("", 0, "");
    TEST_PRINTABLE("?AA\?\?AA?AA\?\?\?AA\?\?AA?", 0,
                   "\?AA\\?\\?AA\?AA\\?\\?\\?AA\\?\\?AA\?");
    TEST_PRINTABLE("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"", fUTIL_PrintableNoNewLine,
                   "AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"");
    TEST_PRINTABLE("A\x01\r\177\x000F\0205B\0CD", 0,
                   "A\\1\\r\\177\\17\\0205B\\0CD");
    TEST_PRINTABLE("A\x01\r\xC1\xF7\x07\x3A\252\336\202\x000F\0205B\0CD",
                   fUTIL_PrintableFullOctal,
                   "A\\001\\r\\301\\367\\a:\\252\\336\\202\\017\\0205B\\000CD");
    TEST_PRINTABLE("A\nB\\\nC", fUTIL_PrintableNoNewLine,
                   "A\\nB\\\\\\nC");
    TEST_PRINTABLE("A\nB\\\nC", 0,
                   "A\\n\\\nB\\\\\\n\\\nC");
}


/* NOTE: closes STDIN */
static void TEST_CORE_GetUsername(void)
{
    static const char* null =
#ifdef NCBI_OS_MSWIN
        "NUL"
#else
        "/dev/null"
#endif /*NCBI_OS_MSWIN*/
        ;
    char buffer[512];
    const char* temp = CORE_GetUsername(buffer, sizeof(buffer));
    char* user = temp  &&  *temp ? strdup(temp) : 0;
    printf("Username = %s%s%s%s\n",
           temp ? (*temp ? "" : "\"") : "<",
           temp ?   temp              : "NULL",
           temp ? (*temp ? "" : "\"") : ">",
           !temp ^ !user ? ", error!" : "");
    temp = CORE_GetUsernameEx(buffer, sizeof(buffer), eCORE_UsernameReal);
    printf("Username(Real) = %s%s%s\n",
           temp ? (*temp ? "" : "\"") : "<",
           temp ?   temp              : "NULL",
           temp ? (*temp ? "" : "\"") : ">");
    temp = CORE_GetUsernameEx(buffer, sizeof(buffer), eCORE_UsernameLogin);
    printf("Username(Login) = %s%s%s\n",
           temp ? (*temp ? "" : "\"") : "<",
           temp ?   temp              : "NULL",
           temp ? (*temp ? "" : "\"") : ">");
    temp = CORE_GetUsernameEx(buffer, sizeof(buffer), eCORE_UsernameCurrent);
    printf("Username(Current) = %s%s%s\n",
           temp ? (*temp ? "" : "\"") : "<",
           temp ?   temp              : "NULL",
           temp ? (*temp ? "" : "\"") : ">");
    /* NB: GCC's __wur (warn unused result) silenced */
    verify(freopen(null, "r", stdin));  /* NCBI_FAKE_WARNING: GCC */
    temp = CORE_GetUsername(buffer, sizeof(buffer));
    printf("Username = %s%s%s\n",
           temp ? (*temp ? "" : "\"") : "<",
           temp ?   temp              : "NULL",
           temp ? (*temp ? "" : "\"") : ">");
    if (temp  &&  !*temp)
        temp = 0;
    assert((!temp  &&  !user)  ||
           ( temp  &&   user  &&  strcasecmp(temp, user) == 0));
    if (user)
        free(user);
}


static void TEST_UTIL_MatchesMask(void)
{
    assert(UTIL_MatchesMaskEx("aaa", "*a",             0) == 1);
    assert(UTIL_MatchesMaskEx("bbb", "*a",             0) == 0);
    assert(UTIL_MatchesMaskEx("bba", "*a",             0) == 1);
    assert(UTIL_MatchesMaskEx("aab", "*a",             0) == 0);
    assert(UTIL_MatchesMaskEx("aaa", "*a*",            0) == 1);
    assert(UTIL_MatchesMaskEx("AAA", "*a",             1) == 1);
    assert(UTIL_MatchesMaskEx("aaa", "*",              0) == 1);
    assert(UTIL_MatchesMaskEx("aaa", "[a-z][a]a",      0) == 1);
    assert(UTIL_MatchesMaskEx("aaa", "[!b][!b-c]a",    0) == 1);
    assert(UTIL_MatchesMaskEx("aaa", "a[]",            0) == 0);
    assert(UTIL_MatchesMaskEx("aaa", "a[a-a][a-",      0) == 0);
    assert(UTIL_MatchesMaskEx("aaa", "a[a-a][a-]",     0) == 1);
    assert(UTIL_MatchesMaskEx("a\\", "a\\",            0) == 0);
    assert(UTIL_MatchesMaskEx("a\\", "a\\\\",          0) == 1);
    assert(UTIL_MatchesMaskEx("a\\", "a[\\]",          0) == 1);
    assert(UTIL_MatchesMaskEx("a\\", "a[\\\\]",        0) == 1);
    assert(UTIL_MatchesMaskEx("aaa", "[a-b][a-b][a-b", 0) == 0);
    assert(UTIL_MatchesMaskEx("a*a", "a\\*a",          0) == 1);
    assert(UTIL_MatchesMaskEx("a[]", "[a-z][[][]]",    0) == 1);
    assert(UTIL_MatchesMaskEx("a[]", "[a-z][[][[\\]]", 0) == 0);
    assert(UTIL_MatchesMaskEx("a!b", "[a-z][!][A-Z]",  1) == 0);
    assert(UTIL_MatchesMaskEx("a!b", "[a-z][!A-Z]b",   1) == 1);
    assert(UTIL_MatchesMaskEx("a!b", "[a-z][!][A-Z]b", 1) == 1);
    assert(UTIL_MatchesMaskEx("a-b", "[a-z][0-][A-Z]", 1) == 1);
    assert(UTIL_MatchesMaskEx("a-b", "[a-z][-9][A-Z]", 1) == 1);
}


static void TEST_CORE_GetVMPageSize(void)
{
    printf("PageSize = %d\n", (int) CORE_GetVMPageSize());
}


static void TEST_UTIL(void)
{
    DO_TEST(TEST_UTIL_Log);
    DO_TEST(TEST_UTIL_CRC32);
    DO_TEST(TEST_UTIL_Printable);
    DO_TEST(TEST_CORE_GetUsername);
    DO_TEST(TEST_UTIL_MatchesMask);
    DO_TEST(TEST_CORE_GetVMPageSize);
}


static void TEST_Base64(void)
{
    static struct {
        const char* plain;
        const char* encoded;
    } kTests[] = {
        /* Empty input (valid) and illegal encoded sequence */
        { "", "1"    },   /* 0: Too short     */
        { "", "22"   },   /* 1: Too short     */
        { "", "333"  },   /* 2: Too short     */
        { "", "====" },   /* 3: Invalid       */
        { "", "T===" },   /* 4: Invalid       */
        { "", "TE==" },   /* 5: Incomplete    */
        { "", "TQ=x" },   /* 6: Bad padding   */
        { "", "TQ?=" },   /* 7: Bad character */
        /* Carriage control => empty decode   */
        { "", "\n\f\r\v" }, /* 8: Whitespace  */
        /* Valid empty input <=> empty output */
        { "", "" },
        /* Test vectors from RFC4648 */
        { "f",      "Zg=="     },
        { "fo",     "Zm8="     },
        { "foo",    "Zm9v"     },
        { "foob",   "Zm9vYg==" },
        { "fooba",  "Zm9vYmE=" },
        { "foobar", "Zm9vYmFy" },
        /* Test vectors from Wikipedia */
        { "M",   "TQ==" },
        { "Ma",  "TWE=" },
        { "Man", "TWFu" },
        { "Many hands make light work.",
          "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu" },
        { "light work.",
          "bGlnaHQgd29yay4=" },
        { "light work",
          "bGlnaHQgd29yaw==" },        
        { "light wor",
          "bGlnaHQgd29y" },
        { "light wo",
          "bGlnaHQgd28=" },
        { "light w",
          "bGlnaHQgdw==" },
        /* Other test vectors */
        { "The quick brown fox jumps over the lazy dog.",
          "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZy4=" },
        {  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do"
           " eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut"
           " enim ad minim veniam, quis nostrud exercitation ullamco laboris"
           " nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor"
           " in reprehenderit in voluptate velit esse cillum dolore eu fugiat"
           " nulla pariatur. Excepteur sint occaecat cupidatat non proident,"
           " sunt in culpa qui officia deserunt mollit anim id est laborum.",
           "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZWxpdCwg\n"
           "c2VkIGRvIGVpdXNtb2QgdGVtcG9yIGluY2lkaWR1bnQgdXQgbGFib3JlIGV0IGRvbG9yZSBtYWdu\n"
           "YSBhbGlxdWEuIFV0IGVuaW0gYWQgbWluaW0gdmVuaWFtLCBxdWlzIG5vc3RydWQgZXhlcmNpdGF0\n"
           "aW9uIHVsbGFtY28gbGFib3JpcyBuaXNpIHV0IGFsaXF1aXAgZXggZWEgY29tbW9kbyBjb25zZXF1\n"
           "YXQuIER1aXMgYXV0ZSBpcnVyZSBkb2xvciBpbiByZXByZWhlbmRlcml0IGluIHZvbHVwdGF0ZSB2\n"
           "ZWxpdCBlc3NlIGNpbGx1bSBkb2xvcmUgZXUgZnVnaWF0IG51bGxhIHBhcmlhdHVyLiBFeGNlcHRl\n"
           "dXIgc2ludCBvY2NhZWNhdCBjdXBpZGF0YXQgbm9uIHByb2lkZW50LCBzdW50IGluIGN1bHBhIHF1\n"
           "aSBvZmZpY2lhIGRlc2VydW50IG1vbGxpdCBhbmltIGlkIGVzdCBsYWJvcnVtLg==" }
    };
    size_t n, n_read, n_written;
    char buf[1024];

    for (n = 0;  n < sizeof(kTests) / sizeof(kTests[0]);  ++n) {
        const char* plain   = kTests[n].plain;
        const char* encoded = kTests[n].encoded;
        size_t len;

        printf("Test %zu\n", n + 1);

        len = strlen(plain);
        assert(BASE64_Encode(plain, len,         &n_read,
                             buf,   sizeof(buf), &n_written, 0));
        assert(n_read == len);
        len = strlen(encoded);
        if (n > 8) {
            assert(n_written == len);
            assert(strcmp(encoded, buf) == 0);
        }

        assert(BASE64_Decode(encoded, len,         &n_read,
                             buf,     sizeof(buf), &n_written) == (n > 7));
        if (n > 7) {
            assert(n_read == len);
            len = strlen(plain);
            assert(n_written == len);
            assert(memcmp(plain, buf, len) == 0);
        } else switch (n) {
        case 0:
            assert(n_read == 1  &&  n_written == 0);
            break;
        case 1:
            assert(n_read == 2  &&  n_written == 1);
            break;
        case 2:
            assert(n_read == 3  &&  n_written == 2);
            break;
        case 3:
            assert(n_read == 0  &&  n_written == 0);
            break;
        case 4:
            assert(n_read == 2  &&  n_written == 0);
            break;
        case 5:
            assert(n_read == 3  &&  n_written == 1);
            break;
        case 6:
            assert(n_read == 3  &&  n_written == 1);
            break;
        case 7:
            assert(n_read == 2  &&  n_written == 1);
            break;
        }
    }
}


/*****************************************************************************
 *  MAIN -- test all
 */

int main(void)
{
    TEST_CORE();
    TEST_UTIL();
    DO_TEST(TEST_Base64);
    puts("\nTEST completed successfully");
    return 0;
}
