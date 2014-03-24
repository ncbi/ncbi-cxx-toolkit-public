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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *    The C library to provide C++ Toolkit-like logging semantics 
 *    and output for C/C++ programs and CGIs.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include "ncbi_c_log_p.h"

#if defined(NCBI_OS_UNIX)
#  include <string.h>
#  include <limits.h>
#  include <ctype.h>
#  include <sys/utsname.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <unistd.h>
#elif defined(NCBI_OS_MSWIN)
#  include <sys/types.h>
#  include <sys/timeb.h>
#  include <windows.h>
#  include <direct.h>
#endif

#if defined(_MT)
#  if defined(NCBI_OS_MSWIN)
#    define NCBI_WIN32_THREADS
#  elif defined(NCBI_OS_UNIX)
#    define NCBI_POSIX_THREADS
#  else
#    define NCBI_NO_THREADS
#  endif
#else
#  define NCBI_NO_THREADS
#endif

#if defined(NCBI_POSIX_THREADS)
#    include <pthread.h>
#    include <sys/errno.h>
#endif

/* assert, verify */
#if !defined(NDEBUG)  &&  !defined(_DEBUG)
#  define NDEBUG
#endif
#include <assert.h>
#if defined(NDEBUG)
#  define verify(expr)  (void)(expr)
#else
#  define verify(expr)  assert(expr)
#endif

/* Critical error. Should never happens */ 
#ifdef TROUBLE
#    undef TROUBLE
#endif
#define TROUBLE assert(0)


/* Directory separator */
#if defined(NCBI_OS_MSWIN)
#  define DIR_SEPARATOR     '\\'
#elif defined(NCBI_OS_UNIX)
#  define DIR_SEPARATOR     '/'
#endif



/******************************************************************************
 *  Configurables that could theoretically change 
 */

/** Maximum length of each log entry, all text after this position will be truncated */
#define NCBILOG_ENTRY_MAX    8191  /* 8Kb - 1 for ending '\0' */ 

#define UNKNOWN_HOST         "UNK_HOST"
#define UNKNOWN_CLIENT       "UNK_CLIENT"
#define UNKNOWN_SESSION      "UNK_SESSION"
#define UNKNOWN_APPNAME      "UNK_APP"


/*****************************************************************************
 *  Global constants
 */

static const char* kBaseLogDir  = "/log/";


/*****************************************************************************
 *  Global variables 
 */

/* Init is done if TRUE */
static volatile int /*bool*/      sx_IsInit    = 0;

/* Enable/disable logging */
static volatile int /*bool*/      sx_IsEnabled = 0;

/* Enable/disable checks and state verifications -- used in ncbi_applog utility */
static volatile int /*bool*/      sx_DisableChecks = 0;

/* Application access log and error postings info structure (global) */
static volatile TNcbiLog_Info*    sx_Info      = NULL;

/* Pointer to the context (single-threaded applications only, otherwise use TLS) */
static volatile TNcbiLog_Context  sx_ContextST = NULL;


/******************************************************************************
 *  MT locking
 */

/* Define default MT handler */

#if defined(NCBI_POSIX_THREADS)
    static pthread_mutex_t sx_MT_handle = PTHREAD_MUTEX_INITIALIZER;
#elif defined(NCBI_WIN32_THREADS)
    static CRITICAL_SECTION sx_MT_handle;
#endif

int/*bool*/ NcbiLog_Default_MTLock_Handler
    (void*                  user_data, 
     ENcbiLog_MTLock_Action action
     )
{
#if defined(NCBI_POSIX_THREADS)
    switch (action) {
        case eNcbiLog_MT_Init:
            /* Already done with static initialization; do nothing here */
            /* verify(pthread_mutex_init(&sx_MT_handle, 0) == 0); */
            break;
        case eNcbiLog_MT_Destroy:
            /* Mutex should be unlocked before destroying */
            assert(pthread_mutex_unlock(&sx_MT_handle) == 0);
            verify(pthread_mutex_destroy(&sx_MT_handle) == 0);
            break;
        case eNcbiLog_MT_Lock:
            verify(pthread_mutex_lock(&sx_MT_handle) == 0);
            break;
        case eNcbiLog_MT_Unlock:
            verify(pthread_mutex_unlock(&sx_MT_handle) == 0);
            break;
    }
    return 1;

#elif defined(NCBI_WIN32_THREADS)

    switch (action) {
        case eNcbiLog_MT_Init:
            InitializeCriticalSection(&sx_MT_handle);
            break;
        case eNcbiLog_MT_Destroy:
            DeleteCriticalSection(&sx_MT_handle);
            break;
        case eNcbiLog_MT_Lock:
            EnterCriticalSection(&sx_MT_handle);
            break;
        case eNcbiLog_MT_Unlock:
            LeaveCriticalSection(&sx_MT_handle);
            break;
    }
    return 1;

#else
    /* Not implemented -- not an MT build or current platform
       doesn't support MT-threading */
    return 0;
#endif
}


/* MT locker data and callbacks */
struct TNcbiLog_MTLock_tag {
    void*                   user_data;     /**< For "handler()"  */
    FNcbiLog_MTLock_Handler handler;       /**< Handler function */
    unsigned int            magic_number;  /**< Used internally to make sure it's init'd */
};
#define kMTLock_magic_number 0x4C0E681F


/* Always use default MT lock by default; can be redefined/disabled by NcbiLog_Init() */
#if defined(_MT)
static struct TNcbiLog_MTLock_tag    sx_MTLock_Default = {NULL, NcbiLog_Default_MTLock_Handler, kMTLock_magic_number};
static volatile TNcbiLog_MTLock      sx_MTLock = &sx_MTLock_Default;
#else
static volatile TNcbiLog_MTLock      sx_MTLock = NULL;
#endif
/* We don't need to destroy default MT lock */
static volatile int /*bool*/         sx_MTLock_Own  = 0;


/* Check the validity of the MT locker */
#define MT_LOCK_VALID \
    if (sx_MTLock) assert(sx_MTLock->magic_number == kMTLock_magic_number)

static int/*bool*/ s_MTLock_Do(ENcbiLog_MTLock_Action action)
{
    MT_LOCK_VALID;
    return sx_MTLock->handler ? sx_MTLock->handler(sx_MTLock->user_data, action)
        : -1 /* rightful non-doing */;
}

#define MT_LOCK_Do(action) (sx_MTLock ? s_MTLock_Do((action)) : -1)

/* Init/Lock/unlock/destroy the MT logger lock 
 */
#define MT_INIT    verify(MT_LOCK_Do(eNcbiLog_MT_Init))
#define MT_LOCK    verify(MT_LOCK_Do(eNcbiLog_MT_Lock))
#define MT_UNLOCK  verify(MT_LOCK_Do(eNcbiLog_MT_Unlock))
#define MT_DESTROY verify(MT_LOCK_Do(eNcbiLog_MT_Destroy))


extern TNcbiLog_MTLock NcbiLog_MTLock_Create
    (void*                   user_data,
     FNcbiLog_MTLock_Handler handler
     )
{
    TNcbiLog_MTLock lock = (struct TNcbiLog_MTLock_tag*)
                           malloc(sizeof(struct TNcbiLog_MTLock_tag));
    if (lock) {
        lock->user_data    = user_data;
        lock->handler      = handler;
        lock->magic_number = kMTLock_magic_number;
    }
    return lock;
}


extern void NcbiLog_MTLock_Delete(TNcbiLog_MTLock lock)
{
    if (lock) {
        MT_LOCK_VALID;
        MT_DESTROY;
        lock->magic_number = 0;
        free(lock);
        lock = NULL;
    }
}



/******************************************************************************
 *  Thread-local storage (TLS)
 */

/* Init is done for TLS */
static volatile int /*bool*/ sx_TlsIsInit = 0;

/* Platform-specific TLS key */
#if defined(NCBI_POSIX_THREADS)
    static volatile pthread_key_t sx_Tls;
#elif defined(NCBI_WIN32_THREADS)
    static volatile DWORD         sx_Tls;
#endif


static void s_TlsInit(void)
{
    assert(!sx_TlsIsInit);
    /* Create platform-dependent TLS key (index) */
#if defined(NCBI_POSIX_THREADS)
    verify(pthread_key_create((pthread_key_t*)&sx_Tls, NULL) == 0);
    /* pthread_key_create does not reset the value to 0 if the key */
    /* has been used and deleted. */
    verify(pthread_setspecific(sx_Tls, 0) == 0);
    sx_TlsIsInit = 1;
#elif defined(NCBI_WIN32_THREADS)
    sx_Tls = TlsAlloc();
    verify(sx_Tls != TLS_OUT_OF_INDEXES);
    sx_TlsIsInit = 1 /*true*/;
#else
    /* Do not use TLS */
#endif
}


static void s_TlsDestroy(void)
{
    assert(sx_TlsIsInit);
#if defined(NCBI_POSIX_THREADS)
    verify(pthread_key_delete(sx_Tls) == 0);
#elif defined(NCBI_WIN32_THREADS)
    TlsFree(sx_Tls);
#else
    TROUBLE;
#endif
    sx_TlsIsInit = 0 /*false*/;
}


static void* s_TlsGetValue(void)
{
    void* ptr = NULL;
    assert(sx_TlsIsInit);
#if defined(NCBI_POSIX_THREADS)
    ptr = pthread_getspecific(sx_Tls);
#elif defined(NCBI_WIN32_THREADS)
    ptr = TlsGetValue(sx_Tls);
    if (!ptr) {
        assert(GetLastError() == ERROR_SUCCESS);
    }
#else
    TROUBLE;
#endif
    return ptr;
}


static void s_TlsSetValue(void* ptr)
{
    assert(sx_TlsIsInit);
#if defined(NCBI_POSIX_THREADS)
    verify(pthread_setspecific(sx_Tls, ptr) == 0);
#elif defined(NCBI_WIN32_THREADS)
    verify(TlsSetValue(sx_Tls, ptr));
#else
    TROUBLE;
#endif
}



/******************************************************************************
 *  Useful macro
 */

/* Check initialization and acquire MT lock */
#define MT_LOCK_API          \
    assert(sx_IsInit);       \
    while (!sx_IsEnabled) {  \
        /* Delays is possible only on the time of first Init() */ \
        s_SleepMicroSec(10); \
    }                        \
    assert(sx_Info);         \
    MT_LOCK


/* Print application start message, if not done yet */
void s_AppStart(TNcbiLog_Context ctx, const char* argv[]);
#define CHECK_APP_START(ctx)      \
    if (sx_Info->state == eNcbiLog_NotSet) { \
        s_AppStart(ctx, NULL);    \
    }

/* Verify an expression and unlock/return on error */
#ifdef VERIFY
#    undef VERIFY
#endif
#define VERIFY(expr)         \
    if (!expr) {             \
        /* error */          \
        assert(expr);        \
        MT_UNLOCK;           \
        return;              \
    }



/******************************************************************************
 *  Aux. functions
 */


/** Sleep the specified number of microseconds.
    Portable and ugly. But we don't need more precision version here,
     we will use it only for context switches and avoiding possible CPU load.
 */
void s_SleepMicroSec(unsigned long mc_sec)
{
#if defined(NCBI_OS_MSWIN)
    Sleep((mc_sec + 500) / 1000);
#elif defined(NCBI_OS_UNIX)
    struct timeval delay;
    delay.tv_sec  = mc_sec / 1000000;
    delay.tv_usec = mc_sec % 1000000;
    while (select(0, (fd_set*) 0, (fd_set*) 0, (fd_set*) 0, &delay) < 0) {
        break;
    }
#endif
}


/** strdup() doesn't exists on all platforms, and we cannot check this here.
    Will use our own implementation.
 */
static char* s_StrDup(const char* str)
{
    size_t size = strlen(str) + 1;
    char*  res = (char*) malloc(size);
    if (res) {
        memcpy(res, str, size);
    }
    return res;
}


/** Concatenate two parts of the path for the current OS.
 */
static const char* s_ConcatPathEx(
    const char* p1, size_t p1_len,  /* [in]     non-NULL */
    const char* p2, size_t p2_len,  /* [in]     non-NULL */
    char*       dst,                /* [in/out] non-NULL */
    size_t      dst_size            /* [in]              */
)
{
    size_t n = 0;
    assert(p1);
    assert(p2);
    assert(dst);
    assert(dst_size);

    if (p1_len + p2_len + 2 >= dst_size) {
        return NULL;
    }
    memcpy(dst, p1, p1_len);
    n += p1_len;
    if (dst[n-1] != DIR_SEPARATOR) {
        dst[n++] = DIR_SEPARATOR;
    }
    memcpy(dst + n, p2, p2_len);
    n += p2_len;
    dst[n] = '\0';
    return dst;
}


/** Concatenate two parts of the path (zero-terminated strings) for the current OS.
 */
static const char* s_ConcatPath(
    const char* p1,         /* [in]     non-NULL */
    const char* p2,         /* [in]     non-NULL */
    char*       dst,        /* [in/out] non-NULL */
    size_t      dst_size    /* [in]              */
)
{
    return s_ConcatPathEx(p1, strlen(p1), p2, strlen(p2), dst, dst_size);
}


/** Get host name.
 *  The order is: cached hostname, cached host IP, uname or COMPUTERNAME,
 *  SERVER_ADDR, empty string.
 */
const char* NcbiLogP_GetHostName(void)
{
    static char* host = NULL;
#if defined(NCBI_OS_UNIX)
    struct utsname buf;
#elif defined(NCBI_OS_MSWIN)
    char* compname;
#endif
    if ( host ) {
        return host;
    }
 
#if defined(NCBI_OS_UNIX)
    if (uname(&buf) == 0) {
        host = s_StrDup(buf.nodename);
    }
#elif defined(NCBI_OS_MSWIN)
    /* MSWIN - use COMPUTERNAME */
    compname = getenv("COMPUTERNAME");
    if ( compname  &&  *compname ) {
        host = compname;
    }
#endif
    return host;
}


/** Get current process ID.
*/
static TNcbiLog_PID s_GetPID(void)
{
    /* For main thread always force caching of PIDs */
    static TNcbiLog_PID pid = 0;
    if ( !pid ) {
#if defined(NCBI_OS_UNIX)
        pid = getpid();
#elif defined(NCBI_OS_MSWIN)
        pid = GetCurrentProcessId();
#endif
    }
    return pid;
}


/** Get current thread ID.
 */
static TNcbiLog_TID s_GetTID(void)
{
    /* Never cache thread ID */
    TNcbiLog_TID tid = 0;
#if defined(NCBI_NO_THREADS)
    tid = 0;
#elif defined(NCBI_POSIX_THREADS)
    tid = (TNcbiLog_TID)pthread_self(); /* NCBI_FAKE_WARNING */
#elif defined(NCBI_WIN32_THREADS)
    tid = GetCurrentThreadId();
#endif
    return tid;
}


/** Create unique process ID.
 *  Adapted from C++ Toolkit code: see CDiagContect::x_CreateUID().
 */
static TNcbiLog_Int8 s_GetGUID(void)
{
    TNcbiLog_PID   pid = s_GetPID();
    time_t         t = time(0);
    const char*    host = NcbiLogP_GetHostName();
    const char*    s;
    TNcbiLog_Int8  h = 212;

    /* Use host name */
    if ( host ) {
        for (s = host;  *s != '\0';  s++) {
            h = h*1265 + *s;
        }
    }
    h &= 0xFFFF;
    /* The low 4 bits are reserved as GUID generator version number.
       We use version #3. Actually we use C++ version #1 algorithm here, 
       but assign new version number to distinguish the Ncbi C logging API
       from the C++ API.
    */
    return ((TNcbiLog_Int8)h << 48) |
           (((TNcbiLog_Int8)pid & 0xFFFF) << 32) |
           (((TNcbiLog_Int8)t & 0xFFFFFFF) << 4) |
           3; 
}


/** Get current GMT time with nanoseconds
*/
static int/*bool*/ s_GetTimeT(time_t* time_sec, unsigned long* time_ns)
{
#if defined(NCBI_OS_MSWIN)
    struct _timeb timebuffer;
#elif defined(NCBI_OS_UNIX)
    struct timeval tp;
#endif
    if (!time_sec || ! time_ns) {
        return 0;
    }
#if defined(NCBI_OS_MSWIN)
    _ftime(&timebuffer);
    *time_sec = timebuffer.time;
    *time_ns  = (long) timebuffer.millitm * 1000000;

#elif defined(NCBI_OS_UNIX)
    if (gettimeofday(&tp,0) != 0) {
        *time_sec = -1;
    } else {
        *time_sec = tp.tv_sec;
        *time_ns  = (long)((double)tp.tv_usec * 1000);
    }
#else
    *time_sec = time(0);
    *time_ns  = 0;
#endif
    if (*time_sec == (time_t)(-1)) {
        return 0;
    }
    return 1;
}


/** Get current GMT time with nanoseconds (STime version)
*/
static int/*bool*/ s_GetTime(STime* t)
{
    if ( !t ) {
        return 0;
    }
    return s_GetTimeT(&t->sec, &t->ns);
}


/** Get time difference, time_end >- time_start
 */
static double s_DiffTime(const STime time_start, const STime time_end)
{
    double ts;
    ts = (time_end.sec - time_start.sec) + 
         ((double)time_end.ns - time_start.ns)/1000000000;
    return ts;
}


/** Get current local time in format 'YYYY-MM-DDThh:mm:ss.sss'
 */
static int/*bool*/ s_GetTimeStr
    (char*         buf,        /* [in/out] non-NULL, must fit 24 symbols */
     time_t        time_sec,   /* [in]              */
     unsigned long time_ns     /* [in]              */
     )
{
#ifdef HAVE_LOCALTIME_R
    struct tm *temp;
#endif
    struct tm *t;
    int n;

    /* Get current time if necessary */
    if ( !time_sec ) {
        if (!s_GetTimeT(&time_sec, &time_ns)) {
            return 0;
        }
    }

    /* Get local time */
#ifdef HAVE_LOCALTIME_R
    localtime_r(&time_sec, &temp);
    t = &temp;
#else
    t = localtime(&time_sec);
    if ( !t ) {
        /* Error was detected: incorrect timer value or system error */
        return 0;
    }
#endif
    /* YYYY-MM-DDThh:mm:ss.sss */
    n = sprintf(buf, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        (unsigned int)(time_ns / 1000000) /* truncate to 3 digits */
    );
    if (n != 23) {
        return 0 /*false*/;
    }
    return 1 /*true*/;
}


/** logic: This routine is called during start up and again in NcbiLog_ReqStart(). 
 *  If there's a client value set in the environment, we use that -- 
 *  otherwise (not in this function), if there's a previously user-supplied
 *  value, use that -- otherwise, fall back to UNK_CLIENT.
 */
static const char* s_GetClientIP(void)
{
    static const char* client = NULL;
    const char* ext = NULL;
    if ( client ) {
        return client;
    }
    ext = getenv("HTTP_CAF_EXTERNAL");
    if ( !ext  ||  !ext[0] ) {
        // !HTTP_CAF_EXTERNAL
        if ( (client = getenv("HTTP_CLIENT_HOST")) != NULL  &&  client[0] ) {
            return client;
        }
    }
    if ( (client = getenv("HTTP_CAF_PROXIED_HOST")) != NULL  &&  client[0] ) {
        return client;
    }
    if ( (client = getenv("PROXIED_IP")) != NULL  &&  client[0] ) {
        return client;
    }
    if ( (client = getenv("REMOTE_ADDR")) != NULL  &&  client[0] ) {
        return client;
    }
    return NULL;
}


/* The URL-encoding table
 */
static const char sx_EncodeTable[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

/** URL-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the encoded data to buffer "dst_buf", but no more than
 *  "dst_size" bytes.
 *  Assign "*src_read" to the # of bytes successfully encoded from "src_buf".
 *  Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
static void s_URL_Encode
    (const void* src_buf,    /* [in]     non-NULL */
     size_t      src_size,   /* [in]              */
     size_t*     src_read,   /* [out]    non-NULL */
     void*       dst_buf,    /* [in/out] non-NULL */
     size_t      dst_size,   /* [in]              */
     size_t*     dst_written /* [out]    non-NULL */
     )
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
        (*src_read)++, (*dst_written)++, src++, dst++)
    {
        const char* subst = sx_EncodeTable[*src];
        if (*subst != '%') {
            *dst = *subst;
        } else if (*dst_written < dst_size - 2) {
            *dst = '%';
            *(++dst) = *(++subst);
            *(++dst) = *(++subst);
            *dst_written += 2;
        } else {
            return;
        }
    }
}


/** Get a printable version of the specified string. 
 *  All non-printable characters will be represented as "\r", "\n", "\v",
 *  "\t", etc, or "\ooo" where 'ooo' is the octal code of the character. 
 *
 *  This function read up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the encoded data to buffer "dst_buf", but no more than
 *  "dst_size" bytes.
 *  Assign "*src_read" to the # of bytes successfully encoded from "src_buf".
 *  Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
static void s_Sanitize
    (const void* src_buf,    /* [in]     non-NULL */
     size_t      src_size,   /* [in]              */
     size_t*     src_read,   /* [out]    non-NULL */
     void*       dst_buf,    /* [in/out] non-NULL */
     size_t      dst_size,   /* [in]              */
     size_t*     dst_written /* [out]    non-NULL */
     )
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
        (*src_read)++, src++, dst++)
    {
        unsigned char c = *src;
        char subst = 0;

        switch (c) {
            case '\n':
                subst = 'n'; break;
            case '\r':
                subst = 'r'; break;
            case '\t':
                subst = 't'; break;
            case '\v':
                subst = 'v'; break;
            case '\b':
                subst = 'b'; break;
            case '\f':
                subst = 'f'; break;
            case '\a':
                subst = 'a'; break;
        }
        if ( subst ) {
            if (*dst_written + 2 > dst_size) {
                break;
            }
            *dst = '\\';
            *(++dst) = subst;
            *dst_written += 2;
        } else if (isprint(c)) {
            *dst = c;
            *dst_written += 1;
        } else {
            /* convert to octal string */
            unsigned char v;
            if (*dst_written + 4 > dst_size) {
                break;
            }
            *dst = '\\';
            v = c >> 6;
            *(++dst) = '0' + v;
            v = (c >> 3) & 7;
            *(++dst) = '0' + v;
            v = c & 7;
            *(++dst) = '0' + v;
            *dst_written += 4;
        }
    }
}


/** Get base name of application.
 *  Rerun a pointer to allocates memory with zero-terminated string.
 */
static char* s_GetAppBaseName(const char* path)
{
    const char* p = path;
    const char* s;
    char* name;

    /* Find file name */
    for (s = p; *p; ++p ) {
        if ((*p == '/') || (*p == '\\') || (*p == ':')) {
	        /* Found a separator, step over it and any others which immediately follow it */
            while ((*p == '/') || (*p == '\\') || (*p == ':')) {
	            ++p;
            }
            /* If we didn't reach the end of the path string */
	        if ( *p ) {
    	        /* We have a new candidate for the file name */
	            s = p;
            } else {
	            /* otherwise strip off any trailing dir separators */
                while ((p > s)  &&  ((*--p == '/') || (*p == '\\')));
	            /* Now, 's' points to the start of the file name, and 'p' to the end */
                break;
            }
        }
    }
    if (s == p) {
        return NULL;
    }
    name = s_StrDup(s);
    if (!name) {
        return NULL;
    }
#if defined(NCBI_OS_MSWIN)
    /* Find and cut file extension, MS Windows only */
    while ((p > s)  &&  (*--p != '.'));
    if (s != p) {
        name[p-s] = '\0';
    }
#endif
    return name;
}


/** Abort application.
 */
void s_Abort(void)
{
    char* value;
    /* Check environment variable for silent exit */
    value = getenv("DIAG_SILENT_ABORT");
    if (value  &&  (*value == 'Y'  ||  *value == 'y'  ||  *value == '1')) {
        exit(255);
    }
    else if (value  &&  (*value == 'N'  ||  *value == 'n' || *value == '0')) {
        abort();
    }
    else {
#if defined(_DEBUG)
        abort();
#else
        exit(255);
#endif
    }
    TROUBLE;
}


/** Create thread-specific context.
 */
static TNcbiLog_Context s_CreateContext(void)
{
    /* Allocate memory for context structure */
    TNcbiLog_Context ctx = (TNcbiLog_Context) malloc(sizeof(TNcbiLog_Context_Data));
    assert(ctx);
    memset((char*)ctx, 0, sizeof(TNcbiLog_Context_Data));
    /* Initialize context values, all other values = 0 */
    ctx->tid = s_GetTID();
    ctx->tsn = 1;
    return ctx;
}


/** Attach thread-specific context.
 */
static int /*bool*/ s_AttachContext(TNcbiLog_Context ctx)
{
    TNcbiLog_Context prev;
    if ( !ctx ) {
        return 0;
    }
    /* Get current context */
    if ( sx_TlsIsInit ) {
        prev = s_TlsGetValue();
    } else {
        prev = sx_ContextST;
    }
    /* Do not attach context if some different context is already attached */
    if (prev  &&  prev!=ctx) {
        return 0;
    }
    /* Set new context */
    if ( sx_TlsIsInit ) {
        s_TlsSetValue((void*)ctx);
    } else {
        sx_ContextST = ctx;
    }
    return 1 /*true*/;
}


/** Get pointer to the thread-specific context.
 */
static TNcbiLog_Context s_GetContext(void)
{
    TNcbiLog_Context ctx = NULL;
    /* Get context for current thread, or use global value for ST mode */
    if ( sx_TlsIsInit ) {
        ctx = s_TlsGetValue();
    } else {
        ctx = sx_ContextST;
    }
    /* Create new context if not created/attached yet */
    if ( !ctx ) {
        int is_attached;
        /* Special case, context for current thread was already destroyed */
        assert(sx_IsInit != 2);
        /* Create context */
        ctx = s_CreateContext();
        is_attached = s_AttachContext(ctx);
        verify(is_attached);
    }
    assert(ctx);
    return ctx;
}


/** Destroy thread-specific context.
 */
static void s_DestroyContext(void)
{
    TNcbiLog_Context ctx = NULL;
    if ( sx_TlsIsInit ) {
        ctx = s_TlsGetValue();
        free((void*)ctx);
        s_TlsSetValue(NULL);
    } else {
        free((void*)sx_ContextST);
        sx_ContextST = NULL;
    }
}


/** Determine logs location on the base of TOOLKITRC_FILE.
 */
static const char* s_GetToolkitRCLogLocation()
{
    static const char* log_path       = NULL;
    static const char  kToolkitRc[]   = "/etc/toolkitrc";
    static const char  kSectionName[] = "[Web_dir_to_port]";

    FILE*  fp;
    char   buf[256];
    char   *line, *token, *p;
    int    inside_section = 0 /*false*/;
    size_t line_size;

    if (log_path) {
        return log_path;
    }

    /* Try to get more specific location from configuration file */
    fp = fopen(kToolkitRc, "rt");
    if (!fp) {
        return log_path;
    }
    /* Reserve first byte in the buffer for the case of non-absolute path */
    buf[0] = '/';
    line = buf + 1;
    line_size = sizeof(buf) - 1;

    /* Read configuration file */
    while (fgets(line, (int)line_size, fp) != NULL)
    {
        if (inside_section) {
            if (line[0] == '#') {
                /* Comment - skip */
                continue;
            }
            if (line[0] == '[') {
                /* New section started - stop */
                break;
            }
            p = strchr(line, '=');
            if (!p) {
                /* Skip not matched lines*/
                continue;
            }
            token = p + 1;

            /* Remove all spaces */
            while ((p > line)  &&  (*--p == ' '));

            /* Compare path with file name */
            if (line[0] == '/') {
                /* absolute path */
                *++p = '\0';
                if (strncmp(line, sx_Info->app_full_name, strlen(sx_Info->app_full_name)) != 0) {
                    continue;
                }
            } else {
                char c;
                /* not an absolute path */
                if (*p != '/') {
                    *++p ='/';
                }
                /* save and restore byte used for string termination */
                c = *++p;
                *p = '\0';
                if (!strstr(sx_Info->app_full_name, buf)) {
                    continue;
                }
                *p = c;
            }
            /* Get token value, removing spaces and following EOL */
            while (*token == ' ') token++;
            p = token + 1;
            while (*p  &&  !(*p == ' ' || *p == '\n')) p++;

            /* Compose full directory name */
            {{
                char path[FILENAME_MAX + 1];
                s_ConcatPath(kBaseLogDir, token, path, FILENAME_MAX + 1);
                log_path = s_StrDup(path);
                break;
            }}
            /* continue to next line */
        }
        if (memcmp(line, kSectionName, sizeof(kSectionName)) == 0) {
            inside_section = 1;
        }
    }
    fclose(fp);

    return log_path;
}


/** Close log files.
 */
static void s_CloseLogFiles(int/*bool*/ cleanup)
{
    switch (sx_Info->destination) {
        case eNcbiLog_Default:
        case eNcbiLog_Stdlog:
        case eNcbiLog_Cwd:
            /* Close files, see below */
            break;
        case eNcbiLog_Stdout:
        case eNcbiLog_Stderr:
        case eNcbiLog_Disable:
            /* Nothing to do here */
            return;
        default:
            TROUBLE;
    }
    if (sx_Info->file_trace) {
        fclose(sx_Info->file_trace);
        sx_Info->file_trace = NULL;
    }
    if (sx_Info->file_log) {
        fclose(sx_Info->file_log);
        sx_Info->file_log = NULL;
    }
    if (sx_Info->file_err) {
        fclose(sx_Info->file_err);
        sx_Info->file_err = NULL;
    }
    if (sx_Info->file_perf) {
        fclose(sx_Info->file_perf);
        sx_Info->file_perf = NULL;
    }
    if (cleanup  &&  sx_Info->file_trace_name) {
        free(sx_Info->file_trace_name);
        sx_Info->file_trace_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_log_name) {
        free(sx_Info->file_log_name);
        sx_Info->file_log_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_err_name) {
        free(sx_Info->file_err_name);
        sx_Info->file_err_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_perf_name) {
        free(sx_Info->file_perf_name);
        sx_Info->file_perf_name = NULL;
    }
    if (cleanup) {
        sx_Info->reuse_file_names = 0 /*false*/;
    }
}


/** (Re)Initialize logging file streams.
 *  The path passed in parameter should have directory and base name for logging files.
 */
static int /*bool*/ s_SetLogFiles(const char* path_with_base_name) 
{
    char path[FILENAME_MAX + 1];
    size_t n;

    assert(path_with_base_name);
    /* Check max possible file name (path.trace) */
    n = strlen(path_with_base_name);
    assert(n);
    assert((n + 6) < sizeof(path));
    memcpy(path, path_with_base_name, n);

    /* Trace */
    strcpy(path + n, ".trace");
    sx_Info->file_trace = fopen(path, "a");
    sx_Info->file_trace_name = s_StrDup(path);

    /* Log */
    if (sx_Info->file_trace) {
        strcpy(path + n, ".log");
        sx_Info->file_log = fopen(path, "a");
        sx_Info->file_log_name = s_StrDup(path);
    }

    /* Err */
    if (sx_Info->file_log) {
        strcpy(path + n, ".err");
        sx_Info->file_err = fopen(path, "a");
        sx_Info->file_err_name = s_StrDup(path);
    }

    /* Perf */
    if (sx_Info->file_err) {
        strcpy(path + n, ".perf");
        sx_Info->file_perf = fopen(path, "a");
        sx_Info->file_perf_name = s_StrDup(path);
    }

    if (sx_Info->file_trace &&
        sx_Info->file_log   &&
        sx_Info->file_err   &&
        sx_Info->file_perf) {
        return 1 /*true*/;
    }
    s_CloseLogFiles(1);

    return 0 /*false*/;
}


/** (Re)Initialize logging file streams.
 *  Use application base name and specified directory as path for logging files.
 */
static int /*bool*/ s_SetLogFilesDir(const char* dir) 
{
    char path[FILENAME_MAX + 1];
    size_t n, nlen;

    assert(dir);
    n = strlen(dir);
    assert(n);
    nlen = strlen(sx_Info->app_base_name);
    assert(nlen);

    /* Check max possible file name (dir/basename.trace) */
    assert((n + 1 + nlen + 6) < sizeof(path));
    s_ConcatPathEx(dir, n,  sx_Info->app_base_name, nlen, path, FILENAME_MAX + 1);
    n += (nlen + 1);
    return s_SetLogFiles(path);
}


/** (Re)Initialize destination.
 */
static void s_InitDestination(const char* logfile_path) 
{
    time_t now;
    assert(sx_Info->destination != eNcbiLog_Disable);
    if (sx_Info->file_log == stdin  ||  sx_Info->file_log == stdout) {
        /* We already have destination set to stdout/stderr -- don't need to reopen it */
        return;
    }
    /* Reopen files every 1 minute */
    time(&now);
    if (now - sx_Info->last_reopen_time < 60) {
        return;
    }
    sx_Info->last_reopen_time = now;

    /* Try to open files */

    if (sx_Info->destination == eNcbiLog_Default  &&  logfile_path) {
        /* special case to redirect all logging to specified files */
        s_CloseLogFiles(1);
        if (s_SetLogFiles(logfile_path)) {
            sx_Info->reuse_file_names = 1;
            return;
        }
        TROUBLE;
        return;
    }

    /* Close */
    s_CloseLogFiles(0);

    if (sx_Info->destination == eNcbiLog_Default  ||
        sx_Info->destination == eNcbiLog_Stdlog   ||
        sx_Info->destination == eNcbiLog_Cwd) {

        /* Destination and file names didn't changed, just reopen files */
        if (sx_Info->reuse_file_names) {
            assert(sx_Info->file_trace_name &&
                   sx_Info->file_log_name   &&
                   sx_Info->file_err_name   &&
                   sx_Info->file_perf_name);
            sx_Info->file_trace = fopen(sx_Info->file_trace_name, "a");
            sx_Info->file_log   = fopen(sx_Info->file_log_name,   "a");
            sx_Info->file_err   = fopen(sx_Info->file_err_name,   "a");
            sx_Info->file_perf  = fopen(sx_Info->file_perf_name,  "a");

            if (sx_Info->file_trace &&
                sx_Info->file_log   &&
                sx_Info->file_err   &&
                sx_Info->file_perf) {
                return;
            }
            /* Failed to reopen, try again from scratch */
            s_CloseLogFiles(1);
            sx_Info->reuse_file_names = 0;
        }

        /* Try default log location */
        {{
            char xdir[FILENAME_MAX + 1];
            const char* dir;

            /* /log */
            if (sx_Info->destination != eNcbiLog_Cwd) {
                /* toolkitrc file */
                dir = s_GetToolkitRCLogLocation();
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
                /* server port */
                if (sx_Info->server_port) {
                    int n;
                    n = sprintf(xdir, "%s%d", kBaseLogDir, sx_Info->server_port);
                    VERIFY(n > 0);
                    if (s_SetLogFilesDir(xdir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }

                /* /log/srv */ 
                dir = s_ConcatPath(kBaseLogDir, "srv", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
                /* /log/fallback */
                dir = s_ConcatPath(kBaseLogDir, "fallback", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
            }
            /* Try current directory -- eNcbiLog_Stdlog, eNcbiLog_Cwd */
            if (sx_Info->destination != eNcbiLog_Default) {
                char* cwd;
                #if defined(NCBI_OS_UNIX)
                    cwd = getcwd(NULL, 0);
                #elif defined(NCBI_OS_MSWIN)
                    cwd = _getcwd(NULL, 0);
                #endif
                if (cwd  &&  s_SetLogFilesDir(cwd)) {
                    free(cwd);
                    sx_Info->reuse_file_names = 1;
                    return;
                }
                free(cwd);
            }
            /* Fallback - use stderr */
            sx_Info->destination = eNcbiLog_Stderr;
        }}
    }

    /* Use stdout/stderr */
    switch (sx_Info->destination) {
        case eNcbiLog_Stdout:
            sx_Info->file_trace = stdout;
            sx_Info->file_log   = stdout;
            sx_Info->file_err   = stdout;
            sx_Info->file_perf  = stdout;
            break;
        case eNcbiLog_Stderr:
            sx_Info->file_trace = stderr;
            sx_Info->file_log   = stderr;
            sx_Info->file_err   = stderr;
            sx_Info->file_perf  = stderr;
            break;
        default:
            TROUBLE;
    }
}


static void s_SetHost(const char* host)
{
    if (host  &&  *host) {
        size_t len;
        len = strlen(host);
        if (len> NCBILOG_HOST_MAX) {
            len = NCBILOG_HOST_MAX;
        }
        memcpy((char*)sx_Info->host, host, len);
        sx_Info->host[len] = '\0';
    } else {
        sx_Info->host[0] = '\0';
    }
}


static void s_SetClient(TNcbiLog_Context ctx, const char* client)
{
    if (client  &&  *client) {
        size_t len;
        len = strlen(client);
        if (len > NCBILOG_CLIENT_MAX) { 
            len = NCBILOG_CLIENT_MAX;
        }
        memcpy((char*)ctx->client, client, len);
        ctx->client[len] = '\0';
    } else {
        ctx->client[0] = '\0';
    }
}



/******************************************************************************
 *  Print 
 */

/** Post prepared message into the log.
 *  The severity parameter is necessary to choose correct logging file.
 */
static void s_Post(TNcbiLog_Context ctx, ENcbiLog_DiagFile diag)
{
    FILE* f = NULL;
    if (sx_Info->destination == eNcbiLog_Disable) {
        return;
    }
    s_InitDestination(NULL);

    switch (diag) {
        case eDiag_Trace:
            f = sx_Info->file_trace;
            break;
        case eDiag_Err:
            f = sx_Info->file_err;
            break;
        case eDiag_Log:
            f = sx_Info->file_log;
            break;
        case eDiag_Perf:
            f = sx_Info->file_perf;
            break;
        default:
            TROUBLE;
    }
    assert(f);

    /* Write */
    fprintf(f, "%s\n", sx_Info->message);
    fflush(f);

    /* Increase posting serial numbers */
    sx_Info->psn++;
    ctx->tsn++;

    /* Reset posting time if no user time defined */
    if (sx_Info->user_posting_time == 0) {
        sx_Info->post_time.sec = 0;
        sx_Info->post_time.ns  = 0;
    }
}


/* Symbols abbreviation for application state, see ENcbiLog_AppState */
static const char* sx_AppStateStr[] = {
    "NS", "PB", "P", "PE", "RB", "R", "RE"
};

/** Print common prefix to message buffer.
 *  Where <Common Prefix> is:
 *      <pid:5>/<tid:3>/<rid:4>/<state:2> <guid:16> <psn:4>/<tsn:4> <time> <host:15> <client:15> <session:24> <appname>
 *  Return number of written bytes (current position in message buffer), or zero on error.
 */
static size_t s_PrintCommonPrefix(TNcbiLog_Context ctx)
{
    TNcbiLog_PID      x_pid;
    TNcbiLog_Counter  x_rid;
    ENcbiLog_AppState x_st;
    const char        *x_state, *x_host, *x_client, *x_session, *x_appname;
    char              x_time[24];
    int               x_guid_hi, x_guid_lo;
    int               n;

    /* Calculate GUID */
    if ( !sx_Info->guid ) {
        sx_Info->guid = s_GetGUID();
    }
    x_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
    x_guid_lo = (int) (sx_Info->guid & 0xFFFFFFFF);

    /* Get application state (context-specific) */
    x_st = (ctx->state == eNcbiLog_NotSet) ? sx_Info->state : ctx->state;
    x_state = sx_AppStateStr[x_st];

    /* Get posting time string (current or specified by user) */
    if (!sx_Info->post_time.sec) {
        if (!s_GetTime((STime*)&sx_Info->post_time)) {
            /* error */
            return 0;
        }
    }
    if (!s_GetTimeStr(x_time, sx_Info->post_time.sec, sx_Info->post_time.ns)) {
        /* error */
        return 0;
    }

    /* Define properties */

    if (!sx_Info->host[0]) {
        s_SetHost(NcbiLogP_GetHostName());
    }
    x_pid     = sx_Info->pid        ? sx_Info->pid            : s_GetPID();
    x_rid     = ctx->rid            ? ctx->rid                : 0;
    x_host    = sx_Info->host[0]    ? (char*)sx_Info->host    : UNKNOWN_HOST;
    x_client  = ctx->client[0]      ? (char*)ctx->client      : UNKNOWN_CLIENT;
    x_session = ctx->session[0]     ? (char*)ctx->session     : UNKNOWN_SESSION;
    x_appname = sx_Info->appname[0] ? (char*)sx_Info->appname : UNKNOWN_APPNAME;

    n = sprintf(sx_Info->message,
        "%05" NCBILOG_UINT8_FORMAT_SPEC "/%03" NCBILOG_UINT8_FORMAT_SPEC \
        "/%04" NCBILOG_UINT8_FORMAT_SPEC "/%-2s %08X%08X %04" NCBILOG_UINT8_FORMAT_SPEC \
        "/%04" NCBILOG_UINT8_FORMAT_SPEC " %s %-15s %-15s %-24s %s ",
        x_pid,
        ctx->tid,
        x_rid,
        x_state,
        x_guid_hi, x_guid_lo,
        sx_Info->psn,
        ctx->tsn,
        x_time,
        x_host,
        x_client,
        x_session,
        x_appname
    );
    if (n < 0) {
        /* error */
        return 0;
    }
    return n;
}


/** Print one parameter's pair (key, value) to message buffer.
 *  Automatically URL encode each key and value.
 *  Return current position in the buffer.
 */
static size_t s_PrintParamsPair(char* dst, size_t pos, const char* key, const char* value)
{
    size_t len, r_len, w_len;

    if (!key  ||  key[0] == '\0') {
        return pos;
    }
    /* Key */
    len = strlen(key);
    s_URL_Encode(key, len, &r_len, dst + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    if (pos >= NCBILOG_ENTRY_MAX-1) {
        return pos;
    }
    /* Value */
    dst[pos++] = '=';
    if (!value  ||  value[0] == '\0') {
        return pos;
    }
    len = strlen(value);
    s_URL_Encode(value, len, &r_len, dst + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;

    return pos;
}


/** Print parameters to message buffer.
 *  Return current position in the buffer, or zero on error.
 */
static size_t s_PrintParams(char* dst, size_t pos, const SNcbiLog_Param* params)
{
    int/*bool*/ amp = 0;

    /* Walk through the list of arguments */
    if (params) {
        while (params  &&  params->key  &&  pos < NCBILOG_ENTRY_MAX) {
            if (params->key[0] != '\0') {
                if (amp) {
                    dst[pos++] = '&';
                } else {
                    amp = 1;
                }
                pos = s_PrintParamsPair(dst, pos, params->key, params->value);
            }
            params++;
        }
    }
    dst[pos] = '\0';
    return pos;
}


/** Print parameters to message buffer (string version).
 *  The 'params' must be already prepared pairs with URL-encoded key and value.
 *  Return current position in the buffer, or zero on error.
 */
static size_t s_PrintParamsStr(char* dst, size_t pos, const char* params)
{
    size_t len;

    if (!params) {
        return pos;
    }
    len = strlen(params);
    if (len >= NCBILOG_ENTRY_MAX - pos) {
        return pos;
    }
    memcpy(dst + pos, params, len);
    pos += len;
    dst[pos] = '\0';
    return pos;
}


/* Severity string representation  */
static const char* sx_SeverityStr[] = {
    "Trace", "Info", "Warning", "Error", "Critical", "Fatal" 
};

/** Print a message with severity to message buffer.
 */
static void s_PrintMessage(ENcbiLog_Severity severity, const char* msg)
{
    TNcbiLog_Context  ctx;
    int               n;
    size_t            pos, r_len, w_len;
    char*             buf;
    ENcbiLog_DiagFile diag = eDiag_Trace;

    switch (severity) {
        case eNcbiLog_Trace:
        case eNcbiLog_Info:
            diag = eDiag_Trace;
            break;
        case eNcbiLog_Warning:
        case eNcbiLog_Error:
        case eNcbiLog_Critical:
        case eNcbiLog_Fatal:
            diag = eDiag_Err;
            break;
        default:
            TROUBLE;
    }
    /* Check minimum allowed posting level */
    if (severity < sx_Info->post_level) {
        return;
    }

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* Severity */
    n = sprintf(buf + pos, "%s: ", sx_SeverityStr[severity]);
    VERIFY(n > 0);
    pos += n;
    /* Message */
    s_Sanitize(msg, strlen(msg), &r_len, buf + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    buf[pos] = '\0';

    /* Post a message */
    s_Post(ctx, diag);

    MT_UNLOCK;
    if (severity == eNcbiLog_Fatal) {
        s_Abort();
    }
}



/******************************************************************************
 *  NcbiLog
 */

void s_Init(const char* appname) 
{
    size_t len, r_len, w_len;
    char* app = NULL;

    /* Initialize TLS; disable using TLS for ST applications */
    if (sx_MTLock) {
        s_TlsInit();
    } else {
        sx_TlsIsInit = 0;
    }

    /* Create info structure */
    assert(!sx_Info);
    sx_Info = (TNcbiLog_Info*) malloc(sizeof(TNcbiLog_Info));
    assert(sx_Info);
    memset((char*)sx_Info, 0, sizeof(TNcbiLog_Info));

    /* Set defaults */
    sx_Info->psn = 1;
    sx_Info->destination = eNcbiLog_Default;
#if defined(NDEBUG)
    sx_Info->post_level = eNcbiLog_Error;
#else
    sx_Info->post_level = eNcbiLog_Warning;
#endif

    /* If defined, use hard-coded application name from 
       "-D NCBI_C_LOG_APPNAME=appname" on a compilation step */
#if defined(NCBI_C_LOG_APPNAME)
    #define MACRO2STR_(x) #x
    #define MACRO2STR(x) MACRO2STR_(x)
    app = MACRO2STR(NCBI_C_LOG_APPNAME);
#else
    app = (char*)appname;
#endif
    /* Set application name */
    if (!(app  &&  *app)) {
        app = "UNKNOWN";
    }
    sx_Info->app_full_name = s_StrDup(app);
    assert(sx_Info->app_full_name);
    sx_Info->app_base_name = s_GetAppBaseName(sx_Info->app_full_name);
    assert(sx_Info->app_base_name);
    /* URL-encode base name and use it as display name */ 
    len = strlen(sx_Info->app_base_name);
    s_URL_Encode(sx_Info->app_base_name, len, &r_len,
                (char*)sx_Info->appname, 3*NCBILOG_APPNAME_MAX, &w_len);
    sx_Info->appname[w_len] = '\0';

    /* Allocate memory for message buffer */
    sx_Info->message = (char*) malloc(NCBILOG_ENTRY_MAX + 1 /* '\0' */);
    assert(sx_Info->message);

    /* Logging is initialized now and can be used */
    sx_IsEnabled = 1 /*true*/;
}


extern void NcbiLog_Init(const char*               appname, 
                         TNcbiLog_MTLock           mt_lock, 
                         ENcbiLog_MTLock_Ownership mt_lock_ownership)
{
    if (sx_IsInit) {
        /* NcbiLog_Init() is already in progress or done */
        /* This function can be called only once         */
        assert(!sx_IsInit);
        /* error */
        return;
    }
    /* Start initialization */
    /* The race condition is possible here if NcbiLog_Init() is simultaneously
       called from several threads. But NcbiLog_Init() should be called
       only once, so we can leave this to user's discretion.
    */
    sx_IsInit = 1;
    sx_MTLock = mt_lock;
    sx_MTLock_Own = (mt_lock_ownership == eNcbiLog_MT_TakeOwnership);

    MT_INIT;
    MT_LOCK;
    s_Init(appname);
    MT_UNLOCK;
}


extern void NcbiLog_InitMT(const char* appname)
{
    TNcbiLog_MTLock mt_lock = NcbiLog_MTLock_Create(NULL, NcbiLog_Default_MTLock_Handler);
    NcbiLog_Init(appname, mt_lock, eNcbiLog_MT_TakeOwnership);
}


extern void NcbiLog_InitST(const char* appname)
{
    NcbiLog_Init(appname, NULL, eNcbiLog_MT_NoOwnership);
}


extern void NcbiLog_InitForAttachedContext(const char* appname)
{
    /* For POSIX we have static initialization for default MT lock,
       and we can start using locks right now. But on Windows this
       is not possible. So, will do initialization first.
    */
#if defined(NCBI_OS_MSWIN)
    int is_init = InterlockedCompareExchange(&sx_IsInit, 1, 0);
    if (is_init) {
        /* Initialization is already in progress or done */
        return;
    }
    MT_INIT;
    MT_LOCK;
#elif defined(NCBI_OS_UNIX)
    MT_LOCK;
    if (sx_IsInit) {
        /* Initialization is already in progress or done */
        MT_UNLOCK;
        return;
    }
    sx_IsInit = 1;
#endif
    s_Init(appname);
    MT_UNLOCK;
}


extern void NcbiLog_InitForAttachedContextST(const char* appname)
{
    NcbiLog_InitST(appname);
}


extern void NcbiLog_Destroy_Thread(void)
{
    MT_LOCK;
    s_DestroyContext();
    MT_UNLOCK;
}


extern void NcbiLog_Destroy(void)
{
    MT_LOCK;
    sx_IsEnabled = 0;
    sx_IsInit    = 2; /* still TRUE to prevent recurring initialization, but not default 1 */

    /* Close files */
    s_CloseLogFiles(1 /*force cleanup*/);

    /* Free memory */
    if (sx_Info->message) {
        free(sx_Info->message);
    }
    if (sx_Info->app_full_name) {
        free(sx_Info->app_full_name);
    }
    if (sx_Info->app_base_name) {
        free(sx_Info->app_base_name);
    }
    if (sx_Info) {
        free((void*)sx_Info);
        sx_Info = NULL;
    }
    /* Destroy thread-specific context and TLS */
    s_DestroyContext();
    if (sx_TlsIsInit) {
        s_TlsDestroy();
    }
    /* Destroy MT support */
    if (sx_MTLock_Own) {
        NcbiLog_MTLock_Delete(sx_MTLock);
    }
    sx_MTLock = NULL;
}


extern TNcbiLog_Context NcbiLog_Context_Create(void)
{
    TNcbiLog_Context ctx = NULL;
    /* Make initialization if not done yet */
    NcbiLog_InitForAttachedContext(NULL);
    /* Create context */
    ctx = s_CreateContext();
    return ctx;
}


extern int /*bool*/ NcbiLog_Context_Attach(TNcbiLog_Context ctx)
{
    int is_attached;
    if ( !ctx ) {
        return 0 /*false*/;
    }
    MT_LOCK_API;
    is_attached = s_AttachContext(ctx);
    MT_UNLOCK;
    return is_attached;
}


extern void NcbiLog_Context_Detach(void)
{
    MT_LOCK_API;
    /* Reset context */
    if ( sx_TlsIsInit ) {
        s_TlsSetValue(NULL);
    } else {
        sx_ContextST = NULL;
    }
    MT_UNLOCK;
}


extern void NcbiLog_Context_Destroy(TNcbiLog_Context ctx)
{
    assert(ctx);
    free(ctx);
}


/** Set application state -- internal version without MT locking.
 *
 *  We have two states: global and context-specific.
 *  We use global state if context-specific state is not specified.
 *  Setting state to eNcbiLog_App* always overwrite context-specific
 *  state. The eNcbiLog_Request* affect only context-specific state.
 */
static void s_SetState(TNcbiLog_Context ctx, ENcbiLog_AppState state)
{
#if !defined(NDEBUG)
    ENcbiLog_AppState s = (ctx->state == eNcbiLog_NotSet)
                          ? sx_Info->state
                          : ctx->state;
#endif
    switch ( state ) {
        case eNcbiLog_AppBegin:
            if (!sx_DisableChecks) {
                assert(sx_Info->state == eNcbiLog_NotSet);
            }
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eNcbiLog_AppRun:
            if (!sx_DisableChecks) {
                assert(sx_Info->state == eNcbiLog_AppBegin ||
                       s == eNcbiLog_RequestEnd);
            }
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eNcbiLog_AppEnd:
            if (!sx_DisableChecks) {
                assert(sx_Info->state != eNcbiLog_AppEnd);
            }
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eNcbiLog_RequestBegin:
            if (!sx_DisableChecks) {
                assert(s == eNcbiLog_AppBegin  ||
                       s == eNcbiLog_AppRun    ||
                       s == eNcbiLog_RequestEnd);
            }
            ctx->state = state;
            break;
        case eNcbiLog_Request:
            if (!sx_DisableChecks) {
                assert(s == eNcbiLog_RequestBegin);
            }
            ctx->state = state;
            break;
        case eNcbiLog_RequestEnd:
            if (!sx_DisableChecks) {
                assert(s == eNcbiLog_Request  ||
                       s == eNcbiLog_RequestBegin);
            }
            ctx->state = state;
            break;
        default:
            TROUBLE;
    }
}


ENcbiLog_Destination NcbiLog_SetDestination(ENcbiLog_Destination ds)
{
    char* logfile = NULL;
    MT_LOCK_API;
    /* Close current destination */
    s_CloseLogFiles(1 /*force cleanup*/);
    /* Set new destination */
    sx_Info->destination = ds;
    if (sx_Info->destination != eNcbiLog_Disable) {
        /* and force to initialize it */
        sx_Info->last_reopen_time = 0;
        if (sx_Info->destination == eNcbiLog_Default) {
            /* Special case to redirect default logging output */
            logfile = getenv("NCBI_CONFIG__LOG__FILE");
            if (logfile) {
                if (!*logfile ) {
                    logfile = NULL;
                } else {
                    if (strcmp(logfile, "-") == 0) {
                        sx_Info->destination = eNcbiLog_Stderr;
                        logfile = NULL;
                    }
                }
            }
        }
        s_InitDestination(logfile);
    }
    ds = sx_Info->destination;
    MT_UNLOCK;
    return ds;
}


ENcbiLog_Destination NcbiLogP_SetDestination(ENcbiLog_Destination ds, unsigned int port)
{
    char* logfile = NULL;
    MT_LOCK_API;
    /* Special case to redirect default logging output */
    if (ds == eNcbiLog_Default) {
        logfile = getenv("NCBI_CONFIG__LOG__FILE");
        if (logfile) {
            if (!*logfile ) {
                logfile = NULL;
            } else {
                if (strcmp(logfile, "-") == 0) {
                    ds = eNcbiLog_Stderr;
                    logfile = NULL;
                }
            }
        }
    }
    /* Set new destination */
    sx_Info->destination = ds;
    sx_Info->server_port = port;
    if (sx_Info->destination != eNcbiLog_Disable) {
        /* and force to initialize it */
        sx_Info->last_reopen_time = 0;
        s_InitDestination(logfile);
    }
    ds = sx_Info->destination;
    MT_UNLOCK;
    return ds;
}


void NcbiLog_SetProcessId(TNcbiLog_PID pid)
{
    MT_LOCK_API;
    sx_Info->pid = pid;
    MT_UNLOCK;
}


void NcbiLog_SetThreadId(TNcbiLog_TID tid)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    ctx->tid = tid;
    MT_UNLOCK;
}


void NcbiLog_SetRequestId(TNcbiLog_Counter rid)
{
    MT_LOCK_API;
    // Enforce setting request id only after NcbiLog_AppRun()
    if (sx_Info->state == eNcbiLog_NotSet  ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE;
    }
    sx_Info->rid = rid;
    MT_UNLOCK;
}


TNcbiLog_Counter NcbiLog_GetRequestId(void)
{
    TNcbiLog_Counter rid;
    MT_LOCK_API;
    rid = sx_Info->rid;
    MT_UNLOCK;
    return rid;
}


void NcbiLog_SetTime(time_t time_sec, unsigned long time_ns)
{
    MT_LOCK_API;
    sx_Info->post_time.sec = time_sec;
    sx_Info->post_time.ns  = time_ns;
    sx_Info->user_posting_time = (time_sec || time_ns ? 1 : 0);
    MT_UNLOCK;
}


void NcbiLog_SetHost(const char* host)
{
    MT_LOCK_API;
    s_SetHost(host);
    MT_UNLOCK;
}


void NcbiLog_SetClient(const char* client)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_SetClient(ctx, client);
    MT_UNLOCK;
}


void NcbiLog_SetSession(const char* session)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();

    if (session  &&  *session) {
        /* URL-encode session name */
        size_t len, r_len, w_len;
        len = strlen(session);
        s_URL_Encode(session, len, &r_len, (char*)ctx->session, 3*NCBILOG_SESSION_MAX, &w_len);
        ctx->session[w_len] = '\0';
    } else {
        ctx->session[0] = '\0';
    }
    MT_UNLOCK;
}


ENcbiLog_Severity NcbiLog_SetPostLevel(ENcbiLog_Severity sev)
{
    ENcbiLog_Severity prev;
    MT_LOCK_API;
    prev = sx_Info->post_level;
    sx_Info->post_level = sev;
    MT_UNLOCK;
    return prev;
}

/* Forward declaration */
void s_Extra(TNcbiLog_Context ctx, const SNcbiLog_Param* params);
void s_ExtraStr(TNcbiLog_Context ctx, const char* params);


/** Print "start" message. 
 *  We should print "start" message always, before any other message.
 *  The NcbiLog_AppStart() is just a wrapper for this with checks and MT locking.
 */
void s_AppStart(TNcbiLog_Context ctx, const char* argv[])
{
    int    i, n;
    size_t pos;
    char*  buf;

    s_SetState(ctx, eNcbiLog_AppBegin);
    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* We already have current time in sx_Info->post_time */
    /* Save it into sx_AppStartTime. */
    sx_Info->app_start_time.sec = sx_Info->post_time.sec;
    sx_Info->app_start_time.ns  = sx_Info->post_time.ns;

    /* Event name */
    n = sprintf(buf + pos, "%-13s", "start");
    VERIFY(n > 0);
    pos += n;
    /* Walk through list of arguments */
    if (argv) {
        for (i = 0; argv[i] != NULL; ++i) {
            n = sprintf(buf + pos, " %s", argv[i]);
            VERIFY(n > 0);
            pos += n;
        }
    }
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}


void NcbiLog_AppStart(const char* argv[])
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_AppStart(ctx, argv);
    MT_UNLOCK;
}


void NcbiLog_AppRun(void)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_AppRun);
    MT_UNLOCK;
}


void NcbiLog_AppStop(int exit_status)
{
    NcbiLog_AppStopSignal(exit_status, 0);
}


void NcbiLog_AppStopSignal(int exit_status, int exit_signal)
{
    TNcbiLog_Context ctx = NULL;
    int    n;
    size_t pos;
    double timespan;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    s_SetState(ctx, eNcbiLog_AppEnd);
    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* We already have current time in sx_Info->post_time */
    timespan = s_DiffTime(sx_Info->app_start_time, sx_Info->post_time);
    if ( exit_signal ) {
        n = sprintf(sx_Info->message + pos, "%-13s %d %.3f SIG=%d",
                    "stop", exit_status, timespan, exit_signal);
    } else {
        n = sprintf(sx_Info->message + pos, "%-13s %d %.3f",
                    "stop", exit_status, timespan);
    }
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

    MT_UNLOCK;
}


static size_t s_ReqStart(TNcbiLog_Context ctx)
{
    int    n;
    size_t pos;

    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_RequestBegin);

    /* Increase global request number, and save it in the context */
    sx_Info->rid++;
    ctx->rid = sx_Info->rid;

    /* Try to get client IP from environment, if unknown */
    if  ( !ctx->client[0] ) {
        s_SetClient(ctx, s_GetClientIP());
    }

    /* Create fake session id, if unknown */
    if ( !ctx->session[0] ) {
        int x_guid_hi, x_guid_lo;
        x_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
        x_guid_lo = (int) (sx_Info->guid & 0xFFFFFFFF);
        n = sprintf((char*)ctx->session, "%08X%08X_%04" NCBILOG_UINT8_FORMAT_SPEC "SID", 
                     x_guid_hi, x_guid_lo, sx_Info->rid);
        if (n <= 0) {
            return 0;
        }
    }
    
    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    if (pos <= 0) {
        return 0;
    }
    /* We already have current time in sx_Info->post_time */
    /* Save it into sx_RequestStartTime. */
    ctx->req_start_time.sec = sx_Info->post_time.sec;
    ctx->req_start_time.ns  = sx_Info->post_time.ns;

    /* TODO: 
        if "params" is NULL, parse and print $ENV{"QUERY_STRING"}  
    */

    /* Event name */
    n = sprintf(sx_Info->message + pos, "%-13s ", "request-start");
    if (pos <= 0) {
        return 0;
    }
    pos += n;

    /* Return position in the message buffer */
    return pos;
}


void NcbiLog_ReqStart(const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t n, pos;

    MT_LOCK_API;
    ctx = s_GetContext();
    /* Common request info */
    pos = s_ReqStart(ctx);
    VERIFY(pos > 0);
    /* Parameters */
    n = s_PrintParams(sx_Info->message, pos, params);
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

    MT_UNLOCK;
}


void NcbiLogP_ReqStartStr(const char* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t n, pos;

    MT_LOCK_API;
    ctx = s_GetContext();
    /* Common request info */
    pos = s_ReqStart(ctx);
    VERIFY(pos > 0);
    /* Parameters */
    n = s_PrintParamsStr(sx_Info->message, pos, params);
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
    MT_UNLOCK;
}


void NcbiLog_ReqRun(void)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_Request);
    MT_UNLOCK;
}


void NcbiLog_ReqStop(int status, size_t bytes_rd, size_t bytes_wr)
{
    TNcbiLog_Context ctx = NULL;
    int    n;
    size_t pos;
    double timespan;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_RequestEnd);

    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* We already have current time in sx_Info->post_time */
    timespan = s_DiffTime(ctx->req_start_time, sx_Info->post_time);
    n = sprintf(sx_Info->message + pos, "%-13s %d %.3f %lu %lu",
                "request-stop", status, timespan,
                (unsigned long)bytes_rd, (unsigned long)bytes_wr);
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
    /* Reset state, request, client and session id's */
    s_SetState(ctx, eNcbiLog_AppRun);
    ctx->rid = 0;
    ctx->client[0]  = '\0';
    ctx->session[0] = '\0';

    MT_UNLOCK;
}


void s_Extra(TNcbiLog_Context ctx, const SNcbiLog_Param* params)
{
    int    n;
    size_t nu, pos;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    nu = s_PrintParams(buf, pos, params);
    VERIFY(nu > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}


void s_ExtraStr(TNcbiLog_Context ctx, const char* params)
{
    int    n;
    size_t nu, pos;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    nu = s_PrintParamsStr(buf, pos, params);
    VERIFY(nu > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}


void NcbiLog_Extra(const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_Extra(ctx, params);
    MT_UNLOCK;
}


void NcbiLogP_ExtraStr(const char* params)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_ExtraStr(ctx, params);
    MT_UNLOCK;
}


void NcbiLog_Perf(int status, double timespan,
                  const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    int    n;
    size_t nu, pos;
    char*  buf;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    nu = s_PrintParams(buf, pos, params);
    VERIFY(nu > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Perf);

    MT_UNLOCK;
}


void NcbiLogP_PerfStr(int status, double timespan, const char* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t n, pos;
    char*  buf;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos > 0);
    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    n = s_PrintParamsStr(buf, pos, params);
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Perf);

    MT_UNLOCK;
}


void NcbiLog_Trace(const char* msg)
{
    s_PrintMessage(eNcbiLog_Trace, msg);
}

void NcbiLog_Info(const char* msg)
{
    s_PrintMessage(eNcbiLog_Info, msg);
}

void NcbiLog_Warning(const char* msg)
{
    s_PrintMessage(eNcbiLog_Warning, msg);
}

void NcbiLog_Error(const char* msg)
{
    s_PrintMessage(eNcbiLog_Error, msg);
}

void NcbiLog_Critical(const char* msg)
{
    s_PrintMessage(eNcbiLog_Critical, msg);
}

void NcbiLog_Fatal(const char* msg)
{
    s_PrintMessage(eNcbiLog_Fatal, msg);
}


void NcbiLogP_Raw(const char* line)
{
    FILE* f = NULL;

    assert(line);
    assert(strlen(line) > 127);  /* minimal applog line length (?) */

    MT_LOCK_API;
    if (sx_Info->destination == eNcbiLog_Disable) {
        MT_UNLOCK;
        return;
    }
    s_InitDestination(NULL);
    f = sx_Info->file_log;

    switch (sx_Info->destination) {
        case eNcbiLog_Default:
        case eNcbiLog_Stdlog:
        case eNcbiLog_Cwd:
            /* Try to get type of the line to redirect output into correct log file */
            {
                const char* ptr = line + 127 + strlen((char*)sx_Info->appname);
                assert(strlen(line) > (size_t)(ptr - line));

                if (strncmp(ptr, "perf", 4) == 0) {
                    f = sx_Info->file_perf;
                } else 
                if (strncmp(ptr, "Trace", 5) == 0  ||
                    strncmp(ptr, "Info" , 4) == 0) {
                    f = sx_Info->file_trace;
                } else 
                if (strncmp(ptr, "Warning" , 7) == 0  ||
                    strncmp(ptr, "Error"   , 5) == 0  ||
                    strncmp(ptr, "Critical", 8) == 0  ||
                    strncmp(ptr, "Fatal"   , 5) == 0) {
                    f = sx_Info->file_err;
                }
            }
            break;
        case eNcbiLog_Stdout:
        case eNcbiLog_Stderr:
        case eNcbiLog_Disable:
            /* Nothing to do here -- all logging going to the same stream sx_Info->file_log */
            break;
    }
    VERIFY(f);
    fprintf(f, "%s\n", line);
    fflush(f);
    MT_UNLOCK;
}



/******************************************************************************
 *  Logging setup functions --- for internal use only
 */

TNcbiLog_Info* NcbiLogP_GetInfoPtr(void)
{
    return (TNcbiLog_Info*)sx_Info;
}

TNcbiLog_Context NcbiLogP_GetContextPtr(void)
{
    return s_GetContext();
}

 int NcbiLogP_DisableChecks(int /*bool*/ disable)
 {
    int current = sx_DisableChecks;
    sx_DisableChecks = disable;
    return current;
 }
