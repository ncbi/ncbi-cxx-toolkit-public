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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Plain portable TCP/IP socket API for:  UNIX, MS-Win, MacOS
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    wsock32.lib
 *     [MacOS]   -DNCBI_OS_MAC      NCSASOCK -- BSD-style socket emulation lib
 *
 */

#include "ncbi_config.h"

/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC!"
#endif


/* Uncomment these(or specify "-DHAVE_GETADDRINFO -DHAVE_GETNAMEINFO") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "getaddrinfo()" and "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
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
 */

/* #define HAVE_SIN_LEN 1 */


/* Platform-specific system headers
 */
#if defined(NCBI_OS_UNIX)
#  include <sys/time.h>
#  include <unistd.h>
#  include <netdb.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  ifndef NCBI_OS_BEOS
#    include <arpa/inet.h>
#  endif
#  include <signal.h>

#elif defined(NCBI_OS_MSWIN)
#  include <winsock2.h>

#elif defined(NCBI_OS_MAC)
#  include <unistd.h>
#  include <sock_ext.h>
#  include <netdb.h>
#  include <s_types.h>
#  include <s_socket.h>
#  include <neti_in.h>
#  include <a_inet.h>
#  include <neterrno.h> /* missing error numbers on Mac */

#else
#  error "Unsupported platform, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC !!!"
#endif /* platform-specific headers (for UNIX, MSWIN, MAC) */


/* NCBI core headers
 */
#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_socket.h>


/* Portable standard C headers
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>




/******************************************************************************
 *  TYPEDEF & MACRO
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE 4096


/* Macro #def for the platform-dependent constants, error codes and functions
 */
#if defined(NCBI_OS_MSWIN)

typedef SOCKET TSOCK_Handle;
#  define SOCK_INVALID        INVALID_SOCKET
#  define SOCK_ERRNO          WSAGetLastError()
#  define SOCK_EINTR          WSAEINTR
#  define SOCK_EWOULDBLOCK    WSAEWOULDBLOCK
#  define SOCK_ECONNRESET     WSAECONNRESET
#  define SOCK_EPIPE          WSAESHUTDOWN
#  define SOCK_EAGAIN         WSAEINPROGRESS
#  define SOCK_EINPROGRESS    WSAEINPROGRESS
#  define SOCK_ENOTCONN       WSAENOTCONN
#  define SOCK_NFDS(s)        0
#  define SOCK_CLOSE(s)       closesocket(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    SD_RECEIVE
#  define SOCK_SHUTDOWN_WR    SD_SEND
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_MSWIN */

#elif defined(NCBI_OS_UNIX)

typedef int TSOCK_Handle;
#  define SOCK_INVALID        (-1)
#  define SOCK_ERRNO          errno
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_NFDS(s)        (s + 1)
#  ifdef NCBI_OS_BEOS
#    define SOCK_CLOSE(s)     closesocket(s)
#  else
#    define SOCK_CLOSE(s)     close(s)	
#  endif
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  if !defined(SHUT_RD)
#    define SHUT_RD           0
#  endif
#  define SOCK_SHUTDOWN_RD    SHUT_RD
#  if !defined(SHUT_WR)
#    define SHUT_WR           1
#  endif
#  define SOCK_SHUTDOWN_WR    SHUT_WR
#  ifndef INADDR_NONE
#    define INADDR_NONE       (unsigned int)(-1)
#  endif
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_UNIX */

#elif defined(NCBI_OS_MAC)

#  if TARGET_API_MAC_CARBON
#    define O_NONBLOCK kO_NONBLOCK
#  endif

typedef int TSOCK_Handle;
#  define SOCK_INVALID        (-1)
#  ifndef SOCK_ERRNO
#    define SOCK_ERRNO        errno
#  endif
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_NFDS(s)        (s + 1)
#  define SOCK_CLOSE(s)       close(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    0
#  define SOCK_SHUTDOWN_WR    1
#  define SOCK_STRERROR(err)  s_StrError(err)
#  ifdef NETDB_INTERNAL
#    undef NETDB_INTERNAL
#  endif /*NETDB_INTERNAL*/

#endif /*NCBI_OS_MSWIN, NCBI_OS_UNIX, NCBI_OS_MAC*/


/* Listening socket
 */
typedef struct LSOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
} LSOCK_struct;


/* Type of socket (except listening)
 */
typedef enum {
    eSOCK_ClientSide = 0,
    eSOCK_ServerSide,
    eSOCK_Datagram
} ESockType;


/* Socket
 */
typedef struct SOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
    struct timeval* r_timeout;  /* zero (if infinite), or points to "r_tv"   */
    struct timeval  r_tv;       /* finite read timeout value                 */
    STimeout        r_to;       /* finite read timeout value (aux., temp.)   */
    struct timeval* w_timeout;  /* zero (if infinite), or points to "w_tv"   */
    struct timeval  w_tv;       /* finite write timeout value                */
    STimeout        w_to;       /* finite write timeout value (aux., temp.)  */
    struct timeval* c_timeout;  /* zero (if infinite), or points to "c_tv"   */
    struct timeval  c_tv;       /* finite close timeout value                */
    STimeout        c_to;       /* finite close timeout value (aux., temp.)  */
    unsigned int    host;       /* peer host (in the network byte order)     */
    unsigned short  port;       /* peer port (in the network byte order)     */
    ESwitch         r_on_w;     /* enable/disable automatic read-on-write    */
    ESwitch         i_on_sig;   /* enable/disable I/O restart on signals     */
    BUF             r_buf;      /* read  buffer                              */
    BUF             w_buf;      /* write buffer, for DATAGRAM sockets only   */

    /* current status and EOF indicator */
    int/*bool*/     is_eof;     /* Stream sockets: 'End of file' seen on read
                                   Datagram socks: 'End of message' written  */
    EIO_Status      r_status;   /* read  status:  eIO_Closed if was shutdown */
    EIO_Status      w_status;   /* write status:  eIO_Closed if was shutdown */

    /* for the tracing/statistics */
    unsigned int    id;         /* the internal ID (see also "s_ID_Counter") */
    ESwitch         log_data;
    size_t          n_read;
    size_t          n_written;

    ESockType       type;       /* socket type: client- or server-side, dgram*/
} SOCK_struct;


/* Globals:
 */


/* Flag to indicate whether the API has been initialized */
static int/*bool*/ s_Initialized = 0/*false*/;

/* SOCK counter */
static unsigned int s_ID_Counter = 0;

/* Read-while-writing switch */
static ESwitch s_ReadOnWrite = eOff;       /* no read-on-write by default */

/* I/O restart on signals */
static ESwitch s_InterruptOnSignal = eOff; /* restart I/O by default      */

/* Data logging */
static ESwitch s_LogData = eOff;           /* no data logging by default  */

/* Flag to indicate whether API should mask SIGPIPE (during initialization) */
#ifdef NCBI_OS_UNIX
static int/*bool*/ s_AllowSigPipe = 0/*false - mask SIGPIPE out*/;
#endif /*NCBI_OS_UNIX*/


/******************************************************************************
 *   Error reporting
 */


static const char* s_StrError(int error)
{
    static struct {
        int         errnum;
        const char* errtxt;
    } errmap[] = {
#ifdef NCBI_OS_MSWIN
        {WSAEINTR,  "Interrupted system call"},
        {WSAEBADF,  "Bad file number"},
        {WSAEACCES, "Access denied"},
        {WSAEFAULT, "Segmentation fault"},
        {WSAEINVAL, "Invalid agrument"},
        {WSAEMFILE, "Too many open files"},
        /*
         * Windows Sockets definitions of regular Berkeley error constants
         */
        {WSAEWOULDBLOCK,     "Resource temporarily unavailable"},
        {WSAEINPROGRESS,     "Operation now in progress"},
        {WSAEALREADY,        "Operation already in progress"},
        {WSAENOTSOCK,        "Not a socket"},
        {WSAEDESTADDRREQ,    "Destination address required"},
        {WSAEMSGSIZE,        "Invalid message size"},
        {WSAEPROTOTYPE,      "Wrong protocol type"},
        {WSAENOPROTOOPT,     "Bad protocol option"},
        {WSAEPROTONOSUPPORT, "Protocol not supported"},
        {WSAESOCKTNOSUPPORT, "Socket type not supported"},
        {WSAEOPNOTSUPP,      "Operation not supported"},
        {WSAEPFNOSUPPORT,    "Protocol family not supported"},
        {WSAEAFNOSUPPORT,    "Address family not supported"},
        {WSAEADDRINUSE,      "Address already in use"},
        {WSAEADDRNOTAVAIL,   "Cannot assign requested address"},
        {WSAENETDOWN,        "Network is down"},
        {WSAENETUNREACH,     "Network is unreachable"},
        {WSAENETRESET,       "Connection dropped on network reset"},
        {WSAECONNABORTED,    "Software caused connection abort"},
        {WSAECONNRESET,      "Connection reset by peer"},
        {WSAENOBUFS,         "No buffer space available"},
        {WSAEISCONN,         "Socket is already connected"},
        {WSAENOTCONN,        "Socket is not connected"},
        {WSAESHUTDOWN,       "Cannot send after socket shutdown"},
        {WSAETOOMANYREFS,    "Too many references"},
        {WSAETIMEDOUT,       "Operation timed out"},
        {WSAECONNREFUSED,    "Connection refused"},
        {WSAELOOP,           "Infinite loop"},
        {WSAENAMETOOLONG,    "Name too long"},
        {WSAEHOSTDOWN,       "Host is down"},
        {WSAEHOSTUNREACH,    "Host unreachable"},
        {WSAENOTEMPTY,       "Not empty"},
        {WSAEPROCLIM,        "Too many processes"},
        {WSAEUSERS,          "Too many users"},
        {WSAEDQUOT,          "Quota exceeded"},
        {WSAESTALE,          "Stale descriptor"},
        {WSAEREMOTE,         "Remote error"},
        /*
         * Extended Windows Sockets error constant definitions
         */
        {WSASYSNOTREADY,         "Network subsystem is unavailable"},
        {WSAVERNOTSUPPORTED,     "Winsock.dll version out of range"},
        {WSANOTINITIALISED,      "Not yet initialized"},
        {WSAEDISCON,             "Graceful shutdown in progress"},
        {WSAENOMORE,             "No more retries"},
        {WSAECANCELLED,          "Cancelled"},
        {WSAEINVALIDPROCTABLE,   "Invalid procedure table"},
        {WSAEINVALIDPROVIDER,    "Invalid provider version number"},
        {WSAEPROVIDERFAILEDINIT, "Cannot init provider"},
        {WSASYSCALLFAILURE,      "System call failed"},
        {WSASERVICE_NOT_FOUND,   "Service not found"},
        {WSATYPE_NOT_FOUND,      "Class type not found"},
        {WSA_E_NO_MORE,          "WSA_E_NO_MORE"},
        {WSA_E_CANCELLED,        "WSA_E_CANCELLED"},
        {WSAEREFUSED,            "Refused"},
#endif /*NCBI_OS_MSWIN*/
#ifdef NCBI_OS_MSWIN
#  define EAI_BASE 0
#else
#  define EAI_BASE 100000
#endif
#ifdef EAI_ADDRFAMILY
        {EAI_ADDRFAMILY + EAI_BASE,
                                 "Address family not supported"},
#endif /*EAI_ADDRFAMILY*/
#ifdef EAI_AGAIN
        {EAI_AGAIN + EAI_BASE,
                                 "Temporary failure in name resolution"},
#endif /*EAI_AGAIN*/
#ifdef EAI_BADFLAGS
        {EAI_BADFLAGS + EAI_BASE,
                                 "Invalid value for lookup flags"},
#endif /*EAI_BADFLAGS*/
#ifdef EAI_FAIL
        {EAI_FAIL + EAI_BASE,
                                 "Non-recoverable failure in name resolution"},
#endif /*EAI_FAIL*/
#ifdef EAI_FAMILY
        {EAI_FAMILY + EAI_BASE,
                                 "Address family not supported"},
#endif /*EAI_FAMILY*/
#ifdef EAI_MEMORY
        {EAI_MEMORY + EAI_BASE,
                                 "Memory allocation failure"},
#endif /*EAI_MEMORY*/
#ifdef EAI_NODATA
        {EAI_NODATA + EAI_BASE,
                                 "No address associated with nodename"},
#endif /*EAI_NODATA*/
#ifdef EAI_NONAME
        {EAI_NONAME + EAI_BASE,
                                 "Host/service name not known"},
#endif /*EAI_NONAME*/
#ifdef EAI_SERVICE
        {EAI_SERVICE + EAI_BASE,
                                 "Service name not supported for socket type"},
#endif /*EAI_SERVICE*/
#ifdef EAI_SOCKTYPE
        {EAI_SOCKTYPE + EAI_BASE,
                                 "Socket type not supported"},
#endif /*EAI_SOCKTYPE*/
#ifdef NCBI_OS_MSWIN
#  define DNS_BASE 0
#else
#  define DNS_BASE 200000
#endif
#ifdef HOST_NOT_FOUND
        {HOST_NOT_FOUND + DNS_BASE,
                                 "Host not found"},
#endif /*HOST_NOT_FOUND*/
#ifdef TRY_AGAIN
        {TRY_AGAIN + DNS_BASE,
                                 "DNS server failure"},
#endif /*TRY_AGAIN*/
#ifdef NO_RECOVERY
        {NO_RECOVERY + DNS_BASE,
                                 "Unrecoverable DNS error"},
#endif /*NO_RECOVERY*/
#ifdef NO_DATA
        {NO_DATA + DNS_BASE,
                                 "No DNS data of requested type"},
#endif /*NO_DATA*/
#ifdef NO_ADDRESS
        {NO_ADDRESS + DNS_BASE,
                                 "No address record found in DNS"},
#endif /*NO_ADDRESS*/

        /* Last dummy entry - must present */
        {0, 0}
    };
    size_t i, n = sizeof(errmap)/sizeof(errmap[0]) - 1/*dummy entry*/;

    /* always called on error, so get error number here if not having already*/
    if ( !error )
        error = errno;
    for (i = 0; i < n; i++) {
        if (errmap[i].errnum == error)
            return errmap[i].errtxt;
    }
    return strerror(error);
}


/******************************************************************************
 *   Data Logging
 */


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLogData
(SOCK sock, EIO_Event event, const void* data, size_t size)
{
    const struct sockaddr_in* sin;
    unsigned int host;
    char message[128];

    if ( !CORE_GetLOG() )
        return;

    switch (event) {
    case eIO_Open:
        sin = (struct sockaddr_in*) data;
        memcpy(&host, &sin->sin_addr, sizeof(host));
        if (SOCK_ntoa(host, message, sizeof(message)) != 0)
            strcpy(message, "<unknown>");
        CORE_LOGF(eLOG_Trace, ("SOCK#%u[%u] -- connecting to %s:%hu",
                               (unsigned int) sock->id,
                               (unsigned int) size,
                               message, ntohs(sin->sin_port)));
        break;
    case eIO_Read:
    case eIO_Write:
        sprintf(message, "SOCK#%u[%u] -- %s at offset %lu",
                (unsigned int) sock->id, (unsigned int) sock->sock,
                (event == eIO_Read) ? "read" : "written",
                (unsigned long) ((event == eIO_Read) ?
                                 sock->n_read : sock->n_written));
        CORE_DATA(data, size, message);
        break;
    case eIO_Close:
        CORE_LOGF(eLOG_Trace, ("SOCK#%u[%u] -- closing "
                               "(%lu byte%s out, %lu byte%s in)",
                               (unsigned int) sock->id,
                               (unsigned int) sock->sock,
                               (unsigned long) sock->n_written,
                               sock->n_written == 1 ? "" : "s",
                               (unsigned long) sock->n_read,
                               sock->n_read == 1 ? "" : "s"));
        break;
    default:
        CORE_LOG(eLOG_Error, "[SOCK::s_DoLogData]  Invalid event");
        assert(0);
        break;
    }
}


extern void SOCK_SetDataLoggingAPI(ESwitch log_data)
{
    if (log_data == eDefault)
        log_data = eOff;
    s_LogData = log_data;
}


extern void SOCK_SetDataLogging(SOCK sock, ESwitch log_data)
{
    sock->log_data = log_data;
}



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


extern void SOCK_AllowSigPipeAPI(void)
{
#ifdef NCBI_OS_UNIX
    s_AllowSigPipe = 1/*true - API will not mask SIGPIPE out at init*/;
#endif /*NCBI_OS_UNIX*/
    return;
}


extern EIO_Status SOCK_InitializeAPI(void)
{
    CORE_LOCK_WRITE;

    if ( s_Initialized ) {
        CORE_UNLOCK;
        return eIO_Success;
    }

#if defined(NCBI_OS_MSWIN)
    {{
        WSADATA wsadata;
        int x_errno = WSAStartup(MAKEWORD(1,1), &wsadata);
        if (x_errno != 0) {
            CORE_UNLOCK;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_InitializeAPI]  Failed WSAStartup()");
            return eIO_Unknown;
        }
    }}
#elif defined(NCBI_OS_UNIX)
    if ( !s_AllowSigPipe ) {
        struct sigaction sa;
        if (sigaction(SIGPIPE, 0, &sa) < 0  ||  sa.sa_handler == SIG_DFL) {
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_IGN;
            sigaction(SIGPIPE, &sa, 0);
        }
    }
#endif

    s_Initialized = 1/*true*/;
    CORE_UNLOCK;
    return eIO_Success;
}


extern EIO_Status SOCK_ShutdownAPI(void)
{
    CORE_LOCK_WRITE;
    if ( !s_Initialized ) {
        CORE_UNLOCK;
        return eIO_Success;
    }

    s_Initialized = 0/*false*/;
#if defined(NCBI_OS_MSWIN)
    {{
        int x_errno = WSACleanup() ? SOCK_ERRNO : 0;
        CORE_UNLOCK;
        if ( x_errno ) {
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_ShutdownAPI]  Failed WSACleanup()");
            return eIO_Unknown;
        }
    }}
#else
    CORE_UNLOCK;
#endif

    return eIO_Success;
}



/******************************************************************************
 *  LSOCK & SOCK AUXILIARES
 */


/* STimeout <--> struct timeval  conversions
 */
static STimeout *s_tv2to(const struct timeval* tv, STimeout* to)
{
    if ( !tv )
        return 0;

    to->sec  = (unsigned int) tv->tv_sec;
    to->usec = (unsigned int) tv->tv_usec;
    return to;
}

static struct timeval* s_to2tv(const STimeout* to, struct timeval* tv)
{
    if ( !to )
        return 0;

    tv->tv_sec  = to->sec;
    tv->tv_usec = to->usec % 1000000;
    return tv;
}


/* Switch the specified socket i/o between blocking and non-blocking mode
 */
static int/*bool*/ s_SetNonblock(TSOCK_Handle sock, int/*bool*/ nonblock)
{
#if defined(NCBI_OS_MSWIN)
    unsigned long argp = nonblock ? 1 : 0;
    return (ioctlsocket(sock, FIONBIO, &argp) == 0);
#elif defined(NCBI_OS_UNIX) || defined(NCBI_OS_MAC)
    return (fcntl(sock, F_SETFL,
                  nonblock ?
                  fcntl(sock, F_GETFL, 0) | O_NONBLOCK :
                  fcntl(sock, F_GETFL, 0) & (int) ~O_NONBLOCK) != -1);
#else
#   error "Unsupported platform"
#endif
}


/* Select on the socket i/o (multiple sockets).
 * "Event" field is not considered for entries, which "sock" field is 0,
 * "revent" for those entries is always "eIO_Open". For all other entries
 * only those sockets will be considered, which "revent" field does not
 * contain "eIO_Open" value. If at least one non-"eIO_Open" status found
 * in "revent", the call terminates with "eIO_Success" status (after,
 * however checking all other entries for validity). No additional checks
 * are made for the pre-ready entries. 
 * If "eIO_Write" event inquired on a socket, and the socket is marked for
 * upread, then returned "revent" may also include "eIO_Read" to indicate
 * that some input is available on that socket.
 * Return eIO_Success when at least one socket is found either ready
 * (including "eIO_Read" event on "eIO_Write" for upreadable sockets)
 * or failing ("revent" contains "eIO_Close").
 * Return "eIO_Timeout", if timeout expired before any socket became available.
 * Any other return code indicates failure.
 */
static EIO_Status s_Select(size_t                n,
                           SSOCK_Poll            polls[],
                           const struct timeval* timeout)
{
    int/*bool*/ write_only = 1;
    int/*bool*/ read_only = 1;
    int/*bool*/ ready = 0;
    int/*bool*/ bad = 0;
    fd_set      r_fds, w_fds, e_fds;
    int         n_fds;
    size_t      i;

    for (;;) { /* (optionally) auto-resume if interrupted by a signal */
        struct timeval tmo;

        n_fds = 0;
        FD_ZERO(&r_fds);
        FD_ZERO(&w_fds);
        FD_ZERO(&e_fds);
        for (i = 0; i < n; i++) {
            if ( !polls[i].sock ) {
                polls[i].revent = eIO_Open;
                continue;
            }
            if ( polls[i].revent ) {
                ready = 1;
                continue;
            }
            if (polls[i].event  &&
                (EIO_Event)(polls[i].event | eIO_ReadWrite) == eIO_ReadWrite) {
                if (polls[i].sock->sock != SOCK_INVALID) {
                    if (n != 1  &&  polls[i].sock->type == eSOCK_Datagram)
                        continue;
                    switch (polls[i].event) {
                    case eIO_ReadWrite:
                    case eIO_Write:
                        read_only = 0;
                        FD_SET(polls[i].sock->sock, &w_fds);
                        if (polls[i].event == eIO_Write
                            && (polls[i].sock->type == eSOCK_Datagram ||
                                polls[i].sock->r_status == eIO_Closed ||
                                polls[i].sock->is_eof                 ||
                                polls[i].sock->r_on_w == eOff         ||
                                (polls[i].sock->r_on_w == eDefault
                                 && s_ReadOnWrite != eOn)))
                            break;
                        /*FALLTHRU*/
                    case eIO_Read:
                        write_only = 0;
                        FD_SET(polls[i].sock->sock, &r_fds);
                        break;
                    default:
                        /* should never get here */
                        assert(0);
                        break;
                    }
                    FD_SET(polls[i].sock->sock, &e_fds);
                    if (n_fds < (int) polls[i].sock->sock)
                        n_fds = (int) polls[i].sock->sock;
                } else {
                    polls[i].revent = eIO_Close;
                    ready = 1;
                }
            } else {
                polls[i].revent = eIO_Close;
                bad = 1;
            }
        }

        if ( bad )
            return eIO_InvalidArg;

        if ( ready )
            return eIO_Success;

        if ( timeout )
            tmo = *timeout;

        n_fds = select(SOCK_NFDS((TSOCK_Handle) n_fds),
                       write_only ? 0 : &r_fds, read_only ? 0 : &w_fds,
                       &e_fds, timeout ? &tmo : 0);

        /* timeout has expired */
        if (n_fds == 0)
            return eIO_Timeout;

        if (n_fds > 0)
            break;

        /* n_fds < 0 */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK::s_Select]  Failed select()");
            return eIO_Unknown;
        }

        if ((n != 1  &&  s_InterruptOnSignal == eOn)  ||
            (n == 1  &&  (polls[0].sock->i_on_sig == eOn
                          ||  (polls[0].sock->i_on_sig == eDefault
                               &&  s_InterruptOnSignal == eOn)))) {
            return eIO_Interrupt;
        }
    }

    n_fds = 0;
    for (i = 0; i < n; i++) {
        if ( polls[i].sock ) {
            assert(polls[i].revent == eIO_Open);
            if (!FD_ISSET(polls[i].sock->sock, &e_fds)) {
                if (FD_ISSET(polls[i].sock->sock, &r_fds))
                    polls[i].revent = eIO_Read;
                if (FD_ISSET(polls[i].sock->sock, &w_fds))
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
            } else
                polls[i].revent = eIO_Close;
            if (polls[i].revent != eIO_Open)
                n_fds++;
        }
    }
    assert(n_fds);

    /* success; can do i/o now */
    return eIO_Success;
}



/******************************************************************************
 *  LISTENING SOCKET
 */


extern EIO_Status LSOCK_Create(unsigned short port,
                               unsigned short backlog,
                               LSOCK*         lsock)
{
    TSOCK_Handle       x_lsock;
    struct sockaddr_in addr;

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* Create new(listening) socket */
    if ((x_lsock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[LSOCK::Create]  Cannot create socket");
        return eIO_Unknown;
    }

    /* Let more than one "bind()" use the same address.
     *
     * It was confirmed(?) that at least under Solaris 2.5 this precaution:
     * 1) makes the address to be released immediately after the process
     *    termination;
     * 2) still issue EADDINUSE error on the attempt to bind() to the
     *    same address being in-use by a living process(if SOCK_STREAM).
     */
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    {{
#  if defined(NCBI_OS_MSWIN)
        BOOL reuse_addr = TRUE;
#  else
        int  reuse_addr = 1;
#  endif
        if (setsockopt(x_lsock, SOL_SOCKET, SO_REUSEADDR, 
                       (const char*) &reuse_addr, sizeof(reuse_addr)) != 0) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[LSOCK::Create]  Failed setsockopt(REUSEADDR)");
            SOCK_CLOSE(x_lsock);
            return eIO_Unknown;
        }
    }}
#endif

    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
#if defined(HAVE_SIN_LEN)
    addr.sin_len         = sizeof(addr);
#endif
    if (bind(x_lsock, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[LSOCK::Create]  Failed bind()");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* Listen */
    if (listen(x_lsock, backlog) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[LSOCK::Create]  Failed listen()");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* Set to non-blocking mode */
    if ( !s_SetNonblock(x_lsock, 1/*true*/) ) {
        CORE_LOG(eLOG_Error,
                 "[LSOCK::Create]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    if (!(*lsock = (LSOCK) calloc(1, sizeof(LSOCK_struct)))) {
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }
    /* Success... */
    (*lsock)->sock = x_lsock;
    return eIO_Success;
}


extern EIO_Status LSOCK_Accept(LSOCK           lsock,
                               const STimeout* timeout,
                               SOCK*           sock)
{
    TSOCK_Handle   x_sock;
    unsigned int   x_host;
    unsigned short x_port;

    if (lsock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error, "[LSOCK::Accept]  Invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    {{ /* wait for the connection request to come (up to timeout) */
        EIO_Status status;
        struct timeval tv;
        SSOCK_Poll poll[2];
        poll[0].sock   = (SOCK)lsock;/*HACK:we'd actually use 1st field only */
        poll[0].event  = eIO_Read;   /* ... due to READ-only operation       */
        poll[0].revent = eIO_Open;   /* (must have this cleared for check)   */
        poll[1].sock   = 0;          /*HACK:[1] to avoid use of i_on_s in [0]*/
        if ((status = s_Select(2, poll, s_to2tv(timeout, &tv))) != eIO_Success)
            return status;
        if (poll[0].revent == eIO_Close)
            return eIO_Unknown;
        assert(poll[0].event == eIO_Read  &&  poll[0].revent == eIO_Read);
    }}

    {{ /* accept next connection */
#if defined(HAVE_SOCKLEN_T)
        typedef socklen_t  SOCK_socklen_t;
#else
        typedef int        SOCK_socklen_t;
#endif
        struct sockaddr_in addr;
        SOCK_socklen_t     addrlen = (SOCK_socklen_t) sizeof(addr);
        if ((x_sock = accept(lsock->sock, (struct sockaddr*) &addr, &addrlen))
            == SOCK_INVALID) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[LSOCK::Accept]  Failed accept()");
            return eIO_Unknown;
        }
        /* man accept(2) notes that non-blocking state is not inherited */
        if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
            CORE_LOG(eLOG_Error, "[LSOCK::Accept]  "
                     "Cannot set accepted socket to non-blocking mode");
            SOCK_CLOSE(x_sock);
            return eIO_Unknown;
        }
        x_host = addr.sin_addr.s_addr;
        x_port = addr.sin_port;
    }}

    /* create new SOCK structure */
    if (!(*sock = (SOCK) calloc(1, sizeof(SOCK_struct)))) {
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }
    /* success */
    (*sock)->sock     = x_sock;
    /* all timeouts zeroed - infinite */
    (*sock)->host     = x_host;
    (*sock)->port     = x_port;
    (*sock)->r_on_w   = eDefault;
    (*sock)->i_on_sig = eDefault;
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    /* w_buf is unused for stream sockets */
    (*sock)->is_eof   = 0/*false*/;
    (*sock)->r_status = eIO_Success;
    (*sock)->w_status = eIO_Success;
    (*sock)->id       = ++s_ID_Counter * 1000;
    (*sock)->log_data = eDefault;
    (*sock)->type     = eSOCK_ServerSide;
    return eIO_Success;
}


extern EIO_Status LSOCK_Close(LSOCK lsock)
{
    /* Set the socket back to blocking mode */
    if ( !s_SetNonblock(lsock->sock, 0/*false*/) ) {
        CORE_LOG(eLOG_Error,
                 "[LSOCK::Close]  Cannot set socket back to blocking mode");
    }

    for (;;) { /* close persistently - retry if interrupted by a signal */
        /* success */
        if (SOCK_CLOSE(lsock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[LSOCK::Close]  Failed close()");
            return eIO_Unknown;
        }
    }

    /* success:  cleanup & return */
    lsock->sock = SOCK_INVALID;
    free(lsock);
    return eIO_Success;
}


extern EIO_Status LSOCK_GetOSHandle(LSOCK  lsock,
                                    void*  handle_buf,
                                    size_t handle_size)
{
    if (handle_size != sizeof(lsock->sock)) {
        CORE_LOG(eLOG_Error, "[LSOCK::GetOSHandle]  Invalid handle size");
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle_buf, &lsock->sock, handle_size);
    return eIO_Success;
}



/******************************************************************************
 *  SOCKET
 */


/* Connect the (pre-allocated) socket to the specified "host:port" peer.
 * HINT: if "host" is NULL then assume(!) that the "sock" already exists,
 *       and connect to the same host;  the same is for zero "port".
 * NOTE: Stream sockets only.
 */
static EIO_Status s_Connect(SOCK            sock,
                            const char*     host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    TSOCK_Handle       x_sock;
    unsigned int       x_host;
    unsigned short     x_port;
    struct sockaddr_in server;

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* Get address of the remote host (assume the same host if it is NULL) */
    x_host = host  &&  *host ? SOCK_gethostbyname(host) : sock->host;
    if ( !x_host ) {
        CORE_LOGF(eLOG_Error, ("[SOCK::s_Connect]  Failed "
                               "SOCK_gethostbyname(%.64s)", host));
        return eIO_Unknown;
    }

    /* Set the port to connect to(assume the same port if "port" is zero) */
    x_port = (unsigned short) (port ? htons(port) : sock->port);

    /* Create new socket */
    if ((x_sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[SOCK::s_Connect]  Cannot create socket");
        return eIO_Unknown;
    }

    /* Set the socket i/o to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        CORE_LOG(eLOG_Error,
                 "[SOCK::s_Connect]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* Fill in the "server" struct */
    memset(&server, 0, sizeof(server));
    memcpy(&server.sin_addr, &x_host, sizeof(x_host));
    server.sin_family = AF_INET;
    server.sin_port   = x_port;
#if defined(HAVE_SIN_LEN)
    server.sin_len    = sizeof(server);
#endif

    if (sock->log_data == eOn  ||
        (sock->log_data == eDefault  &&  s_LogData == eOn)) {
        s_DoLogData(sock, eIO_Open, &server, x_sock);
    }

    /* Establish connection to the peer */
    if (connect(x_sock, (struct sockaddr*) &server, sizeof(server)) != 0) {
        if (SOCK_ERRNO != SOCK_EINTR  &&  SOCK_ERRNO != SOCK_EINPROGRESS  &&
            SOCK_ERRNO != SOCK_EWOULDBLOCK) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("[SOCK::s_Connect]  Failed connect() to "
                                "%.64s:%hu", host? host: "???",ntohs(x_port)));
            SOCK_CLOSE(x_sock);
            return eIO_Unknown; /* unrecoverable error */
        }

        /* The connect could be interrupted by a signal or just cannot be
         * established immediately;  yet, the connect must have been in
         * progress (asynchronous), so wait here for it to succeed
         * (become writable).
         */
        {{
            int               x_errno = 0;
            EIO_Status        status;
            SSOCK_Poll        poll;
            struct timeval    tv;
            SOCK_struct       s;
#if defined(NCBI_OS_UNIX)
#  if defined(HAVE_SOCKLEN_T)
            typedef socklen_t SOCK_socklen_t;
#  else
            typedef int       SOCK_socklen_t;
#  endif /*HAVE_SOCKLEN_T*/
            SOCK_socklen_t    x_len = (SOCK_socklen_t) sizeof(x_errno);
#endif /*NCBI_OS_UNIX*/

            memset(&s, 0, sizeof(s)); /* make it temporary and fill partially*/
            s.type      = eSOCK_ClientSide;
            s.sock      = x_sock;
            s.r_on_w    = eOff;       /* to prevent upread inquiry           */
            s.i_on_sig  = eOff;       /* uninterruptible                     */
            poll.sock   = &s;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            status = s_Select(1, &poll, s_to2tv(timeout, &tv));
#ifdef NCBI_OS_UNIX
            if (status == eIO_Success  &&
                (getsockopt(x_sock, SOL_SOCKET, SO_ERROR, &x_errno,&x_len) != 0
                 ||  x_errno != 0))
                status = eIO_Unknown;
#endif /*NCBI_OS_UNIX*/
            if (status != eIO_Success  ||  poll.revent != eIO_Write) {
                if (status != eIO_Interrupt  ||  poll.revent != eIO_Write)
                    status = eIO_Unknown;
                if ( !x_errno )
                    x_errno = SOCK_ERRNO;
                CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                                   ("[SOCK::s_Connect]  Failed pending connect"
                                    " to %.64s:%hu (%.32s)", host? host: "???",
                                    ntohs(x_port), IO_StatusStr(status)));
                SOCK_CLOSE(x_sock);
                return status;
            }
        }}
    }

    /* Success */
    /* NOTE:  it does not change the timeouts and the data buffer content */
    sock->sock     = x_sock;
    sock->host     = x_host;
    sock->port     = x_port;
    sock->is_eof   = 0/*false*/;
    sock->r_status = eIO_Success;
    sock->w_status = eIO_Success;

    return eIO_Success;
}


static EIO_Status s_WipeRBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->r_buf);

    if (size  &&  BUF_Read(sock->r_buf, 0, size) != size) {
        CORE_LOG(eLOG_Error, "[SOCK::s_WipeRBuf]  Cannot drop aux. data buf");
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    return status;
}


static EIO_Status s_WipeWBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->w_buf);

    if (size  &&  BUF_Read(sock->w_buf, 0, size) != size) {
        CORE_LOG(eLOG_Error, "[SOCK::s_WipeWBuf]  Cannot drop aux. data buf");
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    if (sock->type == eSOCK_Datagram)
        sock->is_eof = 0;
    return status;
}


/* Shutdown and close the socket
 */
static EIO_Status s_Close(SOCK sock)
{
    /* Reset the auxiliary data buffers */
    s_WipeRBuf(sock);
    s_WipeWBuf(sock);

    if (sock->type != eSOCK_Datagram) {
        /* Set the socket back to blocking mode */
        if ( !s_SetNonblock(sock->sock, 0/*false*/) ) {
            CORE_LOG(eLOG_Error, "[SOCK::s_Close] "
                     " Cannot set socket back to blocking mode");
        }

        /* Set the close()'s linger period be equal to the close timeout */
#if (defined(NCBI_OS_UNIX) && !defined(NCBI_OS_BEOS)) || defined(NCBI_OS_MSWIN)
        /* setsockopt() is not implemented for MAC (MIT socket emulation lib)*/
        if (sock->c_timeout  &&  sock->w_status != eIO_Closed) {
            struct linger lgr;
            lgr.l_onoff  = 1;
            lgr.l_linger = sock->c_timeout->tv_sec? sock->c_timeout->tv_sec: 1;
            if (setsockopt(sock->sock, SOL_SOCKET, SO_LINGER,
                           (char*) &lgr, sizeof(lgr)) != 0) {
                int x_errno = SOCK_ERRNO;
                CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno,SOCK_STRERROR(x_errno),
                                  "[SOCK::s_Close] "
                                  " Failed setsockopt(SO_LINGER)");
            }
        }
#endif

        /* Shutdown in both directions */
        if (SOCK_Shutdown(sock, eIO_Write) != eIO_Success) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK::s_Close]  Cannot shutdown socket for writing");
        }
        if (SOCK_Shutdown(sock, eIO_Read) != eIO_Success) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK::s_Close]  Cannot shutdown socket for reading");
        }
    }

    if (sock->log_data == eOn  ||
        (sock->log_data == eDefault  &&  s_LogData == eOn)) {
        s_DoLogData(sock, eIO_Close, 0, 0);
    }

    for (;;) { /* close persistently - retry if interrupted by a signal */
        if (SOCK_CLOSE(sock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK::s_Close]  Failed close()");
            sock->sock = SOCK_INVALID;
            return (SOCK_ERRNO == SOCK_ECONNRESET || SOCK_ERRNO == SOCK_EPIPE)
                ? eIO_Closed : eIO_Unknown;
        }
    }
    /* success */
    sock->sock = SOCK_INVALID;
    return eIO_Success;
}


/* To allow emulating "peek" using the NCBI data buffering.
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves.)
 * Please note the following status combinations and their meanings:
 * -------------------------------+------------------------------------------
 *              Field             |
 * ---------------+---------------+                  Meaning
 * sock->r_status | sock->is_eof  |           (stream sockets only)
 * ---------------+---------------+------------------------------------------
 * eIO_Closed     |       0       |  Socket shut down
 * eIO_Closed     |       1       |  Read severely failed
 * not eIO_Closed |       0       |  Read completed with r_status error
 * not eIO_Closed |       1       |  Read hit EOF and completed with r_status
 * ---------------+---------------+------------------------------------------
 * NOTE: This call is for stream sockets only.
 */
static int s_Recv(SOCK        sock,
                  void*       buffer,
                  size_t      size,
                  int/*bool*/ peek)
{
    char*  x_buffer = (char*) buffer;
    char   xx_buffer[4096];
    size_t n_read;

    if ( !size ) {
        /* internal upread use only */
        assert( !buffer );
        n_read = 0;
    } else {
        /* read (or peek) from the internal buffer */
        n_read = peek ?
            BUF_Peek(sock->r_buf, x_buffer, size) :
            BUF_Read(sock->r_buf, x_buffer, size);
        if ((n_read  &&  (n_read == size ||  !peek))  ||
            sock->r_status == eIO_Closed  ||  sock->is_eof) {
            return (int) n_read;
        }
    }

    /* read (not just peek) from the socket */
    do {
        size_t n_todo;
        int    x_read;

        if ( !size ) {
            /* internal upread call -- read out as much as possible */
            n_todo    = sizeof(xx_buffer);
            x_buffer  = xx_buffer;
        } else if ( !buffer ) {
            /* read to the temporary buffer (to store or discard later) */
            n_todo    = size - n_read;
            if (n_todo > sizeof(xx_buffer))
                n_todo = sizeof(xx_buffer);
            x_buffer  = xx_buffer;
        } else {
            /* read to the data buffer provided by user */
            n_todo    = size - n_read;
            x_buffer += n_read;
        }
        /* recv */
        x_read = recv(sock->sock, x_buffer, n_todo, 0);

        /* catch EOF */
        if (x_read == 0  ||
            (x_read < 0  &&  SOCK_ERRNO == SOCK_ENOTCONN)) {
            sock->r_status = x_read == 0 ? eIO_Success : eIO_Closed;
            sock->is_eof   = 1/*true*/;
            break;
        }

        /* some error */
        if (x_read < 0) {
            int x_errno = SOCK_ERRNO;
            if (x_errno != SOCK_EWOULDBLOCK  &&
                x_errno != SOCK_EAGAIN       &&
                x_errno != SOCK_EINTR) {
                /* catch unknown ERROR */
                sock->r_status = eIO_Unknown;
                CORE_LOG_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                                  "[SOCK::s_Recv]  Failed recv()");
            }
            return n_read ? (int) n_read : -1;
        }

        /* successful read */
        assert(x_read > 0);
        sock->r_status = eIO_Success;

        /* if "peek" -- store the new read data in the internal input buffer */
        if ( peek ) {
            verify(BUF_Write(&sock->r_buf, x_buffer, (size_t) x_read));
        }

        /* statistics & logging */
        if (sock->log_data == eOn  ||
            (sock->log_data == eDefault  &&  s_LogData == eOn)) {
            s_DoLogData(sock, eIO_Read, x_buffer, (size_t) x_read);
        }
        sock->n_read += x_read;
        n_read       += x_read;
    } while (!size  ||  (!buffer  &&  n_read < size));

    return (int) n_read;
}


/* Read/Peek data from the socket
 */
static EIO_Status s_Read(SOCK        sock,
                         void*       buf,
                         size_t      size,
                         size_t*     n_read,
                         int/*bool*/ peek)
{
    *n_read = 0;

    if (size == 0)
        return SOCK_Status(sock, eIO_Read);

    if (sock->type == eSOCK_Datagram) {
        *n_read = peek ?
            BUF_Peek(sock->r_buf, buf, size) :
            BUF_Read(sock->r_buf, buf, size);
        sock->r_status = *n_read ? eIO_Success : eIO_Closed;
        return sock->r_status;
    }

    for (;;) { /* retry if either blocked or interrupted (optional) */
        /* try to read */
        int x_read = s_Recv(sock, buf, size, peek);
        int x_errno;

        if (x_read > 0) {
            assert((size_t) x_read <= size);
            *n_read = x_read;
            return eIO_Success;  /* success */
        }

        if (sock->r_status == eIO_Unknown)
            break;

        if (sock->r_status == eIO_Closed  ||  sock->is_eof)
            return eIO_Closed;   /* shut down or hit EOF */

        x_errno = SOCK_ERRNO;
        /* blocked -- wait for a data to come;  exit if timeout/error */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            EIO_Status status;
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            if ((status = s_Select(1, &poll, sock->r_timeout)) != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                break;
            assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
            continue;
        }

        if (x_errno != SOCK_EINTR)
            break;

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            return eIO_Interrupt;
        }
    }

    /* dont want to handle all possible errors... let them be "unknown" */
    return eIO_Unknown;
}


/* s_Select() with stall protection: try pull incoming data from sockets.
 * If only eIO_Read events requested in "polls" array or
 * read-on-write disabled for all sockets in question, or if
 * EOF was reached in all read streams, this function is almost
 * equivalent to s_Select().
 */
static EIO_Status s_SelectStallsafe(size_t                n,
                                    SSOCK_Poll            polls[],
                                    const struct timeval* timeout,
                                    size_t*               n_ready)
{
    int/*bool*/ r_pending;
    EIO_Status  status;
    size_t      i, j;

    if ((status = s_Select(n, polls, timeout)) != eIO_Success) {
        if ( n_ready )
            *n_ready = 0;
        return status;
    }

    j = 0;
    r_pending = 0;
    for (i = 0; i < n; i++) {
        if (polls[i].revent == eIO_Close)
            break;
        if (polls[i].revent & polls[i].event)
            break;
        if (polls[i].revent != eIO_Open  &&  !r_pending) {
            r_pending++;
            j = i;
        }
    }
    if (i >= n  &&  r_pending) {
        /* all sockets are not ready for the requested events */
        for (i = j; i < n; i++) {
            /* try to find an immediately readable socket */
            if (polls[i].event == eIO_Write  &&  polls[i].revent == eIO_Read) {
                ESwitch    save_r_on_w;
                SSOCK_Poll poll;

                assert(polls[i].sock &&
                       polls[i].sock->type != eSOCK_Datagram &&
                       (polls[i].sock->r_on_w == eOn ||
                        (polls[i].sock->r_on_w == eDefault
                         && s_ReadOnWrite == eOn)));
                /* try upread as mush as possible data into internal buffer */
                s_Recv(polls[i].sock, 0, 0/*infinite*/, 1/*peek*/);
                /* then poll about write-only w/o upread */
                save_r_on_w = polls[i].sock->r_on_w;
                polls[i].sock->r_on_w = eOff;
                poll.sock   = polls[i].sock;
                poll.event  = eIO_Write;
                poll.revent = eIO_Open;
                status = s_Select(1, &poll, timeout);
                polls[i].sock->r_on_w = save_r_on_w;

                if (status == eIO_Success  &&  poll.revent == eIO_Write) {
                    polls[i].revent = eIO_Write/*poll.revent*/;
                    break; /*can write now!*/
                }
            }
        }
    }

    j = 0;
    for (i = 0; i < n; i++) {
        if (polls[i].revent != eIO_Close) {
            polls[i].revent = (EIO_Event)(polls[i].revent & polls[i].event);
            if (polls[i].revent != eIO_Open)
                j++;
        } else
            j++;
    }

    if ( n_ready )
        *n_ready = j;

    return j ? eIO_Success : eIO_Timeout;
}


/* Write data to the socket "as is" (as many bytes at once as possible)
 */
static EIO_Status s_Send(SOCK        sock,
                         const void* buf,
                         size_t      size,
                         size_t*     n_written)
{
    *n_written = 0;

    if (sock->type == eSOCK_Datagram) {
        if ( sock->is_eof )
            s_WipeWBuf(sock);
        if ( BUF_Write(&sock->w_buf, buf, size) ) {
            sock->w_status = eIO_Success;
            *n_written = size;
        } else
            sock->w_status = eIO_Unknown;
        return sock->w_status;
    }

    if (sock->w_status == eIO_Closed) {
        CORE_LOG(eLOG_Warning,
                 "[SOCK::Write]  Socket has been shut down for writing");
        return eIO_Closed;
    }
    if (size == 0)
        return eIO_Success;

    for (;;) { /* retry if interrupted by a signal */
        /* try to write */
        int x_written = send(sock->sock, (void*) buf, size, 0);
        int x_errno;

        if (x_written >= 0) {
            /* statistics & logging */
            if (sock->log_data == eOn  ||
                (sock->log_data == eDefault  &&  s_LogData == eOn)) {
                s_DoLogData(sock, eIO_Write, buf, (size_t) x_written);
            }
            sock->n_written += x_written;

            *n_written = x_written;
            sock->w_status = eIO_Success;
            break/*done*/;
        }
        x_errno = SOCK_ERRNO;
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = eIO_Unknown;

        /* blocked -- retry if unblocked before the timeout is expired */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            EIO_Status status;
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pull incoming data from the socket */
            status = s_SelectStallsafe(1, &poll, sock->w_timeout, 0);
            if (status != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                return eIO_Unknown;
            assert(poll.revent == eIO_Write);
            continue;
        }

        if (x_errno != SOCK_EINTR) {
            /* forcibly closed by peer or shut down? */
            if (x_errno != SOCK_ECONNRESET  &&  x_errno != SOCK_EPIPE) {
                CORE_LOG_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                                  "[SOCK::s_Send]  Failed send()");
            } else {
                sock->w_status = eIO_Closed;
            }
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = eIO_Interrupt;
            break;
        }
    }

    return sock->w_status;
}


static EIO_Status s_Write(SOCK        sock,
                          const void* buf,
                          size_t      size,
                          size_t*     n_written)
{
#if defined(SOCK_WRITE_SLICE)
    /* Split output buffer by slices (of size <= SOCK_WRITE_SLICE)
     * before writing to the socket
     */
    EIO_Status status;

    *n_written = 0;

    do {
        size_t n_io = size > SOCK_WRITE_SLICE ? SOCK_WRITE_SLICE : size;
        size_t n_io_done;
        status = s_Send(sock, (char*) buf + x_written, n_io, &n_io_done);
        *n_written += n_io_done;
        if (n_io != n_io_done  ||  !n_io)
            break;
        size       -= n_io_done;
    } while (status == eIO_Success);

    return status;
#else
    return s_Send(sock, buf, size, n_written);
#endif /*SOCK_WRITE_SLICE*/
}


extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port, 
                              const STimeout* timeout,
                              SOCK*           sock)
{
    return SOCK_CreateEx(host, port, timeout, sock, eDefault);
}


extern EIO_Status SOCK_CreateEx(const char*     host,
                                unsigned short  port,
                                const STimeout* timeout,
                                SOCK*           sock,
                                ESwitch         log_data)
{
    /* Allocate memory for the internal socket structure */
    SOCK x_sock;
    if (!(x_sock = (SOCK_struct*) calloc(1, sizeof(SOCK_struct))))
        return eIO_Unknown;
    x_sock->sock     = SOCK_INVALID;
    x_sock->id       = ++s_ID_Counter * 1000;
    x_sock->log_data = log_data;
    x_sock->type     = eSOCK_ClientSide;

    /* Connect */
    {{
        EIO_Status status;
        if ((status = s_Connect(x_sock, host, port, timeout)) != eIO_Success) {
            free(x_sock);
            return status;
        }
    }}

    /* Setup the input data buffer properties */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);

    /* Success */
    x_sock->r_on_w   = eDefault;
    x_sock->i_on_sig = eDefault;
    *sock = x_sock;
    return eIO_Success;
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    /* Datagram sockets: we do not actually call system's connect() for them */
    if (sock->type == eSOCK_Datagram) {
        assert(sock->sock != SOCK_INVALID);
        sock->id++;
        sock->port = port;
        if (host  &&  *host) {
            if ((sock->host = SOCK_gethostbyname(host)) == 0) {
                CORE_LOGF(eLOG_Warning, ("[SOCK::Reconnect]  Failed "
                                         "SOCK_gethostbyname(%.64s)", host));
                return eIO_Unknown;
            }
        } else
            sock->host = 0;
        return eIO_Success;
    }

    /* Close the socket if necessary */
    if (sock->sock != SOCK_INVALID) {
        s_Close(sock);
    }

    /* Special treatment for server-side socket */
    if (sock->type == eSOCK_ServerSide) {
        if (!host  ||  !*host  ||  !port) {
            CORE_LOG(eLOG_Error, "[SOCK::Reconnect]  Attempt to reconnect "
                     "server-side socket as client one to its peer address");
            return eIO_InvalidArg;
        }
        sock->type = eSOCK_ClientSide;
    }

    /* Connect */
    sock->id++;
    return s_Connect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Shutdown(SOCK      sock,
                                EIO_Event how)
{
    int x_how;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error, "[SOCK::Shutdown]  Invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    if (sock->type == eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[SOCK::Shutdown]  Called for datagram socket");
        return eIO_InvalidArg;
    }

    switch ( how ) {
    case eIO_Read:
        if ( sock->is_eof ) {
            /* Hit EOF (and may be not yet shut down). So, flag it as been
             * shut down, but do not call the actual system shutdown(),
             * as it can cause smart OS'es like Linux to complain. */
            sock->is_eof = 0/*false*/;
            sock->r_status = eIO_Closed;
        }
        if (sock->r_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        x_how = SOCK_SHUTDOWN_RD;
        sock->r_status = eIO_Closed;
        break;
    case eIO_Write:
        if (sock->w_status == eIO_Closed) {
            return eIO_Success;  /* has been shut down already */
        }
        x_how = SOCK_SHUTDOWN_WR;
        sock->w_status = eIO_Closed;
        break;
    case eIO_ReadWrite:
        verify(SOCK_Shutdown(sock, eIO_Write) == eIO_Success);
        verify(SOCK_Shutdown(sock, eIO_Read ) == eIO_Success);
        return eIO_Success;
    default:
        CORE_LOG(eLOG_Warning, "[SOCK::Shutdown]  Invalid direction");
        return eIO_InvalidArg;
    }

    if (SOCK_SHUTDOWN(sock->sock, x_how) != 0) {
        CORE_LOGF(eLOG_Warning, ("[SOCK::Shutdown]  shutdown(%s) failed",
                                 how == eIO_Read ? "read" : "write"));
    }
    return eIO_Success;
}


extern EIO_Status SOCK_Close(SOCK sock)
{
    EIO_Status status;

    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error, "[SOCK::Close]  Invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    status = s_Close(sock);
    assert(sock->sock == SOCK_INVALID);
    BUF_Destroy(sock->r_buf);
    BUF_Destroy(sock->w_buf);
    free(sock);
    return status;
}


extern EIO_Status SOCK_Wait(SOCK            sock,
                            EIO_Event       event,
                            const STimeout* timeout)
{
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error, "[SOCK::Wait]  Invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    /* Check against already shutdown socket */
    switch ( event ) {
    case eIO_Read:
        if (BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->type == eSOCK_Datagram)
            return eIO_Closed;
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Warning,
                      ("[SOCK::Wait(R)]  Attempt to wait on %s socket",
                       sock->is_eof ? "closed" : "shutdown"));
            return eIO_Closed;
        }
        if (sock->is_eof) {
            return sock->r_status;
        }
        break;
    case eIO_Write:
        if (sock->type == eSOCK_Datagram)
            return eIO_Success;
        if (sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK::Wait(W)]  Attempt to wait on shutdown socket");
            return eIO_Closed;
        }
        break;
    case eIO_ReadWrite:
        if (sock->type == eSOCK_Datagram  ||  BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->r_status == eIO_Closed  &&  sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK::Wait(RW)]  Attempt to wait on shutdown socket");
            return eIO_Closed;
        }
        if (sock->is_eof) {
            return sock->r_status;
        }
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Note,
                      ("[SOCK::Wait(RW)]  Attempt to wait on %s socket",
                       sock->is_eof ? "closed" : "R-shutdown"));
            event = eIO_Write;
            break;
        }
        if (sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Note,
                     "[SOCK::Wait(RW)]  Attempt to wait on W-shutdown socket");
            event = eIO_Read;
            break;
        }
        break;
    default:
        CORE_LOG(eLOG_Error,
                 "[SOCK::Wait]  Invalid (not eIO_[Read][Write]) event type");
        return eIO_InvalidArg;
    }

    assert(sock->type != eSOCK_Datagram);
    /* Do wait */
    {{
        EIO_Status status;
        struct timeval tv;
        SSOCK_Poll poll;
        poll.sock   = sock;
        poll.event  = event;
        poll.revent = eIO_Open;
        status = s_SelectStallsafe(1, &poll, s_to2tv(timeout, &tv), 0);
        if (status != eIO_Success)
            return status;
        if (poll.revent == eIO_Close)
            return eIO_Unknown;
        assert(poll.event == poll.revent);
        return status/*success*/;
    }}
}


extern EIO_Status SOCK_Poll(size_t          n,
                            SSOCK_Poll      polls[],
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    SSOCK_Poll    xx_polls[2];
    SSOCK_Poll*    x_polls;
    EIO_Status     status;
    size_t         x_n;
    struct timeval tv;

    if ((n == 0) != (polls == 0)) {
        if ( n_ready )
            *n_ready = 0;
        return eIO_InvalidArg;
    }

    for (x_n = 0; x_n < n; x_n++) {
        if (polls[x_n].sock                        &&
            polls[x_n].sock->sock != SOCK_INVALID  &&
            (polls[x_n].event == eIO_Read  ||
             polls[x_n].event == eIO_ReadWrite)    &&
            BUF_Size(polls[x_n].sock->r_buf) != 0) {
            polls[x_n].revent = eIO_Read;
            if (polls[x_n].event == eIO_ReadWrite  &&
                polls[x_n].sock->type == eSOCK_Datagram) {
                polls[x_n].revent = eIO_ReadWrite;
            }
        } else {
            polls[x_n].revent = eIO_Open;
        }
    }

    if (n == 1) {
        xx_polls[0] = polls[0];
        xx_polls[1].sock = 0;
        x_polls = xx_polls;
        x_n = 2;
    } else {
        x_polls = polls;
        x_n = n;
    }

    status = s_SelectStallsafe(x_n, x_polls, s_to2tv(timeout, &tv), n_ready);

    if (n == 1)
        polls[0].revent = xx_polls[0].revent;

    return status;
}


extern EIO_Status SOCK_SetTimeout(SOCK            sock,
                                  EIO_Event       event,
                                  const STimeout* timeout)
{
    switch ( event ) {
    case eIO_Read:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        break;
    case eIO_Write:
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    case eIO_ReadWrite:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    case eIO_Close:
        sock->c_timeout = s_to2tv(timeout, &sock->c_tv);
        break;
    default:
        CORE_LOG(eLOG_Error, "[SOCK::SetTimeout]  Invalid event");
        assert(0);
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


extern const STimeout* SOCK_GetTimeout(SOCK      sock,
                                       EIO_Event event)
{
    switch ( event ) {
    case eIO_Read:
        return s_tv2to(sock->r_timeout, &sock->r_to);
    case eIO_Write:
        return s_tv2to(sock->w_timeout, &sock->w_to);
    case eIO_Close:
        return s_tv2to(sock->c_timeout, &sock->c_to);
    default:
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

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_ReadPlain:
            status = s_Read(sock, buf, size, &x_read, 0/*false, read*/);
            break;

        case eIO_ReadPeek:
            status = s_Read(sock, buf, size, &x_read, 1/*true, peek*/);
            break;

        case eIO_ReadPersist:
            x_read = 0;
            do {
                size_t xx_read;
                status = SOCK_Read(sock, (char*) buf + (buf? x_read : 0), size,
                                   &xx_read, eIO_ReadPlain);
                x_read += xx_read;
                if (status != eIO_Success)
                    break;
                size   -= xx_read;
            } while ( size );
            break;

        default:
            CORE_LOG(eLOG_Error, "[SOCK::Read]  Invalid read method");
            assert(0);
            x_read = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOG(eLOG_Error, "[SOCK::Read]  Invalid socket");
        assert(0);
        x_read = 0;
        status = eIO_Unknown;
    }

    if ( n_read )
        *n_read = x_read;
    return status;
}


extern EIO_Status SOCK_PushBack(SOCK        sock,
                                const void* buf,
                                size_t      size)
{
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error, "[SOCK::PushBack]  Invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    return BUF_PushBack(&sock->r_buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_Status(SOCK      sock,
                              EIO_Event direction)
{
    switch ( direction ) {
    case eIO_Read:
        if (sock->type != eSOCK_Datagram) {
            return sock->r_status != eIO_Success
                ? sock->r_status : (sock->is_eof ? eIO_Closed : eIO_Success);
        }
        return sock->r_status;
    case eIO_Write:
        return sock->w_status;
    default:
        return eIO_InvalidArg;
    }
}


extern EIO_Status SOCK_Write(SOCK            sock,
                             const void*     buf,
                             size_t          size,
                             size_t*         n_written,
                             EIO_WriteMethod how)
{
    EIO_Status status;
    size_t     x_written;

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_WritePlain:
            status = s_Write(sock, buf, size, &x_written);
            break;

        case eIO_WritePersist:
            x_written = 0;
            do {
                size_t xx_written;
                status = SOCK_Write(sock, (char*) buf + x_written, size,
                                    &xx_written, eIO_WritePlain);
                x_written += xx_written;
                if (status != eIO_Success)
                    break;
                size      -= xx_written;
            } while ( size );
            break;

        default:
            CORE_LOG(eLOG_Error, "[SOCK::Write]  Invalid write method");
            assert(0);
            x_written = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOG(eLOG_Error, "[SOCK::Write]  Invalid socket");
        assert(0);
        x_written = 0;
        status = eIO_Unknown;
    }

    if ( n_written )
        *n_written = x_written;
    return status;
}


extern void SOCK_GetPeerAddress(SOCK            sock,
                                unsigned int*   host,
                                unsigned short* port,
                                ENH_ByteOrder   byte_order)
{
    if ( host ) {
        *host = (unsigned int)
            (byte_order != eNH_HostByteOrder ? sock->host : ntohl(sock->host));
    }
    
    if ( port ) {
        *port = (unsigned short)
            (byte_order != eNH_HostByteOrder ? sock->port : ntohs(sock->port));
    }
}


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle_buf,
                                   size_t handle_size)
{
    if (handle_size != sizeof(sock->sock)) {
        CORE_LOG(eLOG_Error, "[SOCK::GetOSHandle]  Invalid handle size");
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle_buf, &sock->sock, handle_size);
    return eIO_Success;
}


extern void SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    if (on_off == eDefault)
        on_off = eOff;
    s_ReadOnWrite = on_off;
}


extern void SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off)
{
    if (sock->type != eSOCK_Datagram)
        sock->r_on_w = on_off;
}


extern void SOCK_SetInterruptOnSignalAPI(ESwitch on_off)
{
    if (on_off == eDefault)
        on_off = eOff;
    s_InterruptOnSignal = on_off;
}


extern void SOCK_SetInterruptOnSignal(SOCK sock, ESwitch on_off)
{
    sock->i_on_sig = on_off;
}


extern EIO_Status DSOCK_Create(unsigned short port, SOCK* sock)
{
    return DSOCK_CreateEx(port, sock, eDefault);
}


extern EIO_Status DSOCK_CreateEx(unsigned short port, SOCK* sock, ESwitch log)
{
    TSOCK_Handle x_sock;

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* Create new datagram socket */
    if ((x_sock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[DSOCK::Create]  Cannot create datagram socket");
        return eIO_Unknown;
    }

    if ( port ) {
        struct sockaddr_in addr;
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
#  if defined(NCBI_OS_MSWIN)
        BOOL reuse_addr = TRUE;
#  else
        int  reuse_addr = 1;
#  endif
        if (setsockopt(x_sock, SOL_SOCKET, SO_REUSEADDR,
                       (const char*) &reuse_addr, sizeof(reuse_addr)) != 0) {
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[DSOCK::Create]  Failed setsockopt(REUSEADDR)");
            SOCK_CLOSE(x_sock);
            return eIO_Unknown;
        }
#endif

        /* Bind */
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(port);
#if defined(HAVE_SIN_LEN)
        addr.sin_len         = sizeof(addr);
#endif
        if (bind(x_sock, (struct sockaddr*)&addr, sizeof(struct sockaddr))!=0){
            int x_errno = SOCK_ERRNO;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[DSOCK::Create]  Failed bind()");
            SOCK_CLOSE(x_sock);
            return eIO_Unknown;
        }
    }

    /* Set to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        CORE_LOG(eLOG_Error,
                 "[DSOCK::Create]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    if ( !(*sock = (SOCK) calloc(1, sizeof(SOCK_struct))) ) {
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* Success... */
    (*sock)->sock     = x_sock;
    /* all timeouts cleared - infinite */
    /* no host and port - not "connected" */
    (*sock)->r_on_w   = eOff;
    (*sock)->i_on_sig = eDefault;
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    BUF_SetChunkSize(&(*sock)->w_buf, SOCK_BUF_CHUNK_SIZE);
    (*sock)->r_status = eIO_Success;
    (*sock)->w_status = eIO_Success;
    (*sock)->is_eof   = 0/*false*/;
    (*sock)->id       = ++s_ID_Counter * 1000;
    (*sock)->log_data = log;
    (*sock)->type     = eSOCK_Datagram;

    return eIO_Success;
}


extern EIO_Status DSOCK_SendMsg(SOCK            sock,
                                const char*     host,
                                unsigned short  port,
                                const void*     data,
                                size_t          datalen)
{
    EIO_Status         status;
    unsigned short     x_port;
    unsigned int       x_host;
    struct sockaddr_in addr;
    size_t             size;
    void*              msg;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[DSOCK::SendMsg]  Not a datagram socket");
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    if ( datalen )
        s_Write(sock, data, datalen, &size);
    sock->is_eof = 1/*true - finalized message*/;

    x_port = port ? port : sock->port;
    if (host  &&  *host) {
        if ( !(x_host = SOCK_gethostbyname(host)) ) {
            CORE_LOGF(eLOG_Error, ("[DSOCK::SendMsg]  Failed "
                                   "SOCK_gethostbyname(%.64s)", host));
            return eIO_Unknown;
        }
    } else
        x_host = sock->host;

    if (!x_host  ||  !x_port) {
        CORE_LOG(eLOG_Error, "[DSOCK::SendMsg]  Cannot do default send "
                 "on non-connected socket");
        return eIO_Unknown;
    }

    if ((size = BUF_Size(sock->w_buf)) != 0) {
        if ( !(msg = malloc(size + datalen)) ) {
            return eIO_Unknown;
        }
        verify(BUF_Peek(sock->w_buf, msg, size) == size);
    } else
        msg = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(x_host);
    addr.sin_port        = htons(x_port);
#if defined(HAVE_SIN_LEN)
    addr.sin_len         = sizeof(addr);
#endif

    for (;;) { /* optionally auto-resume if interrupted by a signal */
        int  x_written;
        int  x_errno;
        if ((x_written = sendto(sock->sock, msg, size, 0,
                                (struct sockaddr*) &addr, sizeof(addr))) >= 0){
            /* statistics & logging */
            if (sock->log_data == eOn  ||
                (sock->log_data == eDefault  &&  s_LogData == eOn)) {
                s_DoLogData(sock, eIO_Write, data, (size_t) x_written);
            }
            sock->n_written += x_written;
            if (x_written != size) {
                status = sock->w_status = eIO_Closed;
                CORE_LOG(eLOG_Error,"[DSOCK::SendMsg]  Partial datagram sent");
                break;
            }
            status = sock->w_status = eIO_Success;
            break;
        }

        x_errno = SOCK_ERRNO;
        /* don't want to handle all possible errors... let them be "unknown" */
        status = sock->w_status = eIO_Unknown;

        /* blocked -- retry if unblocked before the timeout is expired */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pull incoming data from the socket */
            status = s_Select(1, &poll, sock->w_timeout);
            if (status != eIO_Success)
                break;
            if (poll.revent == eIO_Close) {
                status = eIO_Unknown;
                break;
            }
            assert(poll.revent == eIO_Write);
            continue;
        }

        if (x_errno != SOCK_EINTR) {
            CORE_LOG_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                              "[DSOCK::SendMsg]  Failed sendto()");
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            status = sock->w_status = eIO_Interrupt;
            break;
        }
    }

    if ( msg )
        free(msg);
    if (status == eIO_Success)
        sock->w_status = s_WipeWBuf(sock);
    return status;
}


extern EIO_Status DSOCK_RecvMsg(SOCK            sock,
                                size_t          msgsize,
                                void*           buf,
                                size_t          buflen,
                                size_t*         msglen,
                                unsigned int*   sender_addr,
                                unsigned short* sender_port)
{
    EIO_Status status;
    void*      x_msg;
    int        x_msgsize;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[DSOCK::RecvMsg]  Not a datagram socket");
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    s_WipeRBuf(sock);
    if ( msglen )
        *msglen = 0;
    if ( sender_addr )
        *sender_addr = 0;
    if ( sender_port )
        *sender_port = 0;

    x_msgsize = (msgsize  &&  msgsize < ((1 << 16) - 1))
        ? (int) msgsize : ((1 << 16) - 1);

    if ( !(x_msg = x_msgsize <= buflen ? buf : malloc((size_t) x_msgsize)) )
         return eIO_Unknown;

    for (;;) { /* auto-resume if either blocked or interrupted */
        int                x_errno;
        struct sockaddr_in addr;
#if defined(HAVE_SOCKLEN_T)
        typedef socklen_t  SOCK_socklen_t;
#else
        typedef int        SOCK_socklen_t;
#endif
        SOCK_socklen_t     addrlen = (SOCK_socklen_t) sizeof(addr);
        int                x_read = recvfrom(sock->sock, x_msg, x_msgsize, 0,
                                             (struct sockaddr*)&addr,&addrlen);
        if (x_read >= 0) {
            /* got a message */
            status = sock->r_status = eIO_Success;
            if ( x_read ) {
                if ( msglen )
                    *msglen = x_read;
                if ( sender_addr )
                    memcpy(sender_addr, &addr.sin_addr, sizeof(*sender_addr));
                if ( sender_port )
                    *sender_port = ntohs(addr.sin_port);
                if (x_read > buflen  &&  !BUF_Write(&sock->r_buf,
                                                    (char*) x_msg  + buflen,
                                                    (size_t)x_read - buflen)) {
                    sock->r_status = eIO_Unknown;
                }
                if (buflen  &&  x_msgsize > buflen)
                    memcpy(buf, x_msg, buflen);
            }
            /* statistics & logging */
            if (sock->log_data == eOn  ||
                (sock->log_data == eDefault  &&  s_LogData == eOn)) {
                s_DoLogData(sock, eIO_Read, x_msg, (size_t) x_read);
            }
            sock->n_read += x_read;
            break;
        }

        x_errno = SOCK_ERRNO;
        status = sock->r_status = eIO_Unknown;
        if (x_errno != SOCK_EWOULDBLOCK  &&
            x_errno != SOCK_EAGAIN       &&
            x_errno != SOCK_EINTR) {
            /* catch unknown ERROR */
            CORE_LOG_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                              "[DSOCK::RecvMsg]  Failed recvfrom()");
            break;
        }

        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            if ((status = s_Select(1, &poll, sock->r_timeout)) != eIO_Success)
                break;
            if (poll.revent == eIO_Close) {
                status = eIO_Closed;
                break;
            }
            assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
            continue;
        }

        if (x_errno != SOCK_EINTR)
            break;

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            status = eIO_Interrupt;
            break;
        }
    }

    if (x_msgsize > buflen)
        free(x_msg);
    return status;
}


extern EIO_Status DSOCK_WaitMsg(SOCK sock, const STimeout* timeout)
{
    EIO_Status     status;
    SSOCK_Poll     poll;
    struct timeval tv;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[DSOCK::WaitMsg]  Not a datagram socket");
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    poll.sock   = sock;
    poll.event  = eIO_Read;
    poll.revent = eIO_Open;

    if ((status = s_Select(1, &poll, s_to2tv(timeout, &tv))) != eIO_Success  ||
        poll.revent == eIO_Read) {
        return status;
    }

    assert(poll.revent == eIO_Close);
    return eIO_Closed;
}


extern EIO_Status DSOCK_WipeMsg(SOCK sock, EIO_Event direction)
{
    EIO_Status status;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[DSOCK::WipeMsg]  Not a datagram socket");
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    switch (direction) {
    case eIO_Read:
        status = s_WipeRBuf(sock);
        sock->r_status = status;
        break;
    case eIO_Write:
        status = s_WipeWBuf(sock);
        sock->w_status = status;
        break;
    default:
        CORE_LOG(eLOG_Error, "[DSOCK::WipeMsg]  Invalid direction");
        assert(0);
        status = eIO_InvalidArg;
        break;
    }

    return status;
}


extern EIO_Status DSOCK_SetBroadcast(SOCK sock, int/*bool*/ broadcast)
{
    if (sock->type != eSOCK_Datagram) {
        CORE_LOG(eLOG_Error, "[DSOCK::SetBroadcast]  Not a datagram socket");
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    {{
#  if defined(NCBI_OS_MSWIN)
        BOOL bcast = !!broadcast;
#  else
        int  bcast = !!broadcast;
#  endif
        if (setsockopt(sock->sock, SOL_SOCKET, SO_BROADCAST,
                       (const char*) &bcast, sizeof(bcast)) != 0) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("[DSOCK::SetBroadcast]  Failed "
                                "setsockopt(%sBROADCAST)", bcast ? "" : "NO"));
            return eIO_Unknown;
        }
        return eIO_Success;
    }}
#else
    return eIO_NotSupported;
#endif
}


extern int/*bool*/ SOCK_IsClientSide(SOCK sock)
{
    return sock->type == eSOCK_ClientSide;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock->type == eSOCK_ServerSide;
}


extern int/*bool*/ SOCK_IsDatagram(SOCK sock)
{
    return sock->type == eSOCK_Datagram;
}


extern int SOCK_gethostname(char*  name,
                            size_t namelen)
{
    int error = 0;

    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    name[0] = name[namelen - 1] = '\0';
    if (gethostname(name, (int) namelen) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[SOCK_gethostname]  Cannot get local hostname");
        error = 1;
    } else if ( name[namelen - 1] ) {
        CORE_LOG(eLOG_Error, "[SOCK_gethostname]  Buffer too small");
        error = 1;
    }

    if ( !error )
        return 0/*success*/;

    name[0] = '\0';
    return -1/*failed*/;
}


extern int SOCK_ntoa(unsigned int host,
                     char*        buf,
                     size_t       buflen)
{
    const unsigned char* b = (const unsigned char*) &host;
    char str[16];

    assert(buf && buflen > 0);
    verify(sprintf(str, "%u.%u.%u.%u",
                   (unsigned) b[0], (unsigned) b[1],
                   (unsigned) b[2], (unsigned) b[3]) > 0);
    assert(strlen(str) < sizeof(str));

    if (strlen(str) >= buflen) {
        buf[0] = '\0';
        return -1/*failed*/;
    }

    strcpy(buf, str);
    return 0/*success*/;
}


extern unsigned int SOCK_HostToNetLong(unsigned int value)
{
    return htonl(value);
}


extern unsigned short SOCK_HostToNetShort(unsigned short value)
{
    return htons(value);
}


extern unsigned int SOCK_gethostbyname(const char* hostname)
{
    unsigned int host;
    char buf[256];

    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    if ( !hostname ) {
        if (SOCK_gethostname(buf, sizeof(buf)) != 0)
            return 0;
        hostname = buf;
    }

    host = inet_addr(hostname);
    if (host == htonl(INADDR_NONE)) {
        int x_errno;
#if defined(HAVE_GETADDRINFO)
        struct addrinfo hints, *out = 0;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET; /* We only handle IPv4. */
        if ((x_errno = getaddrinfo(hostname, 0, &hints, &out)) == 0  &&  out) {
            struct sockaddr_in* addr = (struct sockaddr_in *) out->ai_addr;
            assert(addr->sin_family == AF_INET);
            host = addr->sin_addr.s_addr;
        } else {
            if (s_LogData == eOn) {
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno,SOCK_STRERROR(x_errno),
                                  "[SOCK_gethostbyname]  Failed "
                                  "getaddrinfo()");
            }
            host = 0;
        }
        if ( out ) {
            freeaddrinfo(out);
        }
#else /* Use some variant of gethostbyname */
        struct hostent* he;
#  if defined(HAVE_GETHOSTBYNAME_R)
        struct hostent  x_he;
        char            x_buf[1024];

        x_errno = 0;
#    if (HAVE_GETHOSTBYNAME_R == 5)
        he = gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf), &x_errno);
#    elif (HAVE_GETHOSTBYNAME_R == 6)
        if (gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf),
                            &he, &x_errno) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYNAME_R value"
#    endif
#  else
        CORE_LOCK_WRITE;
        he = gethostbyname(hostname);
#    ifdef NCBI_OS_MAC
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if ( he ) {
            memcpy(&host, he->h_addr, sizeof(host));
        } else {
            host = 0;
        }

#  if !defined(HAVE_GETHOSTBYNAME_R)
        CORE_UNLOCK;
#  endif

        if (!host  &&  s_LogData == eOn) {
#  ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_gethostbyname]  Failed "
                              "gethostbyname[_r]()");
        }

#endif /*HAVE_GETADDR_INFO*/
    }

    return host;
}


extern char* SOCK_gethostbyaddr(unsigned int host,
                                char*        name,
                                size_t       namelen)
{
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    if ( !host ) {
        host = SOCK_gethostbyname(0);
    }

    if ( host ) {
        int x_errno;
#if defined(HAVE_GETNAMEINFO)
        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = host;
#  if defined(HAVE_SIN_LEN)
        addr.sin_len = sizeof(addr);
#  endif
        if ((x_errno = getnameinfo((struct sockaddr*) &addr, sizeof(addr),
                                   name, namelen, 0, 0, 0)) == 0) {
            return name;
        } else {
            if (s_LogData == eOn) {
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno,SOCK_STRERROR(x_errno),
                                  "[SOCK_gethostbyaddr]  Failed "
                                  "getnameinfo()");
            }
            name[0] = '\0';
            return 0;
        }
#else /* Use some variant of gethostbyaddr */
        struct hostent* he;
#  if defined(HAVE_GETHOSTBYADDR_R)
        struct hostent  x_he;
        char            x_buf[1024];

        x_errno = 0;
#    if (HAVE_GETHOSTBYADDR_R == 7)
        he = gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                             x_buf, sizeof(x_buf), &x_errno);
#    elif (HAVE_GETHOSTBYADDR_R == 8)
        if (gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                            x_buf, sizeof(x_buf), &he, &x_errno) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYADDR_R value"
#    endif
#  else
        CORE_LOCK_WRITE;
        he = gethostbyaddr((char*) &host, sizeof(host), AF_INET);
#    if defined(NCBI_OS_MAC)
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!he  ||  strlen(he->h_name) >= namelen) {
            if (he  ||  SOCK_ntoa(host, name, namelen) != 0) {
                x_errno = ERANGE;
                name[0] = '\0';
                name = 0;
            }
        } else {
            strcpy(name, he->h_name);
        }

#  ifndef HAVE_GETHOSTBYADDR_R
        CORE_UNLOCK;
#  endif

        if (!name  &&  s_LogData == eOn) {
#ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#endif
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_gethostbyaddr]  Failed "
                              "gethostbyaddr[_r]()");
        }

        return name;

#endif /*HAVE_GETNAMEINFO*/
    }

    name[0] = '\0';
    return 0;
}


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.76  2003/01/15 19:52:47  lavr
 * Datagram sockets added
 *
 * Revision 6.75  2002/12/06 16:38:35  lavr
 * Fix for undefined h_errno on MacOS 9
 *
 * Revision 6.74  2002/12/06 15:06:54  lavr
 * Add missing x_errno definition in s_Connect() for non-Unix platforms
 *
 * Revision 6.73  2002/12/05 21:44:12  lavr
 * Implement SOCK_STRERROR() and do more accurate error logging
 *
 * Revision 6.72  2002/12/05 16:31:14  lavr
 * Define SOCK_Create() as a call
 *
 * Revision 6.71  2002/12/04 21:01:05  lavr
 * -CORE_LOG[F]_SYS_ERRNO()
 *
 * Revision 6.70  2002/12/04 16:55:02  lavr
 * Implement logging on connect and close
 *
 * Revision 6.69  2002/11/08 17:18:18  lavr
 * Minor change: spare -1 in >= by replacing it with >
 *
 * Revision 6.68  2002/11/01 20:12:55  lavr
 * Reimplement SOCK_gethostname() - was somewhat potentally buggy
 *
 * Revision 6.67  2002/10/29 22:20:52  lavr
 * Use proper indentation of preproc. macros; note post-accept() socket state
 *
 * Revision 6.66  2002/10/28 15:45:58  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.65  2002/10/11 19:50:55  lavr
 * Few renames of internal functions (s_NCBI_Recv, s_Recv, etc)
 * Interface change: SOCK_gethostbyaddr() returns dotted IP address
 * when failed to convert given address into FQDN, it also now accepts
 * 0 to return the name (or dotted IP) of the local host
 *
 * Revision 6.64  2002/09/13 19:26:46  lavr
 * Few style-conforming changes
 *
 * Revision 6.63  2002/09/06 15:45:03  lavr
 * Return eIO_InvalidArg instead of generic eIO_Unknown where appropriate
 *
 * Revision 6.62  2002/09/04 15:11:00  lavr
 * Print ERRNO with failed connection attempt
 *
 * Revision 6.61  2002/08/28 15:59:24  lavr
 * Removed few redundant checks in s_SelectStallsafe()
 *
 * Revision 6.60  2002/08/27 03:16:15  lavr
 * Rename SOCK_htonl -> SOCK_HostToNetLong, SOCK_htons -> SOCK_HostToNetShort
 *
 * Revision 6.59  2002/08/15 18:46:52  lavr
 * s_Select(): do not return immediately if given all NULL sockets in "polls"
 *
 * Revision 6.58  2002/08/13 19:51:44  lavr
 * Fix letter case in s_SetNonblock() call
 *
 * Revision 6.57  2002/08/13 19:39:04  lavr
 * Set accepted socket in non-blocking mode after accept (not inherited)
 *
 * Revision 6.56  2002/08/13 19:29:30  lavr
 * Implement interrupted I/O
 *
 * Revision 6.55  2002/08/12 15:06:38  lavr
 * Implementation of plain and persistent SOCK_Write()
 *
 * Revision 6.54  2002/08/07 16:36:45  lavr
 * Added SOCK_SetInterruptOnSignal[API] calls and support placeholders
 * Renamed SOCK_GetAddress() -> SOCK_GetPeerAddress() and enum
 * ENH_ByteOrder employed in the latter call as the last arg
 * All-UNIX-specific kludge to check connected status on non-blocking socket
 * Added more checks against SOCK_INVALID and more error log messages
 *
 * Revision 6.53  2002/07/18 20:19:58  lebedev
 * NCBI_OS_MAC: unistd.h added
 *
 * Revision 6.52  2002/07/15 19:32:03  lavr
 * Do not intercept SIGPIPE in the case of an installed signal handler
 *
 * Revision 6.51  2002/07/01 20:52:23  lavr
 * Error (trace) printouts added in s_Select() and s_NCBI_Recv()
 *
 * Revision 6.50  2002/06/17 18:28:52  lavr
 * +BeOS specifics (by Vladimir Ivanov)
 *
 * Revision 6.49  2002/06/10 21:14:22  ucko
 * [SOCK_gethostbyaddr] When using getnameinfo, properly initialize the
 * sockaddr_in; in particular, set sin_len when present.
 * Also set sin_len in other functions that build a struct sockaddr_in.
 *
 * Revision 6.48  2002/06/10 19:52:45  lavr
 * Additional failsafe check whether the socket actually connected (Solaris)
 *
 * Revision 6.47  2002/05/13 19:49:22  ucko
 * Indent with spaces rather than tabs
 *
 * Revision 6.46  2002/05/13 19:08:11  ucko
 * Use get{addr,name}info in favor of gethostby{name,addr}(_r) when available
 *
 * Revision 6.45  2002/05/06 19:19:43  lavr
 * Remove unnecessary inits of fields returned from s_Select()
 *
 * Revision 6.44  2002/04/26 16:41:16  lavr
 * Redesign of waiting mechanism, and implementation of SOCK_Poll()
 *
 * Revision 6.43  2002/04/22 20:53:16  lavr
 * +SOCK_htons(); Set close timeout only when the socket was not yet shut down
 *
 * Revision 6.42  2002/04/17 20:05:05  lavr
 * Cosmetic changes
 *
 * Revision 6.41  2002/03/22 19:52:19  lavr
 * Do not include <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.40  2002/02/11 20:36:44  lavr
 * Use "ncbi_config.h"
 *
 * Revision 6.39  2002/01/28 20:29:52  lavr
 * Distinguish between EOF and severe read error
 * Return eIO_Success if waiting for read in a stream with EOF already seen
 *
 * Revision 6.38  2001/12/03 21:35:32  vakatov
 * + SOCK_IsServerSide()
 * SOCK_Reconnect() - check against reconnect of server-side socket to its peer
 *
 * Revision 6.37  2001/11/07 19:00:11  vakatov
 * LSOCK_Accept() -- minor adjustments
 *
 * Revision 6.36  2001/08/31 16:00:58  vakatov
 * [MSWIN] "setsockopt()" -- Start using SO_REUSEADDR on MS-Win.
 * [MAC]   "setsockopt()" -- Do not use it on MAC whatsoever (as it is not
 *         implemented in the M.I.T. socket emulation lib).
 *
 * Revision 6.35  2001/08/29 17:32:56  juran
 * Define POSIX macros missing from Universal Interfaces 3.4
 * in terms of the 'proper' constants.
 * Complain about unsupported platforms at compile-time, not runtime.
 *
 * Revision 6.34  2001/07/11 16:16:39  vakatov
 * Fixed comments for HAVE_GETHOSTBYNAME_R, HAVE_GETHOSTBYADDR_R; other
 * minor (style and messages) fixes
 *
 * Revision 6.33  2001/07/11 00:54:35  vakatov
 * SOCK_gethostbyname() and SOCK_gethostbyaddr() -- now can work with
 * gethostbyname_r() with 6 args and gethostbyaddr_r() with 8 args
 * (in addition to those with 5 and 7 args, repectively).
 * [NCBI_OS_IRIX] s_Select() -- no final ASSERT() if built on IRIX.
 * SOCK_gethostbyaddr() -- added missing CORE_UNLOCK.
 *
 * Revision 6.32  2001/06/20 21:26:18  vakatov
 * As per A.Grichenko/A.Lavrentiev report:
 *   SOCK_Shutdown() -- typo fixed (use "how" rather than "x_how").
 *   SOCK_Shutdown(READ) -- do not call system shutdown if EOF was hit.
 *   Whenever shutdown on both READ and WRITE:  do the WRITE shutdown first.
 *
 * Revision 6.31  2001/06/04 21:03:08  vakatov
 * + HAVE_SOCKLEN_T
 *
 * Revision 6.30  2001/05/31 15:42:10  lavr
 * INADDR_* constants are all and always in host byte order -
 * this was mistakenly forgotten, and now fixed by use of htonl().
 *
 * Revision 6.29  2001/05/23 21:03:35  vakatov
 * s_SelectStallsafe() -- fix for the interpretation of "default" R-on-W mode
 * (by A.Lavrentiev)
 *
 * Revision 6.28  2001/05/21 15:10:32  ivanov
 * Added (with Denis Vakatov) automatic read on write data from the socket
 * (stall protection).
 * Added functions SOCK_SetReadOnWriteAPI(), SOCK_SetReadOnWrite()
 * and internal function s_SelectStallsafe().
 *
 * Revision 6.27  2001/04/25 19:16:01  juran
 * Set non-blocking mode on Mac OS. (from pjc)
 *
 * Revision 6.26  2001/04/24 21:03:42  vakatov
 * s_NCBI_Recv()   -- restore "r_status" to eIO_Success on success.
 * SOCK_Wait(READ) -- return eIO_Success if data pending in the buffer.
 * (w/A.Lavrentiev)
 *
 * Revision 6.25  2001/04/23 22:22:08  vakatov
 * SOCK_Read() -- special treatment for "buf" == NULL
 *
 * Revision 6.24  2001/04/04 14:58:59  vakatov
 * Cleaned up after R6.23 (and get rid of the C++ style comments)
 *
 * Revision 6.23  2001/04/03 20:30:15  juran
 * Changes to work with OT sockets.
 * Not all of pjc's changes are here -- I will test them shortly.
 *
 * Revision 6.22  2001/03/29 21:15:36  lavr
 * More accurate length calculation in 'SOCK_gethostbyaddr'
 *
 * Revision 6.21  2001/03/22 17:43:54  vakatov
 * Typo fixed in the SOCK_AllowSigPipeAPI() proto
 *
 * Revision 6.20  2001/03/22 17:40:36  vakatov
 * + SOCK_AllowSigPipeAPI()
 *
 * Revision 6.19  2001/03/06 23:54:20  lavr
 * Renamed: SOCK_gethostaddr -> SOCK_gethostbyname
 * Added:   SOCK_gethostbyaddr
 *
 * Revision 6.18  2001/03/02 20:10:29  lavr
 * Typos fixed
 *
 * Revision 6.17  2001/02/28 00:55:38  lavr
 * SOCK_gethostaddr: InitAPI added, SOCK_gethostname used instead of
 * gethostname
 *
 * Revision 6.16  2001/01/26 23:50:32  vakatov
 * s_NCBI_Recv() -- added check for ENOTCONN to catch EOF (mostly for Mac)
 *
 * Revision 6.15  2001/01/25 17:10:41  lavr
 * The following policy applied: on either read or write,
 * n_read and n_written returned to indicate actual number of passed
 * bytes, regardless of error status. eIO_Success means that the
 * operation went through smoothly, while any other status has to
 * be analyzed. Anyway, the number of passed bytes prior the error
 * occurred is returned in n_read and n_written respectively.
 *
 * Revision 6.14  2001/01/23 23:19:34  lavr
 * Typo fixed in comment
 *
 * Revision 6.13  2000/12/28 21:26:27  lavr
 * Cosmetic fix to get rid of "converting -1 to unsigned" warning
 *
 * Revision 6.12  2000/12/26 21:40:03  lavr
 * SOCK_Read modified to handle properly the case of 0 byte reading
 *
 * Revision 6.11  2000/12/05 23:27:09  lavr
 * Added SOCK_gethostaddr
 *
 * Revision 6.10  2000/12/04 17:34:19  beloslyu
 * the order of include files is important, especially on other Unixes!
 * Look the man on inet_ntoa
 *
 * Revision 6.9  2000/11/22 19:29:16  vakatov
 * SOCK_Create() -- pre-set the sock handle to SOCK_INVALID before connect
 *
 * Revision 6.8  2000/11/15 18:51:21  vakatov
 * Add SOCK_Shutdown() and SOCK_Status().  Remove SOCK_Eof().
 * Add more checking and diagnostics.
 * [NOTE: not tested on Mac]
 *
 * Revision 6.7  2000/06/23 19:34:44  vakatov
 * Added means to log binary data
 *
 * Revision 6.6  2000/05/30 23:31:44  vakatov
 * SOCK_host2inaddr() renamed to SOCK_ntoa(), the home-made out-of-scratch
 * implementation to work properly with GCC on IRIX64 platforms
 *
 * Revision 6.5  2000/03/24 23:12:08  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.4  2000/02/23 22:34:35  vakatov
 * Can work both "standalone" and as a part of NCBI C++ or C toolkits
 *
 * Revision 6.3  1999/10/19 22:21:48  vakatov
 * Put the call to "gethostbyname()" into a CRITICAL_SECTION
 *
 * Revision 6.2  1999/10/19 16:16:01  vakatov
 * Try the NCBI C and C++ headers only if NCBI_OS_{UNIX, MSWIN, MAC} is
 * not #define'd
 *
 * Revision 6.1  1999/10/18 15:36:38  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 *
 * ===========================================================================
 */
