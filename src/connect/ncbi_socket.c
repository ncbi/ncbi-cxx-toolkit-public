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

/* Uncomment this (or specify "-DHAVE_GETHOSTBY***_R=") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "gethostbyname_r()" but not "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
 */

/*   Solaris: */
/* #define HAVE_GETHOSTBYNAME_R 5 */
/* #define HAVE_GETHOSTBYADDR_R 7 */

/*   Linux, IRIX: */
/* #define HAVE_GETHOSTBYNAME_R 6 */
/* #define HAVE_GETHOSTBYADDR_R 8 */

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
#include "ncbi_priv.h"
#ifdef NCBI_CXX_TOOLKIT
#  include <corelib/ncbiatomic.h>
#endif /*NCBI_CXX_TOOLKIT*/
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
#      define IP_MTU  14
#    endif /*!IP_MTU*/
#  endif /*NCBI_OS_LINUX*/
#  if !defined(NCBI_OS_BEOS)
#    include <arpa/inet.h>
#  endif /*NCBI_OS_BEOS*/
#  include <signal.h>
#  include <sys/param.h>
#  ifdef HAVE_POLL_H
#    include <sys/poll.h>
#  endif /*HAVE_POLL_H*/
#  include <sys/stat.h>
#  include <sys/un.h>
#  include <unistd.h>
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
#  if IN_CLASSA_MAX <= IN_LOOPBACKNET
#    error "IN_LOOPBACKNET is out of range"
#  endif /*IN_CLASSA_MAX<=IN_LOOPBACKNET*/
#endif /*IN_CLASSA_MAX*/

#ifndef   MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN  255
#endif /* MAXHOSTNAMELEN */



/******************************************************************************
 *  TYPEDEFS & MACROS
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE  4096

/* Macros for platform-dependent constants, error codes and functions
 */
#if   defined(NCBI_OS_MSWIN)

#  define SOCK_GHB_THREAD_SAFE  1  /* for gethostby...() */
#  define SOCK_INVALID          INVALID_SOCKET
#  define SOCK_ERRNO            WSAGetLastError()
#  define SOCK_NFDS(s)          0
#  define SOCK_CLOSE(s)         closesocket(s)
#  define SOCK_EVENTS           (FD_CLOSE|FD_CONNECT|FD_OOB|FD_WRITE|FD_READ)
/* NCBI_OS_MSWIN */

#elif defined(NCBI_OS_UNIX)

#  define SOCK_INVALID          (-1)
#  define SOCK_ERRNO            errno
#  define SOCK_NFDS(s)          ((s) + 1)
#  ifdef NCBI_OS_BEOS
#    define SOCK_CLOSE(s)       closesocket(s)
#  else
#    define SOCK_CLOSE(s)       close(s)
#  endif /*NCBI_OS_BEOS*/
#  ifndef   INADDR_NONE
#    define INADDR_NONE         ((unsigned int)(~0UL))
#  endif  /*INADDR_NONE*/
/* NCBI_OS_UNIX */

#endif /*NCBI_OS*/

#ifdef   sun
#  undef sun
#endif

#define SESSION_INVALID       ((void*)(-1L))

#define MAXIDLEN              80
#if MAXIDLEN > SOCK_BUF_CHUNK_SIZE
#  error "SOCK_BUF_CHUNK_SIZE too small"
#endif /*MAXIDLEN<SOCK_BUF_CHUNK_SIZE*/

#define SOCK_STRERROR(err)    s_StrError(0, (err))

#define SOCK_LOOPBACK         (assert(INADDR_LOOPBACK), htonl(INADDR_LOOPBACK))

#define _SOCK_CATENATE(x, y)  x##y

#define SOCK_GET_TIMEOUT(s, t)                                          \
    ((s)->_SOCK_CATENATE(t,_tv_set) ? &(s)->_SOCK_CATENATE(t,_tv) : 0)

#define SOCK_SET_TIMEOUT(s, t, v)                                       \
    (((s)->_SOCK_CATENATE(t,_tv_set) = (v) ? 1 : 0)                     \
     ? (void)((s)->_SOCK_CATENATE(t,_tv) = *(v)) : (void) 0)

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
static int/*bool*/ s_Initialized = 0/*-1=deinited;0=uninited;1=inited*/;

/* Which wait API to use, UNIX only */
static ESOCK_IOWaitSysAPI s_IOWaitSysAPI = eSOCK_IOWaitSysAPIAuto;

/* SOCK counter */
static unsigned int s_ID_Counter = 0;

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

/* SSL support */
static SOCKSSL   s_SSL;
static FSSLSetup s_SSLSetup;

/* Error reporting */
static FSOCK_ErrHook s_ErrHook;
static void*         s_ErrData;



/******************************************************************************
 *  ERROR REPORTING
 */


#define NCBI_INCLUDE_STRERROR_C
#include "ncbi_strerror.c"


static const char* s_StrError(SOCK sock, int error)
{
    if (!error)
        return 0;

    if (sock) {
        FSSLError sslerror = s_SSL ? s_SSL->Error : 0;
        if (sslerror) {
            const char* strerr = sslerror(sock->session == SESSION_INVALID
                                          ? 0 : sock->session, error);
            if (strerr  &&  *strerr)
                return MSWIN_STRDUP(strerr);
        }
    }
    return s_StrErrorInternal(error);
}


#ifdef NCBI_OS_MSWIN
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


extern void SOCK_SetErrHook(FSOCK_ErrHook hook, void* data)
{
    CORE_LOCK_WRITE;
    s_ErrData = data;
    s_ErrHook = hook;
    CORE_UNLOCK;
}



/******************************************************************************
 *  DATA LOGGING
 */


static const char* s_CP(unsigned int host, unsigned short port,
                        const char* path, char* buf, size_t bufsize)
{
    if (path[0])
        return path;
    SOCK_HostPortToString(host, port, buf, bufsize);
    return buf;
}


static const char* s_ID(const SOCK sock, char buf[MAXIDLEN])
{
    const char* sname;
    const char* cp;
    char addr[40];
    int n;

    if (!sock)
        return "";
    switch (sock->type) {
    case eTrigger:
        cp = "";
        sname = "TRIGGER";
        break;
    case eSocket:
        cp = s_CP(sock->host, sock->port,
#ifdef NCBI_OS_UNIX
                  sock->path,
#else
                  "",
#endif /*NCBI_OS_UNIX*/
                  addr, sizeof(addr));
#ifdef NCBI_OS_UNIX
        if (sock->path[0])
            sname = sock->session ? "SUSOCK" : "USOCK";
        else
#endif /*NCBI_OS_UNIX*/
            sname = sock->session ? "SSOCK"  : "SOCK";
        break;
    case eDatagram:
        sname = "DSOCK";
        addr[0] = '\0';
        n = sock->myport ? sprintf(addr, "(:%hu)", sock->myport) : 0;
        if (sock->host  ||  sock->port) {
            SOCK_HostPortToString(sock->host, sock->port,
                                  addr + n, sizeof(addr) - n);
        }
        cp = addr;
        break;
    case eListening:
#ifdef NCBI_OS_UNIX
        if (!sock->myport)
            cp = ((LSOCK) sock)->path;
        else
#endif /*NCBI_OS_UNIX*/
        {
            sprintf(addr, ":%hu", sock->myport);
            cp = addr;
        }
        sname = "LSOCK";
        break;
    default:
        cp = "";
        sname = "?";
        assert(0);
        break;
    }
    if (sock->sock != SOCK_INVALID) {
        size_t len = cp  &&  *cp ? strlen(cp) : 0;
        n = (int)(len > sizeof(addr) - 1 ? sizeof(addr) - 1 : len);
        sprintf(buf, "%s#%u[%u]%s%s%.*s: ",
                sname, sock->id, (unsigned int) sock->sock,
                &"@"[!n], (size_t) n < len ? "..." : "", n, cp + len - n);
    } else
        sprintf(buf, "%s#%u[?]: ",  sname, sock->id);
    return buf;
}


static unsigned short s_GetLocalPort(TSOCK_Handle fd)
{
    struct sockaddr_in sin;
    TSOCK_socklen_t sinlen = (TSOCK_socklen_t) sizeof(sin);
    memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SIN_LEN
    sin.sin_len = sinlen;
#endif /*HAVE_SIN_LEN*/
    if (getsockname(fd, (struct sockaddr*) &sin, &sinlen) == 0
        &&  sin.sin_family == AF_INET) {
        return ntohs(sin.sin_port);
    }
    return 0;
}


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLog(ELOG_Level  level, const SOCK sock, EIO_Event   event,
                    const void* data,  size_t     size, const void* ptr)
{
    const struct sockaddr_in* sin;
    char _id[MAXIDLEN];
    const char* what;
    char head[128];
    char tail[128];
    int n;

    if (!CORE_GetLOG())
        return;

    assert(sock  &&  (sock->type & eSocket));
    switch (event) {
    case eIO_Open:
        if (sock->type != eDatagram) {
            unsigned short port;
            if (sock->side == eSOCK_Client) {
                strcpy(head, ptr ? "Connected" : "Connecting");
                port = sock->myport;
            } else if (ptr) {
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
                    assert(sock->side == eSOCK_Client  ||  !ptr);
                    sock->myport = port;
                }
            } else
                *tail = '\0';
        } else if (!(sin = (const struct sockaddr_in*) ptr)) {
            strcpy(head, "Created");
            *tail = '\0';
        } else if (!data) {
            strcpy(head, "Bound @");
            sprintf(tail, "(:%hu)", ntohs(sin->sin_port));
        } else if (sin->sin_family == AF_INET) {
            strcpy(head, "Associated with ");
            SOCK_HostPortToString(sin->sin_addr.s_addr,
                                  ntohs(sin->sin_port),
                                  tail, sizeof(tail));
        } else {
            strcpy(head, "Disassociated");
            *tail = '\0';
        }
        CORE_LOGF_X(112, level,
                    ("%s%s%s", s_ID(sock, _id), head, tail));
        break;

    case eIO_Read:
    case eIO_Write:
        {{
            const char* strerr = NULL;
            what = (event == eIO_Read
                    ? (sock->type != eDatagram  &&  !size
                       ? (data
                          ? (strerr = s_StrError(sock, *((int*) data)))
                          : "EOF hit")
                       : "Read")
                    : (sock->type != eDatagram  &&  !size
                       ? (strerr = s_StrError(sock, *((int*) data)))
                       : "Written"));

            n = (int) strlen(what);
            while (n  &&  isspace((unsigned char) what[n - 1]))
                n--;
            if (n > 1  &&  what[n - 1] == '.')
                n--;
            if (sock->type == eDatagram) {
                sin = (const struct sockaddr_in*) ptr;
                assert(sin  &&  sin->sin_family == AF_INET);
                SOCK_HostPortToString(sin->sin_addr.s_addr,
                                      ntohs(sin->sin_port),
                                      head, sizeof(head));
                sprintf(tail, ", msg# %" NCBI_BIGCOUNT_FORMAT_SPEC,
                        event == eIO_Read ? sock->n_in : sock->n_out);
            } else if (!ptr  ||  !*((char*) ptr)) {
                sprintf(head, " at offset %" NCBI_BIGCOUNT_FORMAT_SPEC,
                        event == eIO_Read ? sock->n_read : sock->n_written);
                strcpy(tail, ptr ? " [OOB]" : "");
            } else {
                strncpy0(head, (const char*) ptr, sizeof(head));
                *tail = '\0';
            }

            CORE_DATAF_X(109, level, data, size,
                         ("%s%.*s%s%s%s", s_ID(sock, _id), n, what,
                          sock->type == eDatagram
                          ? (event == eIO_Read ? " from " : " to ")
                          : !size
                          ? (event == eIO_Read
                             ? " while reading" : " while writing")
                          : "",
                          head, tail));

            UTIL_ReleaseBuffer(strerr);
        }}
        break;

    case eIO_Close:
        n = sprintf(head, "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s",
                    sock->n_written, &"s"[sock->n_written == 1]);
        if (sock->type == eDatagram  ||
            sock->n_out != sock->n_written) {
            sprintf(head + n, "/%" NCBI_BIGCOUNT_FORMAT_SPEC " %s%s",
                    sock->n_out,
                    sock->type == eDatagram ? "msg" : "total byte",
                    &"s"[sock->n_out == 1]);
        }
        n = sprintf(tail, "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s",
                    sock->n_read, &"s"[sock->n_read == 1]);
        if (sock->type == eDatagram  ||
            sock->n_in != sock->n_read) {
            sprintf(tail + n, "/%" NCBI_BIGCOUNT_FORMAT_SPEC " %s%s",
                    sock->n_in,
                    sock->type == eDatagram ? "msg" : "total byte",
                    &"s"[sock->n_in == 1]);
        }
        CORE_LOGF_X(113, level,
                    ("%s%s (out: %s, in: %s)", s_ID(sock, _id),
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
 *  API INITIALIZATION, SHUTDOWN/CLEANUP, and UTILITY
 */


#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  if !defined(__GNUC__)  &&  !defined(offsetof)
#    define offsetof(T, F)  ((size_t)((char*) &(((T*) 0)->F) - (char*) 0))
#  endif
#endif /*_DEBUG && !NDEBUG*/

#if defined(_DEBUG)  &&  !defined(NDEBUG)

#  ifndef   SOCK_HAVE_SHOWDATALAYOUT
#    define SOCK_HAVE_SHOWDATALAYOUT 1
#  endif

#endif /*__GNUC__ && _DEBUG && !NDEBUG*/

#ifdef SOCK_HAVE_SHOWDATALAYOUT

#  define   extentof(T, F)  (sizeof(((T*) 0)->F))

#  define   infof(T, F)     (unsigned int) offsetof(T, F), \
                            (unsigned int) extentof(T, F)

static void s_ShowDataLayout(void)
{
    static const char kLayoutFormat[] = {
        "SOCK data layout:\n"
        "    Sizeof(TRIGGER_struct) = %u\n"
        "    Sizeof(LSOCK_struct) = %u\n"
        "    Sizeof(SOCK_struct) = %u, offsets (sizes) follow\n"
        "\tsock:      %3u (%u)\n"
        "\tid:        %3u (%u)\n"
        "\tisset:     %3u (%u)\n"
        "\thost:      %3u (%u)\n"
        "\tport:      %3u (%u)\n"
        "\tmyport:    %3u (%u)\n"
        "\tbitfield:      (4)\n"
#  ifdef NCBI_OS_MSWIN
        "\tevent:     %3u (%u)\n"
#  endif /*NCBI_OS_MSWIN*/
        "\tsession:   %3u (%u)\n"
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
        infof(SOCK_struct,    host),                \
        infof(SOCK_struct,    port),                \
        infof(SOCK_struct,    myport),              \
        infof(SOCK_struct,    event),               \
        infof(SOCK_struct,    session),             \
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
        infof(SOCK_struct,    host),                \
        infof(SOCK_struct,    port),                \
        infof(SOCK_struct,    myport),              \
        infof(SOCK_struct,    session),             \
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
    CORE_LOGF_X(2, eLOG_Note,
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
    CORE_TRACE("[SOCK::InitializeAPI]  Begin");

    CORE_LOCK_WRITE;

    if (s_Initialized) {
        CORE_UNLOCK;
        CORE_TRACE("[SOCK::InitializeAPI]  Noop");
        return s_Initialized < 0 ? eIO_NotSupported : eIO_Success;
    }

#ifdef SOCK_HAVE_SHOWDATALAYOUT
    if (s_Log == eOn)
        s_ShowDataLayout();
#endif /*SOCK_HAVE_SHOWDATALAYOUT*/

#if defined(_DEBUG)  &&  !defined(NDEBUG)
    /* Layout / alignment sanity check */
    assert(sizeof(TRIGGER_Handle)         == sizeof(TSOCK_Handle));
    assert(offsetof(SOCK_struct, session) == offsetof(LSOCK_struct, context));
#  ifdef NCBI_OS_MSWIN
    assert(offsetof(SOCK_struct, event)   == offsetof(LSOCK_struct, event));
#  endif /*NCBI_OS_MSWIN*/
#endif /*_DEBUG && !NDEBUG*/

#if defined(NCBI_OS_MSWIN)
    {{
        WSADATA wsadata;
        int x_error = WSAStartup(MAKEWORD(1,1), &wsadata);
        if (x_error) {
            const char* strerr;

            CORE_UNLOCK;
            strerr = SOCK_STRERROR(x_error);
            CORE_LOG_ERRNO_EXX(3, eLOG_Error,
                               x_error, strerr,
                               "[SOCK::InitializeAPI] "
                               " Failed WSAStartup()");
            UTIL_ReleaseBuffer(strerr);
            return eIO_NotSupported;
        }
    }}
#elif defined(NCBI_OS_UNIX)
    if (!s_AllowSigPipe) {
        struct sigaction sa;
        if (sigaction(SIGPIPE, 0, &sa) != 0  ||  sa.sa_handler == SIG_DFL) {
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_IGN;
            sigaction(SIGPIPE, &sa, 0);
        }
    }
#endif /*platform-specific init*/

    s_Initialized = 1/*inited*/;
#ifndef NCBI_OS_MSWIN
    {{
        static int/*bool*/ s_AtExitSet = 0;
        if (!s_AtExitSet) {
            atexit((void (*)(void)) SOCK_ShutdownAPI);
            s_AtExitSet = 1;
        }
    }}
#endif

    CORE_UNLOCK;
    CORE_TRACE("[SOCK::InitializeAPI]  End");
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


static EIO_Status s_InitAPI_(int secure)
{
    static const struct SOCKSSL_struct kNoSSL = { 0 };
    EIO_Status status = eIO_Success;

    if (!s_Initialized  &&  (status = s_Init()) != eIO_Success)
        return status;

    assert(s_Initialized);

    if (s_Initialized < 0)
        return eIO_NotSupported;

    if (secure  &&  !s_SSL) {
        if (s_SSLSetup) {
            SOCKSSL ssl = s_SSLSetup();
            if (ssl  &&  ssl->Init) {
                CORE_LOCK_WRITE;
                if (!s_SSL) {
                    s_SSL = ((status = ssl->Init(s_Recv,s_Send)) == eIO_Success
                             ? ssl : &kNoSSL);
                }
                CORE_UNLOCK;
            } else
                status = eIO_NotSupported;
        } else {
#ifdef HAVE_LIBGNUTLS
            static int once = 1/*true*/;
            if (once) {
                once = 0/*false*/;
                CORE_LOG(eLOG_Critical, "Secure socket layer (GNUTLS) has not"
                         " been properly initialized in the NCBI toolkit. "
                         " Have you forgotten to call SOCK_SetupSSL()?");
            }
#endif /*HAVE_LIBGNUTLS*/
            status = eIO_NotSupported;
        }
    }
    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static EIO_Status s_InitAPI(int secure)
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

    CORE_TRACE("[SOCK::ShutdownAPI]  Begin");

    CORE_LOCK_WRITE;

    if (s_Initialized <= 0) {
        CORE_UNLOCK;
        return eIO_Success;
    }
    s_Initialized = -1/*deinited*/;

    if (s_SSL) {
        FSSLExit sslexit = s_SSL->Exit;
        s_SSLSetup = 0;
        s_SSL      = 0;
        if (sslexit)
            sslexit();
    }

#ifdef NCBI_OS_MSWIN
    {{
        int x_error = WSACleanup() ? SOCK_ERRNO : 0;
        CORE_UNLOCK;
        if (x_error) {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOG_ERRNO_EXX(4, eLOG_Warning,
                               x_error, strerr,
                               "[SOCK::ShutdownAPI] "
                               " Failed WSACleanup()");
            UTIL_ReleaseBuffer(strerr);
            return eIO_NotSupported;
        }
    }}
#else
    CORE_UNLOCK;
#endif /*NCBI_OS_MSWIN*/

    CORE_TRACE("[SOCK::ShutdownAPI]  End");
    return eIO_Success;
}


extern void SOCK_AllowSigPipeAPI(void)
{
#ifdef NCBI_OS_UNIX
    s_AllowSigPipe = 1/*true - API will not mask SIGPIPE out at init*/;
#endif /*NCBI_OS_UNIX*/
    return;
}


extern size_t SOCK_OSHandleSize(void)
{
    return sizeof(TSOCK_Handle);
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
#if !defined(NCBI_OS_UNIX)  ||  !defined(HAVE_POLL_H)
    if (api == eSOCK_IOWaitSysAPIPoll) {
        CORE_LOG_X(149, eLOG_Critical, "[SOCK::SetIOWaitSysAPI] "
                   " Poll API requested but not supported on this platform");
    } else
#endif /*!NCBI_OS_UNIX || !HAVE_POLL_H*/
        s_IOWaitSysAPI = api;
    return retval;
}



/******************************************************************************
 *  gethost...() WRAPPERS
 */


static int s_gethostname(char* name, size_t namelen, ESwitch log)
{
    int/*bool*/ error;

    CORE_TRACE("[SOCK::gethostname]");

    name[0] = name[namelen - 1] = '\0';
    if (gethostname(name, (int) namelen) != 0) {
        if (log) {
            int x_error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOG_ERRNO_EXX(103, eLOG_Error,
                               x_error, strerr,
                               "[SOCK_gethostname] "
                               " Failed gethostname()");
            UTIL_ReleaseBuffer(strerr);
        }
        error = 1/*true*/;
    } else if (name[namelen - 1]) {
        if (log) {
            CORE_LOG_X(104, eLOG_Error,
                       "[SOCK_gethostname] "
                       " Buffer too small");
        }
        error = 1/*true*/;
    } else
        error = 0/*false*/;

    CORE_TRACEF(("[SOCK::gethostname] "
                 " \"%.*s\"%s", (int) namelen, name, error ? " (error)" : ""));
    if (error)
        *name = '\0';
    return *name ? 0/*success*/ : -1/*failure*/;
}


static unsigned int s_gethostbyname_(const char* hostname, ESwitch log)
{
    char buf[MAXHOSTNAMELEN + 1];
    unsigned int host;

    if (!hostname  ||  !*hostname) {
        if (s_gethostname(buf, sizeof(buf), log) != 0)
            return 0;
        hostname = buf;
    }

    CORE_TRACEF(("[SOCK::gethostbyname]  \"%s\"", hostname));

    if ((host = inet_addr(hostname)) == htonl(INADDR_NONE)) {
        int x_error;
#if defined(HAVE_GETADDRINFO)  &&  !defined(__GLIBC__)
        struct addrinfo hints, *out = 0;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; /* currently, we only handle IPv4 */
        if ((x_error = getaddrinfo(hostname, 0, &hints, &out)) == 0  &&  out) {
            struct sockaddr_in* sin = (struct sockaddr_in*) out->ai_addr;
            assert(sin->sin_family == AF_INET);
            host = sin->sin_addr.s_addr;
        } else {
            if (log) {
                const char* strerr;
                if (x_error == EAI_SYSTEM)
                    x_error  = SOCK_ERRNO;
                else
                    x_error += EAI_BASE;
                strerr = SOCK_STRERROR(x_error);
                CORE_LOGF_ERRNO_EXX(105, eLOG_Warning,
                                    x_error, strerr,
                                    ("[SOCK_gethostbyname] "
                                     " Failed getaddrinfo(\"%.*s\")",
                                     MAXHOSTNAMELEN, hostname));
                UTIL_ReleaseBuffer(strerr);
            }
            host = 0;
        }
        if (out)
            freeaddrinfo(out);
#else /* use some variant of gethostbyname */
        struct hostent* he;
#  ifdef HAVE_GETHOSTBYNAME_R
        static const char suffix[] = "_r";
        struct hostent x_he;
        char x_buf[1024];

        x_error = 0;
#    if   HAVE_GETHOSTBYNAME_R == 5
        he = gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf), &x_error);
#    elif HAVE_GETHOSTBYNAME_R == 6
        if (gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf),
                            &he, &x_error) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYNAME_R value"
#    endif /*HAVE_GETHOSTNBYNAME_R == N*/
        if (!he) {
            if (!x_error  &&  (x_error = SOCK_ERRNO) == ERANGE)
                log = eOn;
            else
                x_error += DNS_BASE;
        }
#  else
        static const char suffix[] = "";

#    ifndef SOCK_GHB_THREAD_SAFE
        CORE_LOCK_WRITE;
#    endif /*!SOCK_GHB_THREAD_SAFE*/

        he = gethostbyname(hostname);
        x_error = he ? 0 : h_errno + DNS_BASE;
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (!he || he->h_addrtype != AF_INET || he->h_length != sizeof(host)) {
            if (he)
                x_error = EINVAL;
            host = 0;
        } else
            memcpy(&host, he->h_addr, sizeof(host));

#  ifndef HAVE_GETHOSTBYNAME_R
#    ifndef SOCK_GHB_THREAD_SAFE
        CORE_UNLOCK;
#    endif /*!SOCK_GHB_THREAD_SAFE*/
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (!host  &&  log) {
            const char* strerr;
#  ifdef NETDB_INTERNAL
            if (x_error == NETDB_INTERNAL + DNS_BASE)
                x_error  = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(106, eLOG_Warning,
                                x_error, strerr,
                                ("[SOCK_gethostbyname] "
                                 " Failed gethostbyname%s(\"%.*s\")",
                                 suffix, MAXHOSTNAMELEN, hostname));
            UTIL_ReleaseBuffer(strerr);
        }

#endif /*HAVE_GETADDRINFO && !__GLIBC__*/
    }

#if defined(_DEBUG)  &&  !defined(NDEBUG)
    if (!SOCK_isipEx(hostname, 1)  ||  !host) {
        char addr[40];
        CORE_TRACEF(("[SOCK::gethostbyname]  \"%s\" @ %s", hostname,
                     SOCK_ntoa(host, addr, sizeof(addr)) == 0
                     ? addr : sprintf(addr, "0x%08X",
                                      (unsigned int) ntohl(host))
                     ? addr : "(unknown)"));
    }
#endif /*_DEBUG && !NDEBUG*/
    return host;
}


/* a non-standard helper */
static unsigned int s_getlocalhostaddress(ESwitch reget, ESwitch log)
{
    static int once = 1/*true*/;
    /* cached IP address of the local host */
    static unsigned int s_LocalHostAddress = 0;
    if (reget == eOn  ||  (!s_LocalHostAddress  &&  reget != eOff))
        s_LocalHostAddress = s_gethostbyname_(0, log);
    if (s_LocalHostAddress)
        return s_LocalHostAddress;
    if (once  &&  reget != eOff) {
        once = 0/*false*/;
        CORE_LOGF_X(9, reget == eDefault ? eLOG_Warning : eLOG_Error,
                    ("[SOCK::GetLocalHostAddress] "
                     " Cannot obtain local host address%s",
                     reget == eDefault ? ", using loopback instead" : ""));
    }
    return reget == eDefault ? SOCK_LOOPBACK : 0;
}


static unsigned int s_gethostbyname(const char* hostname, ESwitch log)
{
    static int once = 1/*true*/;
    unsigned int retval = s_gethostbyname_(hostname, log);

    if (!retval) {
        if (s_ErrHook) {
            SSOCK_ErrInfo info;
            memset(&info, 0, sizeof(info));
            info.type = eSOCK_ErrDns;
            info.host = hostname;
            s_ErrorCallback(&info);
        }
    } else if (once  &&  !hostname  &&  SOCK_IsLoopbackAddress(retval)) {
        char addr[40 + 1];
        once = 0/*false*/;
        *addr = " "[SOCK_ntoa(retval, addr + 1, sizeof(addr) - 1) ? 1 : 0];
        CORE_LOGF_X(155, eLOG_Warning,
                    ("[SOCK::gethostbyname] "
                     " Got loopback address%s for local host name", addr));
    }
    return retval;
}


static char* s_gethostbyaddr_(unsigned int host, char* name,
                              size_t namelen, ESwitch log)
{
    char addr[40];

    if (!host)
        host = s_getlocalhostaddress(eDefault, log);

    CORE_TRACEF(("[SOCK::gethostbyaddr]  %s",
                 SOCK_ntoa(host, addr, sizeof(addr)) == 0
                 ? addr : sprintf(addr, "0x%08X", (unsigned int) ntohl(host))
                 ? addr : "(unknown)"));

    if (host) {
        int x_error;
#if defined(HAVE_GETNAMEINFO)  &&  !defined(__GLIBC__)
        struct sockaddr_in sin;

        memset(&sin, 0, sizeof(sin));
#  ifdef HAVE_SIN_LEN
        sin.sin_len = (TSOCK_socklen_t) sizeof(sin);
#  endif /*HAVE_SIN_LEN*/
        sin.sin_family      = AF_INET; /* we only handle IPv4 currently */
        sin.sin_addr.s_addr = host;
        if ((x_error = getnameinfo((struct sockaddr*) &sin, sizeof(sin),
                                   name, namelen, 0, 0, 0)) != 0  ||  !*name) {
            if (SOCK_ntoa(host, name, namelen) != 0) {
                if (x_error == EAI_SYSTEM)
                    x_error  = SOCK_ERRNO;
                else if (x_error)
                    x_error += EAI_BASE;
                else {
#  ifdef ENOSPC
                    x_error  = ENOSPC;
#  else
                    x_error  = ERANGE;
#  endif /*ENOSPC*/
                    log = eOn;
                }
                name[0] = '\0';
                name = 0;
            }
            if (!name  &&  log) {
                const char* strerr;
                if (SOCK_ntoa(host, addr, sizeof(addr)) != 0)
                    sprintf(addr, "0x%08X", (unsigned int) ntohl(host));
                strerr = SOCK_STRERROR(x_error);
                CORE_LOGF_ERRNO_EXX(107, eLOG_Warning,
                                    x_error, strerr,
                                    ("[SOCK_gethostbyaddr] "
                                     " Failed getnameinfo(%s)",
                                     addr));
                UTIL_ReleaseBuffer(strerr);
            }
        }

#else /* use some variant of gethostbyaddr */
        struct hostent* he;
#  ifdef HAVE_GETHOSTBYADDR_R
        static const char suffix[] = "_r";
        struct hostent x_he;
        char x_buf[1024];

        x_error = 0;
#    if   HAVE_GETHOSTBYADDR_R == 7
        he = gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                             x_buf, sizeof(x_buf), &x_error);
#    elif HAVE_GETHOSTBYADDR_R == 8
        if (gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                            x_buf, sizeof(x_buf), &he, &x_error) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYADDR_R value"
#    endif /*HAVE_GETHOSTBYADDR_R == N*/
        if (!he) {
            if (!x_error  &&  (x_error = SOCK_ERRNO) == ERANGE)
                log = eOn;
            else
                x_error += DNS_BASE;
        }
#  else /*HAVE_GETHOSTBYADDR_R*/
        static const char suffix[] = "";

#    ifndef SOCK_GHB_THREAD_SAFE
        CORE_LOCK_WRITE;
#    endif /*!SOCK_GHB_THREAD_SAFE*/

        he = gethostbyaddr((char*) &host, sizeof(host), AF_INET);
        x_error = he ? 0 : h_errno + DNS_BASE;
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!he  ||  strlen(he->h_name) >= namelen) {
            if (he  ||  SOCK_ntoa(host, name, namelen) != 0) {
#ifdef ENOSPC
                x_error = ENOSPC;
#else
                x_error = ERANGE;
#endif /*ENOSPC*/
                name[0] = '\0';
                log = eOn;
                name = 0;
            }
        } else
            strcpy(name, he->h_name);

#  ifndef HAVE_GETHOSTBYADDR_R
#    ifndef SOCK_GHB_THREAD_SAFE
        CORE_UNLOCK;
#    endif /*!SOCK_GHB_THREAD_SAFE*/
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!name  &&  log) {
            const char* strerr;
#  ifdef NETDB_INTERNAL
            if (x_error == NETDB_INTERNAL + DNS_BASE)
                x_error  = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            if (SOCK_ntoa(host, addr, sizeof(addr)) != 0)
                sprintf(addr, "0x%08X", (unsigned int) ntohl(host));
            strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(108, eLOG_Warning,
                                x_error, strerr,
                                ("[SOCK_gethostbyaddr] "
                                 " Failed gethostbyaddr%s(%s)",
                                 suffix, addr));
            UTIL_ReleaseBuffer(strerr);
        }

#endif /*HAVE_GETNAMEINFO && !__GLIBC__*/
    } else {
        name[0] = 0;
        name = 0;
    }

    CORE_TRACEF(("[SOCK::gethostbyaddr]  %s @ %s%s%s",
                 SOCK_ntoa(host, addr, sizeof(addr)) == 0
                 ? addr : sprintf(addr, "0x%08X", (unsigned int) ntohl(host))
                 ? addr : "(unknown)",
                 &"\""[!name], name ? name : "(unknown)", &"\""[!name]));
    return name;
}


static const char* s_gethostbyaddr(unsigned int host, char* name,
                                   size_t namelen, ESwitch log)
{
    static int once = 1/*true*/;
    const char* retval = s_gethostbyaddr_(host, name, namelen, log);

    if (once  &&  retval
        &&  ((SOCK_IsLoopbackAddress(host)
              &&  strncasecmp(retval, "localhost", 9) != 0)  ||
             (!host
              &&  strncasecmp(retval, "localhost", 9) == 0))) {
        once = 0/*false*/;
        CORE_LOGF_X(10, eLOG_Warning,
                    ("[SOCK::gethostbyaddr] "
                     " Got \"%.*s\" for %s address", MAXHOSTNAMELEN,
                     retval, host ? "loopback" : "local host"));
    }
    return retval;
}



/******************************************************************************
 *  STATIC SOCKET HELPERS
 */


/* Switch the specified socket I/O between blocking and non-blocking mode
 */
static int/*bool*/ s_SetNonblock(TSOCK_Handle sock, int/*bool*/ nonblock)
{
#if   defined(NCBI_OS_MSWIN)
    unsigned long arg = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(NCBI_OS_UNIX)
    return fcntl(sock, F_SETFL, nonblock
                 ? fcntl(sock, F_GETFL, 0) |        O_NONBLOCK
                 : fcntl(sock, F_GETFL, 0) & (int) ~O_NONBLOCK) == 0;
#else
#   error "Unsupported platform"
#endif /*NCBI_OS*/
}


/* Set close-on-exec flag
 */
static int/*bool*/ s_SetCloexec(TSOCK_Handle x_sock, int/*bool*/ cloexec)
{
#if   defined(NCBI_OS_UNIX)
    return fcntl(x_sock, F_SETFD, cloexec
                 ? fcntl(x_sock, F_GETFD, 0) |        FD_CLOEXEC
                 : fcntl(x_sock, F_GETFD, 0) & (int) ~FD_CLOEXEC) == 0;
#elif defined(NCBI_OS_MSWIN)
    return SetHandleInformation((HANDLE)x_sock,HANDLE_FLAG_INHERIT,!cloexec);
#else
#   error "Unsupported platform"
#endif /*NCBI_OS*/
}


/*ARGSUSED*/
static int/*bool*/ s_SetReuseAddress(TSOCK_Handle x_sock, int/*bool*/ on_off)
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
    return 1;
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
}


#ifdef SO_KEEPALIVE
static int/*bool*/ s_SetKeepAlive(TSOCK_Handle x_sock, int/*bool*/ on_off)
{
#  ifdef NCBI_OS_MSWIN
    BOOL oobinline = on_off ? TRUE      : FALSE;
#  else
    int  oobinline = on_off ? 1/*true*/ : 0/*false*/;
#  endif /*NCBI_OS_MSWIN*/
    return setsockopt(x_sock, SOL_SOCKET, SO_KEEPALIVE,
                      (char*) &oobinline, sizeof(oobinline)) == 0;
}
#endif /*SO_KEEPALIVE*/


#ifdef SO_OOBINLINE
static int/*bool*/ s_SetOobInline(TSOCK_Handle x_sock, int/*bool*/ on_off)
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


#if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
static int/*bool*/ x_TryLowerSockFileno(SOCK sock)
{
#  ifdef STDERR_FILENO
#    define SOCK_DUPOVER  STDERR_FILENO
#  else
#    define SOCK_DUPOVER  2
#  endif /*STDERR_FILENO*/
    int fd = fcntl(sock->sock, F_DUPFD, SOCK_DUPOVER + 1);
    if (fd >= 0) {
        if (fd < FD_SETSIZE) {
            char _id[MAXIDLEN];
            int cloexec = fcntl(sock->sock, F_GETFD, 0);
            if (cloexec > 0  &&  (cloexec & FD_CLOEXEC))
                fcntl(fd, F_SETFD, cloexec);
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
#endif /*!NCBI_MSWIN && FD_SETSIZE*/


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
/* compare 2 normalized timeval timeouts: "whether *v1 is less than *v2" */
static int/*bool*/ s_IsSmallerTimeout(const struct timeval* v1,
                                      const struct timeval* v2)
{
    if (!v1/*inf*/)
        return 0;
    if (!v2/*inf*/)
        return 1;
    if (v1->tv_sec > v2->tv_sec)
        return 0;
    if (v1->tv_sec < v2->tv_sec)
        return 1;
    return v1->tv_usec < v2->tv_usec;
}


#if !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_CXX_TOOLKIT)


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
        int/*bool*/    bad   = 0/*false*/;
#  ifdef NCBI_OS_MSWIN
        unsigned int   count = 0;
#  endif /*NCBI_OS_MSWIN*/
        struct timeval xx_tv;

        write_only = 1/*true*/;
        read_only = 1/*true*/;
        ready = 0/*false*/;
        FD_ZERO(&efds);
        nfds = 0;

        for (i = 0;  i < n;  i++) {
            EIO_Event    event;
            SOCK         sock;
            ESOCK_Type   type;
            TSOCK_Handle fd;

            if (!(sock = polls[i].sock)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }

            event = polls[i].event;
            if ((event | eIO_ReadWrite) != eIO_ReadWrite) {
                polls[i].revent = eIO_Close;
                if (!bad) {
                    ready = 0/*false*/;
                    bad   = 1/*true*/;
                }
                continue;
            }
            if (!event) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            if (bad)
                continue;

            if ((fd = sock->sock) == SOCK_INVALID) {
                polls[i].revent = eIO_Close;
                ready = 1/*true*/;
                continue;
            }
            if (polls[i].revent) {
                ready = 1/*true*/;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
            }

#  if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
            if (fd >= FD_SETSIZE) {
                if (!x_TryLowerSockFileno(sock)) {
                    /* NB: only once here, as this sets "bad" to "1" */
                    CORE_LOGF_ERRNO_X(145, eLOG_Error, errno,
                                      ("%s[SOCK::Select] "
                                       " Socket file descriptor must "
                                       " be less than %d",
                                       s_ID(sock, _id), FD_SETSIZE));
                    polls[i].revent = eIO_Close;
                    ready = bad = 1/*true*/;
                    continue;
                }
                fd = sock->sock;
                assert(fd < FD_SETSIZE);
            }
#  endif /*!NCBI_OS_MSWIN && FD_SETSIZE*/

            type = (ESOCK_Type) sock->type;
            switch (type & eSocket ? event : event & eIO_Read) {
            case eIO_Write:
            case eIO_ReadWrite:
                assert(type & eSocket);
                if (type == eDatagram  ||  sock->w_status != eIO_Closed) {
                    if (read_only) {
                        FD_ZERO(&wfds);
                        read_only = 0/*false*/;
                    }
                    FD_SET(fd, &wfds);
                }
                if (event == eIO_Write  &&
                    (type == eDatagram  ||  asis
                     ||  (sock->r_on_w == eOff
                          ||  (sock->r_on_w == eDefault
                               &&  s_ReadOnWrite != eOn)))) {
                    break;
                }
                /*FALLTHRU*/

            case eIO_Read:
                if (type != eSocket
                    ||  (sock->r_status != eIO_Closed  &&  !sock->eof)) {
                    if (write_only) {
                        FD_ZERO(&rfds);
                        write_only = 0/*false*/;
                    }
                    FD_SET(fd, &rfds);
                }
                if (type != eSocket  ||  asis  ||  event != eIO_Read
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
                ready = bad = 1/*true*/;
                continue;
            }
            count++;
#  endif /*NCBI_OS_MSWIN*/
        }
        assert(i >= n);

        if (bad) {
            if (ready) {
                errno = SOCK_ETOOMANY;
                return eIO_Unknown;
            } else {
                errno = EINVAL;
                return eIO_InvalidArg;
            }
        }

        if (ready)
            memset(&xx_tv, 0, sizeof(xx_tv));
        else if (tv  &&  s_IsSmallerTimeout(&x_tv, s_SelectTimeout))
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
                if (s_IsSmallerTimeout(s_SelectTimeout, &x_tv)) {
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
            int x_error = SOCK_ERRNO;
            if (x_error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(x_error);
                CORE_LOGF_ERRNO_EXX(5, eLOG_Warning,
                                    x_error, strerr,
                                    ("%s[SOCK::Select] "
                                     " Failed select()",
                                     n == 1 ? s_ID(polls[0].sock, _id) : ""));
                UTIL_ReleaseBuffer(strerr);
                if (!ready)
                    return eIO_Unknown;
            } else if ((n != 1  &&  s_InterruptOnSignal == eOn)  ||
                       (n == 1  &&  (polls[0].sock->i_on_sig == eOn
                                     ||  (polls[0].sock->i_on_sig == eDefault
                                          &&  s_InterruptOnSignal == eOn)))) {
                return eIO_Interrupt;
            } else
                continue;
            assert(x_error != SOCK_EINTR  &&  ready);
        }
        break;
    }

    if (nfds > 0) {
        /* NB: some fd bits could have been counted multiple times if reside
           in different fd_set's (such as ready for both R and W), recount. */
        for (ready = 0, i = 0;  i < n;  i++) {
            SOCK sock = polls[i].sock;
            if (sock  &&  polls[i].event) {
                TSOCK_Handle fd;
                if (polls[i].revent == eIO_Close) {
                    ready++;
                    continue;
                }
                if ((fd = sock->sock) == SOCK_INVALID) {
                    polls[i].revent = eIO_Close;
                    ready++;
                    continue;
                }
#  if !defined(NCBI_OS_MSWIN)  &&  defined(FD_SETSIZE)
                assert(fd < FD_SETSIZE);
#  endif /*!NCBI_OS_MSWIN && FD_SETSIZE*/
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
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                if (polls[i].revent == eIO_Open) {
                    if (!FD_ISSET(fd, &efds))
                        continue;
                    polls[i].revent = eIO_Close;
                } else if (sock->type == eTrigger)
                    polls[i].revent = polls[i].event;
                assert(polls[i].revent != eIO_Open);
                ready++;
            } else
                assert(polls[i].revent == eIO_Open);
        }
    }

    assert(ready);
    /* can do I/O now */
    return eIO_Success;
}


#  if defined(NCBI_OS_UNIX) && !defined(NCBI_OS_DARWIN) && defined(HAVE_POLL_H)


#    define NPOLLS  ((3 * sizeof(fd_set)) / sizeof(struct pollfd))


static size_t s_CountPolls(size_t n, SSOCK_Poll polls[])
{
    int/*bool*/ bigfd = 0/*false*/;
    int/*bool*/ good  = 1/*true*/;
    size_t      count = 0;
    size_t      i;

    for (i = 0;  i < n;  i++) {
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
            &&  (s_IOWaitSysAPI == eSOCK_IOWaitSysAPIPoll
                 ||  !x_TryLowerSockFileno(polls[i].sock))) {
            bigfd = 1/*true*/;
        }
#    endif /*FD_SETSIZE*/
        count++;
    }
    return good  &&  (s_IOWaitSysAPI != eSOCK_IOWaitSysAPIAuto
                      ||  count <= NPOLLS  ||  bigfd) ? count : 0;
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
    if (!(m = s_CountPolls(n, polls)))
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
        int/*bool*/ bad = 0/*false*/;
        int         x_ready;
        int         slice;

        ready = count = 0;
        for (i = 0;  i < n;  i++) {
            short        bitset;
            EIO_Event    event;
            SOCK         sock;
            ESOCK_Type   type;
            TSOCK_Handle fd;

            if (!(sock = polls[i].sock)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }

            event = polls[i].event;
            if ((event | eIO_ReadWrite) != eIO_ReadWrite) {
                polls[i].revent = eIO_Close;
                bad = 1/*true*/;
                continue;
            }
            if (!event) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            if (bad)
                continue;

            if ((fd = sock->sock) == SOCK_INVALID) {
                polls[i].revent = eIO_Close;
                ready++;
                continue;
            }
            if (polls[i].revent) {
                ready++;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
            }

            bitset = 0;
            type = (ESOCK_Type) sock->type;
            switch (type & eSocket ? event : event & eIO_Read) {
            case eIO_Write:
            case eIO_ReadWrite:
                assert(type & eSocket);
                if (type == eDatagram  ||  sock->w_status != eIO_Closed)
                    bitset |= POLLOUT;
                if (event == eIO_Write  &&
                    (type == eDatagram  ||  asis
                     ||  (sock->r_on_w == eOff
                          ||  (sock->r_on_w == eDefault
                               &&  s_ReadOnWrite != eOn)))) {
                    break;
                }
                /*FALLTHRU*/

            case eIO_Read:
                if (type != eSocket
                    ||  (sock->r_status != eIO_Closed  &&  !sock->eof))
                    bitset |= POLLIN;
                if (type != eSocket  ||  asis  ||  event != eIO_Read
                    ||  sock->w_status == eIO_Closed
                    ||  !(sock->pending | sock->w_len)) {
                    break;
                }
                bitset |= POLLOUT;
                break;

            default:
                /*fully pre-ready*/
                continue;
            }

            if (!bitset)
                continue;
            assert(count < (nfds_t) m);
            x_polls[count].fd      = fd;
            x_polls[count].events  = bitset;
            x_polls[count].revents = 0;
            count++;
        }
        assert(i >= n);

        if (bad) {
            status = eIO_InvalidArg;
            errno = EINVAL;
            break;
        }

        if (s_SelectTimeout) {
            slice = (s_SelectTimeout->tv_sec         * 1000 +
                    (s_SelectTimeout->tv_usec + 500) / 1000);
            if (wait != -1  &&  wait < slice)
                slice = wait;
        } else
            slice = wait;

        if (count  ||  !ready) {
            x_ready = poll(x_polls, count, !ready ? slice : 0);

            if (x_ready > 0) {
#    ifdef NCBI_OS_DARWIN
                /* Mac OS X sometimes misreports, weird! */
                if (x_ready > (int) count)
                    x_ready = (int) count;  /* this is *not* a workaround!!! */
#    endif /*NCBI_OS_DARWIN*/
                assert(status == eIO_Success);
                ready = (nfds_t) x_ready;
                assert(ready <= count);
                break;
            }
        } else
            x_ready = 0;

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
                const char* strerr = SOCK_STRERROR(x_ready);
                CORE_LOGF_ERRNO_EXX(147, ready ? eLOG_Warning : eLOG_Error,
                                    x_ready, strerr,
                                    ("%s[SOCK::Select] "
                                     " Failed poll()",
                                     n == 1 ? s_ID(polls[0].sock, _id) : ""));
                UTIL_ReleaseBuffer(strerr);
                if (!ready) {
                    status = eIO_Unknown;
                    break;
                }
            } else if ((n != 1  &&  s_InterruptOnSignal == eOn)  ||
                       (n == 1  &&  (polls[0].sock->i_on_sig == eOn
                                     ||  (polls[0].sock->i_on_sig == eDefault
                                          &&  s_InterruptOnSignal == eOn)))) {
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
        nfds_t x_ready = 0;
        nfds_t scanned = 0;
        for (m = 0, i = 0;  i < n;  i++) {
            SOCK sock = polls[i].sock;
            if (sock  &&  polls[i].event) {
                TSOCK_Handle fd;
                short events, revents;
                if (polls[i].revent == eIO_Close) {
                    x_ready++;
                    continue;
                }
                if ((fd = sock->sock) == SOCK_INVALID) {
                    polls[i].revent = eIO_Close;
                    x_ready++;
                    continue;
                }
                events = revents = 0;
                if (scanned < ready) {
                    nfds_t x_scanned = 0;
                    nfds_t j;
                    assert((nfds_t) m < count);
                    for (j = (nfds_t) m;  j < count;  ++j) {
                        if (x_polls[j].revents)
                            x_scanned++;
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
                if ((events & POLLIN)
                     &&  (revents & (POLLIN | POLLHUP | POLLPRI))) {
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Read);
                }
                if ((events & POLLOUT)
                    &&  (revents & (POLLOUT | POLLHUP))) {
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
                }
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                if (polls[i].revent == eIO_Open) {
                    if (!(revents & (POLLERR | POLLNVAL)))
                        continue;
                    polls[i].revent = eIO_Close;
                } else if (sock->type == eTrigger)
                    polls[i].revent = polls[i].event;
                assert(polls[i].revent != eIO_Open);
                x_ready++;
            } else
                assert(polls[i].revent == eIO_Open);
        }
        assert(scanned <= ready);
        assert(x_ready >= ready);
    }

    if (x_polls != xx_polls)
        free(x_polls);
    return status;
}


#  endif /*NCBI_OS_UNIX && !NCBI_OS_DARWIN && HAVE_POLL_H*/


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

        for (i = 0;  i < n;  i++) {
            long      bitset;
            EIO_Event event;
            SOCK      sock;
            HANDLE    ev;

            if (!(sock = polls[i].sock)) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }

            event = polls[i].event;
            if ((event | eIO_ReadWrite) != eIO_ReadWrite) {
                polls[i].revent = eIO_Close;
                if (!done) {
                    ready = 0/*false*/;
                    done  = 1/*true*/;
                }
                continue;
            }
            if (!event) {
                assert(!polls[i].revent/*eIO_Open*/);
                continue;
            }
            if (done)
                continue;

            if (sock->sock == SOCK_INVALID) {
                polls[i].revent = eIO_Close;
                ready = 1/*true*/;
                continue;
            }
            if (polls[i].revent) {
                ready = 1/*true*/;
                if (polls[i].revent == eIO_Close)
                    continue;
                assert((polls[i].revent | eIO_ReadWrite) == eIO_ReadWrite);
                event = (EIO_Event)(event & ~polls[i].revent);
            }

            bitset = 0;
            if (sock->type != eTrigger) {
                ESOCK_Type type = (ESOCK_Type) sock->type;
                EIO_Event  readable = sock->readable ? eIO_Read  : eIO_Open;
                EIO_Event  writable = sock->writable ? eIO_Write : eIO_Open;
                switch (type & eSocket ? event : event & eIO_Read) {
                case eIO_Write:
                case eIO_ReadWrite:
                    if (type == eDatagram  ||  sock->w_status != eIO_Closed) {
                        if (writable) {
                            polls[i].revent |= eIO_Write;
                            ready = 1/*true*/;
                        }
                        if (!sock->connected)
                            bitset |= FD_CONNECT/*C*/;
                        bitset     |= FD_WRITE/*W*/;
                    }
                    if (event == eIO_Write  &&
                        (type == eDatagram  ||  asis
                         ||  (sock->r_on_w == eOff
                              ||  (sock->r_on_w == eDefault
                                   &&  s_ReadOnWrite != eOn)))) {
                        break;
                    }
                    /*FALLTHRU*/

                case eIO_Read:
                    if (type != eSocket
                        ||  (sock->r_status != eIO_Closed  &&  !sock->eof)) {
                        if (readable) {
                            polls[i].revent |= eIO_Read;
                            ready = 1/*true*/;
                        }
                        if (type & eSocket) {
                            if (type == eSocket)
                                bitset |= FD_OOB/*O*/;
                            bitset     |= FD_READ/*R*/;
                        } else
                            bitset     |= FD_ACCEPT/*A*/;
                    }
                    if (type != eSocket  ||  asis  ||  event != eIO_Read
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
                /* NB: only once here, as this sets "bad" to "1" */
                CORE_LOGF_X(145, eLOG_Error,
                            ("[SOCK::Select] "
                             " Too many objects, must be fewer than %u",
                             (unsigned int) count));
                polls[i].revent = eIO_Close;
                ready = done = 1/*true*/;
                continue;
            }
            want[count]   = bitset;
            what[count++] = ev;
        }
        assert(i >= n);

        if (done) {
            if (ready) {
                errno = SOCK_ETOOMANY;
                return eIO_Unknown;
            } else {
                errno = EINVAL;
                return eIO_InvalidArg;
            }
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
                for (j = i;  j < n;  j++) {
                    SOCK sock = polls[j].sock;
                    WSANETWORKEVENTS e;
                    long bitset;
                    if (!sock  ||  !polls[j].event)
                        continue;
                    if (polls[j].revent == eIO_Close) {
                        ready = 1/*true*/;
                        continue;
                    }
                    if (sock->type == eTrigger) {
                        if (what[m] != ((TRIGGER) sock)->fd)
                            continue;
                        polls[j].revent = polls[j].event;
                        assert(polls[j].revent != eIO_Open);
                        done = 1/*true*/;
                        break;
                    }
                    if (sock->sock == SOCK_INVALID) {
                        polls[j].revent = eIO_Close;
                        ready = 1/*true*/;
                        continue;
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
                        int x_error = SOCK_ERRNO;
                        const char* strerr = SOCK_STRERROR(x_error);
                        CORE_LOGF_ERRNO_EXX(136, eLOG_Error,
                                            x_error, strerr,
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
                        if (sock->type == eListening
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
                        if (sock->type != eSocket) {
                            polls[j].revent = eIO_Close;
                            done = 1/*true*/;
                            break;
                        }
                        bitset |= FD_READ/*at least SHUT_WR @ remote end*/;
                        sock->readable = 1/*true*/;
                        sock->closing  = 1/*true*/;
                    } else {
                        if (bitset & (FD_CONNECT | FD_WRITE)) {
                            assert(sock->type & eSocket);
                            sock->writable = 1/*true*/;
                        }
                        if (bitset & (FD_ACCEPT | FD_OOB | FD_READ))
                            sock->readable = 1/*true*/;
                    }
                    bitset &= want[m];
                    if ((bitset & (FD_CONNECT | FD_WRITE))
                        &&  sock->writable) {
                        assert(sock->type & eSocket);
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
                        } else for (k = 0;  k < FD_MAX_EVENTS;  k++) {
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
                    m++;
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

#  if defined(NCBI_OS_UNIX) && !defined(NCBI_OS_DARWIN) && defined(HAVE_POLL_H)
    if (s_IOWaitSysAPI != eSOCK_IOWaitSysAPISelect)
        return s_Poll_(n, polls, tv, asis);
#  endif /*NCBI_OS_UNIX && !NCBI_OS_DARWIN && HAVE_POLL_H*/

    return s_Select_(n, polls, tv, asis);

#endif /*NCBI_OS_MSWIN && NCBI_CXX_TOOLKIT*/
}


#ifdef NCBI_COMPILER_GCC
#  pragma GCC diagnostic push                       /* NCBI_FAKE_WARNING */
#  pragma GCC diagnostic ignored "-Wuninitialized"  /* NCBI_FAKE_WARNING */
static inline void x_tvcpy(struct timeval* dst, struct timeval* src)
{
    memcpy(dst, src, sizeof(*dst));
}
#  pragma GCC diagnostic warning "-Wuninitialized"  /* NCBI_FAKE_WARNING */
#  pragma GCC diagnostic pop                        /* NCBI_FAKE_WARNING */
#else
#  define x_tvcpy(d, s)  (void) memcpy((d), (s), sizeof(*(d)))
#endif /*NCBI_COMPILER_GCC*/


/* connect() could be async/interrupted by a signal or just cannot establish
 * the connection immediately;  yet, it must have been in progress
 * (asynchronous), so wait here for it to succeed (become writeable).
 */
static EIO_Status s_IsConnected_(SOCK                  sock,
                                 const struct timeval* tv,
                                 int*                  error,
                                 int/*bool*/           writeable)
{
    char _id[MAXIDLEN];
    EIO_Status status;
    SSOCK_Poll poll;

    *error = 0;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;

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
        TSOCK_socklen_t len = (TSOCK_socklen_t) sizeof(*error);
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
#if defined(_DEBUG)  &&  !defined(NDEBUG)
        if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn)) {
            char mtu[128];
#  if defined(SOL_IP)  &&  defined(IP_MTU)
            if (sock->port) {
                int             m    = 0;
                TSOCK_socklen_t mlen = (TSOCK_socklen_t) sizeof(m);
                if (getsockopt(sock->sock, SOL_IP, IP_MTU, &m, &mlen) != 0) {
                    const char* strerr = SOCK_STRERROR(SOCK_ERRNO);
                    sprintf(mtu, ", MTU unknown (%.80s)", strerr);
                    UTIL_ReleaseBuffer(strerr);
                } else
                    sprintf(mtu, ", MTU = %d", m);
            } else
#  endif /*SOL_IP && IP_MTU*/
                *mtu = '\0';
            CORE_TRACEF(("%sConnection established%s", s_ID(sock, _id), mtu));
        }
#endif /*_DEBUG && !NDEBUG*/
        if (s_ReuseAddress == eOn
#ifdef NCBI_OS_UNIX
            &&  !sock->path[0]
#endif /*NCBI_OS_UNIX*/
            &&  !s_SetReuseAddress(sock->sock, 1/*true*/)) {
            int x_error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(6, eLOG_Trace,
                                x_error, strerr,
                                ("%s[SOCK::IsConnected] "
                                 " Failed setsockopt(REUSEADDR)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
        sock->connected = 1;
    }

    if (sock->pending) {
        if (sock->session) {
            FSSLOpen sslopen = s_SSL ? s_SSL->Open : 0;
            assert(sock->session != SESSION_INVALID);
            if (sslopen) {
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
                status = sslopen(sock->session, error);
                if ((sock->w_tv_set = wtv_set) != 0)
                    x_tvcpy(&sock->w_tv, &wtv);
                if ((sock->r_tv_set = rtv_set) != 0)
                    x_tvcpy(&sock->r_tv, &rtv);
                if (status != eIO_Success) {
                    if (status != eIO_Timeout) {
                        const char* strerr = s_StrError(sock, *error);
                        CORE_LOGF_ERRNO_EXX(126, eLOG_Trace,
                                            *error, strerr,
                                            ("%s[SOCK::IsConnected] "
                                             " Failed SSL hello",
                                             s_ID(sock, _id)));
                        UTIL_ReleaseBuffer(strerr);
                    }
                } else
                    sock->pending = 0;
            } else
                status = eIO_NotSupported;
        } else
            sock->pending = 0;
    }

    return status;
}


static EIO_Status s_IsConnected(SOCK sock, const struct timeval* tv)
{
    int        unused;
    EIO_Status status = s_IsConnected_(sock, tv, &unused, 0);
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Timeout) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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
                         int     flag)
{
    int/*bool*/ readable;
    char _id[MAXIDLEN];

    assert(sock->type == eSocket  &&  buf  &&  size > 0  &&  !*n_read);

    if (sock->r_status == eIO_Closed  ||  sock->eof)
        return eIO_Closed;

    /* read from the socket */
    readable = 0/*false*/;
    for (;;) { /* optionally auto-resume if interrupted */
        int x_read = recv(sock->sock, buf,
#ifdef NCBI_OS_MSWIN
                          /*WINSOCK wants it weird*/ (int)
#endif /*NCBI_OS_MSWIN*/
                          size, 0/*flags*/);
        int x_error;

#ifdef NCBI_OS_MSWIN
        /* recv() resets IO event recording */
        sock->readable = sock->closing;
#endif /*NCBI_OS_MSWIN*/

        /* success/EOF? */
        if (x_read >= 0  ||
            (x_read < 0  &&  ((x_error = SOCK_ERRNO) == SOCK_ENOTCONN    ||
                              x_error                == SOCK_ETIMEDOUT   ||
                              x_error                == SOCK_ENETRESET   ||
                              x_error                == SOCK_ECONNRESET  ||
                              x_error                == SOCK_ECONNABORTED))) {
            /* statistics & logging */
            if ((x_read < 0  &&  sock->log != eOff)  ||
                ((sock->log == eOn || (sock->log == eDefault && s_Log == eOn))
                 &&  (!sock->session  ||  flag > 0))) {
                s_DoLog(x_read < 0
                        ? (sock->n_read & sock->n_written
                           ? eLOG_Error : eLOG_Trace)
                        : eLOG_Note, sock, eIO_Read,
                        x_read < 0 ? (void*) &x_error :
                        x_read > 0 ? buf              : 0,
                        (size_t)(x_read < 0 ? 0 : x_read), 0);
            }

            if (x_read > 0) {
                assert((size_t) x_read <= size);
                sock->n_read += (TNCBI_BigCount) x_read;
                *n_read       = x_read;
            } else {
                /* catch EOF/failure */
                sock->eof = 1/*true*/;
                if (x_read) {
                    sock->r_status = sock->w_status = eIO_Closed;
                    break/*closed*/;
                }
#ifdef NCBI_OS_MSWIN
                sock->closing = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
            }
            sock->r_status = eIO_Success;
            break/*success*/;
        }

        if (x_error == SOCK_EWOULDBLOCK  ||  x_error == SOCK_EAGAIN) {
            /* blocked -- wait for data to come;  return if timeout/error */
            EIO_Status status;
            SSOCK_Poll poll;

            if (sock->r_tv_set  &&  !(sock->r_tv.tv_sec | sock->r_tv.tv_usec)){
                sock->r_status = eIO_Timeout;
                break/*timeout*/;
            }
            if (readable) {
                CORE_TRACEF(("%s[SOCK::Recv] "
                             " Spurious false indication of data ready",
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

        if (x_error != SOCK_EINTR) {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(7, eLOG_Trace,
                                x_error, strerr,
                                ("%s[SOCK::Recv] "
                                 " Failed recv()",
                                 s_ID(sock, _id)));            
            UTIL_ReleaseBuffer(strerr);
            /* don't want to handle all possible errors...
               let them be "unknown" */
            sock->r_status = eIO_Unknown;
            break/*unknown*/;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->r_status = eIO_Interrupt;
            break/*interrupt*/;
        }
    }

    return (EIO_Status) sock->r_status;
}


/*fwdecl*/
static EIO_Status s_WritePending(SOCK, const struct timeval*, int, int);


/* Read/Peek data from the socket.  Return eIO_Success if some data have been
 * read.  Return other (error) code if an error/EOF occurred (zero bytes read).
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves.)
 * NB:  peek = {-1=upread, 0=read, 1=peek}
 */
static EIO_Status s_Read_(SOCK    sock,
                          void*   buf,
                          size_t  size,
                          size_t* n_read,
                          int     peek)
{
    char xx_buf[SOCK_BUF_CHUNK_SIZE];
    unsigned int rtv_set;
    struct timeval rtv;
    EIO_Status status;
    int/*bool*/ done;

    assert(sock->type & eSocket);

    if (sock->type != eDatagram  &&  peek >= 0) {
        *n_read = 0;
        status = s_WritePending(sock, SOCK_GET_TIMEOUT(sock, r), 0, 0);
        if (sock->pending)
            return status;
        if (!size  &&  peek >= 0)
            return sock->eof ? eIO_Closed : sock->r_status;
    }

    if (sock->type == eDatagram  ||  peek >= 0) {
        *n_read = (peek
                   ? BUF_Peek(sock->r_buf, buf, size)
                   : BUF_Read(sock->r_buf, buf, size));
        if (sock->type == eDatagram) {
            if (size  &&  !*n_read)
                sock->r_status = eIO_Closed;
            return (EIO_Status) sock->r_status;
        }
        if (*n_read  &&  (*n_read == size  ||  !peek))
            return eIO_Success;
    } else
        *n_read = 0;

    if (sock->r_status == eIO_Closed  ||  sock->eof) {
        if (*n_read)
            return eIO_Success;
        if (!sock->eof) {
            CORE_TRACEF(("%s[SOCK::Read] "
                         " Socket already shut down for reading",
                         s_ID(sock, xx_buf)));
        }
        return eIO_Closed;
    }

    done = 0/*false*/;
    if ((rtv_set = sock->r_tv_set) != 0)
        rtv = sock->r_tv;
    assert(!*n_read  ||  peek > 0);
    assert((peek >= 0  &&  size)  ||  (peek < 0  &&  !(buf  ||  size)));
    do {
        size_t x_read;
        size_t n_todo;
        char*  x_buf;

        if (!buf/*internal upread/skipping*/  ||
            ((n_todo = size - *n_read) < sizeof(xx_buf))) {
            n_todo   = sizeof(xx_buf);
            x_buf    =        xx_buf;
        } else
            x_buf    = (char*) buf + *n_read;

        if (sock->session) {
            int x_error;
            FSSLRead sslread = s_SSL ? s_SSL->Read : 0;
            assert(sock->session != SESSION_INVALID);
            if (!sslread) {
                status = eIO_NotSupported;
                break/*error*/;
            }
            status = sslread(sock->session, x_buf, n_todo, &x_read, &x_error);
            assert(status == eIO_Success  ||  x_error);
            assert(status == eIO_Success  ||  !x_read);

            /* statistics & logging */
            if ((status != eIO_Success  &&  sock->log != eOff)  ||
                sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(x_read > 0 ? eLOG_Note : eLOG_Trace, sock, eIO_Read,
                        x_read > 0 ? x_buf :
                        status == eIO_Success ? 0 : (void*) &x_error,
                        status != eIO_Success ? 0 : x_read, " [decrypt]");
            }

            if (status == eIO_Closed) {
                sock->r_status = eIO_Closed;
                sock->eof = 1/*true*/;
                break/*bad error*/;
            }
        } else {
            x_read = 0;
            status = s_Recv(sock, x_buf, n_todo, &x_read, 0);
            assert(status == eIO_Success  ||  !x_read);
        }
        if (status != eIO_Success)
            break/*error*/;
        if (!x_read) {
            status = eIO_Closed;
            break/*EOF*/;
        }
        assert(status == eIO_Success  &&  0 < x_read  &&  x_read <= n_todo);

        if (x_read < n_todo)
            done = 1/*true*/;
        if (buf  ||  size) {
            n_todo = size - *n_read;
            if (n_todo > x_read)
                n_todo = x_read;
            if (buf  &&  x_buf == xx_buf)
                memcpy((char*) buf + *n_read, x_buf, n_todo);
        } else
            n_todo = x_read;

        if (peek  ||  x_read > n_todo) {
            /* store the newly read/excess data in the internal input buffer */
            if (!BUF_Write(&sock->r_buf,
                           peek ? x_buf  : x_buf  + n_todo,
                           peek ? x_read : x_read - n_todo)) {
                CORE_LOGF_ERRNO_X(8, eLOG_Error, errno,
                                  ("%s[SOCK::Read] "
                                   " Cannot store data in peek buffer",
                                   s_ID(sock, xx_buf)));
                sock->eof      = 1/*failure*/;
                sock->r_status = eIO_Closed;
                status = eIO_Unknown;
            }
            if (x_read > n_todo)
                x_read = n_todo;
        }
        *n_read += x_read;

        if (status != eIO_Success  ||  done)
            break;
        /*zero timeout*/
        sock->r_tv_set = 1;
        memset(&sock->r_tv, 0, sizeof(sock->r_tv));
    } while (peek < 0  ||  (!buf  &&  *n_read < size));
    if ((sock->r_tv_set = rtv_set) != 0)
        x_tvcpy(&sock->r_tv, &rtv);

    return *n_read ? eIO_Success : status;
}


static EIO_Status s_Read(SOCK    sock,
                         void*   buf,
                         size_t  size,
                         size_t* n_read,
                         int     peek)
{
    EIO_Status status = s_Read_(sock, buf, size, n_read, peek);
    if (s_ErrHook  &&  status != eIO_Success
        &&  (status != eIO_Closed
             ||  !(sock->r_status == eIO_Success  &&  sock->eof))) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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
    size_t i, k;

    assert(!n  ||  polls);

    for (;;) { /* until one full "tv" term expires or an error occurs */
        int/*bool*/ pending;
        EIO_Status  status;

        status = s_Select(n, polls, tv, 0);
        if (status != eIO_Success) {
            if (n_ready)
                *n_ready = 0;
            return status;
        }

        k = 0;
        pending = 0;
        for (i = 0;  i < n;  i++) {
            if (polls[i].revent == eIO_Close)
                break;
            if (polls[i].revent & polls[i].event)
                break;
            if (polls[i].revent != eIO_Open  &&  !pending) {
                pending = 1;
                k = i;
            }
        }
        if (i < n/*ready*/)
            break;

        /* all sockets are not ready for the requested events */
        assert(pending);
        for (i = k;  i < n;  i++) {
            static const struct timeval zero = { 0 };
            SOCK sock = polls[i].sock;
            /* try to push pending writes */
            if (polls[i].event == eIO_Read  &&  polls[i].revent == eIO_Write) {
                assert(sock                          &&
                       sock->sock != SOCK_INVALID    &&
                       sock->type == eSocket         &&
                       sock->w_status != eIO_Closed  &&
                       (sock->pending | sock->w_len));
                s_WritePending(sock, &zero, 1/*writeable*/, 0);
                if (sock->r_status == eIO_Closed  ||  sock->eof) {
                    polls[i].revent = eIO_Read;
                    pending = 0;
                } else
                    polls[i].revent = eIO_Open;
                continue;
            }
            /* try to upread immediately readable sockets */
            if (polls[i].event == eIO_Write  &&  polls[i].revent == eIO_Read) {
                size_t dummy;
                assert(sock                          &&
                       sock->sock != SOCK_INVALID    &&
                       sock->type == eSocket         &&
                       sock->w_status != eIO_Closed  &&
                       sock->r_status != eIO_Closed  &&
                       !sock->eof  && !sock->pending &&
                       (sock->r_on_w == eOn
                        ||  (sock->r_on_w == eDefault
                             &&  s_ReadOnWrite == eOn)));
                s_Read_(sock, 0, 0, &dummy, -1/*upread*/);
                if (sock->w_status == eIO_Closed) {
                    polls[i].revent = eIO_Write;
                    pending = 0;
                } else
                    polls[i].revent = eIO_Open;
            }
        }
        if (!pending)
            break;
    }

    k = 0;
    for (i = 0;  i < n;  i++) {
        if (polls[i].revent != eIO_Close) {
            polls[i].revent = (EIO_Event)(polls[i].revent & polls[i].event);
            if (!polls[i].revent)
                continue;
        }
        k++;
    }

    if (n_ready)
        *n_ready = k;

    return k ? eIO_Success : eIO_Timeout;
}


static EIO_Status s_Wait(SOCK sock, EIO_Event event, const STimeout* timeout)
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
    assert(poll.revent & event);
    return status/*success*/;
}


#ifdef NCBI_OS_MSWIN
static void s_AddTimeout(struct timeval* tv, int ms_addend)
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
                         int         flag)
{
#ifdef NCBI_OS_MSWIN
    int wait_buf_ms = 0;
    struct timeval waited;
    memset(&waited, 0, sizeof(waited));
#endif /*NCBI_OS_MSWIN*/

    assert(sock->type == eSocket  &&  data  &&  size > 0  &&  !*n_written);

    if (sock->w_status == eIO_Closed)
        return eIO_Closed;

    for (;;) { /* optionally auto-resume if interrupted */
        int x_error = 0;
        int x_written = send(sock->sock, (void*) data,
#ifdef NCBI_OS_MSWIN
                             /*WINSOCK wants it weird*/ (int)
#endif /*NCBI_OS_MSWIN*/
                             size, flag < 0 ? MSG_OOB : 0);
        if (x_written >= 0  ||
            (x_written < 0  &&  ((x_error= SOCK_ERRNO) == SOCK_EPIPE       ||
                                 x_error               == SOCK_ENOTCONN    ||
                                 x_error               == SOCK_ETIMEDOUT   ||
                                 x_error               == SOCK_ENETRESET   ||
                                 x_error               == SOCK_ECONNRESET  ||
                                 x_error               == SOCK_ECONNABORTED))){
            /* statistics & logging */
            if ((x_written <= 0  &&  sock->log != eOff)  ||
                ((sock->log == eOn || (sock->log == eDefault && s_Log == eOn))
                 &&  (!sock->session  ||  flag > 0))) {
                s_DoLog(x_written <= 0
                        ? (sock->n_read & sock->n_written
                           ? eLOG_Error : eLOG_Trace)
                        : eLOG_Note, sock, eIO_Write,
                        x_written <= 0 ? (void*) &x_error : data,
                        (size_t)(x_written <= 0 ? 0 : x_written),
                        flag < 0 ? "" : 0);
            }

            if (x_written > 0) {
                sock->n_written += (TNCBI_BigCount) x_written;
                *n_written       = x_written;
                sock->w_status = eIO_Success;
                break/*success*/;
            }
            if (x_written < 0) {
                if (x_error != SOCK_EPIPE)
                    sock->r_status = eIO_Closed;
                sock->w_status = eIO_Closed;
                break/*closed*/;
            }
        }

        if (flag < 0/*OOB*/  ||  !x_written)
            return eIO_Unknown;

        /* blocked -- retry if unblocked before the timeout expires
         * (use stall protection if specified) */
        if (x_error == SOCK_EWOULDBLOCK  ||  x_error == SOCK_EAGAIN
#ifdef NCBI_OS_MSWIN
            ||  x_error == WSAENOBUFS
#endif /*NCBI_OS_MSWIN*/
            ) {
            SSOCK_Poll            poll;
            EIO_Status            status;
            const struct timeval* timeout;

#ifdef NCBI_OS_MSWIN
            struct timeval        slice;
            unsigned int          writable = sock->writable;

            /* special send()'s semantics of IO event recording reset */
            sock->writable = 0/*false*/;
            if (x_error == WSAENOBUFS) {
                if (size < SOCK_BUF_CHUNK_SIZE) {
                    s_AddTimeout(&waited, wait_buf_ms);
                    if (s_IsSmallerTimeout(SOCK_GET_TIMEOUT(sock, w),&waited)){
                        sock->w_status = eIO_Timeout;
                        return eIO_Timeout;
                    }
                    if (wait_buf_ms == 0)
                        wait_buf_ms  = 10;
                    else if (wait_buf_ms < 160)
                        wait_buf_ms <<= 1;
                    slice.tv_sec  = 0;
                    slice.tv_usec = wait_buf_ms * 1000;
                } else {
                    size >>= 1;
                    memset(&slice, 0, sizeof(slice));
                }
                timeout = &slice;
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

            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pulling incoming data from the socket */
            status = s_SelectStallsafe(1, &poll, timeout, 0);
            assert(poll.event == eIO_Write);
#ifdef NCBI_OS_MSWIN
            if (x_error == WSAENOBUFS) {
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
            continue/*write again*/;
        }

        if (x_error != SOCK_EINTR) {
            char _id[MAXIDLEN];
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(11, eLOG_Trace,
                                x_error, strerr,
                                ("%s[SOCK::Send] "
                                 " Failed send()",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            /* don't want to handle all possible errors...
               let them be "unknown" */
            sock->w_status = eIO_Unknown;
            break/*unknown*/;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = eIO_Interrupt;
            break/*interrupt*/;
        }
    }

    return (EIO_Status) sock->w_status;
}


/* Wrapper for s_Send() that slices the output buffer for some brain-dead
 * systems (e.g. old Macs) that cannot handle large data chunks in "send()".
 * Return eIO_Success if some data have been successfully sent;
 * an error code if nothing at all has been sent.
 */
#ifdef SOCK_SEND_SLICE
#  undef s_Send
static EIO_Status s_Send(SOCK        sock,
                         const void* data,
                         size_t      size,
                         size_t*     n_written,
                         int         flag)
{
    /* split output buffer in slices (of size <= SOCK_SEND_SLICE) */
    EIO_Status status;

    assert(!*n_written);

    do {
        size_t      n_todo = size > SOCK_SEND_SLICE ? SOCK_SEND_SLICE : size;
        const char* temp   = (const char*) data + *n_written;
        size_t      n_done = 0;
        status = s_Send_(sock, temp, n_todo, &n_done, flag);
        assert(status == eIO_Success  ||  !n_done);
        if (status != eIO_Success)
            break;
        *n_written += n_done;
        if (n_todo != n_done)
            break;
        size       -= n_done;
    } while (size);

    return *n_written ? eIO_Success : status;
}
#endif /*SOCK_SEND_SLICE*/


static EIO_Status s_WriteData(SOCK        sock,
                              const void* data,
                              size_t      size,
                              size_t*     n_written,
                              int/*bool*/ oob)
{
    assert(sock->type == eSocket  &&  !sock->pending  &&  size > 0);

    if (sock->session) {
        int x_error;
        EIO_Status status;
        FSSLWrite sslwrite = s_SSL ? s_SSL->Write : 0;
        assert(sock->session != SESSION_INVALID);
        if (!sslwrite  ||  oob)
            return eIO_NotSupported;
        status = sslwrite(sock->session, data, size, n_written, &x_error);
        assert((status == eIO_Success) == (*n_written > 0));
        assert(status == eIO_Success  ||  x_error);

        /* statistics & logging */
        if ((status != eIO_Success  &&  sock->log != eOff)  ||
            sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn)) {
            s_DoLog(*n_written > 0 ? eLOG_Note : eLOG_Trace, sock, eIO_Write,
                    status == eIO_Success ? data : (void*) &x_error,
                    status != eIO_Success ? 0    : *n_written, " [encrypt]");
        }

        if (status == eIO_Closed)
            sock->w_status = eIO_Closed;
        return status;
    }

    *n_written = 0;
    return s_Send(sock, data, size, n_written, oob ? -1 : 0);
}


static EIO_Status s_WritePending(SOCK                  sock,
                                 const struct timeval* tv,
                                 int/*bool*/           writeable,
                                 int/*bool*/           oob)
{
    unsigned int wtv_set;
    struct timeval wtv;
    EIO_Status status;
    int restore;
    size_t off;

    assert(sock->type == eSocket  &&  sock->sock != SOCK_INVALID);

    if (sock->pending) {
        int x_error;
        status = s_IsConnected_(sock, tv, &x_error, writeable);
        if (status != eIO_Success) {
            if (status != eIO_Timeout) {
                char _id[MAXIDLEN];
                const char* strerr = s_StrError(sock, x_error);
                CORE_LOGF_ERRNO_EXX(12, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    x_error, strerr,
                                    ("%s[SOCK::WritePending] "
                                     " Failed pending connect(): %s",
                                     s_ID(sock, _id),
                                     IO_StatusStr(status)));
                UTIL_ReleaseBuffer(strerr);
                sock->w_status = status;
            }
            return status;
        }
    }
    if ((!sock->session  &&  oob)  ||  !sock->w_len)
        return eIO_Success;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;

    if (tv != &sock->w_tv) {
        if ((wtv_set = sock->w_tv_set) != 0)
            wtv = sock->w_tv;
        SOCK_SET_TIMEOUT(sock, w, tv);
        restore = 1;
    } else
        restore = wtv_set/*to silence compiler warning*/ = 0;
    off = BUF_Size(sock->w_buf) - sock->w_len;
    do {
        size_t n_written;
        char   buf[SOCK_BUF_CHUNK_SIZE];
        size_t n_write = BUF_PeekAt(sock->w_buf, off, buf, sizeof(buf));
        status = s_WriteData(sock, buf, n_write, &n_written, 0);
        assert(status == eIO_Success ||  !n_written);
        if (status != eIO_Success)
            break;
        sock->w_len -= n_written;
        off         += n_written;
    } while (sock->w_len);
    if (restore) {
        if ((sock->w_tv_set = wtv_set) != 0)
            x_tvcpy(&sock->w_tv, &wtv);
    }

    assert((sock->w_len != 0) == (status != eIO_Success));
    return status;
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
    EIO_Status status;

    if (sock->type == eDatagram) {
        sock->w_len = 0;
        if (sock->eof) {
            BUF_Erase(sock->w_buf);
            sock->eof = 0;
        }
        if (BUF_Write(&sock->w_buf, data, size)) {
            *n_written = size;
            sock->w_status = eIO_Success;
        } else {
            *n_written = 0;
            sock->w_status = eIO_Unknown;
        }
        return (EIO_Status) sock->w_status;
    }

    if (sock->w_status == eIO_Closed) {
        if (size) {
            CORE_DEBUG_ARG(char _id[MAXIDLEN];)
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

    assert(sock->w_len == 0);
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
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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
    return status;
}


/* For non-datagram sockets only */
static EIO_Status s_Shutdown(SOCK                  sock,
                             EIO_Event             dir,
                             const struct timeval* tv)
{
    int        x_error;
    char       _id[MAXIDLEN];
    EIO_Status status = eIO_Success;
    int        how = SOCK_SHUTDOWN_WR;

    assert(sock->type == eSocket);

    switch (dir) {
    case eIO_Read:
        if (sock->eof) {
            /* hit EOF (and may be not yet shut down) -- so, flag it as been
             * shut down, but do not perform the actual system call,
             * as it can cause smart OS'es like Linux to complain.
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
            if ((status = s_WritePending(sock, tv, 0, 0)) != eIO_Success
                &&  !sock->pending   &&  sock->w_len) {
                CORE_LOGF_X(13, !tv  ||  (tv->tv_sec | tv->tv_usec)
                            ? eLOG_Warning : eLOG_Trace,
                            ("%s[SOCK::%s] "
                             " %s with output (%lu byte%s) still pending (%s)",
                             s_ID(sock, _id),
                             dir & eIO_ReadWrite ? "Shutdown" : "Close",
                             !dir ? "Leaving " : dir == eIO_Close ? "Closing" :
                             dir == eIO_Write
                             ? "Shutting down for write"
                             : "Shutting down for read/write",
                             (unsigned long) sock->w_len,
                             &"s"[sock->w_len == 1],
                             IO_StatusStr(status)));
            }
            if (sock->session  &&  !sock->pending) {
                FSSLClose sslclose = s_SSL ? s_SSL->Close : 0;
                assert(sock->session != SESSION_INVALID);
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
                    status = sslclose(sock->session, how, &x_error);
                    if ((sock->w_tv_set = wtv_set) != 0)
                        x_tvcpy(&sock->w_tv, &wtv);
                    if ((sock->r_tv_set = rtv_set) != 0)
                        x_tvcpy(&sock->r_tv, &rtv);
                    if (status != eIO_Success) {
                        const char* strerr = s_StrError(sock, x_error);
                        CORE_LOGF_ERRNO_EXX(127, eLOG_Trace,
                                            x_error, strerr,
                                            ("%s[SOCK::%s] "
                                             " Failed SSL bye",
                                             s_ID(sock, _id),
                                             dir & eIO_ReadWrite
                                             ? "Shutdown" : "Close"));
                        UTIL_ReleaseBuffer(strerr);
                    }
                }
            }
        }

        sock->w_status = eIO_Closed;
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
    
#ifndef NCBI_OS_MSWIN
    /* on MS-Win, socket shutdown for write apparently messes up (?!)
     * with later reading, especially when reading a lot of data... */

#  ifdef NCBI_OS_BSD
    /* at least on FreeBSD: shutting down a socket for write (i.e. forcing to
     * send a FIN) for a socket that has been already closed by another end
     * (e.g. when peer has done writing, so this end has done reading and is
     * about to close) seems to cause ECONNRESET in the coming close()...
     * see kern/146845 @ http://www.freebsd.org/cgi/query-pr.cgi?pr=146845 */
    if (dir == eIO_ReadWrite  &&  how != SOCK_SHUTDOWN_RDWR)
        return status;
#  endif /*NCBI_OS_BSD*/

#  ifdef NCBI_OS_UNIX
    if (sock->path[0])
        return status;
#  endif /*NCBI_OS_UNIX*/

    if (s_Initialized > 0  &&  shutdown(sock->sock, how) != 0) {
        x_error = SOCK_ERRNO;
#  ifdef NCBI_OS_MSWIN
        if (x_error == WSANOTINITIALISED)
            s_Initialized = -1/*deinited*/;
        else
#  endif /*NCBI_OS_MSWIN*/
        if (
#  if   defined(NCBI_OS_LINUX)/*bug in the Linux kernel to report*/  || \
        defined(NCBI_OS_IRIX)                                        || \
        defined(NCBI_OS_OSF1)
            x_error != SOCK_ENOTCONN
#  else
            x_error != SOCK_ENOTCONN  ||  sock->pending
#  endif /*UNIX flavors*/
            ) {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(16, eLOG_Trace,
                                x_error, strerr,
                                ("%s[SOCK::Shutdown] "
                                 " Failed shutdown(%s)",
                                 s_ID(sock, _id), dir == eIO_Read ? "R" :
                                 dir == eIO_Write ? "W" : "RW"));
            UTIL_ReleaseBuffer(strerr);
        }
    }
#endif /*!NCBI_OS_MSWIN*/

    return status;
}


/* Close the socket (either orderly or abruptly)
 */
static EIO_Status s_Close_(SOCK sock, int abort)
{
    char       _id[MAXIDLEN];
    int        x_error;
    EIO_Status status;

    assert(sock->sock != SOCK_INVALID);
    BUF_Erase(sock->r_buf);
    if (sock->type == eDatagram) {
        sock->r_len = 0;
        BUF_Erase(sock->w_buf);
    } else if (abort  ||  !sock->keep) {
        int/*bool*/ linger = 0/*false*/;
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
                unsigned int seconds = sock->c_tv.tv_sec
                    + (sock->c_tv.tv_usec + 500000) / 1000000;
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
                const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
                CORE_LOGF_ERRNO_EXX(17, eLOG_Trace,
                                    x_error, strerr,
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
                    const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
                    CORE_LOGF_ERRNO_EXX(18, eLOG_Trace,
                                        x_error, strerr,
                                        ("%s[SOCK::Close] "
                                         " Failed setsockopt(TCP_LINGER2)",
                                         s_ID(sock, _id)));
                    UTIL_ReleaseBuffer(strerr);
                }
            }
#  endif /*TCP_LINGER2*/
        }
#endif /*(NCBI_OS_UNIX && !NCBI_OS_BEOS) || NCBI_OS_MSWIN*/

        if (!abort) {
            /* orderly shutdown in both directions */
            s_Shutdown(sock, eIO_Close, SOCK_GET_TIMEOUT(sock, c));
            assert(sock->r_status == eIO_Closed  &&
                   sock->w_status == eIO_Closed);
        } else
            sock->r_status = sock->w_status = eIO_Closed;

#ifdef NCBI_OS_MSWIN
        WSAEventSelect(sock->sock, sock->event/*ignored*/, 0/*cancel*/);
#endif /*NCBI_OS_MSWIN*/
        /* set the socket back to blocking mode */
        if (s_Initialized > 0
            &&  linger  &&  !s_SetNonblock(sock->sock, 0/*false*/)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            assert(!abort);
            CORE_LOGF_ERRNO_EXX(19, eLOG_Trace,
                                x_error, strerr,
                                ("%s[SOCK::Close] "
                                 " Cannot set socket back to blocking mode",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
    } else
        status = s_Shutdown(sock, eIO_Open, SOCK_GET_TIMEOUT(sock, c));
    sock->w_len = 0;

    if (sock->session  &&  sock->session != SESSION_INVALID) {
        FSSLDelete ssldelete = s_SSL ? s_SSL->Delete : 0;
        if (ssldelete)
            ssldelete(sock->session);
        sock->session = SESSION_INVALID;
    }

    if (abort >= -1) {
        if (sock->type != eDatagram) {
            sock->n_in  += sock->n_read;
            sock->n_out += sock->n_written;
        }

        /* statistics & logging */
        if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
            s_DoLog(eLOG_Note, sock, eIO_Close, 0, 0, abort ? "Aborting" : 0);
    }

    status = eIO_Success;
    if (abort  ||  !sock->keep) {
        if (abort < 0)
            abort = 1;
#ifdef NCBI_OS_MSWIN
        if (sock->event)
            WSASetEvent(sock->event); /*signal closure*/
#endif /*NCBI_OS_MSWIN*/
        for (;;) { /* close persistently - retry if interrupted by a signal */
            if (SOCK_CLOSE(sock->sock) == 0)
                break;

            /* error */
            if (s_Initialized <= 0)
                break;
            x_error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
            if (x_error == WSANOTINITIALISED) {
                s_Initialized = -1/*deinited*/;
                break;
            }
#endif /*NCBI_OS_MSWIN*/
            if (x_error == SOCK_ENOTCONN/*already closed by now*/
                ||  (!(sock->n_read | sock->n_written)
                     &&  (x_error == SOCK_ENETRESET   ||
                          x_error == SOCK_ECONNRESET  ||
                          x_error == SOCK_ECONNABORTED))) {
                break;
            }
            if (abort  ||  x_error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(x_error);
                CORE_LOGF_ERRNO_EXX(21, abort == 1 ? eLOG_Warning : eLOG_Error,
                                    x_error, strerr,
                                    ("%s[SOCK::%s] "
                                     " Failed close()",
                                     s_ID(sock, _id),
                                     abort ? "Abort" : "Close"));
                UTIL_ReleaseBuffer(strerr);
                if (abort > 1  ||  x_error != SOCK_EINTR) {
                    status =
                        x_error == SOCK_ETIMEDOUT ? eIO_Timeout : eIO_Unknown;
                    break;
                }
                if (abort)
                    abort++;
            }
        }
    }

    /* return */
    sock->sock = SOCK_INVALID;
#ifdef NCBI_OS_MSWIN
    if (sock->event) {
        WSACloseEvent(sock->event);
        sock->event = 0;
    }
#endif /*NCBI_OS_MSWIN*/
    sock->myport = 0;
    return status;
}


static EIO_Status s_Close(SOCK sock)
{
    EIO_Status status = s_Close_(sock, 0/*orderly*/);
    if (s_ErrHook  &&  status != eIO_Success) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } addr;
    char            _id[MAXIDLEN];
    int             x_error;
    TSOCK_socklen_t addrlen;
    TSOCK_Handle    x_sock;
    EIO_Status      status;
    int             n;

    assert(sock->type == eSocket  &&  sock->side == eSOCK_Client);

    /* initialize internals */
    if (s_InitAPI(sock->session ? 1/*secure*/ : 0/*regular*/) != eIO_Success)
        return eIO_NotSupported;

    if (sock->session) {
        FSSLCreate sslcreate = s_SSL ? s_SSL->Create : 0;
        void* session;
        assert(sock->sock == SOCK_INVALID);
        assert(sock->session == SESSION_INVALID);
        if (!sslcreate) {
            session = 0;
            x_error = 0;
        } else
            session = sslcreate(eSOCK_Client, sock, &x_error);
        if (!session) {
            const char* strerr = s_StrError(sock, x_error);
            CORE_LOGF_ERRNO_EXX(131, eLOG_Error,
                                x_error, strerr,
                                ("%s[SOCK::Connect] "
                                 " Failed to initialize secure session",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            return eIO_NotSupported;
        }
        assert(session != SESSION_INVALID);
        sock->session = session;
    }

    memset(&addr, 0, sizeof(addr));
#ifdef NCBI_OS_UNIX
    if (sock->path[0]) {
        size_t pathlen = strlen(sock->path);
        if (sizeof(addr.un.sun_path) <= pathlen++/*account for '\0'*/) {
            CORE_LOGF_X(142, eLOG_Error,
                        ("%s[SOCK::Connect] "
                         " Path too long (%lu vs %lu bytes allowed)",
                         s_ID(sock, _id), (unsigned long) pathlen,
                         (unsigned long) sizeof(addr.un.sun_path)));
            return eIO_InvalidArg;
        }
        addrlen = (TSOCK_socklen_t) sizeof(addr.un);
#  ifdef HAVE_SIN_LEN
        addr.un.sun_len    = addrlen;
#  endif /*HAVE_SIN_LEN*/
        addr.un.sun_family = AF_UNIX;
        memcpy(addr.un.sun_path, sock->path, pathlen);
        assert(!sock->port);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        /* get address of the remote host (assume the same host if NULL) */
        if (host && !(sock->host = s_gethostbyname(host, (ESwitch)sock->log))){
            CORE_LOGF_X(22, eLOG_Error,
                        ("%s[SOCK::Connect] "
                         " Failed SOCK_gethostbyname(\"%.*s\")",
                         s_ID(sock, _id), MAXHOSTNAMELEN, host));
            return eIO_Unknown;
        }
        /* set the port to connect to (same port if zero) */
        if (port)
            sock->port = port;
        else
            assert(sock->port);
        addrlen = (TSOCK_socklen_t) sizeof(addr.in);
#ifdef HAVE_SIN_LEN
        addr.in.sin_len         = addrlen;
#endif /*HAVE_SIN_LEN*/
        addr.in.sin_family      = AF_INET;
        addr.in.sin_addr.s_addr =       sock->host;
        addr.in.sin_port        = htons(sock->port);
#ifdef NCBI_OS_UNIX
        assert(!sock->path[0]);
#endif /*NCBI_OS_UNIX*/
    }

    /* create the new socket */
    if ((x_sock = socket(addr.sa.sa_family, SOCK_STREAM, 0)) == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(23, eLOG_Error,
                            x_error, strerr,
                            ("%s[SOCK::Connect] "
                             " Cannot create socket",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
    sock->sock     = x_sock;
    sock->r_status = eIO_Success;
    sock->eof      = 0/*false*/;
    sock->w_status = eIO_Success;
    assert(sock->w_len == 0);

#ifdef NCBI_OS_MSWIN
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
        s_Close_(sock, -2/*silent abort*/);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(sock->sock, sock->event, SOCK_EVENTS) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(123, eLOG_Error,
                            x_error, strerr,
                            ("%s[SOCK::Connect] "
                             " Failed to bind IO event",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        s_Close_(sock, -2/*silent abort*/);
        return eIO_Unknown;
    }
#else
    /* set non-blocking mode */
    if (!s_SetNonblock(x_sock, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(24, eLOG_Error,
                            x_error, strerr,
                            ("%s[SOCK::Connect] "
                             " Cannot set socket to non-blocking mode",
                             s_ID(sock, _id)));
        UTIL_ReleaseBuffer(strerr);
        s_Close_(sock, -2/*silent abort*/);
        return eIO_Unknown;
    }
#endif

    if (sock->port) {
#ifdef SO_KEEPALIVE
        if (sock->keepalive  &&  !s_SetKeepAlive(x_sock, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(151, eLOG_Warning,
                                x_error, strerr,
                                ("%s[SOCK::Connect] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(x_sock, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(135, eLOG_Warning,
                                x_error, strerr,
                                ("%s[SOCK::Connect] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

    if ((!sock->crossexec  ||  sock->session)  &&  !s_SetCloexec(x_sock, 1)) {
        const char* strerr;
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        x_error = err;
#else
        x_error = errno;
        strerr = SOCK_STRERROR(x_error);
#endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(129, eLOG_Warning,
                            x_error, strerr ? strerr : "",
                            ("%s[SOCK::Connect] "
                             " Cannot set socket close-on-exec mode",
                             s_ID(sock, _id)));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    /* establish connection to the peer */
    sock->connected = 0/*false*/;
#ifdef NCBI_OS_MSWIN
    sock->readable  = 0/*false*/;
    sock->writable  = 0/*false*/;
    sock->closing   = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/
    for (n = 0; ; n = 1) { /* optionally auto-resume if interrupted */
        if (connect(x_sock, &addr.sa, addrlen) == 0) {
            x_error = 0;
            break;
        }
        x_error = SOCK_ERRNO;
        if (x_error != SOCK_EINTR  ||  sock->i_on_sig == eOn
            ||  (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal)) {
            break;
        }
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, 0, 0, x_error ? 0 : "");

    if (x_error) {
        if (((n == 0  &&  x_error != SOCK_EINPROGRESS)  ||
             (n != 0  &&  x_error != SOCK_EALREADY))
            &&  x_error != SOCK_EWOULDBLOCK) {
            if (x_error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(x_error);
                CORE_LOGF_ERRNO_EXX(25, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    x_error, strerr,
                                    ("%s[SOCK::Connect] "
                                     " Failed connect()",
                                     s_ID(sock, _id)));
                UTIL_ReleaseBuffer(strerr);
                if (x_error == SOCK_ECONNREFUSED
#ifdef NCBI_OS_UNIX
                    ||  (sock->path[0]  &&  x_error == ENOENT)
#endif /*NCBI_OS_UNIX*/
                    ) {
                    status = eIO_Closed;
                } else
                    status = eIO_Unknown;
            } else
                status = eIO_Interrupt;
            s_Close_(sock, -1/*abort*/);
            return status/*error*/;
        }
        sock->pending = 1;
    } else
        sock->pending = sock->session ? 1 : 0;

    if (!x_error  ||  !timeout  ||  (timeout->sec | timeout->usec)) {
        struct timeval tv;
        const struct timeval* x_tv = s_to2tv(timeout, &tv);

        status = s_IsConnected_(sock, x_tv, &x_error, !x_error);
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
                const char* strerr = s_StrError(sock, x_error);
                CORE_LOGF_ERRNO_EXX(26, sock->log != eOff
                                    ? eLOG_Error : eLOG_Trace,
                                    x_error, strerr,
                                    ("%s[SOCK::Connect] "
                                     " Failed pending connect(): %s",
                                     s_ID(sock, _id), reason));
                UTIL_ReleaseBuffer(strerr);
            }
            s_Close_(sock, -1/*abort*/);
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
    if (status != eIO_Success) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            if (sock->host) {
                SOCK_ntoa(sock->host, addr, sizeof(addr));
                info.host = addr;
            } else
                info.host = host;
            info.port = port;
        }
#ifdef NCBI_OS_UNIX
        else
            info.host = sock->path;
#endif /*NCBI_OS_UNIX*/
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


static EIO_Status s_Create(const char*     hostpath,
                           unsigned short  port,
                           const STimeout* timeout,
                           SOCK*           sock,
                           const void*     data,
                           size_t          size,
                           TSOCK_Flags     flags)
{
    size_t       x_n = port ? 0 : strlen(hostpath);
    unsigned int x_id = ++s_ID_Counter * 1000;
    char         _id[MAXIDLEN];
    EIO_Status   status;
    SOCK         x_sock;

    assert(!*sock);

    /* allocate memory for the internal socket structure */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + x_n)))
        return eIO_Unknown;
    x_sock->sock      = SOCK_INVALID;
    x_sock->id        = x_id;
    x_sock->type      = eSocket;
    x_sock->log       = flags;
    x_sock->side      = eSOCK_Client;
    x_sock->session   = flags & fSOCK_Secure ? SESSION_INVALID : 0;
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/  : 0/*false*/;
    x_sock->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn  : eDefault;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn  : eDefault;
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/  : 0/*false*/;
    x_sock->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/  : 0/*false*/;
#ifdef NCBI_OS_UNIX
    if (!port)
        strcpy(x_sock->path, hostpath);
#endif /*NCBI_OS_UNIX*/

    /* setup the I/O data buffer properties */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    if (size) {
        if (BUF_SetChunkSize(&x_sock->w_buf, size) < size  ||
            !BUF_Write(&x_sock->w_buf, data, size)) {
            CORE_LOGF_ERRNO_X(27, eLOG_Error, errno,
                              ("%s[SOCK::Create] "
                               " Cannot store initial data",
                               s_ID(x_sock, _id)));
            SOCK_Close(x_sock);
            return eIO_Unknown;
        }
    }

    /* connect */
    status = s_Connect(x_sock, hostpath, port, timeout);
    if (status != eIO_Success)
        SOCK_Close(x_sock);
    else
        *sock = x_sock;
    return status;
}


static EIO_Status s_CreateOnTop(const void* handle,
                                size_t      handle_size,
                                SOCK*       sock,
                                const void* data,
                                size_t      size,
                                TSOCK_Flags flags)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } peer;
    TSOCK_Handle    fd;
    struct linger   lgr;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event;
#endif /*NCBI_OS_MSWIN*/
    SOCK            x_sock;
    int             x_error;
    TSOCK_socklen_t peerlen;
    size_t          socklen;
    BUF             w_buf = 0;
    char            _id[MAXIDLEN];
    unsigned int    x_id = ++s_ID_Counter * 1000;

    assert(!*sock);
    assert(!size  ||  data);

    if (!handle  ||  handle_size != sizeof(fd)) {
        CORE_LOGF_X(47, eLOG_Error,
                    ("SOCK#%u[?]: [SOCK::CreateOnTop] "
                     " Invalid handle%s %lu",
                     x_id,
                     handle ? " size"                     : "",
                     handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }
    memcpy(&fd, handle, sizeof(fd));

    /* initialize internals */
    if (s_InitAPI(flags & fSOCK_Secure) != eIO_Success)
        return eIO_NotSupported;

    /* get peer's address */
    peerlen = (TSOCK_socklen_t) sizeof(peer);
    memset(&peer, 0, sizeof(peer));
#ifdef HAVE_SIN_LEN
    peer.sa.sa_len = peerlen;
#endif /*HAVE_SIN_LEN*/
    if (getpeername(fd, &peer.sa, &peerlen) != 0) {
        const char* strerr = s_StrError(0, x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(148, eLOG_Error,
                            x_error, strerr,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Invalid OS socket handle",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Closed;
    }
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family != AF_INET  &&  peer.sa.sa_family != AF_UNIX)
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
        if (peer.sa.sa_family != AF_UNSPEC/*0*/)
#  endif /*NCBI_OS_???*/
#else
    if (peer.sa.sa_family != AF_INET)
#endif /*NCBI_OS_UNIX*/
        return eIO_NotSupported;

#ifdef NCBI_OS_UNIX
    if (
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
        peer.sa.sa_family == AF_UNSPEC/*0*/  ||
#  endif /*NCBI_OS*/
        peer.sa.sa_family == AF_UNIX) {
        if (!peer.un.sun_path[0]) {
            peerlen = (TSOCK_socklen_t) sizeof(peer);
            memset(&peer, 0, sizeof(peer));
#  ifdef HAVE_SIN_LEN
            peer.sa.sa_len = peerlen;
#  endif /*HAVE_SIN_LEN*/
            if (getsockname(fd, &peer.sa, &peerlen) != 0)
                return eIO_Closed;
            assert(peer.sa.sa_family == AF_UNIX);
            if (!peer.un.sun_path[0]) {
                CORE_LOGF_X(48, eLOG_Error,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Unbound UNIX socket",
                             x_id, (unsigned int) fd));
                assert(0);
                return eIO_InvalidArg;
            }
        }
        socklen = strlen(peer.un.sun_path);
    } else
#endif /*NCBI_OS_UNIX*/
        socklen = 0;
    
#ifdef NCBI_OS_MSWIN
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(161, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Failed to create IO event",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBufferOnHeap(strerr);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(fd, event, SOCK_EVENTS) != 0) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(162, eLOG_Error,
                            x_error, strerr,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#else
    /* set to non-blocking mode */
    if (!s_SetNonblock(fd, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(50, eLOG_Error,
                            x_error, strerr,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) fd));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
#endif /*NCBI_OS_MSWIN*/

    /* store initial data */
    if (size) {
        if (BUF_SetChunkSize(&w_buf, size) < size  ||
            !BUF_Write(&w_buf, data, size)) {
            CORE_LOGF_ERRNO_X(49, eLOG_Error, errno,
                              ("SOCK#%u[%u]: [SOCK::CreateOnTop] "
                               " Cannot store initial data",
                               x_id, (unsigned int) fd));
            BUF_Destroy(w_buf);
#ifdef NCBI_OS_MSWIN
            WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
            return eIO_Unknown;
        }
    }

    /* create and fill socket handle */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + socklen))) {
        BUF_Destroy(w_buf);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }
    x_sock->sock      = fd;
    x_sock->id        = x_id;
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family == AF_UNIX)
        strcpy(x_sock->path, peer.un.sun_path);
    else
#endif /*NCBI_OS_UNIX*/
    {
        x_sock->host  =       peer.in.sin_addr.s_addr;
        x_sock->port  = ntohs(peer.in.sin_port);
        assert(x_sock->port);
    }
    x_sock->type      = eSocket;
    x_sock->log       = flags;
    x_sock->side      = eSOCK_Server;
    x_sock->session   = flags & fSOCK_Secure ? SESSION_INVALID : 0;
    x_sock->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/  : 0/*false*/;
    x_sock->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn  : eDefault;
    x_sock->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn  : eDefault;
    x_sock->r_status  = eIO_Success;
    x_sock->w_status  = eIO_Success;
#ifdef NCBI_OS_MSWIN
    x_sock->event     = event;
    x_sock->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    x_sock->pending   = 1/*have to check at the nearest I/O*/;
    x_sock->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/  : 0/*false*/;
    x_sock->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/  : 0/*false*/;
    /* all timeout bits zeroed - infinite */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    x_sock->w_buf     = w_buf;
    x_sock->w_len     = size;

    if (x_sock->session) {
        FSSLCreate sslcreate = s_SSL ? s_SSL->Create : 0;
        void* session;
        if (!sslcreate) {
            session = 0;
            x_error = 0;
        } else
            session = sslcreate(eSOCK_Client, x_sock, &x_error);
        if (!session) {
            const char* strerr = s_StrError(x_sock, x_error);
            CORE_LOGF_ERRNO_EXX(132, eLOG_Error,
                                x_error, strerr,
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed to initialize secure session",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            x_sock->sock = SOCK_INVALID;
            SOCK_Close(x_sock);
#ifdef NCBI_OS_MSWIN
            WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
            return eIO_NotSupported;
        }
        assert(session != SESSION_INVALID);
        x_sock->session = session;
    }

    if (x_sock->port) {
#ifdef SO_KEEPALIVE
        if (!s_SetKeepAlive(fd, x_sock->keepalive)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(153, eLOG_Warning,
                                x_error, strerr,
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(fd, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(138, eLOG_Warning,
                                x_error, strerr,
                                ("%s[SOCK::CreateOnTop] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(x_sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

    if (!s_SetCloexec(fd, !x_sock->crossexec  ||  x_sock->session)) {
        const char* strerr;
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        x_error = err;
#else
        x_error = errno;
        strerr = SOCK_STRERROR(x_error);
#endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(124, eLOG_Warning,
                            x_error, strerr ? strerr : "",
                            ("%s[SOCK::CreateOnTop] "
                             " Cannot modify socket close-on-exec mode",
                             s_ID(x_sock, _id)));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    memset(&lgr, 0, sizeof(lgr));
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*) &lgr, sizeof(lgr)) != 0){
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(43, eLOG_Warning,
                            x_error, strerr,
                            ("%s[SOCK::CreateOnTop] "
                             " Failed setsockopt(SO_NOLINGER)",
                             s_ID(x_sock, _id)));
        UTIL_ReleaseBuffer(strerr);
    }

    /* statistics & logging */
    if (x_sock->log == eOn  ||  (x_sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, x_sock, eIO_Open, 0, 0, 0);

    /* success */
    *sock = x_sock;
    return eIO_Success;
}


static EIO_Status s_CreateListening(const char*    path,
                                    unsigned short port,
                                    unsigned short backlog, 
                                    LSOCK*         lsock,
                                    TSOCK_Flags    flags)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } addr;
#ifdef NCBI_OS_UNIX
    mode_t          u;
#endif /*NCBI_OS_UNIX*/
    const char*     cp;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event;
#endif /*NCBI_OS_MSWIN*/
    TSOCK_Handle    x_lsock;
    int             x_error;
    TSOCK_socklen_t addrlen;
    char            _id[MAXIDLEN];
    unsigned int    x_id = ++s_ID_Counter;

    assert(!*lsock);
    assert(!path  ||  *path);

    memset(&addr, 0, sizeof(addr));
    if (path) {
#ifdef NCBI_OS_UNIX
        size_t pathlen = strlen(path);
        if (sizeof(addr.un.sun_path) <= pathlen++/*account for end '\0'*/) {
            CORE_LOGF_X(144, eLOG_Error,
                        ("LSOCK#%u[?]@%s: [LSOCK::Create] "
                         " Path too long (%lu vs %lu bytes allowed)",
                         x_id, path, (unsigned long) pathlen,
                         (unsigned long) sizeof(addr.un.sun_path)));
            return eIO_InvalidArg;
        }
        addr.sa.sa_family = AF_UNIX;
#else
        return eIO_NotSupported;
#endif /*NCBI_OS_UNIX*/
    } else
        addr.sa.sa_family = AF_INET;

    /* initialize internals */
    if (s_InitAPI(flags & fSOCK_Secure) != eIO_Success)
        return eIO_NotSupported;

    if (flags & fSOCK_Secure) {
        /*FIXME:  Add secure server support later*/
        return eIO_NotSupported;
    }

    /* create new(listening) socket */
    if ((x_lsock = socket(addr.sa.sa_family, SOCK_STREAM, 0)) == SOCK_INVALID){
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        if (!path) {
            if (port)
                sprintf(_id, ":%hu", port);
            else
                strcpy (_id, ":?");
            cp = _id;
        } else
            cp = path;
        CORE_LOGF_ERRNO_EXX(34, eLOG_Error,
                            x_error, strerr,
                            ("LSOCK#%u[?]@%s: [LSOCK::Create] "
                             " Failed socket()", x_id, cp));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }


    if (!path) {
        const char* failed = 0;
#if    defined(NCBI_OS_MSWIN)  &&  defined(SO_EXCLUSIVEADDRUSE)
        /* The use of this option comes with caveats, but it is better
         * to use it rather than having (or leaving) a chance for another
         * process (which uses SO_REUSEADDR, maliciously or not) be able
         * to bind to the same port number and snatch incoming connections.
         * Until a connection exists originated from the port with this
         * option set, the port (even if the listening instance was closed)
         * cannot be re-bound (important for service restarts!).  See MSDN.
         */
        BOOL excl = TRUE;
        if (setsockopt(x_lsock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       (const char*) &excl, sizeof(excl)) != 0) {
            failed = "EXCLUSIVEADDRUSE";
        }
#elif !defined(NCBI_OS_MSWIN)
        /*
         * It was confirmed(?) that at least on Solaris 2.5 this precaution:
         * 1) makes the address released immediately upon the process
         *    termination;
         * 2) still issues EADDRINUSE error on the attempt to bind() to the
         *    same address being in-use by a living process (if SOCK_STREAM).
         * 3) MS-Win treats SO_REUSEADDR completely differently in (as always)
         *    their own twisted way:  it *allows* to bind() to an already
         *    listening socket, which is why we jump the hoops above (also,
         *    note that SO_EXCLUSIVEADDRUSE == ~SO_REUSEADDR on MS-Win).
         */
        if (!s_SetReuseAddress(x_lsock, 1/*true*/))
            failed = "REUSEADDR";
#endif /*NCBI_OS_MSWIN...*/
        if (failed) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            if (port)
                sprintf(_id, "%hu", port);
            else
                strcpy (_id, "?");
            CORE_LOGF_ERRNO_EXX(35, eLOG_Error,
                                x_error, strerr,
                                ("LSOCK#%u[%u]@:%s: [LSOCK::Create] "
                                 " Failed setsockopt(%s)", x_id,
                                 (unsigned int) x_lsock, _id, failed));
            UTIL_ReleaseBuffer(strerr);
            SOCK_CLOSE(x_lsock);
            return eIO_Unknown;
        }
    }

    /* bind */
#ifdef NCBI_OS_UNIX
    if (path) {
        assert(addr.un.sun_family == AF_UNIX);
        addrlen = (TSOCK_socklen_t) sizeof(addr.un);
#  ifdef HAVE_SIN_LEN
        addr.un.sun_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
        strcpy(addr.un.sun_path, path);
        cp = path;
        u = umask(0);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        unsigned int host =
            flags & fSOCK_BindLocal ? SOCK_LOOPBACK : htonl(INADDR_ANY);
        assert(addr.in.sin_family == AF_INET);
        addrlen = sizeof(addr.in);
#ifdef HAVE_SIN_LEN
        addr.in.sin_len         = addrlen;
#endif /*HAVE_SIN_LEN*/
        addr.in.sin_addr.s_addr =       host;
        addr.in.sin_port        = htons(port);
#ifdef NCBI_OS_UNIX
        u = 0/*dummy*/;
#endif /*NCBI_OS_UNIX*/
    }
    x_error = bind(x_lsock, &addr.sa, addrlen) != 0 ? SOCK_ERRNO : 0;
#ifdef NCBI_OS_UNIX
    if (path)
        umask(u);
#endif /*NCBI_OS_UNIX*/
    if (x_error) {
        const char* strerr = SOCK_STRERROR(x_error);
        if (!path) {
            if (!port) {
                SOCK_ntoa(addr.in.sin_addr.s_addr, _id, sizeof(_id));
                strcat(_id + strlen(_id), ":?");
            } else {
                SOCK_HostPortToString(addr.in.sin_addr.s_addr, port,
                                      _id, sizeof(_id));
            }
            cp = _id;
        } else
            cp = path;
        CORE_LOGF_ERRNO_EXX(36, x_error != SOCK_EADDRINUSE
                            ? eLOG_Error : eLOG_Trace,
                            x_error, strerr,
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed bind()",
                             x_id, (unsigned int) x_lsock, cp));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_lsock);
        return x_error != SOCK_EADDRINUSE ? eIO_Unknown : eIO_Closed;
    }
    if (path)
#ifdef NCBI_OS_IRIX
        (void) fchmod(x_lsock, S_IRWXU | S_IRWXG | S_IRWXO)
#endif /*NCBI_OS_IRIX*/
            ;
    else if (!port) {
        assert(addr.in.sin_family == AF_INET);
        x_error = getsockname(x_lsock, &addr.sa, &addrlen) != 0
            ? SOCK_ERRNO : 0;
        if (x_error  ||  addr.sa.sa_family != AF_INET  ||  !addr.in.sin_port) {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(150, eLOG_Error,
                                x_error, strerr,
                                ("LSOCK#%u[%u]@:?: [LSOCK::Create] "
                                 " Cannot obtain free socket port",
                                 x_id, (unsigned int) x_lsock));
            UTIL_ReleaseBuffer(strerr);
            SOCK_CLOSE(x_lsock);
            return eIO_Closed;
        }
        port = ntohs(addr.in.sin_port);
    }
    assert((path  &&  !port)  ||
           (port  &&  !path));

#ifdef NCBI_OS_MSWIN
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        assert(!path);
        CORE_LOGF_ERRNO_EXX(118, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@:%hu: [LSOCK::Create] "
                             " Failed to create IO event",
                             x_id, (unsigned int) x_lsock, port));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(x_lsock, event, FD_CLOSE/*X*/ | FD_ACCEPT/*A*/) != 0) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        assert(!path);
        CORE_LOGF_ERRNO_EXX(119, eLOG_Error,
                            x_error, strerr,
                            ("LSOCK#%u[%u]@:%hu: [LSOCK::Create] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) x_lsock, port));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_lsock);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#else
    /* set non-blocking mode */
    if (!s_SetNonblock(x_lsock, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        if (!path) {
            sprintf(_id, ":%hu", port);
            cp = _id;
        } else
            cp = path;
        CORE_LOGF_ERRNO_EXX(38, eLOG_Error,
                            x_error, strerr,
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) x_lsock, cp));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }
#endif /*NCBI_OS_MSWIN*/

    /* listen */
    if (listen(x_lsock, backlog) != 0) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        if (!path) {
            sprintf(_id, ":%hu", port);
            cp = _id;
        } else
            cp = path;
        CORE_LOGF_ERRNO_EXX(37, eLOG_Error,
                            x_error, strerr,
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Failed listen(%hu)",
                             x_id, (unsigned int) x_lsock, cp, backlog));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_lsock);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* allocate memory for the internal socket structure */
    if (!(*lsock = (LSOCK)calloc(1, sizeof(**lsock) + (path?strlen(path):0)))){
        SOCK_CLOSE(x_lsock);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }
    (*lsock)->sock     = x_lsock;
    (*lsock)->id       = x_id;
    (*lsock)->port     = port;
    (*lsock)->type     = eListening;
    (*lsock)->log      = flags;
    (*lsock)->side     = eSOCK_Server;
    (*lsock)->keep     = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    (*lsock)->i_on_sig = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
#if   defined(NCBI_OS_UNIX)
    if (path)
        strcpy((*lsock)->path, path);
#elif defined(NCBI_OS_MSWIN)
    (*lsock)->event    = event;
#endif /*NCBI_OS*/

    if (!(flags & fSOCK_KeepOnExec)  &&  !s_SetCloexec(x_lsock, 1/*true*/)) {
        const char* strerr;
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        x_error = err;
#else
        x_error = errno;
        strerr = SOCK_STRERROR(x_error);
#endif /*NCBI_OS_MSWIN*/
        if (!path) {
            sprintf(_id, ":%hu", port);
            cp = _id;
        } else
            cp = path;
        CORE_LOGF_ERRNO_EXX(110, eLOG_Warning,
                            x_error, strerr ? strerr : "",
                            ("LSOCK#%u[%u]@%s: [LSOCK::Create] "
                             " Cannot set socket close-on-exec mode",
                             x_id, (unsigned int) x_lsock, cp));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    /* statistics & logging */
    if ((*lsock)->log == eOn  ||  ((*lsock)->log == eDefault && s_Log == eOn)){
        CORE_LOGF_X(115, eLOG_Note,
                    ("%sListening", s_ID((SOCK)(*lsock), _id)));
    }

    return eIO_Success;
}


/* Mimic SOCK_CLOSE() */
static void SOCK_ABORT(TSOCK_Handle x_sock)
{
    struct SOCK_tag temp;
    memset(&temp, 0, sizeof(temp));
    temp.side = eSOCK_Server;
    temp.type = eSocket;
    temp.sock = x_sock;
    s_Close_(&temp, -2/*silent abort*/);
}


static EIO_Status s_Accept(LSOCK           lsock,
                           const STimeout* timeout,
                           SOCK*           sock,
                           TSOCK_Flags     flags)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } addr;
    unsigned int    x_id;
    const char*     path;
    unsigned int    host;
    unsigned short  port;
#ifdef NCBI_OS_MSWIN
    WSAEVENT        event;
#endif /*NCBI_OS_MSWIN*/
    TSOCK_Handle    x_sock;
    int             x_error;
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

    x_id = (lsock->id * 1000 + ++s_ID_Counter) * 1000;

    /* accept next connection */
    memset(&addr, 0, sizeof(addr));
#ifdef NCBI_OS_UNIX
    if (lsock->path[0]) {
        addrlen = (TSOCK_socklen_t) sizeof(addr.un);
#  ifdef HAVE_SIN_LEN
        addr.un.sun_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
        assert(!lsock->port);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        addrlen = (TSOCK_socklen_t) sizeof(addr.in);
#ifdef HAVE_SIN_LEN
        addr.in.sin_len = addrlen;
#endif /*HAVE_SIN_LEN*/
        assert(lsock->port);
#ifdef NCBI_OS_MSWIN
        /* accept() [to follow shortly] resets IO event recording */
        lsock->readable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/
    }
    if ((x_sock = accept(lsock->sock, &addr.sa, &addrlen)) == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(40, eLOG_Error,
                            x_error, strerr,
                            ("%s[LSOCK::Accept] "
                             " Failed accept()",
                             s_ID((SOCK) lsock, _id)));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }
    lsock->n_accept++;

#ifdef NCBI_OS_UNIX
    if (lsock->path[0]) {
        assert(addr.un.sun_family == AF_UNIX);
        path = lsock->path;
        host = 0;
        port = 0;
    } else
#endif /*NCBI_OS_UNIX*/
    {
        assert(addr.in.sin_family == AF_INET);
        host =       addr.in.sin_addr.s_addr;
        port = ntohs(addr.in.sin_port);
        assert(port);
        path = "";
    }

#ifdef NCBI_OS_MSWIN
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(120, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Failed to create IO event",
                             x_id, (unsigned int) x_sock,
                             s_CP(host, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_ABORT(x_sock);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(x_sock, event, SOCK_EVENTS) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(121, eLOG_Error,
                            x_error, strerr,
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) x_sock,
                             s_CP(host, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBuffer(strerr);
        SOCK_ABORT(x_sock);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#else
    /* man accept(2) notes that non-blocking state may not be inherited */
    if (!s_SetNonblock(x_sock, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(41, eLOG_Error,
                            x_error, strerr,
                            ("SOCK#%u[%u]@%s: [LSOCK::Accept] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) x_sock,
                             s_CP(host, port, path, _id, sizeof(_id))));
        UTIL_ReleaseBuffer(strerr);
        SOCK_ABORT(x_sock);
        return eIO_Unknown;
    }
#endif /*NCBI_OS_MSWIN*/

    /* create new SOCK structure */
    addrlen = *path ? (TSOCK_socklen_t) strlen(path) : 0;
    if (!(*sock = (SOCK) calloc(1, sizeof(**sock) + addrlen))) {
        SOCK_ABORT(x_sock);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* success */
#ifdef NCBI_OS_UNIX
    if (!port) {
        assert(!lsock->port  &&  path[0]);
        strcpy((*sock)->path, path);
    } else
#endif /*NCBI_OS_UNIX*/
    {
        assert(!path[0]);
        (*sock)->host = host;
        (*sock)->port = port;
    }
    (*sock)->myport    = lsock->port;
    (*sock)->sock      = x_sock;
    (*sock)->id        = x_id;
    (*sock)->type      = eSocket;
    (*sock)->log       = flags;
    (*sock)->side      = eSOCK_Server;
    (*sock)->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    (*sock)->r_on_w    = flags & fSOCK_ReadOnWrite       ? eOn : eDefault;
    (*sock)->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
    (*sock)->r_status  = eIO_Success;
    (*sock)->w_status  = eIO_Success;
#ifdef NCBI_OS_MSWIN
    (*sock)->event     = event;
    (*sock)->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    (*sock)->connected = 1/*true*/;
    (*sock)->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    (*sock)->keepalive = flags & fSOCK_KeepAlive   ? 1/*true*/ : 0/*false*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    /* w_buf is unused for accepted sockets */

    if (port) {
        if (s_ReuseAddress == eOn  &&  !s_SetReuseAddress(x_sock, 1)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(42, eLOG_Warning,
                                x_error, strerr,
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(REUSEADDR)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }

#ifdef SO_KEEPALIVE
        if ((*sock)->keepalive  &&  !s_SetKeepAlive(x_sock, 1)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(152, eLOG_Warning,
                                x_error, strerr,
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(KEEPALIVE)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_KEEPALIVE*/
#ifdef SO_OOBINLINE
        if (!s_SetOobInline(x_sock, 1/*true*/)) {
            const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
            CORE_LOGF_ERRNO_EXX(137, eLOG_Warning,
                                x_error, strerr,
                                ("%s[LSOCK::Accept] "
                                 " Failed setsockopt(OOBINLINE)",
                                 s_ID(*sock, _id)));
            UTIL_ReleaseBuffer(strerr);
        }
#endif /*SO_OOBINLINE*/
    }

    if (!(*sock)->crossexec  &&  !s_SetCloexec(x_sock, 1/*true*/)) {
        const char* strerr;
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        x_error = err;
#else
        x_error = errno;
        strerr = SOCK_STRERROR(x_error);
#endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(128, eLOG_Warning,
                            x_error, strerr ? strerr : "",
                            ("%s[LSOCK::Accept] "
                             " Cannot set socket close-on-exec mode",
                             s_ID(*sock, _id)));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    /* statistics & logging */
    if ((*sock)->log == eOn  ||  ((*sock)->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, *sock, eIO_Open, 0, 0, "");

    return eIO_Success;
}


static EIO_Status s_CloseListening(LSOCK lsock)
{
    int        x_error;
    EIO_Status status;

    assert(lsock->sock != SOCK_INVALID);

#ifdef NCBI_OS_MSWIN
    WSAEventSelect(lsock->sock, lsock->event/*ignored*/, 0/*cancel*/);
#endif /*NCBI_OS_MSWIN*/

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn)) {
        char port[10];
        const char* c;
#ifdef NCBI_OS_UNIX
        if (lsock->path[0]) {
            assert(!lsock->port);
            c = lsock->path;
        } else
#endif /*NCBI_OS_UNIX*/ 
        {
            sprintf(port, ":%hu", lsock->port);
            c = port;
        }
        CORE_LOGF_X(44, eLOG_Note,
                    ("LSOCK#%u[%u]: %s at %s (%u accept%s total)",
                     lsock->id, (unsigned int) lsock->sock,
                     lsock->keep ? "Leaving" : "Closing", c,
                     lsock->n_accept, lsock->n_accept == 1 ? "" : "s"));
    }

    status = eIO_Success;
    if (!lsock->keep) {
#ifdef NCBI_OS_MSWIN
        assert(lsock->event);
        WSASetEvent(lsock->event); /*signal closure*/
#endif /*NCBI_OS_MSWIN*/
        for (;;) { /* close persistently - retry if interrupted */
            if (SOCK_CLOSE(lsock->sock) == 0)
                break;

            /* error */
            if (s_Initialized <= 0)
                break;
            x_error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
            if (x_error == WSANOTINITIALISED) {
                s_Initialized = -1/*deinited*/;
                break;
            }
#endif /*NCBI_OS_MSWIN*/
            if (x_error != SOCK_EINTR) {
                const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
                CORE_LOGF_ERRNO_EXX(45, eLOG_Error,
                                    x_error, strerr,
                                    ("LSOCK#%u[%u]: [LSOCK::Close] "
                                     " Failed close()",
                                     lsock->id, (unsigned int) lsock->sock));
                UTIL_ReleaseBuffer(strerr);
                status = eIO_Unknown;
                break;
            }
        }
    }

    /* cleanup & return */
    lsock->sock = SOCK_INVALID;
#if   defined(NCBI_OS_UNIX)
    if (!lsock->keep  &&  lsock->path[0]) {
        assert(!lsock->port);
        remove(lsock->path);
    }
#elif defined(NCBI_OS_MSWIN)
    WSACloseEvent(lsock->event);
    lsock->event = 0;
#endif /*NCBI_OS*/

    return status;
}


static EIO_Status s_RecvMsg(SOCK            sock,
                            void*           buf,
                            size_t          bufsize,
                            size_t          msgsize,
                            size_t*         msglen,
                            unsigned int*   sender_addr,
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

    for (;;) { /* auto-resume if either blocked or interrupted (optional) */
        int                x_error;
        int                x_read;
        struct sockaddr_in sin;
        TSOCK_socklen_t    sinlen = (TSOCK_socklen_t) sizeof(sin);
        memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SIN_LEN
        sin.sin_len = sinlen;
#endif
        x_read = recvfrom(sock->sock, x_msg,
#ifdef NCBI_OS_MSWIN
                          /*WINSOCK wants it weird*/ (int)
#endif /*NCBI_OS_MSWIN*/
                          x_msgsize, 0/*flags*/,
                          (struct sockaddr*) &sin, &sinlen);
#ifdef NCBI_OS_MSWIN
        /* recvfrom() resets IO event recording */
        sock->readable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/

        if (x_read >= 0) {
            /* got a message */
            sock->r_status = eIO_Success;
            sock->r_len = (TNCBI_BigCount) x_read;
            if ( msglen)
                *msglen = x_read;
            if ( sender_addr )
                *sender_addr =       sin.sin_addr.s_addr;
            if ( sender_port )
                *sender_port = ntohs(sin.sin_port);
            if ((size_t) x_read > bufsize
                &&  !BUF_Write(&sock->r_buf,
                               (char*) x_msg  + bufsize,
                               (size_t)x_read - bufsize)) {
                CORE_LOGF_X(20, eLOG_Error,
                            ("%s[DSOCK::RecvMsg] "
                             " Message truncated: %lu/%u",
                             s_ID(sock, w), (unsigned long) bufsize, x_read));
                status = eIO_Unknown;
            } else
                status = eIO_Success;
            if (bufsize  &&  x_msgsize > bufsize)
                memcpy(buf, x_msg, bufsize);

            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(eLOG_Note, sock, eIO_Read, x_msg,
                        (size_t) x_read, &sin);
            }

            sock->n_read += (TNCBI_BigCount) x_read;
            sock->n_in++;
            break;
        }

        x_error = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        if (x_error == SOCK_EWOULDBLOCK  ||  x_error == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, SOCK_GET_TIMEOUT(sock, r), 1/*asis*/);
            assert(poll.event == eIO_Read);
            if (status != eIO_Success)
                break;
            if (poll.revent != eIO_Close) {
                assert(poll.revent == eIO_Read);
                continue/*read again*/;
            }
        } else if (x_error == SOCK_EINTR) {
            if (sock->i_on_sig == eOn  ||
                (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
                sock->r_status = status = eIO_Interrupt;
                break;
            }
            continue;
        } else {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(94, eLOG_Trace,
                                x_error, strerr,
                                ("%s[DSOCK::RecvMsg] "
                                 " Failed recvfrom()",
                                 s_ID(sock, w)));
            UTIL_ReleaseBuffer(strerr);
        }
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->r_status = status = eIO_Unknown;
        break;
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
    size_t             x_msgsize;
    char               w[1536];
    EIO_Status         status;
    unsigned short     x_port;
    unsigned int       x_host;
    void*              x_msg;
    struct sockaddr_in sin;

    if (datalen) {
        status = s_Write(sock, data, datalen, &x_msgsize, 0);
        if (status != eIO_Success) {
            CORE_LOGF_ERRNO_X(154, eLOG_Error, errno,
                              ("%s[DSOCK::SendMsg] "
                               " Failed to finalize message (%lu byte%s)",
                               s_ID(sock, w), (unsigned long) datalen,
                               &"s"[datalen == 1]));
            return status;
        }
        verify(x_msgsize == datalen);
    } else
        sock->w_len = 0;
    sock->eof = 1/*true - finalized message*/;

    x_port = port ? port : sock->port;
    if (!host  ||  !*host)
        x_host = sock->host;
    else if (!(x_host = s_gethostbyname(host, (ESwitch) sock->log))) {
        CORE_LOGF_X(88, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Failed SOCK_gethostbyname(\"%.*s\")",
                     s_ID(sock, w), MAXHOSTNAMELEN, host));
        return eIO_Unknown;
    }

    if (!x_host  ||  !x_port) {
        SOCK_HostPortToString(x_host, x_port, w, sizeof(w)/2);
        CORE_LOGF_X(89, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Address \"%s\" incomplete, missing %s",
                     s_ID(sock, w + sizeof(w)/2), w,
                     x_port ? "host" : &"host:port"[x_host ? 5 : 0]));
         return eIO_Unknown;
    }

    if ((x_msgsize = BUF_Size(sock->w_buf)) != 0) {
        if (x_msgsize <= sizeof(w))
            x_msg = w;
        else if (!(x_msg = malloc(x_msgsize)))
            return eIO_Unknown;
        verify(BUF_Peek(sock->w_buf, x_msg, x_msgsize) == x_msgsize);
    } else
        x_msg = 0;

    memset(&sin, 0, sizeof(sin));
#ifdef HAVE_SIN_LEN
    sin.sin_len         = (TSOCK_socklen_t) sizeof(sin);
#endif /*HAVE_SIN_LEN*/
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr =       x_host;
    sin.sin_port        = htons(x_port);

    for (;;) { /* optionally auto-resume if interrupted */
        int x_error;
        int x_written;

        if ((x_written = sendto(sock->sock, x_msg,
#ifdef NCBI_OS_MSWIN
                                /*WINSOCK wants it weird*/ (int)
#endif /*NCBI_OS_MSWIN*/
                                x_msgsize, 0/*flags*/,
                                (struct sockaddr*) &sin, sizeof(sin))) >= 0) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(eLOG_Note, sock, eIO_Write, x_msg,
                        (size_t) x_written, &sin);
            }

            sock->w_len      = (TNCBI_BigCount) x_written;
            sock->n_written += (TNCBI_BigCount) x_written;
            sock->n_out++;
            if ((size_t) x_written != x_msgsize) {
                sock->w_status = status = eIO_Closed;
                if (!host  &&  !port)
                    w[0] = '\0';
                else
                    SOCK_HostPortToString(x_host, x_port, w, sizeof(w)/2);
                CORE_LOGF_X(90, eLOG_Error,
                            ("%s[DSOCK::SendMsg] "
                             " Partial datagram sent (%lu out of %lu)%s%s",
                             s_ID(sock, w + sizeof(w)/2),
                             (unsigned long) x_written,
                             (unsigned long) x_msgsize, *w ? " to " : "", w));
                break;
            }
            sock->w_status = status = eIO_Success;
            break;
        }

#ifdef NCBI_OS_MSWIN
        /* special sendto()'s semantics of IO recording reset */
        sock->writable = 0/*false*/;
#endif /*NCBI_OS_MSWIN*/

        x_error = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        if (x_error == SOCK_EWOULDBLOCK  ||  x_error == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, SOCK_GET_TIMEOUT(sock, w), 1/*asis*/);
            assert(poll.event == eIO_Write);
            if (status != eIO_Success)
                break;
            if (poll.revent != eIO_Close) {
                assert(poll.revent == eIO_Write);
                continue;
            }
        } else if (x_error == SOCK_EINTR) {
            if (sock->i_on_sig == eOn  ||
                (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
                sock->w_status = status = eIO_Interrupt;
                break;
            }
            continue;
        } else {
            const char* strerr = SOCK_STRERROR(x_error);
            if (!host  &&  !port)
                w[0] = '\0';
            else
                SOCK_HostPortToString(x_host, x_port, w, sizeof(w)/2);
            CORE_LOGF_ERRNO_EXX(91, eLOG_Trace,
                                x_error, strerr,
                                ("%s[DSOCK::SendMsg] "
                                 " Failed sendto(%s)",
                                 s_ID(sock, w + sizeof(w)/2), w));
            UTIL_ReleaseBuffer(strerr);
        }
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = status = eIO_Unknown;
        break;
    }

    if (x_msg  &&  x_msg != w)
        free(x_msg);
    if (status == eIO_Success)
        BUF_Erase(sock->w_buf);
    return status;
}



/******************************************************************************
 *  TRIGGER
 */


extern EIO_Status TRIGGER_Create(TRIGGER* trigger, ESwitch log)
{
    unsigned int x_id = ++s_ID_Counter;

    *trigger = 0;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return eIO_NotSupported;

#ifdef NCBI_CXX_TOOLKIT

#  if defined(NCBI_OS_UNIX)
    {{
        int fd[3];

        if (pipe(fd) != 0) {
            CORE_LOGF_ERRNO_X(28, eLOG_Error, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Cannot create pipe", x_id));
            return eIO_Closed;
        }

#    ifdef FD_SETSIZE
        if ((fd[2] = fcntl(fd[1], F_DUPFD, FD_SETSIZE)) < 0) {
            /* We don't need "out" to be selectable, so move it out
             * of the way to spare precious "selectable" fd numbers */
            CORE_LOGF_ERRNO_X(143, eLOG_Warning, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Failed to dup(%d) to higher fd(%d+))",
                               x_id, fd[1], FD_SETSIZE));
        } else {
            close(fd[1]);
            fd[1] = fd[2];
        }
#    endif /*FD_SETSIZE*/

        if (!s_SetNonblock(fd[0], 1/*true*/)  ||
            !s_SetNonblock(fd[1], 1/*true*/)) {
            CORE_LOGF_ERRNO_X(29, eLOG_Error, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Failed to set non-blocking mode", x_id));
            close(fd[0]);
            close(fd[1]);
            return eIO_Closed;
        }

        if (!s_SetCloexec(fd[0], 1/*true*/)  ||
            !s_SetCloexec(fd[1], 1/*true*/)) {
            CORE_LOGF_ERRNO_X(30, eLOG_Warning, errno,
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
        (*trigger)->type     = eTrigger;
        (*trigger)->log      = log;
        (*trigger)->i_on_sig = eDefault;

        /* statistics & logging */
        if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
            CORE_LOGF_X(116, eLOG_Note,
                        ("TRIGGER#%u[%u, %u]: Ready", x_id, fd[0], fd[1]));
        }
    }}

    return eIO_Success;

#  elif defined(NCBI_OS_MSWIN)

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
        (*trigger)->type     = eTrigger;
        (*trigger)->log      = log;
        (*trigger)->i_on_sig = eDefault;

        /* statistics & logging */
        if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
            CORE_LOGF_X(116, eLOG_Note,
                        ("TRIGGER#%u: Ready", x_id));
        }
    }}

    return eIO_Success;

#  else

    CORE_LOGF_X(31, eLOG_Error, ("TRIGGER#%u[?]: [TRIGGER::Create] "
                                 " Not yet supported on this platform", x_id));
    return eIO_NotSupported;

#  endif /*NCBI_OS*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Close(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

    /* statistics & logging */
    if (trigger->log == eOn  ||  (trigger->log == eDefault  &&  s_Log == eOn)){
        CORE_LOGF_X(117, eLOG_Note,
                    ("TRIGGER#%u[%u]: Closing", trigger->id, trigger->fd));
    }

#  if   defined(NCBI_OS_UNIX)

    /* Prevent SIGPIPE by closing in this order:  writing end first */
    close(trigger->out);
    close(trigger->fd);

#  elif defined(NCBI_OS_MSWIN)

    WSACloseEvent(trigger->fd);

#  endif /*NCBI_OS*/

    free(trigger);
    return eIO_Success;

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Set(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

#  if   defined(NCBI_OS_UNIX)

    if (!NCBI_SwapPointers((void**) &trigger->isset.ptr, (void*) 1/*true*/)) {
        if (write(trigger->out, "", 1) < 0  &&  errno != EAGAIN)
            return eIO_Unknown;
    }

    return eIO_Success;

#  elif defined(NCBI_OS_MSWIN)

    return WSASetEvent(trigger->fd) ? eIO_Success : eIO_Unknown;

#  else

    CORE_LOG_X(32, eLOG_Error,
               "[TRIGGER::Set] "
               " Not yet supported on this platform");
    return eIO_NotSupported;

#  endif /*NCBI_OS*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_IsSet(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

#  if   defined(NCBI_OS_UNIX)

#    ifdef PIPE_SIZE
#      define MAX_TRIGGER_BUF PIPE_SIZE
#    else
#      define MAX_TRIGGER_BUF 8192
#    endif /*PIPE_SIZE*/

    static char x_buf[MAX_TRIGGER_BUF];
    ssize_t     x_read;

    while ((x_read = read(trigger->fd, x_buf, sizeof(x_buf))) > 0)
        trigger->isset.ptr = (void*) 1/*true*/;

    if (x_read == 0/*EOF?*/)
        return eIO_Unknown;

    return trigger->isset.ptr ? eIO_Success : eIO_Closed;

#  elif defined(NCBI_OS_MSWIN)

    switch (WaitForSingleObject(trigger->fd, 0)) {
    case WAIT_OBJECT_0:
        return eIO_Success;
    case WAIT_TIMEOUT:
        return eIO_Closed;
    default:
        break;
    }

    return eIO_Unknown;

#  else

    CORE_LOG_X(33, eLOG_Error,
               "[TRIGGER::IsSet] "
               " Not yet supported on this platform");
    return eIO_NotSupported;

#  endif /*NCBI_OS*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Reset(TRIGGER trigger)
{
    EIO_Status status = TRIGGER_IsSet(trigger);

#if   defined(NCBI_OS_UNIX)

    trigger->isset.ptr = (void*) 0/*false*/;

#elif defined(NCBI_OS_MSWIN)

    if (!WSAResetEvent(trigger->fd))
        return eIO_Unknown;

#endif /*NCBI_OS*/

    return status == eIO_Closed ? eIO_Success : status;
}



/******************************************************************************
 *  LISTENING SOCKET
 */


extern EIO_Status LSOCK_Create(unsigned short port,
                               unsigned short backlog,
                               LSOCK*         lsock)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock, fSOCK_LogDefault);
}


extern EIO_Status LSOCK_CreateEx(unsigned short port,
                                 unsigned short backlog,
                                 LSOCK*         lsock,
                                 TSOCK_Flags    flags)
{
    *lsock = 0;
    return s_CreateListening(0, port, backlog, lsock, flags);
}


extern EIO_Status LSOCK_CreateUNIX(const char*    path,
                                   unsigned short backlog,
                                   LSOCK*         lsock,
                                   TSOCK_Flags    flags)
{
    *lsock = 0;
    if (!path  ||  !*path)
        return eIO_InvalidArg;
    return s_CreateListening(path, 0, backlog, lsock, flags);
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
                     handle ? (unsigned long) handle_size : 0));
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
    port = lsock->sock != SOCK_INVALID ? lsock->port : 0;
    return byte_order == eNH_HostByteOrder ? port : htons(port);
}



/******************************************************************************
 *  SOCKET
 */


extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port, 
                              const STimeout* timeout,
                              SOCK*           sock)
{
    *sock = 0;
    if (!host  ||  !port)
        return eIO_InvalidArg;
    return s_Create(host, port, timeout, sock, 0, 0, fSOCK_LogDefault);
}


extern EIO_Status SOCK_CreateEx(const char*     host,
                                unsigned short  port,
                                const STimeout* timeout,
                                SOCK*           sock,
                                const void*     data,
                                size_t          size,
                                TSOCK_Flags     flags)
{
    *sock = 0;
    if (!host  ||  !port)
        return eIO_InvalidArg;
    return s_Create(host, port, timeout, sock, data, size, flags);
}


extern EIO_Status SOCK_CreateUNIX(const char*     path,
                                  const STimeout* timeout,
                                  SOCK*           sock,
                                  const void*     data,
                                  size_t          size,
                                  TSOCK_Flags     flags)
{
    *sock = 0;
    if (!path  ||  !*path)
        return eIO_InvalidArg;
#ifdef NCBI_OS_UNIX
    return s_Create(path, 0, timeout, sock, data, size, flags);
#else
    return eIO_NotSupported;
#endif /*NCBI_OS_UNIX*/
}


extern EIO_Status SOCK_CreateOnTopEx(const void* handle,
                                     size_t      handle_size,
                                     SOCK*       sock,
                                     const void* data,
                                     size_t      size,
                                     TSOCK_Flags flags)
{
    *sock = 0;
    if (!handle_size) {
        TSOCK_Handle fd     = SOCK_INVALID;
        SOCK         xsock  = (SOCK) handle;
        EIO_Status   status = SOCK_GetOSHandleEx(xsock, &fd, sizeof(fd),
                                                 eTakeOwnership);
        if (status == eIO_Success) {
            assert(fd != SOCK_INVALID);
            SOCK_CloseEx(xsock, 0/*do not destroy*/);
            status  = s_CreateOnTop(&fd, sizeof(fd), sock,
                                    data, size, flags);
            if (status != eIO_Success) {
                SOCK_CloseOSHandle(&fd, sizeof(fd));
                assert(!*sock);
            } else
                assert(*sock);
        } else {
            if (xsock  &&  fd != SOCK_INVALID)
                SOCK_Abort(xsock);
            SOCK_CloseEx(xsock, 0/*do not destroy*/);
        }
        return status;
    }
    return s_CreateOnTop(handle, handle_size, sock, data, size, flags);
}


extern EIO_Status SOCK_CreateOnTop(const void* handle,
                                   size_t      handle_size,
                                   SOCK*       sock)
{
    *sock = 0;
    return SOCK_CreateOnTopEx(handle, handle_size, sock, 0,0,fSOCK_LogDefault);
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    char _id[MAXIDLEN];

    if (sock->type == eDatagram) {
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
    if (sock->side == eSOCK_Server) {
        if (!host  ||  !port) {
            CORE_LOGF_X(51, eLOG_Error,
                        ("%s[SOCK::Reconnect] "
                         " Attempt to reconnect server-side socket as"
                         " client one to its peer address",
                         s_ID(sock, _id)));
            return eIO_InvalidArg;
        }
    }

    /* close the socket if necessary */
    if (sock->sock != SOCK_INVALID)
        s_Close(sock);

    /* connect */
    sock->id++;
    sock->myport    = 0;
    sock->side      = eSOCK_Client;
    sock->n_read    = 0;
    sock->n_written = 0;
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
    if (sock->type == eDatagram) {
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
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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


extern EIO_Status SOCK_Close(SOCK sock)
{
    return SOCK_CloseEx(sock, 1/*destroy*/);
}


extern EIO_Status SOCK_CloseEx(SOCK sock, int/*bool*/ destroy)
{
    EIO_Status status;
    if (!sock)
        return eIO_InvalidArg;
    if (sock->sock == SOCK_INVALID)
        status = eIO_Closed;
    else if (s_Initialized > 0)        
        status = s_Close(sock);
    else {
        sock->sock = SOCK_INVALID;
        status = eIO_Success;
    }
    if (destroy) {
        BUF_Destroy(sock->r_buf);
        BUF_Destroy(sock->w_buf);
        free(sock);
    }
    return status;
}


extern EIO_Status SOCK_CloseOSHandle(const void* handle, size_t handle_size)
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
        int x_error;

        if (SOCK_CLOSE(fd) == 0)
            break;

        /* error */
        if (s_Initialized <= 0)
            break;
        x_error = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
        if (x_error == WSANOTINITIALISED) {
            s_Initialized = -1/*deinited*/;
            break;
        }
#endif /*NCBI_OS_MSWIN*/
        if (x_error == SOCK_ENOTCONN    ||
            x_error == SOCK_ENETRESET   ||
            x_error == SOCK_ECONNRESET  ||
            x_error == SOCK_ECONNABORTED) {
            break;
        }
        if (x_error != SOCK_EINTR) {
            status = x_error == SOCK_ETIMEDOUT ? eIO_Timeout : eIO_Unknown;
            break;
        }
        /* Maybe in an Ex version of this call someday...
        if (s_InterruptOnSignal) {
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

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(56, eLOG_Error,
                    ("%s[SOCK::Wait] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }

    /* check against already shutdown socket there */
    switch (event) {
    case eIO_Open:
        if (sock->type == eDatagram)
            return eIO_Success/*always connected*/;
        if (sock->pending) {
            struct timeval tv;
            return s_IsConnected(sock, s_to2tv(timeout, &tv));
        }
        if (sock->r_status == eIO_Success  &&  sock->w_status == eIO_Success)
            return eIO_Success;
        if (sock->r_status == eIO_Closed   &&  sock->w_status == eIO_Closed)
            return eIO_Closed;
        return eIO_Unknown;

    case eIO_Read:
        if (BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->type == eDatagram)
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
        if (sock->type == eDatagram)
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
        if (sock->type == eDatagram  ||  BUF_Size(sock->r_buf) != 0)
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
        return eIO_InvalidArg;
    }

    assert(sock->type == eSocket);
    status = s_Wait(sock, event, timeout);
    if (s_ErrHook  &&  status != eIO_Success  &&  status != eIO_Timeout) {
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        if (sock->port) {
            char addr[40];
            SOCK_ntoa(sock->host, addr, sizeof(addr));
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
    struct timeval tv;
    size_t         i;

    if (n  &&  !polls) {
        if ( n_ready )
            *n_ready = 0;
        return eIO_InvalidArg;
    }

    for (i = 0;  i < n;  i++) {
        SOCK sock = polls[i].sock;
        polls[i].revent =
            sock  &&  sock->type == eTrigger  &&  ((TRIGGER) sock)->isset.ptr
            ? polls[i].event
            : eIO_Open;
        if (!sock  ||  !(sock->type & eSocket)  ||  sock->sock == SOCK_INVALID)
            continue;
        if ((polls[i].event & eIO_Read)  &&  BUF_Size(sock->r_buf) != 0) {
            polls[i].revent = eIO_Read;
            continue;
        }
        if (sock->type != eSocket)
            continue;
        if ((polls[i].event == eIO_Read
             &&  (sock->r_status == eIO_Closed  ||  sock->eof))  ||
            (polls[i].event == eIO_Write
             &&   sock->w_status == eIO_Closed)) {
            polls[i].revent = eIO_Close;
        }
    }

    return s_SelectStallsafe(n, polls, s_to2tv(timeout, &tv), n_ready);
}


extern EIO_Status SOCK_SetTimeout(SOCK            sock,
                                  EIO_Event       event,
                                  const STimeout* timeout)
{
    char _id[MAXIDLEN];

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
            /* timeouts stored normalized */
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
    return 0;
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
        switch (how) {
        case eIO_ReadPeek:
            status = s_Read(sock, buf, size, &x_read, 1/*peek*/);
            break;

        case eIO_ReadPlain:
            status = s_Read(sock, buf, size, &x_read, 0/*read*/);
            break;

        case eIO_ReadPersist:
            x_read = 0;
            do {
                size_t xx_read;
                status = s_Read(sock, (char*) buf + (buf ? x_read : 0),
                                size, &xx_read, 0/*read*/);
                x_read += xx_read;
                size   -= xx_read;
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
        status = eIO_Closed;
        x_read = 0;
    }

    if ( n_read )
        *n_read = x_read;
    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static EIO_Status s_PushBack(SOCK sock, const void* buf, size_t size)
{
    return BUF_PushBack(&sock->r_buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_ReadLine(SOCK    sock,
                                char*   line,
                                size_t  size,
                                size_t* n_read)
{
    int/*bool*/ cr_seen, done;
    EIO_Status  status;
    size_t      len;

    if (n_read)
        *n_read = 0;

    if (sock->sock == SOCK_INVALID) {
        char _id[MAXIDLEN];
        CORE_LOGF_X(125, eLOG_Error,
                    ("%s[SOCK::ReadLine] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }

    cr_seen = done = 0/*false*/;
    status = eIO_Success;
    len = 0;
    do {
        size_t i;
        char   w[1024], c;
        size_t x_size = BUF_Size(sock->r_buf);
        char*  x_buf  = size - len < sizeof(w) - cr_seen ? w : line + len;
        if (!x_size  ||  x_size > sizeof(w) - cr_seen)
            x_size = sizeof(w) - cr_seen;
        status = s_Read(sock, x_buf + cr_seen, x_size, &x_size, 0/*read*/);
        assert(status == eIO_Success  ||  !x_size);
        if (!x_size)
            done = 1/*true*/;
        else if (cr_seen)
            x_size++;
        i = cr_seen;
        while (i < x_size  &&  len < size) {
            c = x_buf[i++];
            if (c == '\n') {
                cr_seen = 0/*false*/;
                done = 1/*true*/;
                break;
            }
            if (c == '\r'  &&  !cr_seen) {
                cr_seen = 1/*true*/;
                continue;
            }
            if (cr_seen)
                line[len++] = '\r';
            cr_seen = 0/*false*/;
            if (len >= size) {
                --i; /* have to read it again */
                break;
            }
            if (c == '\r') {
                cr_seen = 1/*true*/;
                continue;
            } else if (!c) {
                done = 1/*true*/;
                break;
            }
            line[len++] = c;
        }
        if (len >= size)
            done = 1/*true*/;
        if (done  &&  cr_seen) {
            c = '\r';
            if (s_PushBack(sock, &c, 1) != eIO_Success)
                status = eIO_Unknown;
        }
        if (i < x_size
            &&  s_PushBack(sock, &x_buf[i], x_size - i) != eIO_Success) {
            status = eIO_Unknown;
        }
    } while (!done  &&  status == eIO_Success);

    if (len < size)
        line[len] = '\0';
    if (n_read)
        *n_read = len;

    return status;
}


extern EIO_Status SOCK_PushBack(SOCK        sock,
                                const void* buf,
                                size_t      size)
{
    if (sock->sock == SOCK_INVALID) {
        char _id[MAXIDLEN];
        CORE_LOGF_X(67, eLOG_Error,
                    ("%s[SOCK::PushBack] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }

    return s_PushBack(sock, buf, size);
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
                return eIO_Closed;
            if (sock->pending)
                return eIO_Timeout;
            if (direction == eIO_Read) {
                return sock->type == eSocket  &&  sock->eof
                    ? eIO_Closed : (EIO_Status) sock->r_status;
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
                             const void*     buf,
                             size_t          size,
                             size_t*         n_written,
                             EIO_WriteMethod how)
{
    EIO_Status status;
    size_t     x_written;
    char       _id[MAXIDLEN];
    
    if (sock->sock != SOCK_INVALID) {
        switch (how) {
        case eIO_WriteOutOfBand:
            if (sock->type == eDatagram) {
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
            status = s_Write(sock, buf, size, &x_written,
                             how == eIO_WriteOutOfBand ? 1 : 0);
            break;

        case eIO_WritePersist:
            x_written = 0;
            do {
                size_t xx_written;
                status = s_Write(sock, (char*) buf + x_written,
                                 size, &xx_written, 0);
                x_written += xx_written;
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
    if (sock->type == eDatagram) {
        CORE_LOGF_X(72, eLOG_Error,
                    ("%s[SOCK::Abort] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    sock->eof = 0;
    sock->w_len = 0;
    sock->pending = 0;
    return s_Close_(sock, 1/*abort*/);
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
            sock->myport = port;
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
    if (!sock) {
        if ( host )
            *host = 0;
        if ( port )
            *port = 0;
        return;
    }
    if ( host ) {
        *host = byte_order == eNH_HostByteOrder
            ? ntohl(sock->host) :       sock->host;
    }
    if ( port ) {
        *port = byte_order == eNH_HostByteOrder
            ?       sock->port  : ntohs(sock->port);
    }
}


extern unsigned short SOCK_GetRemotePort(SOCK          sock,
                                         ENH_ByteOrder byte_order)
{
    unsigned short port;
    SOCK_GetPeerAddress(sock, 0, &port, byte_order);
    return port;
}


extern char* SOCK_GetPeerAddressString(SOCK   sock,
                                       char*  buf,
                                       size_t bufsize)
{
    return SOCK_GetPeerAddressStringEx(sock, buf, bufsize, eSAF_Full);
}


extern char* SOCK_GetPeerAddressStringEx(SOCK                sock,
                                         char*               buf,
                                         size_t              bufsize,
                                         ESOCK_AddressFormat format)
{
    char   port[10];
    size_t len;

    if (!buf  ||  !bufsize)
        return 0/*error*/;
    if (!sock) {
        *buf = '\0';
        return 0/*error*/;
    }
    switch (format) {
    case eSAF_Full:
#ifdef NCBI_OS_UNIX
        if (sock->path[0]) {
            size_t len = strlen(sock->path);
            if (len < bufsize)
                memcpy(buf, sock->path, len + 1);
            else
                return 0;
        } else
#endif /*NCBI_OS_UNIX*/
            if (!SOCK_HostPortToString(sock->host, sock->port, buf, bufsize))
                return 0/*error*/;
        break;
    case eSAF_Port:
#ifdef NCBI_OS_UNIX
        if (sock->path[0]) 
            *buf = '\0';
        else
#endif /*NCBI_OS_UNIX*/
            if ((len = (size_t) sprintf(port, "%hu", sock->port)) >= bufsize)
                return 0/*error*/;
            else
                memcpy(buf, port, len + 1);
        break;
    case eSAF_IP:
#ifdef NCBI_OS_UNIX
        if (sock->path[0]) 
            *buf = '\0';
        else
#endif /*NCBI_OS_UNIX*/
            if (SOCK_ntoa(sock->host, buf, bufsize) != 0)
                return 0/*error*/;
        break;
    default:
        return 0/*error*/;
    }
    return buf;
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
                     handle ? (unsigned long) handle_size : 0));
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
        status = s_Close(sock);
        assert(sock->sock == SOCK_INVALID);
    }
    return status;
}


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle,
                                   size_t handle_size)
{
    return SOCK_GetOSHandleEx(sock, handle, handle_size, eNoOwnership);
}


extern ESwitch SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    ESwitch old = s_ReadOnWrite;
    if (on_off != eDefault)
        s_ReadOnWrite = on_off;
    return old;
}


extern ESwitch SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off)
{
    if (sock->type != eDatagram) {
        ESwitch old = (ESwitch) sock->r_on_w;
        sock->r_on_w = on_off;
        return old;
    }
    return eDefault;
}


/*ARGSUSED*/
extern void SOCK_SetCork(SOCK sock, int/*bool*/ on_off)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(158, eLOG_Warning,
                    ("%s[SOCK::SetCork] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return;
    }
    if (sock->type == eDatagram) {
        CORE_LOGF_X(159, eLOG_Error,
                    ("%s[SOCK::SetCork] "
                     " Datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return;
    }

#ifdef TCP_CORK
    if (setsockopt(sock->sock, IPPROTO_TCP, TCP_CORK,
                   (char*) &on_off, sizeof(on_off)) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(160, eLOG_Warning,
                            x_error, strerr,
                            ("%s[SOCK::SetCork] "
                             " Failed setsockopt(%sTCP_CORK)",
                             s_ID(sock, _id), on_off ? "" : "!"));
        UTIL_ReleaseBuffer(strerr);
    }
#endif /*TCP_CORK*/
}


/*ARGSUSED*/
extern void SOCK_DisableOSSendDelay(SOCK sock, int/*bool*/ on_off)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(156, eLOG_Warning,
                    ("%s[SOCK::DisableOSSendDelay] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return;
    }
    if (sock->type == eDatagram) {
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
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(75, eLOG_Warning,
                            x_error, strerr,
                            ("%s[SOCK::DisableOSSendDelay] "
                             " Failed setsockopt(%sTCP_NODELAY)",
                             s_ID(sock, _id), on_off ? "" : "!"));
        UTIL_ReleaseBuffer(strerr);
    }
#endif /*TCP_NODELAY*/
}



/******************************************************************************
 *  DATAGRAM SOCKET
 */


extern EIO_Status DSOCK_Create(SOCK* sock)
{
    return DSOCK_CreateEx(sock, fSOCK_LogDefault);
}


extern EIO_Status DSOCK_CreateEx(SOCK* sock, TSOCK_Flags flags)
{
#ifdef NCBI_OS_MSWIN
    HANDLE       event;
#endif /*NCBI_OS_MSWIN*/
    TSOCK_Handle x_sock;
    int          x_error;
    unsigned int x_id = ++s_ID_Counter * 1000;

    *sock = 0;

    /* initialize internals */
    if ((flags & fSOCK_Secure)  ||  s_InitAPI(0) != eIO_Success)
        return eIO_NotSupported;

    /* create new datagram socket */
    if ((x_sock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCK_INVALID) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(76, eLOG_Error,
                            x_error, strerr,
                            ("DSOCK#%u[?]: [DSOCK::Create] "
                             " Cannot create socket",
                             x_id));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }

#ifdef NCBI_OS_MSWIN
    if (!(event = WSACreateEvent())) {
        DWORD err = GetLastError();
        const char* strerr = s_WinStrerror(err);
        CORE_LOGF_ERRNO_EXX(139, eLOG_Error,
                            err, strerr ? strerr : "",
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Failed to create IO event",
                             x_id, (unsigned int) x_sock));
        UTIL_ReleaseBufferOnHeap(strerr);
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }
    /* NB: WSAEventSelect() sets non-blocking automatically */
    if (WSAEventSelect(x_sock, event, SOCK_EVENTS) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(140, eLOG_Error,
                            x_error, strerr,
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Failed to bind IO event",
                             x_id, (unsigned int) x_sock));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_sock);
        WSACloseEvent(event);
        return eIO_Unknown;
    }
#else
    /* set to non-blocking mode */
    if (!s_SetNonblock(x_sock, 1/*true*/)) {
        const char* strerr = SOCK_STRERROR(x_error = SOCK_ERRNO);
        CORE_LOGF_ERRNO_EXX(77, eLOG_Error,
                            x_error, strerr,
                            ("DSOCK#%u[%u]: [DSOCK::Create] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) x_sock));
        UTIL_ReleaseBuffer(strerr);
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }
#endif /*NCBI_OS_MSWIN*/

    if (!(*sock = (SOCK) calloc(1, sizeof(**sock)))) {
        SOCK_CLOSE(x_sock);
#ifdef NCBI_OS_MSWIN
        WSACloseEvent(event);
#endif /*NCBI_OS_MSWIN*/
        return eIO_Unknown;
    }

    /* success... */
    (*sock)->sock      = x_sock;
    (*sock)->id        = x_id;
    /* no host and port - not "connected" */
    (*sock)->type      = eDatagram;
    (*sock)->log       = flags;
    (*sock)->side      = eSOCK_Client;
    (*sock)->keep      = flags & fSOCK_KeepOnClose ? 1/*true*/ : 0/*false*/;
    (*sock)->i_on_sig  = flags & fSOCK_InterruptOnSignal ? eOn : eDefault;
    (*sock)->r_status  = eIO_Success;
    (*sock)->w_status  = eIO_Success;
#ifdef NCBI_OS_MSWIN
    (*sock)->event     = event;
    (*sock)->writable  = 1/*true*/;
#endif /*NCBI_OS_MSWIN*/
    (*sock)->crossexec = flags & fSOCK_KeepOnExec  ? 1/*true*/ : 0/*false*/;
    /* all timeout bits cleared - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    BUF_SetChunkSize(&(*sock)->w_buf, SOCK_BUF_CHUNK_SIZE);

    if (!(*sock)->crossexec  &&  !s_SetCloexec(x_sock, 1/*true*/)) {
        const char* strerr;
        char _id[MAXIDLEN];
#ifdef NCBI_OS_MSWIN
        DWORD err = GetLastError();
        strerr = s_WinStrerror(err);
        x_error = err;
#else
        x_error = errno;
        strerr = SOCK_STRERROR(x_error);
#endif /*NCBI_OS_MSWIN*/
        CORE_LOGF_ERRNO_EXX(130, eLOG_Warning,
                            x_error, strerr ? strerr : "",
                            ("%s[DSOCK::Create]  Cannot set"
                             " socket close-on-exec mode",
                             s_ID(*sock, _id)));
#ifdef NCBI_OS_MSWIN
        UTIL_ReleaseBufferOnHeap(strerr);
#else
        UTIL_ReleaseBuffer(strerr);
#endif /*NCBI_OS_MSWIN*/
    }

    /* statistics & logging */
    if ((*sock)->log == eOn  ||  ((*sock)->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, *sock, eIO_Open, 0, 0, 0);

    return eIO_Success;
}


extern EIO_Status DSOCK_Bind(SOCK sock, unsigned short port)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
    } addr;
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(79, eLOG_Error,
                    ("%s[DSOCK::Bind] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eDatagram) {
        CORE_LOGF_X(78, eLOG_Error,
                    ("%s[DSOCK::Bind] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    /* bind */
    memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SIN_LEN
    addr.in.sin_len         = (TSOCK_socklen_t) sizeof(addr.in);
#endif /*HAVE_SIN_LEN*/
    addr.in.sin_family      = AF_INET;
    addr.in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.in.sin_port        = htons(port);
    if (bind(sock->sock, &addr.sa, sizeof(addr.in)) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(80, x_error != SOCK_EADDRINUSE
                            ? eLOG_Error : eLOG_Trace,
                            x_error, strerr,
                            ("%s[DSOCK::Bind] "
                             " Failed bind(:%hu)",
                             s_ID(sock, _id), port));
        UTIL_ReleaseBuffer(strerr);
        return x_error != SOCK_EADDRINUSE ? eIO_Unknown : eIO_Closed;
    }
    if (!port) {
        int x_error;
        TSOCK_socklen_t addrlen = sizeof(addr);
        assert(addr.sa.sa_family == AF_INET);
        x_error = getsockname(sock->sock, &addr.sa, &addrlen) != 0
            ? SOCK_ERRNO : 0;
        if (x_error  ||  addr.sa.sa_family != AF_INET  ||  !addr.in.sin_port) {
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(114, eLOG_Error,
                                x_error, strerr,
                                ("%s[DSOCK::Bind] "
                                 " Cannot obtain free socket port",
                                 s_ID(sock, _id)));
            UTIL_ReleaseBuffer(strerr);
            return eIO_Closed;
        }
        port = ntohs(addr.in.sin_port);
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, 0, 0, &addr.in);

    sock->myport = port;
    return eIO_Success;
}


extern EIO_Status DSOCK_Connect(SOCK sock,
                                const char* hostname, unsigned short port)
{
    struct sockaddr_in peer;
    char _id[MAXIDLEN];
    unsigned int host;
    char addr[40];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(82, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eDatagram) {
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

    if (!hostname  ||  !*hostname)
        host = 0;
    else if (!(host = s_gethostbyname(hostname, (ESwitch) sock->log))) {
        CORE_LOGF_X(83, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Failed SOCK_gethostbyname(\"%.*s\")",
                     s_ID(sock, _id), MAXHOSTNAMELEN, hostname));
        return eIO_Unknown;
    }

    if (!host != !port) {
        if (port) {
            assert(!host);
            sprintf(addr, ":%hu", port);
        } else
            *addr = '\0';
        CORE_LOGF_X(84, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Address \"%.*s%s\" incomplete, missing %s",
                     s_ID(sock, _id), MAXHOSTNAMELEN, host ? hostname : "",
                     addr, port ? "host" : "port"));
        return eIO_InvalidArg;
    }

    /* connect (non-empty address) or drop association (on empty address) */
    memset(&peer, 0, sizeof(peer));
#ifdef HAVE_SIN_LEN
    peer.sin_len             = (TSOCK_socklen_t) sizeof(peer);
#endif /*HAVE_SIN_LEN*/
    if (host/*  &&  port*/) {
        peer.sin_family      = AF_INET;
        peer.sin_addr.s_addr =       host;
        peer.sin_port        = htons(port);
    }
#ifdef AF_UNSPEC
    else
        peer.sin_family      = AF_UNSPEC;
#endif /*AF_UNSPEC*/
    if (connect(sock->sock, (struct sockaddr*) &peer, sizeof(peer)) != 0) {
        int x_error = SOCK_ERRNO;
        const char* strerr = SOCK_STRERROR(x_error);
        if (host)
            SOCK_HostPortToString(host, port, addr, sizeof(addr));
        else
            *addr = '\0';
        CORE_LOGF_ERRNO_EXX(85, eLOG_Error,
                            x_error, strerr,
                            ("%s[DSOCK::Connect] "
                             " Failed %sconnect%s%s%s",
                             s_ID(sock, _id), *addr ? "" : "to dis",
                             &"("[!*addr], addr, &")"[!*addr]));
        UTIL_ReleaseBuffer(strerr);
        return eIO_Unknown;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(eLOG_Note, sock, eIO_Open, "", 0, &peer);

    sock->host = host;
    sock->port = port;
    return eIO_Success;
}


extern EIO_Status DSOCK_WaitMsg(SOCK sock, const STimeout* timeout)
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
        return eIO_Closed;
    }
    if (sock->type != eDatagram) {
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
    if (status != eIO_Success  ||  poll.revent == eIO_Read)
        return status;
    assert(poll.revent == eIO_Close);
    return eIO_Unknown;
}


extern EIO_Status DSOCK_RecvMsg(SOCK            sock,
                                void*           buf,
                                size_t          bufsize,
                                size_t          msgsize,
                                size_t*         msglen,
                                unsigned int*   sender_addr,
                                unsigned short* sender_port)
{
    char       _id[MAXIDLEN];
    EIO_Status status;

    if ( msglen )
        *msglen = 0;
    if ( sender_addr )
        *sender_addr = 0;
    if ( sender_port )
        *sender_port = 0;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(93, eLOG_Error,
                    ("%s[DSOCK::RecvMsg] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eDatagram) {
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
        char          addr[40];
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        SOCK_ntoa(sock->host, addr, sizeof(addr));
        info.host =       addr;
        info.port = sock->port;
        info.event = eIO_Read;
        info.status = status;
        s_ErrorCallback(&info);
    }
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
    if (sock->type != eDatagram) {
        CORE_LOGF_X(86, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    status = s_SendMsg(sock, host, port, data, datalen);
    if (s_ErrHook  &&  status != eIO_Success) {
        char          addr[40];
        SSOCK_ErrInfo info;
        memset(&info, 0, sizeof(info));
        info.type = eSOCK_ErrIO;
        info.sock = sock;
        SOCK_ntoa(sock->host, addr, sizeof(addr));
        info.host =       addr;
        info.port = sock->port;
        info.event = eIO_Write;
        info.status = status;
        s_ErrorCallback(&info);
    }
    return status;
}


extern EIO_Status DSOCK_WipeMsg(SOCK sock, EIO_Event direction)
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
    if (sock->type != eDatagram) {
        CORE_LOGF_X(97, eLOG_Error,
                    ("%s[DSOCK::WipeMsg] "
                     " Not a datagram socket",
                     s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    switch (direction) {
    case eIO_Read:
        BUF_Erase(sock->r_buf);
        sock->r_len = 0;
        sock->r_status = status = eIO_Success;
        break;
    case eIO_Write:
        BUF_Erase(sock->w_buf);
        sock->w_len = 0;
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


extern EIO_Status DSOCK_SetBroadcast(SOCK sock, int/*bool*/ broadcast)
{
    char _id[MAXIDLEN];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(101, eLOG_Error,
                    ("%s[DSOCK::SetBroadcast] "
                     " Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->type != eDatagram) {
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
        BOOL bcast = !!broadcast;
#  else
        int  bcast = !!broadcast;
#  endif /*NCBI_OS_MSWIN*/
        if (setsockopt(sock->sock, SOL_SOCKET, SO_BROADCAST,
                       (const char*) &bcast, sizeof(bcast)) != 0) {
            int x_error = SOCK_ERRNO;
            const char* strerr = SOCK_STRERROR(x_error);
            CORE_LOGF_ERRNO_EXX(102, eLOG_Error,
                                x_error, strerr,
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


extern TNCBI_BigCount DSOCK_GetMessageCount(SOCK sock, EIO_Event direction)
{
    if (sock  &&  sock->type == eDatagram) {
        switch (direction) {
        case eIO_Read:
            return sock->n_in;
        case eIO_Write:
            return sock->n_out;
        default:
            break;
        }
    }
    return 0;
}



/******************************************************************************
 *  CLASSIFICATION & STATS
 */


extern int/*bool*/ SOCK_IsDatagram(SOCK sock)
{
    return sock &&  sock->sock != SOCK_INVALID  &&  sock->type == eDatagram;
}


extern int/*bool*/ SOCK_IsClientSide(SOCK sock)
{
    return sock &&  sock->sock != SOCK_INVALID  &&  sock->side == eSOCK_Client;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock &&  sock->sock != SOCK_INVALID  &&  sock->side == eSOCK_Server;
}


extern int/*bool*/ SOCK_IsUNIX(SOCK sock)
{
#ifdef NCBI_OS_UNIX
    return sock &&  sock->sock != SOCK_INVALID  &&  sock->path[0];
#else
    return 0/*false*/;
#endif /*NCBI_OS_UNIX*/
}


extern int/*bool*/ SOCK_IsSecure(SOCK sock)
{
    return sock &&  sock->sock != SOCK_INVALID  &&  sock->session;
}


extern TNCBI_BigCount SOCK_GetPosition(SOCK sock, EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            if (sock->type == eDatagram)
                return sock->r_len - BUF_Size(sock->r_buf);
            return sock->n_read    - (TNCBI_BigCount) BUF_Size(sock->r_buf);
        case eIO_Write:
            if (sock->type == eDatagram)
                return BUF_Size(sock->w_buf);
            return sock->n_written + (TNCBI_BigCount)          sock->w_len;
        default:
            break;
        }
    }
    return 0;
}


extern TNCBI_BigCount SOCK_GetCount(SOCK sock, EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            return sock->type == eDatagram ? sock->r_len : sock->n_read;
        case eIO_Write:
            return sock->type == eDatagram ? sock->w_len : sock->n_written;
        default:
            break;
        }
    }
    return 0;
}


extern TNCBI_BigCount SOCK_GetTotalCount(SOCK sock, EIO_Event direction)
{
    if (sock) {
        switch (direction) {
        case eIO_Read:
            return sock->type != eDatagram ? sock->n_in  : sock->n_read;
        case eIO_Write:
            return sock->type != eDatagram ? sock->n_out : sock->n_written;
        default:
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


extern ESwitch SOCK_SetInterruptOnSignal(SOCK sock, ESwitch on_off)
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


extern void SOCK_SetReuseAddress(SOCK sock, int/*bool*/ on_off)
{
    if (sock->sock != SOCK_INVALID && !s_SetReuseAddress(sock->sock, on_off)) {
        int x_error = SOCK_ERRNO;
        char _id[MAXIDLEN];
        const char* strerr = SOCK_STRERROR(x_error);
        CORE_LOGF_ERRNO_EXX(74, eLOG_Warning,
                            x_error, strerr,
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


extern ESwitch SOCK_SetDataLogging(SOCK sock, ESwitch log)
{
    ESwitch old = (ESwitch) sock->log;
    sock->log = log;
    return old;
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


extern POLLABLE POLLABLE_FromTRIGGER(TRIGGER trigger)
{
    assert(!trigger  ||  trigger->type == eTrigger);
    return (POLLABLE) trigger;
}


extern POLLABLE POLLABLE_FromLSOCK(LSOCK lsock)
{
    assert(!lsock  ||  lsock->type == eListening);
    return (POLLABLE) lsock;
}


extern POLLABLE POLLABLE_FromSOCK(SOCK sock)
{
    assert(!sock  ||  (sock->type & eSocket));
    return (POLLABLE) sock;
}


extern TRIGGER POLLABLE_ToTRIGGER(POLLABLE poll)
{
    TRIGGER trigger = (TRIGGER) poll;
    return trigger  &&  trigger->type == eTrigger ? trigger : 0;
}


extern LSOCK POLLABLE_ToLSOCK(POLLABLE poll)
{
    LSOCK lsock = (LSOCK) poll;
    return lsock  &&  lsock->type == eListening ? lsock : 0;
}


extern SOCK  POLLABLE_ToSOCK(POLLABLE poll)
{
    SOCK sock = (SOCK) poll;
    return sock  &&  (sock->type & eSocket) ? sock : 0;
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
            memcpy(buf, x_buf, len + 1);
            return 0/*success*/;
        }
        buf[0] = '\0';
    }
    return -1/*failed*/;
}


extern int/*bool*/ SOCK_isip(const char* str)
{
    return SOCK_isipEx(str, 0/*nofullquad*/);
}


extern int/*bool*/ SOCK_isipEx(const char* str, int/*bool*/ fullquad)
{
    unsigned long val;
    int dots;

    if (!str)
        return 0/*false*/;

    dots = 0;
    for (;;) {
        char* e;
        if (!isdigit((unsigned char)(*str)))
            return 0/*false*/;
        errno = 0;
        val = strtoul(str, &e, fullquad ? 10 : 0);
        if (errno  ||  str == e)
            return 0/*false*/;
        str = e;
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


extern int/*bool*/ SOCK_IsLoopbackAddress(unsigned int ip)
{
    /* 127/8 */
    if (ip) {
        unsigned int addr = ntohl(ip);
#if defined(IN_CLASSA) && defined(IN_CLASSA_NET) && defined(IN_CLASSA_NSHIFT)
        return IN_CLASSA(addr)
            &&  (addr & IN_CLASSA_NET) == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT);
#else
        return !((addr & 0xFF000000) ^ (INADDR_LOOPBACK-1));
#  endif /*IN_CLASSA && IN_CLASSA_NET && IN_CLASSA_NSHIFT*/
    }
    return 0/*false*/;
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


extern int SOCK_gethostnameEx(char* buf, size_t bufsize, ESwitch log)
{
    assert(buf  &&  bufsize > 0);

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success) {
        buf[0] = buf[bufsize - 1] = 0;
        return -1/*failure*/;
    }

    return s_gethostname(buf, bufsize, log);
}


extern int SOCK_gethostname(char* buf, size_t bufsize)
{
    return SOCK_gethostnameEx(buf, bufsize, s_Log);
}


extern unsigned int SOCK_gethostbynameEx(const char* hostname, ESwitch log)
{
    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return 0;

    return s_gethostbyname(hostname, log);
}


extern unsigned int SOCK_gethostbyname(const char* hostname)
{
    return SOCK_gethostbynameEx(hostname, s_Log);
}


extern const char* SOCK_gethostbyaddrEx(unsigned int host,
                                        char*        buf,
                                        size_t       bufsize,
                                        ESwitch      log)
{
    assert(buf  &&  bufsize > 0);

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success) {
        buf[0] = '\0';
        return 0;
    }

    return s_gethostbyaddr(host, buf, bufsize, log);
}


extern const char* SOCK_gethostbyaddr(unsigned int host,
                                      char*        buf,
                                      size_t       bufsize)
{
    return SOCK_gethostbyaddrEx(host, buf, bufsize, s_Log);
}


extern unsigned int SOCK_GetLoopbackAddress(void)
{
    return SOCK_LOOPBACK;
}


extern unsigned int SOCK_GetLocalHostAddress(ESwitch reget)
{
    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return 0;

    return s_getlocalhostaddress(reget, s_Log);
}


extern const char* SOCK_StringToHostPort(const char*     str,
                                         unsigned int*   host,
                                         unsigned short* port)
{
    char x_buf[MAXHOSTNAMELEN + 1];
    unsigned short p;
    unsigned int h;
    const char* s;
    size_t len;
    size_t n;

    if ( host )
        *host = 0;
    if ( port )
        *port = 0;
    if (!*str)
        return 0;

    /* initialize internals */
    if (s_InitAPI(0) != eIO_Success)
        return 0;

    for (s = str;  *s;  s++) {
        if (isspace((unsigned char)(*s))  ||  *s == ':')
            break;
    }
    if ((len = (size_t)(s - str)) > sizeof(x_buf) - 1)
        return 0;
    if (*s == ':') {
        long  i;
        char* e;
        if (isspace((unsigned char) s[1]))
            return str;
        errno = 0;
        i = strtol(++s, &e, 10);
        if (errno  ||  s == e  ||  i ^ (i & 0xFFFF)
            ||  (*e  &&  !isspace((unsigned char)(*e)))) {
            return str;
        }
        p = (unsigned short) i;
        n = (size_t)(e - s);
    } else {
        p = 0;
        n = 0;
    }
    if (len) {
        memcpy(x_buf, str, len);
        x_buf[len] = '\0';
        if (!(h = s_gethostbyname(x_buf, s_Log)))
            return str;
        if ( host )
            *host = h;
    }
    if (port  &&  p)
        *port = p;
    return s + n;
}


extern size_t SOCK_HostPortToString(unsigned int   host,
                                    unsigned short port,
                                    char*          buf,
                                    size_t         bufsize)
{
    char   x_buf[16/*sizeof("255.255.255.255")*/ + 6/*:port#*/];
    size_t len;

    if (!buf  ||  !bufsize)
        return 0;
    if (!host) {
        *x_buf = '\0';
        len = 0;
    } else if (SOCK_ntoa(host, x_buf, sizeof(x_buf)) != 0) {
        *buf = '\0';
        return 0;
    } else
        len = strlen(x_buf);
    if (port  ||  !host)
        len += sprintf(x_buf + len, ":%hu", port);
    assert(len < sizeof(x_buf));
    if (len >= bufsize) {
        *buf = '\0';
        return 0;
    }
    memcpy(buf, x_buf, len + 1);
    return len;
}



/******************************************************************************
 *  SECURE SOCKET LAYER
 */


extern void SOCK_SetupSSL(FSSLSetup setup)
{
    s_SSLSetup = setup;
}
