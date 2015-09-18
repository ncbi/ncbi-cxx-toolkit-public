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

#if defined(NDEBUG)
#  define verify(expr)  while ( expr ) break
#else
#  define verify(expr)  assert(expr)
#endif

/* Critical error. Should never happen */ 
#ifdef TROUBLE
#  undef TROUBLE
#endif
#define TROUBLE           s_Abort(__LINE__, 0)
#define TROUBLE_MSG(msg)  s_Abort(__LINE__, msg)

/* Verify an expression and abort on error */
#ifdef VERIFY
#  undef VERIFY
#endif

#if defined(NDEBUG)
#  define VERIFY(expr)  while ( expr ) break
#else
#  define VERIFY(expr)  do  { if ( !(expr) )  s_Abort(__LINE__, #expr); }  while ( 0 )
#endif


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
#define NCBILOG_ENTRY_MAX_ALLOC 8192   /* 8Kb*/ 
#define NCBILOG_ENTRY_MAX       8190   /* NCBILOG_ENTRY_MAX_ALLOC - 2, for ending '\n\0' */ 

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



/******************************************************************************
 *  Abort  (for the locally defined ASSERT, VERIFY and TROUBLE macros)
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
    char* res = NULL;
    if ( !size ) {
        return NULL;
    }
    res = (char*) malloc(size);
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
 *  The order is: cached host name, uname or COMPUTERNAME, empty string.
 */
const char* NcbiLog_GetHostName(void)
{
    static const char* host = NULL;
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
        host = s_StrDup(compname);
    }
#endif
    return host;
}

/** Read first string from specified file.
 *  Return NULL on error.
 */
static const char* s_ReadFileString(const char* filename)
{
    FILE*  fp;
    char   buf[32];
    size_t len = sizeof(buf);
    char*  p = buf;

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
    if ( role ) {
        return role;
    }
    role = s_ReadFileString("/etc/ncbi/role");
    return role;
}


/** Get host location string.
 */
const char* NcbiLog_GetHostLocation(void)
{
    static const char* location = NULL;
    if ( location ) {
        return location;
    }
    location = s_ReadFileString("/etc/ncbi/location");
    return location;
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
#elif defined(NCBI_OS_UNIX)
#  if defined(SYS_gettid)
    tid = (TNcbiLog_TID)syscall(SYS_gettid);
#  else
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
static TNcbiLog_Int8 s_CreateUID(void)
{
    TNcbiLog_PID   pid = s_GetPID();
    time_t         t = time(0);
    const char*    host = NcbiLog_GetHostName();
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
       but assign new version number to distinguish the NCBI C logging API
       from the C++ API.
    */
    return ((TNcbiLog_Int8)h << 48) |
           (((TNcbiLog_Int8)pid & 0xFFFF) << 32) |
           (((TNcbiLog_Int8)t & 0xFFFFFFF) << 4) |
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


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value wil be cached for whole process. 
 */
static const char* s_GetClient_Env(void)
{
    static const char* client = NULL;
    const char* p = NULL;
    
    if ( client ) {
        return client;
    }
    p = getenv("HTTP_CAF_EXTERNAL");
    if ( !p  ||  !p[0] ) {
        /* !HTTP_CAF_EXTERNAL */
        if ( (p = getenv("HTTP_CLIENT_HOST")) != NULL  &&  *p ) {
            client = s_StrDup(p);
            return client;
        }
    }
    if ( (p = getenv("HTTP_CAF_PROXIED_HOST")) != NULL  &&  *p ) {
        client = s_StrDup(p);
        return client;
    }
    if ( (p = getenv("PROXIED_IP")) != NULL  &&  *p ) {
        client = s_StrDup(p);
        return client;
    }
    if ( (p = getenv("REMOTE_ADDR")) != NULL  &&  *p ) {
        client = s_StrDup(p);
        return client;
    }
    return NULL;
}


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value wil be cached for whole process. 
 */
extern const char* NcbiLogP_GetSessionID_Env(void)
{
    static const char* session = NULL;
    const char* p = NULL;

    if ( session ) {
        return session;
    }
    if ( (p = getenv("HTTP_NCBI_SID")) != NULL  &&  *p ) {
        session = s_StrDup(p);
        return session;
    }
    if ( (p = getenv("NCBI_LOG_SESSION_ID")) != NULL  &&  *p ) {
        session = s_StrDup(p);
        return session;
    }
    return NULL;
}


/** This routine is called during start up and again in NcbiLog_ReqStart(). 
    The received environment value wil be cached for whole process. 
 */
extern const char* NcbiLogP_GetHitID_Env(void)
{
    static const char* phid = NULL;
    const char* p = NULL;

    if ( phid ) {
        return phid;
    }
    if ( (p = getenv("HTTP_NCBI_PHID")) != NULL  &&  *p ) {
        phid = s_StrDup(p);
        return phid;
    }
    if ( (p = getenv("NCBI_LOG_HIT_ID")) != NULL  &&  *p ) {
        phid = s_StrDup(p);
        return phid;
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
            TROUBLE_MSG("unknown destination");
    }
    if (sx_Info->file_trace) {
    
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_trace);
#else
        fclose(sx_Info->file_trace);
#endif
        sx_Info->file_trace = kInvalidFileHandle;
    }
    if (sx_Info->file_log) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_log);
#else
        fclose(sx_Info->file_log);
#endif
        sx_Info->file_log = kInvalidFileHandle;
    }
    if (sx_Info->file_err) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_err);
#else
        fclose(sx_Info->file_err);
#endif
        sx_Info->file_err = kInvalidFileHandle;
    }
    if (sx_Info->file_perf) {
#if NCBILOG_USE_FILE_DESCRIPTORS
        close(sx_Info->file_perf);
#else
        fclose(sx_Info->file_perf);
#endif
        sx_Info->file_perf = kInvalidFileHandle;
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
#if NCBILOG_USE_FILE_DESCRIPTORS
    int flags = O_CREAT | O_APPEND | O_WRONLY;
    mode_t mode = 0664;
#endif    

    assert(path_with_base_name);
    /* Check max possible file name (path.trace) */
    n = strlen(path_with_base_name);
    assert(n);
    assert((n + 6) < sizeof(path));
    memcpy(path, path_with_base_name, n);

    /* Trace */
    strcpy(path + n, ".trace");
#if NCBILOG_USE_FILE_DESCRIPTORS
    sx_Info->file_trace = open(path, flags, mode);
#else
    sx_Info->file_trace = fopen(path, "a");
#endif    
    sx_Info->file_trace_name = s_StrDup(path);

    /* Log */
    if (sx_Info->file_trace) {
        strcpy(path + n, ".log");
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_log = open(path, flags, mode);
#else
        sx_Info->file_log = fopen(path, "a");
#endif        
        sx_Info->file_log_name = s_StrDup(path);
    }

    /* Err */
    if (sx_Info->file_log) {
        strcpy(path + n, ".err");
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_err = open(path, flags, mode);
#else
        sx_Info->file_err = fopen(path, "a");
#endif        
        sx_Info->file_err_name = s_StrDup(path);
    }

    /* Perf */
    if (sx_Info->file_err) {
        strcpy(path + n, ".perf");
#if NCBILOG_USE_FILE_DESCRIPTORS
        sx_Info->file_perf = open(path, flags, mode);
#else
        sx_Info->file_perf = fopen(path, "a");
#endif        
        sx_Info->file_perf_name = s_StrDup(path);
    }

    if ((sx_Info->file_trace != kInvalidFileHandle)  &&
        (sx_Info->file_log   != kInvalidFileHandle)  &&
        (sx_Info->file_err   != kInvalidFileHandle)  &&
        (sx_Info->file_perf  != kInvalidFileHandle) ) {
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
    return s_SetLogFiles(path);
}


/** (Re)Initialize destination.
 */
static void s_InitDestination(const char* logfile_path) 
{
#if NCBILOG_USE_FILE_DESCRIPTORS
    int flags = O_CREAT | O_APPEND | O_WRONLY;
    mode_t mode = 0664;
#endif    
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
        TROUBLE_MSG("error opening logging location");
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

            assert((sx_Info->file_trace == kInvalidFileHandle)  &&
                   (sx_Info->file_log   == kInvalidFileHandle)  &&
                   (sx_Info->file_err   == kInvalidFileHandle)  &&
                   (sx_Info->file_perf  == kInvalidFileHandle));

#if NCBILOG_USE_FILE_DESCRIPTORS
            sx_Info->file_trace = open(sx_Info->file_trace_name, flags, mode);
            sx_Info->file_log   = open(sx_Info->file_log_name,   flags, mode);
            sx_Info->file_err   = open(sx_Info->file_err_name,   flags, mode);
            sx_Info->file_perf  = open(sx_Info->file_perf_name,  flags, mode);
#else
            sx_Info->file_trace = fopen(sx_Info->file_trace_name, "a");
            sx_Info->file_log   = fopen(sx_Info->file_log_name,   "a");
            sx_Info->file_err   = fopen(sx_Info->file_err_name,   "a");
            sx_Info->file_perf  = fopen(sx_Info->file_perf_name,  "a");
#endif
            if ((sx_Info->file_trace != kInvalidFileHandle)  &&
                (sx_Info->file_log   != kInvalidFileHandle)  &&
                (sx_Info->file_err   != kInvalidFileHandle)  &&
                (sx_Info->file_perf  != kInvalidFileHandle) ) {
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
                    sprintf(xdir, "%s%d", kBaseLogDir, sx_Info->server_port);
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


static char* s_GenerateSID_Str(char* dst)
{
    TNcbiLog_UInt8 guid;
    int hi, lo, n;

    if (!sx_Info->guid) {
        sx_Info->guid = s_CreateUID();
    }
    guid = sx_Info->guid;
    hi = (int)((guid >> 32) & 0xFFFFFFFF);
    lo = (int) (guid & 0xFFFFFFFF);
    n = sprintf(dst, "%08X%08X_%04" NCBILOG_UINT8_FORMAT_SPEC "SID", hi, lo, sx_Info->rid);
    if (n <= 0) {
        return NULL;
    }
    dst[n] = '\0';
    return dst;
}


static char* s_GenerateHitID_Str(char* dst)
{
    TNcbiLog_UInt8 hi, tid, rid, us, lo;
    unsigned int b0, b1, b2, b3;
    time_t time_sec;
    unsigned long time_ns;
    int n;

    if ( !sx_Info->guid ) {
        sx_Info->guid = s_CreateUID();
    }
    hi  = sx_Info->guid;
    b3  = (unsigned int)((hi >> 32) & 0xFFFFFFFF);
    b2  = (unsigned int)(hi & 0xFFFFFFFF);
    tid = (s_GetTID() & 0xFFFFFF) << 40;
    rid = (TNcbiLog_UInt8)(sx_Info->rid & 0xFFFFFF) << 16;
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
    dst[n] = '\0';
    return dst;
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
        n   -= n_written;
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
#if NCBILOG_USE_FILE_DESCRIPTORS
    size_t n_write;
#endif
    int n;
   
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
    
    /* Write */
    assert(f != kInvalidFileHandle);

#if NCBILOG_USE_FILE_DESCRIPTORS
    n_write = strlen(sx_Info->message);
    VERIFY(n_write <= NCBILOG_ENTRY_MAX);
    sx_Info->message[n_write] = '\n';
    n_write++;
    sx_Info->message[n_write] = '\0';
    n = s_Write(f, sx_Info->message, n_write);
    VERIFY(n == n_write);
#else
    n = fprintf(f, "%s\n", sx_Info->message);
    fflush(f);
    VERIFY(n > 0);
#endif

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
    if (n <= 0) {
        return 0;  /* error */
    }
    return n;
}


/** Print one parameter's pair (key, value) to message buffer.
 *  Automatically URL encode each key and value.
 *  Return new position in the buffer, or zero on error.
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
        return 0;  /* error */
    }
    /* Value */
    dst[pos++] = '=';
    if (!value  ||  value[0] == '\0') {
        return pos;
    }
    len = strlen(value);
    s_URL_Encode(value, len, &r_len, dst + pos, NCBILOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    dst[pos] = '\0';

    return pos;
}


/** Print parameters to message buffer.
 *  Return new position in the buffer, or zero on error.
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
                VERIFY(pos);
            }
            params++;
        }
    }
    dst[pos] = '\0';
    return pos;
}


/** Print parameters to message buffer (string version).
 *  The 'params' must be already prepared pairs with URL-encoded key and value.
 *  Return new position in the buffer, or zero on error.
 */
static size_t s_PrintParamsStr(char* dst, size_t pos, const char* params)
{
    size_t len;

    if (!params) {
        return pos;
    }
    len = strlen(params);
    if (len >= NCBILOG_ENTRY_MAX - pos) {
        return 0;  /* error */
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
    VERIFY(pos);

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
        s_Abort(0,0); /* no additional error reporting */
    }
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
    sx_Info->message = (char*) malloc(NCBILOG_ENTRY_MAX_ALLOC);
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


extern ENcbiLog_Destination NcbiLog_SetDestination(ENcbiLog_Destination ds)
{
    char* logfile = NULL;

    MT_LOCK_API;
    /* Close current destination */
    s_CloseLogFiles(1 /*force cleanup*/);

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
    }
    ds = sx_Info->destination;
    MT_UNLOCK;
    return ds;
}


extern ENcbiLog_Destination NcbiLogP_SetDestination(ENcbiLog_Destination ds, unsigned int port)
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

    params[0].key   = "ncbi_phid";
    params[0].value = hit_id;
    params[1].key   = NULL;
    params[1].value = NULL;
    s_Extra(ctx, params);
}


extern void NcbiLog_AppSetHitID(const char* hit_id)
{
    MT_LOCK_API;
    /* Enforce calling this method before NcbiLog_AppStart() */
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
        // if not the same
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


extern char* NcbiLog_GetNextSubHitID(void)
{
    TNcbiLog_Context ctx = NULL;
    char*            hit_id = NULL;
    unsigned int*    sub_id = NULL;
    char             buf[NCBILOG_HITID_MAX+1];
    int n;

    MT_LOCK_API;
    ctx = s_GetContext();

    /* Enforce calling this method after NcbiLog_AppStart() */
    if (sx_Info->state == eNcbiLog_NotSet ||
        sx_Info->state == eNcbiLog_AppBegin) {
        TROUBLE_MSG("NcbiLog_GetNextSubHitID() can be used after NcbiLog_AppStart() only");
    }

    VERIFY(sx_Info->phid[0]);

    /* Select PHID to use */
    if ( s_IsInsideRequest(ctx) ) {
        if (ctx->phid[0]) {
            hit_id = ctx->phid;
            sub_id = &ctx->phid_sub_id;
        } else {
            /* No request-specific PHID, inherit app-wide value */
            VERIFY(sx_Info->phid_inherit);
            hit_id = (char*)sx_Info->phid;
            sub_id = (unsigned int*)&sx_Info->phid_sub_id;
        }
    } else {
        hit_id = (char*)sx_Info->phid;
        sub_id = (unsigned int*)&sx_Info->phid_sub_id;
    }

    /* Generate sub hit ID */
    assert(strlen(hit_id) + 6 <= NCBILOG_HITID_MAX);
    if (strlen(hit_id) + 6 > NCBILOG_HITID_MAX) {
        MT_UNLOCK;
        return NULL;
    }
    n = sprintf(buf, "%s.%d", hit_id, ++(*sub_id));
    ++sub_id;

    MT_UNLOCK;

    if (n <= 0) {
        return NULL;  /* error */
    }
    return s_StrDup(buf);
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


/** Print "start" message. 
 *  We should print "start" message always, before any other message.
 *  The NcbiLog_AppStart() is just a wrapper for this with checks and MT locking.
 */
static void s_AppStart(TNcbiLog_Context ctx, const char* argv[])
{
    int    i, n;
    size_t pos;
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

    /* Log app-wide hit ID */
    VERIFY(sx_Info->phid[0]);
    s_LogHitID(ctx, (char*)sx_Info->phid);

    MT_UNLOCK;
}


extern void NcbiLog_AppStop(int exit_status)
{
    NcbiLog_AppStopSignal(exit_status, 0);
}


extern void NcbiLog_AppStopSignal(int exit_status, int exit_signal)
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
    VERIFY(pos);
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


/** Print extra parameters for request start.
 *  Automatically URL encode each key and value.
 *  Return current position in the buffer.
 */
static size_t s_PrintReqStartExtraParams(char* dst, size_t pos)
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
    VERIFY(pos);

    /* Print host role/location -- add it before users parameters */
    prev = pos;
    pos = s_PrintReqStartExtraParams(sx_Info->message, pos);
    VERIFY(pos);
    if (prev != pos  &&  params  &&  params->key  &&  params->key[0]  &&  pos < NCBILOG_ENTRY_MAX) {
        sx_Info->message[pos++] = '&';
    }

    /* User request parameters */
    pos = s_PrintParams(sx_Info->message, pos, params);
    VERIFY(pos);

    /* Post a message */
    s_Post(ctx, eDiag_Log);

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
    VERIFY(pos);

    /* Print host role/location */
    prev = pos;
    pos = s_PrintReqStartExtraParams(sx_Info->message, pos);
    VERIFY(pos);
    if (prev != pos  &&  params  &&  params[0]  &&  pos < NCBILOG_ENTRY_MAX) {
        sx_Info->message[pos++] = '&';
    }

    /* Parameters */
    pos = s_PrintParamsStr(sx_Info->message, pos, params);
    VERIFY(pos);

    /* Post a message */
    s_Post(ctx, eDiag_Log);

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
           in the case for ncbi_applog utility */
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
    int    n;
    size_t pos;
    double timespan;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);
    s_SetState(ctx, eNcbiLog_RequestEnd);

    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos);
    /* We already have current time in sx_Info->post_time */
    timespan = s_DiffTime(ctx->req_start_time, sx_Info->post_time);
    n = sprintf(sx_Info->message + pos, "%-13s %d %.3f %lu %lu",
                "request-stop", status, timespan,
                (unsigned long)bytes_rd, (unsigned long)bytes_wr);
    VERIFY(n > 0);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
    /* Reset state */
    s_SetState(ctx, eNcbiLog_AppRun);

    /* Reset request, client, session and hit ID */
    ctx->rid = 0;
    ctx->client[0]  = '\0';  ctx->is_client_set  = 0;
    ctx->session[0] = '\0';  ctx->is_session_set = 0;
    ctx->phid[0]    = '\0';  ctx->phid_sub_id = 0;

    MT_UNLOCK;
}


static void s_Extra(TNcbiLog_Context ctx, const SNcbiLog_Param* params)
{
    int    n;
    size_t pos;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    pos = s_PrintParams(buf, pos, params);
    VERIFY(pos);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}


static void s_ExtraStr(TNcbiLog_Context ctx, const char* params)
{
    int    n;
    size_t pos;
    char*  buf;

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos);
    /* Event name */
    n = sprintf(buf + pos, "%-13s ", "extra");
    VERIFY(n > 0);
    pos += n;
    /* Parameters */
    pos = s_PrintParamsStr(buf, pos, params);
    VERIFY(pos);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
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


extern void NcbiLog_Perf(int status, double timespan,
                         const SNcbiLog_Param* params)
{
    TNcbiLog_Context ctx = NULL;
    int    n;
    size_t pos, pos_prev;
    char*  buf;
    char*  hit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos);

    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY(n > 0);
    pos += n;

    /* Parameters */
    pos_prev = pos;
    pos = s_PrintParams(buf, pos, params);
    VERIFY(pos);

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
        VERIFY(pos);
    }

    /* Post a message */
    s_Post(ctx, eDiag_Perf);

    MT_UNLOCK;
}


extern void NcbiLogP_PerfStr(int status, double timespan, const char* params)
{
    TNcbiLog_Context ctx = NULL;
    int    n;
    size_t pos, pos_prev;
    char*  buf;
    char*  hit_id = NULL;

    MT_LOCK_API;
    ctx = s_GetContext();
    CHECK_APP_START(ctx);

    /* Prefix */
    buf = sx_Info->message;
    pos = s_PrintCommonPrefix(ctx);
    VERIFY(pos);

    /* Print event name, status and timespan */
    n = sprintf(buf + pos, "%-13s %d %f ", "perf", status, timespan);
    VERIFY(n > 0);
    pos += n;

    /* Parameters */
    pos_prev = pos;
    pos = s_PrintParamsStr(buf, pos, params);
    VERIFY(pos);

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
        VERIFY(pos);
    }

    /* Post a message */
    s_Post(ctx, eDiag_Perf);

    MT_UNLOCK;
}


extern void NcbiLog_Trace(const char* msg)
{
    s_PrintMessage(eNcbiLog_Trace, msg);
}

extern void NcbiLog_Info(const char* msg)
{
    s_PrintMessage(eNcbiLog_Info, msg);
}

extern void NcbiLog_Warning(const char* msg)
{
    s_PrintMessage(eNcbiLog_Warning, msg);
}

extern void NcbiLog_Error(const char* msg)
{
    s_PrintMessage(eNcbiLog_Error, msg);
}

void NcbiLog_Critical(const char* msg)
{
    s_PrintMessage(eNcbiLog_Critical, msg);
}

extern void NcbiLog_Fatal(const char* msg)
{
    s_PrintMessage(eNcbiLog_Fatal, msg);
}

extern void NcbiLogP_Raw(const char* line)
{
    assert(line);
    NcbiLogP_Raw2(line, strlen(line));
}

extern void NcbiLogP_Raw2(const char* line, size_t len)
{
    TFileHandle f = kInvalidFileHandle;
    int n;

    assert(line);
    assert(line[len] == '\0');
    assert(len > 127);  /* minimal applog line length (?) */

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
                assert(len > (size_t)(ptr - line));

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
    
    assert(f != kInvalidFileHandle);
    
#if NCBILOG_USE_FILE_DESCRIPTORS
    VERIFY(len <= NCBILOG_ENTRY_MAX);
    n = s_Write(f, line, len);
    VERIFY(n == len);
    n = s_Write(f, "\n", 1);
    VERIFY(n == 1);
#else
    n = fprintf(f, "%s\n", line);
    fflush(f);
    VERIFY(n > 0);
#endif

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
