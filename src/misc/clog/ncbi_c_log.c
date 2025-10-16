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
#  include <unistd.h>
#  include <sys/utsname.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  if !defined(__CYGWIN__)
#      include <sys/syscall.h>
#  endif
#  include <fcntl.h> /* for open() constants */
#  include <sys/errno.h>

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
#  include <pthread.h>
#endif

/* assert, verify */
#if !defined(NDEBUG)  &&  !defined(_DEBUG)
#  define NDEBUG
#endif

#include <assert.h>

/* Critical error, unrecoverable. Should never happen */
#ifdef TROUBLE
#  undef TROUBLE
#endif
#define TROUBLE           s_Abort(__LINE__, 0)
#define TROUBLE_MSG(msg)  s_Abort(__LINE__, msg)

/* Macro to catch a minor errors on a debugging stage. 
 * The code it checks can fails theoretically in release mode, but should not be fatal.
 * See also VERIFY for verifying fatal errors.
 */
#if defined(NDEBUG)
#  define verify(expr)  while ( expr ) break
#else
#  define verify(expr)  assert(expr)
#endif

/* Verify an expression.
 * Debug modes - abort on error.
 * Release modes - report an error if specified (to stderr), continue execution.
 */
#ifdef VERIFY
#  undef VERIFY
#endif

#if defined(NDEBUG)
#  define VERIFY(expr)  do { if ( !(expr) ) s_ReportError(__LINE__, #expr); }  while ( 0 )
#else
#  define VERIFY(expr)  do { if ( !(expr) ) s_Abort(__LINE__, #expr); s_ReportError(__LINE__, #expr); }  while ( 0 )
#endif

#if defined(NDEBUG)
#  define VERIFY_CATCH(expr)  do { if ( !(expr) ) { s_ReportError(__LINE__, #expr); goto __catch_error; } }  while ( 0 )
#else
#  define VERIFY_CATCH(expr)  do { if ( !(expr) ) { s_Abort(__LINE__, #expr); goto __catch_error; } }  while ( 0 )
#endif

/* Catch errors from VERIFY_CATCH.
 * @example
 *         // some yours code with verifying errors
 *         VERIFY(non-critical expression);
 *         VERIFY_CATCH(critical expression);
 *         // and normal return
 *         return;
 *     CATCH:
 *         // recovery code
 *         return;
 */
#define CATCH  __catch_error

#if defined(__GNUC__)
#  define UNUSED_ARG(x)  x##_UNUSED __attribute__((unused))
#else
#  define UNUSED_ARG(x)  x##_UNUSED
#endif

/* min() */
#define min_value(a,b) ((a) < (b) ? (a) : (b))

/* Directory separator */
#if defined(NCBI_OS_MSWIN)
#  define DIR_SEPARATOR     '\\'
#elif defined(NCBI_OS_UNIX)
#  define DIR_SEPARATOR     '/'
#endif



/******************************************************************************
 *  Configurables that could theoretically change 
 */

#define UNKNOWN_HOST         "UNK_HOST"
#define UNKNOWN_CLIENT       "UNK_CLIENT"
#define UNKNOWN_SESSION      "UNK_SESSION"
#define UNKNOWN_HITID        ""
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

/* Real PID for the main thread */
static volatile TNcbiLog_PID      sx_PID = 0;


/******************************************************************************
 *  Abort  (for the locally defined VERIFY and TROUBLE macros)
 */

static void s_Abort(long line, const char* msg)
{
    /* Check environment variable for silent exit */
    char* v = getenv("DIAG_SILENT_ABORT");

    if (msg  &&  *msg) {
        fprintf(stderr, "Critical error: %s, %s, line %ld\n", msg, __FILE__, line);
    } else {
        fprintf(stderr, "Critical error at %s, line %ld\n", __FILE__, line);
    }
    if (v  &&  (*v == 'Y' || *v == 'y' || *v == '1')) {
        exit(255);
    }
    if (v  &&  (*v == 'N' || *v == 'n' || *v == '0')) {
        abort();
    }

#if defined(_DEBUG)
    abort();
#else
    exit(255);
#endif
}


/******************************************************************************
 *  CLog error reporting  (for the locally defined VERIFY)
 *  Used for debugging release binaries only. 
 *  Do nothing if CLog reporting destination is set to eNcbiLog_Stderr.
*/

static void s_ReportError(long line, const char* msg)
{
    if (sx_Info && sx_Info->destination != eNcbiLog_Stderr) {
        return;
    }
    char* v = getenv("NCBI_CLOG_REPORT_ERRORS");
    if (!v || !(*v == 'Y' || *v == 'y' || *v == '1')) {
        return;
    }
    /* Report error */
    const char* m = (msg && *msg) ? msg : "unknown";
    fprintf(stderr, "\nCLog error: %s, %s, line %ld\n", m, __FILE__, line);
}


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
    (void*                  UNUSED_ARG(user_data),
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
#define MT_INIT    verify(MT_LOCK_Do(eNcbiLog_MT_Init) != 0)
#define MT_LOCK    verify(MT_LOCK_Do(eNcbiLog_MT_Lock) != 0)
#define MT_UNLOCK  verify(MT_LOCK_Do(eNcbiLog_MT_Unlock) != 0)
#define MT_DESTROY verify(MT_LOCK_Do(eNcbiLog_MT_Destroy) != 0)


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
static void s_AppStart(TNcbiLog_Context ctx, const char* argv[]);
#define CHECK_APP_START(ctx)      \
    if (sx_Info->state == eNcbiLog_NotSet) { \
        s_AppStart(ctx, NULL);    \
    }

/* Forward declaration */
static void s_Extra(TNcbiLog_Context ctx, const SNcbiLog_Param* params);
static void s_ExtraStr(TNcbiLog_Context ctx, const char* params);



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
    delay.tv_sec  = (long)(mc_sec / 1000000);
    #if defined(NCBI_OS_DARWIN)    
        delay.tv_usec = (int)(mc_sec % 1000000);
    #else
        delay.tv_usec = (long)(mc_sec % 1000000);
    #endif  
    while (select(0, (fd_set*) 0, (fd_set*) 0, (fd_set*) 0, &delay) < 0) {
        break;
    }
#endif
}


/** strdup() doesn't exists on all platforms, will use our own implementation.
    This variant is used when we know a string length in advance or want to copy a part of the string only.
    <len> doesn't include a finishing '\0'.
 */
static char* s_StrDupLen(const char* str, size_t len)
{
    size_t size = len + 1;
    char* res = (char*)malloc(size);
    if (res) {
        memcpy(res, str, size);
    }
    return res;
}

/** strdup() doesn't exists on all platforms, will use our own implementation.
 */
static char* s_StrDup(const char* str)
{
    return s_StrDupLen(str, strlen(str));
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
 *  The order is: cached host name, $NCBI_HOST, uname() or COMPUTERNAME, empty string.
 */
const char* NcbiLog_GetHostName(void)
{
    static const char* host = NULL;
    char* pv = NULL;
    char* p;
    size_t len;
#if defined(NCBI_OS_UNIX)
    struct utsname buf;
#endif
    if ( host ) {
        return host;
    }
    if ( (p = getenv("NCBI_HOST")) != NULL  &&  *p ) {
        pv = p;
    }
#if defined(NCBI_OS_UNIX)
    if ( !pv  &&  uname(&buf) == 0 ) {
        pv = buf.nodename;
    }
#elif defined(NCBI_OS_MSWIN)
    /* MSWIN - use COMPUTERNAME */
    if ( !pv  &&  (p = getenv("COMPUTERNAME")) != NULL  &&  *p ) {
        pv = p;
    }
#endif

    if (pv) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(pv);
        if (len > NCBILOG_HOST_MAX) {
            len = NCBILOG_HOST_MAX;
            pv[len] = '\0';
        }
        host = s_StrDupLen(pv, len);
    }
    return host;
}


/** Read first string from specified file (host string only).
 *  Return NULL on error.
 */
static const char* s_ReadHostFileString(const char* filename)
{
    FILE*  fp;
    char   buf[NCBILOG_HOST_ROLE_MAX];
    size_t len = sizeof(buf);
    char*  p = buf;

    /* to allocate buf size the host role and location size should be equal */
    assert(NCBILOG_HOST_ROLE_MAX == NCBILOG_HOST_LOC_MAX);

    fp = fopen(filename, "rt");
    if (!fp) {
        return NULL;
    }
    if (fgets(buf, (int)len, fp) == NULL) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    /* Removing trailing spaces and following EOL */
    while (*p  &&  !(*p == ' ' || *p == '\n')) p++;
    *p = '\0';
    return s_StrDup(buf);
}


/** Get host role string.
 */
const char* NcbiLog_GetHostRole(void)
{
    static const char* role = NULL;
    char* p;
    size_t len;

    if ( role ) {
        return role;
    }
    if ( (p = getenv("NCBI_ROLE")) != NULL  &&  *p ) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(p);
        if (len > NCBILOG_HOST_ROLE_MAX) {
            len = NCBILOG_HOST_ROLE_MAX;
            p[len] = '\0';
        }
        role = s_StrDupLen(p, len);
        return role;
    }
    role = s_ReadHostFileString("/etc/ncbi/role");
    return role;
}


/** Get host location string.
 */
const char* NcbiLog_GetHostLocation(void)
{
    static const char* location = NULL;
    char* p;
    size_t len;

    if ( location ) {
        return location;
    }
    if ( (p = getenv("NCBI_LOCATION")) != NULL  &&  *p ) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(p);
        if (len > NCBILOG_HOST_LOC_MAX) {
            len = NCBILOG_HOST_LOC_MAX;
            p[len] = '\0';
        }
        location = s_StrDupLen(p, len);
        return location;
    }
    location = s_ReadHostFileString("/etc/ncbi/location");
    return location;
}


/** Get current process ID.
*/
static TNcbiLog_PID s_GetPID(void)
{
    /* For main thread always force caching of PIDs */
    if ( !sx_PID ) {
#if defined(NCBI_OS_UNIX)
        sx_PID = (TNcbiLog_PID)getpid();
#elif defined(NCBI_OS_MSWIN)
        sx_PID = GetCurrentProcessId();
#endif
    }
    return sx_PID;
}


/** Get current thread ID.
 */
static TNcbiLog_TID s_GetTID(void)
{
    /* Never cache thread ID */
    TNcbiLog_TID tid = 0;
#if defined(NCBI_NO_THREADS)
    tid = 0;
#elif defined(NCBI_OS_UNIX)
#  if defined(SYS_gettid)
    tid = (TNcbiLog_TID)syscall(SYS_gettid); /* NCBI_FAKE_WARNING: Clang */
#  else
    // pthread_self() function returns the pthread handle of the calling thread.
    // On Mac OS X >= 10.6 and iOS >= 3.2 non-portable pthread_getthreadid_np()
    // can be used to return an integral identifier for the thread, 
    // but pthread_self() is good enough for our needs and portable.
    tid = (TNcbiLog_TID)pthread_self(); /* NCBI_FAKE_WARNING */
#  endif
#elif defined(NCBI_OS_MSWIN)
    tid = GetCurrentThreadId();
#endif
    return tid;
}


/** Create unique process ID.
 *  Adapted from C++ Toolkit code: see CDiagContect::x_CreateUID().
 */
static TNcbiLog_UInt8 s_CreateUID(void)
{
    TNcbiLog_PID   pid = s_GetPID();
    TNcbiLog_UInt8 t = (TNcbiLog_UInt8)time(0);
    const char*    host = NcbiLog_GetHostName();
    const char*    s;
    TNcbiLog_UInt8 h = 212;

    /* Use host name */
    if ( host ) {
        for (s = host;  *s != '\0';  s++) {
            h = h*1265 + (unsigned char)(*s);
        }
    }
    h &= 0xFFFF;
    /* The low 4 bits are reserved as GUID generator version number.
       We use version #3. Actually we use C++ version #1 algorithm here, 
       but assign new version number to distinguish the NCBI C logging API
       from the C++ API.
    */
    return (h << 48) |
           ((pid & 0xFFFF) << 32) |
           ((t & 0xFFFFFFF) << 4) |
           3; 
}


/** Update current UID with new timestamp.
 *  This functions don't change global UID, just return a copy.
 *
 * -- not used at this moment, but can be usable later.
 */
#if 0
static TNcbiLog_Int8 s_UpdateUID(void)
{
    time_t t;
    TNcbiLog_Int8 guid;
    /* Update with current timestamp */
    assert(sx_Info->guid);
    guid = sx_Info->guid;
    t = time(0);
    /* Clear old timestamp */
    guid &= ~((TNcbiLog_Int8)0xFFFFFFF << 4);
    /* Add current timestamp */
    guid |= (((TNcbiLog_Int8)t & 0xFFFFFFF) << 4);
    return guid;
}
#endif


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
        *time_ns  = (unsigned long)((double)tp.tv_usec * 1000);
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
    ts = ((double)time_end.sec - (double)time_start.sec) + 
         ((double)time_end.ns  - (double)time_start.ns)/1000000000;
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
    n = snprintf(buf, 24, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
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


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value will be cached for whole process. 
 */
static const char* s_GetClient_Env(void)
{
    static const char* client = NULL;
    char* pv = NULL;
    char* p;
    size_t len;
    int/*bool*/ internal, external;

    if ( client ) {
        return client;
    }
    internal =   (p = getenv("HTTP_CAF_INTERNAL"))  != NULL  &&  *p   ? 1 : 0;
    external = (((p = getenv("HTTP_CAF_EXTERNAL"))  != NULL  &&  *p)  ||
                ((p = getenv("HTTP_NCBI_EXTERNAL")) != NULL  &&  *p)) ? 1 : 0;
    if ( internal  ||  !external ) {
        if ( (p = getenv("HTTP_CLIENT_HOST")) != NULL  &&  *p ) {
            pv = p;
        }
    }
    if ( !pv  &&  (p = getenv("HTTP_CAF_PROXIED_HOST")) != NULL  &&  *p ) {
        pv = p;
    }
    if ( !pv  &&  (p = getenv("PROXIED_IP")) != NULL  &&  *p ) {
        pv = p;
    }
    if ( !pv  &&  (p = getenv("HTTP_X_REAL_IP")) != NULL  &&  *p ) {
        pv = p;
    }
    if ( !pv  &&  (p = getenv("REMOTE_ADDR")) != NULL  &&  *p ) {
        pv = p;
    }

    if (pv) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(pv);
        if (len > NCBILOG_CLIENT_MAX) {
            len = NCBILOG_CLIENT_MAX;
            pv[len] = '\0';
        }
        client = s_StrDupLen(pv, len);
    }
    return client;
}


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value will be cached for whole process. 
 */
extern const char* NcbiLogP_GetSessionID_Env(void)
{
    static const char* session = NULL;
    char* p;
    char* pv = NULL;
    size_t len;

    if ( session ) {
        return session;
    }
    if ( (p = getenv("HTTP_NCBI_SID")) != NULL  &&  *p ) {
        pv = p;
    }
    if ( !pv  &&  (p = getenv("NCBI_LOG_SESSION_ID")) != NULL  &&  *p ) {
        pv = p;
    }
    if (pv) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(pv);
        if (len > NCBILOG_SESSION_MAX) {
            len = NCBILOG_SESSION_MAX;
            pv[len] = '\0';
        }
        session = s_StrDupLen(pv, len);
    }
    return session;
}


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value will be cached for whole process. 
 */
extern const char* NcbiLogP_GetHitID_Env(void)
{
    static const char* phid = NULL;
    char* p;
    char* pv = NULL;
    size_t len;

    if ( phid ) {
        return phid;
    }
    if ( (p = getenv("HTTP_NCBI_PHID")) != NULL  &&  *p ) {
        pv = p;
    }
    if ( !pv  &&  (p = getenv("NCBI_LOG_HIT_ID")) != NULL  &&  *p ) {
        pv = p;
    }
    if (pv) {
        // Check string length to prevent any unexpected buffer overflow later
        len = strlen(pv);
        if (len > NCBILOG_HITID_MAX) {
            len = NCBILOG_HITID_MAX;
            pv[len] = '\0';
        }
        phid = s_StrDupLen(pv, len);
    }
    return phid;
}


/** Get value from numeric-based environment variables.
 */
extern int NcbiLogP_GetEnv_UInt(const char* env_var_name, unsigned int* value)
{
    const char* p;
    if ( (p = getenv(env_var_name)) != NULL  &&  *p ) {
        char* e;
        unsigned long v = strtoul(p, &e, 10);
        if (v <= UINT_MAX  &&  !*e) {
            if (value) {
                *value = (unsigned int)v;
            }
            return 0;
        }
    }
    return -1;
}



/* The URL-encoding table
 */
static const unsigned char sx_EncodeTable[256][4] = {
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
        const unsigned char* subst = sx_EncodeTable[*src];
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


#if 0
-- not used.
-- only s_EscapeNewlines() is used to escape newlines to applog format during printing messages.

/** Get a printable version of the specified string. 
 *  All non-printable characters will be represented as "\r", "\n", "\v",
 *  "\t", etc, or "\ooo" where 'ooo' is the octal code of the character. 
 *
 *  This function read up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the encoded data to buffer "dst_buf", but no more than "dst_size" bytes.
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
        unsigned char subst = 0;

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
            *(++dst) = (unsigned char)('0' + v);
            v = (c >> 3) & 7;
            *(++dst) = (unsigned char)('0' + v);
            v = c & 7;
            *(++dst) = (unsigned char)('0' + v);

            *dst_written += 4;
        }
    }
}
#endif


/** Escape newlines in the string. 
 * 
 *  Follow CXX-7439 applog standard newline replacement convention.
 *  - \n -> \v
 *  - \v -> 0xFF \v
 *  - 0xFF -> 0xFF 0xFF
 *
 *  This function read up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the escaped data to buffer "dst_buf", but no more than "dst_size" bytes.
 *  Assign "*src_read" to the # of bytes successfully escaped from "src_buf".
 *  Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
static void s_EscapeNewlines
    (const void* src_buf,    /* [in]     non-NULL */
     size_t      src_size,   /* [in]              */
     size_t*     src_read,   /* [out]    non-NULL */
     void*       dst_buf,    /* [in/out] non-NULL */
     size_t      dst_size,   /* [in]              */
     size_t*     dst_written /* [out]    non-NULL */
     )
{
    char* src = (char*) src_buf;
    char* dst = (char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
             src++, (*src_read)++, 
             dst++, (*dst_written)++)
    {
        char c = *src;
        switch (c) {
            case '\377':
            case '\v':
                if (*dst_written + 2 > dst_size) {
                    break;
                }
                *dst = '\377';
                dst++;
                (*dst_written)++;
                break;
            case '\n':
                *dst = '\v'; 
                continue;
        }
        *dst = c;
    }
}



/** Get base name of application.
 *  Return a pointer to a newly allocated memory with zero-terminated string.
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
        /* Special case, context for current thread was already destroyed */
        assert(sx_IsInit != 2);
        /* Create context */
        ctx = s_CreateContext();
        VERIFY(s_AttachContext(ctx));
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
    static const char  kSectionName[] = "[Web_dir_to_port]";

    FILE*  fp;
    char   buf[FILENAME_MAX];
    char   *line, *token, *p;
    int    inside_section = 0 /*false*/;
    size_t line_size;

    if (log_path) {
        return log_path;
    }

    /* Try to get more specific location from configuration file */
    fp = fopen("/etc/toolkitrc", "rt");
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
            *p = '\0';

            /* Compose full directory name */
            {{
                char path[FILENAME_MAX + 1];
                s_ConcatPath(kBaseLogDir, token, path, FILENAME_MAX + 1);
                log_path = s_StrDup(path);
                break;
            }}
            /* continue to next line */
        }
        if (memcmp(line, kSectionName, sizeof(kSectionName)) >= 0) {
            inside_section = 1;
        }
    }
    fclose(fp);

    return log_path;
}


/** Close log files for a current destination. 
    Files can be fully cleaned for setting a new destination or just
    closed to be reopened later with the same destination.
 */
static void s_CloseLogFiles(int/*bool*/ cleanup /* CLOSE_CLEANUP / CLOSE_FOR_REOPEN */) 
{
    /* Current destination */
    
    switch (sx_Info->destination) {
        case eNcbiLog_Default:
        case eNcbiLog_Stdlog:
        case eNcbiLog_Cwd:
        case eNcbiLog_File:
            /* Close files, see below */
            break;
        case eNcbiLog_Stdout:
        case eNcbiLog_Stderr:
        case eNcbiLog_Disable:
            sx_Info->file_log   = kInvalidFileHandle;
            sx_Info->file_trace = kInvalidFileHandle;
            sx_Info->file_err   = kInvalidFileHandle;
            sx_Info->file_perf  = kInvalidFileHandle;
            return;
        default:
            TROUBLE_MSG("unknown destination");
    }

    /* Close files, if any are open */

    if (sx_Info->file_log != kInvalidFileHandle) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_log);
#else
        fclose(sx_Info->file_log);
#endif
        sx_Info->file_log = kInvalidFileHandle;
    }

    if (sx_Info->file_trace != kInvalidFileHandle) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_trace);
#else
        fclose(sx_Info->file_trace);
#endif
        sx_Info->file_trace = kInvalidFileHandle;
    }

    if (sx_Info->file_err != kInvalidFileHandle) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_err);
#else
        fclose(sx_Info->file_err);
#endif
        sx_Info->file_err = kInvalidFileHandle;
    }

    if (sx_Info->file_perf != kInvalidFileHandle) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_perf);
#else
        fclose(sx_Info->file_perf);
#endif
        sx_Info->file_perf = kInvalidFileHandle;
    }

    if (cleanup  &&  sx_Info->file_log_name) {
        free(sx_Info->file_log_name);
        sx_Info->file_log_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_trace_name) {
        free(sx_Info->file_trace_name);
        sx_Info->file_trace_name = NULL;
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


/** Open file streams.
 *  All file names should be already defined.
 */
static int /*bool*/ s_OpenLogFiles(void)
{
#if NCBILOG_USE_FILE_DESCRIPTORS
    int flags = O_CREAT | O_APPEND | O_WRONLY;
    mode_t mode = 0664;
#endif    
    assert(sx_Info->file_log_name   &&
           sx_Info->file_trace_name &&
           sx_Info->file_err_name   &&
           sx_Info->file_perf_name);

    assert((sx_Info->file_log   == kInvalidFileHandle)  &&
           (sx_Info->file_trace == kInvalidFileHandle)  &&
           (sx_Info->file_err   == kInvalidFileHandle)  &&
           (sx_Info->file_perf  == kInvalidFileHandle));

    /* Log */
#if NCBILOG_USE_FILE_DESCRIPTORS
    sx_Info->file_log = open(sx_Info->file_log_name, flags, mode);
#else
    sx_Info->file_log = fopen(sx_Info->file_log_name, "a");
#endif        
    if (sx_Info->file_log == kInvalidFileHandle) {
        return 0 /*false*/;
    }

    /* Trace */
    if (sx_Info->split_log_file) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_trace = open(sx_Info->file_trace_name, flags, mode);
#else
        sx_Info->file_trace = fopen(sx_Info->file_trace_name, "a");
#endif    
        if (sx_Info->file_trace == kInvalidFileHandle) {
            return 0 /*false*/;
        }
    } else {
        sx_Info->file_trace = sx_Info->file_log;
    }

    /* Err */
    if (sx_Info->split_log_file) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_err = open(sx_Info->file_err_name, flags, mode);
#else
        sx_Info->file_err = fopen(sx_Info->file_err_name, "a");
#endif        
        if (sx_Info->file_err == kInvalidFileHandle) {
            return 0 /*false*/;
        }
    } else {
        sx_Info->file_err = sx_Info->file_log;
    }

    /* Perf */
    if (sx_Info->split_log_file || sx_Info->is_applog) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_perf = open(sx_Info->file_perf_name, flags, mode);
#else
        sx_Info->file_perf = fopen(sx_Info->file_perf_name, "a");
#endif        
        if (sx_Info->file_perf == kInvalidFileHandle) {
            return 0 /*false*/;
        }
    } else {
        sx_Info->file_perf = sx_Info->file_log;
    }

    return 1 /*true*/;
}


/** (Re)Initialize logging file streams.
 *  The path passed in parameter should have directory and base name for logging files.
 */
static int /*bool*/ s_SetLogFiles(const char* path_with_base_name, int/*bool*/ is_applog) 
{
    char path[FILENAME_MAX + 1];
    size_t n;

    assert(path_with_base_name);
    /* Check max possible file name (path.trace) */
    n = strlen(path_with_base_name);
    assert(n);
    assert((n + 5 /*.perf*/) <= FILENAME_MAX);
    if (!n || ((n + 5 /*.perf*/) > FILENAME_MAX) ) {
        // path is too long
        return 0 /*false*/;
    }
    memcpy(path, path_with_base_name, n);

    strcpy(path + n, ".log");
    sx_Info->file_log_name = s_StrDup(path);
    strcpy(path + n, ".trace");
    sx_Info->file_trace_name = s_StrDup(path);
    strcpy(path + n, ".err");
    sx_Info->file_err_name = s_StrDup(path);
    strcpy(path + n, ".perf");
    sx_Info->file_perf_name = s_StrDup(path);

    /* Open files */
    sx_Info->is_applog = is_applog;
    if (!s_OpenLogFiles()) {
        s_CloseLogFiles(CLOSE_CLEANUP);
        return 0 /*false*/;
    }
    sx_Info->reuse_file_names = 1;
    return 1 /*true*/;
}


/** (Re)Initialize logging file streams.
 *  Use application base name and specified directory as path for logging files.
 */
static int /*bool*/ s_SetLogFilesDir(const char* dir, int/*bool*/ is_applog) 
{
    char path[FILENAME_MAX + 1];
#if defined(NCBI_OS_UNIX)
    char filename_buf[FILENAME_MAX + 1];
#endif
    const char* filename = NULL;
    size_t n, filelen;

    assert(dir);
    n = strlen(dir);
    assert(n);

    /* When using applog, create separate log file for each user */
    /* to avoid permission problems */

#if 0
 -- GUID approach for file names
    if (is_applog) {
        int nbuf;
        int x_guid_hi, x_guid_lo;
        /* sprintf() overflow guard */
        if (strlen(sx_Info->app_base_name) 
            + 1    /* . */
            + 16   /* guid string */
            >= FILENAME_MAX) {
            return 0;
        }
        x_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
        x_guid_lo = (int) (sx_Info->guid & 0xFFFFFFFF);
        nbuf = sprintf(filename_buf, "%s.%08X%08X", sx_Info->app_base_name, x_guid_hi, x_guid_lo);
        if (nbuf <= 0) {
            return 0;
        }
        filelen  = (size_t)nbuf;
        filename = filename_buf;
    }
#endif

#if defined(NCBI_OS_UNIX)
    if (is_applog) {
        int nbuf;
        /* sprintf() overflow guard */
        if (strlen(sx_Info->app_base_name) 
            + 1    /* . */
            + 10   /* max(uid_t) ~ MAX_UINT32 ~ 4,294,967,295 */
            >= FILENAME_MAX) {
            return 0;
        }
        nbuf = snprintf(filename_buf, FILENAME_MAX+1, "%s.%d", sx_Info->app_base_name, geteuid());
        if (nbuf <= 0  ||  nbuf > FILENAME_MAX) {
            return 0;
        }
        filelen  = (size_t)nbuf;
        filename = filename_buf;
    }
#endif
    if (!filename) {
        filename = sx_Info->app_base_name;
        filelen  = strlen(filename);
        assert(filelen);
        if (!filelen) {
            return 0 /* false */;
        }
    }
    
    /* Check max possible file name (dir/basename.trace) */
    assert((n + 1 + filelen + 5) <= sizeof(path));
    if ((n + 1 + filelen + 5) > sizeof(path)) {
        return 0 /* false */;
    }
    s_ConcatPathEx(dir, n, filename, filelen, path, FILENAME_MAX + 1);

    /* Set logging files */
    return s_SetLogFiles(path, is_applog);
}


/** (Re)Initialize destination.
 */
static void s_InitDestination(const char* logfile_path) 
{
    time_t now;
    assert(sx_Info->destination != eNcbiLog_Disable);
    
#if NCBILOG_USE_FILE_DESCRIPTORS
    if (sx_Info->file_log == STDOUT_FILENO  ||  sx_Info->file_log == STDERR_FILENO) {
        /* We already have destination set to stdout/stderr -- don't need to reopen it */
        return;
    }
#else
    if (sx_Info->file_log == stdout  ||  sx_Info->file_log == stderr) {
        /* We already have destination set to stdout/stderr -- don't need to reopen it */
        return;
    }
#endif

#if 0
    -- use regular behavior with reopening user files as well

    /* We already have logging set to user defined file(s) -- don't need to reopen it */
    if (sx_Info->destination == eNcbiLog_File  &&  
        sx_Info->file_log != kInvalidFileHandle) {
        return;
    }
#endif    

    /* Reopen all files every 1 minute */

    time(&now);
    if (now - sx_Info->last_reopen_time < 60) {
        return;
    }
    sx_Info->last_reopen_time = now;

    /* Try to open files */

    if ((sx_Info->destination == eNcbiLog_Default  ||
         sx_Info->destination == eNcbiLog_File)  &&  logfile_path) {
        /* special case to redirect all logging to specified files */
        s_CloseLogFiles(CLOSE_CLEANUP);
        if (s_SetLogFiles(logfile_path, NO_LOG)) {
            return;
        }
        /* Error: disable logging.
           The error reporting should be done on an upper level(s) outside of the CLog.
        */
        sx_Info->destination = eNcbiLog_Disable;
        return;
    }

    /* Close */
    s_CloseLogFiles(CLOSE_FOR_REOPEN);
    
    if (sx_Info->destination == eNcbiLog_Default  ||
        sx_Info->destination == eNcbiLog_File     ||
        sx_Info->destination == eNcbiLog_Stdlog   ||
        sx_Info->destination == eNcbiLog_Cwd) {

        /* Destination and file names didn't changed, just reopen files */
        if (sx_Info->reuse_file_names) {
            if (s_OpenLogFiles()) {
                return;
            }
            /* Error: failed to reopen */
            s_CloseLogFiles(CLOSE_CLEANUP);
            /* Will try again from scratch (below), can lead to change the logging location */
        }
        /* Reopening failed. For a local "file" logging this is an error -- disable logging */
        if (sx_Info->destination == eNcbiLog_File) {
            sx_Info->destination = eNcbiLog_Disable;
            return;
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
                    if (s_SetLogFilesDir(dir, TO_LOG)) {
                        return;
                    }
                }
                /* server port */
                if (sx_Info->server_port) {
                    int n = snprintf(xdir, FILENAME_MAX+1, "%s%d", kBaseLogDir, sx_Info->server_port);
                    VERIFY(n > 0  &&  n <= FILENAME_MAX);
                    if (s_SetLogFilesDir(xdir, TO_LOG)) {
                        return;
                    }
                }

                /* /log/srv */ 
                dir = s_ConcatPath(kBaseLogDir, "srv", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir, TO_LOG)) {
                        return;
                    }
                }
                /* /log/fallback */
                dir = s_ConcatPath(kBaseLogDir, "fallback", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir, TO_LOG)) {
                        return;
                    }
                }
                /* /log/{{logsite}} */
                if (sx_Info->logsite  &&  sx_Info->logsite[0] != '\0') {
                    dir = s_ConcatPath(kBaseLogDir, sx_Info->logsite, xdir, FILENAME_MAX + 1);
                    if (dir) {
                        if (s_SetLogFilesDir(dir, TO_LOG)) {
                            return;
                        }
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
                if (cwd  &&  s_SetLogFilesDir(cwd, NO_LOG)) {
                    sx_Info->destination = eNcbiLog_Cwd;
                    free(cwd);
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
#if NCBILOG_USE_FILE_DESCRIPTORS
            sx_Info->file_trace = STDOUT_FILENO;
            sx_Info->file_log   = STDOUT_FILENO;
            sx_Info->file_err   = STDOUT_FILENO;
            sx_Info->file_perf  = STDOUT_FILENO;
#else
            sx_Info->file_trace = stdout;
            sx_Info->file_log   = stdout;
            sx_Info->file_err   = stdout;
            sx_Info->file_perf  = stdout;
#endif
            break;
        case eNcbiLog_Stderr:
#if NCBILOG_USE_FILE_DESCRIPTORS
            sx_Info->file_trace = STDERR_FILENO;
            sx_Info->file_log   = STDERR_FILENO;
            sx_Info->file_err   = STDERR_FILENO;
            sx_Info->file_perf  = STDERR_FILENO;
#else
            sx_Info->file_trace = stderr;
            sx_Info->file_log   = stderr;
            sx_Info->file_err   = stderr;
            sx_Info->file_perf  = stderr;
#endif
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


static void s_SetClient(char* dst, const char* client)
{
    if (client  &&  *client) {
        size_t len;
        len = strlen(client);
        if (len > NCBILOG_CLIENT_MAX) { 
            len = NCBILOG_CLIENT_MAX;
        }
        memcpy(dst, client, len);
        dst[len] = '\0';
    } else {
        dst[0] = '\0';
    }
}


static void s_SetSession(char* dst, const char* session)
{
    if (session  &&  *session) {
        /* URL-encode */
        size_t len, r_len, w_len;
        len = strlen(session);
        s_URL_Encode(session, len, &r_len, dst, 3*NCBILOG_SESSION_MAX, &w_len);
        verify(len == r_len);
        dst[w_len] = '\0';
    } else {
        dst[0] = '\0';
    }
}


static void s_SetHitID(char* dst, const char* hit_id)
{
    if (hit_id  &&  *hit_id) {
        /*TODO: Validate hit ID ?? */
        /* URL-encode */
        size_t len, r_len, w_len;
        len = strlen(hit_id);
        s_URL_Encode(hit_id, len, &r_len, dst, 3*NCBILOG_HITID_MAX, &w_len);
        verify(len == r_len);
        dst[w_len] = '\0';
    } else {
        dst[0] = '\0';
    }
}


static int/*bool*/ s_IsInsideRequest(TNcbiLog_Context ctx)
{
    ENcbiLog_AppState state;

    /* Get application state (context-specific) */
    state = (ctx->state == eNcbiLog_NotSet) ? sx_Info->state : ctx->state;
    switch (state) {
    case eNcbiLog_RequestBegin:
    case eNcbiLog_Request:
        return 1;
    default:
        break;
    }
    return 0;
}


static char* s_GenerateSID_Str_Ex(char* dst, int /*bool*/ use_logging_api, TNcbiLog_UInt8 uid)
{
    TNcbiLog_UInt8 rid;
    int hi, lo, n;

    if (use_logging_api) {
        if (!sx_Info->guid) {
            sx_Info->guid = uid ? uid : s_CreateUID();
        }
        uid = sx_Info->guid;
        rid = sx_Info->rid;
    } else {
        uid = uid ? uid : s_CreateUID();
        rid = 0;
    }
    hi = (int)((uid >> 32) & 0xFFFFFFFF);
    lo = (int) (uid & 0xFFFFFFFF);
    n = sprintf(dst, "%08X%08X_%04" NCBILOG_UINT8_FORMAT_SPEC "SID", hi, lo, rid);
    if (n <= 0) {
        return NULL;
    }
    return dst;
}


static char* s_GenerateSID_Str(char* dst)
{
    return s_GenerateSID_Str_Ex(dst, 1 /*api call*/, 0 /*autogenerated UID*/);
}


static char* s_GenerateHitID_Str_Ex(char* dst, int /*bool*/ is_api_call, TNcbiLog_UInt8 uid)
{
    TNcbiLog_UInt8 tid, rid, us, hi, lo;
    unsigned int   b0, b1, b2, b3;
    time_t         time_sec;
    unsigned long  time_ns;
    int n;

    if (is_api_call) {
        if (!sx_Info->guid) {
            sx_Info->guid = uid ? uid : s_CreateUID();
        }
        hi  = sx_Info->guid;
        rid = (TNcbiLog_UInt8)(sx_Info->rid & 0xFFFFFF) << 16;
    } else {
        // some external call, create a new UID especially for caller
        hi = uid ? uid : s_CreateUID();
        rid = 0;
    }
    b3  = (unsigned int)((hi >> 32) & 0xFFFFFFFF);
    b2  = (unsigned int)(hi & 0xFFFFFFFF);
    tid = (s_GetTID() & 0xFFFFFF) << 40;
    if (!s_GetTimeT(&time_sec, &time_ns)) {
        us = 0;
    } else {
        us = (time_ns/1000/16) & 0xFFFF;
    }
    lo = tid | rid | us;
    b1 = (unsigned int)((lo >> 32) & 0xFFFFFFFF);
    b0 = (unsigned int)(lo & 0xFFFFFFFF);
    n = sprintf(dst, "%08X%08X%08X%08X", b3, b2, b1, b0);
    if (n <= 0) {
        return NULL;
    }
    return dst;
}


static char* s_GenerateHitID_Str(char* dst)
{
    return s_GenerateHitID_Str_Ex(dst, 1 /*api call*/, 0 /*autogenerated UID*/);
}


/** Generate new unique process ID.
 */
extern TNcbiLog_UInt8 NcbiLogP_GenerateUID(void)
{
    return s_CreateUID();
}


/** Generate new Hit ID string.
 */
extern const char* NcbiLogP_GenerateHitID(char* buf, size_t n, TNcbiLog_UInt8 uid)
{
    if (n <= NCBILOG_HITID_MAX) {
        return NULL;
    }
    if (!s_GenerateHitID_Str_Ex(buf, 0 /*external call*/, uid)) {
        return NULL;
    }
    return buf;
}


/** Generate new SID string.
 */
extern const char* NcbiLogP_GenerateSID(char* buf, size_t n, TNcbiLog_UInt8 uid)
{
    if (n <= NCBILOG_SESSION_MAX) {
        return NULL;
    }
    if (!s_GenerateSID_Str_Ex(buf, 0 /*ext api call*/, uid)) {
        return NULL;
    }
    return buf;
}




/******************************************************************************
 *  Print 
 */

#if NCBILOG_USE_FILE_DESCRIPTORS

static size_t s_Write(int fd, const void *buf, size_t count)
{
    const char* ptr = (const char*) buf;
    size_t  n = count;
    ssize_t n_written;
    
    if (count == 0) {
        return 0;
    }
    do {
        n_written = write(fd, ptr, n);
        if (n_written == 0) {
            return count - n;
        }
        if ( n_written < 0 ) {
            if (errno == EINTR) {
                continue;
            }
            return count - n;
        }
        n   -= (size_t)n_written;
        ptr += n_written;
    }
    while (n > 0);
    return count;
}

#endif


/** Post prepared message into the log.
 *  The severity parameter is necessary to choose correct logging file.
 */
static void s_Post(TNcbiLog_Context ctx, ENcbiLog_DiagFile diag)
{
    TFileHandle f = kInvalidFileHandle;
    size_t n_write;
#if NCBILOG_USE_FILE_DESCRIPTORS
    size_t n;
#else    
    int n;
#endif
   
    if (sx_Info->destination == eNcbiLog_Disable) {
        return;
    }
    /* Reopen logging files, if necessary */
    s_InitDestination(NULL);
    /* Destination can be disabled on reopening, so check again */
    if (sx_Info->destination == eNcbiLog_Disable) {
        return;
    }

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
    
    /* Write */
    assert(f != kInvalidFileHandle);

    n_write = strlen(sx_Info->message);
    VERIFY_CATCH(n_write <= NCBILOG_ENTRY_MAX);

#if NCBILOG_USE_FILE_DESCRIPTORS
    sx_Info->message[n_write] = '\n';
    n_write++;
    sx_Info->message[n_write] = '\0';
    n = s_Write(f, sx_Info->message, n_write);
    VERIFY(n == n_write);
    /* fdatasync(f); */
#else
    n = fprintf(f, "%s\n", sx_Info->message);
    VERIFY(n == n_write + 1);
    fflush(f);
#endif

    /* Increase posting serial numbers */
    sx_Info->psn++;
    ctx->tsn++;

    /* Reset posting time if no user time defined */
    if (sx_Info->user_posting_time == 0) {
        sx_Info->post_time.sec = 0;
        sx_Info->post_time.ns  = 0;
    }

CATCH:
    return;
}


/* Symbols abbreviation for application state, see ENcbiLog_AppState */
static const char* sx_AppStateStr[] = {
    "NS", "PB", "P", "PE", "RB", "R", "RE"
};

/** Print common prefix to message buffer.
 *  Return number of written bytes (current position in message buffer), or zero on error.
 *  See documentation about log format:
 *    https://ncbi.github.io/cxx-toolkit/pages/ch_log#ch_core.The_New_Post_Format
 *  C++ implementation: CDiagContext::WriteStdPrefix() (src/corelib/ncbidiag.cpp)
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
    int               inside_request;

    /* Calculate GUID */
    if ( !sx_Info->guid ) {
        sx_Info->guid = s_CreateUID();
    }
    x_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
    x_guid_lo = (int) (sx_Info->guid & 0xFFFFFFFF);

    /* Get application state (context-specific) */
    x_st = (ctx->state == eNcbiLog_NotSet) ? sx_Info->state : ctx->state;
    x_state = sx_AppStateStr[x_st];
    switch (x_st) {
        case eNcbiLog_RequestBegin:
        case eNcbiLog_Request:
        case eNcbiLog_RequestEnd:
            inside_request = 1;
            break;
        default:
            inside_request = 0;
    }

    /* Get posting time string (current or specified by user) */
    if (!sx_Info->post_time.sec) {
        if (!s_GetTime((STime*)&sx_Info->post_time)) {
            /* error */
            return 0;
        }
    }
    if (!s_GetTimeStr(x_time, sx_Info->post_time.sec, sx_Info->post_time.ns)) {
        return 0;  /* error */
    }

    /* Define properties */

    if (!sx_Info->host[0]) {
        s_SetHost(NcbiLog_GetHostName());
    }
    x_pid     = sx_Info->pid        ? sx_Info->pid            : s_GetPID();
    x_rid     = ctx->rid            ? ctx->rid                : 0;
    x_host    = sx_Info->host[0]    ? (char*)sx_Info->host    : UNKNOWN_HOST;
    x_appname = sx_Info->appname[0] ? (char*)sx_Info->appname : UNKNOWN_APPNAME;

    /* client */
    if (inside_request  &&  ctx->is_client_set) {
        x_client = ctx->client[0] ? (char*)ctx->client : UNKNOWN_CLIENT;
    } else {
        x_client = sx_Info->client[0] ? (char*)sx_Info->client : UNKNOWN_CLIENT;
    }
    /* session */
    if (inside_request  &&  ctx->is_session_set) {
        x_session = ctx->session[0] ? (char*)ctx->session : UNKNOWN_SESSION;
    } else {
        x_session = sx_Info->session[0] ? (char*)sx_Info->session : UNKNOWN_SESSION;
    }

    n = snprintf(sx_Info->message, NCBILOG_ENTRY_MAX,
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
    assert(n > 0  &&  n < NCBILOG_ENTRY_MAX);
    if (n <= 0 || n >= NCBILOG_ENTRY_MAX) {
        return 0;  /* error */
    }
    return (size_t)n;
}


/** Add parameters pair to the list if not empty. Update and return index.
 *  Because this method is for internal usage only, to print automatically
 *  logged extra parameters, neither key or value can be empty here.
 */
static int s_AddParamsPair(SNcbiLog_Param params[], int index, const char* key, const char* value)
{
    assert(key || key[0]);
    if (!value || !value[0]) {
        return index;
    }
    params[index].key   = key;
    params[index].value = value;
    return ++index;
}


/** Print one parameter's pair (key, value) to message buffer.
 *  Automatically URL encode each key and value.
 *  Return new position in the buffer.
 */
static size_t s_PrintParamsPair(char* dst, size_t pos, const char* key, const char* value)
{
    size_t len, r_len, w_len;

    if (pos >= NCBILOG_ENTRY_MAX) {
        return pos; /* error, pre-check on overflow */
    }

    /* Key */
    if (!key  ||  key[0] == '\0') {
        return pos;  /* error */
    }
    len = strlen(key);
    s_URL_Encode(key, len, &r_len, dst + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    if (pos > NCBILOG_ENTRY_MAX-2) {
        /* we should have space at least for 2 chars '=<value>' */
        return pos;  /* error, overflow */
    }

    /* Value */
    dst[pos++] = '=';
    if (!value  ||  value[0] == '\0') {
        return pos;
    }
    len = strlen(value);
    s_URL_Encode(value, len, &r_len, dst + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    if (pos > NCBILOG_ENTRY_MAX) {
        return pos;  /* error, overflow */
    }
    dst[pos] = '\0';

    return pos;
}


/** Print parameters to message buffer.
 *  Return new position in the buffer.
 */
static size_t s_PrintParams(char* dst, size_t pos, const SNcbiLog_Param* params)
{
    int/*bool*/ amp = 0;
    size_t      new_pos = 0;

    /* Walk through the list of arguments */
    if (params) {
        while (params  &&  params->key  &&  pos < NCBILOG_ENTRY_MAX) {
            if (params->key[0] != '\0') {
                if (amp) {
                    dst[pos++] = '&';
                } else {
                    amp = 1;
                }
                /* Write 'whole' pair or ignore */
                new_pos = s_PrintParamsPair(dst, pos, params->key, params->value);
                /* Check on overflow */
                VERIFY_CATCH(new_pos <= NCBILOG_ENTRY_MAX);  
                pos = new_pos;
            }
            params++;
        }
    }

CATCH:
    dst[pos] = '\0';
    return pos;
}


/** Print parameters to message buffer (string version).
 *  The 'params' must be already prepared pairs with URL-encoded key and value.
 *  Return new position in the buffer.
 */
static size_t s_PrintParamsStr(char* dst, size_t pos, const char* params)
{
    size_t len;

    if (!params) {
        return pos;
    }
    /* Check on overflow */
    VERIFY_CATCH(pos < NCBILOG_ENTRY_MAX);
    len = min_value(strlen(params), NCBILOG_ENTRY_MAX-pos);
    memcpy(dst + pos, params, len);
    pos += len;
    dst[pos] = '\0';

CATCH:
    return pos;
}


/* Severity string representation  */
static const char* sx_SeverityStr[] = {
    "Trace", "Info", "Warning", "Error", "Critical", "Fatal" 
};

/** Print a message with severity to message buffer.
 */
static void s_PrintMessage(ENcbiLog_Severity severity, const char* msg, int/*bool*/ print_as_note)
{
    TNcbiLog_Context  ctx;
    size_t            pos, r_len, w_len;
    char*             buf;
    int               n;
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
    VERIFY_CATCH(pos);

    /* Severity */
    if (print_as_note) {
        n = sprintf(buf + pos, "Note[%c]: ", sx_SeverityStr[severity][0]);
    } else {
        n = sprintf(buf + pos, "%s: ", sx_SeverityStr[severity]);
    }
    VERIFY_CATCH(n > 0);
    pos += (size_t)n;

    /* Message */
    s_EscapeNewlines(msg, strlen(msg), &r_len, buf + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    buf[pos] = '\0';

    /* Post a message */
    s_Post(ctx, diag);

CATCH:
    MT_UNLOCK;
    if (severity == eNcbiLog_Fatal) {
        s_Abort(0,0); /* no additional error reporting */
    }
    return;
 }



/******************************************************************************
 *  NcbiLog
 */

static void s_Init(const char* appname) 
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
    sx_Info->file_log   = kInvalidFileHandle;
    sx_Info->file_trace = kInvalidFileHandle;
    sx_Info->file_err   = kInvalidFileHandle;
    sx_Info->file_perf  = kInvalidFileHandle;

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
    verify(len == r_len);
    sx_Info->appname[w_len] = '\0';

    /* Allocate memory for message buffer */
    sx_Info->message = (char*) malloc(NCBILOG_ENTRY_MAX_ALLOC);
    assert(sx_Info->message);

    /* Try to get LOG_ISSUED_SUBHIT_LIMIT value, see docs for it */
    {{
        unsigned int v; /* necessary to avoid compilation warnings */
        if (NcbiLogP_GetEnv_UInt("LOG_ISSUED_SUBHIT_LIMIT", &v) == 0) {
            sx_Info->phid_sub_id_limit = v;
        } else {
            /* Not defined, use default, same as for C++ toolkit */
            sx_Info->phid_sub_id_limit = 200; 
        }
     }}

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
        VERIFY(!sx_IsInit);
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
    s_CloseLogFiles(CLOSE_CLEANUP);

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


extern void NcbiLogP_ReInit(void)
{
    sx_IsInit = 0;
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


extern ENcbiLog_Destination 
NcbiLog_SetDestination(ENcbiLog_Destination ds)
{
    char* logfile = NULL;

    MT_LOCK_API;
    /* Close current destination files, if any */
    s_CloseLogFiles(CLOSE_CLEANUP);

    /* Server port */
    if (!sx_Info->server_port) {
        const char* port_str = getenv("SERVER_PORT");
        if (port_str  &&  *port_str) {
            char* e;
            unsigned long port = strtoul(port_str, &e, 10);
            if (port > 0  &&  port <= NCBILOG_PORT_MAX  &&  !*e) {
                sx_Info->server_port = (unsigned int)port;
            }
        }
    }
    /* Set new destination */
    sx_Info->destination = ds;
    if (sx_Info->destination != eNcbiLog_Disable) {
        /* and force to initialize it */
        sx_Info->last_reopen_time = 0;
        if (sx_Info->destination == eNcbiLog_Default) {
            /* Special case to redirect default logging output */
            logfile = getenv("NCBI_CONFIG__LOG__FILE");
            /* logfile value will have a sanity check at s_InitDestination(), if needed */
            if (logfile) {
                if (!*logfile) {
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
        /* Read destination back, it can change */
        ds = sx_Info->destination;
    }
    MT_UNLOCK;
    return ds;
}


extern ENcbiLog_Destination 
NcbiLog_SetDestinationFile(const char* logfile_base)
{
    const char* logfile = logfile_base;
    ENcbiLog_Destination ds = eNcbiLog_File;
    
    MT_LOCK_API;
    /* Close current destination */
    s_CloseLogFiles(CLOSE_CLEANUP);
   
    if (!logfile  &&  !*logfile ) {
        sx_Info->destination = eNcbiLog_Disable; /* error */
        MT_UNLOCK;
        return sx_Info->destination;
    }
    /* Special case to redirect file logging output to stderr */
    if (strcmp(logfile, "-") == 0) {
        ds = eNcbiLog_Stderr;
        logfile = NULL;
    }
    /* Set new destination */
    sx_Info->destination = ds;
    sx_Info->last_reopen_time = 0;
    s_InitDestination(logfile);

    /* Read destination back, it can change */
    ds = sx_Info->destination;
   
    MT_UNLOCK;
    return ds;
}


extern ENcbiLog_Destination 
NcbiLogP_SetDestination(ENcbiLog_Destination ds,
                        unsigned int port, const char* logsite)
{
    char* logfile = NULL;
    MT_LOCK_API;
    
    /* NOTE: 
       this is an internal version especially for ncbi_applog, so no need
       to close previous destination, befor setting new one.
    */
    
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
    sx_Info->logsite     = logsite;
    if (sx_Info->destination != eNcbiLog_Disable) {
        /* and force to initialize it */
        sx_Info->last_reopen_time = 0;
        s_InitDestination(logfile);
    }
    ds = sx_Info->destination;
    MT_UNLOCK;
    return ds;
}


extern void NcbiLog_SetSplitLogFile(int /*bool*/ value)
{
    MT_LOCK_API;
    sx_Info->split_log_file = value;
    MT_UNLOCK;
}


extern void NcbiLog_SetProcessId(TNcbiLog_PID pid)
{
    MT_LOCK_API;
    sx_Info->pid = pid;
    MT_UNLOCK;
}


extern void NcbiLog_SetThreadId(TNcbiLog_TID tid)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    ctx->tid = tid;
    MT_UNLOCK;
}


extern void NcbiLog_SetRequestId(TNcbiLog_Counter rid)
{
    MT_LOCK_API;
    /* Enforce setting request id only after NcbiLog_AppRun() */
    if (sx_Info->state == eNcbiLog_NotSet  ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_SetRequestId() can be used after NcbiLog_AppRun() only");
    }
    sx_Info->rid = rid;
    MT_UNLOCK;
}


extern TNcbiLog_Counter NcbiLog_GetRequestId(void)
{
    TNcbiLog_Counter rid;
    MT_LOCK_API;
    rid = sx_Info->rid;
    MT_UNLOCK;
    return rid;
}


extern void NcbiLog_SetTime(time_t time_sec, unsigned long time_ns)
{
    MT_LOCK_API;
    sx_Info->post_time.sec = time_sec;
    sx_Info->post_time.ns  = time_ns;
    sx_Info->user_posting_time = (time_sec || time_ns ? 1 : 0);
    MT_UNLOCK;
}


extern void NcbiLog_SetHost(const char* host)
{
    MT_LOCK_API;
    s_SetHost(host);
    MT_UNLOCK;
}


extern void NcbiLog_AppSetClient(const char* client)
{
    MT_LOCK_API;
    s_SetClient((char*)sx_Info->client, client);
    MT_UNLOCK;
}


extern void NcbiLog_SetClient(const char* client)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_SetClient(ctx->client, client);
    ctx->is_client_set = 1;
    MT_UNLOCK;
}


extern void NcbiLog_AppSetSession(const char* session)
{
    MT_LOCK_API;
    s_SetSession((char*)sx_Info->session, session);
    MT_UNLOCK;
}


extern char* NcbiLog_AppGetSession(void)
{
    char* sid = NULL;

    MT_LOCK_API;
    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state == eNcbiLog_NotSet ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_AppGetSession() can be used after NcbiLog_AppStart() only");
    }
    if (sx_Info->session[0]) {
        sid = s_StrDup((char*)sx_Info->session);
    }
    MT_UNLOCK;

    return sid;
}


extern char* NcbiLog_GetSession(void)
{
    TNcbiLog_Context ctx = NULL;
    char* sid = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();

    if ( s_IsInsideRequest(ctx) ) {
        if (ctx->session[0]) {
            sid = s_StrDup(ctx->session);
        } else {
            if (sx_Info->session[0]) {
                sid = s_StrDup((char*)sx_Info->session);
            }
        }
    }
    MT_UNLOCK;
    return sid;
}


extern void NcbiLog_SetSession(const char* session)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_SetSession(ctx->session, session);
    ctx->is_session_set = 1;
    MT_UNLOCK;
}


extern void NcbiLog_AppNewSession(void)
{
    char session[NCBILOG_SESSION_MAX+1]; 
    MT_LOCK_API;
    s_SetSession((char*)sx_Info->session, s_GenerateSID_Str(session));
    MT_UNLOCK;
}


extern void NcbiLog_NewSession(void)
{
    char session[NCBILOG_SESSION_MAX+1]; 
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_SetSession(ctx->session, s_GenerateSID_Str(session));
    ctx->is_session_set = 1;
    MT_UNLOCK;
}


/* Log request/app-wide hit ID. See NcbiLog_GetNextSubHitID().
 */
static void s_LogHitID(TNcbiLog_Context ctx, const char* hit_id)
{
    SNcbiLog_Param params[2];
    assert(hit_id  &&  hit_id[0]);
    int i = s_AddParamsPair(params, 0, "ncbi_phid", hit_id);
    params[i].key   = NULL;
    params[i].value = NULL;
    s_Extra(ctx, params);
}

static void s_LogSubHitID(TNcbiLog_Context ctx, const char* subhit_id)
{
    SNcbiLog_Param params[2];
    assert(subhit_id  &&  subhit_id[0]);
    int i = s_AddParamsPair(params, 0, "issued_subhit", subhit_id);
    params[i].key   = NULL;
    params[i].value = NULL;
    s_Extra(ctx, params);
}


extern void NcbiLog_AppSetHitID(const char* hit_id)
{
    MT_LOCK_API;
    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state != eNcbiLog_NotSet  &&
        sx_Info->state != eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_AppSetHitID() can be used before NcbiLog_App[Start/Run]() only");
    }
    s_SetHitID((char*)sx_Info->phid, hit_id);
    sx_Info->phid_inherit = 1;
    MT_UNLOCK;
}


extern void NcbiLog_SetHitID(const char* hit_id)
{
    TNcbiLog_Context ctx = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();

    /* Log new request-specific hit ID immediately if inside request
       or just save it for very next request.
    */
    if (hit_id  &&  s_IsInsideRequest(ctx)) {
        /* if not the same */
        if ( !(ctx->phid[0]  &&  strcmp(ctx->phid, hit_id) == 0) ) {
            s_LogHitID(ctx, hit_id);
        }
    } else {
        s_SetHitID(ctx->phid, hit_id);
    }
    MT_UNLOCK;
}


/* @deprecated */
extern void NcbiLog_AppNewHitID(void)
{
    return;
}


/* @deprecated */
extern void NcbiLog_NewHitID(void)
{
    return;
}


extern char* NcbiLog_AppGetHitID(void)
{
    char* phid = NULL;

    MT_LOCK_API;
    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state == eNcbiLog_NotSet ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_AppGetHitID() can be used after NcbiLog_AppStart() only");
    }
    if (sx_Info->phid[0]) {
        phid = s_StrDup((char*)sx_Info->phid);
    }
    MT_UNLOCK;
    return phid;
}


extern char* NcbiLog_GetHitID(void)
{
    TNcbiLog_Context ctx = NULL;
    char* phid = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();

    if ( s_IsInsideRequest(ctx) ) {
        if (ctx->phid[0]) {
            phid = s_StrDup(ctx->phid);
        } else {
            if (sx_Info->phid[0]  &&  sx_Info->phid_inherit) {
                phid = s_StrDup((char*)sx_Info->phid);
            }
        }
    }
    MT_UNLOCK;
    return phid;
}


/* Log request/app-wide hit ID. See NcbiLog_GetNextSubHitID().
 */
static char* s_GetSubHitID(TNcbiLog_Context ctx, int /*bool*/ need_increment, const char* prefix)
{
    char*          hit_id = NULL;
    unsigned int*  sub_id = NULL;
    #define SUBHIT_BUF_LEN (5 * NCBILOG_HITID_MAX)
    char           buf[SUBHIT_BUF_LEN]; /* use much bigger buffer to avoid overflow: prefix + 3*hitid + subid */
    size_t         prefix_len = 0;
    int n;

    VERIFY_CATCH(sx_Info->phid[0]);

    // Check prefix length 
    if (prefix) {
        prefix_len = strlen(prefix);
        VERIFY_CATCH(prefix_len < NCBILOG_HITID_MAX);
    }

    /* Select PHID to use */
    if (s_IsInsideRequest(ctx)) {
        if (ctx->phid[0]) {
            hit_id = ctx->phid;
            sub_id = &ctx->phid_sub_id;
        } else {
            /* No request-specific PHID, inherit app-wide value */
            VERIFY_CATCH(sx_Info->phid_inherit);
            hit_id = (char*)sx_Info->phid;
            sub_id = (unsigned int*)&sx_Info->phid_sub_id;
        }
    } else {
        hit_id = (char*)sx_Info->phid;
        sub_id = (unsigned int*)&sx_Info->phid_sub_id;
    }

    /* Generate sub hit ID */
    if (need_increment) {
        ++(*sub_id);

        /* Print issued sub hit ID number */
        if (*sub_id <= sx_Info->phid_sub_id_limit) {
            n = sprintf(buf, "%d", *sub_id);
            if (n <= 0) {
                return NULL;  /* error */
            }
            s_LogSubHitID(ctx, buf);
        }
    }

    /* Generate sub hit ID string representation */
    n = snprintf(buf, SUBHIT_BUF_LEN, "%s%s.%d", prefix ? prefix : "", hit_id, *sub_id);
    VERIFY_CATCH(n > 0 && n < SUBHIT_BUF_LEN);
    return s_StrDup(buf);

CATCH:
    return NULL;  /* error */
}


extern char* NcbiLog_GetCurrentSubHitID(void)
{
    return NcbiLog_GetCurrentSubHitID_Prefix(NULL);
}
    

extern char* NcbiLog_GetCurrentSubHitID_Prefix(const char* prefix)
{
    TNcbiLog_Context ctx = NULL;
    char* subhit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();

    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state == eNcbiLog_NotSet ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_GetCurrentSubHitID() can be used after NcbiLog_AppStart() only");
    }
    subhit_id = s_GetSubHitID(ctx, 0, prefix);

    MT_UNLOCK;
    return subhit_id;
}


extern char* NcbiLog_GetNextSubHitID(void)
{
    return NcbiLog_GetNextSubHitID_Prefix(NULL);
}


extern char* NcbiLog_GetNextSubHitID_Prefix(const char* prefix)
{
    TNcbiLog_Context ctx = NULL;
    char* subhit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();

    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state == eNcbiLog_NotSet ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_GetNextSubHitID() can be used after NcbiLog_AppStart() only");
    }
    subhit_id = s_GetSubHitID(ctx, 1, prefix);

    MT_UNLOCK;
    return subhit_id;
}


extern void NcbiLog_FreeMemory(void* ptr)
{
    if (ptr) {
        free(ptr);
    }
}


extern ENcbiLog_Severity NcbiLog_SetPostLevel(ENcbiLog_Severity sev)
{
    ENcbiLog_Severity prev;
    MT_LOCK_API;
    prev = sx_Info->post_level;
    sx_Info->post_level = sev;
    MT_UNLOCK;
    return prev;
}


extern ENcbiLog_AppState NcbiLog_GetState(void)
{
    ENcbiLog_AppState state = eNcbiLog_NotSet;
    TNcbiLog_Context ctx = NULL;
    if (sx_IsInit == 1) {
        MT_LOCK_API;
        ctx = s_GetContext();
        /* Get application state (context-specific) */
        state = (ctx->state == eNcbiLog_NotSet) ? sx_Info->state : ctx->state;
        MT_UNLOCK;
    }
    return state;
}


/** Print "start" message. 
 *  We should print "start" message always, before any other message.
 *  The NcbiLog_AppStart() is just a wrapper for this with checks and MT locking.
 */
static void s_AppStart(TNcbiLog_Context ctx, const char* argv[])
{
    size_t pos;
    int    n;
    char*  buf;

    /* Try to get app-wide client from environment */
    if (!sx_Info->client[0]) {
        s_SetClient((char*)sx_Info->client, s_GetClient_Env());
    }
    /* Try to get app-wide session from environment */
    if (!sx_Info->session[0]) {
        s_SetSession((char*)sx_Info->session, NcbiLogP_GetSessionID_Env());
    }
    /* Try to get app-wide hit ID from environment */
    if (!sx_Info->phid[0]) {
        const char* ev = NcbiLogP_GetHitID_Env();
        if (ev) {
            s_SetHitID((char*)sx_Info->phid, ev);
            sx_Info->phid_inherit = 1;
        } else {
            /* Auto-generate new PHID (not-inherited by requests) */
            char phid_str[NCBILOG_HITID_MAX + 1];
            s_SetHitID((char*)sx_Info->phid, s_GenerateHitID_Str(phid_str));
        }
    }

    s_SetState(ctx, eNcbiLog_AppBegin);
    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    /* We already have current time in sx_Info->post_time */
    /* Save it into app_start_time. */
    sx_Info->app_start_time.sec = sx_Info->post_time.sec;
    sx_Info->app_start_time.ns  = sx_Info->post_time.ns;

    VERIFY_CATCH(pos);

    /* Event name */
    n = sprintf(buf + pos, "%-13s", "start");
    VERIFY_CATCH(n > 0);
    pos += (size_t) n;

    /* Walk through list of arguments */
    if (argv) {
        int i;
        for (i = 0; argv[i] != NULL; ++i) {
            n = snprintf(buf + pos, NCBILOG_ENTRY_MAX - pos, " %s", argv[i]);
            VERIFY_CATCH(n > 0  &&  n < (NCBILOG_ENTRY_MAX - pos));
            pos += (size_t) n;
        }
    }
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
    return;
}


extern void NcbiLog_AppStart(const char* argv[])
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    s_AppStart(ctx, argv);
    MT_UNLOCK;
}


extern void NcbiLog_AppRun(void)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();

    /* Start app, if not started yet */
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_AppRun);

    /* Log extra parameters: app-wide PHID, host role and location */

    SNcbiLog_Param params[4];
    int i = 0;
    VERIFY(sx_Info->phid[0]);

    if (!sx_Info->host_role  &&  !sx_Info->remote_logging) {
        sx_Info->host_role = NcbiLog_GetHostRole();
    }
    if (!sx_Info->host_location  &&  !sx_Info->remote_logging) {
        sx_Info->host_location = NcbiLog_GetHostLocation();
    }
    i = s_AddParamsPair(params, i, "ncbi_phid", (char*)sx_Info->phid);
    i = s_AddParamsPair(params, i, "ncbi_role",        sx_Info->host_role);
    i = s_AddParamsPair(params, i, "ncbi_location",    sx_Info->host_location);
    params[i].key   = NULL;
    params[i].value = NULL;
    s_Extra(ctx, params);

    MT_UNLOCK;
}


extern void NcbiLog_AppStop(int exit_status)
{
    NcbiLogP_AppStop(exit_status, 0, -1);
}


extern void NcbiLog_AppStopSignal(int exit_status, int exit_signal)
{
    NcbiLogP_AppStop(exit_status, exit_signal, -1);
}


extern void NcbiLogP_AppStop(int exit_status, int exit_signal, double execution_time)
{
    TNcbiLog_Context ctx = NULL;
    size_t pos;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    s_SetState(ctx, eNcbiLog_AppEnd);
    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);

    if (execution_time < 0) {
        /* We already have current time in sx_Info->post_time */
        execution_time = s_DiffTime(sx_Info->app_start_time, sx_Info->post_time);
    }
    if ( exit_signal ) {
        sprintf(sx_Info->message + pos, "%-13s %d %.3f SIG=%d",
                "stop", exit_status, execution_time, exit_signal);
    } else {
        sprintf(sx_Info->message + pos, "%-13s %d %.3f",
                "stop", exit_status, execution_time);
    }
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
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

    /* Try to get client IP from environment, if not set by user */
    if (!ctx->is_client_set) {
        const char* ip = s_GetClient_Env();
        if (ip && *ip) {
            s_SetClient(ctx->client, ip);
            ctx->is_client_set = 1;
        }
    }

    /* Try to get session id from environment, or generate new one,
       if not set by user */
    if (!ctx->is_session_set) {
        const char* sid = NcbiLogP_GetSessionID_Env();
        if (sid && *sid) {
            s_SetSession(ctx->session, sid);
        } else {
            /* Unknown, create new session id */
            char session[NCBILOG_SESSION_MAX+1]; 
            s_SetSession(ctx->session, s_GenerateSID_Str(session));
        }
        ctx->is_session_set = 1;
    }
    
    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);

    /* We already have current time in sx_Info->post_time */
    /* Save it into sx_RequestStartTime. */
    ctx->req_start_time.sec = sx_Info->post_time.sec;
    ctx->req_start_time.ns  = sx_Info->post_time.ns;

    /* TODO: 
        if "params" is NULL, parse and print $ENV{"QUERY_STRING"}  
    */

    /* Event name */
    n = sprintf(sx_Info->message + pos, "%-13s ", "request-start");
    VERIFY_CATCH(n > 0);
    pos += (size_t)n;

    /* Return position in the message buffer */
    return pos;

CATCH:
    /* error */
    return 0;
}


/** Print extra parameters for request start.
 *  Automatically URL encode each key and value.
 *  Return current position in the sx_Info->message buffer.
 */
static size_t s_PrintReqStartExtraParams(size_t pos)
{
    SNcbiLog_Param ext[3];
    size_t ext_idx = 0;

    memset((char*)ext, 0, sizeof(ext));

    /* Add host role */
    if (!sx_Info->host_role  &&  !sx_Info->remote_logging) {
        sx_Info->host_role = NcbiLog_GetHostRole();
    }
    if (sx_Info->host_role  &&  sx_Info->host_role[0]) {
        ext[ext_idx].key = "ncbi_role";
        ext[ext_idx].value = sx_Info->host_role;
        ext_idx++;
    }

    /* Add host location */
    if (!sx_Info->host_location  &&  !sx_Info->remote_logging) {
        sx_Info->host_location = NcbiLog_GetHostLocation();
    }
    if (sx_Info->host_location  &&  sx_Info->host_location[0]) {
        ext[ext_idx].key = "ncbi_location";
        ext[ext_idx].value = sx_Info->host_location;
        ext_idx++;
    }

    if (ext_idx) {
        pos = s_PrintParams(sx_Info->message, pos, ext);
    }
    return pos;
}


void NcbiLog_ReqStart(const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t prev, pos = 0;

    MT_LOCK_API;

    ctx = s_GetContext();
    /* Common request info */
    pos = s_ReqStart(ctx);
    VERIFY_CATCH(pos);

    /* Print host role/location -- add it before users parameters */
    prev = pos;
    pos = s_PrintReqStartExtraParams(pos);
    if (prev != pos  &&  params  &&  params->key  &&  params->key[0]  &&  pos < NCBILOG_ENTRY_MAX) {
        sx_Info->message[pos++] = '&';
    }
    /* User request parameters */
    pos = s_PrintParams(sx_Info->message, pos, params);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
    MT_UNLOCK;
}


extern void NcbiLogP_ReqStartStr(const char* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t prev, pos = 0;

    MT_LOCK_API;

    ctx = s_GetContext();
    /* Common request info */
    pos = s_ReqStart(ctx);
    VERIFY_CATCH(pos);

    /* Print host role/location */
    prev = pos;
    pos = s_PrintReqStartExtraParams(pos);
    if (prev != pos  &&  params  &&  params[0]  &&  pos < NCBILOG_ENTRY_MAX) {
        sx_Info->message[pos++] = '&';
    }
    /* Parameters */
    pos = s_PrintParamsStr(sx_Info->message, pos, params);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
    MT_UNLOCK;
}


extern void NcbiLog_ReqRun(void)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();

    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_Request);

    /* Always log PHID on the request start.
       If PHID is not set on application or request level, then new PHID
       will be generated and set for the request.
    */
    if (!ctx->phid[0]) {
        char phid_str[NCBILOG_HITID_MAX + 1];
        /* Usually we should always have app-wide PHID, but not 
           in the case of ncbi_applog utility */
        /* VERIFY(sx_Info->phid[0]); */
        if (sx_Info->phid[0]  &&  sx_Info->phid_inherit) {
            /* Just log app-wide PHID, do not set it as request specific
               to allow NcbiLog_GetNextSubHitID() work correctly and
               use app-wide sub hits.
            */
            s_LogHitID(ctx, (char*)sx_Info->phid);
            MT_UNLOCK;
            return;
        }
        /* Generate and set new request-specific PHID */
        s_SetHitID(ctx->phid, s_GenerateHitID_Str(phid_str));
    }
    s_LogHitID(ctx, ctx->phid);

    MT_UNLOCK;
}


extern void NcbiLog_ReqStop(int status, size_t bytes_rd, size_t bytes_wr)
{
    TNcbiLog_Context ctx = NULL;
    size_t pos;
    double timespan;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_RequestEnd);

    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);

    /* We already have current time in sx_Info->post_time */
    timespan = s_DiffTime(ctx->req_start_time, sx_Info->post_time);
    sprintf(sx_Info->message + pos, "%-13s %d %.3f %lu %lu",
            "request-stop", status, timespan,
            (unsigned long)bytes_rd, (unsigned long)bytes_wr);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
    /* Reset state */
    s_SetState(ctx, eNcbiLog_AppRun);

    /* Reset request, client, session and hit ID */
    ctx->rid = 0;
    ctx->client[0]  = '\0';  ctx->is_client_set  = 0;
    ctx->session[0] = '\0';  ctx->is_session_set = 0;
    ctx->phid[0]    = '\0';  ctx->phid_sub_id = 0;

CATCH:
    MT_UNLOCK;
}


static void s_Extra(TNcbiLog_Context ctx, const SNcbiLog_Param* params)
{
    size_t pos;
    int    n;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY_CATCH(n > 0);
    pos += (size_t) n;
    /* Parameters */
    pos = s_PrintParams(buf, pos, params);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
    return;
}


static void s_ExtraStr(TNcbiLog_Context ctx, const char* params)
{
    size_t pos;
    int    n;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY_CATCH(n > 0);
    pos += (size_t)n;
    /* Parameters */
    pos = s_PrintParamsStr(buf, pos, params);
    /* Post a message */
    s_Post(ctx, eDiag_Log);

CATCH:
    return;
}


extern void NcbiLog_Extra(const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_Extra(ctx, params);
    MT_UNLOCK;
}


extern void NcbiLogP_ExtraStr(const char* params)
{
    TNcbiLog_Context ctx = NULL;
    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_ExtraStr(ctx, params);
    MT_UNLOCK;
}


extern void NcbiLog_Perf(int status, double timespan, const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t pos, pos_prev;
    int    n;
    char*  buf;
    char*  hit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);

    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY_CATCH(n > 0);
    pos += (size_t)n;

    /* Parameters */
    pos_prev = pos;
    pos = s_PrintParams(buf, pos, params);

    /* Add PHID if known */
    if (s_IsInsideRequest(ctx)) {
        hit_id = ctx->phid[0] ? ctx->phid : (char*)sx_Info->phid;
    } else {
        hit_id = (char*)sx_Info->phid;
    }
    if (hit_id) {
        /* need to add '&' ? */
        if ((pos > pos_prev)  &&  (pos < NCBILOG_ENTRY_MAX - 1)) {
            buf[pos++] = '&';
        }
        pos = s_PrintParamsPair(buf, pos, "ncbi_phid", hit_id);
    }
    /* Post a message */
    s_Post(ctx, eDiag_Perf);

CATCH:
    MT_UNLOCK;
}


extern void NcbiLogP_PerfStr(int status, double timespan, const char* params)
{
    TNcbiLog_Context ctx = NULL;
    size_t pos, pos_prev;
    int    n;
    char*  buf;
    char*  hit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY_CATCH(pos);

    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY_CATCH(n > 0);
    pos += (size_t)n;

    /* Parameters */
    pos_prev = pos;
    pos = s_PrintParamsStr(buf, pos, params);

    /* Add PHID if known */
    if (s_IsInsideRequest(ctx)) {
        hit_id = ctx->phid[0] ? ctx->phid : (char*)sx_Info->phid;
    } else {
        hit_id = (char*)sx_Info->phid;
    }
    if (hit_id && hit_id[0]) {
        /* need to add '&' ? */
        if ((pos > pos_prev)  &&  (pos < NCBILOG_ENTRY_MAX - 1)) {
            buf[pos++] = '&';
        }
        pos = s_PrintParamsPair(buf, pos, "ncbi_phid", hit_id);
    }
    /* Post a message */
    s_Post(ctx, eDiag_Perf);

CATCH:
    MT_UNLOCK;
}


extern void NcbiLog_Trace(const char* msg)
{
    s_PrintMessage(eNcbiLog_Trace, msg, 0 /*false*/);
}

extern void NcbiLog_Info(const char* msg)
{
    s_PrintMessage(eNcbiLog_Info, msg, 0 /*false*/);
}

extern void NcbiLog_Warning(const char* msg)
{
    s_PrintMessage(eNcbiLog_Warning, msg, 0 /*false*/);
}

extern void NcbiLog_Error(const char* msg)
{
    s_PrintMessage(eNcbiLog_Error, msg, 0 /*false*/);
}

void NcbiLog_Critical(const char* msg)
{
    s_PrintMessage(eNcbiLog_Critical, msg, 0 /*false*/);
}

extern void NcbiLog_Fatal(const char* msg)
{
    s_PrintMessage(eNcbiLog_Fatal, msg, 0 /*false*/);
}

extern void NcbiLog_Note(ENcbiLog_Severity severity, const char* msg)
{
    s_PrintMessage(severity, msg, 1 /*true*/);
}


extern void NcbiLogP_Raw(const char* line)
{
    assert(line);
    NcbiLogP_Raw2(line, strlen(line));
}

extern void NcbiLogP_Raw2(const char* line, size_t len)
{
    TFileHandle f = kInvalidFileHandle;
#if NCBILOG_USE_FILE_DESCRIPTORS
    size_t n;
#else
    int n;
#endif

    VERIFY(line);
    VERIFY(line[len] == '\0');
    VERIFY(len > NCBILOG_ENTRY_MIN);

    MT_LOCK_API;
    if (sx_Info->destination == eNcbiLog_Disable) {
        MT_UNLOCK;
        return;
    }
    /* Reopen logging files, if necessary */
    s_InitDestination(NULL);
    /* Destination can be disabled on reopening, so check again */
    if (sx_Info->destination == eNcbiLog_Disable) {
        MT_UNLOCK;
        return;
    }
    f = sx_Info->file_log;

    switch (sx_Info->destination) {
        case eNcbiLog_Default:
        case eNcbiLog_Stdlog:
        case eNcbiLog_Cwd:
        case eNcbiLog_File:
            /* Try to get type of the line to redirect output into correct log file */
            {
                const char* start = line + NCBILOG_ENTRY_MIN - 1;
                const char* ptr = strstr(start, (char*)sx_Info->appname);

                if (!ptr || (ptr - start > NCBILOG_APPNAME_MAX)) {
                    break;
                }
                ptr += strlen((char*)sx_Info->appname) + 1;

                if (len > (size_t)(ptr - line) + 5) {
                    if (strncmp(ptr, "perf", 4) == 0) {
                        f = sx_Info->file_perf;
                    } else 
                    if (strncmp(ptr, "Trace", 5) == 0  ||
                        strncmp(ptr, "Info" , 4) == 0) {
                        f = sx_Info->file_trace;
                    }
                }
                if (f == kInvalidFileHandle) {
                    /* Warning, Error, Critical, Fatal, or any other line */
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
    
    VERIFY_CATCH(f != kInvalidFileHandle);
    
#if NCBILOG_USE_FILE_DESCRIPTORS
    VERIFY_CATCH(len < NCBILOG_ENTRY_MAX); /* account +1 for '\n' */
    memcpy(sx_Info->message, line, len);
    sx_Info->message[len] = '\n';
    /* should be a single write */
    n = s_Write(f, sx_Info->message, len + 1);
    VERIFY(n == len + 1);
    /* fdatasync(f); */
#else
    n = fprintf(f, "%s\n", line);
    VERIFY(n > 0);
    fflush(f);
#endif

CATCH:
    MT_UNLOCK;
}


extern void NcbiLog_UpdateOnFork(TNcbiLog_OnForkFlags flags)
{
    int  n;
    #define kIdBufSize 128
    char buf[kIdBufSize];
    int  old_guid_hi, old_guid_lo;
    TNcbiLog_Context ctx = NULL;

    MT_LOCK_API;
    
    ctx = s_GetContext();
    TNcbiLog_PID old_pid = sx_PID;

#if defined(NCBI_OS_UNIX)
    TNcbiLog_PID new_pid = (TNcbiLog_PID)getpid();
#elif defined(NCBI_OS_MSWIN)
    TNcbiLog_PID new_pid = GetCurrentProcessId();
#endif
    if (old_pid == new_pid) {
        /* Do not perform any actions in the parent process. */
        MT_UNLOCK;
        return;
    }
    /* -- Child process -- */
    
    sx_PID = new_pid;
    
    /* Update GUID to match the new PID */
    old_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
    old_guid_lo = (int)(sx_Info->guid & 0xFFFFFFFF);
    sx_Info->guid = s_CreateUID();

    /* Reset tid for the current context */
    ctx->tid = s_GetTID();

    if (flags & fNcbiLog_OnFork_ResetTimer) {
        s_GetTime((STime*)&sx_Info->app_start_time);
    }
    if (flags & fNcbiLog_OnFork_PrintStart) {
        /* Forcedly reset state to allow starting sequince */
        sx_Info->state = eNcbiLog_NotSet;
        s_AppStart(ctx, NULL);
        s_SetState(ctx, eNcbiLog_AppRun);
    }

    /* Log ID changes */
    n = snprintf(buf, kIdBufSize,
        "action=fork&parent_guid=%08X%08X&parent_pid=%05" NCBILOG_UINT8_FORMAT_SPEC, 
        old_guid_hi, old_guid_lo, old_pid);
    VERIFY(n > 0  &&  n < kIdBufSize);
    s_ExtraStr(ctx, buf);

    if (flags & fNcbiLog_OnFork_PrintStart) {
        /* Also, log app-wide hit ID for the new process as well */
        VERIFY(sx_Info->phid[0]);
        s_LogHitID(ctx, (char*)sx_Info->phid);
    }

    MT_UNLOCK;
}



/******************************************************************************
 *  Logging setup functions --- for internal use only
 */

extern TNcbiLog_Info* NcbiLogP_GetInfoPtr(void)
{
    return (TNcbiLog_Info*)sx_Info;
}

extern TNcbiLog_Context NcbiLogP_GetContextPtr(void)
{
    return s_GetContext();
}

 extern int NcbiLogP_DisableChecks(int /*bool*/ disable)
 {
    int current = sx_DisableChecks;
    sx_DisableChecks = disable;
    return current;
 }
