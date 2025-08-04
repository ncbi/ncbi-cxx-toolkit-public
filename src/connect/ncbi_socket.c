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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Plain portable TCP/IP socket API for:  UNIX, MS-Win, MacOS
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    ws2_32.lib
 *
 */

/* Uncomment these(or specify "-DHAVE_GETADDRINFO -DHAVE_GETNAMEINFO") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "getaddrinfo()" and "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code, and
 * 4) you are aware that GLIBC implementation is rather heavy (creates tons of
 *    test sockets on the fly), yet you prefer to use that API nonetheless
 */

/* #define HAVE_GETADDRINFO 1 */
/* #define HAVE_GETNAMEINFO 1 */

/* Uncomment this (or specify "-DNCBI_HAVE_GETHOSTBY***_R=") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "gethostbyname_r()" but not "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
 */

/*   Solaris: */
/* #define NCBI_HAVE_GETHOSTBYNAME_R 5 */
/* #define NCBI_HAVE_GETHOSTBYADDR_R 7 */

/*   Linux, IRIX: */
/* #define NCBI_HAVE_GETHOSTBYNAME_R 6 */
/* #define NCBI_HAVE_GETHOSTBYADDR_R 8 */

/* Uncomment this (or specify "-DHAVE_SIN_LEN") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) on your platform, struct sockaddr_in contains a field called "sin_len"
 *    (and sockaddr_un::sun_len is then assumed to be also present).
 */

/* #define HAVE_SIN_LEN 1 */

/* NCBI core headers
 */
#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_once.h"
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_socket_unix.h>

/* Remaining platform-specific system headers
 */
#ifdef NCBI_OS_UNIX
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  ifdef NCBI_OS_LINUX
#    ifndef   IP_MTU
#      define IP_MTU      14
#    endif /*!IP_MTU*/
#  endif /*NCBI_OS_LINUX*/
#  ifdef NCBI_OS_CYGWIN
#    ifndef   IPV6_MTU
#      define IPV6_MTU    72
#    endif /*!IPV6_MTU*/
#  endif/*NCBI_OS_CYGWIN*/
#  ifdef HAVE_POLL_H
#    include <poll.h>
#    ifndef   POLLPRI
#      define POLLPRI     POLLIN
#    endif /*!POLLPRI*/
#    ifndef   POLLRDNORM
#      define POLLRDNORM  POLLIN
#    endif /*!POLLRDNORM*/
#    ifndef   POLLRDBAND
#      define POLLRDBAND  POLLIN
#    endif /*!POLLRDBAND*/
#    ifndef   POLLWRNORM
#      define POLLWRNORM  POLLOUT
#    endif /*!POLLWRNORM*/
#    ifndef   POLLWRBAND
#      define POLLWRBAND  POLLOUT
#    endif /*!POLLWRBAND*/
#    ifndef   POLLRDHUP
#      define POLLRDHUP   POLLHUP
#    endif /*!POLLRDHUP*/
#    define   POLL_READ         (POLLIN  | POLLRDNORM | POLLRDBAND | POLLPRI)
#    define   POLL_WRITE        (POLLOUT | POLLWRNORM | POLLWRBAND)
#    define  _POLL_READY        (POLLERR | POLLHUP)
#    define   POLL_READ_READY   (_POLL_READY | POLL_READ | POLLRDHUP)
#    define   POLL_WRITE_READY  (_POLL_READY | POLL_WRITE)
#    define   POLL_ERROR         POLLNVAL
#  endif /*HAVE_POLL_H*/
#  include <signal.h>
#  include <unistd.h>
#  if !defined(NCBI_OS_BEOS)
#    include <arpa/inet.h>
#  endif /*NCBI_OS_BEOS*/
#  include <sys/param.h>
#  ifdef HAVE_SYS_RESOURCE_H
#    include <sys/resource.h>
#  endif /*HAVE_SYS_RESOURCE_H*/
#  include <sys/stat.h>
#  include <sys/un.h>
#endif /*NCBI_OS_UNIX*/

/* Portable standard C headers
 */
#include <ctype.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Socket


#ifndef   INADDR_LOOPBACK
#  define INADDR_LOOPBACK  0x7F000001
#endif /*!INADDR_LOOPBACK*/
#ifndef   IN_LOOPBACKNET
#  define IN_LOOPBACKNET   127
#endif /*!IN_LOOPBACKNET*/
#ifdef IN_CLASSA_MAX
#  if  IN_CLASSA_MAX <= IN_LOOPBACKNET
#    error "IN_LOOPBACKNET is out of range"
#  endif /*IN_CLASSA_MAX<=IN_LOOPBACKNET*/
#endif /*IN_CLASSA_MAX*/

#ifdef NCBI_OS_MSWIN
/* NB: MSWIN defines INET6_ADDRSTRLEN to include port # etc */
#  define SOCK_ADDRSTRLEN     46
#else
#  ifndef INET6_ADDRSTRLEN
   /*POSIX: sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")*/
#    define INET6_ADDRSTRLEN  46
#  endif /*!INET6_ADDRSTRLEN*/
#  define SOCK_ADDRSTRLEN     INET6_ADDRSTRLEN
#endif /*NCBI_OS_MSWIN*/

#define SOCK_HOSTPORTSTRLEN   (1/*[*/ + SOCK_ADDRSTRLEN + 2/*]:*/ + 5/*port*/)

#ifdef ENOSPC
#  define SOCK_ENOSPC  ENOSPC
#else
#  define SOCK_ENOSPC  ERANGE
#endif /*ENOSPC*/

#ifdef NCBI_MONKEY
/* A hack - we assume that SOCK variable is named "sock" in the code.
   If the desired behavior is timeout, "sock" will be replaced with a
   connection to a non-responsive server */
#  define send(a, b, c, d)                                              \
    (g_MONKEY_Send    ? g_MONKEY_Send((a),(b),(c),(d),&sock) : send((a),(b),(c),(d)))
#  define recv(a, b, c, d)                                              \
    (g_MONKEY_Recv    ? g_MONKEY_Recv((a),(b),(c),(d),&sock) : recv((a),(b),(c),(d)))
#  define connect(a, b, c)                                              \
    (g_MONKEY_Connect ? g_MONKEY_Connect((a),(b),(c))        : connect((a),(b),(c)))
#endif /*NCBI_MONKEY*/



/******************************************************************************
 *  TYPEDEFS & MACROS
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE   16384

/* Macros for platform-dependent constants, error codes and functions
 */
#if   defined(NCBI_OS_UNIX)

#  define SOCK_INVALID        (-1)
#  define SOCK_ERRNO          errno
#  define SOCK_NFDS(s)        ((s) + 1)
#  ifdef NCBI_OS_BEOS
#    define SOCK_CLOSE(s)     closesocket(s)
#  else
#    define SOCK_CLOSE(s)     close(s)
#  endif /*NCBI_OS_BEOS*/

#  define WIN_INT_CAST        /*(no cast)*/
#  define WIN_DWORD_CAST      /*(no cast)*/

#  ifdef NCBI_OS_CYGWIN
     /* These do not work correctly as of cygwin 2.11.1 */
#    ifdef   SOCK_NONBLOCK
#      undef SOCK_NONBLOCK
#    endif /*SOCK_NONBLOCK*/
#    ifdef   SOCK_CLOEXEC
#      undef SOCK_CLOEXEC
#    endif /*SOCK_CLOEXEC*/
#    ifdef   HAVE_ACCEPT4
#      undef HAVE_ACCEPT4
#    endif /*HAVE_ACCEPT4*/
#  endif /*NCBI_OS_CYGWIN*/

#  ifndef   INADDR_NONE
#    define INADDR_NONE       ((unsigned int)(~0UL))
#  endif /*!INADDR_NONE*/

#  if defined(TCP_NOPUSH)  &&  !defined(TCP_CORK)
#    define TCP_CORK          TCP_NOPUSH  /* BSDism */
#  endif /*TCP_NOPUSH && !TCP_CORK*/
/* NCBI_OS_UNIX */

#elif defined(NCBI_OS_MSWIN)

#  define SOCK_INVALID        INVALID_SOCKET
#  define SOCK_ERRNO          WSAGetLastError()
#  define SOCK_NFDS(s)        0
#  define SOCK_CLOSE(s)       closesocket(s)

#  define WIN_INT_CAST        (int)
#  define WIN_DWORD_CAST      (DWORD)

#  define SOCK_GHBX_MT_SAFE   1  /* for gethostby...() */
#  define SOCK_SEND_SLICE     (4 << 10)  /* 4K */

#  define SOCK_EVENTS         (FD_CLOSE|FD_CONNECT|FD_OOB|FD_WRITE|FD_READ)
#  ifndef   WSA_INVALID_EVENT
#    define WSA_INVALID_EVENT ((WSAEVENT) 0)
#  endif /*!WSA_INVALID_EVENT*/
/* NCBI_OS_MSWIN */

#endif /*NCBI_OS*/

#ifdef   sun
#  undef sun
#endif /*sun*/

#ifndef   abs
#  define abs(a)              ((a) < 0 ? -(a) : (a))
#endif /*!abs*/

#define MAXIDLEN              128

#define SOCK_STRERROR(error)  s_StrError(0, (error))

#define SOCK_LOOPBACK         (assert(INADDR_LOOPBACK), htonl(INADDR_LOOPBACK))

#define SOCK_GET_TIMEOUT(s, t)      ((s)->t##_tv_set ? &(s)->t##_tv : 0)

#define SOCK_SET_TIMEOUT(s, t, v)  (((s)->t##_tv_set = (v) ? 1 : 0)     \
                                    ? (void)((s)->t##_tv = *(v)) : (void) 0)


#if defined(HAVE_SOCKLEN_T)  ||  defined(_SOCKLEN_T)
typedef socklen_t  TSOCK_socklen_t;
#else
typedef int        TSOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T || _SOCKLEN_T*/



/******************************************************************************
 *  INTERNAL GLOBALS
 */


const char g_kNcbiSockNameAbbr[] = "SOCK";



/******************************************************************************
 *  STATIC GLOBALS
 */


/* Flag to indicate whether the API has been [de]initialized */
static volatile int/*bool*/ s_Initialized = 0/*-1=deinit;0=uninit;1=init*/;

/* Which wait API to use, UNIX only */
static ESOCK_IOWaitSysAPI s_IOWaitSysAPI = eSOCK_IOWaitSysAPIAuto;

/* Through ID counter */
static volatile unsigned int s_ID_Counter = 0;

/* Read-while-writing switch */
static ESwitch s_ReadOnWrite = eOff;        /* no read-on-write by default   */

/* Reuse address flag for newly created stream sockets */
static ESwitch s_ReuseAddress = eOff;       /* off by default                */

/* I/O restart on signals */
static ESwitch s_InterruptOnSignal = eOff;  /* restart I/O by default        */

/* Data/event logging */
static ESwitch s_Log = eOff;                /* no logging by default         */

/* Select restart timeout */
static const struct timeval* s_SelectTimeout = 0; /* =0 (disabled) by default*/

/* Flag to indicate whether API should mask SIGPIPE (during initialization)  */
#ifdef NCBI_OS_UNIX
static int/*bool*/ s_AllowSigPipe = 0/*false - mask SIGPIPE out*/;
#endif /*NCBI_OS_UNIX*/

/* IP version: UNSPEC means accept both IPv4 and IPv6 */
static int s_IPVersion = AF_UNSPEC;

/* SSL support */
static SOCKSSL            s_SSL;
static FSSLSetup volatile s_SSLSetup;

/* External error reporting */
static FSOCK_ErrHook volatile s_ErrHook;
static void*         volatile s_ErrData;

/* Connection approval hook (IP/host/port/type/side) */
static FSOCK_ApproveHook volatile s_ApproveHook;
static void*             volatile s_ApproveData;



/******************************************************************************
 *  ERROR REPORTING
 */

#define NCBI_INCLUDE_STRERROR_C
#include "ncbi_strerror.c"


static const char* s_StrError(SOCK sock,
                              int  error)
{
    if (!error)
        return 0;

    if (sock  &&  error < 0) {
        FSSLError sslerror = sock->sslctx  &&  s_SSL ? s_SSL->Error : 0;
        if (sslerror) {
            char errbuf[256];
            const char* strerr = sslerror(sock->sslctx->sess, error,
                                          errbuf, sizeof(errbuf));
            if (strerr  &&  *strerr)
                return ERR_STRDUP(strerr);
        }
    }
    return s_StrErrorInternal(error);
}


#ifdef NCBI_OS_MSWIN
/* To dechiper GetLastError() results only */
static const char* s_WinStrerror(DWORD error)
{
    TCHAR* str;
    DWORD  rv;

    if (!error)
        return 0;
    str = NULL;
    rv  = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                        FORMAT_MESSAGE_FROM_SYSTEM     |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, error,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPTSTR) &str, 0, NULL);
    if (!rv  &&  str) {
        LocalFree((HLOCAL) str);
        return 0;
    }
    return UTIL_TcharToUtf8OnHeap(str);
}
#endif /*NCBI_OS_MSWIN*/


static void s_ErrorCallback(const SSOCK_ErrInfo* info)
{
    FSOCK_ErrHook hook;
    void*         data;

    CORE_LOCK_READ;
    hook = s_ErrHook;
    data = s_ErrData;
    CORE_UNLOCK;
    if (hook)
        hook(info, data);
}



/******************************************************************************
 *  API INITIALIZATION, SHUTDOWN/CLEANUP, and UTILITY
 */


#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  if !defined(__GNUC__)  &&  !defined(offsetof)
#    define offsetof(T, F)  ((size_t)((char*) &(((T*) 0)->F) - (char*) 0))
#  endif
#endif /*_DEBUG && !NDEBUG*/

#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  define SOCK_HAVE_SHOWDATALAYOUT  1
#endif /*__GNUC__ && _DEBUG && !NDEBUG*/

#ifdef SOCK_HAVE_SHOWDATALAYOUT

#  define   extentof(T, F)  (sizeof(((T*) 0)->F))

#  define   infof(T, F)     (unsigned int) offsetof(T, F), \
                            (unsigned int) extentof(T, F)

static void x_ShowDataLayout(void)
{
    static const char kLayoutFormat[] = {
        "SOCK data layout:\n"
        "    Sizeof(TRIGGER_struct) = %u\n"
        "    Sizeof(LSOCK_struct) = %u\n"
        "    Sizeof(SOCK_struct) = %u, offsets (sizes) follow\n"
        "\tsock:      %3u (%u)\n"
        "\tid:        %3u (%u)\n"
        "\tisset:     %3u (%u)\n"
        "\thost_:     %3u (%u)\n"
        "\tport:      %3u (%u)\n"
        "\tmyport:    %3u (%u)\n"
        "\terr:       %3u (%u)\n"
        "\tbitfield:      (4)\n"
#  ifdef NCBI_OS_MSWIN
        "\tevent:     %3u (%u)\n"
#  endif /*NCBI_OS_MSWIN*/
        "\tsslctx:    %3u (%u)\n"
        "\taddr:      %3u (%u)\n"
        "\tr_tv:      %3u (%u)\n"
        "\tw_tv:      %3u (%u)\n"
        "\tc_tv:      %3u (%u)\n"
        "\tr_to:      %3u (%u)\n"
        "\tw_to:      %3u (%u)\n"
        "\tc_to:      %3u (%u)\n"
        "\tr_buf:     %3u (%u)\n"
        "\tw_buf:     %3u (%u)\n"
        "\tr_len:     %3u (%u)\n"
        "\tw_len:     %3u (%u)\n"
        "\tn_read:    %3u (%u)\n"
        "\tn_written: %3u (%u)\n"
        "\tn_in:      %3u (%u)\n"
        "\tn_out:     %3u (%u)"
#  ifdef NCBI_OS_UNIX
        "\n\tpath:      %3u (%u)"
#  endif /*NCBI_OS_UNIX*/
    };
#  ifdef NCBI_OS_MSWIN
#    define SOCK_SHOWDATALAYOUT_PARAMS              \
        infof(SOCK_struct,    sock),                \
        infof(SOCK_struct,    id),                  \
        infof(TRIGGER_struct, isset),               \
        infof(SOCK_struct,    host_),               \
        infof(SOCK_struct,    port),                \
        infof(SOCK_struct,    myport),              \
        infof(SOCK_struct,    err),                 \
        infof(SOCK_struct,    event),               \
        infof(SOCK_struct,    sslctx),              \
        infof(SOCK_struct,    addr),                \
        infof(SOCK_struct,    r_tv),                \
        infof(SOCK_struct,    w_tv),                \
        infof(SOCK_struct,    c_tv),                \
        infof(SOCK_struct,    r_to),                \
        infof(SOCK_struct,    w_to),                \
        infof(SOCK_struct,    c_to),                \
        infof(SOCK_struct,    r_buf),               \
        infof(SOCK_struct,    w_buf),               \
        infof(SOCK_struct,    r_len),               \
        infof(SOCK_struct,    w_len),               \
        infof(SOCK_struct,    n_read),              \
        infof(SOCK_struct,    n_written),           \
        infof(SOCK_struct,    n_in),                \
        infof(SOCK_struct,    n_out)
#  else
#    define SOCK_SHOWDATALAYOUT_PARAMS              \
        infof(SOCK_struct,    sock),                \
        infof(SOCK_struct,    id),                  \
        infof(TRIGGER_struct, isset),               \
        infof(SOCK_struct,    host_),               \
        infof(SOCK_struct,    port),                \
        infof(SOCK_struct,    myport),              \
        infof(SOCK_struct,    err),                 \
        infof(SOCK_struct,    sslctx),              \
        infof(SOCK_struct,    addr),                \
        infof(SOCK_struct,    r_tv),                \
        infof(SOCK_struct,    w_tv),                \
        infof(SOCK_struct,    c_tv),                \
        infof(SOCK_struct,    r_to),                \
        infof(SOCK_struct,    w_to),                \
        infof(SOCK_struct,    c_to),                \
        infof(SOCK_struct,    r_buf),               \
        infof(SOCK_struct,    w_buf),               \
        infof(SOCK_struct,    r_len),               \
        infof(SOCK_struct,    w_len),               \
        infof(SOCK_struct,    n_read),              \
        infof(SOCK_struct,    n_written),           \
        infof(SOCK_struct,    n_in),                \
        infof(SOCK_struct,    n_out),               \
        infof(SOCK_struct,    path)
#  endif /*NCBI_OS_MSWIN*/
    CORE_LOGF_X(2, eLOG_Trace,
                (kLayoutFormat,
                 (unsigned int) sizeof(TRIGGER_struct),
                 (unsigned int) sizeof(LSOCK_struct),
                 (unsigned int) sizeof(SOCK_struct),
                 SOCK_SHOWDATALAYOUT_PARAMS));
#  undef SOCK_SHOWDATALAYOUT_PARAMS
}

#endif /*SOCK_HAVE_SHOWDATALAYOUT*/


static EIO_Status s_Init(void)
{
    CORE_TRACE("[SOCK::InitializeAPI]  Enter");

    CORE_LOCK_WRITE;

    if (s_Initialized) {
        CORE_UNLOCK;
        CORE_TRACE("[SOCK::InitializeAPI]  No-op");
        return s_Initialized < 0 ? eIO_NotSupported : eIO_Success;
    }

#ifdef SOCK_HAVE_SHOWDATALAYOUT
    if (s_Log == eOn)
        x_ShowDataLayout();
#endif /*SOCK_HAVE_SHOWDATALAYOUT*/

#if defined(_DEBUG)  &&  !defined(NDEBUG)
    /* Layout / alignment sanity checks */
    assert(sizeof(((struct sockaddr_in*) 0)->sin_addr.s_addr) == sizeof(unsigned int));
    assert(sizeof(((struct sockaddr_in6*) 0)->sin6_addr)      == sizeof(TNCBI_IPv6Addr));
    assert(sizeof(TRIGGER_Handle)        == sizeof(TSOCK_Handle));
    assert(offsetof(TRIGGER_struct, err) == offsetof(SOCK_struct,  err));
    assert(offsetof(TRIGGER_struct, err) == offsetof(LSOCK_struct, err));
    assert(offsetof(SOCK_struct, port)   == offsetof(LSOCK_struct, port));
    assert(offsetof(SOCK_struct, sslctx) == offsetof(LSOCK_struct, context));
#  ifdef NCBI_OS_MSWIN
    assert(offsetof(SOCK_struct, event)  == offsetof(LSOCK_struct, event));
    assert(WSA_INVALID_EVENT == 0);
#  endif /*NCBI_OS_MSWIN*/
    assert(offsetof(SOCK_struct, addr)   == offsetof(LSOCK_struct, addr));
#endif /*_DEBUG && !NDEBUG*/

#if   defined(NCBI_OS_UNIX)
if (!s_AllowSigPipe) {
    struct sigaction sa;
    if (sigaction(SIGPIPE, 0, &sa) != 0  ||  sa.sa_handler == SIG_DFL) {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sa, 0);
    }
}
#elif defined(NCBI_OS_MSWIN)
    {{
        WSADATA wsadata;
        int error = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (error
            ||  LOBYTE(wsadata.wVersion) != 2
            ||  HIBYTE(wsadata.wVersion) != 2) {
            const char* strerr;

            CORE_UNLOCK;
            strerr = SOCK_STRERROR(error ? error : WSAVERNOTSUPPORTED);
            CORE_LOG_ERRNO_EXX(3, eLOG_Error,
                               error, strerr ? strerr : "",
                               "[SOCK::InitializeAPI] "
                               " Failed WSAStartup()");
            UTIL_ReleaseBuffer(strerr);
            return eIO_NotSupported;
        }
    }}
#endif /*NCBI_OS*/

    s_Initialized = 1/*inited*/;

    CORE_UNLOCK;

#ifndef NCBI_OS_MSWIN
    {{
        static void* volatile /*bool*/ s_AtExitSet = 0/*false*/;
        if (CORE_Once(&s_AtExitSet)
            &&  atexit((void (*)(void)) SOCK_ShutdownAPI) != 0) {
            CORE_LOG_ERRNO_X(161, eLOG_Error, errno,
                             "Failed to register exit handler");
        }
    }}
#endif /*!NCBI_OS_MSWIN*/

    CORE_TRACE("[SOCK::InitializeAPI]  Leave");
    return eIO_Success;
}


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static EIO_Status s_Recv(SOCK,       void*, size_t, size_t*, int);
static EIO_Status s_Send(SOCK, const void*, size_t, size_t*, int);
#ifdef __cplusplus
}
#endif /*__cplusplus*/


static EIO_Status s_InitAPI_(int/*bool*/ secure)
{
    static const struct SOCKSSL_struct kNoSSL = { "", 0 };
    EIO_Status status;

    if (!s_Initialized  &&  (status = s_Init()) != eIO_Success)
        return status;

    assert(s_Initialized);

    if (s_Initialized < 0)
        return eIO_NotSupported;

    if (!secure)
        return eIO_Success;

    if (s_SSL)
        return s_SSL == &kNoSSL ? eIO_NotSupported : eIO_Success;

    if (s_SSLSetup) {
        const char* what = 0;
        CORE_LOCK_WRITE;
        if (!s_SSL) {
            SOCKSSL ssl;
            if (!s_SSLSetup  ||  !(ssl = s_SSLSetup())) {
                what   = (const char*)(-1L);
                s_SSL  = &kNoSSL;
                status = eIO_NotSupported;
            } else {
                what   = ssl->Name;
                s_SSL  = ((status = ssl->Init(s_Recv, s_Send)) == eIO_Success
                          ? ssl : &kNoSSL);
            }
        } else
            status = s_SSL == &kNoSSL ? eIO_NotSupported : eIO_Success;
        CORE_UNLOCK;
        if (status != eIO_Success  &&  what) {
            const char* provider;
            char buf[40];
            if (what != (const char*)(-1L)) {
                provider = *what ? what : "???";
                what = "initialize";
            } else if (!s_SSLSetup) {
                provider = "";
                what = "re-initialize";
            } else {
                sprintf(buf, "%p()", s_SSLSetup);
                provider = buf;
                what = "setup";
            }
            CORE_LOGF(eLOG_Critical,
                      ("Failed to %s SSL provider%s%s: %s", what,
                       &" "[!*provider], provider, IO_StatusStr(status)));
        }
    } else {
        static void* volatile /*bool*/ s_Once = 0/*false*/;
        if (CORE_Once(&s_Once)) {
            CORE_LOG(eLOG_Critical, "Secure Socket Layer (SSL) has not"
                     " been properly initialized in the NCBI Toolkit. "
                     " Have you forgotten to call SOCK_SetupSSL[Ex]()?");
        }
        status = eIO_NotSupported;
    }

    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static EIO_Status s_InitAPI(int/*bool*/ secure)
{
    EIO_Status status = s_InitAPI_(secure);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrInit;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


/* Must be called under a lock */
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static void x_ShutdownSSL(void)
{
    FSSLExit sslexit = s_Initialized > 0  &&  s_SSL ? s_SSL->Exit : 0;
    s_SSL      = 0;
    s_SSLSetup = 0;
    if (sslexit)
        sslexit();
}


extern EIO_Status SOCK_InitializeAPI(void)
{
    EIO_Status status = s_Init();
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrInit;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status SOCK_ShutdownAPI(void)
{
    if (s_Initialized < 0)
        return eIO_Success;

    CORE_TRACE("[SOCK::ShutdownAPI]  Enter");

    CORE_LOCK_WRITE;

    if (s_Initialized <= 0) {
        CORE_UNLOCK;
        return eIO_Success;
    }

    x_ShutdownSSL();

    s_Initialized = -1/*deinited*/;

#ifdef NCBI_OS_MSWIN
    {{
        int error = WSACleanup() ? SOCK_ERRNO : 0;
        CORE_UNLOCK;
        if (error) {
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOG_ERRNO_EXX(4, eLOG_Warning,
                               error, strerr ? strerr : "",
                               "[SOCK::ShutdownAPI] "
                               " Failed WSACleanup()");
            UTIL_ReleaseBuffer(strerr);
            return eIO_NotSupported;
        }
    }}
#else
    CORE_UNLOCK;
#endif /*NCBI_OS_MSWIN*/

    CORE_TRACE("[SOCK::ShutdownAPI]  Leave");
    return eIO_Success;
}



/******************************************************************************
 *  gethost[by]...() WRAPPERS
 */


static int s_gethostname(char*   buf,
                         size_t  bufsize,
                         ESwitch log)
{
    int/*bool*/ failed;

    CORE_TRACE("[SOCK::gethostname]");

    buf[0] = buf[bufsize - 1] = '\0';
    if (gethostname(buf, WIN_INT_CAST bufsize) != 0) {
        if (log) {
            int error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOG_ERRNO_EXX(103, eLOG_Error,
                               error, strerr ? strerr : "",
                               "[SOCK_gethostname] "
                               " Failed gethostname()");
            UTIL_ReleaseBuffer(strerr);
        }
        failed = 1/*true*/;
    } else if (buf[bufsize - 1]) {
        if (log) {
            CORE_LOGF_X(104, eLOG_Error,
                        ("[SOCK_gethostname] "
                         " Buffer too small (%lu) for \"%.*s\"",
                         (unsigned long) bufsize, (int) bufsize, buf));
        }
        failed = 1/*true*/;
        errno = SOCK_ENOSPC;
    } else if (NCBI_HasSpaces(buf, bufsize = strlen(buf))) {
        if (log) {
            CORE_LOGF_X(162, eLOG_Error,
                        ("[SOCK_gethostname] "
                         " Hostname with spaces \"%s\"", buf));
        }
        failed = 1/*true*/;
        errno = EINVAL;
    } else {
        failed = 0/*false*/;
        if (!*buf)
            errno = ENOTSUP;
    }

    CORE_TRACEF(("[SOCK::gethostname] "
                 " \"%.*s\"%s", (int) bufsize, buf,
                 failed ? " (failed)" : ""));
    if (failed)
        *buf = '\0';
    return *buf ? 0/*success*/ : -1/*failure*/;
}


static char* s_AddrToString(char*                 buf,
                            size_t                bufsize,
                            const TNCBI_IPv6Addr* addr,
                            int                   family,
                            int/*bool*/           empty)
{
    char* rv;
    if (!empty  &&  family == AF_UNSPEC)
        rv = NcbiAddrToString(buf, bufsize, addr);
    else if ((empty  ||  NcbiIsIPv4(addr))  &&  family != AF_INET6)
        rv = NcbiIPv4ToString(buf, bufsize, NcbiIPv6ToIPv4(addr, 0));
    else
        rv = NcbiIPv6ToString(buf, bufsize, addr);
    assert(rv  ||  bufsize < SOCK_ADDRSTRLEN);
    return rv;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsAPIPA(unsigned int ip/*host byte order*/)
{
    return !((ip & 0xFFFF0000) ^ 0xA9FE0000); /* 169.254/16 per IANA */
}


static const char* x_ChooseSelfIP(char** addrs)
{
    int n;
    for (n = 0;  addrs[n];  ++n) {
        unsigned int ip;
        memcpy(&ip, addrs[n], sizeof(ip));
        if (!SOCK_IsLoopbackAddress(ip)  &&  !x_IsAPIPA(ntohl(ip)))
            return addrs[n];
    }
    return addrs[0];
}


static TNCBI_IPv6Addr* s_gethostbyname_(TNCBI_IPv6Addr* addr,
                                        const char*     host,
                                        int             family,
                                        int/*bool*/     not_ip,
                                        int/*bool*/     self,
                                        ESwitch         log)
{
    char buf[CONN_HOST_LEN + 1];
    const char* parsed;
    unsigned int ipv4;
    size_t len;

    assert(addr  &&  (!host  ||  *host));
    if (!host) {
        if (s_gethostname(buf, sizeof(buf), log) != 0) {
            memset(addr, 0, sizeof(*addr));
            return 0/*failure*/;
        }
        not_ip = 1/*true*/;
        assert(*buf);
        host = buf;
    }
    len = strlen(host);

    CORE_TRACEF(("[SOCK::gethostbyname%s]  \"%s\"",
                 family == AF_INET  ? "(IPv4)" :
                 family == AF_INET6 ? "(IPv6)" : "",
                 host));

    if (family != AF_INET6) {
#ifdef NCBI_OS_DARWIN
        if (strspn(host, ".0123456789") == len) {
            /* Darwin's inet_addr() does not care for integer overflows :-/ */
            if (!SOCK_isip(host)) {
                memset(addr, 0, sizeof(*addr));
                parsed = 0;
                goto done;
            }
        }
#endif /*NCBI_OS_DARWIN*/
        if (!not_ip  &&  (ipv4 = inet_addr(host)) != htonl(INADDR_NONE)) {
            NcbiIPv4ToIPv6(addr, ipv4, 0);
            parsed = host;
            goto done;
        }
    }

    if (NCBI_HasSpaces(host, len)) {
        memset(addr, 0, sizeof(*addr));
        parsed = 0;
        goto done;
    }

    parsed = NcbiStringToAddr(addr, host, len);
    if (!parsed) {
        assert(NcbiIsEmptyIPv6(addr));
    } else {
        if (!*parsed) {
            /* fully parsed address string */
            if (family == AF_INET6) {
                if (NcbiIsIPv4(addr))
                    parsed = 0;
            } else if (NcbiIsIPv4(addr)) {
                if (!NcbiIPv6ToIPv4(addr, 0))
                    parsed = 0;
            } else if (family == AF_INET)
                parsed = 0;
        } else
            parsed = 0; /* partially parsed still can't be good */
        if (!parsed)
            memset(addr, 0, sizeof(*addr));
        goto done;
    }

    {{
        int error;
#if defined(HAVE_GETADDRINFO)
        struct addrinfo hints, *out = 0;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = family;
        if ((error = getaddrinfo(host, 0, &hints, &out)) == 0  &&  out) {
            union {
                const struct sockaddr*     sa;
                const struct sockaddr_in*  in;
                const struct sockaddr_in6* in6;
            } u;
            ipv4 = 0;
            if (family != AF_INET6) {
                /* favor IPv4 over IPv6 in this case to maintain backward compatibility */
                const struct addrinfo* tmp = out;
                char* addrs[33];
                size_t n = 0;
                do {
                    u.sa = tmp->ai_addr;
                    if (u.sa->sa_family == AF_INET) {
                        addrs[n++] = (char*) &u.in->sin_addr;
                        if (!self)
                            break;
                    }
                    if (!(tmp = tmp->ai_next))
                        break;
                } while (n < sizeof(addrs) / sizeof(addrs[0]) - 1);
                if (n) {
                    memcpy(&ipv4, self
                           ? (addrs[n] = 0, x_ChooseSelfIP(addrs))
                           : addrs[0], sizeof(ipv4));
                    NcbiIPv4ToIPv6(addr, ipv4, 0);
                    assert(ipv4);
                }
            }
            if (!ipv4) {
                const struct addrinfo* tmp = out;
                if (self) {
                    do {
                        u.sa = tmp->ai_addr;
                        if (u.sa->sa_family == AF_INET6) {
                            const TNCBI_IPv6Addr* temp
                                = (const TNCBI_IPv6Addr*) &u.in6->sin6_addr;
                            /* skip specials (like loopback,
                               multicast and link-local) */
                            if ((temp->octet[0] & 0xE0)  &&
                                (temp->octet[0] ^ 0xE0)) {
                                break;
                            }
                        }
                        tmp = tmp->ai_next;
                    } while (tmp);
                    if (!tmp)
                        tmp = out;
                }
                u.sa = tmp->ai_addr;
                if (u.sa->sa_family == AF_INET6) {
                    memcpy(addr, &u.in6->sin6_addr, sizeof(*addr));
                    assert(!NcbiIsEmptyIPv6(addr));
                }
                assert(family == AF_INET6  ||  family == AF_UNSPEC);
            }
        } else {
            if (log) {
                const char* strerr;
#  ifdef EAI_SYSTEM
                if (error == EAI_SYSTEM)
                    error  = SOCK_ERRNO;
                else
#  endif /*EAI_SYSTEM*/
                if (error)
                    error += EAI_BASE;
                else
                    error  = EFAULT;
                strerr = SOCK_STRERROR(error);
                CORE_LOGF_ERRNO_EXX(105, eLOG_Warning,
                                    error, strerr ? strerr : "",
                                    ("[SOCK_gethostbyname] "
                                     " Failed getaddrinfo(\"%.*s\")",
                                     CONN_HOST_LEN, host));
                UTIL_ReleaseBuffer(strerr);
            }
        }
        if (out)
            freeaddrinfo(out);
#else /* use some variant of gethostbyname */
        struct hostent* he;
#  ifdef NCBI_HAVE_GETHOSTBYNAME_R
        static const char suffix[] = "_r";
        struct hostent x_he;
        char x_buf[1024];

        error = 0;
#    if   NCBI_HAVE_GETHOSTBYNAME_R == 5
        he = gethostbyname_r(host, &x_he, x_buf, sizeof(x_buf), &error);
#    elif NCBI_HAVE_GETHOSTBYNAME_R == 6
        if (gethostbyname_r(host, &x_he, x_buf, sizeof(x_buf), &he, &error) != 0) {
            /*NB: retval == errno on error*/
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown NCBI_HAVE_GETHOSTBYNAME_R value"
#    endif /*NCBI_HAVE_GETHOSTNBYNAME_R == N*/
        if (!he) {
            if (!error)
                error = SOCK_ERRNO;
            else
                error += DNS_BASE;
        }
#  else
        static const char suffix[] = "";

#    ifndef SOCK_GHBX_MT_SAFE
        CORE_LOCK_WRITE;
#    endif /*!SOCK_GHBX_MT_SAFE*/

        he = gethostbyname(host);
        error = he ? 0 : h_errno + DNS_BASE;
#  endif /*NCBI_HAVE_GETHOSTBYNAME_R*/

        if (!he
            ||  (family == AF_INET6  &&  he->h_addrtype == AF_INET)
            ||  (family == AF_INET   &&  he->h_addrtype == AF_INET6)) {
            if (he) {
                error = ENOTSUP;
                he = 0;
            }
        } else switch (he->h_addrtype) {
        case AF_INET:
            if (he->h_length != sizeof(ipv4)) {
                error = EINVAL;
                he = 0;
            } else {
                memcpy(&ipv4,
                       self ? x_ChooseSelfIP(he->h_addr_list) : he->h_addr,
                       sizeof(ipv4));
                NcbiIPv4ToIPv6(addr, ipv4, 0);
            }
            break;
        case AF_INET6:
            if (he->h_length != sizeof(*addr)) {
                error = EINVAL;
                he = 0;
            } else
                memcpy(addr, he->h_addr, sizeof(*addr));
            break;
        default:
            error = ENOTSUP;
            he = 0;
            break;
        }

#  ifndef NCBI_HAVE_GETHOSTBYNAME_R
#    ifndef SOCK_GHBX_MT_SAFE
        CORE_UNLOCK;
#    endif /*!SOCK_GHBX_MT_SAFE*/
#  endif /*!NCBI_HAVE_GETHOSTBYNAME_R*/

        if (!he) {
#  ifdef NETDB_INTERNAL
            if (error == NETDB_INTERNAL + DNS_BASE)
                error  = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            if (error == ERANGE)
                log = eOn;
            if (log) {
                const char* strerr = SOCK_STRERROR(error);
                CORE_LOGF_ERRNO_EXX(106, eLOG_Warning,
                                    error, strerr ? strerr : "",
                                    ("[SOCK_gethostbyname] "
                                     " Failed gethostbyname%s(\"%.*s\")",
                                     suffix, CONN_HOST_LEN, host));
                UTIL_ReleaseBuffer(strerr);
            }
        }
#endif /*HAVE_GETADDRINFO*/
    }}

    parsed = NcbiIsEmptyIPv6(addr) ? 0 : host;

  done:
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    if (!SOCK_isipEx(host, 1/*full-quad*/)) {
        char addrstr[SOCK_ADDRSTRLEN];
        s_AddrToString(addrstr, sizeof(addrstr), addr, family, !parsed);
        CORE_TRACEF(("[SOCK::gethostbyname%s]  \"%s\" @ %s",
                     family == AF_INET  ? "(IPv4)" :
                     family == AF_INET6 ? "(IPv6)" : "",
                     host, addrstr));
    }
#endif /*_DEBUG && !NDEBUG*/
    return parsed ? addr/*success*/ : 0/*failure*/;
}


/* a non-standard helper */
static TNCBI_IPv6Addr* s_getlocalhostaddress(TNCBI_IPv6Addr* addr,
                                             int             family,
                                             ESwitch         reget,
                                             ESwitch         log)
{
    /* Cached IP addresses of the local host [0]=IPv4, [1]=IPv6 */
    static volatile struct {
        TNCBI_IPv6Addr addr;
        int/*bool*/    isset;
    } s_LocalHostAddress[2] = { 0 };
    static void* volatile /*bool*/ s_Once = 0/*false*/;
    int/*bool*/ empty;
    int n;

    assert(addr);
    n = !(family != AF_INET6);
    if (reget != eOn) {
        int i, m = family == AF_UNSPEC ? 2 : 1;
        for (i = 0;  i < m;  ++i) {
            int isset;
            CORE_LOCK_READ;
            isset = s_LocalHostAddress[n].isset;
            *addr = s_LocalHostAddress[n].addr;
            CORE_UNLOCK;
            if (!(i | isset)) {
                assert(NcbiIsEmptyIPv6(addr));
                reget = eOn;
                break;
            }
            if (!(empty = NcbiIsEmptyIPv6(addr)))
                return addr;
            if (!(m & 1))
                n ^= 1;
        }
    }
    if (reget != eOff) {
        if (!(empty = !s_gethostbyname_(addr, 0, family, 0, 1/*self*/, log))) {
            assert(!NcbiIsEmptyIPv6(addr));
            n = !NcbiIsIPv4(addr);
        }
        CORE_LOCK_WRITE;
        s_LocalHostAddress[n].addr = *addr;
        s_LocalHostAddress[n].isset = 1;
        CORE_UNLOCK;
    }
    if (!empty)
        return addr;
    assert(NcbiIsEmptyIPv6(addr));
    if (reget != eOff  &&  CORE_Once(&s_Once)) {
        CORE_LOGF_X(9, reget == eDefault ? eLOG_Warning : eLOG_Error,
                    ("[SOCK::GetLocalHostAddress%s] "
                     " Cannot obtain local host address%s",
                     family == AF_INET  ? "(IPv4)" :
                     family == AF_INET6 ? "(IPv6)" : "",
                     reget == eDefault ? ", using loopback instead" : ""));
    }
    if (reget == eDefault) {
        if (family == AF_INET6)
           SOCK_GetLoopbackAddress6(addr);
        else
           NcbiIPv4ToIPv6(addr, SOCK_LOOPBACK, 0);
    } else
        addr = 0/*failure*/;
    return addr;
}


static TNCBI_IPv6Addr* s_gethostbyname(TNCBI_IPv6Addr* addr,
                                       const char*     host,
                                       int             family,
                                       int/*bool*/     not_ip,
                                       ESwitch         log)
{
    static void* volatile /*bool*/ s_Once = 0/*false*/;

    assert(addr);
    if (host  &&  !*host)
        host = 0;

    if (!s_gethostbyname_(addr, host, family, not_ip, 0/*any*/, log)) {
        if (s_ErrHook) {
            SSOCK_ErrInfo info;
            memset(&info, 0, sizeof(info));
            info.type = eSOCK_ErrDns;
            info.host = host;
            s_ErrorCallback(&info);
        }
        assert(NcbiIsEmptyIPv6(addr));
        addr = 0;
    } else if (!s_Once  &&  !host
               &&  SOCK_IsLoopbackAddress6(addr)  &&  CORE_Once(&s_Once)) {
        char addrstr[SOCK_ADDRSTRLEN];
        s_AddrToString(addrstr, sizeof(addrstr), addr, family, 0);
        CORE_LOGF_X(155, eLOG_Warning,
                    ("[SOCK::gethostbyname] "
                     " Got loopback address %s for local host name", addrstr));
    }

    return addr;
}


static char* s_gethostbyaddr_(const TNCBI_IPv6Addr* addr,
                              int                   family,
                              char*                 buf,
                              size_t                bufsize,
                              ESwitch               log)
{
    char addrstr[SOCK_ADDRSTRLEN];
    TNCBI_IPv6Addr localhost;
    int/*bool*/ empty;

    if (NcbiIsEmptyIPv6(addr)) {
        /* NB: this also includes the case of "addr" == NULL */
        empty = !s_getlocalhostaddress(&localhost, family, eDefault, log);
        assert(!empty == !NcbiIsEmptyIPv6(&localhost));
        addr = &localhost;
    } else
        empty = 0/*false*/;

    CORE_TRACEF(("[SOCK::gethostbyaddr%s]  %s",
                 family == AF_INET  ? "(IPv4)" :
                 family == AF_INET6 ? "(IPv6)" : "",
                 (s_AddrToString(addrstr, sizeof(addrstr),
                                 addr, family, empty), addrstr)));

    if (!empty) {
        int error = 0;
#if defined(HAVE_GETNAMEINFO)
        union {
            struct sockaddr     sa;
            struct sockaddr_in  in;
            struct sockaddr_in6 in6;
        } u;
        int ipv4 = NcbiIsIPv4(addr);
        TSOCK_socklen_t x_bufsize = bufsize > CONN_HOST_LEN
            ? CONN_HOST_LEN + 1 : (TSOCK_socklen_t) bufsize;
        TSOCK_socklen_t addrlen = ipv4 ? sizeof(u.in) : sizeof(u.in6);
        memset(&u, 0, addrlen);
        u.sa.sa_family = ipv4 ? AF_INET : AF_INET6;
#  ifdef HAVE_SIN_LEN
        u.sa.sa_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
        if (ipv4)
            u.in.sin_addr.s_addr = NcbiIPv6ToIPv4(addr, 0);
        else
            memcpy(&u.in6.sin6_addr, addr, sizeof(u.in6.sin6_addr));
        if ((ipv4  &&  u.in.sin_addr.s_addr == htonl(INADDR_NONE))
            ||  (error = getnameinfo(&u.sa, addrlen,
                                     buf, WIN_DWORD_CAST x_bufsize,
                                     0, 0, NI_NAMEREQD)) != 0
            ||  !*buf) {
            if (!s_AddrToString(buf, bufsize, addr, family, 0)) {
                if (error == EAI_NONAME)
                    error  = 0;
#  ifdef EAI_SYSTEM
                if (error == EAI_SYSTEM)
                    error  = SOCK_ERRNO;
                else
#  endif /*EAI_SYSTEM*/
                if (error)
                    error += EAI_BASE;
                else {
                    error  = SOCK_ENOSPC;
                    log = eOn;
                }
                buf[0] = '\0';
                buf = 0;
            }
            if (!buf  &&  log) {
                const char* strerr = SOCK_STRERROR(error);
                s_AddrToString(addrstr, sizeof(addrstr), addr, family, 0);
                CORE_LOGF_ERRNO_EXX(107, eLOG_Warning,
                                    error, strerr ? strerr : "",
                                    ("[SOCK_gethostbyaddr] "
                                     " Failed getnameinfo(%s)",
                                     addrstr));
                UTIL_ReleaseBuffer(strerr);
            }
        }
#else /* use some variant of gethostbyaddr */
        size_t len;
        struct hostent* he;
        int ipv4 = NcbiIsIPv4(addr);
        unsigned int temp = ipv4 ? NcbiIPv6ToIPv4(addr, 0) : 0;
        TSOCK_socklen_t addrlen = ipv4 ? sizeof(temp) : sizeof(*addr);
        char* x_addr = ipv4 ? (char*) &temp : (char*) addr;
#  ifdef NCBI_HAVE_GETHOSTBYADDR_R
        static const char suffix[] = "_r";
        struct hostent x_he;
        char x_buf[1024];

#    if   NCBI_HAVE_GETHOSTBYADDR_R == 7
        he = gethostbyaddr_r(x_addr, addrlen, ipv4 ? AF_INET : AF_INET6,
                             &x_he, x_buf, sizeof(x_buf), &error);
#    elif NCBI_HAVE_GETHOSTBYADDR_R == 8
        if (gethostbyaddr_r(x_addr, addrlen, ipv4 ? AF_INET : AF_INET6,
                            &x_he, x_buf, sizeof(x_buf), &he, &error) != 0) {
            /*NB: retval == errno on error*/
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown NCBI_HAVE_GETHOSTBYADDR_R value"
#    endif /*NCBI_HAVE_GETHOSTBYADDR_R == N*/
        if (!he) {
            if (!error)
                error = SOCK_ERRNO;
            else
                error += DNS_BASE;
        }
#  else /*NCBI_HAVE_GETHOSTBYADDR_R*/
        static const char suffix[] = "";

#    ifndef SOCK_GHBX_MT_SAFE
        CORE_LOCK_WRITE;
#    endif /*!SOCK_GHBX_MT_SAFE*/

        he = gethostbyaddr(x_addr, addrlen, ipv4 ? AF_INET : AF_INET6);
        error = he ? 0 : h_errno + DNS_BASE;
#  endif /*NCBI_HAVE_GETHOSTBYADDR_R*/

        if (ipv4  &&  NcbiIPv6ToIPv4(addr, 0) == htonl(INADDR_NONE))
            he = 0;
        if (!he  ||  (len = strlen(he->h_name)) >= bufsize) {
            if (he  ||  !s_AddrToString(buf, bufsize, addr, family, 0)) {
                error = SOCK_ENOSPC;
                log = eOn;
                buf[0] = '\0';
                buf = 0;
            }
        } else
            memcpy(buf, he->h_name, len + 1);

#  ifndef NCBI_HAVE_GETHOSTBYADDR_R
#    ifndef SOCK_GHBX_MT_SAFE
        CORE_UNLOCK;
#    endif /*!SOCK_GHBX_MT_SAFE*/
#  endif /*!NCBI_HAVE_GETHOSTBYADDR_R*/

        if (!buf) {
#  ifdef NETDB_INTERNAL
            if (error == NETDB_INTERNAL + DNS_BASE)
                error  = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            if (error == ERANGE)
                log = eOn;
            if (log) {
                const char* strerr = SOCK_STRERROR(error);
                s_AddrToString(addrstr, sizeof(addrstr), addr, family, 0);
                CORE_LOGF_ERRNO_EXX(108, eLOG_Warning,
                                    error, strerr ? strerr : "",
                                    ("[SOCK_gethostbyaddr] "
                                     " Failed gethostbyaddr%s(%s)",
                                     suffix, addrstr));
                UTIL_ReleaseBuffer(strerr);
            }
        }
#endif /*HAVE_GETNAMEINFO && !__GLIBC__*/
    } else {
        buf[0] = '\0';
        buf = 0;
    }

    assert(!buf  ||  (*buf  &&  !NCBI_HasSpaces(buf, strlen(buf))));
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    if (!SOCK_IsAddress(buf)) {
        CORE_TRACEF(("[SOCK::gethostbyaddr%s]  %s @ %s%s%s",
                        family == AF_INET  ? "(IPv4)" :
                        family == AF_INET6 ? "(IPv6)" : "",
                        (s_AddrToString(addrstr, sizeof(addrstr),
                                        addr, family, empty), addrstr),
                        &"\""[!buf], buf ? buf : "(none)", &"\""[!buf]));
    }
#endif /*_DEBUG && !NDEBUG*/
    return buf;
}


static const char* s_gethostbyaddr(const TNCBI_IPv6Addr* addr,
                                   char*                 buf,
                                   size_t                bufsize,
                                   ESwitch               log)
{
    static void* volatile /*bool*/ s_Once = 0/*false*/;
    const char* rv;
    assert(buf  &&  bufsize);
    rv = s_gethostbyaddr_(addr, s_IPVersion, buf, bufsize, log);
    if (!s_Once  &&  rv
        &&  ((SOCK_IsLoopbackAddress6(addr)
              &&  strncasecmp(rv, "localhost", 9) != 0)  ||
             (NcbiIsEmptyIPv6(addr)
              &&  strncasecmp(rv, "localhost", 9) == 0))
        &&  CORE_Once(&s_Once)) {
        CORE_LOGF_X(10, eLOG_Warning,
                    ("[SOCK::gethostbyaddr] "
                     " Got \"%.*s\" for %s address", CONN_HOST_LEN,
                     rv, NcbiIsEmptyIPv6(addr) ? "local host" : "loopback"));
    }
    return rv;
}


static const char* s_StringToHostPort(const char*     str,
                                      TNCBI_IPv6Addr* addr,
                                      int             family,
                                      unsigned short* port,
                                      int/*bool*/     flag)
{
    char x_buf[CONN_HOST_LEN + 1];
    const char *s, *t, *q;
    TNCBI_IPv6Addr temp;
    unsigned short p;

    if ( addr )
        memset(addr, 0, sizeof(*addr));
    if ( port )
        *port = 0;
    if (!str)
        return 0;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return 0;

    for (s = str;  *s;  ++s) {
        if (!isspace((unsigned char)(*s)))
            break;
    }
    if (!*s)
        return str;
    if (*s == '[') {
        if (!(t = strchr(++s, ']'))  ||  t == s)
            return str;
        if (!(q = NcbiIPToAddr(&temp, s, (size_t)(t - s))))
            return str;
        if (q++ != t)
            return str;
        if (*q != ':')
            return str;
        s = 0;
    } else if ((t = NcbiIPToAddr(&temp, s, 0)) != 0) {
        q = t;
        s = 0;
    } else if ((q = strchr(s, ':')) == s) {
        if (family == AF_INET)
            NcbiIPv4ToIPv6(&temp, 0, 0);
        else
            memset(&temp, 0, sizeof(temp));
        s = 0;
    }

    if (q  &&  *q == ':') {
        long  i;
        char* e;
        if (!isdigit((unsigned char) q[1]))
            return str;
        errno = 0;
        i = strtol(++q, &e, 10);
        if (errno  ||  q == e  ||  (i ^ (i & 0xFFFF))
            ||  (*e  &&  !isspace((unsigned char)(*e)))) {
            return str;
        }
        p = (unsigned short) i;
        q = e;
    } else if (!q  ||  !*q  ||  isspace((unsigned char)(*q))) {
        assert(t  ||  s);
        p = 0;
        q = 0;
    } else
        return str;

    if (s) {
        size_t len;
        assert(!t);
        for (t = s;  *t  &&  *t != ':';  ++t) {
            if (isspace((unsigned char)(*t)))
                break;
        }
        assert(t > s);
        if ((len = (size_t)(t - s)) >= sizeof(x_buf))
            return 0;
        memcpy(x_buf, s, len);
        x_buf[len] = '\0';
        if (!s_gethostbyname(&temp, x_buf, family, 1/*not-IP*/, s_Log)) {
            if (!flag)
                return str;
            /* NB: "temp" has been already cleared all out at this point */
            NcbiIPv4ToIPv6(&temp, htonl(INADDR_NONE), family == AF_INET6 ? 96 : 0);
        }
    }
    if (family != AF_UNSPEC  &&  !NcbiIsEmptyIPv6(&temp)) {
        int/*bool*/ ipv4 = NcbiIsIPv4(&temp);
        assert(family == AF_INET  ||  family == AF_INET6);
        if ((ipv4  &&  family == AF_INET6)  ||  (!ipv4  &&  family == AF_INET))
            return str;
    }

    if ( addr )
        *addr = temp;
    if (port  &&  p)
        *port = p;
    return q ? q : t;
}


static size_t s_HostPortToStringEx(const TNCBI_IPv6Addr* addr,
                                   int                   family,
                                   unsigned short        port,
                                   char*                 buf,
                                   size_t                bufsize)
{
    size_t len;

    if (!buf  ||  !bufsize)
        return 0;
    if (addr) {
        int/*bool*/ ipv4 = NcbiIsIPv4(addr);
        if (!NcbiIsEmptyIPv6(addr)  ||  (!ipv4  &&  family != AF_INET)) {
            int/*bool*/ bare = (ipv4  &&  family != AF_INET6)  ||  !port;
            char* end = s_AddrToString(buf + !bare, bufsize - !bare, addr, family, 0);
            if (!end) {
                *buf = '\0';
                return 0;
            }
            if (!bare) {
                buf[0] = '[';
                *end++ = ']';
            }
            len = (size_t)(end - buf);
            assert(len < bufsize);
        } else
            len = 0;
    } else
        len = 0;
    if (port  ||  !len) {
        char   x_buf[10];
        size_t x_len = (size_t) sprintf(x_buf, ":%hu", port);
        if (len + x_len < bufsize) {
            memcpy(buf + len, x_buf, x_len);
            len += x_len;
        } else
            len = 0;
    }
    buf[len] = '\0';
    return len;
}

#define s_HostPortToString(a,b,c,d)  s_HostPortToStringEx((a),s_IPVersion,(b),(c),(d))



/******************************************************************************
 *  DATA LOGGING
 */


static unsigned int x_ID_Counter(void)
{
    unsigned int id;
    CORE_LOCK_WRITE;
    id = ++s_ID_Counter;
    CORE_UNLOCK;
    return id;
}


static const char* s_CP(const TNCBI_IPv6Addr* addr,
                        unsigned short        port,
                        const char*           path,
                        char*                 buf,
                        size_t                bufsize)
{
    if (path[0])
        return path;
    if (NcbiIsEmptyIPv6(addr)  &&  !port)
        return "";
    assert(bufsize > SOCK_HOSTPORTSTRLEN);
    s_HostPortToString(addr, port, buf, bufsize);
    return buf;
}


static const char* s_CPListening(const TNCBI_IPv6Addr* addr,
                                 unsigned short        port,
                                 char*                 buf,
                                 size_t                bufsize)
{
    int/*bool*/ ipv4 = NcbiIsIPv4(addr);
    size_t len = NcbiIsEmptyIPv6(addr) ? 0
        : s_HostPortToString(addr, ipv4 ? 0 : port, buf + (!port  &&  !ipv4), bufsize - 8);
    if (!ipv4) {
        if (!len) {
            strcpy(buf + 1, "::");
            len = 2;
        } else if (port)
            return buf;
        len++;
        *buf = '[';
        buf[len++] = ']';
    }
    if (port)
        sprintf(buf + len, ":%hu", port);
    else
        strcpy(buf + len, ":?");
    return buf;
}


static const char* s_ID(const SOCK sock,
                        char       buf[MAXIDLEN])
{
    char addr[10 + SOCK_HOSTPORTSTRLEN];
    const char* sname;
    const char* cp;
    char fd[20];
    size_t len;
    int n;

    if (!sock)
        return "";
    switch (sock->type) {
    case eSOCK_Trigger:
        cp = "";
        sname = "TRIGGER";
        break;
    case eSOCK_Socket:
        cp = s_CP(&sock->addr, sock->port,
#ifdef NCBI_OS_UNIX
                  sock->path,
#else
                  "",
#endif /*NCBI_OS_UNIX*/
                  addr, sizeof(addr));
#ifdef NCBI_OS_UNIX
        if (sock->path[0])
            sname = sock->sslctx ? "SUSOCK" : "USOCK";
        else
#endif /*NCBI_OS_UNIX*/
            sname = sock->sslctx ? "SSOCK"  : g_kNcbiSockNameAbbr;
        break;
    case eSOCK_Datagram:
        sname = "DSOCK";
        addr[0] = '\0';
        n = sock->myport ? sprintf(addr, "(:%hu)", sock->myport) : 0;
        if (!NcbiIsEmptyIPv6(&sock->addr)  ||  sock->port) {
            s_HostPortToString(&sock->addr, sock->port,
                               addr + n, sizeof(addr) - (size_t) n);
        }
        cp = addr;
        break;
    case eSOCK_Listening:
#ifdef NCBI_OS_UNIX
        if (!sock->port)
            cp = ((LSOCK) sock)->path;
        else
#endif /*NCBI_OS_UNIX*/
            cp = s_CPListening(&sock->addr, sock->port, addr, sizeof(addr));
        sname = "LSOCK";
        break;
    default:
        cp = "";
        sname = "?";
        assert(0);
        break;
    }

    if (sock->sock != SOCK_INVALID)
        sprintf(fd, "%u", (unsigned int) sock->sock);
    else
        strcpy(fd, "?");
    len = cp  &&  *cp ? strlen(cp) : 0;
    n = (int)(len > sizeof(addr) - 1 ? sizeof(addr) - 1 : len);
    sprintf(buf, "%s#%u[%s]%s%s%.*s: ", sname, sock->id, fd,
            &"@"[!n], (size_t) n < len ? "..." : "", n, cp + len - n);
    assert(strlen(buf) < MAXIDLEN);
    return buf;
}


static unsigned short s_GetLocalPort(TSOCK_Handle fd)
{
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } u;
    TSOCK_socklen_t addrlen = sizeof(u);
    memset(&u, 0, sizeof(u.sa));
#ifdef HAVE_SIN_LEN
    u.sa.sa_len = addrlen;
#endif /*HAVE_SIN_LEN*/
    if (getsockname(fd, &u.sa, &addrlen) == 0) {
        switch (u.sa.sa_family) {
        case AF_INET:
            return ntohs(u.in.sin_port);
        case AF_INET6:
            return ntohs(u.in6.sin6_port);
        default:
            break;
        }
    }
    return 0;
}


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLog(ELOG_Level  level, const SOCK sock, EIO_Event   event,
                    const void* data,  size_t     size, const void* ptr)
{
    union {
        const struct sockaddr*     sa;
        const struct sockaddr_in*  in;
        const struct sockaddr_in6* in6;
    } u;
    const char* what, *strerr;
    char _id[MAXIDLEN];
    char head[128];
    char tail[128];
    int n;

    if (!CORE_GetLOG())
        return;

    assert(sock  &&  (sock->type & eSOCK_Socket));
    switch (event) {
    case eIO_Open:
        if (sock->type != eSOCK_Datagram) {
            unsigned short port;
            if (sock->side == eSOCK_Client) {
                what = ptr ? (const char*) ptr : "Connecting";
                strcpy(head, *what ? what : "Re-using");
                port = sock->myport;
            } else if (!ptr) {
                strcpy(head, "Accepted");
                port = 0;
            } else {
                strcpy(head, "Created");
                port = sock->myport;
            }
            if (!port) {
#ifdef NCBI_OS_UNIX
                if (!sock->path[0])
#endif /*NCBI_OS_UNIX*/
                    port = s_GetLocalPort(sock->sock);
            }
            if (port) {
                sprintf(tail, " @:%hu", port);
                if (!sock->myport) {
                    /* here: not LSOCK_Accept()'d network sockets only */
                    assert(sock->side == eSOCK_Client  ||  ptr);
                    sock->myport = port;  /*cache it*/
                }
            } else
                *tail = '\0';
        } else if (!(u.sa = (const struct sockaddr*) ptr)) {
            strcpy(head, "Created");
            *tail = '\0';
        } else if (!data) {
            unsigned short port;
            switch (u.sa->sa_family) {
            case AF_INET:
                port = ntohs(u.in->sin_port);
                break;
            case AF_INET6:
                port = ntohs(u.in6->sin6_port);
                break;
            default:
                port = 0;
                assert(0);
                break;
            }
            strcpy(head, "Bound @");
            sprintf(tail, "(:%hu)", port);
        } else if (u.sa->sa_family) {
            TNCBI_IPv6Addr addr;
            unsigned short port;
            switch (u.sa->sa_family) {
            case AF_INET:
                NcbiIPv4ToIPv6(&addr, u.in->sin_addr.s_addr, 0);
                port = ntohs(u.in->sin_port);
                break;
            case AF_INET6:
                memcpy(&addr, &u.in6->sin6_addr, sizeof(addr));
                port = ntohs(u.in6->sin6_port);
                break;
            default:
                memset(&addr, 0, sizeof(addr));
                port = 0;
                assert(0);
                break;
            }
            strcpy(head, "Associated ");
            s_HostPortToString(&addr, port, tail, sizeof(tail));
        } else {
            strcpy(head, "Disassociated");
            *tail = '\0';
        }
        CORE_LOGF_X(112, level,
                    ("%s%s%s", s_ID(sock, _id), head, tail));
        break;

    case eIO_Read:
    case eIO_Write:
        strerr = 0;
        what = (event == eIO_Read
                ? (sock->type == eSOCK_Datagram  ||  size
                   ||  (data  &&  !(strerr = s_StrError(sock, *((int*) data))))
                   ? "Read"
                   : data ? strerr : "EOF hit")
                : (sock->type == eSOCK_Datagram  ||  size
                   ||  !(strerr = s_StrError(sock, *((int*) data)))
                   ? "Written"
                   :        strerr));

        n = (int) strlen(what);
        while (n  &&  isspace((unsigned char) what[n - 1]))
            --n;
        if (n > 1  &&  what[n - 1] == '.')
            --n;
        if (sock->type == eSOCK_Datagram) {
            u.sa = (const struct sockaddr*) ptr;
            TNCBI_IPv6Addr addr;
            unsigned short port;
            assert(u.sa  &&  (u.sa->sa_family == AF_INET ||
                              u.sa->sa_family == AF_INET6));
            switch (u.sa->sa_family) {
            case AF_INET:
                NcbiIPv4ToIPv6(&addr, u.in->sin_addr.s_addr, 0);
                port = ntohs(u.in->sin_port);
                break;
            case AF_INET6:
                memcpy(&addr, &u.in6->sin6_addr, sizeof(addr));
                port = ntohs(u.in6->sin6_port);
                break;
            default:
                memset(&addr, 0, sizeof(addr));
                port = 0;
                assert(0);
                break;
            }
            s_HostPortToString(&addr, port, head, sizeof(head));
            sprintf(tail, ", msg# %" NCBI_BIGCOUNT_FORMAT_SPEC,
                    event == eIO_Read ? sock->n_in : sock->n_out);
        } else if (!ptr  ||  !*((char*) ptr)) {
            sprintf(head, " at offset %" NCBI_BIGCOUNT_FORMAT_SPEC,
                    event == eIO_Read ? sock->n_read : sock->n_written);
            strcpy(tail, !ptr ? "" : " [OOB]");
        } else {
            strncpy0(head, (const char*) ptr, sizeof(head));
            *tail = '\0';
        }

        CORE_DATAF_X(109, level, data, size,
                     ("%s%.*s%s%s%s", s_ID(sock, _id), n, what,
                      sock->type == eSOCK_Datagram
                      ? (event == eIO_Read ? " from " : " to ")
                      : size  ||  !data ? "" : strerr
                      ? (event == eIO_Read
                         ? " while reading" : " while writing")
                      : " 0 bytes",
                      head, tail));

        UTIL_ReleaseBuffer(strerr);
        break;

    case eIO_Close:
        n = sprintf(head, "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s",
                    sock->n_read, &"s"[sock->n_read == 1]);
        if (sock->type == eSOCK_Datagram  ||
            sock->n_in != sock->n_read) {
            sprintf(head + n, "/%" NCBI_BIGCOUNT_FORMAT_SPEC " %s%s",
                    sock->n_in,
                    sock->type == eSOCK_Datagram ? "msg" : "total byte",
                    &"s"[sock->n_in == 1]);
        }
        n = sprintf(tail, "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s",
                    sock->n_written, &"s"[sock->n_written == 1]);
        if (sock->type == eSOCK_Datagram  ||
            sock->n_out != sock->n_written) {
            sprintf(tail + n, "/%" NCBI_BIGCOUNT_FORMAT_SPEC " %s%s",
                    sock->n_out,
                    sock->type == eSOCK_Datagram ? "msg" : "total byte",
                    &"s"[sock->n_out == 1]);
        }
        CORE_LOGF_X(113, level,
                    ("%s%s (in: %s, out: %s)", s_ID(sock, _id),
                     ptr ? (const char*) ptr :
                     sock->keep ? "Leaving" : "Closing",
                     head, tail));
        break;

    default:
        CORE_LOGF_X(1, eLOG_Error,
                    ("%s[SOCK::DoLog] "
                     " Invalid event #%u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
        break;
    }
}



/******************************************************************************
 *  STimeout <--> struct timeval CONVERSIONS
 */


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static STimeout*       s_tv2to(const struct timeval* tv, STimeout* to)
{
    assert(tv);

    /* NB: internally tv always kept normalized */
    to->sec  = (unsigned int) tv->tv_sec;
    to->usec = (unsigned int) tv->tv_usec;
    return to;
}

#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static struct timeval* s_to2tv(const STimeout* to,       struct timeval* tv)
{
    if (!to)
        return 0;

    tv->tv_sec  = to->usec / 1000000 + to->sec;
    tv->tv_usec = to->usec % 1000000;
    return tv;
}



/******************************************************************************
*  UTILITY
*/


extern size_t SOCK_OSHandleSize(void)
{
    return sizeof(TSOCK_Handle);
}


extern void SOCK_AllowSigPipeAPI(void)
{
#ifdef NCBI_OS_UNIX
    s_AllowSigPipe = 1/*true - API will not mask SIGPIPE out at init*/;
#endif /*NCBI_OS_UNIX*/
    return;
}


extern ESwitch SOCK_SetIPv6API(ESwitch ipv6)
{
    int version = s_IPVersion;
    switch (ipv6) {
    case eOn:
        s_IPVersion = AF_INET6;
        break;
    case eOff:
        s_IPVersion = AF_INET;
        break;
    default:
        s_IPVersion = AF_UNSPEC;
        break;
    }
    switch (version) {
    case AF_INET6:
        return eOn;
    case AF_INET:
        return eOff;
    default:
        break;
    }
    return eDefault;
}


extern void SOCK_SetApproveHookAPI(FSOCK_ApproveHook hook, void* data)
{
    CORE_LOCK_WRITE;
    s_ApproveData = hook ? data : 0;
    s_ApproveHook = hook;
    CORE_UNLOCK;
}


extern const STimeout* SOCK_SetSelectInternalRestartTimeout(const STimeout* t)
{
    static struct timeval s_New;
    static STimeout       s_Old;
    const  STimeout*      retval;
    retval          = s_SelectTimeout ? s_tv2to(s_SelectTimeout, &s_Old) : 0;
    s_SelectTimeout =                   s_to2tv(t,               &s_New);
    return retval;
}


extern ESOCK_IOWaitSysAPI SOCK_SetIOWaitSysAPI(ESOCK_IOWaitSysAPI api)
{
    ESOCK_IOWaitSysAPI retval = s_IOWaitSysAPI;
#ifndef NCBI_OS_MSWIN
#  if !defined(HAVE_POLL_H)  ||  defined(NCBI_OS_DARWIN)
    if (api == eSOCK_IOWaitSysAPIPoll) {
        CORE_LOG_X(149, eLOG_Critical, "[SOCK::SetIOWaitSysAPI] "
            " Poll API requested but not supported on this platform");
    } else
#  endif /*!HAVE_POLL_H || NCBI_OS_DARWIN*/
        s_IOWaitSysAPI = api;
#else
    (void) api;
#endif /*NCBI_OS_MSWIN*/
    return retval;
}



/******************************************************************************
*  STATIC SOCKET HELPERS
*/


static EIO_Status s_ApproveCallback(const char*    host, const TNCBI_IPv6Addr* addr,
                                    unsigned short port, ESOCK_Side            side,
                                    ESOCK_Type     type, SOCK                  sock)
{
    EIO_Status        status = eIO_Success;
    FSOCK_ApproveHook hook;
    void*             data;

    CORE_LOCK_READ;
    hook = s_ApproveHook;
    data = s_ApproveData;
    CORE_UNLOCK;
    if (hook) {
        char cp[SOCK_HOSTPORTSTRLEN];
        char _id[MAXIDLEN];
        SSOCK_ApproveInfo info;

        *cp = '\0';
        memset(&info, 0, sizeof(info));
        info.host =  host;
        info.addr = *addr;
        info.port =  port;
        info.side =  side;
        info.type =  type;
        assert(!NcbiIsEmptyIPv6(addr)  &&  port);
        assert(type == eSOCK_Socket  ||  type == eSOCK_Datagram);
#ifdef _DEBUG
        if (type == eSOCK_Datagram  ||  side == eSOCK_Server)
            s_HostPortToString(addr, port, cp, sizeof(cp));
        /* else: "cp" is shown as part of s_ID() of the socket */
#endif /*_DEBUG*/
        CORE_TRACEF(("%s[SOCK::ApproveHook] "
                     " Seeking approval for %s %s%s%s%s%s%s",
                     s_ID(sock, _id),
                     side == eSOCK_Client ? "outgoing"   : "incoming",
                     type == eSOCK_Socket ? "connection" : "message",
                     !host  &&  !*cp ? "" :
                     side == eSOCK_Client ? " to "       : " from ",
                     &"\""[!host], host ? host : "", &"\""[!host], cp));
        status = hook(&info, data);
        if (status != eIO_Success) {
            if (!*cp  &&  (type == eSOCK_Datagram  ||  side == eSOCK_Server))
                s_HostPortToString(addr, port, cp, sizeof(cp));
            CORE_LOGF_X(163, eLOG_Error,
                        ("%s[SOCK::ApproveHook] "
                         " Approval denied for %s %s%s%s%s%s%s: %s",
                         s_ID(sock, _id),
                         side == eSOCK_Client ? "outgoing"   : "incoming",
                         type == eSOCK_Socket ? "connection" : "message",
                         !host  &&  !*cp ? "" :
                         side == eSOCK_Client ? " to "       : " from ",
                         &"\""[!host], host ? host : "", &"\""[!host], cp,
                         IO_StatusStr(status)));
        } else
            CORE_TRACEF(("%s[SOCK::ApproveHook]  Allowed", s_ID(sock, _id)));
        if (status == eIO_Timeout  ||  status == eIO_Closed)
            status  = eIO_Unknown;
    }
    return status;
}


/* Switch the specified socket I/O between blocking and non-blocking mode
 */
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_SetNonblock(TSOCK_Handle sock,
                                 int/*bool*/  nonblock)
{
#if   defined(NCBI_OS_MSWIN)
    unsigned long arg = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(NCBI_OS_UNIX)
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
        return 0/*false*/;
    if (!nonblock == !(flags & O_NONBLOCK))
        return 1/*true*/;
    return fcntl(sock, F_SETFL, nonblock
                 ? flags |        O_NONBLOCK
                 : flags & (int) ~O_NONBLOCK) == 0;
#else
#  error "Unsupported platform"
#endif /*NCBI_OS*/
}


/* Set close-on-exec flag
 */
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_SetCloexec(TSOCK_Handle x_sock,
                                int/*bool*/  cloexec)
{
#if   defined(NCBI_OS_UNIX)
    int flags = fcntl(x_sock, F_GETFD, 0);
    if (flags == -1)
        return 0/*false*/;
    if (!cloexec == !(flags & FD_CLOEXEC))
        return 1/*true*/;
    return fcntl(x_sock, F_SETFD, cloexec
                 ? flags |        FD_CLOEXEC
                 : flags & (int) ~FD_CLOEXEC) == 0;
#elif defined(NCBI_OS_MSWIN)
    return SetHandleInformation((HANDLE)x_sock, HANDLE_FLAG_INHERIT, !cloexec);
#else
#  error "Unsupported platform"
#endif /*NCBI_OS*/
}


/*ARGSUSED*/
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_SetReuseAddress(TSOCK_Handle x_sock,
                                     int/*bool*/  on_off)
{
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
#  ifdef NCBI_OS_MSWIN
    BOOL reuse_addr = on_off ? TRUE : FALSE;
#  else
    int  reuse_addr = on_off ? 1    : 0;
#  endif /*NCBI_OS_MSWIN*/
    return setsockopt(x_sock, SOL_SOCKET, SO_REUSEADDR, 
                      (char*) &reuse_addr, sizeof(reuse_addr)) == 0;
#else
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    (void) on_off;
    return 1;
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
}


#ifdef SO_KEEPALIVE
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_SetKeepAlive(TSOCK_Handle x_sock,
                                  int/*bool*/  on_off)
{
#  ifdef NCBI_OS_MSWIN
    BOOL keepalive = on_off ? TRUE      : FALSE;
#  else
    int  keepalive = on_off ? 1/*true*/ : 0/*false*/;
#  endif /*NCBI_OS_MSWIN*/
    return setsockopt(x_sock, SOL_SOCKET, SO_KEEPALIVE,
                      (char*) &keepalive, sizeof(keepalive)) == 0;
}
#endif /*SO_KEEPALIVE*/


#ifdef SO_OOBINLINE
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_SetOobInline(TSOCK_Handle x_sock,
                                  int/*bool*/  on_off)
{
#  ifdef NCBI_OS_MSWIN
    BOOL oobinline = on_off ? TRUE      : FALSE;
#  else
    int  oobinline = on_off ? 1/*true*/ : 0/*false*/;
#  endif /*NCBI_OS_MSWIN*/
    return setsockopt(x_sock, SOL_SOCKET, SO_OOBINLINE,
                      (char*) &oobinline, sizeof(oobinline)) == 0;
}
#endif /*SO_OOBINLINE*/


#ifdef __GNUC__
inline
#endif /*__GNUC_*/
/* compare 2 normalized timeval timeouts: "whether *t1 is less than *t2" */
static int/*bool*/ x_IsSmallerTimeout(const struct timeval* t1,
                                      const struct timeval* t2)
{
    if (!t1/*inf*/)
        return 0;
    if (!t2/*inf*/)
        return 1;
    if (t1->tv_sec > t2->tv_sec)
        return 0;
    if (t1->tv_sec < t2->tv_sec)
        return 1;
    return t1->tv_usec < t2->tv_usec;
}


#ifdef __GNUC__
inline
#endif /*__GNUC_*/
static int/*bool*/ x_IsInterruptibleSOCK(SOCK sock)
{
    assert(sock);
    if (sock->i_on_sig == eOn)
        return 1/*true*/;
    if (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)
        return 1/*true*/;
    return 0/*false*/;
}


#if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
static int/*bool*/ x_TryLowerSockFileno(SOCK sock)
{
#  ifdef STDERR_FILENO
#    define SOCK_DUPOVER  STDERR_FILENO
#  else
#    define SOCK_DUPOVER  2
#  endif /*STDERR_FILENO*/
#  ifdef F_DUPFD_CLOEXEC
    int fd = fcntl(sock->sock, F_DUPFD_CLOEXEC, SOCK_DUPOVER + 1);
#  else
    int fd = fcntl(sock->sock, F_DUPFD,         SOCK_DUPOVER + 1);
#  endif /*F_DUPFD_CLOEXEC*/
    if (fd >= 0) {
        if (fd < FD_SETSIZE) {
            char _id[MAXIDLEN];
#  ifndef F_DUPFD_CLOEXEC
            int cloexec = fcntl(sock->sock, F_GETFD, 0);
            if (cloexec == -1  ||  ((cloexec & FD_CLOEXEC)
                                    &&  fcntl(fd, F_SETFD, cloexec) != 0)) {
                close(fd);
                return 0/*failure*/;
            }
#  endif /*F_DUPFD_CLOEXEC*/
            CORE_LOGF_X(111, eLOG_Trace,
                        ("%s[SOCK::Select] "
                         " File descriptor has been lowered to %d",
                         s_ID(sock, _id), fd));
            close(sock->sock);
            sock->sock = fd;
            return 1/*success*/;
        }
        close(fd);
        errno = 0;
    }
    return 0/*failure*/;
}
#endif /*!NCBI_OS_MSWIN && FD_SETSIZE*/


#if !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_CXX_TOOLKIT)


#ifdef __GNUC__
inline
#endif /*__GNUC_*/
static int/*bool*/ x_IsInterruptiblePoll(SOCK sock0)
{
    return sock0 ? x_IsInterruptibleSOCK(sock0) : s_InterruptOnSignal == eOn ? 1/*true*/ : 0/*false*/;
}


static EIO_Status s_Select_(size_t                n,
                            SSOCK_Poll            polls[],
                            const struct timeval* tv,
                            int/*bool*/           asis)
{
    char           _id[MAXIDLEN];
    int/*bool*/    write_only;
    int/*bool*/    read_only;
    int            ready;
    fd_set         rfds;
    fd_set         wfds;
    fd_set         efds;
    int            nfds;
    struct timeval x_tv;
    size_t         i;

#  ifdef NCBI_OS_MSWIN
    if (!n) {
        DWORD ms =
            tv ? tv->tv_sec * 1000 + (tv->tv_usec + 500) / 1000 : INFINITE;
        Sleep(ms);
        return eIO_Timeout;
    }
#  endif /*NCBI_OS_MSWIN*/

    if (tv)
        x_tv = *tv;
    else /* won't be used but keeps compilers happy */
        memset(&x_tv, 0, sizeof(x_tv));

    for (;;) { /* optionally auto-resume if interrupted / sliced */
        int/*bool*/    error = 0/*false*/;
#  ifdef NCBI_OS_MSWIN
        unsigned int   count = 0;
#  endif /*NCBI_OS_MSWIN*/
        struct timeval xx_tv;

        write_only = 1/*true*/;
        read_only = 1/*true*/;
        ready = 0/*false*/;
        FD_ZERO(&efds);
        nfds = 0;

        for (i = 0;  i < n;  ++i) {
            int/*bool*/  x_asis;
            EIO_Event    event;
            SOCK         sock;
            ESOCK_Type   type;
            TSOCK_Handle fd;

            if (!(sock = polls[i].sock)  ||  !(event = polls[i].event)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            assert((event | eIO_ReadWrite) == eIO_ReadWrite);

            if ((fd = sock->sock) == SOCK_INVALID)
                polls[i].revent = eIO_Close;
            if (error)
                continue;

            if (polls[i].revent) {
                ready = 1/*true*/;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
                x_asis = 1/*true*/;
            } else
                x_asis = asis;

#  if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
            if (fd >= FD_SETSIZE) {
                if (!x_TryLowerSockFileno(sock)) {
                    /* NB: only once here, as this sets "error" to "1" */
                    CORE_LOGF_ERRNO_X(145, eLOG_Error, errno,
                                      ("%s[SOCK::Select] "
                                       " Socket file descriptor must "
                                       " not exceed %d",
                                       s_ID(sock, _id), FD_SETSIZE));
                    polls[i].revent = eIO_Close;
                    error = 1/*true*/;
                    continue;
                }
                fd = sock->sock;
                assert(fd < FD_SETSIZE);
            }
#  endif /*!NCBI_OS_MSWIN && FD_SETSIZE*/

            type = (ESOCK_Type) sock->type;
            switch (type & eSOCK_Socket ? event : event & eIO_Read) {
            case eIO_Write:
            case eIO_ReadWrite:
                assert(type & eSOCK_Socket);
                if (type == eSOCK_Datagram  ||  sock->w_status != eIO_Closed) {
                    if (read_only) {
                        FD_ZERO(&wfds);
                        read_only = 0/*false*/;
                    }
                    FD_SET(fd, &wfds);
                }
                if (event == eIO_Write
                    &&  (type == eSOCK_Datagram  ||  x_asis
                         ||  (sock->r_on_w == eOff
                              ||  (sock->r_on_w == eDefault
                                   &&  s_ReadOnWrite != eOn)))) {
                    break;
                }
                /*FALLTHRU*/

            case eIO_Read:
                if (type != eSOCK_Socket
                    ||  (sock->r_status != eIO_Closed  &&  !sock->eof)) {
                    if (write_only) {
                        FD_ZERO(&rfds);
                        write_only = 0/*false*/;
                    }
                    FD_SET(fd, &rfds);
                }
                if (type != eSOCK_Socket  ||  x_asis  ||  event != eIO_Read
                    ||  sock->w_status == eIO_Closed
                    ||  !(sock->pending | sock->w_len)) {
                    break;
                }
                if (read_only) {
                    FD_ZERO(&wfds);
                    read_only = 0/*false*/;
                }
                FD_SET(fd, &wfds);
                break;

            default:
                /*fully pre-ready*/
                break;
            }

            FD_SET(fd, &efds);
            if (nfds < (int) fd)
                nfds = (int) fd;

#  ifdef NCBI_OS_MSWIN
            /* check whether FD_SETSIZE has been exceeded */
            if (!FD_ISSET(fd, &efds)) {
                /* NB: only once here, as this sets "bad" to "1" */
                CORE_LOGF_X(145, eLOG_Error,
                            ("[SOCK::Select] "
                             " Too many sockets in select(),"
                             " must be fewer than %u", count));
                polls[i].revent = eIO_Close;
                error = 1/*true*/;
                continue;
            }
            ++count;
#  endif /*NCBI_OS_MSWIN*/
        }
        assert(i >= n);

        if (error) {
            errno = SOCK_ETOOMANY;
            return eIO_Unknown;
        }

        if (ready)
            memset(&xx_tv, 0, sizeof(xx_tv));
        else if (tv  &&  x_IsSmallerTimeout(&x_tv, s_SelectTimeout))
            xx_tv = x_tv;
        else if (s_SelectTimeout)
            xx_tv = *s_SelectTimeout;
        /* else infinite (0) timeout will be used */

        nfds = select(SOCK_NFDS((TSOCK_Handle) nfds),
                      write_only ? 0 : &rfds,
                      read_only  ? 0 : &wfds, &efds,
                      ready  ||  tv  ||  s_SelectTimeout ? &xx_tv : 0);

        if (nfds > 0)
            break;

        if (!nfds) {
            /* timeout has expired */
            if (!ready) {
                if (!tv)
                    continue;
                if (x_IsSmallerTimeout(s_SelectTimeout, &x_tv)) {
                    x_tv.tv_sec -= s_SelectTimeout->tv_sec;
                    if (x_tv.tv_usec < s_SelectTimeout->tv_usec) {
                        x_tv.tv_sec--;
                        x_tv.tv_usec += 1000000;
                    }
                    x_tv.tv_usec -= s_SelectTimeout->tv_usec;
                    continue;
                }
                return eIO_Timeout;
            }
            /* NB: ready */
        } else { /* nfds < 0 */
            int error = SOCK_ERRNO;
            if (error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(error);
                CORE_LOGF_ERRNO_EXX(5, eLOG_Warning,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::Select] "
                                     " Failed select()",
                                     n == 1 ? s_ID(polls[0].sock, _id) : ""));
                UTIL_ReleaseBuffer(strerr);
                if (!ready)
                    return eIO_Unknown;
            } else if (x_IsInterruptiblePoll(n == 1 ? polls[0].sock : 0)) {
                return eIO_Interrupt;
            } else
                continue;
            assert(error != SOCK_EINTR  &&  ready);
        }

        assert(ready);
        break;
    }

    if (nfds > 0) {
        /* NB: some fd bits could have been counted multiple times if reside
           in different fd_set's (such as ready for both R and W), recount. */
        for (ready = 0, i = 0;  i < n;  ++i) {
            EIO_Event    event;
            SOCK         sock;
            TSOCK_Handle fd;
            if (!(sock = polls[i].sock)  ||  !(event = polls[i].event)) {
                assert(polls[i].revent == eIO_Open);
                continue;
            }
            if (polls[i].revent == eIO_Close) {
                ++ready;
                continue;
            }
            if ((fd = sock->sock) == SOCK_INVALID) {
                polls[i].revent = eIO_Close;
                ++ready;
                continue;
            }
#  if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
            assert(fd < FD_SETSIZE);
#  endif /*!NCBI_OS_MSWIN && FD_SETSIZE*/
            if (FD_ISSET(fd, &efds)) {
                polls[i].revent = event;
#  ifdef NCBI_OS_MSWIN
                if (event & eIO_Read)
                    sock->readable = 1/*true*/;
                if (event & eIO_Write)
                    sock->writable = 1/*true*/;
#  endif /*NCBI_OS_MSWIN*/
            } else {
                if (!write_only  &&  FD_ISSET(fd, &rfds)) {
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Read);
#  ifdef NCBI_OS_MSWIN
                    sock->readable = 1/*true*/;
#  endif /*NCBI_OS_MSWIN*/
                }
                if (!read_only   &&  FD_ISSET(fd, &wfds)) {
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
#  ifdef NCBI_OS_MSWIN
                    sock->writable = 1/*true*/;
#  endif /*NCBI_OS_MSWIN*/
                }
                if (polls[i].revent == eIO_Open)
                    continue;
                if (sock->type == eSOCK_Trigger)
                    polls[i].revent = event;
            }
            assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
            ++ready;
        }
    }

    (void) ready;
    assert(ready);
    /* can do I/O now */
    return eIO_Success;
}


#  if defined(NCBI_OS_UNIX) && defined(HAVE_POLL_H) && !defined(NCBI_OS_DARWIN)

#    define NPOLLS  ((3 * sizeof(fd_set)) / sizeof(struct pollfd))


static size_t x_AutoCountPolls(size_t n, SSOCK_Poll polls[])
{
    int/*bool*/ bigfd = 0/*false*/;
    int/*bool*/ good  = 1/*true*/;
    size_t      count = 0;
    size_t      i;

    assert(s_IOWaitSysAPI == eSOCK_IOWaitSysAPIAuto);
    for (i = 0;  i < n;  ++i) {
        if (!polls[i].sock) {
            assert(!polls[i].revent/*eIO_Open*/);
            continue;
        }
        if ((polls[i].event | eIO_ReadWrite) != eIO_ReadWrite) {
            good = 0/*false*/;
            continue;
        }
        if (!polls[i].event) {
            assert(!polls[i].revent/*eIO_Open*/);
            continue;
        }
        if (polls[i].sock->sock == SOCK_INVALID
            ||  polls[i].revent == eIO_Close) {
            /* pre-ready */
            continue;
        }
#    ifdef FD_SETSIZE
        if (polls[i].sock->sock >= FD_SETSIZE
            &&  !x_TryLowerSockFileno(polls[i].sock)) {
            bigfd = 1/*true*/;
        }
#    endif /*FD_SETSIZE*/
        ++count;
    }
    return good  &&  (count <= NPOLLS  ||  bigfd) ? count : 0;
}


static EIO_Status s_Poll_(size_t                n,
                          SSOCK_Poll            polls[],
                          const struct timeval* tv,
                          int/*bool*/           asis)
{
    struct pollfd  xx_polls[NPOLLS];
    char           _id[MAXIDLEN];
    struct pollfd* x_polls;
    EIO_Status     status;
    nfds_t         ready;
    nfds_t         count;
    int            wait;
    size_t         m, i;

    if (s_IOWaitSysAPI != eSOCK_IOWaitSysAPIAuto)
        m = n;
    else
#    ifdef FD_SETSIZE
    if (n > FD_SETSIZE)
        m = n;
    else
#    endif /*FD_SETSIZE*/
    if (!(m = x_AutoCountPolls(n, polls)))
        return s_Select_(n, polls, tv, asis);

    if (m <= NPOLLS)
        x_polls = xx_polls;
    else if (!(x_polls = (struct pollfd*) malloc(m * sizeof(*x_polls)))) {
        CORE_LOGF_ERRNO_X(146, eLOG_Critical, errno,
                          ("%s[SOCK::Select] "
                           " Cannot allocate poll vector(%lu)",
                           n == 1 ? s_ID(polls[0].sock, _id) : "",
                           (unsigned long) m));
        return eIO_Unknown;
    }

    status = eIO_Success;
    wait = tv ? (int)(tv->tv_sec * 1000 + (tv->tv_usec + 500) / 1000) : -1;
    for (;;) { /* optionally auto-resume if interrupted / sliced */
        int x_ready;
        int slice;

        ready = count = 0;
        for (i = 0;  i < n;  ++i) {
            int/*bool*/  x_asis;
            short        bitset;
            EIO_Event    event;
            SOCK         sock;
            ESOCK_Type   type;
            TSOCK_Handle fd;

            if (!(sock = polls[i].sock)  ||  !(event = polls[i].event)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            assert((event | eIO_ReadWrite) == eIO_ReadWrite);

            if ((fd = sock->sock) == SOCK_INVALID)
                polls[i].revent = eIO_Close;

            if (polls[i].revent) {
                ++ready;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
                x_asis = 1/*true*/;
            } else
                x_asis = asis;

            bitset = 0;
            type = (ESOCK_Type) sock->type;
            switch (type & eSOCK_Socket ? event : event & eIO_Read) {
            case eIO_Write:
            case eIO_ReadWrite:
                assert(type & eSOCK_Socket);
                if (type == eSOCK_Datagram  ||  sock->w_status != eIO_Closed)
                    bitset |= POLL_WRITE;
                if (event == eIO_Write
                    &&  (type == eSOCK_Datagram  ||  x_asis
                         ||  (sock->r_on_w == eOff
                              ||  (sock->r_on_w == eDefault
                                   &&  s_ReadOnWrite != eOn)))) {
                    break;
                }
                /*FALLTHRU*/

            case eIO_Read:
                if (type != eSOCK_Socket
                    ||  (sock->r_status != eIO_Closed  &&  !sock->eof))
                    bitset |= POLL_READ;
                if (type != eSOCK_Socket  ||  x_asis  ||  event != eIO_Read
                    ||  sock->w_status == eIO_Closed
                    ||  !(sock->pending | sock->w_len)) {
                    break;
                }
                bitset |= POLL_WRITE;
                break;

            default:
                /*fully pre-ready*/
                continue;
            }

            if (!bitset)
                continue;
            assert(0 <= fd  &&  count < (nfds_t) m);
            x_polls[count].fd      = fd;
            x_polls[count].events  = bitset;
            x_polls[count].revents = 0;
            ++count;
        }
        assert(i >= n);

        if (ready)
            slice = 0;
        else if (!s_SelectTimeout)
            slice = wait;
        else {
            slice = (int)((s_SelectTimeout->tv_sec         * 1000 +
                          (s_SelectTimeout->tv_usec + 500) / 1000));
            if (wait != -1  &&  wait < slice)
                slice = wait;
        }

        if (count  ||  !ready) {
            x_ready = poll(x_polls, count, slice);

            if (x_ready > 0) {
#    ifdef NCBI_OS_DARWIN
                /* Mac OS X sometimes misreports, weird! */
                if (x_ready > (int) count)
                    x_ready = (int) count;  /* this is *NOT* a workaround!!! */
#    endif /*NCBI_OS_DARWIN*/
                assert(status == eIO_Success);
                ready = (nfds_t) x_ready;
                assert(ready <= count);
                break;
            }
        } else
            x_ready = 0;

        assert(x_ready <= 0);
        if (!x_ready) {
            /* timeout has expired */
            if (!ready) {
                if (!tv)
                    continue;
                if (wait  > slice) {
                    wait -= slice;
                    continue;
                }
                status = eIO_Timeout;
                break;
            }
            /* NB: ready */
        } else { /* x_ready < 0 */
            if ((x_ready = SOCK_ERRNO) != SOCK_EINTR) {
                const char* strerr =
                    SOCK_STRERROR(x_ready == EINVAL ? SOCK_ETOOMANY : x_ready);
                CORE_LOGF_ERRNO_EXX(147, ready ? eLOG_Warning : eLOG_Error,
                                    x_ready, strerr ? strerr : "",
                                    ("%s[SOCK::Select] "
                                     " Failed poll()",
                                     n == 1 ? s_ID(polls[0].sock, _id) : ""));
                UTIL_ReleaseBuffer(strerr);
                if (!ready) {
                    status = eIO_Unknown;
                    break;
                }
            } else if (x_IsInterruptiblePoll(n == 1 ? polls[0].sock : 0)) {
                status = eIO_Interrupt;
                break;
            } else
                continue;
            assert(x_ready != SOCK_EINTR  &&  ready);
        }

        assert(status == eIO_Success  &&  ready);
        n = 0/*no post processing*/;
        break;
    }

    assert(status != eIO_Success  ||  ready > 0);
    if (status == eIO_Success  &&  n) {
        nfds_t scanned = 0;
        nfds_t x_ready = 0;
        for (m = 0, i = 0;  i < n;  ++i) {
            TSOCK_Handle fd;
            short events, revents;
            SOCK sock = polls[i].sock;
            if (!sock  ||  !polls[i].event) {
                assert(!polls[i].revent);
                continue;
            }
            if (polls[i].revent == eIO_Close) {
                ++x_ready;
                continue;
            }
            if ((fd = sock->sock) == SOCK_INVALID) {
                polls[i].revent = eIO_Close;
                ++x_ready;
                continue;
            }
            events = revents = 0;
            if (scanned < ready) {
                nfds_t x_scanned = 0;
                nfds_t j;
                assert((nfds_t) m < count);
                for (j = (nfds_t) m;  j < count;  ++j) {
                    if (x_polls[j].revents)
                        ++x_scanned;
                    if (x_polls[j].fd == fd) {
                        events   = x_polls[j].events;
                        revents  = x_polls[j].revents;
                        scanned += x_scanned;
                        m        = (size_t) ++j;
                        break;
                    }
                }
                assert(events  ||  ((nfds_t) m < count  &&  count <= j));
            }
            assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
            if (!(revents & POLL_ERROR)) {
                if ((events & POLL_READ)   &&  (revents & POLL_READ_READY))
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Read);
                if ((events & POLL_WRITE)  &&  (revents & POLL_WRITE_READY))
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
                if (polls[i].revent == eIO_Open)
                    continue;
                if (sock->type == eSOCK_Trigger)
                    polls[i].revent = polls[i].event;
            } else
                polls[i].revent = eIO_Close;
            assert(polls[i].revent != eIO_Open);
            ++x_ready;
        }
        assert(scanned <= ready);
        assert(x_ready >= ready);
        (void) x_ready;
    }

    if (x_polls != xx_polls)
        free(x_polls);
    return status;
}


#  endif /*NCBI_OS_UNIX && HAVE_POLL_H && !NCBI_OS_DARWIN*/


#endif /*!NCBI_OS_MSWIN || !NCBI_CXX_TOOLKIT*/


/* Select on the socket I/O (multiple sockets).
 *
 * "Event" field is not considered for entries, whose "sock" field is 0,
 * "revent" for those entries is always set "eIO_Open".  For all other entries
 * "revent" will be checked, and if set, it will be "subtracted" from the
 * requested "event" (or the entry won't be considered at all if "revent" is
 * already set to "eIO_Close").  If at least one non-"eIO_Open" status found
 * in "revent", the call terminates with "eIO_Success" (after, however, having
 * checked all other entries for validity, and) after having polled
 * (with timeout 0) on all remaining entries and events.
 *
 * This function always checks datagram and listening sockets, and triggers
 * exactly as they are requested (according to "event").  For stream sockets,
 * the call behaves differently only if the last parameter is passed as zero:
 *
 * If "eIO_Write" event is inquired on a stream socket, and the socket is
 * marked for upread, then returned "revent" may also include "eIO_Read" to
 * indicate that some input is available on that socket.
 *
 * If "eIO_Read" event is inquired on a stream socket, and the socket still
 * has its connection/data pending, the "revent" field may then include
 * "eIO_Write" to indicate that connection can be completed/data sent.
 *
 * Return "eIO_Success" when at least one socket is found either ready 
 * (including "eIO_Read" event on "eIO_Write" for upreadable sockets, and/or
 * "eIO_Write" on "eIO_Read" for sockets in pending state when "asis!=0")
 * or failing ("revent" contains "eIO_Close").
 *
 * NB:  Make sure to consider sock's file descriptor going invalid while the
 * call was still waiting (the usage may involve SOCK_Abort() from elsewhere).
 *
 * Return "eIO_Timeout", if timeout expired before any socket was capable
 * of doing any IO.  Any other return code indicates some usage failure.
 */
static EIO_Status s_Select(size_t                n,
                           SSOCK_Poll            polls[],
                           const struct timeval* tv,
                           int/*bool*/           asis)
{ 
#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_CXX_TOOLKIT)
    DWORD  wait = tv ? tv->tv_sec * 1000 + (tv->tv_usec + 500)/1000 : INFINITE;
    HANDLE what[MAXIMUM_WAIT_OBJECTS];
    long   want[MAXIMUM_WAIT_OBJECTS];
    char  _id[MAXIDLEN];

    for (;;) { /* timeslice loop */
        int/*bool*/ done  = 0/*false*/;
        int/*bool*/ ready = 0/*false*/;
        DWORD       count = 0;
        DWORD       slice;
        size_t      i;

        for (i = 0;  i < n;  ++i) {
            int/*bool*/ x_asis;
            long        bitset;
            EIO_Event   event;
            SOCK        sock;
            HANDLE      ev;

            if (!(sock = polls[i].sock)  ||  !(event = polls[i].event)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            assert((event | eIO_ReadWrite) == eIO_ReadWrite);

            if (sock->sock == SOCK_INVALID)
                polls[i].revent = eIO_Close;
            if (done)
                continue;

            if (polls[i].revent) {
                ready = 1/*true*/;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
                x_asis = 1/*true*/;
            } else
                x_asis = asis;

            bitset = 0;
            if (sock->type != eSOCK_Trigger) {
                ESOCK_Type type = (ESOCK_Type) sock->type;
                EIO_Event  readable = sock->readable ? eIO_Read  : eIO_Open;
                EIO_Event  writable = sock->writable ? eIO_Write : eIO_Open;
                switch (type & eSOCK_Socket ? event : event & eIO_Read) {
                case eIO_Write:
                case eIO_ReadWrite:
                    if (type == eSOCK_Datagram
                        ||  sock->w_status != eIO_Closed) {
                        if (writable) {
                            polls[i].revent |= eIO_Write;
                            ready = 1/*true*/;
                        }
                        if (!sock->connected)
                            bitset |= FD_CONNECT/*C*/;
                        bitset     |= FD_WRITE/*W*/;
                    }
                    if (event == eIO_Write
                        &&  (type == eSOCK_Datagram  ||  x_asis
                             ||  (sock->r_on_w == eOff
                                  ||  (sock->r_on_w == eDefault
                                       &&  s_ReadOnWrite != eOn)))) {
                        break;
                    }
                    /*FALLTHRU*/

                case eIO_Read:
                    if (type != eSOCK_Socket
                        ||  (sock->r_status != eIO_Closed  &&  !sock->eof)) {
                        if (readable) {
                            polls[i].revent |= eIO_Read;
                            ready = 1/*true*/;
                        }
                        if (type & eSOCK_Socket) {
                            if (type == eSOCK_Socket)
                                bitset |= FD_OOB/*O*/;
                            bitset     |= FD_READ/*R*/;
                        } else
                            bitset     |= FD_ACCEPT/*A*/;
                    }
                    if (type != eSOCK_Socket  ||  x_asis  ||  event != eIO_Read
                        ||  sock->w_status == eIO_Closed
                        ||  !(sock->pending | sock->w_len)) {
                        break;
                    }
                    if (writable) {
                        polls[i].revent |= eIO_Write;
                        ready = 1/*true*/;
                    }
                    bitset |= FD_WRITE/*W*/;
                    break;

                default:
                    /*fully pre-ready*/
                    continue;
                }

                if (!bitset)
                    continue;
                ev = sock->event;
            } else
                ev = ((TRIGGER) sock)->fd;

            if (count >= sizeof(what) / sizeof(what[0])) {
                /* NB: only once here, as this sets "done" to "1" */
                CORE_LOGF_X(145, eLOG_Error,
                            ("[SOCK::Select] "
                             " Too many objects exceeding %u",
                             (unsigned int) count));
                polls[i].revent = eIO_Close;
                done = 1/*true*/;
                continue;
            }
            want[count] = bitset;
            what[count] = ev;
            ++count;
        }
        assert(i >= n);

        if (done) {
            errno = SOCK_ETOOMANY;
            return eIO_Unknown;
        }

        if (s_SelectTimeout) {
            slice = (s_SelectTimeout->tv_sec         * 1000 +
                    (s_SelectTimeout->tv_usec + 500) / 1000);
            if (wait != INFINITE  &&  wait < slice)
                slice = wait;
        } else
            slice = wait;

        if (count) {
            DWORD m = 0, r;
            i = 0;
            do {
                size_t j;
                DWORD  c = count - m;
                r = WaitForMultipleObjects(c,
                                           what + m,
                                           FALSE/*any*/,
                                           ready ? 0 : slice);
                if (r == WAIT_FAILED) {
                    DWORD err = GetLastError();
                    const char* strerr = s_WinStrerror(err);
                    CORE_LOGF_ERRNO_EXX(133, eLOG_Error,
                                        err, strerr ? strerr : "",
                                        ("[SOCK::Select] "
                                         " Failed WaitForMultipleObjects(%u)",
                                         (unsigned int) c));
                    UTIL_ReleaseBufferOnHeap(strerr);
                    break;
                }
                if (r == WAIT_TIMEOUT)
                    break;
                if (r < WAIT_OBJECT_0  ||  WAIT_OBJECT_0 + c <= r) {
                    CORE_LOGF_X(134, !ready ? eLOG_Error : eLOG_Warning,
                                ("[SOCK::Select] "
                                 " WaitForMultipleObjects(%u) returned %d",
                                 (unsigned int) c, (int)(r - WAIT_OBJECT_0)));
                    r = WAIT_FAILED;
                    break;
                }
                m += r - WAIT_OBJECT_0;
                assert(!done);

                /* something must be ready */
                for (j = i;  j < n;  ++j) {
                    SOCK sock = polls[j].sock;
                    WSANETWORKEVENTS e;
                    long bitset;
                    if (!sock  ||  !polls[j].event)
                        continue;
                    if (polls[j].revent == eIO_Close) {
                        ready = 1/*true*/;
                        continue;
                    }
                    if (sock->sock == SOCK_INVALID) {
                        polls[j].revent = eIO_Close;
                        ready = 1/*true*/;
                        continue;
                    }
                    if (sock->type == eSOCK_Trigger) {
                        if (what[m] != ((TRIGGER) sock)->fd)
                            continue;
                        polls[j].revent = polls[j].event;
                        assert(polls[j].revent != eIO_Open);
                        done = 1/*true*/;
                        break;
                    }
                    if (what[m] != sock->event)
                        continue;
                    /* reset well before a re-enabling WSA API call occurs */
                    if (!WSAResetEvent(what[m])) {
                        sock->r_status = sock->w_status = eIO_Closed;
                        polls[j].revent = eIO_Close;
                        done = 1/*true*/;
                        break;
                    }
                    if (WSAEnumNetworkEvents(sock->sock, what[m], &e) != 0) {
                        int error = SOCK_ERRNO;
                        const char* strerr = SOCK_STRERROR(error);
                        CORE_LOGF_ERRNO_EXX(136, eLOG_Error,
                                            error, strerr ? strerr : "",
                                            ("%s[SOCK::Select] "
                                             " Failed WSAEnumNetworkEvents",
                                             s_ID(sock, _id)));
                        UTIL_ReleaseBuffer(strerr);
                        polls[j].revent = eIO_Close;
                        done = 1/*true*/;
                        break;
                    }
                    /* NB: the bits are XCAOWR */
                    if (!(bitset = e.lNetworkEvents)) {
                        if (ready  ||  !slice) {
                            m = count - 1;
                            assert(!done);
                            break;
                        }
                        if (sock->type == eSOCK_Listening
                            &&  (sock->log == eOn  ||
                                 (sock->log == eDefault  &&  s_Log == eOn))) {
                            LSOCK lsock = (LSOCK) sock;
                            ELOG_Level level;
                            if (lsock->away < 10) {
                                lsock->away++;
                                level = eLOG_Warning;
                            } else
                                level = eLOG_Trace;
                            CORE_LOGF_X(141, level,
                                        ("%s[SOCK::Select] "
                                         " Run-away connection detected",
                                         s_ID(sock, _id)));
                        }
                        break;
                    }
                    if (bitset & FD_CLOSE/*X*/) {
                        if (sock->type != eSOCK_Socket) {
                            polls[j].revent = eIO_Close;
                            done = 1/*true*/;
                            break;
                        }
                        bitset |= FD_READ/*at least SHUT_WR @ remote end*/;
                        sock->readable = sock->closing = 1/*true*/;
                    } else {
                        if (bitset & (FD_CONNECT | FD_WRITE)) {
                            assert(sock->type & eSOCK_Socket);
                            sock->writable = 1/*true*/;
                        }
                        if (bitset & (FD_ACCEPT | FD_OOB | FD_READ))
                            sock->readable = 1/*true*/;
                    }
                    bitset &= want[m];
                    if ((bitset & (FD_CONNECT | FD_WRITE))
                        &&  sock->writable) {
                        assert(sock->type & eSOCK_Socket);
                        polls[j].revent=(EIO_Event)(polls[j].revent|eIO_Write);
                        done = 1/*true*/;
                    }
                    if ((bitset & (FD_ACCEPT | FD_OOB | FD_READ))
                        &&  sock->readable) {
                        polls[j].revent=(EIO_Event)(polls[j].revent|eIO_Read);
                        done = 1/*true*/;
                    }
                    assert((polls[j].revent | eIO_ReadWrite) == eIO_ReadWrite);
                    if (!polls[j].revent) {
                        int k;
                        if ((e.lNetworkEvents & FD_CLOSE)
                            &&  !e.iErrorCode[FD_CLOSE_BIT]) {
                            polls[j].revent = polls[j].event;
                            done = 1/*true*/;
                        } else for (k = 0;  k < FD_MAX_EVENTS;  ++k) {
                            if (!(e.lNetworkEvents & (1 << k)))
                                continue;
                            if (e.iErrorCode[k]) {
                                polls[j].revent = eIO_Close;
                                errno = e.iErrorCode[k];
                                done = 1/*true*/;
                                break;
                            }
                        }
                    } else
                        done = 1/*true*/;
                    break;
                }
                if (done) {
                    ready = 1/*true*/;
                    done = 0/*false*/;
                    i = ++j;
                }
                if (ready  ||  !slice)
                    ++m;
            } while (m < count);

            if (ready)
                break;

            if (r == WAIT_FAILED)
                return eIO_Unknown;
            /* treat this as a timed out slice */
        } else if (ready) {
            break;
        } else
            Sleep(slice);

        if (wait != INFINITE) {
            if (wait  > slice) {
                wait -= slice;
                continue;
            }
            return eIO_Timeout;
        }
    }

    /* can do I/O now */
    return eIO_Success;

#else /*!NCBI_OS_MSWIN || !NCBI_CXX_TOOLKIT*/

#  if defined(NCBI_OS_UNIX) && defined(HAVE_POLL_H) && !defined(NCBI_OS_DARWIN)
    if (s_IOWaitSysAPI != eSOCK_IOWaitSysAPISelect)
        return s_Poll_(n, polls, tv, asis);
#  endif /*NCBI_OS_UNIX && HAVE_POLL_H && !NCBI_OS_DARWIN*/

    return s_Select_(n, polls, tv, asis);

#endif /*NCBI_OS_MSWIN && NCBI_CXX_TOOLKIT*/
}


#if defined(NCBI_COMPILER_GCC)  ||  defined(NCBI_COMPILER_ANY_CLANG)
#  pragma GCC diagnostic push                           /* NCBI_FAKE_WARNING */
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"/* NCBI_FAKE_WARNING */
static inline void x_tvcpy(struct timeval* dst, struct timeval* src)
{
    memcpy(dst, src, sizeof(*dst));
}
#  pragma GCC diagnostic warning "-Wmaybe-uninitialized"/* NCBI_FAKE_WARNING */
#  pragma GCC diagnostic pop                            /* NCBI_FAKE_WARNING */
#else
#  define x_tvcpy(d, s)  (void) memcpy((d), (s), sizeof(*(d)))
#endif /*NCBI_COMPILER_GCC*/


/* connect() could be async/interrupted by a signal or just cannot establish
 * the connection immediately;  yet, it must have been in progress
 * (asynchronously), so wait here for it to succeed (become writeable).
 */
static EIO_Status s_IsConnected_(SOCK                  sock,
                                 const struct timeval* tv,
                                 const char**          what,
                                 int*                  error,
                                 int/*bool*/           writeable)
{
    char _id[MAXIDLEN];
    EIO_Status status;
    SSOCK_Poll poll;

    *what = 0;
    *error = 0;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;

    errno = 0;
    if (!writeable) {
        poll.sock   = sock;
        poll.event  = eIO_Write;
        poll.revent = eIO_Open;
        status = s_Select(1, &poll, tv, 1/*asis*/);
        assert(poll.event == eIO_Write);
        if (status == eIO_Timeout)
            return status;
    } else {
        status      = eIO_Success;
        poll.revent = eIO_Write;
    }

#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    if (!sock->connected  &&  status == eIO_Success) {
        TSOCK_socklen_t len = sizeof(*error);
        /* Note WSA resets SOCK_ERROR to 0 after this call, if successful */
        if (getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, (void*) error, &len)
            != 0  ||  *error != 0) {
            status = eIO_Unknown;
            /* if left zero, *error will be assigned errno just a bit later */
        }
    }
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/

    if (status != eIO_Success  ||  poll.revent != eIO_Write) {
        if (!*error) {
            *error = SOCK_ERRNO;
#  ifdef NCBI_OS_MSWIN
            if (!*error)
                *error = errno;
#  endif /*NCBI_OS_MSWIN*/
        }
        if (*error == SOCK_ECONNREFUSED  ||  *error == SOCK_ETIMEDOUT)
            sock->r_status = sock->w_status = status = eIO_Closed;
        else if (status == eIO_Success)
            status = eIO_Unknown;
        return status;
    }

    if (!sock->connected) {
        if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn)) {
#if defined(_DEBUG)  &&  !defined(NDEBUG)
            char  mtu[128];
            int   lev, opt;
            int   m    = 0;
            char* x_m  = (char*) &m;
            if (NcbiIsIPv4(&sock->addr)) {
#  if defined(IPPROTO_IP)  &&  defined(IP_MTU)
                lev = IPPROTO_IP;
                opt = IP_MTU;
#  else
                lev = opt = 0;
                x_m = 0;
#  endif /*IPPROTO_IP && IP_MTU*/
            } else {
#  if (defined(NCBI_OS_MSWIN)  ||  defined(IPPROTO_IPV6))  &&  defined(IPV6_MTU)
                lev = IPPROTO_IPV6; /* Argh! enum on Win */
                opt = IPV6_MTU;
#  else
                lev = opt = 0;
                x_m = 0;
#  endif /*(NCBI_OS_MSWIN || IPPROTO_IPV6) && IPV6_MTU*/
            }
            if (x_m  &&  sock->port) {
                TSOCK_socklen_t mlen = sizeof(m);
                if (getsockopt(sock->sock, lev, opt, x_m, &mlen) != 0) {
                    const char* strerr = SOCK_STRERROR(SOCK_ERRNO);
                    sprintf(mtu, ", MTU ?? (%.80s)", strerr ? strerr : "??");
                    UTIL_ReleaseBuffer(strerr);
                } else
                    sprintf(mtu, ", MTU = %d", m);
            } else
                *mtu = '\0';
#else
            static const char* mtu = "";
#endif /*_DEBUG && !NDEBUG*/
            CORE_LOGF(eLOG_Trace,
                      ("%sConnection established%s", s_ID(sock, _id), mtu));
        }
        if (s_ReuseAddress == eOn
#ifdef NCBI_OS_UNIX
            &&  !sock->path[0]
#endif /*NCBI_OS_UNIX*/
            &&  !s_SetReuseAddress(sock->sock, 1/*true*/)) {
            int x_error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(6, eLOG_Trace,
                                x_error, strerr ? strerr : "",
                                ("%s[SOCK::IsConnected] "
                                 " Failed setsockopt(REUSEADDR)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
        sock->connected = 1/*true*/;
    }

    if (sock->pending) {
        if (sock->sslctx) {
            FSSLOpen sslopen = s_SSL ? s_SSL->Open : 0;
            if (sslopen) {
                int/*bool*/ want_desc
                    = (sock->log == eOn
                       ||  (sock->log == eDefault  &&  s_Log == eOn)
                       ? 1/*true*/ : 0/*false*/);
                const unsigned int rtv_set = sock->r_tv_set;
                const unsigned int wtv_set = sock->w_tv_set;
                struct timeval rtv;
                struct timeval wtv;
                char* desc;
                if (rtv_set)
                    rtv = sock->r_tv;
                if (wtv_set)
                    wtv = sock->w_tv;
                SOCK_SET_TIMEOUT(sock, r, tv);
                SOCK_SET_TIMEOUT(sock, w, tv);
                status = sslopen(sock->sslctx->sess, error,
                                 want_desc ? &desc : 0);
                if ((sock->w_tv_set = wtv_set & 1) != 0)
                    x_tvcpy(&sock->w_tv, &wtv);
                if ((sock->r_tv_set = rtv_set & 1) != 0)
                    x_tvcpy(&sock->r_tv, &rtv);
                if (status == eIO_Success) {
                    sock->pending = 0/*false*/;
                    if (want_desc) {
                        CORE_LOGF(eLOG_Trace,
                                  ("%sSSL session created%s%s%s%s%s",
                                   s_ID(sock, _id),
                                   sock->sslctx->host? " \""              : "",
                                   sock->sslctx->host? sock->sslctx->host : "",
                                   &"\""[!sock->sslctx->host],
                                   &" "[!desc], desc ? desc : ""));
                        if (desc)
                            free(desc);
                    }
                } else
                    *what = "SSL handshake";
            } else
                status = eIO_NotSupported;
        } else
            sock->pending = 0/*false*/;
    }

    return status;
}


static EIO_Status s_WaitConnected(SOCK                  sock,
                                  const struct timeval* tv)
{
    const char* what;
    int         unused;
    EIO_Status  status = s_IsConnected_(sock, tv, &what, &unused, 0);
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Timeout) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = eIO_Open;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


/* Read as many as "size" bytes of data from the socket.  Return eIO_Success
 * if at least one byte has been read or EOF has been reached (0 bytes read).
 * Otherwise (nothing read), return an error code to indicate the problem.
 * NOTE:  This call is for stream sockets only.  Also, it can return the
 * above mentioned EOF indicator only once, with all successive calls to
 * return an error (usually, eIO_Closed).
 */
static EIO_Status s_Recv(SOCK    sock,
                         void*   buf,
                         size_t  size,
                         size_t* n_read,
                         int     flag/*log(>0)*/)
{
    int/*bool*/ readable;
    char _id[MAXIDLEN];

    assert(sock->type == eSOCK_Socket  &&  buf  &&  size > 0  &&  !*n_read);

    if (sock->eof)
        return sock->r_status == eIO_Closed ? eIO_Unknown : eIO_Closed;
    if (sock->r_status == eIO_Closed)
        return eIO_Closed;

    /* read from the socket */
    readable = 0/*false*/;
    for (;;) { /* optionally auto-resume if interrupted */
        int error;
        ssize_t x_read = recv(sock->sock, buf, WIN_INT_CAST size, 0/*flags*/);
#ifdef NCBI_OS_MSWIN
        /* recv() resets IO event recording */
        sock->readable = sock->closing;
#endif /*NCBI_OS_MSWIN*/

        /* success/EOF? */
        if (x_read >= 0  ||
            (x_read < 0  &&  ((error = SOCK_ERRNO) == SOCK_ENOTCONN    ||  /*NCBI_FAKE_WARNING*/
                              error                == SOCK_ETIMEDOUT   ||
                              error                == SOCK_ENETRESET   ||
                              error                == SOCK_ECONNRESET  ||
                              error                == SOCK_ECONNABORTED))) {
            /* statistics & logging */
            if ((x_read < 0  &&  sock->log != eOff)  ||
                ((sock->log == eOn || (sock->log == eDefault && s_Log == eOn))
                 &&  (!sock->sslctx  ||  flag > 0))) {
                s_DoLog(x_read < 0
                        ? (sock->n_read  &&  sock->n_written
                           ? eLOG_Error : eLOG_Trace)
                        : eLOG_Note, sock, eIO_Read,
                        x_read < 0 ? (void*) &error :
                        x_read > 0 ? buf            : 0,
                        (size_t)(x_read < 0 ? 0 : x_read), 0);
            }

            if (x_read > 0) {
                assert((size_t) x_read <= size);
                sock->n_read += (TNCBI_BigCount) x_read;
                *n_read       = (size_t)         x_read;
            } else {
                /* catch EOF/failure */
                sock->eof = 1/*true*/;
                if (x_read/*<0*/) {
                    if (error != SOCK_ENOTCONN)
                        sock->w_status = eIO_Closed;
                    sock->r_status = eIO_Closed;
                    return eIO_Unknown/*error*/;
                }
#ifdef NCBI_OS_MSWIN
                sock->closing = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
            }
            sock->r_status = eIO_Success;
            break/*success*/;
        }

        if (error == SOCK_EWOULDBLOCK  ||  error == SOCK_EAGAIN) {
            /* blocked -- wait for data to come;  return if timeout/error */
            EIO_Status status;
            SSOCK_Poll poll;

            if (sock->r_tv_set  &&  !(sock->r_tv.tv_sec | sock->r_tv.tv_usec)){
                sock->r_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (readable) {
                CORE_TRACEF(("%s[SOCK::Recv] "
                             " Spurious false indication of read-ready",
                             s_ID(sock, _id)));
            }
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, SOCK_GET_TIMEOUT(sock, r), 1/*asis*/);
            assert(poll.event == eIO_Read);
            if (status == eIO_Timeout) {
                sock->r_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (status != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                return eIO_Unknown;
            assert(poll.revent == eIO_Read);
            readable = 1/*true*/;
            continue/*read again*/;
        }

        if (error != SOCK_EINTR) {
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOGF_ERRNO_EXX(7, eLOG_Trace,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::Recv] "
                                 " Failed recv()",
                                 s_ID(sock, _id)));            
            UTIL_ReleaseBuffer(strerr);
            /* don't want to handle all possible errors...
               let them be "unknown" */
            sock->r_status = eIO_Unknown;
            break/*unknown*/;
        }

        if (x_IsInterruptibleSOCK(sock)) {
            sock->r_status = eIO_Interrupt;
            break/*interrupt*/;
        }
    }

    return (EIO_Status) sock->r_status;
}


/*fwdecl*/
static EIO_Status s_WritePending(SOCK, const struct timeval*, int, int);


/* Read/Peek data from the socket.  Return eIO_Success iff some data have been
 * read.  Return other (error) code if an error/EOF occurred (zero bytes read).
 * MSG_PEEK is not implemented on Mac, and it is poorly implemented on Win32,
 * so we had to implement this feature by ourselves.
 * NB:  peek = {-1=upread(!buf && !size), 0=read, 1=peek}
 */
static EIO_Status s_Read_(SOCK    sock,
                          void*   buf,
                          size_t  size,
                          size_t* n_read,
                          int     peek)
{
    unsigned int rtv_set;
    struct timeval rtv;
    char _id[MAXIDLEN];
    EIO_Status status;
    int/*bool*/ done;

    assert(sock->type & eSOCK_Socket);
    assert(peek >= 0  ||  (!buf  &&  !size));

    if (sock->type != eSOCK_Datagram  &&  peek >= 0) {
        *n_read = 0;
        status = s_WritePending(sock, SOCK_GET_TIMEOUT(sock, r), 0, 0);
        if (sock->pending) {
            assert(status != eIO_Success);
            return status == eIO_Closed ? eIO_Unknown : status;
        }
        if (!size  &&  peek >= 0) {
            status = (EIO_Status) sock->r_status;
            if (status == eIO_Closed)
                status  = eIO_Unknown;
            else if (sock->eof)
                status  = eIO_Closed;
            return status;
        }
    }

    if (sock->type == eSOCK_Datagram  ||  peek >= 0) {
        *n_read = (peek
                   ? BUF_Peek(sock->r_buf, buf, size)
                   : BUF_Read(sock->r_buf, buf, size));
        if (sock->type == eSOCK_Datagram) {
            if (size  &&  !*n_read) {
                sock->r_status = eIO_Closed;
                return eIO_Closed;
            }
            return !size ? (EIO_Status) sock->r_status : eIO_Success;
        }
        if (*n_read  &&  (*n_read == size  ||  !peek))
            return eIO_Success;
    } else
        *n_read = 0;

    if ((status = (EIO_Status) sock->r_status) == eIO_Closed  ||  sock->eof) {
        if (*n_read)
            return eIO_Success;
        if (status == eIO_Closed) {
            CORE_TRACEF(("%s[SOCK::Read] "
                         " Socket already shut down for reading",
                         s_ID(sock, _id)));
            return eIO_Unknown;
        }
        return eIO_Closed/*EOF*/;
    }

    if (peek < 0) {
        /* upread */
        if ((rtv_set = sock->r_tv_set) != 0)
            rtv = sock->r_tv;
        /*zero timeout*/
        sock->r_tv_set = 1;
        memset(&sock->r_tv, 0, sizeof(sock->r_tv));
    } else
        rtv_set = 2;
    done = 0/*false*/;
    assert(!*n_read  ||  peek > 0);
    assert((peek >= 0  &&  size > 0)  ||  (peek < 0  &&  !buf  &&  !size));
    do {
        char   xx_buf[SOCK_BUF_CHUNK_SIZE / 4], *x_buf, *p_buf;
        size_t x_todo, x_read, x_save;

        if (buf  &&  (x_todo = size - *n_read) >= SOCK_BUF_CHUNK_SIZE) {
            x_buf  = (char*) buf + *n_read;
            p_buf  = 0;
        } else if (!(p_buf = (char*) malloc(SOCK_BUF_CHUNK_SIZE))) {
            x_todo = sizeof(xx_buf);
            x_buf  = xx_buf;
        } else {
            x_todo = SOCK_BUF_CHUNK_SIZE;
            x_buf  = p_buf;
        }
        assert(x_buf  &&  x_todo);
        if (sock->sslctx) {
            int error = 0;
            FSSLRead sslread = s_SSL ? s_SSL->Read : 0;
            if (!sslread) {
                if (p_buf)
                    free(p_buf);
                status = eIO_NotSupported;
                break/*error*/;
            }
            status = sslread(sock->sslctx->sess,
                             x_buf, x_todo, &x_read, &error);
            assert(status == eIO_Success  ||  !x_read);
            assert(status == eIO_Success  ||  error);
            assert(x_read <= x_todo);

            /* statistics & logging */
            if ((status != eIO_Success  &&  sock->log != eOff)  ||
                sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(x_read ? eLOG_Note : eLOG_Trace, sock, eIO_Read,
                        x_read ? x_buf :
                        status != eIO_Closed  ||  !sock->eof ?
                        (void*) &error : 0,
                        status != eIO_Success ? 0 : x_read,
                        x_read ? " [decrypt]" : 0);
            }

            if (status == eIO_Closed  &&  !sock->eof)
                sock->r_status = eIO_Closed;
        } else {
            x_read = 0;
            status = s_Recv(sock, x_buf, x_todo, &x_read, 0);
            assert(status == eIO_Success  ||  !x_read);
            assert(x_read <= x_todo);
        }
        if (status != eIO_Success  ||  !x_read) {
            if (p_buf)
                free(p_buf);
            if (status == eIO_Success)
                status  = eIO_Closed/*EOF*/;
            break;
        }
        assert(status == eIO_Success  &&  0 < x_read  &&  x_read <= x_todo);

        if (x_read < x_todo)
            done = 1/*true*/;

        if (peek >= 0) {
            assert(size > *n_read);  /* NB: subsumes size > 0 */
            x_todo = size - *n_read;
            if (x_todo > x_read)
                x_todo = x_read;
            if (buf  &&  (p_buf  ||  x_buf == xx_buf))
                memcpy((char*) buf + *n_read, x_buf, x_todo);
            x_save = peek ? x_read : x_read - x_todo;
        } else
            x_save = x_read;
        if (x_save) {
            /* store the newly read/excess data in the internal input buffer */
            if (!p_buf  ||  x_save < SOCK_BUF_CHUNK_SIZE / 2) {
                sock->eof = !BUF_Write(&sock->r_buf,
                                       peek ? x_buf : x_buf + x_todo,
                                       x_save);
                if (p_buf)
                    free(p_buf);
            } else {
                sock->eof = !BUF_AppendEx(&sock->r_buf, p_buf, peek
                                          ? SOCK_BUF_CHUNK_SIZE
                                          : SOCK_BUF_CHUNK_SIZE - x_todo,
                                          peek ? p_buf : p_buf + x_todo,
                                          x_save);
                if (sock->eof)
                    free(p_buf);
            }
            if (sock->eof) {
                CORE_LOGF_ERRNO_X(8, eLOG_Critical, errno,
                                  ("%s[SOCK::Read] "
                                   " Cannot save %lu byte%s of unread data",
                                   s_ID(sock, _id), (unsigned long) x_save,
                                   &"s"[x_save == 1]));
                sock->r_status = eIO_Closed/*failure*/;
                x_read = peek >= 0 ? x_todo : 0;
                status = eIO_Unknown;
            } else if (peek >= 0)
                x_read = x_todo;
        } else if (p_buf)
            free(p_buf);
        *n_read += x_read;

        if (status != eIO_Success  ||  done)
            break;
        if (rtv_set & 2) {
            if ((rtv_set = sock->r_tv_set) != 0)
                rtv = sock->r_tv;
            /*zero timeout*/
            sock->r_tv_set = 1;
            memset(&sock->r_tv, 0, sizeof(sock->r_tv));
        }
    } while (peek < 0/*upread*/  ||  (!buf  &&  *n_read < size));
    if (!(rtv_set & 2)  &&  (sock->r_tv_set = rtv_set & 1) != 0)
        x_tvcpy(&sock->r_tv, &rtv);

    assert(*n_read  ||  status != eIO_Success);
    return *n_read ? eIO_Success : status/*non-eIO_Success*/;
}


static EIO_Status s_Read(SOCK    sock,
                         void*   buf,
                         size_t  size,
                         size_t* n_read,
                         int     peek)
{
    EIO_Status status = s_Read_(sock, buf, size, n_read, peek);
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Closed) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = eIO_Read;
        info.status = status;
        s_ErrorCallback(&info);
    }
    assert(*n_read <= size);
    return status;
}


/* s_Select() with stall protection:  try pull incoming data from sockets.
 * This method returns array of polls, "revent"s of which are always
 * compatible with requested "event"s.  That is, it always strips additional
 * events that s_Select() may have set to indicate additional I/O events
 * some sockets are ready for.  Return eIO_Timeout if no compatible events
 * were found (all sockets are not ready for inquired respective I/O) within
 * the specified timeout (and no other socket error was flagged).
 * Return eIO_Success if at least one socket is ready.  Return the number
 * of sockets that are ready via pointer argument "n_ready" (may be NULL).
 * Return other error code to indicate an error condition.
 */
static EIO_Status s_SelectStallsafe(size_t                n,
                                    SSOCK_Poll            polls[],
                                    const struct timeval* tv,
                                    size_t*               n_ready)
{
    size_t i, k, m;

    assert(!n  ||  polls);

    for (;;) { /* until ready, or one full "tv" term expires or error occurs */
        int/*bool*/ pending;
        EIO_Status  status;

        status = s_Select(n, polls, tv, 0);
        if (status != eIO_Success) {
            if ( n_ready )
                *n_ready = 0;
            return status;
        }

        m = k = 0;
        pending = 0/*false*/;
        for (i = 0;  i < n;  ++i) {
            if (polls[i].revent == eIO_Close) {
                break/*ready*/;
            }
            assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
            if (polls[i].revent & polls[i].event) {
                polls[i].revent
                    = (EIO_Event)(polls[i].revent & polls[i].event);
                break/*ready*/;
            }
            if (polls[i].revent  &&  !pending) {
                assert(polls[i].sock);
                pending = 1/*true*/;
                k = i;
            }
        }
        if (i < n/*ready*/) {
            m = 1/*ready*/;
            break;
        }

        assert(pending  &&  !m);
        /* all sockets are not ready for the requested events */
        for (i = k;  i < n;  ++i) {
            SOCK sock;
            /* try to push pending writes */
            if (polls[i].event == eIO_Read  &&  polls[i].revent == eIO_Write) {
                static const struct timeval zero = { 0 };
                sock = polls[i].sock;
                assert(sock                          &&
                       sock->sock != SOCK_INVALID    &&
                       sock->type == eSOCK_Socket    &&
                       sock->w_status != eIO_Closed  &&
                       (sock->pending | sock->w_len));
                (void) s_WritePending(sock, &zero, 1/*writeable*/, 0);
                if (sock->r_status == eIO_Closed  ||  sock->eof) {
                    polls[i].revent = eIO_Read;
                    ++m/*ready*/;
                } else
                    polls[i].revent = eIO_Open;
                continue;
            }
            /* try to upread immediately readable sockets */
            if (polls[i].event == eIO_Write  &&  polls[i].revent == eIO_Read) {
                size_t dummy;
                sock = polls[i].sock;
                assert(sock                          &&
                       sock->sock != SOCK_INVALID    &&
                       sock->type == eSOCK_Socket    &&
                       sock->w_status != eIO_Closed  &&
                       sock->r_status != eIO_Closed  &&
                       !sock->eof  && !sock->pending &&
                       (sock->r_on_w == eOn
                        ||  (sock->r_on_w == eDefault
                             &&  s_ReadOnWrite == eOn)));
                (void) s_Read_(sock, 0, 0, &dummy, -1/*upread*/);
                if (sock->w_status == eIO_Closed) {
                    polls[i].revent = eIO_Write;
                    ++m/*ready*/;
                } else
                    polls[i].revent = eIO_Open;
            }
        }
        if (m)
            break/*ready*/;
    }

    assert(m);
    for ( ;  i < n;  ++i) {
        if (polls[i].revent != eIO_Close) {
            polls[i].revent = (EIO_Event)(polls[i].revent & polls[i].event);
            if (!polls[i].revent)
                continue;
        }
        ++m;
    }

    if ( n_ready )
        *n_ready = m;
    return eIO_Success;
}


static EIO_Status s_Wait(SOCK            sock,
                         EIO_Event       event,
                         const STimeout* timeout)
{
    struct timeval tv;
    SSOCK_Poll     poll;
    EIO_Status     status;

    poll.sock   = sock;
    poll.event  = event;
    poll.revent = eIO_Open;
    status = s_SelectStallsafe(1, &poll, s_to2tv(timeout, &tv), 0);
    assert(poll.event == event);
    if (status != eIO_Success)
        return status;
    if (poll.revent == eIO_Close)
        return eIO_Unknown;
    assert(!(poll.revent ^ (poll.revent & event)));
    return status/*success*/;
}


#ifdef NCBI_OS_MSWIN

static void x_AddTimeout(struct timeval* tv,
                         int             ms_addend)
{
    tv->tv_usec += (ms_addend % 1000) * 1000;
    tv->tv_sec  +=  ms_addend / 1000;
    if (tv->tv_usec >= 10000000) {
        tv->tv_sec  += tv->tv_usec / 10000000;
        tv->tv_usec %= 10000000;
    }
}

#endif /*NCBI_OS_MSWIN*/


#ifdef SOCK_SEND_SLICE
#  define s_Send  s_Send_
#endif /*SOCK_SEND_SLICE*/

/* Write data to the socket "as is" (as many bytes at once as possible).
 * Return eIO_Success iff at least some bytes have been written successfully.
 * Otherwise (nothing written), return an error code to indicate the problem.
 * NOTE: This call is for stream sockets only.
 */
static EIO_Status s_Send(SOCK        sock,
                         const void* data,
                         size_t      size,
                         size_t*     n_written,
                         int         flag/*OOB(<0); log(>0)*/)
{
    int/*bool*/ writeable;
    char _id[MAXIDLEN];

#ifdef NCBI_OS_MSWIN
    int wait_buf_ms = 0;
    struct timeval waited;
    memset(&waited, 0, sizeof(waited));
#endif /*NCBI_OS_MSWIN*/

    assert(sock->type == eSOCK_Socket  &&  data  &&  size > 0  &&  !*n_written);

    if (sock->w_status == eIO_Closed)
        return eIO_Closed;

    /* write to the socket */
    writeable = 0/*false*/;
    for (;;) { /* optionally auto-resume if interrupted */
        int error = 0;

        ssize_t x_written = send(sock->sock, (void*) data, WIN_INT_CAST size,
#ifdef MSG_NOSIGNAL
                                 (s_AllowSigPipe ? 0 : MSG_NOSIGNAL) |
#endif /*MSG_NOSIGNAL*/
                                 (flag < 0 ? MSG_OOB : 0));

        if (x_written >= 0  ||
            (x_written < 0  &&  ((error = SOCK_ERRNO) == SOCK_EPIPE         ||
                                 error                == SOCK_ENOTCONN      ||
                                 error                == SOCK_ETIMEDOUT     ||
                                 error                == SOCK_ENETRESET     ||
                                 error                == SOCK_ECONNRESET    ||
                                 error                == SOCK_ECONNABORTED  ||
                                 flag < 0/*OOB*/))) {
            /* statistics & logging */
            if ((x_written <= 0  &&  sock->log != eOff)  ||
                ((sock->log == eOn || (sock->log == eDefault && s_Log == eOn))
                 &&  (!sock->sslctx  ||  flag > 0))) {
                s_DoLog(x_written <= 0
                        ? (sock->n_read  &&  sock->n_written
                           ? eLOG_Error : eLOG_Trace)
                        : eLOG_Note, sock, eIO_Write,
                        x_written <= 0 ? (void*) &error : data,
                        (size_t)(x_written <= 0 ? 0 : x_written),
                        flag < 0/*OOB*/ ? "" : 0);
            }

            if (x_written > 0) {
                sock->n_written += (TNCBI_BigCount) x_written;
                *n_written       = (size_t)         x_written;
                sock->w_status = eIO_Success;
                break/*success*/;
            }
            if (x_written < 0) {
                if (error != SOCK_EPIPE  &&  error != SOCK_ENOTCONN)
                    sock->r_status = eIO_Closed;
                sock->w_status = eIO_Closed;
                break/*closed*/;
            }
        }

        if (flag < 0/*OOB*/  ||  !x_written)
            return eIO_Unknown;

        /* blocked -- retry if unblocked before the timeout expires
         * (use stall protection if specified) */
        if (error == SOCK_EWOULDBLOCK
#ifdef NCBI_OS_MSWIN
            ||  error == WSAENOBUFS
#endif /*NCBI_OS_MSWIN*/
            ||  error == SOCK_EAGAIN) {
            SSOCK_Poll            poll;
            EIO_Status            status;
            const struct timeval* timeout;

#ifdef NCBI_OS_MSWIN
            struct timeval        slice;
            unsigned int          writable = sock->writable;

            /* special send()'s semantics of IO event recording reset */
            sock->writable = 0/*false*/;
            if (error == WSAENOBUFS) {
                if (size < SOCK_BUF_CHUNK_SIZE / 4) {
                    x_AddTimeout(&waited, wait_buf_ms);
                    if (x_IsSmallerTimeout(SOCK_GET_TIMEOUT(sock, w),&waited)){
                        sock->writable = writable;
                        sock->w_status = eIO_Timeout;
                        break/*timeout*/;
                    }
                    if (wait_buf_ms == 0)
                        wait_buf_ms  = 10;
                    else if (wait_buf_ms < 500/*640*/)
                        wait_buf_ms <<= 1;
                    slice.tv_sec  = 0;
                    slice.tv_usec = wait_buf_ms * 1000;
                } else {
                    verify(size >>= 1);
                    memset(&slice, 0, sizeof(slice));
                }
                timeout = &slice;
                writeable = 0/*false*/;
            } else {
                if (wait_buf_ms) {
                    wait_buf_ms = 0;
                    memset(&waited, 0, sizeof(waited));
                }
                timeout = SOCK_GET_TIMEOUT(sock, w);
            }
#else
            {
                if (sock->w_tv_set && !(sock->w_tv.tv_sec|sock->w_tv.tv_usec)){
                    sock->w_status = eIO_Timeout;
                    break/*timeout*/;
                }
                timeout = SOCK_GET_TIMEOUT(sock, w);
            }
#endif /*NCBI_OS_MSWIN*/

            if (writeable) {
                CORE_TRACEF(("%s[SOCK::Send] "
                             " Spurious false indication of write-ready",
                             s_ID(sock, _id)));
            }
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pulling incoming data from the socket */
            status = s_SelectStallsafe(1, &poll, timeout, 0);
            assert(poll.event == eIO_Write);
#ifdef NCBI_OS_MSWIN
            if (error == WSAENOBUFS) {
                assert(timeout == &slice);
                sock->writable = writable/*restore*/;
                if (status == eIO_Timeout)
                    continue/*try to write again*/;
            } else
#endif /*NCBI_OS_MSWIN*/
            if (status == eIO_Timeout) {
                sock->w_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (status != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                return eIO_Unknown;
            assert(poll.event == eIO_Write);
            writeable = 1/*true*/;
            continue/*try to write again*/;
        }

        if (error != SOCK_EINTR) {
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOGF_ERRNO_EXX(11, eLOG_Trace,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::Send] "
                                 " Failed send()",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            /* don't want to handle all possible errors...
               let them be "unknown" */
            sock->w_status = eIO_Unknown;
            break/*unknown*/;
        }

        if (x_IsInterruptibleSOCK(sock)) {
            sock->w_status = eIO_Interrupt;
            break/*interrupt*/;
        }
    }

    return (EIO_Status) sock->w_status;
}


/* Wrapper for s_Send() that slices the output buffer for some brain-dead
 * systems (e.g. old Macs) that cannot handle large data chunks in "send()".
 * Return eIO_Success if and only if some data have been successfully sent;
 * otherwise, an error code if nothing at all has been sent.
 */
#ifdef SOCK_SEND_SLICE
#  undef s_Send
static EIO_Status s_Send(SOCK        sock,
                         const void* x_data,
                         size_t      size,
                         size_t*     n_written,
                         int         flag)
{
    /* split output buffer in slices (of size <= SOCK_SEND_SLICE) */
    const unsigned char* data = (const unsigned char*) x_data;
    unsigned int wtv_set = 2;
    struct timeval wtv;
    EIO_Status status;

    assert(sock->type == eSOCK_Socket  &&  data  &&  size > 0  &&  !*n_written);
    
    for (;;) {
        size_t n_todo = size > SOCK_SEND_SLICE ? SOCK_SEND_SLICE : size;
        size_t n_done = 0;
        status = s_Send_(sock, data, n_todo, &n_done, flag);
        assert((status == eIO_Success) == (n_done > 0));
        assert(n_done <= n_todo);
        if (status != eIO_Success)
            break;
        *n_written += n_done;
        if (n_todo != n_done)
            break;
        if (!(size -= n_done))
            break;
        data       += n_done;
        if (wtv_set & 2) {
            if ((wtv_set = sock->w_tv_set) != 0)
                wtv = sock->w_tv;
            /*zero timeout*/
            sock->w_tv_set = 1;
            memset(&sock->w_tv, 0, sizeof(sock->w_tv));
        }
    }
    if (!(wtv_set & 2)  &&  (sock->w_tv_set = wtv_set & 1) != 0)
        x_tvcpy(&sock->w_tv, &wtv);

    return *n_written ? eIO_Success : status;
}
#endif /*SOCK_SEND_SLICE*/


/* Return eIO_Success iff some data have been written; error code otherwise */
static EIO_Status s_WriteData(SOCK        sock,
                              const void* data,
                              size_t      size,
                              size_t*     n_written,
                              int/*bool*/ oob)
{
    EIO_Status status;

    assert(sock->type == eSOCK_Socket  &&  !sock->pending  &&  size > 0);

    if (sock->sslctx) {
        int error = 0;
        FSSLWrite sslwrite = s_SSL ? s_SSL->Write : 0;
        if (!sslwrite  ||  oob) {
            *n_written = 0;
            return eIO_NotSupported;
        }
        status = sslwrite(sock->sslctx->sess, data, size, n_written, &error);
        assert((status == eIO_Success) == (*n_written > 0));
        assert(status == eIO_Success  ||  error);

        /* statistics & logging */
        if ((status != eIO_Success  &&  sock->log != eOff)  ||
            sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn)) {
            s_DoLog(*n_written > 0 ? eLOG_Note : eLOG_Trace, sock, eIO_Write,
                    status == eIO_Success ? data : (void*) &error,
                    status != eIO_Success ? 0    : *n_written,
                    *n_written > 0 ? " [encrypt]" : 0);
        }

        if (status == eIO_Closed)
            sock->w_status = eIO_Closed;
        return status;
    }

    *n_written = 0;
    status = s_Send(sock, data, size, n_written, oob ? -1 : 0);
    assert((status == eIO_Success) == (*n_written > 0));
    return status;
}


struct XWriteBufCtx {
    SOCK       sock;
    EIO_Status status;
};


static size_t x_WriteBuf(void*       x_ctx,
                         const void* data,
                         size_t      size)
{
    struct XWriteBufCtx* ctx = (struct XWriteBufCtx*) x_ctx;
    size_t n_written = 0;

    assert(data  &&  size > 0  &&  ctx->status == eIO_Success);

    do {
        size_t x_written;
        ctx->status = s_WriteData(ctx->sock, data, size, &x_written, 0);
        assert((ctx->status == eIO_Success) == (x_written > 0));
        assert(x_written <= size);
        if (ctx->status != eIO_Success)
            break;
        n_written += x_written;
        size      -= x_written;
        data       = (const char*) data + x_written;
    } while (size);

    assert(!size/*n_written == initial size*/  ||  ctx->status != eIO_Success);

    return n_written;
}


static EIO_Status s_WritePending(SOCK                  sock,
                                 const struct timeval* tv,
                                 int/*bool*/           writeable,
                                 int/*bool*/           oob)
{
    struct XWriteBufCtx ctx;
    unsigned int wtv_set;
    struct timeval wtv;

    assert(sock->type == eSOCK_Socket  &&  sock->sock != SOCK_INVALID);

    if (sock->pending) {
        const char* what;
        int        error;
        EIO_Status status = s_IsConnected_(sock, tv, &what, &error, writeable);
        if (status != eIO_Success) {
            if (status != eIO_Timeout) {
                char _id[MAXIDLEN];
                const char* strerr = s_StrError(sock, error);
                CORE_LOGF_ERRNO_EXX(12, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::WritePending] "
                                     " Failed %s: %s",
                                     s_ID(sock, _id),
                                     what ? what : "pending connect()",
                                     IO_StatusStr(status)));
                UTIL_ReleaseBuffer(strerr);
                sock->w_status = status;
            }
            return status;
        }
        assert(sock->connected  &&  !sock->pending);
    }
    if ((!sock->sslctx  &&  oob)  ||  !sock->w_len)
        return eIO_Success;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;
    assert(sock->w_len == BUF_Size(sock->w_buf));

    if (tv != &sock->w_tv) {
        if ((wtv_set = sock->w_tv_set) != 0)
            wtv = sock->w_tv;
        SOCK_SET_TIMEOUT(sock, w, tv);
    } else
        wtv_set = 2;

    ctx.sock     = sock;
    ctx.status   = eIO_Success;
    sock->w_len -= BUF_PeekAtCB(sock->w_buf,
                                BUF_Size(sock->w_buf) - sock->w_len,
                                x_WriteBuf, &ctx, sock->w_len);
    assert((sock->w_len != 0) == (ctx.status != eIO_Success));

    if (!(wtv_set & 2)  &&  (sock->w_tv_set = wtv_set & 1) != 0)
        x_tvcpy(&sock->w_tv, &wtv);
    return ctx.status;
}


/* Write to the socket.  Return eIO_Success if some data have been written.
 * Return other (error) code only if nothing at all can be written.
 */
static EIO_Status s_Write_(SOCK        sock,
                           const void* data,
                           size_t      size,
                           size_t*     n_written,
                           int/*bool*/ oob)
{
    char _id[MAXIDLEN];
    EIO_Status status;

    assert(sock->type & eSOCK_Socket);

    if (sock->type == eSOCK_Datagram) {
        sock->w_len = 0;
        if (sock->eof) {
            BUF_Erase(sock->w_buf);
            sock->eof = 0;
        }
        if (BUF_Write(&sock->w_buf, data, size)) {
            *n_written = size;
            sock->w_status = eIO_Success;
        } else {
            int error = errno;
            CORE_LOGF_ERRNO_X(154, eLOG_Error, error,
                              ("%s%s "
                               " Failed to %s message (%lu + %lu byte%s)",
                               s_ID(sock, _id),
                               oob ? "[DSOCK::SendMsg]" : "[SOCK::Write]",
                               oob ? "finalize"         : "store",
                               (unsigned long) BUF_Size(sock->w_buf),
                               (unsigned long) size, &"s"[size == 1]));
            *n_written = 0;
            sock->w_status = eIO_Unknown;
        }
        return (EIO_Status) sock->w_status;
    }

    if (sock->w_status == eIO_Closed) {
        if (size) {
            CORE_TRACEF(("%s[SOCK::Write] "
                         " Socket already shut down for writing",
                         s_ID(sock, _id)));
        }
        *n_written = 0;
        return eIO_Closed;
    }

    status = s_WritePending(sock, SOCK_GET_TIMEOUT(sock, w), 0, oob);
    if (status != eIO_Success  ||  !size) {
        *n_written = 0;
        if (status == eIO_Timeout  ||  status == eIO_Closed)
            return status;
        return size ? status : eIO_Success;
    }

    assert(size > 0  &&  sock->w_len == 0);
    return s_WriteData(sock, data, size, n_written, oob);
}


static EIO_Status s_Write(SOCK        sock,
                          const void* data,
                          size_t      size,
                          size_t*     n_written,
                          int/*bool*/ oob)
{
    EIO_Status status = s_Write_(sock, data, size, n_written, oob);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = eIO_Write;
        info.status = status;
        s_ErrorCallback(&info);
    }
    assert(*n_written <= size);
    return status;
}


/* For non-datagram sockets only */
static EIO_Status s_Shutdown(SOCK                  sock,
                             EIO_Event             dir,
                             const struct timeval* tv)
{
    int        error;
    char       _id[MAXIDLEN];
    EIO_Status status = eIO_Success;
    int        how = SOCK_SHUTDOWN_WR;

    assert(sock->type == eSOCK_Socket);

    switch (dir) {
    case eIO_Read:
        if (sock->eof) {
            /* hit EOF (and maybe not yet shut down) -- so, flag it as been
             * shut down, but do not perform the actual system call,
             * as it can cause smart OS'es like Linux to complain
             */
            sock->eof = 0/*false*/;
            sock->r_status = eIO_Closed;
        }
        if (sock->r_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        sock->r_status = eIO_Closed;
        how = SOCK_SHUTDOWN_RD;
        break;

    case eIO_ReadWrite:
        if (sock->eof) {
            sock->eof = 0/*false*/;
            sock->r_status = eIO_Closed;
        } else
            how = SOCK_SHUTDOWN_RDWR;
        if (sock->w_status == eIO_Closed  &&  sock->r_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        /*FALLTHRU*/

    case eIO_Write:
        if (sock->w_status == eIO_Closed  &&  dir == eIO_Write)
            return eIO_Success;  /* has been shut down already */
        /*FALLTHRU*/

    case eIO_Open:
    case eIO_Close:
        if (sock->w_status != eIO_Closed) {
            if ((status = s_WritePending(sock, tv, 0, 0)) != eIO_Success) {
                if (!sock->pending  &&  sock->w_len) {
                    CORE_LOGF_X(13, !tv  ||  (tv->tv_sec | tv->tv_usec)
                                ? eLOG_Warning : eLOG_Trace,
                                ("%s[SOCK::%s] "
                                 " %s with output (%lu byte%s) still pending"
                                 " (%s)", s_ID(sock, _id),
                                 dir & eIO_ReadWrite ? "Shutdown" : "Close",
                                 !dir ? "Leaving " : dir == eIO_Close
                                 ?      "Closing"  : dir == eIO_Write
                                 ?      "Shutting down for write"
                                 :      "Shutting down for read/write",
                                 (unsigned long) sock->w_len,
                                 &"s"[sock->w_len == 1],
                                 IO_StatusStr(status)));
                } else if (!(dir & eIO_ReadWrite)  &&  !sock->w_len)
                    status = eIO_Success;
            }
            if (dir != eIO_Write  &&  !sock->pending  &&  sock->sslctx) {
                FSSLClose sslclose = s_SSL ? s_SSL->Close : 0;
                if (sslclose) {
                    const unsigned int rtv_set = sock->r_tv_set;
                    const unsigned int wtv_set = sock->w_tv_set;
                    struct timeval rtv;
                    struct timeval wtv;
                    if (rtv_set)
                        rtv = sock->r_tv;
                    if (wtv_set)
                        wtv = sock->w_tv;
                    SOCK_SET_TIMEOUT(sock, r, tv);
                    SOCK_SET_TIMEOUT(sock, w, tv);
                    status = sslclose(sock->sslctx->sess, how, &error);
                    if ((sock->w_tv_set = wtv_set & 1) != 0)
                        x_tvcpy(&sock->w_tv, &wtv);
                    if ((sock->r_tv_set = rtv_set & 1) != 0)
                        x_tvcpy(&sock->r_tv, &rtv);
                    if (status != eIO_Success) {
                        const char* strerr = s_StrError(sock, error);
                        CORE_LOGF_ERRNO_EXX(127, eLOG_Trace,
                                            error, strerr ? strerr : "",
                                            ("%s[SOCK::%s] "
                                             " Failed SSL teardown",
                                             s_ID(sock, _id),
                                             dir & eIO_ReadWrite
                                             ? "Shutdown" : "Close"));
                        UTIL_ReleaseBuffer(strerr);
                    }
                }
            }
            sock->w_status = eIO_Closed;
        }

        sock->w_len = 0;
        BUF_Erase(sock->w_buf);
        if (dir != eIO_Write) {
            sock->eof = 0/*false*/;
            sock->r_status = eIO_Closed;
            if (!(dir & eIO_ReadWrite))
                return status;
        }
        break;

    default:
        assert(0);
        return eIO_InvalidArg;
    }
    assert((EIO_Event)(dir | eIO_ReadWrite) == eIO_ReadWrite);

    /* Secure connection can use both READ and WRITE for any I/O */
    if (dir != eIO_ReadWrite  &&  sock->sslctx)
        return status;

#ifdef NCBI_OS_BSD
    /* at least on FreeBSD: shutting down a socket for write (i.e. forcing to
     * send a FIN) for a socket that has been already closed by the other end
     * (e.g. when peer has done writing, so this end has done reading and is
     * about to close) seems to cause ECONNRESET in the coming close()...
     * see kern/146845 @ http://www.freebsd.org/cgi/query-pr.cgi?pr=146845 */
    if (dir == eIO_ReadWrite  &&  how != SOCK_SHUTDOWN_RDWR)
        return status;
#endif /*NCBI_OS_BSD*/

    if (s_Initialized > 0  &&  shutdown(sock->sock, how) != 0) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(16, eLOG_Trace,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Shutdown] "
                             " Failed shutdown(%s)",
                             s_ID(sock, _id), dir == eIO_Read ? "R" :
                             dir == eIO_Write ? "W" : "RW"));
        UTIL_ReleaseBuffer(strerr);
        status = eIO_Unknown;
    }

    return status;
}


enum ESOCK_Keep {
    fSOCK_KeepNone    = 0,
    fSOCK_KeepEvent   = 1,
    fSOCK_KeepSession = 2,
    fSOCK_KeepPending = 4
};
typedef unsigned int TSOCK_Keep;  /* Bitwise-OR of ESOCK_Keep */


/* Close the socket either orderly (abort==0) or abnormally (abort!=0):
 * abort == -2 to abort the socket silently;
 * abort == -1 to abort the socket internally;
 * abort ==  1 to abort the socket from SOCK_Abort();
 * abort ==  2 to re-close the socket when SOCK_CreateOnTop*(sock) failed.
 */
static EIO_Status s_Close_(SOCK       sock,
                           int        abort,
                           TSOCK_Keep keep)
{
    int/*bool*/ linger = 0/*false*/;
    char       _id[MAXIDLEN];
    EIO_Status status;
    int        error;

    assert(abs(abort) <= 2);
    assert(sock->sock != SOCK_INVALID);
    if (sock->type == eSOCK_Datagram) {
        assert(!abort);
        sock->r_len = 0;
        status = eIO_Success;
        BUF_Erase(sock->r_buf);
        BUF_Erase(sock->w_buf);
        keep = fSOCK_KeepNone;
    } else if ((abort  &&  abort < 2)  ||  !sock->keep) {
        keep = fSOCK_KeepNone;
#if (defined(NCBI_OS_UNIX) && !defined(NCBI_OS_BEOS)) || defined(NCBI_OS_MSWIN)
        /* setsockopt() is not implemented for MAC (MIT socket emulation lib)*/
        if (sock->w_status != eIO_Closed
#  ifdef NCBI_OS_UNIX
            &&  !sock->path[0]
#  endif /*NCBI_OS_UNIX*/
            ) {
            /* set the close()'s linger period be equal to the close timeout */
            struct linger lgr;

            if (abort) {
                lgr.l_linger = 0;   /* RFC 793, Abort */
                lgr.l_onoff  = 1;
            } else if (!sock->c_tv_set) {
                linger = 1/*true*/;
                lgr.l_linger = 120; /* this is standard TCP TTL, 2 minutes */
                lgr.l_onoff  = 1;
            } else if (sock->c_tv.tv_sec | sock->c_tv.tv_usec) {
                int seconds = (int)(sock->c_tv.tv_sec  +
                                   (sock->c_tv.tv_usec + 500000) / 1000000);
                if (seconds) {
                    linger = 1/*true*/;
                    lgr.l_linger = seconds;
                    lgr.l_onoff  = 1;
                } else
                    lgr.l_onoff  = 0;
            } else
                lgr.l_onoff = 0;
            if (lgr.l_onoff
                &&  setsockopt(sock->sock, SOL_SOCKET, SO_LINGER,
                               (char*) &lgr, sizeof(lgr)) != 0
                &&  abort >= 0  &&  sock->connected) {
                const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
                CORE_LOGF_ERRNO_EXX(17, eLOG_Trace,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::%s] "
                                     " Failed setsockopt(SO_LINGER)",
                                     s_ID(sock, _id),
                                     abort ? "Abort" : "Close"));
                UTIL_ReleaseBuffer(strerr);
            }
#  ifdef TCP_LINGER2
            if (abort  ||
                (sock->c_tv_set && !(sock->c_tv.tv_sec | sock->c_tv.tv_usec))){
                int no = -1;
                if (setsockopt(sock->sock, IPPROTO_TCP, TCP_LINGER2,
                               (char*) &no, sizeof(no)) != 0
                    &&  !abort  &&  sock->connected) {
                    const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
                    CORE_LOGF_ERRNO_EXX(18, eLOG_Trace,
                                        error, strerr ? strerr : "",
                                        ("%s[SOCK::Close] "
                                         " Failed setsockopt(TCP_LINGER2)",
                                         s_ID(sock, _id)));
                    UTIL_ReleaseBuffer(strerr);
                }
            }
#  endif /*TCP_LINGER2*/
        }
#endif /*(NCBI_OS_UNIX && !NCBI_OS_BEOS) || NCBI_OS_MSWIN*/

        if (abort) {
            sock->eof = 0;
            sock->w_len = 0;
            sock->pending = 0;
            status = eIO_Success;
            BUF_Erase(sock->r_buf);
            BUF_Erase(sock->w_buf);
            sock->r_status = sock->w_status = eIO_Closed;
        } else {
            /* orderly shutdown in both directions */
            status = s_Shutdown(sock, eIO_Close, SOCK_GET_TIMEOUT(sock, c));
            assert(sock->r_status == eIO_Closed  &&
                   sock->w_status == eIO_Closed);
            assert(sock->w_len == 0);
        }
    } else if (!(keep & fSOCK_KeepPending)) {
        /* flush everything out */
        status = s_Shutdown(sock, eIO_Open, SOCK_GET_TIMEOUT(sock, c));
        assert(sock->w_len == 0);
    } else
        status = eIO_Success;

    if (!(keep & fSOCK_KeepSession)  &&  sock->sslctx  &&  sock->sslctx->sess){
        FSSLDelete ssldelete = s_SSL ? s_SSL->Delete : 0;
        if (ssldelete)
            ssldelete(sock->sslctx->sess);
        sock->sslctx->sess = 0;
        sock->sslctx->sock = 0;
    }

    if (abs(abort) <= 1) {
        /* statistics & logging */
        if (sock->type != eSOCK_Datagram) {
            sock->n_in  += sock->n_read;
            sock->n_out += sock->n_written;
        }
        if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
            s_DoLog(eLOG_Note, sock, eIO_Close, 0, 0, abort ? "Aborting" : 0);
    }

#ifdef NCBI_OS_MSWIN
    if (!(keep & fSOCK_KeepEvent))
        WSAEventSelect(sock->sock, sock->event/*ignored*/, 0/*de-associate*/);
#endif /*NCBI_OS_MSWIN*/

    if (s_Initialized > 0  &&  !sock->keep
        /* set the socket back to blocking mode */
        &&  linger  &&  !s_SetNonblock(sock->sock, 0/*false*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        assert(!abort);
        CORE_LOGF_ERRNO_EXX(19, eLOG_Trace,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Close] "
                             " Cannot set socket back to blocking mode",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
    }

    if ((abort  &&  abort < 2)  ||  !sock->keep) {
        if (abort)
            abort = 1;
#ifdef NCBI_MONKEY
        /* Not interception of close():  only to "forget" this socket */
        if (g_MONKEY_Close)
            g_MONKEY_Close(sock->sock);
#endif /*NCBI_MONKEY*/
        for (;;) { /* close persistently - retry if interrupted by a signal */
            if (SOCK_CLOSE(sock->sock) == 0)
                break;
            /* error */
            if (s_Initialized <= 0)
                break;
            error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
            if (error == WSANOTINITIALISED) {
                s_Initialized = -1/*deinited*/;
                break;
            }
#endif /*NCBI_OS_MSWIN*/
            if (error == SOCK_ENOTCONN/*already closed by now*/
                ||  (!(sock->n_read | sock->n_written)
                     &&  (error == SOCK_ENETRESET   ||
                          error == SOCK_ECONNRESET  ||
                          error == SOCK_ECONNABORTED))) {
                break;
            }
            if (abort  ||  error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(error);
                CORE_LOGF_ERRNO_EXX(21, abort == 1 ? eLOG_Warning : eLOG_Error,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::%s] "
                                     " Failed close()",
                                     s_ID(sock, _id),
                                     abort ? "Abort" : "Close"));
                UTIL_ReleaseBuffer(strerr);
                if (abort > 1  ||  error != SOCK_EINTR) {
                    status =
                        error == SOCK_ETIMEDOUT ? eIO_Timeout : eIO_Unknown;
                    break;
                }
                if (abort)
                    abort = 2;
            }
        }
        sock->sock = SOCK_INVALID;
#ifdef NCBI_OS_MSWIN
        WSASetEvent(sock->event); /*signal closure*/
#endif /*NCBI_OS_MSWIN*/
    } else
        sock->sock = SOCK_INVALID;

#ifdef NCBI_OS_MSWIN
    if (!(keep & fSOCK_KeepEvent)) {
        WSACloseEvent(sock->event);
        sock->event = 0;
    }
#endif /*NCBI_OS_MSWIN*/
    sock->myport = 0;
    return status;
}


static EIO_Status s_Close(SOCK        sock,
                          int/*bool*/ reclose,
                          TSOCK_Keep  keep)
{
    EIO_Status status;

    assert(!reclose  ||  !keep);
    status = s_Close_(sock, reclose << 1, keep);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = eIO_Close;
        info.status = status;
        s_ErrorCallback(&info);        
    }

    assert(sock->sock == SOCK_INVALID);
    return status;
}


/* Connect the (pre-allocated) socket to the specified "host:port"/"file" peer.
 * HINT: if "host" is NULL then keep the original host;
 *       likewise for zero "port".
 * NOTE: Client-side stream sockets only.
 */
static EIO_Status s_Connect_(SOCK            sock,
                             const char*     host,
                             unsigned short  port,
                             const STimeout* timeout)
{
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un  un;
#endif /*NCBI_OS_UNIX*/
    } u;
    char            _id[MAXIDLEN];
    TSOCK_socklen_t addrlen;
    SNcbiSSLctx*    sslctx;
    EIO_Status      status;
    int             error;
    int             type;
    TSOCK_Handle    fd;
    int             n;

    assert(sock->sock == SOCK_INVALID);
    assert(sock->type == eSOCK_Socket  &&  sock->side == eSOCK_Client);

    sslctx = sock->sslctx;
    /* initialize internals */
    if (s_InitAPI(sslctx ? 1/*secure*/ : 0/*regular*/) != eIO_Success)
        return eIO_NotSupported;

#ifdef NCBI_OS_UNIX
    if (sock->path[0]) {
        size_t pathlen = strlen(sock->path) + 1/*account for '\0'*/;
        if (sizeof(u.un.sun_path) < pathlen) {
            CORE_LOGF_X(142, eLOG_Error,
                        ("%s[SOCK::Connect] "
                         " Path too long (%lu vs %lu bytes allowed)",
                         s_ID(sock, _id), (unsigned long) pathlen,
                         (unsigned long) sizeof(u.un.sun_path)));
            return eIO_InvalidArg;
        }
        addrlen = sizeof(u.un);
        memset(&u, 0, addrlen);
        u.un.sun_family = AF_UNIX;
#  ifdef HAVE_SIN_LEN
        u.un.sun_len    = addrlen;
#  endif /*HAVE_SIN_LEN*/
        memcpy(u.un.sun_path, sock->path, pathlen);
        assert(!sock->port  &&  !port);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        int/*bool*/ ipv4;
        /* first, set the port to connect to (same port if zero) */
        if (port)
            sock->port = port;
        else
            assert(sock->port);
        /* get address of the remote host (assume the same host if NULL) */
        if (host
            &&  !s_gethostbyname(&sock->addr, host, s_IPVersion, 0, (ESwitch) sock->log)) {
            CORE_LOGF_X(22, eLOG_Error,
                        ("%s[SOCK::Connect] "
                         " Failed SOCK_gethostbyname(\"%.*s\")",
                         s_ID(sock, _id), CONN_HOST_LEN, host));
            return eIO_Unknown;
        }
        ipv4 = NcbiIsIPv4(&sock->addr);
        addrlen = ipv4 ? sizeof(u.in) : sizeof(u.in6);
        memset(&u, 0, addrlen);
        u.sa.sa_family = ipv4 ? AF_INET : AF_INET6;
#ifdef HAVE_SIN_LEN
        u.sa.sa_len    = addrlen;
#endif /*HAVE_SIN_LEN*/
        if (ipv4) {
            u.in.sin_addr.s_addr = NcbiIPv6ToIPv4(&sock->addr, 0);
            u.in.sin_port        = htons(sock->port);
        } else {
            memcpy(&u.in6.sin6_addr, &sock->addr, sizeof(u.in6.sin6_addr));
            u.in6.sin6_port      = htons(sock->port);
        }
        if (s_ApproveHook) {
            status = s_ApproveCallback(SOCK_IsAddress(host) ? 0 : host,
                                       &sock->addr, sock->port,
                                       eSOCK_Client, eSOCK_Socket, sock);
            if (status != eIO_Success)
                return status;
        }
    }

    /* create a new socket */
    type  = SOCK_STREAM;
#ifdef SOCK_NONBLOCK
    type |= SOCK_NONBLOCK;
#endif /*SOCK_NONBLOCK*/
#ifdef SOCK_CLOEXEC
    if (!sock->crossexec  ||  sslctx)
        type |= SOCK_CLOEXEC;
#endif /*SOCK_CLOEXEC*/
    if ((fd = socket(u.sa.sa_family, type, 0)) == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(23, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Cannot create socket",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
    sock->sock = fd;

#ifdef NCBI_MONKEY
    /* Bind created fd to the sock in Chaos Monkey, this information is 
       important to keep rules working */
    if (g_MONKEY_SockHasSocket)
        g_MONKEY_SockHasSocket(sock, fd);
#endif /*NCBI_MONKEY*/

#if defined(NCBI_OS_MSWIN)
    assert(!sock->event);
    if (!(sock->event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(122, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Failed to create IO event",
                             s_ID(sock, _id)));
        UTIL_ReleaseBufferOnHeap(strerr);
        s_Close_(sock, -2/*silent abort*/, fSOCK_KeepNone);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(sock->sock, sock->event, SOCK_EVENTS) != 0) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(123, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Failed to bind IO event",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        s_Close_(sock, -2/*silent abort*/, fSOCK_KeepNone);
        return eIO_Unknown;
    }
#elif !defined(SOCK_NONBLOCK)
    /* set non-blocking mode */
    if (!s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(24, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Cannot set socket to non-blocking mode",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        s_Close_(sock, -2/*silent abort*/, fSOCK_KeepNone);
        return eIO_Unknown;
    }
#endif /*O_NONBLOCK setting*/

    if (sock->port) {
#ifdef SO_KEEPALIVE
        if (sock->keepalive  &&  !s_SetKeepAlive(fd, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(151, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::Connect] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(fd, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(135, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::Connect] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

#ifndef SOCK_CLOEXEC
    if ((!sock->crossexec  ||  sslctx)  &&  !s_SetCloexec(fd, 1)) {
        const char* strerr;
#  ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        error = err;
#  else
        error = errno;
        strerr = SOCK_STRERROR(error);
#  endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(129, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Cannot set socket close-on-exec mode",
                             s_ID(sock, _id)));
#  ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#  else
        UTIL_ReleaseBuffer(strerr);
#  endif /*NCBI_OS_MSWIN*/
    }
#endif /*!SOCK_CLOEXEC*/

    if (sslctx) {
        FSSLCreate sslcreate = s_SSL ? s_SSL->Create : 0;
        assert(!sslctx->sess);
        assert(!sslctx->host  ||  *sslctx->host);
        if (sslcreate) {
            sslctx->sock = sock;
            sslctx->sess = sslcreate(eSOCK_Client, sslctx, &error);
        } else
            error = 0;
        if (!sslctx->sess) {
            const char* strerr = s_StrError(sock, error);
            CORE_LOGF_ERRNO_EXX(131, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::Connect] "
                                 " %s to initialize secure session%s%s",
                                 s_ID(sock, _id),
                                 sslcreate ? "Failed" : "Unable",
                                 sslctx->host ? " with "     : "",
                                 sslctx->host ? sslctx->host : 0));
            UTIL_ReleaseBuffer(strerr);
            s_Close_(sock, -2/*silent abort*/, fSOCK_KeepNone);
            return eIO_NotSupported;
        }
    }

    /* establish connection to the peer */
    sock->eof       = 0/*false*/;
    sock->r_status  = eIO_Success;
    sock->w_status  = eIO_Success;
    sock->pending   = 1/*true*/;
    sock->connected = 0/*false*/;
#ifdef NCBI_OS_MSWIN
    sock->readable  = 0/*false*/;
    sock->writable  = 0/*false*/;
    sock->closing   = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/
    assert(sock->w_len == 0);
    for (n = 0;  ; n = 1) { /* optionally auto-resume if interrupted */
        if (connect(fd, &u.sa, addrlen) == 0) {
            error = 0;
            break;
        }
        error = SOCK_ERRNO;
        if (error != SOCK_EINTR  ||  x_IsInterruptibleSOCK(sock))
            break;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, 0, 0, error ? 0 : "Connected");

    if (error) {
        if (((n == 0  &&  error != SOCK_EINPROGRESS)  ||
             (n != 0  &&  error != SOCK_EALREADY))
            &&  error != SOCK_EWOULDBLOCK) {
            if (error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(error);
                CORE_LOGF_ERRNO_EXX(25, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::Connect] "
                                     " Failed connect()",
                                     s_ID(sock, _id)));
                UTIL_ReleaseBuffer(strerr);
                if (error == SOCK_ECONNREFUSED
#ifdef NCBI_OS_UNIX
                    ||  (sock->path[0]  &&  error == ENOENT)
#endif /*NCBI_OS_UNIX*/
                    ) {
                    status = eIO_Closed;
                } else
                    status = eIO_Unknown;
            } else
                status = eIO_Interrupt;
            s_Close_(sock, -1/*internal abort*/, fSOCK_KeepNone);
            return status/*error*/;
        }
    }

    if (!error  ||  !timeout  ||  (timeout->sec | timeout->usec)) {
        const char* what;
        struct timeval tv;
        const struct timeval* x_tv = s_to2tv(timeout, &tv);

        status = s_IsConnected_(sock, x_tv, &what, &error, !error);
        if (status != eIO_Success) {
            char buf[80];
            const char* reason;
            if (status == eIO_Timeout) {
                assert(x_tv/*it is also normalized*/);
                sprintf(buf, "%s[%u.%06u]",
                        IO_StatusStr(status),
                        (unsigned int) x_tv->tv_sec,
                        (unsigned int) x_tv->tv_usec);
                reason = buf;
            } else
                reason = IO_StatusStr(status);
            {
                const char* strerr = s_StrError(sock, error);
                CORE_LOGF_ERRNO_EXX(26, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::Connect] "
                                     " Failed %s: %s", s_ID(sock, _id),
                                     what ? what : "pending connect()",
                                     reason));
                UTIL_ReleaseBuffer(strerr);
            }
            s_Close_(sock, -1/*internal abort*/, fSOCK_KeepNone);
            return status;
        }
    }

    /* success: do not change any timeouts */
    sock->w_len = BUF_Size(sock->w_buf);
    return eIO_Success;
}


static EIO_Status s_Connect(SOCK            sock,
                            const char*     host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    EIO_Status status = s_Connect_(sock, host, port, timeout);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (!host) {
#ifdef NCBI_OS_UNIX
            if (sock->path[0]) {
                assert(!sock->port  &&  !port);
                info.host = sock->path;
            } else
#endif /*NCBI_OS_UNIX*/
            {
                s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion,
                               NcbiIsEmptyIPv6(&sock->addr));
                info.host = addr;
            }
        } else
            info.host = host;
        info.port = port ? port : sock->port;
        info.event = eIO_Open;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


/* NB: a sped-up version of !SOCK_IsAddress(), basically */
static int/*bool*/ s_VHostOk(const char* vhost)
{
    TNCBI_IPv6Addr temp;
    const char*    end;
    size_t         len;

    if (!vhost  ||  !*vhost)
        return 0;
    if (NCBI_HasSpaces(vhost, len = strlen(vhost)))
        return 0/*false*/;
    if (SOCK_isip(vhost))
        return 0/*false*/;
    end = NcbiStringToIPv6(&temp, vhost, len);
    if (end  &&  !*end)
        return 0/*false*/;
    return 1/*true*/;
}


static EIO_Status s_Create(const char*       hostpath,
                           unsigned short    port,
                           const STimeout*   timeout,
                           SOCK*             sock,
                           const SSOCK_Init* init,
                           TSOCK_Flags       flags)
{
    size_t       size = port ? 0 : strlen(hostpath);
    unsigned int x_id = x_ID_Counter() * 1000;
    char         _id[MAXIDLEN];
    EIO_Status   status;
    SOCK         x_sock;

    assert(!*sock);

    /* allocate memory for the internal socket structure(s) */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + size)))
        return eIO_Unknown;
    if (flags & fSOCK_Secure) {
        SNcbiSSLctx* sslctx;
        const char*  host;
        if (init  &&  s_VHostOk(init->host)) {
            host = init->host;
            assert(*host);
        } else
            host = 0;
        if (!(sslctx = (SNcbiSSLctx*) calloc(1, sizeof(*sslctx)))) {
            free(x_sock);
            return eIO_Unknown;
        }
        sslctx->cred = init ? init->cred   : 0;
        sslctx->host = host ? strdup(host) : 0;
        x_sock->sslctx = sslctx;
    }
    x_sock->sock      = SOCK_INVALID;
    x_sock->id        = x_id;
    x_sock->type      = eSOCK_Socket;
    x_sock->side      = eSOCK_Client;
    x_sock->log       = flags & (fSOCK_LogDefault | fSOCK_LogOn);
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    x_sock->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn : eDefault;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    x_sock->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/ : 0/*false*/;
#ifdef NCBI_OS_UNIX
    if (!port)
        memcpy(x_sock->path, hostpath, size + 1);
#endif /*NCBI_OS_UNIX*/

    /* setup initial data */
    size = init ? init->size : 0;
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    if (size
        &&  (BUF_SetChunkSize(&x_sock->w_buf, size) < size
             ||  !BUF_Write(&x_sock->w_buf, init ? init->data : 0, size))) {
        CORE_LOGF_ERRNO_X(27, eLOG_Critical, errno,
                          ("%s[SOCK::Create] "
                           " Cannot store initial data (%lu byte%s)",
                           s_ID(x_sock, _id), (unsigned long) size,
                           &"s"[size == 1]));
        SOCK_Destroy(x_sock);
        return eIO_Unknown;
    }

    /* connect */
    status = s_Connect(x_sock, hostpath, port, timeout);
    if (status != eIO_Success)
        SOCK_Destroy(x_sock);
    else
        *sock = x_sock;
    return status;
}


/* Mimic SOCK_CLOSE() */
static void SOCK_ABORT(TSOCK_Handle x_sock)
{
    struct SOCK_tag temp;
    memset(&temp, 0, sizeof(temp));
    temp.side = eSOCK_Server;
    temp.type = eSOCK_Socket;
    temp.sock = x_sock;
    s_Close_(&temp, -2/*silent abort*/, fSOCK_KeepNone);
}


static EIO_Status s_CreateOnTop(const void*       handle,
                                size_t            handle_size,
                                SOCK*             sock,
                                const SSOCK_Init* init,
                                TSOCK_Flags       flags)
{
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un  un;
#endif /*NCBI_OS_UNIX*/
    } u;
    size_t          size;
    int             error;
    EIO_Status      status;
    TSOCK_socklen_t addrlen;
    size_t          socklen;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event = 0;
#endif /*NCBI_OS_MSWIN*/
    BUF             w_buf = 0;
    unsigned short  myport = 0;
    char            _id[MAXIDLEN];
    SOCK            x_sock, x_orig = 0;
    SNcbiSSLctx*    sslctx = 0, *oldctx = 0;
    TSOCK_Handle    fd, oldfd = SOCK_INVALID;
    unsigned int    x_id = x_ID_Counter() * 1000;

    assert(!*sock);

    if (!handle  ||  (handle_size  &&  handle_size != sizeof(fd))) {
        CORE_LOGF_X(47, eLOG_Error,
                    ("SOCK#%u[?]: [SOCK::CreateOnTop] "
                     " Invalid handle%s %lu",
                     x_id,
                     handle ? " size"                     : "",
                     handle ? (unsigned long) handle_size : 0UL));
        assert(0);
        return eIO_InvalidArg;
    }

    if (!handle_size) {
        TSOCK_Keep keep = fSOCK_KeepEvent;
        x_orig = (SOCK) handle;
        if (x_orig->type != eSOCK_Socket) {
            assert(0);
            return eIO_InvalidArg;
        }
        fd = x_orig->sock;
        if (s_Initialized <= 0  ||  fd == SOCK_INVALID)
            return eIO_Closed;
        if (!x_orig->keep) {
            x_orig->keep = 1/*true*/;
            oldfd = fd;
        }
        if (!x_orig->sslctx == !(flags & fSOCK_Secure)) {
            keep |= fSOCK_KeepPending;
            if (flags & fSOCK_Secure) {
                keep |= fSOCK_KeepSession;
                oldctx = x_orig->sslctx;
            }
        }
        myport = x_orig->myport;
        s_Close(x_orig, 0, keep);
#ifdef NCBI_OS_MSWIN
        event = x_orig->event;
        x_orig->event = 0;
        assert(event);
#endif/*NCBI_OS_MSWIN*/
        if (oldfd != SOCK_INVALID)
            x_orig->keep = 0/*false*/;
    } else
        memcpy(&fd, handle, sizeof(fd));

    /* initialize internals */
    if (s_InitAPI(flags & fSOCK_Secure) != eIO_Success) {
        status = eIO_NotSupported;
        goto errout;
    }

    /* get peer's address */
    memset(&u, 0, sizeof(u));
    if (!x_orig) {
        addrlen = sizeof(u);
#ifdef HAVE_SIN_LEN
        u.sa.sa_len = addrlen;
#endif /*HAVE_SIN_LEN*/
        if (getpeername(fd, &u.sa, &addrlen) != 0) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(148, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                                 " %s %s handle",
                                 x_id, (unsigned int) fd,
                                 error == SOCK_ENOTCONN
                                 ? "Unconnected" : "Invalid",
                                 x_orig ? "SOCK" : "OS socket"));
            UTIL_ReleaseBuffer(strerr);
            return eIO_Closed;
        }
#ifdef NCBI_OS_UNIX
        if (u.sa.sa_family != AF_INET  &&  u.sa.sa_family != AF_INET6
            &&  u.sa.sa_family != AF_UNIX)
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
            if (u.sa.sa_family != AF_UNSPEC/*0*/)
#  endif /*NCBI_OS*/
#else
        if (u.sa.sa_family != AF_INET  &&  u.sa.sa_family != AF_INET6)
#endif /*NCBI_OS_UNIX*/
            return eIO_NotSupported;
    }

#ifdef NCBI_OS_UNIX
    if (x_orig  &&  x_orig->path[0])
        socklen = strlen(x_orig->path);
    else if (!x_orig  &&  (
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
                             u.sa.sa_family == AF_UNSPEC/*0*/  ||
#  endif /*NCBI_OS*/
                             u.sa.sa_family == AF_UNIX)) {
        if (addrlen == sizeof(u.sa.sa_family)  ||  !u.un.sun_path[0]) {
            memset(&u, 0, sizeof(u));
            addrlen = sizeof(u);
#  ifdef HAVE_SIN_LEN
            u.sa.sa_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
            if (getsockname(fd, &u.sa, &addrlen) != 0)
                return eIO_Closed;
            assert(u.sa.sa_family == AF_UNIX);
            if (addrlen == sizeof(u.sa.sa_family) || !u.un.sun_path[0]) {
                CORE_LOGF_X(48, eLOG_Error,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " %s UNIX socket handle",
                             x_id, (unsigned int) fd,
                             addrlen == sizeof(u.sa.sa_family)
                             ? "Unnamed" : "Abstract"));
                return eIO_InvalidArg;
            }
        }
        socklen = strnlen(u.un.sun_path, sizeof(u.un.sun_path));
        assert(socklen);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        socklen = 0;
        assert(!x_orig  ||  x_orig->port);
    }

#ifdef NCBI_OS_MSWIN
    if (!event) {
        assert(!x_orig);
        if (!(event = WSACreateEvent())) {
            DWORD err = GetLastError();
            const char* strerr = s_WinStrerror(err);
            CORE_LOGF_ERRNO_EXX(31, eLOG_Error,
                                err, strerr ? strerr : "",
                                ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                                 " Failed to create IO event",
                                 x_id, (unsigned int) fd));
            UTIL_ReleaseBufferOnHeap(strerr);
            return eIO_Unknown;
        }
        /* NB: WSAEventSelect() sets non-blocking automatically */
        if (WSAEventSelect(fd, event, SOCK_EVENTS) != 0) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(32, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                                 " Failed to bind IO event",
                                 x_id, (unsigned int) fd));
            UTIL_ReleaseBuffer(strerr);
            return eIO_Unknown;
        }
    } else
        assert(x_orig);
#else
    /* set to non-blocking mode */
    if (!x_orig  &&  !s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(50, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
#endif /*NCBI_OS_MSWIN*/

    status = eIO_Unknown;

    /* setup initial data */
    size = init ? init->size : 0;
    if (size
        &&  (BUF_SetChunkSize(&w_buf, size) < size
             ||  !BUF_Write(&w_buf, init ? init->data : 0, size))) {
        CORE_LOGF_ERRNO_X(49, eLOG_Critical, errno,
                          ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                           " Cannot store initial data (%lu byte%s)",
                           x_id, (unsigned int) fd, (unsigned long) size,
                           &"s"[size == 1]));
        goto errout;
    }

    /* create and fill in a socket handle */
    if ((flags & fSOCK_Secure)
        &&  !(sslctx = (SNcbiSSLctx*) calloc(1, sizeof(*sslctx)))) {
        goto errout;
    }
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + socklen)))
        goto errout;
    x_sock->sock      = fd;
    x_sock->id        = x_id;
#ifdef NCBI_OS_UNIX
    if (socklen) {
        strncpy0(x_sock->path,
                 x_orig ? x_orig->path : u.un.sun_path, socklen);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        assert(x_orig  ||  u.sa.sa_family == AF_INET  ||  u.sa.sa_family == AF_INET6);
        if (x_orig) {
            x_sock->addr = x_orig->addr;
            x_sock->port = x_orig->port;
        } else switch (u.sa.sa_family) {
        case AF_INET:
            NcbiIPv4ToIPv6(&x_sock->addr, u.in.sin_addr.s_addr, 0);
            x_sock->port = ntohs(u.in.sin_port);
            break;
        case AF_INET6:
            memcpy(&x_sock->addr, &u.in6.sin6_addr, sizeof(x_sock->addr));
            x_sock->port = ntohs(u.in6.sin6_port);
            break;
        default:
            assert(0);
            break;
        }
        assert(x_sock->port);
    }
    x_sock->myport    = myport;
    x_sock->type      = eSOCK_Socket;
    x_sock->side      = x_orig ? x_orig->side : eSOCK_Server;
    x_sock->log       = flags & (fSOCK_LogDefault | fSOCK_LogOn);
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    x_sock->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn : eDefault;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
    x_sock->pending   = 1/*have to check at the nearest I/O*/;
 #ifdef NCBI_OS_MSWIN
    x_sock->event     = event;
    x_sock->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    x_sock->connected = x_orig ? x_orig->connected            : 0/*false*/;
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    x_sock->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/ : 0/*false*/;
    /* all timeout bits zeroed - infinite */
    x_sock->w_buf     = w_buf;
    if (oldctx  &&  sslctx) {
        NCBI_CRED   cred;
        const char* host;
        if (!oldctx->sess  &&  init) {
            cred = init->cred;
            host = s_VHostOk(init->host) ? init->host : 0;
            assert(!host  ||  *host);
        } else {
            cred = oldctx->cred;
            host = oldctx->host;
            assert(!host  ||  *host);
        }
        x_sock->sslctx = oldctx;
        x_sock->sslctx->sock = x_sock;
        x_orig->sslctx = sslctx;
        x_orig->sslctx->cred = oldctx->cred;
        x_orig->sslctx->host = oldctx->host;
        x_sock->sslctx->cred = cred;
        x_sock->sslctx->host = host ? strdup(host) : 0;
        sslctx = x_sock->sslctx;
    } else if (sslctx) {
        const char* host = init  &&  s_VHostOk(init->host) ? init->host : 0;
        assert(!host  ||  *host);
        x_sock->sslctx = sslctx;
        x_sock->sslctx->sock = x_sock;
        x_sock->sslctx->cred = init ? init->cred   : 0;
        x_sock->sslctx->host = host ? strdup(host) : 0;
    }

    if (sslctx) {
        assert(sslctx == x_sock->sslctx);
        if (!sslctx->sess) {
            FSSLCreate sslcreate = s_SSL ? s_SSL->Create : 0;
            assert(!sslctx->host  ||  *sslctx->host);
            if (sslcreate) {
                assert(sslctx->sock == x_sock);
                sslctx->sess = sslcreate(eSOCK_Client, sslctx, &error);
            } else
                error = 0;
            if (!sslctx->sess) {
                const char* strerr = s_StrError(x_sock, error);
                CORE_LOGF_ERRNO_EXX(132, eLOG_Error,
                                    error, strerr ? strerr : "",
                                    ("%s[SOCK::CreateOnTop] "
                                     " %s to initialize secure session%s%s",
                                     s_ID(x_sock, _id),
                                     sslcreate ? "Failed" : "Unable",
                                     sslctx->host ? " with "     : "",
                                     sslctx->host ? sslctx->host : ""));
                UTIL_ReleaseBuffer(strerr);
                x_sock->sock = SOCK_INVALID;
#ifdef NCBI_OS_MSWIN
                WSAEventSelect(fd, event, 0/*de-associate*/);
                WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
                SOCK_Destroy(x_sock);
                if (oldfd != SOCK_INVALID)
                    SOCK_ABORT(oldfd);
                return eIO_NotSupported;
            }
        } else {
            if (x_sock->log == eOn
                ||  (x_sock->log == eDefault  &&  s_Log == eOn)) {
                CORE_LOGF(eLOG_Trace,
                          ("%sSSL session re-acquired%s%s%s",
                           s_ID(x_sock, _id),
                           sslctx->host ? " \""        : "",
                           sslctx->host ? sslctx->host : "",
                           &"\""[!sslctx->host]));
            }
            x_sock->pending = x_orig->pending;
        }
    }

    if (x_orig) {
        size_t w_len = BUF_Size(x_orig->w_buf) - x_orig->w_len;
        x_sock->r_buf = x_orig->r_buf;
        x_orig->r_buf = 0;
        x_sock->w_buf = x_orig->w_buf;
        x_orig->w_buf = 0;
        x_orig->w_len = 0;
        verify(BUF_Read(x_sock->w_buf, 0, w_len) == w_len);
        if (x_sock->w_buf) {
            verify(BUF_Splice(&x_sock->w_buf, w_buf));
            assert(!BUF_Size(w_buf));
            BUF_Destroy(w_buf);
        } else
            x_sock->w_buf = w_buf;
    } else
        BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    x_sock->w_len = BUF_Size(x_sock->w_buf);

    if (!x_orig  &&  x_sock->port) {
#ifdef SO_KEEPALIVE
        if (!s_SetKeepAlive(fd, x_sock->keepalive)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(153, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(fd, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(138, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

    if ((!x_orig
         ||  ((!x_orig->crossexec  ||  x_orig->sslctx) !=
              (!x_sock->crossexec  ||          sslctx)))
        &&  !s_SetCloexec(fd, !x_sock->crossexec  ||  sslctx)) {
        const char* strerr;
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        error = err;
#else
        error = errno;
        strerr = SOCK_STRERROR(error);
#endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(124, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::CreateOnTop] "
                             " Cannot modify socket close-on-exec mode",
                             s_ID(x_sock, _id)));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    if (!x_orig) {
        struct linger lgr;
        memset(&lgr, 0, sizeof(lgr));
        if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
                       (char*) &lgr, sizeof(lgr)) != 0) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(43, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed setsockopt(SO_NOLINGER)",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
    }

    /* statistics & logging */
    if (x_sock->log == eOn  ||  (x_sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, x_sock, eIO_Open, 0, 0, "");

    /* success */
    *sock = x_sock;
    return eIO_Success;

 errout:
    assert(status != eIO_Success);
    if (sslctx) {
        assert(!oldctx  ||  oldctx != sslctx);
        if (sslctx->host)
            free((void*) sslctx->host);
        free(sslctx);
    }
    BUF_Destroy(w_buf);
    if (x_orig) {
        assert(oldfd != SOCK_INVALID);
        x_orig->sock  = oldfd;
#ifdef NCBI_OS_MSWIN
        x_orig->event = event;
#endif /*NCBI_OS_MSWIN*/
        s_Close(x_orig, 1/*re-close*/, fSOCK_KeepNone);
    }
    return status;
}


static EIO_Status s_CreateListening(const char*    path,
                                    unsigned short port,
                                    unsigned short backlog, 
                                    LSOCK*         lsock,
                                    TSOCK_Flags    flags,
                                    ESwitch        ipv6)
{
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un  un;
#endif /*NCBI_OS_UNIX*/
    } u;
#ifdef NCBI_OS_UNIX
    mode_t          m;
#endif /*NCBI_OS_UNIX*/
    TSOCK_Handle    fd;
    const char*     cp;
    TNCBI_IPv6Addr  addr;
    int             type;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event;
#endif /*NCBI_OS_MSWIN*/
    int             error;
    int             family;
    TSOCK_socklen_t addrlen;
    size_t          pathlen;
    LSOCK           x_lsock;
    char            _id[MAXIDLEN];
    unsigned int    x_id = x_ID_Counter();

    assert(!*lsock);
    assert(!path  ||  *path);

    if (path) {
#ifdef NCBI_OS_UNIX
        pathlen = strlen(path) + 1/*account for end '\0'*/;
        if (sizeof(u.un.sun_path) < pathlen) {
            CORE_LOGF_X(144, eLOG_Error,
                        ("LSOCK#%u[?]@%s: [LSOCK::Create] "
                         " Path too long (%lu vs %lu bytes allowed)",
                         x_id, path, (unsigned long) pathlen,
                         (unsigned long) sizeof(u.un.sun_path)));
            return eIO_InvalidArg;
        }
        family = AF_UNIX;
        addrlen = sizeof(u.un);
        memset(&addr, 0, sizeof(addr));
#else
        return eIO_NotSupported;
#endif /*NCBI_OS_UNIX*/
    } else {
        pathlen = 0/*dummy*/;
        if ((ipv6 == eOn   &&  s_IPVersion == AF_INET)  ||
            (ipv6 == eOff  &&  s_IPVersion == AF_INET6)) {
            return eIO_NotSupported;
        }
        switch (ipv6) {
        case eOn:
            family = AF_INET6;
            break;
        case eOff:
            family = AF_INET;
            break;
        default:
            if ((family = s_IPVersion) == AF_UNSPEC)
                family = AF_INET;
            break;
        }
        switch (family) {
        case AF_INET:
            NcbiIPv4ToIPv6(&addr, flags & fSOCK_BindLocal ? SOCK_LOOPBACK : htonl(INADDR_ANY), 0);
            addrlen = sizeof(u.in);
            break;
        case AF_INET6:
            if (flags & fSOCK_BindLocal)
                SOCK_GetLoopbackAddress6(&addr);
            else
                memset(&addr, 0, sizeof(addr));
            addrlen = sizeof(u.in6);
            break;
        default:
            return eIO_NotSupported;
        }
    }

    /* initialize internals */
    if (s_InitAPI(flags & fSOCK_Secure) != eIO_Success)
        return eIO_NotSupported;

    if (flags & fSOCK_Secure) {
        /*FIXME:  Add secure server support later*/
        return eIO_NotSupported;
    }

    /* create new(listening) socket */
    type  = SOCK_STREAM;
#ifdef SOCK_NONBLOCK
    type |= SOCK_NONBLOCK;
#endif /*SOCK_NONBLOCK*/
#ifdef SOCK_CLOEXEC
    if (!(flags & fSOCK_KeepOnExec))
        type |= SOCK_CLOEXEC;
#endif /*SOCK_CLOEXEC*/
    if ((fd = socket(family, type, 0)) == SOCK_INVALID){
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        cp = path ? path : s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(34, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("LSOCK#%u[?]@%s: [LSOCK::Create] "
                             " Failed socket()", x_id, cp));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }

    if (!path) {
        const char* failed = 0;
#if    defined(NCBI_OS_MSWIN)  &&  defined(SO_EXCLUSIVEADDRUSE)
        /* The use of this option comes with caveats, but it is better
         * to use it rather than to have (or leave) a chance for another
         * process (which uses SO_REUSEADDR, maliciously or not) to be able
         * to bind to the same port number, and snatch incoming connections.
         * Until a connection exists, that originated from the port with this
         * option set, the port (even if the listening instance was closed)
         * cannot be re-bound (important for service restarts!).  See MSDN.
         */
        BOOL excl = TRUE;
        if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       (char*) &excl, sizeof(excl)) != 0) {
            failed = "EXCLUSIVEADDRUSE";
        }
#elif !defined(NCBI_OS_MSWIN)
        /*
         * It was confirmed(?) that at least on Solaris 2.5 this precaution:
         * 1) makes the address(port) released immediately upon the process
         *    termination;
         * 2) still issues EADDRINUSE error on the attempt to bind() to the
         *    same address being in-use by a living process (if SOCK_STREAM).
         * 3) MS-Win treats SO_REUSEADDR completely differently in (as always)
         *    their own twisted way:  it *allows* to bind() to an already
         *    listening socket, which is why we jump the hoops above (also,
         *    note that SO_EXCLUSIVEADDRUSE == ~SO_REUSEADDR on MS-Win).
         */
        if (!s_SetReuseAddress(fd, 1/*true*/))
            failed = "REUSEADDR";
#endif /*NCBI_OS_MSWIN...*/
        if (failed) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            cp = s_CPListening(&addr, port, _id, sizeof(_id));
            CORE_LOGF_ERRNO_EXX(35, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                                 " Failed setsockopt(%s)", x_id,
                                 (unsigned int) fd, cp, failed));
            UTIL_ReleaseBuffer(strerr);
            SOCK_CLOSE(fd);
            return eIO_Unknown;
        }
    }

    /* bind */
    memset(&u, 0, addrlen);
    u.sa.sa_family = family;
#ifdef HAVE_SIN_LEN
    u.sa.sa_len = addrlen;
#endif /*HAVE_SIN_LEN*/
#ifdef NCBI_OS_UNIX
    if (path) {
        assert(family == AF_UNIX);
        memcpy(u.un.sun_path, path, pathlen);
        m = umask(0);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        if (NcbiIsIPv4(&addr)) {
            assert(family == AF_INET);
            u.in.sin_addr.s_addr = NcbiIPv6ToIPv4(&addr, 0);
            u.in.sin_port        = htons(port);
        } else {
            assert(family == AF_INET6);
            u.in6.sin6_port      = htons(port);
            memcpy(&u.in6.sin6_addr, &addr, sizeof(u.in6.sin6_addr));
        }
#ifdef NCBI_OS_UNIX
        m = 0/*dummy*/;
#endif /*NCBI_OS_UNIX*/
    }

    error = bind(fd, &u.sa, addrlen) != 0 ? SOCK_ERRNO : 0;
#ifdef NCBI_OS_UNIX
    if (path)
        umask(m);
#endif /*NCBI_OS_UNIX*/
    if (error) {
        const char* strerr = SOCK_STRERROR(error);
        cp = path ? path : s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(36, error != SOCK_EADDRINUSE
                            ? eLOG_Error : eLOG_Trace,
                            error, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed bind()",
                             x_id, (unsigned int) fd, cp));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
        return error != SOCK_EADDRINUSE ? eIO_Unknown : eIO_Closed;
    }
#ifdef NCBI_OS_UNIX
    if (path) {
#  if !defined(NCBI_OS_BSD)   &&  !defined(NCBI_OS_DARWIN)  &&  \
      !defined(NCBI_OS_IRIX)  &&  !defined(NCBI_OS_CYGWIN)
        (void) fchmod(fd, S_IRWXU | S_IRWXG | S_IRWXO);
#  endif/*!NCBI_OS_BSD && !NCBI_OS_DARWIN && !NCBI_OS_IRIX && !NCBI_OS_CYGWIN*/
#  ifdef NCBI_OS_CYGWIN
        (void) chmod(path, S_IRWXU | S_IRWXG | S_IRWXO);
#  endif /*NCBI_OS_CYGWIN*/
    } else
#endif /*NCBI_OS_UNIX*/
    if (!port) {
        assert(u.sa.sa_family == AF_INET  ||  u.sa.sa_family == AF_INET6);
        error = getsockname(fd, &u.sa, &addrlen) != 0 ? SOCK_ERRNO : 0;
        if (error  ||  u.sa.sa_family != family
            ||  (family == AF_INET   &&  !(port = ntohs(u.in.sin_port)))
            ||  (family == AF_INET6  &&  !(port = ntohs(u.in6.sin6_port)))) {
            const char* strerr = SOCK_STRERROR(error);
            cp = s_CPListening(&addr, port, _id, sizeof(_id));
            CORE_LOGF_ERRNO_EXX(150, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                                 " Cannot obtain free listening port",
                                 x_id, (unsigned int) fd, cp));
            UTIL_ReleaseBuffer(strerr);
            SOCK_CLOSE(fd);
            return eIO_Closed;
        }
    }
    assert((path  &&  !port)  ||
           (port  &&  !path));

#if defined(NCBI_OS_MSWIN)
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        assert(!path);
        cp = s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(118, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed to create IO event",
                             x_id, (unsigned int) fd, cp));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_CLOSE(fd);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(fd, event, FD_CLOSE/*X*/ | FD_ACCEPT/*A*/) != 0) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        assert(!path);
        cp = s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(119, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) fd, cp));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#elif !defined(SOCK_NONBLOCK)
    /* set non-blocking mode */
    if (!s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        cp = path ? path : s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(38, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) fd, cp));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
        return eIO_Unknown;
    }
#endif /*O_NONBLOCK setting*/

    /* listen */
    if (listen(fd, backlog) != 0) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        cp = path ? path : s_CPListening(&addr, port, _id, sizeof(_id));
        CORE_LOGF_ERRNO_EXX(37, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed listen(%hu)",
                             x_id, (unsigned int) fd, cp, backlog));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* allocate memory for the internal socket structure */
    if (!(x_lsock =(LSOCK)calloc(1,sizeof(*x_lsock) + (path?strlen(path):0)))){
        SOCK_CLOSE(fd);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }
    x_lsock->sock     = fd;
    x_lsock->id       = x_id;
    x_lsock->port     = port;
    x_lsock->type     = eSOCK_Listening;
    x_lsock->side     = eSOCK_Server;
    x_lsock->log      = flags & (fSOCK_LogDefault | fSOCK_LogOn);
    x_lsock->keep     = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    x_lsock->i_on_sig = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
#if   defined(NCBI_OS_UNIX)
    if (path)
        strcpy(x_lsock->path, path);
#elif defined(NCBI_OS_MSWIN)
    x_lsock->event    = event;
#endif /*NCBI_OS*/
    x_lsock->addr     = addr;

#ifndef SOCK_CLOEXEC
    if (!(flags & fSOCK_KeepOnExec)  &&  !s_SetCloexec(fd, 1/*true*/)) {
        const char* strerr;
#  ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        error = err;
#  else
        error = errno;
        strerr = SOCK_STRERROR(error);
#  endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(110, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[LSOCK::Create] "
                             " Cannot set socket close-on-exec mode",
                             s_ID((SOCK) lsock, _id)));
#  ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#  else
        UTIL_ReleaseBuffer(strerr);
#  endif /*NCBI_OS_MSWIN*/
    }
#endif /*!SOCK_CLOEXEC*/

    /* statistics & logging */
    if (x_lsock->log == eOn  ||  (x_lsock->log == eDefault && s_Log == eOn)){
        CORE_LOGF_X(115, eLOG_Note,
                    ("%sListening", s_ID((SOCK) x_lsock, _id)));
    }

    *lsock = x_lsock;
    return eIO_Success;
}


static EIO_Status s_Accept(LSOCK           lsock,
                           const STimeout* timeout,
                           SOCK*           sock,
                           TSOCK_Flags     flags)
{
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un  un;
#endif /*NCBI_OS_UNIX*/
    } u;
    TSOCK_Handle    fd;
    unsigned int    x_id;
    TNCBI_IPv6Addr  addr;
    const char*     path;
    unsigned short  port;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event;
#endif /*NCBI_OS_MSWIN*/
    int             error;
    SOCK            x_sock;
    TSOCK_socklen_t addrlen;
    char            _id[MAXIDLEN];

    *sock = 0;

    if (!lsock  ||  lsock->sock == SOCK_INVALID) {
        CORE_LOGF_X(39, eLOG_Error,
                    ("%s[LSOCK::Accept] "
                     " Invalid socket",
                     s_ID((SOCK) lsock, _id)));
        assert(0);
        return eIO_Unknown;
    }

    if (flags & fSOCK_Secure) {
        /* FIXME:  Add secure support later */
        return eIO_NotSupported;
    }

    {{ /* wait for the connection request to come (up to timeout) */
        EIO_Status     status;
        SSOCK_Poll     poll;
        struct timeval tv;

        poll.sock   = (SOCK) lsock;
        poll.event  = eIO_Read;
        poll.revent = eIO_Open;
        status = s_Select(1, &poll, s_to2tv(timeout, &tv), 1/*asis*/);
        assert(poll.event == eIO_Read);
        if (status != eIO_Success)
            return status;
        if (poll.revent == eIO_Close)
            return eIO_Unknown;
        assert(poll.revent == eIO_Read);
    }}

    x_id = (lsock->id * 1000 + x_ID_Counter()) * 1000;

    /* accept next connection */
#ifdef NCBI_OS_UNIX
    if (lsock->path[0]) {
        addrlen = sizeof(u.un);
        assert(!lsock->port);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        addrlen = sizeof(u);
        assert(lsock->port);
#ifdef NCBI_OS_MSWIN
        /* accept() [to follow shortly] resets IO event recording */
        lsock->readable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/
    }
#ifdef HAVE_ACCEPT4
    error = SOCK_NONBLOCK | (flags & fSOCK_KeepOnExec ? SOCK_CLOEXEC : 0);
    fd = accept4(lsock->sock, &u.sa, &addrlen, error/*just flags*/);
#else
    fd = accept (lsock->sock, &u.sa, &addrlen);
#endif /*HAVE_ACCEPT4*/
    if (fd == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(40, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("%s[LSOCK::Accept] "
                             " Failed accept()",
                             s_ID((SOCK) lsock, _id)));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
    lsock->n_accept++;

#ifdef NCBI_OS_UNIX
    if (lsock->path[0]) {
        assert(u.un.sun_family == AF_UNIX);
        memset(&addr, 0, sizeof(addr));
        path = lsock->path;
        port = 0;
    } else
#endif /*NCBI_OS_UNIX*/
    {
        switch (u.sa.sa_family) {
        case AF_INET:
            NcbiIPv4ToIPv6(&addr, u.in.sin_addr.s_addr, 0);
            port = ntohs(u.in.sin_port);
            break;
        case AF_INET6:
            memcpy(&addr, &u.in6.sin6_addr, sizeof(addr));
            port = ntohs(u.in6.sin6_port);
            break;
        default:
            port = 0;
            assert(0);
            break;
        }
        assert(port);
        if (s_ApproveHook) {
            EIO_Status status = s_ApproveCallback(0, &addr, port,
                                                  eSOCK_Server, eSOCK_Socket,
                                                  (SOCK) lsock);
            if (status != eIO_Success) {
                SOCK_ABORT(fd);
                return status;
            }
        }
        path = "";
    }

#if defined(NCBI_OS_MSWIN)
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(120, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Failed to create IO event",
                             x_id, (unsigned int) fd,
                             s_CP(&addr, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_ABORT(fd);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(fd, event, SOCK_EVENTS) != 0) {
        int error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(error);
        CORE_LOGF_ERRNO_EXX(121, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) fd,
                             s_CP(&addr, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBuffer(strerr);
        SOCK_ABORT(fd);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#elif !defined(HAVE_ACCEPT4)
    /* man accept(2) notes that non-blocking state may not be inherited */
    if (!s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(41, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) fd,
                             s_CP(&addr, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBuffer(strerr);
        SOCK_ABORT(fd);
        return eIO_Unknown;
    }
#endif /*O_NONBLOCK setting*/

    /* create new SOCK structure */
    addrlen = (TSOCK_socklen_t)(path ? strlen(path) : 0);
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + addrlen))) {
        SOCK_ABORT(fd);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* success */
#ifdef NCBI_OS_UNIX
    if (!port) {
        assert(!lsock->port  &&  path[0]);
        strcpy(x_sock->path, path);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        assert(!path[0]);
        x_sock->addr  = addr;
        x_sock->port  = port;
    }
    x_sock->myport    = lsock->port;
    x_sock->sock      = fd;
    x_sock->id        = x_id;
    x_sock->type      = eSOCK_Socket;
    x_sock->side      = eSOCK_Server;
    x_sock->log       = flags & (fSOCK_LogDefault | fSOCK_LogOn);
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    x_sock->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn : eDefault;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
#ifdef NCBI_OS_MSWIN
    x_sock->event     = event;
    x_sock->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    x_sock->connected = 1/*true*/;
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    x_sock->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/ : 0/*false*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    /* w_buf is unused for accepted sockets */

    if (port) {
        if (s_ReuseAddress == eOn  &&  !s_SetReuseAddress(fd, 1)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(42, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(REUSEADDR)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#ifdef SO_KEEPALIVE
        if (x_sock->keepalive  &&  !s_SetKeepAlive(fd, 1)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(152, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(fd, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(137, eLOG_Warning,
                                error, strerr ? strerr : "",
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

#ifndef HAVE_ACCEPT4
    if (!x_sock->crossexec  &&  !s_SetCloexec(fd, 1/*true*/)) {
        const char* strerr;
#  ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        error = err;
#  else
        error = errno;
        strerr = SOCK_STRERROR(error);
#  endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(128, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[LSOCK::Accept] "
                             " Cannot set socket close-on-exec mode",
                             s_ID(*sock, _id)));
#  ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#  else
        UTIL_ReleaseBuffer(strerr);
#  endif /*NCBI_OS_MSWIN*/
    }
#endif /*!HAVE_ACCEPT4*/

    /* statistics & logging */
    if (x_sock->log == eOn  ||  (x_sock->log == eDefault  &&  (s_Log == eOn  ||  lsock->log == eOn)))
        s_DoLog(eLOG_Note, x_sock, eIO_Open, 0, 0, 0);

    *sock = x_sock;
    return eIO_Success;
}


static EIO_Status s_CloseListening(LSOCK lsock)
{
    char _id[MAXIDLEN];
    int        error;
    EIO_Status status;

    assert(lsock->sock != SOCK_INVALID);

#if   defined(NCBI_OS_UNIX)
    if (!lsock->keep  &&  lsock->path[0]) {
        assert(!lsock->port);
        remove(lsock->path);
    }
#elif defined(NCBI_OS_MSWIN)
    assert(lsock->event);
    WSAEventSelect(lsock->sock, lsock->event/*ignored*/, 0/*de-associate*/);
#endif /*NCBI_OS*/

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn)) {
        CORE_LOGF_X(44, eLOG_Note,
                    ("%s%s (%u accept%s total)",
                     s_ID((SOCK) lsock, _id),
                     lsock->keep ? "Leaving" : "Closing",
                     lsock->n_accept, lsock->n_accept == 1 ? "" : "s"));
    }

    status = eIO_Success;
    if (!lsock->keep) {
        TSOCK_Handle fd = lsock->sock;
        for (;;) { /* close persistently - retry if interrupted */
            if (SOCK_CLOSE(fd) == 0)
                break;

            /* error */
            if (s_Initialized <= 0)
                break;
            error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
            if (error == WSANOTINITIALISED) {
                s_Initialized = -1/*deinited*/;
                break;
            }
#endif /*NCBI_OS_MSWIN*/
            if (error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
                CORE_LOGF_ERRNO_EXX(45, eLOG_Error,
                                    error, strerr ? strerr : "",
                                    ("%s[LSOCK::Close] "
                                     " Failed close()",
                                     s_ID((SOCK) lsock, _id)));
                UTIL_ReleaseBuffer(strerr);
                status = eIO_Unknown;
                break;
            }
        }
        lsock->sock = SOCK_INVALID;
#ifdef NCBI_OS_MSWIN
        WSASetEvent(lsock->event); /*signal closure*/
#endif /*NCBI_OS_MSWIN*/
    } else
        lsock->sock = SOCK_INVALID;

    /* cleanup & return */
#ifdef NCBI_OS_MSWIN
    WSACloseEvent(lsock->event);
    lsock->event = 0;
#endif /*NCBI_OS_MSWIN*/
    return status;
}


static EIO_Status s_RecvMsg(SOCK            sock,
                            void*           buf,
                            size_t          bufsize,
                            size_t          msgsize,
                            size_t*         msglen,
                            TNCBI_IPv6Addr* sender_addr,
                            unsigned short* sender_port)
{
    size_t     x_msgsize;
    char       w[1536];
    EIO_Status status;
    void*      x_msg;

    BUF_Erase(sock->r_buf);
    sock->r_len = 0;

    x_msgsize = (msgsize  &&  msgsize < ((1 << 16) - 1))
        ? msgsize : ((1 << 16) - 1);

    if (!(x_msg = (x_msgsize <= bufsize
                   ? buf : (x_msgsize <= sizeof(w)
                            ? w : malloc(x_msgsize))))) {
        sock->r_status = eIO_Unknown;
        return eIO_Unknown;
    }
    sock->r_status = eIO_Success;

    for (;;) { /* auto-resume if either blocked or interrupted (optional) */
        ssize_t x_read;
        int     error;
        union {
            struct sockaddr     sa;
            struct sockaddr_in  in;
            struct sockaddr_in6 in6;
        } u;
        TSOCK_socklen_t addrlen = sizeof(u);
        memset(&u, 0, sizeof(u.sa));
#ifdef HAVE_SIN_LEN
        u.sa.sa_len = addrlen;
#endif /*HAVE_SIN_LEN*/
        x_read = recvfrom(sock->sock, x_msg, WIN_INT_CAST x_msgsize, 0/*flags*/,
                          &u.sa, &addrlen);
#ifdef NCBI_OS_MSWIN
        /* recvfrom() resets IO event recording */
        sock->readable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/

        if (x_read >= 0) {
            /* got a message */
            TNCBI_IPv6Addr addr;
            unsigned short port;
            assert(sock->r_status == eIO_Success);
            sock->r_len = (TNCBI_BigCount) x_read;
            if ( msglen )
                *msglen = (size_t) x_read;
            switch (u.sa.sa_family) {
            case AF_INET:
                NcbiIPv4ToIPv6(&addr, u.in.sin_addr.s_addr, 0);
                port = ntohs(u.in.sin_port);
                break;
            case AF_INET6:
                memcpy(&addr, &u.in6.sin6_addr, sizeof(addr));
                port = ntohs(u.in6.sin6_port);
                break;
            default:
                memset(&addr, 0, sizeof(addr));
                port = 0;
                assert(0);
                break;
            }
            if ( sender_addr )
                *sender_addr = addr;
            if ( sender_port )
                *sender_port = port;
            if (s_ApproveHook) {
                status = s_ApproveCallback(0, &addr, port,
                                           eSOCK_Server, eSOCK_Datagram, sock);
                if (status != eIO_Success)
                    break;
            }
            if ((size_t) x_read > bufsize
                &&  !BUF_Write(&sock->r_buf,
                               (char*) x_msg  + bufsize,
                               (size_t)x_read - bufsize)) {
                CORE_LOGF_X(20, eLOG_Critical,
                            ("%s[DSOCK::RecvMsg] "
                             " Message truncated: %lu/%lu",
                             s_ID(sock, w),
                             (unsigned long) bufsize, (unsigned long) x_read));
                status = eIO_Unknown;
            } else
                status = eIO_Success;
            if (bufsize  &&  bufsize < x_msgsize) {
                memcpy(buf, x_msg,
                       (size_t) x_read < bufsize ? (size_t) x_read : bufsize);
            }

            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(eLOG_Note, sock, eIO_Read, x_msg, (size_t) x_read, &u.sa);

            sock->n_read += (TNCBI_BigCount) x_read;
            sock->n_in++;
            break;
        }

        error = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        if (error == SOCK_EWOULDBLOCK  ||  error == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, SOCK_GET_TIMEOUT(sock, r), 1/*asis*/);
            assert(poll.event == eIO_Read);
            if (status == eIO_Timeout) {
                sock->r_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (status != eIO_Success)
                break;
            if (poll.revent != eIO_Close) {
                assert(poll.revent == eIO_Read);
                continue/*read again*/;
            }
        } else if (error == SOCK_EINTR) {
            if (x_IsInterruptibleSOCK(sock)) {
                sock->r_status = status = eIO_Interrupt;
                break/*interrupt*/;
            }
            continue;
        } else {
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOGF_ERRNO_EXX(94, eLOG_Trace,
                                error, strerr ? strerr : "",
                                ("%s[DSOCK::RecvMsg] "
                                 " Failed recvfrom()",
                                 s_ID(sock, w)));
            UTIL_ReleaseBuffer(strerr);
        }
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->r_status = status = eIO_Unknown;
        break/*unknown*/;
    }

    if (x_msgsize > bufsize  &&  x_msg != w)
        free(x_msg);
    return status;
}


static EIO_Status s_SendMsg(SOCK           sock,
                            const char*    host,
                            unsigned short port,
                            const void*    data,
                            size_t         datalen)
{
    size_t          x_msgsize;
    TSOCK_socklen_t addrlen;
    char            w[1536];
    EIO_Status      status;
    void*           x_msg;
    TNCBI_IPv6Addr  addr;
    int/*bool*/     ipv4;
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } u;

    if (datalen) {
        status = s_Write_(sock, data, datalen, &x_msgsize, 1/*SendMsg*/);
        if (status != eIO_Success)
            return status;
        assert(x_msgsize == datalen);
        assert(sock->w_len == 0);
    } else
        sock->w_len = 0;
    sock->w_status = eIO_Success;
    sock->eof = 1/*true - finalized message*/;

    assert(!host  ||  *host);
    if (!host)
        addr = sock->addr;
    else if (!s_gethostbyname(&addr, host, s_IPVersion, 0, (ESwitch) sock->log)) {
        CORE_LOGF_X(88, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Failed SOCK_gethostbyname(\"%.*s\")",
                     s_ID(sock, w), CONN_HOST_LEN, host));
        return eIO_Unknown;
    }
    if (!port)
        port = sock->port;

    x_msgsize = !NcbiIsEmptyIPv6(&addr);
    if (!x_msgsize  ||  !port) {
        s_HostPortToString(&addr, port, w, sizeof(w)/2);
        CORE_LOGF_X(89, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Address \"%s\" incomplete, missing %s",
                     s_ID(sock, w + sizeof(w)/2), w,
                     port ? "host" : &"host:port"[x_msgsize ? 5 : 0]));
         return eIO_Unknown;
    }
    if (s_ApproveHook) {
        TNCBI_IPv6Addr temp;
        const char* parsed = host ? NcbiStringToAddr(&temp, host, 0) : 0;
        status = s_ApproveCallback(parsed  &&  !*parsed ? 0 : host,
                                   &addr, port,
                                   eSOCK_Client, eSOCK_Datagram, sock);
        if (status != eIO_Success)
            return status;
    }

    if ((x_msgsize = BUF_Size(sock->w_buf)) != 0) {
        if (x_msgsize <= sizeof(w))
            x_msg = w;
        else if (!(x_msg = malloc(x_msgsize)))
            return eIO_Unknown;
        verify(BUF_Peek(sock->w_buf, x_msg, x_msgsize) == x_msgsize);
    } else
        x_msg = 0;

    ipv4 = NcbiIsIPv4(&addr);
    addrlen = ipv4 ? sizeof(u.in) : sizeof(u.in6);
    memset(&u, 0, addrlen);
    u.sa.sa_family           = ipv4 ? AF_INET : AF_INET6;
#ifdef HAVE_SIN_LEN
    u.sa.sa_len              = addrlen;
#endif /*HAVE_SIN_LEN*/
    if (ipv4) {
        u.in.sin_addr.s_addr = NcbiIPv6ToIPv4(&addr, 0);
        u.in.sin_port        = htons(port);
    } else {
        memcpy(&u.in6.sin6_addr, &addr, sizeof(u.in6.sin6_addr));
        u.in6.sin6_port      = htons(port);
    }

    for (;;) { /* optionally auto-resume if interrupted */
        int error;
        ssize_t x_written;

        if ((x_written = sendto(sock->sock, x_msg, WIN_INT_CAST x_msgsize, 0/*flags*/,
                                &u.sa, addrlen)) >= 0) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(eLOG_Note, sock, eIO_Write, x_msg, (size_t) x_written, &u.sa);

            sock->w_len      = (TNCBI_BigCount) x_written;
            sock->n_written += (TNCBI_BigCount) x_written;
            sock->n_out++;
            if ((size_t) x_written != x_msgsize) {
                sock->w_status = status = eIO_Closed;
                if (!NcbiIsEmptyIPv6(&addr)  ||  port)
                    s_HostPortToString(&addr, port, w, sizeof(w)/2);
                else
                    w[0] = '\0';
                CORE_LOGF_X(90, eLOG_Error,
                            ("%s[DSOCK::SendMsg] "
                             " Partial datagram sent (%lu out of %lu)%s%s",
                             s_ID(sock, w + sizeof(w)/2),
                             (unsigned long) x_written,
                             (unsigned long) x_msgsize, *w ? " to " : "", w));
                break/*closed*/;
            }
            assert(sock->w_status == eIO_Success);
            status = eIO_Success;
            break/*success*/;
        }

#ifdef NCBI_OS_MSWIN
        /* special sendto()'s semantics of IO recording reset */
        sock->writable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/

        error = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        if (error == SOCK_EWOULDBLOCK  ||  error == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, SOCK_GET_TIMEOUT(sock, w), 1/*asis*/);
            assert(poll.event == eIO_Write);
            if (status == eIO_Timeout) {
                sock->w_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (status != eIO_Success)
                break;
            if (poll.revent != eIO_Close) {
                assert(poll.revent == eIO_Write);
                continue/*send again*/;
            }
        } else if (error == SOCK_EINTR) {
            if (x_IsInterruptibleSOCK(sock)) {
                sock->w_status = status = eIO_Interrupt;
                break/*interrupt*/;
            }
            continue;
        } else {
            const char* strerr = SOCK_STRERROR(error);
            if (!NcbiIsEmptyIPv6(&addr)  ||  port)
                s_HostPortToString(&addr, port, w, sizeof(w)/2);
            else
                w[0] = '\0';
            CORE_LOGF_ERRNO_EXX(91, eLOG_Trace,
                                error, strerr ? strerr : "",
                                ("%s[DSOCK::SendMsg] "
                                 " Failed sendto(%s)",
                                 s_ID(sock, w + sizeof(w)/2), w));
            UTIL_ReleaseBuffer(strerr);
        }
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = status = eIO_Unknown;
        break/*unknown*/;
    }

    if (x_msg  &&  x_msg != w)
        free(x_msg);
    if (status == eIO_Success)
        BUF_Erase(sock->w_buf);
    return status;
}


static EIO_Status x_TriggerRead(const TRIGGER trigger,
                                int/*bool*/   isset)
{
#ifndef NCBI_CXX_TOOLKIT

    (void) trigger;
    return eIO_NotSupported;

#elif defined(NCBI_OS_UNIX)

#  ifdef PIPE_SIZE
#    define MAX_TRIGGER_BUF  PIPE_SIZE
#  else
#    define MAX_TRIGGER_BUF  8192
#  endif /*PIPE_SIZE*/

    EIO_Status  status = eIO_Unknown;
    for (;;) {
        static char x_buf[MAX_TRIGGER_BUF];
        ssize_t     x_read = read(trigger->fd, x_buf, sizeof(x_buf));
        if (x_read == 0/*EOF?*/)
            break;
        if (x_read < 0) {
            int error;
            if (status == eIO_Success)
                break;
            if ((error = errno) == EAGAIN  ||  error == EWOULDBLOCK)
                status  = eIO_Timeout;
            break;
        }
        status = eIO_Success;
    }
    return status;

#elif defined(NCBI_OS_MSWIN)

    switch (WaitForSingleObject(trigger->fd, 0)) {
    case WAIT_OBJECT_0:
        return eIO_Success;
    case WAIT_TIMEOUT:
        return eIO_Timeout;
    default:
        break;
    }
    return eIO_Unknown;

#endif /*NCBI_OS*/
}



/******************************************************************************
 *  TRIGGER
 */


extern EIO_Status TRIGGER_Create(TRIGGER* trigger,
                                 ESwitch  log)
{
    unsigned int x_id = x_ID_Counter();

    if (!trigger)
        return eIO_InvalidArg;

    *trigger = 0;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return eIO_NotSupported;

#ifndef NCBI_CXX_TOOLKIT

    return eIO_NotSupported;

#elif defined(NCBI_OS_UNIX)
    {{
        int err;
        int fd[3];

#  ifdef HAVE_PIPE2
        err = pipe2(fd, O_NONBLOCK | O_CLOEXEC);
#  else
        err = pipe (fd);
#  endif /*HAVE_PIPE2*/
        if (err != 0) {
            CORE_LOGF_ERRNO_X(28, eLOG_Error, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Cannot create pipe", x_id));
            return eIO_Closed;
        }

#  ifdef FD_SETSIZE
        /* We don't need "out" to be selectable, so move it out of the way to
           spare precious "selectable" file descriptor numbers */
#    ifdef F_DUPFD_CLOEXEC
        fd[2] = fcntl(fd[1], F_DUPFD_CLOEXEC, FD_SETSIZE);
#    else
        /* NB: dup() does not inherit the CLOEXEC flag */
        fd[2] = fcntl(fd[1], F_DUPFD,         FD_SETSIZE);
#    endif /*F_DUPFD_CLOEXEC*/
        if (fd[2] == -1) {
            err = errno;
#    if   defined(RLIMIT_NOFILE)
            struct rlimit rl;
            if (getrlimit(RLIMIT_NOFILE, &rl) == 0
                &&  rl.rlim_cur <= FD_SETSIZE) {
                err = 0/*no error*/;
            }
#    elif defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DARWIN)
            if (getdtablesize() <= FD_SETSIZE)
                err = 0/*no error*/;
#    endif /*file limit*/
            if (err) {
                CORE_LOGF_ERRNO_X(143, eLOG_Warning, err,
                                  ("TRIGGER#%u[?]: [TRIGGER::Create] "
                                   " Failed to dup(%d) to higher fd(%d+)",
                                   x_id, fd[1], FD_SETSIZE));
                err = 0;
            }
        } else {
            assert(!err);
            close(fd[1]);
            fd[1] = fd[2];
#    ifdef F_DUPFD_CLOEXEC
            assert((fd[2]=fcntl(fd[1],F_GETFD,0))!=-1 && (fd[2] & FD_CLOEXEC));
#    else
            if (!s_SetCloexec(fd[1], 1/*true*/)) {
                CORE_LOGF_ERRNO_X(30, eLOG_Warning, errno,
                                  ("TRIGGER#%u[?]: [TRIGGER::Create] "
                                   " Failed to set close-on-exec", x_id));
                err = -1;
            }
#    endif /*F_DUPFD_CLOEXEC*/
        }
#  endif /*FD_SETSIZE*/

#  ifndef HAVE_PIPE2
        if (!s_SetNonblock(fd[0], 1/*true*/)  ||
            !s_SetNonblock(fd[1], 1/*true*/)) {
            CORE_LOGF_ERRNO_X(29, eLOG_Error, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Failed to set non-blocking mode", x_id));
            close(fd[0]);
            close(fd[1]);
            return eIO_Unknown;
        }

        if (!s_SetCloexec(fd[0], 1/*true*/)  &&  err != -1)
            err = errno;
#    ifndef FD_SETSIZE
        if (!s_SetCloexec(fd[1], 1/*true*/))
            err = errno;
#    endif /*!FD_SETSIZE*/
#  endif /*!HAVE_PIPE2*/

        if (err  &&  err != -1) {
            CORE_LOGF_ERRNO_X(30, eLOG_Warning, err,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Failed to set close-on-exec", x_id));
        }

        if (!(*trigger = (TRIGGER) calloc(1, sizeof(**trigger)))) {
            close(fd[0]);
            close(fd[1]);
            return eIO_Unknown;
        }
        (*trigger)->fd       = fd[0];
        (*trigger)->id       = x_id;
        (*trigger)->out      = fd[1];
        (*trigger)->type     = eSOCK_Trigger;
        (*trigger)->log      = log;
        (*trigger)->i_on_sig = eDefault;

        /* statistics & logging */
        if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
            CORE_LOGF_X(116, eLOG_Note,
                        ("TRIGGER#%u[%u, %u]: Ready", x_id, fd[0], fd[1]));
        }
    }}
    return eIO_Success;

#elif defined(NCBI_OS_MSWIN)
    {{
        HANDLE event = WSACreateEvent();
        if (!event) {
            DWORD err = GetLastError();
            const char* strerr = s_WinStrerror(err);
            CORE_LOGF_ERRNO_EXX(14, eLOG_Error,
                                err, strerr ? strerr : "",
                                ("TRIGGER#%u: [TRIGGER::Create] "
                                 " Cannot create event object", x_id));
            UTIL_ReleaseBufferOnHeap(strerr);
            return eIO_Closed;
        }
        if (!(*trigger = (TRIGGER) calloc(1, sizeof(**trigger)))) {
            WSACloseEvent(event);
            return eIO_Unknown;
        }
        (*trigger)->fd       = event;
        (*trigger)->id       = x_id;
        (*trigger)->type     = eSOCK_Trigger;
        (*trigger)->log      = log;
        (*trigger)->i_on_sig = eDefault;

        /* statistics & logging */
        if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
            CORE_LOGF_X(116, eLOG_Note,
                        ("TRIGGER#%u: Ready", x_id));
        }
    }}
    return eIO_Success;

#else
#  error "Unsupported platform"
#endif /*NCBI_OS*/
}


extern EIO_Status TRIGGER_Close(TRIGGER trigger)
{
    if (!trigger)
        return eIO_InvalidArg;

#ifndef NCBI_CXX_TOOLKIT

    return eIO_NotSupported;

#else

    /* statistics & logging */
    if (trigger->log == eOn  ||  (trigger->log == eDefault  &&  s_Log == eOn)){
        CORE_LOGF_X(117, eLOG_Note,
                    ("TRIGGER#%u[%u]: Closing", trigger->id, trigger->fd));
    }

#  if   defined(NCBI_OS_UNIX)

    /* Avoid SIGPIPE by closing in this order:  writing end first */
    close(trigger->out);
    close(trigger->fd);

#  elif defined(NCBI_OS_MSWIN)

    WSACloseEvent(trigger->fd);

#  endif /*NCBI_OS*/

    free(trigger);
    return eIO_Success;

#endif /*!NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Set(TRIGGER trigger)
{
    if (!trigger)
        return eIO_InvalidArg;

#ifndef NCBI_CXX_TOOLKIT

    return eIO_NotSupported;

#elif defined(NCBI_OS_UNIX)

    if (CORE_Once(&trigger->isset.ptr)) {
        if (write(trigger->out, "", 1) < 0  &&  errno != EAGAIN)
            return eIO_Unknown;
    }

    return eIO_Success;

#elif defined(NCBI_OS_MSWIN)

    return WSASetEvent(trigger->fd) ? eIO_Success : eIO_Unknown;

#endif /*NCBI_OS*/
}


extern EIO_Status TRIGGER_IsSet(TRIGGER trigger)
{
    EIO_Status status;

    if (!trigger)
        return eIO_InvalidArg;

    status = x_TriggerRead(trigger, 1/*IsSet*/);

#ifdef NCBI_OS_UNIX

    if (status == eIO_Success)
        trigger->isset.ptr = (void*) 1/*true*/;
    else if (status != eIO_Timeout)
        return status;
    return trigger->isset.ptr ? eIO_Success : eIO_Closed;

#else

    return status == eIO_Timeout ? eIO_Closed : status;

#endif /*NCBI_OS_UNIX*/
}


extern EIO_Status TRIGGER_Reset(TRIGGER trigger)
{
    EIO_Status status;

    if (!trigger)
        return eIO_InvalidArg;

    status = x_TriggerRead(trigger, 0/*Reset*/);

#if   defined(NCBI_OS_UNIX)

    trigger->isset.ptr = (void*) 0/*false*/;

#elif defined(NCBI_OS_MSWIN)

    if (!WSAResetEvent(trigger->fd))
        return eIO_Unknown;

#endif /*NCBI_OS*/

    return status == eIO_Timeout ? eIO_Success : status;
}



/******************************************************************************
 *  LISTENING SOCKET
 */


extern EIO_Status LSOCK_Create(unsigned short port,
                               unsigned short backlog,
                               LSOCK*         lsock)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock,
                             fSOCK_LogDefault, eDefault);
}


extern EIO_Status LSOCK_CreateEx(unsigned short port,
                                 unsigned short backlog,
                                 LSOCK*         lsock,
                                 TSOCK_Flags    flags)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock,
                             flags, eDefault);
}


extern EIO_Status LSOCK_Create6(unsigned short port,
                                unsigned short backlog,
                                LSOCK*         lsock,
                                ESwitch        ipv6)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock,
                             fSOCK_LogDefault, ipv6);
}


extern EIO_Status LSOCK_CreateEx6(unsigned short port,
                                  unsigned short backlog,
                                  LSOCK*         lsock,
                                  TSOCK_Flags    flags,
                                  ESwitch        ipv6)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock,
                             flags, ipv6);
}


extern EIO_Status LSOCK_CreateUNIX(const char*    path,
                                   unsigned short backlog,
                                   LSOCK*         lsock,
                                   TSOCK_Flags    flags)
{
    *lsock = 0;
    if (!path  ||  !*path)
        return eIO_InvalidArg;
    return s_CreateListening(path, 0, backlog, lsock,
                             flags, eDefault/*dontcare*/);
}


extern EIO_Status LSOCK_Accept(LSOCK           lsock,
                               const STimeout* timeout,
                               SOCK*           sock)
{
    return s_Accept(lsock, timeout, sock, fSOCK_LogDefault);
}


extern EIO_Status LSOCK_AcceptEx(LSOCK           lsock,
                                 const STimeout* timeout,
                                 SOCK*           sock,
                                 TSOCK_Flags     flags)
{
    return s_Accept(lsock, timeout, sock, flags);
}


extern EIO_Status LSOCK_Close(LSOCK lsock)
{
    EIO_Status status;

    if (lsock) {
        status = (lsock->sock != SOCK_INVALID
                  ? s_CloseListening(lsock)
                  : eIO_Closed);
        free(lsock);
    } else
        status = eIO_InvalidArg;
    return status;
}


extern EIO_Status LSOCK_GetOSHandleEx(LSOCK      lsock,
                                      void*      handle,
                                      size_t     handle_size,
                                      EOwnership ownership)
{
    TSOCK_Handle fd;
    EIO_Status   status;

    if (!handle  ||  handle_size != sizeof(lsock->sock)) {
        CORE_LOGF_X(46, eLOG_Error,
                    ("LSOCK#%u[%u]: [LSOCK::GetOSHandle] "
                     " Invalid handle%s %lu",
                     lsock->id, (unsigned int) lsock->sock,
                     handle ? " size"                     : "",
                     handle ? (unsigned long) handle_size : 0UL));
        assert(0);
        return eIO_InvalidArg;
    }
    if (!lsock) {
        fd = SOCK_INVALID;
        memcpy(handle, &fd, handle_size);
        return eIO_InvalidArg;
    }
    fd = lsock->sock;
    memcpy(handle, &fd, handle_size);
    if (s_Initialized <= 0  ||  fd == SOCK_INVALID)
        status = eIO_Closed;
    else if (ownership != eTakeOwnership)
        status = eIO_Success;
    else {
        lsock->keep = 1/*true*/;
        status = s_CloseListening(lsock);
        assert(lsock->sock == SOCK_INVALID);
    }
    return status;
}


extern EIO_Status LSOCK_GetOSHandle(LSOCK  lsock,
                                    void*  handle,
                                    size_t handle_size)
{
    return LSOCK_GetOSHandleEx(lsock, handle, handle_size, eNoOwnership);
}


extern unsigned short LSOCK_GetPort(LSOCK         lsock,
                                    ENH_ByteOrder byte_order)
{
    unsigned short port;
    port = lsock  &&  lsock->sock != SOCK_INVALID ? lsock->port : 0;
    return byte_order == eNH_HostByteOrder ? port : htons(port);
}


extern void LSOCK_GetListeningAddress(LSOCK           lsock,
                                      unsigned int*   host,
                                      unsigned short* port,
                                      ENH_ByteOrder   byte_order)
{
    SOCK_GetPeerAddress((SOCK) lsock, host, port, byte_order);
}


extern void LSOCK_GetListeningAddress6(LSOCK           lsock,
                                       TNCBI_IPv6Addr* addr,
                                       unsigned short* port,
                                       ENH_ByteOrder   byte_order)
{
    SOCK_GetPeerAddress6((SOCK) lsock, addr, port, byte_order);
}


extern char* LSOCK_GetListeningAddressStringEx(LSOCK               lsock,
                                               char*               buf,
                                               size_t              bufsize,
                                               ESOCK_AddressFormat format)
{
    return SOCK_GetPeerAddressStringEx((SOCK) lsock, buf, bufsize, format);
}


extern char* LSOCK_GetListeningAddressString(LSOCK lsock, char* buf, size_t bufsize)
{
    return LSOCK_GetListeningAddressStringEx(lsock, buf, bufsize, eSAF_Full);
}



/******************************************************************************
 *  SOCKET
 */


EIO_Status SOCK_CreateInternal(const char*       host,
                               unsigned short    port,
                               const STimeout*   timeout,
                               SOCK*             sock,
                               const SSOCK_Init* init,
                               TSOCK_Flags       flags)
{
    EIO_Status status;
    *sock = 0;
    if (!host  ||  !port)
        return eIO_InvalidArg;
    status = s_Create(host, port, timeout, sock, init, flags);
    assert(!*sock == !(status == eIO_Success));
    return status;
}


extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port, 
                              const STimeout* timeout,
                              SOCK*           sock)
{
    return SOCK_CreateInternal(host, port, timeout, sock, 0, fSOCK_LogDefault);
}


extern EIO_Status SOCK_CreateEx(const char*     host,
                                unsigned short  port,
                                const STimeout* timeout,
                                SOCK*           sock,
                                const void*     data,
                                size_t          size,
                                TSOCK_Flags     flags)
{
    SSOCK_Init init;
    memset(&init, 0, sizeof(init));
    init.data = data;
    init.size = size;
    return SOCK_CreateInternal(host, port, timeout, sock, &init, flags);
}


extern EIO_Status SOCK_CreateUNIX(const char*     path,
                                  const STimeout* timeout,
                                  SOCK*           sock,
                                  const void*     data,
                                  size_t          size,
                                  TSOCK_Flags     flags)
{
    EIO_Status status;
    *sock = 0;
    if (!path  ||  !*path)
        return eIO_InvalidArg;
#ifndef NCBI_OS_UNIX
    status = eIO_NotSupported;
#else
    {{
        SSOCK_Init init;
        memset(&init, 0, sizeof(init));
        init.data = data;
        init.size = size;
        status = s_Create(path, 0, timeout, sock, &init, flags);
        assert(!*sock == !(status == eIO_Success));
    }}
#endif /*!NCBI_OS_UNIX*/
    return status;
}


EIO_Status SOCK_CreateOnTopInternal(const void*       handle,
                                    size_t            handle_size,
                                    SOCK*             sock,
                                    const SSOCK_Init* init,
                                    TSOCK_Flags       flags)
{
    EIO_Status status;
    *sock = 0;
    status = s_CreateOnTop(handle, handle_size, sock, init, flags);
    assert(!*sock == !(status == eIO_Success));
    return status;
}


extern EIO_Status SOCK_CreateOnTop(const void* handle,
                                   size_t      handle_size,
                                   SOCK*       sock)
{
    return SOCK_CreateOnTopInternal(handle, handle_size, sock,
                                    0, fSOCK_LogDefault);
}


extern EIO_Status SOCK_CreateOnTopEx(const void* handle,
                                     size_t      handle_size,
                                     SOCK*       sock,
                                     const void* data,
                                     size_t      size,
                                     TSOCK_Flags flags)
{
    SSOCK_Init init;
    memset(&init, 0, sizeof(init));
    init.data = data;
    init.size = size;
    return SOCK_CreateOnTopInternal(handle, handle_size, sock,
                                    &init, flags);
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    char _id[MAXIDLEN];

    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF_X(52, eLOG_Error,
                    ("%s[SOCK::Reconnect] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

#ifdef NCBI_OS_UNIX
    if (sock->path[0]  &&  (host  ||  port)) {
        CORE_LOGF_X(53, eLOG_Error,
                    ("%s[SOCK::Reconnect] "
                     " Unable to reconnect UNIX socket as INET at \"%s:%hu\"",
                     s_ID(sock, _id), host ? host : "", port));
        assert(0);
        return eIO_InvalidArg;
    }
#endif /*NCBI_OS_UNIX*/

    /* special treatment for server-side socket */
    if (sock->side == eSOCK_Server  &&  (!host  ||  !port)) {
        CORE_LOGF_X(51, eLOG_Error,
                    ("%s[SOCK::Reconnect] "
                     " Attempt to reconnect server-side socket as"
                     " client one to its peer address",
                     s_ID(sock, _id)));
        return eIO_InvalidArg;
    }

    /* close the socket if necessary */
    if (sock->sock != SOCK_INVALID) {
        s_Close(sock, 0, fSOCK_KeepNone);
        /* likely NOOP */
        BUF_Erase(sock->r_buf);
        BUF_Erase(sock->w_buf);
    }

    /* connect */
    sock->id++;
    sock->side      = eSOCK_Client;
    sock->n_read    = 0;
    sock->n_written = 0;
    if ((host  ||  port)  &&  sock->sslctx  &&  sock->sslctx->host) {
        free((void*) sock->sslctx->host);
        sock->sslctx->host = 0;
    }
    return s_Connect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Shutdown(SOCK      sock,
                                EIO_Event dir)
{
    char       _id[MAXIDLEN];
    EIO_Status status;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(54, eLOG_Error,
                    ("%s[SOCK::Shutdown] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF_X(55, eLOG_Error,
                    ("%s[SOCK::Shutdown] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (!dir  ||  (EIO_Event)(dir | eIO_ReadWrite) != eIO_ReadWrite) {
        CORE_LOGF_X(15, eLOG_Error,
                    ("%s[SOCK::Shutdown] "
                     " Invalid direction #%u",
                     s_ID(sock, _id), (unsigned int) dir));
        return eIO_InvalidArg;
    }

    status = s_Shutdown(sock, dir, SOCK_GET_TIMEOUT(sock, c));
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = eIO_Close;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status SOCK_CloseEx(SOCK        sock,
                               int/*bool*/ destroy)
{
    EIO_Status status;
    if (!sock)
        return eIO_InvalidArg;
    if (sock->sock == SOCK_INVALID)
        status = eIO_Closed;
    else if (s_Initialized > 0)
        status = s_Close(sock, 0, fSOCK_KeepNone);
    else {
        if (sock->sslctx)
            sock->sslctx->sess = 0;  /*NB: session may leak out, if deinited*/
        sock->sock = SOCK_INVALID;
        status = eIO_Success;
    }
    /* likely NOOP */
    BUF_Erase(sock->r_buf);
    BUF_Erase(sock->w_buf);
    if (destroy) {
        if (sock->sslctx) {
            if (sock->sslctx->host)
                free((void*) sock->sslctx->host);
            assert(!sock->sslctx->sess);
            free(sock->sslctx);
        }
        BUF_Destroy(sock->r_buf);
        BUF_Destroy(sock->w_buf);
        free(sock);
    }
    return status;
}


/* NB: aka SOCK_Destroy() */
extern EIO_Status SOCK_Close(SOCK sock)
{
    return SOCK_CloseEx(sock, 1/*destroy*/);
}


extern EIO_Status SOCK_GetOSHandleEx(SOCK       sock,
                                     void*      handle,
                                     size_t     handle_size,
                                     EOwnership ownership)
{
    EIO_Status   status;
    TSOCK_Handle fd;

    if (!handle  ||  handle_size != sizeof(sock->sock)) {
        char _id[MAXIDLEN];
        CORE_LOGF_X(73, eLOG_Error,
            ("%s[SOCK::GetOSHandle] "
                " Invalid handle%s %lu",
                s_ID(sock, _id),
                handle ? " size"                     : "",
                handle ? (unsigned long) handle_size : 0UL));
        assert(0);
        return eIO_InvalidArg;
    }
    if (!sock) {
        fd = SOCK_INVALID;
        memcpy(handle, &fd, handle_size);
        return eIO_InvalidArg;
    }
    fd = sock->sock;
    memcpy(handle, &fd, handle_size);
    if (s_Initialized <= 0  ||  fd == SOCK_INVALID)
        status = eIO_Closed;
    else if (ownership != eTakeOwnership)
        status = eIO_Success;
    else {
        sock->keep = 1/*true*/;
        status = s_Close(sock, 0, fSOCK_KeepNone);
    }
    return status;
}


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle,
                                   size_t handle_size)
{
    return SOCK_GetOSHandleEx(sock, handle, handle_size, eNoOwnership);
}


extern EIO_Status SOCK_CloseOSHandle(const void* handle,
                                     size_t      handle_size)
{
    EIO_Status    status;
    struct linger lgr;
    TSOCK_Handle  fd;

    if (!handle  ||  handle_size != sizeof(fd))
        return eIO_InvalidArg;

    memcpy(&fd, handle, sizeof(fd));
    if (fd == SOCK_INVALID)
        return eIO_Closed;

    /* drop all possible hold-ups w/o checks */
    lgr.l_linger = 0;  /* RFC 793, Abort */
    lgr.l_onoff  = 1;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*) &lgr, sizeof(lgr));
#ifdef TCP_LINGER2
    {{
        int no = -1;
        setsockopt(fd, IPPROTO_TCP, TCP_LINGER2, (char*) &no, sizeof(no));
    }}
#endif /*TCP_LINGER2*/

    status = eIO_Success;
    for (;;) { /* close persistently - retry if interrupted by a signal */
        int error;

        if (SOCK_CLOSE(fd) == 0)
            break;

        /* error */
        if (s_Initialized <= 0)
            break;
        error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
        if (error == WSANOTINITIALISED) {
            s_Initialized = -1/*deinited*/;
            break;
        }
#endif /*NCBI_OS_MSWIN*/
        if (error == SOCK_ENOTCONN    ||
            error == SOCK_ENETRESET   ||
            error == SOCK_ECONNRESET  ||
            error == SOCK_ECONNABORTED) {
            status = eIO_Closed;
            break;
        }
        if (error != SOCK_EINTR) {
            status = error == SOCK_ETIMEDOUT ? eIO_Timeout : eIO_Unknown;
            break;
        }
        /* Maybe in an Ex version of this call someday...
        if (s_InterruptOnSignal == eOn) {
            status = eIO_Interrupt;
            break;
        }
        */
    }
    return status;
}


extern EIO_Status SOCK_Wait(SOCK            sock,
                            EIO_Event       event,
                            const STimeout* timeout)
{
    char       _id[MAXIDLEN];
    EIO_Status status;

    if (timeout == kDefaultTimeout) {
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(56, eLOG_Error,
                    ("%s[SOCK::Wait] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Unknown;
    }

    /* check against already shutdown socket there */
    switch (event) {
    case eIO_Open:
        if (sock->type == eSOCK_Datagram)
            return eIO_Success/*always connected*/;
        if (!sock->connected  ||  sock->pending) {
            struct timeval tv;
            return s_WaitConnected(sock, s_to2tv(timeout, &tv));
        }
        if (sock->r_status == eIO_Success  &&  sock->w_status == eIO_Success)
            return sock->eof ? eIO_Unknown : eIO_Success;
        if (sock->r_status == eIO_Closed   &&  sock->w_status == eIO_Closed)
            return eIO_Closed;
        return eIO_Unknown;

    case eIO_Read:
        if (BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->type == eSOCK_Datagram)
            return eIO_Closed;
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF_X(57, eLOG_Warning,
                        ("%s[SOCK::Wait(R)] "
                         " Socket already %s",
                         s_ID(sock, _id), sock->eof ? "closed" : "shut down"));
            return eIO_Closed;
        }
        if (sock->eof)
            return eIO_Closed;
        break;

    case eIO_Write:
        if (sock->type == eSOCK_Datagram)
            return eIO_Success;
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF_X(58, eLOG_Warning,
                        ("%s[SOCK::Wait(W)] "
                         " Socket already shut down",
                         s_ID(sock, _id)));
            return eIO_Closed;
        }
        break;

    case eIO_ReadWrite:
        if (sock->type == eSOCK_Datagram  ||  BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if ((sock->r_status == eIO_Closed  ||  sock->eof)  &&
            (sock->w_status == eIO_Closed)) {
            if (sock->r_status == eIO_Closed) {
                CORE_LOGF_X(59, eLOG_Warning,
                            ("%s[SOCK::Wait(RW)] "
                             " Socket already shut down",
                             s_ID(sock, _id)));
            }
            return eIO_Closed;
        }
        if (sock->r_status == eIO_Closed  ||  sock->eof) {
            if (sock->r_status == eIO_Closed) {
                CORE_LOGF_X(60, eLOG_Warning,
                            ("%s[SOCK::Wait(RW)] "
                             " Socket already %s",
                             s_ID(sock, _id), sock->eof
                             ? "closed" : "shut down for reading"));
            }
            event = eIO_Write;
            break;
        }
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF_X(61, eLOG_Warning,
                        ("%s[SOCK::Wait(RW)] "
                         " Socket already shut down for writing",
                         s_ID(sock, _id)));
            event = eIO_Read;
            break;
        }
        break;

    default:
        CORE_LOGF_X(62, eLOG_Error,
                    ("%s[SOCK::Wait] "
                     " Invalid event #%u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
        return eIO_InvalidArg;
    }

    assert(sock->type == eSOCK_Socket);
    status = s_Wait(sock, event, timeout);
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Timeout) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            info.host =       addr;
            info.port = sock->port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.event = event;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status SOCK_Poll(size_t          n,
                            SSOCK_Poll      polls[],
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    EIO_Status     status = eIO_InvalidArg;
    int/*bool*/    error;
    struct timeval tv;
    size_t         i;

#ifdef NCBI_MONKEY
    SSOCK_Poll* orig_polls = polls; /* to know if 'polls' was replaced */
    EIO_Status  mnk_status = -1;
    size_t      orig_n = n;
    /* Not a poll function itself, just removes some of "polls" items */
    if (g_MONKEY_Poll)
        g_MONKEY_Poll(&n, &polls, &mnk_status);
    /* Even if call was intercepted, s_Select() continues as if nothing
       happened, because what we did was just removed some SSOCK_Poll pointers.
       The changes made in s_Select() will appear in the original array, but
       only for those SSOCK_Poll's that were left by Monkey */
#endif /*NCBI_MONKEY*/

    if (n  &&  !polls) {
        if ( n_ready )
            *n_ready = 0;
        return status/*eIO_InvalidArg*/;
    }

    error = 0/*false*/;
    for (i = 0;  i < n;  ++i) {
        SOCK      sock;
        EIO_Event event;
        if (!(sock = polls[i].sock)  ||  !(event = polls[i].event)) {
            polls[i].revent = eIO_Open;
            continue;
        }
        if ((event | eIO_ReadWrite) != eIO_ReadWrite) {
            polls[i].revent = eIO_Close;
            /*status = eIO_InvalidArg;*/
            error = 1/*true*/;
            continue;
        }
        if (error) {
            polls[i].revent = eIO_Open;
            continue;
        }
        polls[i].revent =
            sock->type == eSOCK_Trigger  &&  ((TRIGGER) sock)->isset.ptr
            ? polls[i].event
            : eIO_Open;
        if (!(sock->type & eSOCK_Socket)  ||  sock->sock == SOCK_INVALID)
            continue;
        if ((polls[i].event & eIO_Read)  &&  BUF_Size(sock->r_buf) != 0) {
            polls[i].revent = eIO_Read;
            continue;
        }
        if (sock->type != eSOCK_Socket)
            continue;
        if ((polls[i].event == eIO_Read
            &&  (sock->r_status == eIO_Closed  ||  sock->eof))  ||
            (polls[i].event == eIO_Write
            &&   sock->w_status == eIO_Closed)) {
            polls[i].revent = eIO_Close;
        }
    }

    if (!n_ready)
        n_ready = &i;
    if (!error) {
        status = s_SelectStallsafe(n, polls, s_to2tv(timeout, &tv), n_ready);
        assert(!(status == eIO_Success) == !*n_ready);
    } else {
        assert(status != eIO_Success);
        *n_ready = 0;
    }

    if (status != eIO_Success) {
        for (i = 0;  i < n;  ++i) {
            if (polls[i].revent != eIO_Close)
                polls[i].revent  = eIO_Open;
        }
        assert(!*n_ready);
    }

#ifdef NCBI_MONKEY
    if (orig_polls != polls) {
        /* Copy poll results to the original array if some were excluded */
        size_t orig_iter, new_iter;
        for (orig_iter = 0;  orig_iter < orig_n;  ++orig_iter) {
            for (new_iter = 0;  new_iter < n;  ++new_iter) {
                if (orig_polls[orig_iter].sock == polls[new_iter].sock) {
                    orig_polls[orig_iter] = polls[new_iter];
                    break;
                }
                if (new_iter >= n)
                    orig_polls[orig_iter].revent = eIO_Open/*no event*/;
            }
        }
        free(polls);
        polls = orig_polls;
        if (mnk_status != -1)
            status = mnk_status;
        /* FIXME make sure: !(status == eIO_Success) == !*n_ready */
    }
#endif /*NCBI_MONKEY*/

    return status;
}


extern EIO_Status SOCK_SetTimeout(SOCK            sock,
                                  EIO_Event       event,
                                  const STimeout* timeout)
{
    char _id[MAXIDLEN];

    if (timeout == kDefaultTimeout) {
        assert(0);
        return eIO_InvalidArg;
    }
    switch (event) {
    case eIO_Read:
        sock->r_tv_set = s_to2tv(timeout, &sock->r_tv) ? 1 : 0;
        break;
    case eIO_Write:
        sock->w_tv_set = s_to2tv(timeout, &sock->w_tv) ? 1 : 0;
        break;
    case eIO_ReadWrite:
        sock->r_tv_set = s_to2tv(timeout, &sock->r_tv) ? 1 : 0;
        sock->w_tv_set = s_to2tv(timeout, &sock->w_tv) ? 1 : 0;
        break;
    case eIO_Close:
        sock->c_tv_set = s_to2tv(timeout, &sock->c_tv) ? 1 : 0;
        break;
    default:
        CORE_LOGF_X(63, eLOG_Error,
                    ("%s[SOCK::SetTimeout] "
                     " Invalid event #%u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


extern const STimeout* SOCK_GetTimeout(SOCK      sock,
                                       EIO_Event event)
{
    char _id[MAXIDLEN];

    if (event == eIO_ReadWrite) {
        if      (!sock->r_tv_set)
            event = eIO_Write;
        else if (!sock->w_tv_set)
            event = eIO_Read;
        else {
            /* NB: timeouts stored normalized */
            if (sock->r_tv.tv_sec > sock->w_tv.tv_sec)
                return s_tv2to(&sock->w_tv, &sock->w_to);
            if (sock->w_tv.tv_sec > sock->r_tv.tv_sec)
                return s_tv2to(&sock->r_tv, &sock->r_to);
            assert(sock->r_tv.tv_sec == sock->w_tv.tv_sec);
            return sock->r_tv.tv_usec > sock->w_tv.tv_usec
                ? s_tv2to(&sock->w_tv, &sock->w_to)
                : s_tv2to(&sock->r_tv, &sock->r_to);
        }
    }
    switch (event) {
    case eIO_Read:
        return sock->r_tv_set ? s_tv2to(&sock->r_tv, &sock->r_to) : 0;
    case eIO_Write:
        return sock->w_tv_set ? s_tv2to(&sock->w_tv, &sock->w_to) : 0;
    case eIO_Close:
        return sock->c_tv_set ? s_tv2to(&sock->c_tv, &sock->c_to) : 0;
    default:
        CORE_LOGF_X(64, eLOG_Error,
                    ("%s[SOCK::GetTimeout] "
                     " Invalid event #%u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
    }
    return 0/*kInfiniteTimeout*/;
}


extern EIO_Status SOCK_Read(SOCK           sock,
                            void*          buf,
                            size_t         size,
                            size_t*        n_read,
                            EIO_ReadMethod how)
{
    EIO_Status status;
    size_t     x_read;
    char       _id[MAXIDLEN];

    if (sock->sock != SOCK_INVALID) {
        char*  x_buf;
        switch (how) {
        case eIO_ReadPeek:
            status = s_Read(sock, buf, size, &x_read, 1/*peek*/);
            break;

        case eIO_ReadPlain:
            status = s_Read(sock, buf, size, &x_read, 0/*read*/);
            break;

        case eIO_ReadPersist:
            x_buf = (char*) buf;
            x_read = 0;
            do {
                size_t xx_read;
                status = s_Read(sock, x_buf,
                                size, &xx_read, 0/*read*/);
                x_read    += xx_read;
                if (x_buf)
                    x_buf += xx_read;
                size      -= xx_read;
            } while (size  &&  status == eIO_Success);
            break;

        default:
            CORE_LOGF_X(65, eLOG_Error,
                        ("%s[SOCK::Read] "
                         " Unsupported read method #%u",
                         s_ID(sock, _id), (unsigned int) how));
            status = eIO_NotSupported;
            x_read = 0;
            assert(0);
            break;
        }
    } else {
        CORE_LOGF_X(66, eLOG_Error,
                    ("%s[SOCK::Read] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        status = eIO_Unknown;
        x_read = 0;
    }

    if ( n_read )
        *n_read = x_read;
    return status;
}


#define s_Pushback(s, d, n)  BUF_Pushback(&(s)->r_buf, d, n)


extern EIO_Status SOCK_ReadLine(SOCK    sock,
                                char*   line,
                                size_t  size,
                                size_t* n_read)
{
    unsigned int/*bool*/ cr_seen, done;
    char        _id[MAXIDLEN];
    EIO_Status  status;
    size_t      len;

    if ( n_read )
        *n_read = 0;
    if (!size  ||  !line) {
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(125, eLOG_Error,
                    ("%s[SOCK::ReadLine] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Unknown;
    }

    len = 0;
    cr_seen = done = 0/*false*/;
    do {
        size_t i, x_size;
        char   w[1024], c;
        char*  x_buf = size - len < sizeof(w) - cr_seen ? w : line + len;
        if (!(x_size = BUF_Size(sock->r_buf)) ||  x_size > sizeof(w) - cr_seen)
            x_size = sizeof(w) - cr_seen;

        status = s_Read(sock, x_buf + cr_seen, x_size, &x_size, 0/*read!*/);
        assert(status == eIO_Success  ||  !x_size);
        if (cr_seen)
            ++x_size;

        i = cr_seen;
        while (i < x_size  &&  len < size) {
            c = x_buf[i++];
            if (c == '\n') {
                cr_seen = 0/*false*/;
                done = 1/*true*/;
                break;
            }
            if (cr_seen) {
                line[len++] = '\r';
                cr_seen = 0/*false*/;
                --i;  /*have to re-read*/
                continue;
            }
            if (c == '\r') {
                cr_seen = 1/*true*/;
                continue;
            }
            if (!c) {
                assert(!cr_seen);
                done = 1/*true*/;
                break;
            }
            line[len++] = c;
        }
        assert(!done  ||  !cr_seen);
        if (len >= size) {
            /* out of room */
            assert(!done  &&  len);
            if (cr_seen) {
                c = '\r';
                if (!s_Pushback(sock, &c, 1)) {
                    CORE_LOGF_X(165, eLOG_Critical,
                                ("%s[SOCK::ReadLine] "
                                 " Cannot pushback extra CR",
                                 s_ID(sock, _id)));
                    /* register a severe error */
                    sock->r_status = eIO_Closed;
                    sock->eof = 1/*true*/;
                } else
                    cr_seen = 0/*false*/;
            }
            done = 1/*true*/;
        }
        if (i < x_size) {
            /* pushback excess */
            assert(done);
            if (!cr_seen) {
                if (!s_Pushback(sock, &x_buf[i], x_size -= i)) {
                    CORE_LOGF_X(166, eLOG_Critical,
                                ("%s[SOCK::ReadLine] "
                                 " Cannot pushback extra data (%lu byte%s)",
                                 s_ID(sock, _id), (unsigned long) x_size,
                                 &"s"[x_size == 1]));
                    /* register a severe error */
                    sock->r_status = eIO_Closed;
                    sock->eof = 1/*true*/;
                }
            }
            break;
        }
    } while (!done  &&  status == eIO_Success);

    if (len < size)
        line[len] = '\0';
    if ( n_read )
        *n_read = len;

    return done ? eIO_Success : status;
}


extern EIO_Status SOCK_Pushback(SOCK        sock,
                                const void* data,
                                size_t      size)
{
    if (size  &&  !data) {
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        char _id[MAXIDLEN];
        CORE_LOGF_X(67, eLOG_Error,
                    ("%s[SOCK::Pushback] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    return s_Pushback(sock, data, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_Status(SOCK      sock,
                              EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Open:
        case eIO_Read:
        case eIO_Write:
            if (sock->sock == SOCK_INVALID)
                return direction/*!eIO_Open*/ ? eIO_Unknown : eIO_Closed;
            if (!sock->connected  ||  sock->pending)
                return eIO_Timeout;
            if (direction == eIO_Read) {
                if (sock->type != eSOCK_Socket  ||  !sock->eof)
                    return (EIO_Status) sock->r_status;
                return sock->r_status == eIO_Closed ? eIO_Unknown : eIO_Closed;
            }
            if (direction == eIO_Write)
                return (EIO_Status) sock->w_status;
            return eIO_Success;
        default:
            break;
        }
    }
    return eIO_InvalidArg;
}


extern EIO_Status SOCK_Write(SOCK            sock,
                             const void*     data,
                             size_t          size,
                             size_t*         n_written,
                             EIO_WriteMethod how)
{
    EIO_Status status;
    size_t     x_written;
    char       _id[MAXIDLEN];

    if (size  &&  !data) {
        if ( n_written )
            *n_written = 0;
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock != SOCK_INVALID) {
        const char* x_data;
        switch (how) {
        case eIO_WriteOutOfBand:
            if (sock->type == eSOCK_Datagram) {
                CORE_LOGF_X(68, eLOG_Error,
                            ("%s[SOCK::Write] "
                             " OOB not supported for datagrams",
                             s_ID(sock, _id)));
                status = eIO_NotSupported;
                x_written = 0;
                break;
            }
            /*FALLTHRU*/

        case eIO_WritePlain:
            status = s_Write(sock, data, size, &x_written,
                             how == eIO_WriteOutOfBand ? 1 : 0);
            break;

        case eIO_WritePersist:
            x_written = 0;
            x_data = (const char*) data;
            do {
                size_t xx_written;
                status = s_Write(sock, x_data,
                                 size, &xx_written, 0);
                x_written += xx_written;
                x_data    += xx_written;
                size      -= xx_written;
            } while (size  &&  status == eIO_Success);
            break;

        default:
            CORE_LOGF_X(69, eLOG_Error,
                        ("%s[SOCK::Write] "
                         " Unsupported write method #%u",
                         s_ID(sock, _id), (unsigned int) how));
            status = eIO_NotSupported;
            x_written = 0;
            assert(0);
            break;
        }
    } else {
        CORE_LOGF_X(70, eLOG_Error,
                    ("%s[SOCK::Write] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        status = eIO_Closed;
        x_written = 0;
    }

    if ( n_written )
        *n_written = x_written;
    return status;
}


extern EIO_Status SOCK_Abort(SOCK sock)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(71, eLOG_Warning,
                    ("%s[SOCK::Abort] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF_X(72, eLOG_Error,
                    ("%s[SOCK::Abort] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    return s_Close_(sock, 1/*abort*/, fSOCK_KeepNone);
}


extern unsigned short SOCK_GetLocalPortEx(SOCK          sock,
                                          int/*bool*/   trueport,
                                          ENH_ByteOrder byte_order)
{
    unsigned short port;

    if (!sock  ||  sock->sock == SOCK_INVALID)
        return 0;

#ifdef NCBI_OS_UNIX
    if (sock->path[0])
        return 0/*UNIX socket*/;
#endif /*NCBI_OS_UNIX*/

    if (trueport  ||  !sock->myport) {
        port = s_GetLocalPort(sock->sock);
        if (!trueport)
            sock->myport = port; /*cache it*/
    } else
        port = sock->myport;
    return byte_order == eNH_HostByteOrder ? port : htons(port);
}


extern unsigned short SOCK_GetLocalPort(SOCK          sock,
                                        ENH_ByteOrder byte_order)
{
    return SOCK_GetLocalPortEx(sock, 0/*false*/, byte_order);
}


extern void SOCK_GetPeerAddress(SOCK            sock,
                                unsigned int*   host,
                                unsigned short* port,
                                ENH_ByteOrder   byte_order)
{
    if (!sock  ||  sock->sock == SOCK_INVALID) {
        if ( host )
            *host = 0;
        if ( port )
            *port = 0;
        return;
    }
    if ( host ) {
        unsigned int x_host = NcbiIPv6ToIPv4(&sock->addr, 0);
        *host = byte_order == eNH_HostByteOrder
            ? ntohl(x_host) : x_host;
    }
    if ( port ) {
        *port = byte_order == eNH_HostByteOrder
            ? sock->port : ntohs(sock->port);
    }
}


extern void SOCK_GetPeerAddress6(SOCK            sock,
                                 TNCBI_IPv6Addr* addr,
                                 unsigned short* port,
                                 ENH_ByteOrder   byte_order)
{
    if (!sock  ||  sock->sock == SOCK_INVALID) {
        if ( addr )
            memset(addr, 0, sizeof(*addr));
        if ( port )
            *port = 0;
        return;
    }
    if ( addr ) {
        *addr = sock->addr;
    }
    if ( port ) {
        *port = byte_order == eNH_HostByteOrder
            ? sock->port : ntohs(sock->port);
    }
}


extern unsigned short SOCK_GetRemotePort(SOCK          sock,
                                         ENH_ByteOrder byte_order)
{
    unsigned short port;
    SOCK_GetPeerAddress(sock, 0, &port, byte_order);
    return port;
}


extern char* SOCK_GetPeerAddressStringEx(SOCK                sock,
                                         char*               buf,
                                         size_t              bufsize,
                                         ESOCK_AddressFormat format)
{
#ifdef NCBI_OS_UNIX
    const char* path = sock->type & eSOCK_Socket ? sock->path : ((LSOCK) sock)->path;
#endif /*NCBI_OS_UNIX*/
    char   port[10];
    size_t len;

    if (!buf  ||  !bufsize)
        return 0/*error*/;
    if (!sock  ||  sock->sock == SOCK_INVALID) {
        *buf = '\0';
        return 0/*error*/;
    }
    switch (format) {
    case eSAF_Full:
#ifdef NCBI_OS_UNIX
        if (path[0]) {
            if ((len = strlen(path)) < bufsize)
                memcpy(buf, path, len + 1);
            else
                return 0/*error*/;
        } else
#endif /*NCBI_OS_UNIX*/
        if (!s_HostPortToString(&sock->addr, sock->port, buf, bufsize))
            return 0/*error*/;
        break;
    case eSAF_Port:
#ifdef NCBI_OS_UNIX
        if (path[0]) 
            *buf = '\0';
        else
#endif /*NCBI_OS_UNIX*/
        if ((len = (size_t) sprintf(port, "%hu", sock->port)) < bufsize)
            memcpy(buf, port, len + 1);
        else
            return 0/*error*/;
        break;
    case eSAF_IP:
#ifdef NCBI_OS_UNIX
        if (path[0]) 
            *buf = '\0';
        else
#endif /*NCBI_OS_UNIX*/
        if (!s_AddrToString(buf, bufsize, &sock->addr, s_IPVersion,
                            NcbiIsEmptyIPv6(&sock->addr))) {
            return 0/*error*/;
        }
        break;
    default:
        return 0/*error*/;
    }
    return buf;
}


extern char* SOCK_GetPeerAddressString(SOCK   sock,
                                       char*  buf,
                                       size_t bufsize)
{
    return SOCK_GetPeerAddressStringEx(sock, buf, bufsize, eSAF_Full);
}


extern ESwitch SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    ESwitch old = s_ReadOnWrite;
    if (on_off != eDefault)
        s_ReadOnWrite = on_off;
    return old;
}


extern ESwitch SOCK_SetReadOnWrite(SOCK    sock,
                                   ESwitch on_off)
{
    if (sock->type != eSOCK_Datagram) {
        ESwitch old = (ESwitch) sock->r_on_w;
        sock->r_on_w = on_off;
        return old;
    }
    return eDefault;
}


/*ARGSUSED*/
extern void SOCK_DisableOSSendDelay(SOCK        sock,
                                    int/*bool*/ on_off)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(156, eLOG_Warning,
                    ("%s[SOCK::DisableOSSendDelay] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF_X(157, eLOG_Error,
                    ("%s[SOCK::DisableOSSendDelay] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return;
    }

#ifdef TCP_NODELAY
    if (setsockopt(sock->sock, IPPROTO_TCP, TCP_NODELAY,
                   (char*) &on_off, sizeof(on_off)) != 0) {
        int error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(error);
        CORE_LOGF_ERRNO_EXX(75, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[SOCK::DisableOSSendDelay] "
                             " Failed setsockopt(%sTCP_NODELAY)",
                             s_ID(sock, _id), on_off ? "" : "!"));
        UTIL_ReleaseBuffer(strerr);
    }
#else
    (void) on_off;
#endif /*TCP_NODELAY*/
}


/*ARGSUSED*/
extern void SOCK_SetCork(SOCK        sock,
                         int/*bool*/ on_off)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(158, eLOG_Warning,
            ("%s[SOCK::SetCork] "
                " Invalid socket",
                s_ID(sock, _id)));
        return;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF_X(159, eLOG_Error,
            ("%s[SOCK::SetCork] "
                " Datagram socket",
                s_ID(sock, _id)));
        assert(0);
        return;
    }

#if defined(TCP_CORK)  &&  !defined(NCBI_OS_CYGWIN)
    if (setsockopt(sock->sock, IPPROTO_TCP, TCP_CORK,
        (char*) &on_off, sizeof(on_off)) != 0) {
        int error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(error);
        CORE_LOGF_ERRNO_EXX(160, eLOG_Warning,
            error, strerr ? strerr : "",
            ("%s[SOCK::SetCork] "
                " Failed setsockopt(%sTCP_CORK)",
                s_ID(sock, _id), on_off ? "" : "!"));
        UTIL_ReleaseBuffer(strerr);
    }
#  ifdef TCP_NOPUSH
    /* try to avoid 5 second delay on BSD systems (incl. Darwin) */
    if (!on_off)
        (void) send(sock->sock, _id/*dontcare*/, 0, 0);
#  endif /*TCP_NOPUSH*/
#else
    (void) on_off;
#endif /*TCP_CORK && !NCBI_OS_CYGWIN*/
}



/******************************************************************************
 *  DATAGRAM SOCKET
 */


extern EIO_Status DSOCK_CreateEx(SOCK*       sock,
                                 TSOCK_Flags flags)
{
    TSOCK_Handle fd;
    int          type;
#ifdef NCBI_OS_MSWIN
    HANDLE       event;
#endif /*NCBI_OS_MSWIN*/
    int          error;
    int          family;
    SOCK         x_sock;
    unsigned int x_id = x_ID_Counter() * 1000;

    *sock = 0;

    /* initialize internals */
    if ((flags & fSOCK_Secure)  ||  s_InitAPI(0) != eIO_Success)
        return eIO_NotSupported;

    type  = SOCK_DGRAM;
#ifdef SOCK_NONBLOCK
    type |= SOCK_NONBLOCK;
#endif /*SOCK_NONBLOCK*/
#ifdef SOCK_CLOEXEC
    if (!(flags & fSOCK_KeepOnExec))
        type |= SOCK_CLOEXEC;
#endif /*SOCK_CLOEXEC*/
    if ((family = s_IPVersion) == AF_UNSPEC)
        family = AF_INET;
    /* create new datagram socket */
    if ((fd = socket(family, type, 0)) == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(76, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("DSOCK#%u[?]: [DSOCK::Create] "
                             " Cannot create socket",
                             x_id));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }

#if defined(NCBI_OS_MSWIN)
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(139, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Failed to create IO event",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_CLOSE(fd);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(fd, event, SOCK_EVENTS) != 0) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(140, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#elif !defined(SOCK_NONBLOCK)
    /* set to non-blocking mode */
    if (!s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(77, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(fd);
        return eIO_Unknown;
    }
#endif /*O_NONBLOCK setting*/

    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock)))) {
        SOCK_CLOSE(fd);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* success... */
    x_sock->sock      = fd;
    x_sock->id        = x_id;
    /* no host and port - not "connected" */
    x_sock->type      = eSOCK_Datagram;
    x_sock->side      = eSOCK_Client;
    x_sock->log       = flags & (fSOCK_LogDefault | fSOCK_LogOn);
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
#ifdef NCBI_OS_MSWIN
    x_sock->event     = event;
    x_sock->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    /* all timeout bits cleared - infinite */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    BUF_SetChunkSize(&x_sock->w_buf, SOCK_BUF_CHUNK_SIZE);

#ifndef SOCK_CLOEXEC
    if (!x_sock->crossexec  &&  !s_SetCloexec(fd, 1/*true*/)) {
        const char* strerr;
        char _id[MAXIDLEN];
#  ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        error = err;
#  else
        error = errno;
        strerr = SOCK_STRERROR(error);
#  endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(130, eLOG_Warning,
                            error, strerr ? strerr : "",
                            ("%s[DSOCK::Create]  Cannot set"
                             " socket close-on-exec mode",
                             s_ID(*sock, _id)));
#  ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#  else
        UTIL_ReleaseBuffer(strerr);
#  endif /*NCBI_OS_MSWIN*/
    }
#endif /*!SOCK_CLOEXEC*/

    /* statistics & logging */
    if (x_sock->log == eOn  ||  (x_sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, x_sock, eIO_Open, 0, 0, 0);

    *sock = x_sock;
    return eIO_Success;
}


extern EIO_Status DSOCK_Create(SOCK* sock)
{
    return DSOCK_CreateEx(sock, fSOCK_LogDefault);
}


extern EIO_Status DSOCK_Bind6(SOCK           sock,
                              unsigned short port,
                              ESwitch        ipv6)
{
    char _id[MAXIDLEN];
    int error, family;
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } u;
    TSOCK_socklen_t addrlen;
    EIO_Status status;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(79, eLOG_Error,
                    ("%s[DSOCK::Bind] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(78, eLOG_Error,
                    ("%s[DSOCK::Bind] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    if ((ipv6 == eOn   &&  s_IPVersion == AF_INET)  ||
        (ipv6 == eOff  &&  s_IPVersion == AF_INET6)) {
       status = eIO_NotSupported;
       error = ENOTSUP;
    } else {
        switch (ipv6) {
        case eOn:
            family = AF_INET6;
            break;
        case eOff:
            family = AF_INET;
            break;
        default:
            if ((family = s_IPVersion) == AF_UNSPEC)
                family = AF_INET;
            break;
        }
        assert(family == AF_INET  ||  family == AF_INET6);
        addrlen = family == AF_INET6 ? sizeof(u.in6) : sizeof(u.in);
        /* bind */
        memset(&u, 0, addrlen);
        u.sa.sa_family = family;
    #ifdef HAVE_SIN_LEN
        u.sa.sa_len = addrlen;
    #endif /*HAVE_SIN_LEN*/
        if (family == AF_INET) {
            u.in.sin_addr.s_addr = htonl(INADDR_ANY);
            u.in.sin_port        = htons(port);
        } else
            u.in6.sin6_port      = htons(port);
        if (bind(sock->sock, &u.sa, addrlen) != 0) {
            error = SOCK_ERRNO;
            status = error != SOCK_EADDRINUSE ? eIO_Unknown : eIO_Closed;
        } else
            status = eIO_Success;
    }
    if (status != eIO_Success) {
        const char* strerr = SOCK_STRERROR(error);
        CORE_LOGF_ERRNO_EXX(80, error != SOCK_EADDRINUSE
                            ? eLOG_Error : eLOG_Trace,
                            error, strerr ? strerr : "",
                            ("%s[DSOCK::Bind] "
                             " Failed bind(:%hu)",
                             s_ID(sock, _id), port));
        UTIL_ReleaseBuffer(strerr);
        return status;
    }
    if (!port) {
        assert(u.sa.sa_family == family);
        error = getsockname(sock->sock, &u.sa, &addrlen) != 0
            ? SOCK_ERRNO : 0;
        if (!error) {
            switch (u.sa.sa_family) {
            case AF_INET:
                port = htons(u.in.sin_port);
                break;
            case AF_INET6:
                port = htons(u.in6.sin6_port);
                break;
            default:
                error = ENOTSUP;
                break;
            }
        }
        if (error  ||  !port) {
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOGF_ERRNO_EXX(114, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("%s[DSOCK::Bind] "
                                 " Cannot obtain a free socket port",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            return error == ENOTSUP ? eIO_NotSupported : eIO_Closed;
        }
    }
    sock->side = eSOCK_Server;

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, 0, 0, &u.sa);

    sock->myport = port;
    return eIO_Success;
}


extern EIO_Status DSOCK_Bind(SOCK           sock,
                             unsigned short port)
{
    return DSOCK_Bind6(sock, port, eDefault);
}


extern EIO_Status DSOCK_Connect(SOCK           sock,
                                const char*    host,
                                unsigned short port)
{
    char _id[MAXIDLEN];
    union {
        struct sockaddr     sa;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } u;
    TNCBI_IPv6Addr  addr;
    TSOCK_socklen_t addrlen;
    char addrstr[SOCK_HOSTPORTSTRLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(82, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(81, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    /* drop all pending data */
    BUF_Erase(sock->r_buf);
    BUF_Erase(sock->w_buf);
    sock->r_len = 0;
    sock->w_len = 0;
    sock->eof = 0;
    sock->id++;

    if (host  &&  !*host)
        host = 0;
    if (!host != !port) {
        if (port) {
            assert(!host);
            sprintf(addrstr, ":%hu", port);
        } else
            *addrstr = '\0';
        CORE_LOGF_X(84, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Address \"%.*s%s\" incomplete, missing %s",
                     s_ID(sock, _id), CONN_HOST_LEN, host ? host : "",
                     addrstr, port ? "host" : "port"));
        return eIO_InvalidArg;
    }
    if (!host)
        memset(&addr, 0, sizeof(addr));
    else if (!s_gethostbyname(&addr, host, s_IPVersion, 0, (ESwitch) sock->log)) {
        CORE_LOGF_X(83, eLOG_Error,
            ("%s[DSOCK::Connect] "
                " Failed SOCK_gethostbyname(\"%.*s\")",
                s_ID(sock, _id), CONN_HOST_LEN, host));
        return eIO_Unknown;
    } else if (NcbiIsEmptyIPv6(&addr))
        host = 0;

    /* connect (non-empty address) or drop association (on empty address) */
    if (host/*  &&  port*/) {
        int/*bool*/ ipv4 = NcbiIsIPv4(&addr);
        addrlen = ipv4 ? sizeof(u.in) : sizeof(u.in6);
        memset(&u, 0, addrlen);
        u.sa.sa_family = ipv4 ? AF_INET : AF_INET6;
        if (ipv4) {
            u.in.sin_addr.s_addr = NcbiIPv6ToIPv4(&addr, 0);
            u.in.sin_port        = htons(port);
        } else {
            memcpy(&u.in6.sin6_addr, &addr, sizeof(u.in6.sin6_addr));
            u.in6.sin6_port      = htons(port);
        }
    } else {
        addrlen = sizeof(u.sa);
        memset(&u, 0, addrlen);
        u.sa.sa_family = AF_UNSPEC;
    }
#ifdef HAVE_SIN_LEN
    u.sa.sa_len = addrlen;
#endif /*HAVE_SIN_LEN*/
    if (connect(sock->sock, &u.sa, addrlen) != 0) {
        int error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(error);
        if (port | sock->port) {
            /* NB: respective "addr" non-empty as well */
            s_HostPortToString(port ? &addr : &sock->addr,
                               port ?  port :  sock->port, 
                               addrstr, sizeof(addrstr));
        } else
            *addrstr = '\0';
        CORE_LOGF_ERRNO_EXX(85, eLOG_Error,
                            error, strerr ? strerr : "",
                            ("%s[DSOCK::Connect] "
                             " Failed %sconnect%s%s",
                             s_ID(sock, _id), port ? "" : "to dis",
                             !*addrstr ? "" : port ? " to " : " from ",
                             !*addrstr ? addrstr : " (not connected)"));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Closed;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, "", 0, &u.sa);

    sock->addr = addr;
    sock->port = port;
    return eIO_Success;
}


extern EIO_Status DSOCK_WaitMsg(SOCK            sock,
                                const STimeout* timeout)
{
    char           _id[MAXIDLEN];
    EIO_Status     status;
    SSOCK_Poll     poll;
    struct timeval tv;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(96, eLOG_Error,
                    ("%s[DSOCK::WaitMsg] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Unknown;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(95, eLOG_Error,
                    ("%s[DSOCK::WaitMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    poll.sock   = sock;
    poll.event  = eIO_Read;
    poll.revent = eIO_Open;
    status = s_Select(1, &poll, s_to2tv(timeout, &tv), 1/*asis*/);
    assert(poll.event == eIO_Read);
    if (status == eIO_Success  &&  poll.revent != eIO_Read) {
        assert(poll.revent == eIO_Close);
        status = eIO_Closed;
    }
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Timeout) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
        info.host =       addr;
        info.port = sock->port;
        info.event = eIO_Read;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status DSOCK_RecvMsg6(SOCK            sock,
                                 void*           buf,
                                 size_t          bufsize,
                                 size_t          msgsize,
                                 size_t*         msglen,
                                 TNCBI_IPv6Addr* sender_addr,
                                 unsigned short* sender_port)
{
    char       _id[MAXIDLEN];
    EIO_Status status;

    if ( msglen )
        *msglen = 0;
    if ( sender_addr )
        memset(sender_addr, 0, sizeof(*sender_addr));
    if ( sender_port )
        *sender_port = 0;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(93, eLOG_Error,
                    ("%s[DSOCK::RecvMsg] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Unknown;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(92, eLOG_Error,
                    ("%s[DSOCK::RecvMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    status = s_RecvMsg(sock, buf, bufsize, msgsize, msglen,
                       sender_addr, sender_port);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
        info.host =       addr;
        info.port = sock->port;
        info.event = eIO_Read;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status DSOCK_RecvMsg(SOCK            sock,
                                void*           buf,
                                size_t          bufsize,
                                size_t          msgsize,
                                size_t*         msglen,
                                unsigned int*   sender_addr,
                                unsigned short* sender_port)
{
    TNCBI_IPv6Addr addr;
    EIO_Status status = DSOCK_RecvMsg6(sock, buf, bufsize, msgsize, msglen,
                                       &addr, sender_port);
    if ( sender_addr )
        *sender_addr = NcbiIPv6ToIPv4(&addr, 0);
    return status;
}

    
extern EIO_Status DSOCK_SendMsg(SOCK           sock,
                                const char*    host,
                                unsigned short port,
                                const void*    data,
                                size_t         datalen)
{
    char       _id[MAXIDLEN];
    EIO_Status status;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(87, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(86, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (host  &&  !*host)
        host = 0;

    status = s_SendMsg(sock, host, port, data, datalen);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        char          addr[SOCK_ADDRSTRLEN];
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (!host) {
            s_AddrToString(addr, sizeof(addr), &sock->addr, s_IPVersion, 0);
            host = addr;
        }
        info.host = host;
        info.port = port ? port : sock->port;
        info.event = eIO_Write;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status DSOCK_WipeMsg(SOCK      sock,
                                EIO_Event direction)
{
    char _id[MAXIDLEN];
    EIO_Status status;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(98, eLOG_Error,
                    ("%s[DSOCK::WipeMsg] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(97, eLOG_Error,
                    ("%s[DSOCK::WipeMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    switch (direction) {
    case eIO_Read:
        sock->r_len = 0;
        BUF_Erase(sock->r_buf);
        sock->r_status = status = eIO_Success;
        break;
    case eIO_Write:
        sock->r_len = 0;
        BUF_Erase(sock->w_buf);
        sock->w_status = status = eIO_Success;
        break;
    default:
        CORE_LOGF_X(99, eLOG_Error,
                    ("%s[DSOCK::WipeMsg] "
                     " Invalid direction #%u",
                     s_ID(sock, _id), (unsigned int) direction));
        assert(0);
        status = eIO_InvalidArg;
        break;
    }

    return status;
}


extern EIO_Status DSOCK_SetBroadcast(SOCK        sock,
                                     int/*bool*/ on_off)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(101, eLOG_Error,
                    ("%s[DSOCK::SetBroadcast] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF_X(100, eLOG_Error,
                    ("%s[DSOCK::SetBroadcast] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    {{
#  ifdef NCBI_OS_MSWIN
        BOOL bcast = !!on_off;
#  else
        int  bcast = !!on_off;
#  endif /*NCBI_OS_MSWIN*/
        if (setsockopt(sock->sock, SOL_SOCKET, SO_BROADCAST,
                       (char*) &bcast, sizeof(bcast)) != 0) {
            int error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(error);
            CORE_LOGF_ERRNO_EXX(102, eLOG_Error,
                                error, strerr ? strerr : "",
                                ("%s[DSOCK::SetBroadcast] "
                                 " Failed setsockopt(%sBROADCAST)",
                                 s_ID(sock, _id), bcast ? "" : "NO"));
            UTIL_ReleaseBuffer(strerr);
            return eIO_Unknown;
        }
    }}
#else
    return eIO_NotSupported;
#endif /*NCBI_OS_UNIX || NXBI_OS_MSWIN*/
    return eIO_Success;
}


extern TNCBI_BigCount DSOCK_GetMessageCount(SOCK      sock,
                                            EIO_Event direction)
{
    if (sock  &&  sock->type == eSOCK_Datagram) {
        switch (direction) {
        case eIO_Read:
            return sock->n_in;
        case eIO_Write:
            return sock->n_out;
        default:
            assert(0);
            break;
        }
    }
    return 0;
}



/******************************************************************************
*  SOCKET SETTINGS
*/


extern ESwitch SOCK_SetInterruptOnSignalAPI(ESwitch on_off)
{
    ESwitch old = s_InterruptOnSignal;
    if (on_off != eDefault)
        s_InterruptOnSignal = on_off;
    return old;
}


extern ESwitch SOCK_SetInterruptOnSignal(SOCK    sock,
                                         ESwitch on_off)
{
    ESwitch old = (ESwitch) sock->i_on_sig;
    sock->i_on_sig = on_off;
    return old;
}


extern ESwitch SOCK_SetReuseAddressAPI(ESwitch on_off)
{
    ESwitch old = s_ReuseAddress;
    if (on_off != eDefault)
        s_ReuseAddress = on_off;
    return old;
}


extern void SOCK_SetReuseAddress(SOCK        sock,
                                 int/*bool*/ on_off)
{
    if (sock->sock != SOCK_INVALID
        &&  !s_SetReuseAddress(sock->sock, on_off)) {
        char _id[MAXIDLEN];
        int error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(error);
        CORE_LOGF_ERRNO_EXX(74, eLOG_Warning,
            error, strerr ? strerr : "",
            ("%s[SOCK::SetReuseAddress] "
                " Failed setsockopt(%sREUSEADDR)",
                s_ID(sock, _id), on_off ? "" : "NO"));
        UTIL_ReleaseBuffer(strerr);
    }
}


extern ESwitch SOCK_SetDataLoggingAPI(ESwitch log)
{
    ESwitch old = s_Log;
    if (log != eDefault)
        s_Log = log;
    return old;
}


extern ESwitch SOCK_SetDataLogging(SOCK    sock,
                                   ESwitch log)
{
    ESwitch old = (ESwitch) sock->log;
    sock->log = log;
    return old;
}


extern void SOCK_SetErrHookAPI(FSOCK_ErrHook hook,
                               void*         data)
{
    CORE_LOCK_WRITE;
    s_ErrData = hook ? data : 0;
    s_ErrHook = hook;
    CORE_UNLOCK;
}



/******************************************************************************
 *  CLASSIFICATION & STATS
 */


extern int/*bool*/ SOCK_IsDatagram(SOCK sock)
{
    return sock
        &&  sock->sock != SOCK_INVALID  &&  sock->type == eSOCK_Datagram;
}


extern int/*bool*/ SOCK_IsClientSide(SOCK sock)
{
    return sock
        &&  sock->sock != SOCK_INVALID  &&  sock->side == eSOCK_Client;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock
        &&  sock->sock != SOCK_INVALID  &&  sock->side == eSOCK_Server;
}


extern int/*bool*/ SOCK_IsUNIX(SOCK sock)
{
#ifdef NCBI_OS_UNIX
    return sock
        &&  sock->sock != SOCK_INVALID  &&  sock->path[0];
#else
    return 0/*false*/;
#endif /*NCBI_OS_UNIX*/
}


extern int/*bool*/ SOCK_IsSecure(SOCK sock)
{
    return sock  &&  sock->sock != SOCK_INVALID  &&  sock->sslctx;
}


extern TNCBI_BigCount SOCK_GetPosition(SOCK      sock,
                                       EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            if (sock->type == eSOCK_Datagram)
                return sock->r_len - BUF_Size(sock->r_buf);
            return sock->n_read    - (TNCBI_BigCount) BUF_Size(sock->r_buf);
        case eIO_Write:
            if (sock->type == eSOCK_Datagram)
                return BUF_Size(sock->w_buf);
            return sock->n_written + (TNCBI_BigCount)          sock->w_len;
        default:
            assert(0);
            break;
        }
    }
    return 0;
}


extern TNCBI_BigCount SOCK_GetCount(SOCK      sock,
                                    EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            return sock->type == eSOCK_Datagram? sock->r_len : sock->n_read;
        case eIO_Write:
            return sock->type == eSOCK_Datagram? sock->w_len : sock->n_written;
        default:
            assert(0);
            break;
        }
    }
    return 0;
}


extern TNCBI_BigCount SOCK_GetTotalCount(SOCK      sock,
                                         EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            return sock->type != eSOCK_Datagram? sock->n_in  : sock->n_read;
        case eIO_Write:
            return sock->type != eSOCK_Datagram? sock->n_out : sock->n_written;
        default:
            assert(0);
            break;
        }
    }
    return 0;
}



/******************************************************************************
 *  GENERIC POLLABLE API
 */


extern EIO_Status POLLABLE_Poll(size_t          n,
                                SPOLLABLE_Poll  polls[],
                                const STimeout* timeout,
                                size_t*         n_ready)
{
    return SOCK_Poll(n, (SSOCK_Poll*) polls, timeout, n_ready);
}


extern ESOCK_Type POLLABLE_What(POLLABLE poll)
{
    assert(poll);
    return ((SOCK) poll)->type;
}


extern POLLABLE POLLABLE_FromTRIGGER(TRIGGER trigger)
{
    assert(!trigger  ||  trigger->type == eSOCK_Trigger);
    return (POLLABLE) trigger;
}


extern POLLABLE POLLABLE_FromLSOCK(LSOCK lsock)
{
    assert(!lsock  ||  lsock->type == eSOCK_Listening);
    return (POLLABLE) lsock;
}


extern POLLABLE POLLABLE_FromSOCK(SOCK sock)
{
    assert(!sock  ||  (sock->type & eSOCK_Socket));
    return (POLLABLE) sock;
}


extern TRIGGER POLLABLE_ToTRIGGER(POLLABLE poll)
{
    TRIGGER trigger = (TRIGGER) poll;
    return trigger  &&  trigger->type == eSOCK_Trigger ? trigger : 0;
}


extern LSOCK POLLABLE_ToLSOCK(POLLABLE poll)
{
    LSOCK lsock = (LSOCK) poll;
    return lsock  &&  lsock->type == eSOCK_Listening ? lsock : 0;
}


extern SOCK  POLLABLE_ToSOCK(POLLABLE poll)
{
    SOCK sock = (SOCK) poll;
    return sock  &&  (sock->type & eSOCK_Socket) ? sock : 0;
}



/******************************************************************************
 *  BSD-LIKE INTERFACE
 */


extern int SOCK_ntoa(unsigned int host,
                     char*        buf,
                     size_t       bufsize)
{
    if (buf  &&  bufsize) {
        char x_buf[16/*sizeof("255.255.255.255")*/];
        const unsigned char* b = (const unsigned char*) &host;
        int len = sprintf(x_buf, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        assert(0 < len  &&  (size_t) len < sizeof(x_buf));
        if ((size_t) len < bufsize) {
            memcpy(buf, x_buf, (size_t) len + 1);
            return 0/*success*/;
        }
        *buf = '\0';
    }
    return -1/*failed*/;
}


extern int/*bool*/ SOCK_isipEx(const char* str,
                               int/*bool*/ fullquad)
{
    unsigned long val;
    int dots;

    if (!str  ||  !*str)
        return 0/*false*/;

    dots = 0;
    for (;;) {
        char* end;
        if (!isdigit((unsigned char)(*str)))
            return 0/*false*/;
        errno = 0;
        val = strtoul(str, &end, 0);
        if (errno  ||  str == end)
            return 0/*false*/;
        if (fullquad  &&  *str == '0'  &&  (size_t)(end - str) > 1)
            return 0/*false*/;
        str = end;
        if (*str != '.')
            break;
        if (++dots > 3)
            return 0/*false*/;
        if (val > 255)
            return 0/*false*/;
        ++str;
    }

    return !*str  &&
        (!fullquad  ||  dots == 3)  &&  val <= (0xFFFFFFFFUL >> (dots << 3));
}


extern int/*bool*/ SOCK_isip(const char* str)
{
    return SOCK_isipEx(str, 0/*nofullquad*/);
}


extern int/*bool*/ SOCK_isip6(const char* str)
{
    TNCBI_IPv6Addr temp;
    const char*    end;
    if (!str  ||  isspace((unsigned char)(*str)))
        return 0;
    end = NcbiStringToIPv6(&temp, str, 0);
    return end  &&  !*end;
}


extern int/*bool*/ SOCK_IsAddress(const char* str)
{
    return SOCK_isip(str)  ||  SOCK_isip6(str);
}


extern unsigned int SOCK_HostToNetLong(unsigned int value)
{
    return htonl(value);
}


extern unsigned short SOCK_HostToNetShort(unsigned short value)
{
    return htons(value);
}


extern unsigned int SOCK_htonl(unsigned int value)
{
    return htonl(value);
}


extern unsigned short SOCK_htons(unsigned short value)
{
    return htons(value);
}


extern int SOCK_gethostnameEx(char*   buf,
                              size_t  bufsize,
                              ESwitch log)
{
    if (!buf  ||  !bufsize)
        return -1/*failure*/;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success) {
        buf[0] = '\0';
        return -1/*failure*/;
    }

    return s_gethostname(buf, bufsize, log == eDefault ? s_Log : log);
}


extern int SOCK_gethostname(char*  buf,
                            size_t bufsize)
{
    return SOCK_gethostnameEx(buf, bufsize, s_Log);
}


extern unsigned int SOCK_gethostbynameEx(const char* host,
                                         ESwitch     log)
{
    TNCBI_IPv6Addr addr;

    /* initialize internals */
    if (s_IPVersion == AF_INET6  ||  s_InitAPI(0) != eIO_Success)
        return 0;

    return s_gethostbyname(&addr, host, AF_INET, 0, log == eDefault ? s_Log : log)
        ? NcbiIPv6ToIPv4(&addr, 0)
        : 0;
}


extern unsigned int SOCK_gethostbyname(const char* host)
{
    return SOCK_gethostbynameEx(host, s_Log);
}


extern TNCBI_IPv6Addr* SOCK_gethostbynameEx6(TNCBI_IPv6Addr* addr,
                                             const char*     host,
                                             ESwitch         log)
{
    /* initialize internals */
    if (!addr  ||  s_InitAPI(0) != eIO_Success)
        return 0;

    return s_gethostbyname(addr, host, s_IPVersion, 0, log == eDefault ? s_Log : log);
}


extern TNCBI_IPv6Addr* SOCK_gethostbyname6(TNCBI_IPv6Addr* addr,
                                           const char*     host)
{
    return SOCK_gethostbynameEx6(addr, host, s_Log);
}


extern const char* SOCK_gethostbyaddrEx(unsigned int host,
                                        char*        buf,
                                        size_t       bufsize,
                                        ESwitch      log)
{
    TNCBI_IPv6Addr addr;

    if (!buf  ||  !bufsize)
        return 0;

    /* initialize internals */
    if (s_IPVersion == AF_INET6  ||  s_InitAPI(0) != eIO_Success) {
        *buf = '\0';
        return 0;
    }

    NcbiIPv4ToIPv6(&addr, host, 0);
    return s_gethostbyaddr(&addr, buf, bufsize, log == eDefault ? s_Log : log);
}


extern const char* SOCK_gethostbyaddr(unsigned int host,
                                      char*        buf,
                                      size_t       bufsize)
{
    return SOCK_gethostbyaddrEx(host, buf, bufsize, s_Log);
}


extern const char* SOCK_gethostbyaddrEx6(const TNCBI_IPv6Addr* addr,
                                         char*                 buf,
                                         size_t                bufsize,
                                         ESwitch               log)
{
    if (!buf  ||  !bufsize)
        return 0;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success) {
        *buf = '\0';
        return 0;
    }

    return s_gethostbyaddr(addr, buf, bufsize, log == eDefault ? s_Log : log);
}


extern const char* SOCK_gethostbyaddr6(const TNCBI_IPv6Addr* addr,
                                       char*                 buf,
                                       size_t                bufsize)
{
    return SOCK_gethostbyaddrEx6(addr, buf, bufsize, s_Log);
}


extern unsigned int SOCK_GetLocalHostAddress(ESwitch reget)
{
    TNCBI_IPv6Addr addr;

    /* initialize internals */
    if (s_IPVersion == AF_INET6  ||  s_InitAPI(0) != eIO_Success)
        return 0;

    return s_getlocalhostaddress(&addr, AF_INET, reget, s_Log)
        ? NcbiIPv6ToIPv4(&addr, 0)
        : 0;
}


extern TNCBI_IPv6Addr* SOCK_GetLocalHostAddress6(TNCBI_IPv6Addr* addr,
                                                 ESwitch         reget)
{
    /* initialize internals */
    if (!addr  ||  s_InitAPI(0) != eIO_Success)
        return 0;

    return s_getlocalhostaddress(addr, s_IPVersion, reget, s_Log);
}


extern unsigned int SOCK_GetLoopbackAddress(void)
{
    return s_IPVersion == AF_INET6 ? 0 : SOCK_LOOPBACK;
}


extern TNCBI_IPv6Addr* SOCK_GetLoopbackAddress6(TNCBI_IPv6Addr* loop)
{
    if (loop) {
        if (s_IPVersion != AF_INET) {
            memset(loop->octet, 0, sizeof(loop->octet) - 1);
            loop->octet[sizeof(loop->octet) - 1]  = 1;
        } else
            NcbiIPv4ToIPv6(loop, SOCK_LOOPBACK, 0);
    }
    return loop;
}


extern int/*bool*/ SOCK_IsLoopbackAddress(unsigned int ip)
{
    if (ip == SOCK_LOOPBACK)
        return 1/*true*/;
    /* 127/8 */
    if (ip) {
        unsigned int addr = ntohl(ip);
#if defined(IN_CLASSA) && defined(IN_CLASSA_NET) && defined(IN_CLASSA_NSHIFT)
        return IN_CLASSA(addr)
            &&  (addr & IN_CLASSA_NET) == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT);
#else
        return !((addr & 0xFF000000) ^ (INADDR_LOOPBACK-1));
#endif /*IN_CLASSA && IN_CLASSA_NET && IN_CLASSA_NSHIFT*/
    }
    return 0/*false*/;
}


extern int/*bool*/ SOCK_IsLoopbackAddress6(const TNCBI_IPv6Addr* addr)
{
    if (!addr)
        return 0/*false*/;
    if (NcbiIsIPv4(addr))
        return SOCK_IsLoopbackAddress(NcbiIPv6ToIPv4(addr, 0));
    if (addr->octet[sizeof(addr->octet) - 1] != 1)
        return 0/*false*/;
    return !memcchr(addr->octet, 0, sizeof(addr->octet) - 1);
}


const char* SOCK_StringToHostPortEx(const char*     str,
                                    unsigned int*   host,
                                    unsigned short* port,
                                    int/*bool*/     flag)
{
    TNCBI_IPv6Addr addr;
    const char* rv = s_StringToHostPort(str, &addr, AF_INET, port, flag);
    if ( host )
        *host = NcbiIPv6ToIPv4(&addr, 0);
    return rv;
}


extern const char* SOCK_StringToHostPort(const char*     str,
                                         unsigned int*   host,
                                         unsigned short* port)
{
    return SOCK_StringToHostPortEx(str, host, port, 0/*false*/);
}


extern const char* SOCK_StringToHostPort6(const char*     str,
                                          TNCBI_IPv6Addr* addr,
                                          unsigned short* port)
{
    return s_StringToHostPort(str, addr, s_IPVersion, port, 0/*false*/);
}


extern size_t SOCK_HostPortToString(unsigned int   host,
                                    unsigned short port,
                                    char*          buf,
                                    size_t         size)
{
    TNCBI_IPv6Addr addr;
    NcbiIPv4ToIPv6(&addr, host, 0);
    return s_HostPortToStringEx(&addr, AF_INET, port, buf, size);
}


extern size_t SOCK_HostPortToString6(const TNCBI_IPv6Addr* addr,
                                     unsigned short        port,
                                     char*                 buf,
                                     size_t                size)
{
    return s_HostPortToString(addr, port, buf, size);
}



/******************************************************************************
 *  SECURE SOCKET LAYER SUPPORT
 */


void SOCK_SetupSSLInternal(FSSLSetup   setup,
                           int/*bool*/ init)
{
    TCORE_Set x_set = eCORE_SetSSL;
    CORE_LOCK_WRITE;

    if (setup  ||  init) {
        if (s_SSLSetup  &&  s_SSLSetup != setup) {
            const char* mess;
            if (s_SSL)
                mess = "Cannot reset SSL while it is in use";
            else if (!init)
                mess = (const char*)(-1L)/*warn multiple*/;
            else if (setup)
                mess = "Conflicting SSL auto-setup";
            else
                mess = 0/*just ignore downgrade*/;
            CORE_UNLOCK;

            if (mess) {
                CORE_LOG_X(164,
                           mess == (const char*)(-1L)
                           ? eLOG_Warning : eLOG_Critical,
                           mess == (const char*)(-1L)
                           ? "Conflicting SSL setup ignored"
                           : mess);
            }
            return;
        }
        if (!(s_SSLSetup = s_Initialized < 0 ? 0 : setup))
            x_set = 0;
    } else
        x_ShutdownSSL();

    g_CORE_Set |= x_set;
    CORE_UNLOCK;
}


extern void SOCK_SetupSSL(FSSLSetup setup)
{
    SOCK_SetupSSLInternal(setup, 0/*false*/);
}


EIO_Status SOCK_SetupSSLInternalEx(FSSLSetup   setup,
                                   int/*bool*/ init)
{
    SOCK_SetupSSLInternal(setup, init);
    return setup ? s_InitAPI(1/*secure*/) : eIO_Success;
}


extern EIO_Status SOCK_SetupSSLEx(FSSLSetup setup)
{
    return SOCK_SetupSSLInternalEx(setup, 0/*false*/);
}


extern const char* SOCK_SSLName(void)
{
    return !s_SSLSetup ? 0 : !s_SSL ? "" : s_SSL->Name;
}



#ifdef NCBI_OS_MSWIN

/*ARGSUSED*/
extern int gettimeofday(struct timeval* tp, void* unused)
{
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        tp->tv_sec  = (long) ts.tv_sec;
        tp->tv_usec = (long)(ts.tv_nsec / 1000);
    } else {
        /* Seconds since January 1, 1601 (UTC) to January 1, 1970 (UTC) */
        static const ULONGLONG EPOCH = 11644473600L;
        /* The number of 100-nanosecond intervals since January 1, 1601 (UTC) */
        FILETIME       systime;
        ULARGE_INTEGER time;

#  if _WIN32_WINNT >= _WIN32_WINNT_WIN8
        GetSystemTimePreciseAsFileTime(&systime);
#  else
        GetSystemTimeAsFileTime(&systime);
#  endif

        time.LowPart  = systime.dwLowDateTime;
        time.HighPart = systime.dwHighDateTime;

        tp->tv_sec  = (long)(time.QuadPart / 10000000 - EPOCH);
        tp->tv_usec = (long)(time.QuadPart % 10000000) / 10;
    }
    return 0;
}

#endif /*NCBI_OS_MSWIN*/
