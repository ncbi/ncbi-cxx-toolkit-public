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

/* NCBI core headers
 */
#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_socket_unix.h>

#define NCBI_USE_ERRCODE_X   Connect_Socket

/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC!"
#endif /*supported platforms*/

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
#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  else
#    include <netdb.h>
#  endif
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  ifndef NCBI_COMPILER_METROWERKS
#    include <netinet/tcp.h>
#    if defined(NCBI_OS_LINUX)  &&  !defined(IP_MTU)
#      define IP_MTU 14
#    endif
#  endif
#  if !defined(NCBI_OS_BEOS) && !defined(NCBI_COMPILER_MW_MSL)
#    include <arpa/inet.h>
#  endif /*NCBI_OS_BEOS*/
#  include <signal.h>
#  include <sys/un.h>

#elif defined(NCBI_OS_MSWIN)
#  ifndef NCBI_COMPILER_METROWERKS
#    include <winsock2.h>
#  else
#    define SD_RECEIVE      0x00
#    define SD_SEND         0x01
#    define SD_BOTH         0x02
#  endif
#elif defined(NCBI_OS_MAC)
#  include <unistd.h>
#  include <sock_ext.h>
#ifdef __MWERKS__
# if TARGET_API_MAC_CARBON
#  include <carbon_netdb.h>
#else
#  include <netdb.h>
#endif
#else
#  include <netdb.h>
#endif
#  include <s_types.h>
#  include <s_socket.h>
#  include <neti_in.h>
#  include <a_inet.h>
#  include <neterrno.h> /* missing error numbers on Mac */

#else
#  error "Unsupported platform, must be one of NCBI_OS_{UNIX|MSWIN|MAC} !!!"
#endif /* platform-specific headers (for UNIX, MSWIN, MAC) */

#ifdef NCBI_CXX_TOOLKIT
#  include <corelib/ncbiatomic.h>
#endif /*NCBI_CXX_TOOLKIT*/

/* Portable standard C headers
 */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>


/******************************************************************************
 *  TYPEDEFS & MACROS
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE 4096


/* Macro #def for the platform-dependent constants, error codes and functions
 */
#if defined(NCBI_OS_MSWIN)

typedef SOCKET TSOCK_Handle;
typedef HANDLE TRIGGER_Handle;
#  define SOCK_INVALID        INVALID_SOCKET
#  define SOCK_ERRNO          WSAGetLastError()
#  define SOCK_EINTR          WSAEINTR
#  define SOCK_EWOULDBLOCK    WSAEWOULDBLOCK
#  define SOCK_EADDRINUSE     WSAEADDRINUSE
#  define SOCK_ECONNRESET     WSAECONNRESET
#  define SOCK_EPIPE          WSAESHUTDOWN
#  define SOCK_EAGAIN         WSAEINPROGRESS
#  define SOCK_EINPROGRESS    WSAEINPROGRESS
#  define SOCK_EALREADY       WSAEALREADY
#  define SOCK_ENOTCONN       WSAENOTCONN
#  define SOCK_ECONNABORTED   WSAECONNABORTED
#  define SOCK_ECONNREFUSED   WSAECONNREFUSED
#  define SOCK_ENETRESET      WSAENETRESET
#  define SOCK_ETIMEDOUT      WSAETIMEDOUT
#  define SOCK_NFDS(s)        0
#  define SOCK_CLOSE(s)       closesocket(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    SD_RECEIVE
#  define SOCK_SHUTDOWN_WR    SD_SEND
#  define SOCK_SHUTDOWN_RDWR  SD_BOTH
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_MSWIN */

#elif defined(NCBI_OS_UNIX)

typedef int TSOCK_Handle;
typedef int TRIGGER_Handle;
#  define SOCK_INVALID        (-1)
#  define SOCK_ERRNO          errno
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_EADDRINUSE     EADDRINUSE
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_EALREADY       EALREADY
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_ECONNABORTED   ECONNABORTED
#  define SOCK_ECONNREFUSED   ECONNREFUSED
#  define SOCK_ENETRESET      ENETRESET
#  define SOCK_ETIMEDOUT      ETIMEDOUT
#  define SOCK_NFDS(s)        (s + 1)
#  ifdef NCBI_OS_BEOS
#    define SOCK_CLOSE(s)     closesocket(s)
#  else
#    define SOCK_CLOSE(s)     close(s)	
#  endif /*NCBI_OS_BEOS*/
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  ifndef SHUT_RD
#    define SHUT_RD           0
#  endif /*SHUT_RD*/
#  define SOCK_SHUTDOWN_RD    SHUT_RD
#  ifndef SHUT_WR
#    define SHUT_WR           1
#  endif /*SHUT_WR*/
#  define SOCK_SHUTDOWN_WR    SHUT_WR
#  ifndef SHUT_RDWR
#    define SHUT_RDWR         2
#  endif /*SHUT_RDWR*/
#  define SOCK_SHUTDOWN_RDWR  SHUT_RDWR
#  ifndef INADDR_NONE
#    define INADDR_NONE       ((unsigned int)(-1))
#  endif /*INADDR_NONE*/
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_UNIX */

#elif defined(NCBI_OS_MAC)

#  if TARGET_API_MAC_CARBON
#    define O_NONBLOCK kO_NONBLOCK
#  endif /*TARGET_API_MAC_CARBON*/

typedef int TSOCK_Handle;
typedef int TRIGGER_Handle;
#  define SOCK_INVALID        (-1)
#  ifndef SOCK_ERRNO
#    define SOCK_ERRNO        errno
#  endif /*SOCK_ERRNO*/
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_EADDRINUSE     EADDRINUSE
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_EALREADY       EALREADY
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_ECONNABORTED   ECONNABORTED
#  define SOCK_ECONNREFUSED   ECONNREFUSED
#  define SOCK_ENETRESET      ENETRESET
#  define SOCK_ETIMEDOUT      ETIMEDOUT
#  define SOCK_NFDS(s)        (s + 1)
#  define SOCK_CLOSE(s)       close(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    0
#  define SOCK_SHUTDOWN_WR    1
#  define SOCK_SHUTDOWN_RDWR  2
#  define SOCK_STRERROR(err)  s_StrError(err)
#  ifdef NETDB_INTERNAL
#    undef NETDB_INTERNAL
#  endif /*NETDB_INTERNAL*/
#  ifndef INADDR_LOOPBACK
#    define	INADDR_LOOPBACK	0x7F000001
#  endif /*INADDR_LOOPBACK*/

#endif /*NCBI_OS_MSWIN, NCBI_OS_UNIX, NCBI_OS_MAC*/

#ifdef sun
#  undef sun
#endif


#if defined(HAVE_SOCKLEN_T)  ||  defined (_SOCKLEN_T)
typedef socklen_t  SOCK_socklen_t;
#else
typedef int	       SOCK_socklen_t;
#endif


#if 0/*defined(__GNUC__)*/
typedef ESwitch    EBSwitch;
typedef EIO_Status EBIO_Status;
typedef ESockType  EBSockType;
#else
typedef unsigned   EBSwitch;
typedef unsigned   EBIO_Status;
typedef unsigned   EBSockType;
#endif


typedef enum {
    eInvalid = 0,
    eTrigger,
    eListening,
    eSocket
} EType;
typedef unsigned short TType;

/* Event trigger
 */
typedef struct TRIGGER_tag {
    TRIGGER_Handle  fd;         /* OS-specific trigger handle                */
    unsigned int    id;         /* the internal ID (cf. "s_ID_Counter")      */

    volatile int    isset;      /* trigger state                             */
    unsigned short  reserved;   /* MBZ                                       */

    TType           type;       /* eTrigger                                  */

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    EBSwitch             log:2; /* how to log events                         */
    EBSockType         stype:2; /* MBZ                                       */
    EBSwitch          r_on_w:2; /* MBZ                                       */
    EBSwitch        i_on_sig:2; /* eDefault                                  */
    EBIO_Status     r_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/     eof:1; /* 0                                         */
    EBIO_Status     w_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/ pending:1; /* 0                                         */

#ifdef NCBI_OS_UNIX
    void* volatile  lock;       /* trigger read-out lock                     */
    int             out;        /* must go last                              */
#endif /*NCBI_OS_UNIX*/
} TRIGGER_struct;


/* Listening socket [must be in one-2-one binary correspondene with TRIGGER]
 */
typedef struct LSOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
    unsigned int    id;         /* the internal ID (see also "s_ID_Counter") */

    unsigned int    n_accept;   /* total number of accepted clients          */
    unsigned short  port;       /* port on which listening (host byte order) */

    TType           type;       /* eListening                                */

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    EBSwitch             log:2; /* how to log events and data for this socket*/
    EBSockType         stype:2; /* MBZ                                       */
    EBSwitch          r_on_w:2; /* MBZ                                       */
    EBSwitch        i_on_sig:2; /* eDefault                                  */
    EBIO_Status     r_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/     eof:1; /* 0                                         */
    EBIO_Status     w_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/ pending:1; /* 0                                         */

#ifdef NCBI_OS_UNIX
    char            path[1];    /* must go last                              */
#endif /*NCBI_OS_UNIX*/
} LSOCK_struct;


/* Types of connecting socket
 */
typedef enum {
    eSOCK_Datagram = 0,
    eSOCK_ClientSide,
    eSOCK_ServerSide,
    eSOCK_ServerSideKeep
} ESockType;


/* Socket [it must be in 1-2-1 binary correspondence with LSOCK above]
 */
typedef struct SOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
    unsigned int    id;         /* the internal ID (see also "s_ID_Counter") */

    /* connection point */
    unsigned int    host;       /* peer host (in the network byte order)     */
    unsigned short  port;       /* peer port (in the network byte order)     */

    TType           type;       /* eSocket                                   */

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    EBSwitch             log:2; /* how to log events and data for this socket*/
    EBSockType         stype:2; /* socket type: client- or server-side, dgram*/
    EBSwitch          r_on_w:2; /* enable/disable automatic read-on-write    */
    EBSwitch        i_on_sig:2; /* enable/disable I/O restart on signals     */
    EBIO_Status     r_status:3; /* read  status:  eIO_Closed if was shut down*/
    unsigned/*bool*/     eof:1; /* Stream sockets: 'End of file' seen on read
                                   Datagram socks: 'End of message' written  */
    EBIO_Status     w_status:3; /* write status:  eIO_Closed if was shut down*/
    unsigned/*bool*/ pending:1; /* != 0 if connection is still pending       */

    /* timeouts */
    const struct timeval* r_timeout;/* NULL if infinite, or points to "r_tv" */
    struct timeval  r_tv;       /* finite read  timeout value                */
    STimeout        r_to;       /* finite read  timeout value (aux., temp.)  */
    const struct timeval* w_timeout;/* NULL if infinite, or points to "w_tv" */
    struct timeval  w_tv;       /* finite write timeout value                */
    STimeout        w_to;       /* finite write timeout value (aux., temp.)  */
    const struct timeval* c_timeout;/* NULL if infinite, or points to "c_tv" */
    struct timeval  c_tv;       /* finite close timeout value                */
    STimeout        c_to;       /* finite close timeout value (aux., temp.)  */

    /* aux I/O data */
    BUF             r_buf;      /* read  buffer                              */
    BUF             w_buf;      /* write buffer                              */
    size_t          w_len;      /* SOCK: how much data is pending for output */

    /* statistics */
    size_t          n_read;     /* DSOCK: total #; SOCK: last connect/ only  */
    size_t          n_written;  /* DSOCK: total #; SOCK: last /session only  */
    size_t          n_in;       /* DSOCK: msg #; SOCK: total # of bytes read */
    size_t          n_out;      /* DSOCK: msg #; SOCK: total # of bytes sent */

    unsigned short  myport;     /* this socket's port number, host byte order*/

#ifdef NCBI_OS_UNIX
    /* pathname for UNIX socket */
    char            path[1];    /* must go last                              */
#endif /*NCBI_OS_UNIX*/
} SOCK_struct;


/*
 * Please note the following implementation details:
 *
 * 1. w_buf is used for stream sockets to keep initial data segment
 *    that has to be sent upon connection establishment.
 *
 * 2. eof is used differently for stream and datagram sockets:
 *    =1 for stream sockets means that read has hit EOF;
 *    =1 for datagram sockets means that message in w_buf has been completed.
 *
 * 3. r_status keeps completion code of the last low-level read call;
 *    however, eIO_Closed is there when the socket is shut down for reading;
 *    see the table below for full details on stream sockets.
 *
 * 4. w_status keeps completion code of the last low-level write call;
 *    however, eIO_Closed is there when the socket is shut down for writing.
 *
 * 5. The following table depicts r_status and eof combinations and their
 *    meanings for stream sockets:
 * -------------------------------+--------------------------------------------
 *              Field             |
 * ---------------+---------------+                  Meaning
 * sock->r_status |   sock->eof   |           (stream sockets only)
 * ---------------+---------------+--------------------------------------------
 * eIO_Closed     |       0       |  Socket shut down for reading
 * eIO_Closed     |       1       |  Read severely failed
 * not eIO_Closed |       0       |  Read completed with r_status error
 * not eIO_Closed |       1       |  Read hit EOF (and [maybe later] r_status)
 * ---------------+---------------+--------------------------------------------
 */


/* Globals:
 */


/* Flag to indicate whether the API has been initialized */
static int/*bool*/ s_Initialized = 0/*false*/;

/* SOCK counter */
static unsigned int s_ID_Counter = 0;

/* Cached IP address of local host */
static unsigned int s_LocalHostAddress = 0;

/* Read-while-writing switch */
static ESwitch s_ReadOnWrite = eOff;        /* no read-on-write by default   */

/* Reuse address flag for newly created stream sockets */
static int/*bool*/ s_ReuseAddress = 0;      /* off by default                */

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
#endif /*NCBI_OS_MSWIN*/
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
#endif /*NCBI_OS_MSWIN*/
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
#ifdef NO_ADDRESS
        {NO_ADDRESS + DNS_BASE,
                                 "No address record found in DNS"},
#endif /*NO_ADDRESS*/
#ifdef NO_DATA
        {NO_DATA + DNS_BASE,
                                 "No DNS data of requested type"},
#endif /*NO_DATA*/

        /* Last dummy entry - must present */
        {0, 0}
    };
    size_t i, n = sizeof(errmap) / sizeof(errmap[0]) - 1/*dummy entry*/;

    if (!error)
        return "";
    for (i = 0;  i < n;  i++) {
        if (errmap[i].errnum == error)
            return errmap[i].errtxt;
    }
    return strerror(error);
}


/******************************************************************************
 *   Data Logging
 */


static const char* s_ID(const SOCK sock, char* buf)
{
    const char* sname;

    if ( !sock )
        return "";
    switch (sock->type) {
    case eTrigger:
        sname = "TRIGGER";
        break;
    case eListening:
        sname = "LSOCK";
        break;
    case eSocket:
        sname = "SOCK";
        break;
    default:
        sname = "?";
        assert(0);
        break;
    }
    if (sock->sock == SOCK_INVALID)
        sprintf(buf, "%s#%u[?]: ",  sname, sock->id);
    else
        sprintf(buf, "%s#%u[%u]: ", sname, sock->id, (unsigned int)sock->sock);
    return buf;
}


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLog
(const SOCK  sock, EIO_Event event,
 const void* data, size_t    size,  const void* ptr)
{
    const struct sockaddr* sa = (const struct sockaddr*) ptr;
    char head[128];
    char tail[128];
    char _id[32];

    if ( !CORE_GetLOG() )
        return;

    assert(sock);
    switch (event) {
    case eIO_Open:
        if (sock->stype == eSOCK_Datagram) {
            if ( !sa ) {
                strcpy(head, "Datagram socket created");
                *tail = 0;
            } else {
                const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
                if ( !data ) {
                    strcpy(head, "Datagram socket bound to port :");
                    sprintf(tail, "%hu", ntohs(sin->sin_port));
                } else {
                    strcpy(head, "Datagram socket connected to ");
                    SOCK_HostPortToString(sin->sin_addr.s_addr,
                                          ntohs(sin->sin_port),
                                          tail, sizeof(tail));
                }
            }
        } else {
            if (sock->stype == eSOCK_ClientSide)
                strcpy(head, "Connecting to ");
            else if ( data )
                strcpy(head, "Connected to ");
            else
                strcpy(head, "Accepted from ");
            if (sa->sa_family == AF_INET) {
                const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
                SOCK_HostPortToString(sin->sin_addr.s_addr,
                                      ntohs(sin->sin_port),
                                      tail, sizeof(tail));
            }
#ifdef NCBI_OS_UNIX
            else if (sa->sa_family == AF_UNIX) {
                const struct sockaddr_un* un = (const struct sockaddr_un*) sa;
                strncpy0(tail, un->sun_path, sizeof(tail) - 1);
            }
#endif /*NCBI_OS_UNIX*/
            else
                strcpy(tail, "???");
        }
        CORE_TRACEF(("%s%s%s", s_ID(sock, _id), head, tail));
        break;

    case eIO_Read:
    case eIO_Write:
        if (sock->stype == eSOCK_Datagram) {
            const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
            assert(sa  &&  sa->sa_family == AF_INET);
            SOCK_HostPortToString(sin->sin_addr.s_addr,
                                  ntohs(sin->sin_port),
                                  tail, sizeof(tail));
            sprintf(tail + strlen(tail), ", msg# %u",
                    (unsigned)(event == eIO_Read ? sock->n_in : sock->n_out));
        } else if (ptr)
            strncpy0(tail, " OUT-OF-BAND", sizeof(tail) - 1);
        else
            *tail = '\0';
        sprintf(head, "%s%s%s at offset %lu%s%s", s_ID(sock, _id),
                event == eIO_Read
                ? (sock->stype != eSOCK_Datagram  &&  !size
                   ? (data ? SOCK_STRERROR(*((int*) data)) : "EOF hit")
                   : "Read")
                : (sock->stype != eSOCK_Datagram  &&  !size
                   ? SOCK_STRERROR(*((int*) data)) : "Written"),
                sock->stype == eSOCK_Datagram  ||  size ? "" :
                (event == eIO_Read ? " while reading" : " while writing"),
                (unsigned long) (event == eIO_Read
                                 ? sock->n_read : sock->n_written),
                sock->stype == eSOCK_Datagram
                ? (event == eIO_Read ? " from " : " to ")
                : "", tail);
        CORE_DATA_X(109, data, size, head);
        break;

    case eIO_Close:
        {{
            int n = sprintf(head, "%lu byte%s",
                            (unsigned long) sock->n_written,
                            sock->n_written == 1 ? "" : "s");
            if (sock->stype == eSOCK_Datagram  ||
                sock->n_out != sock->n_written) {
                sprintf(head + n, "/%lu %s%s",
                        (unsigned long) sock->n_out,
                        sock->stype == eSOCK_Datagram ? "msg" : "total byte",
                        sock->n_out == 1 ? "" : "s");
            }
        }}
        {{
            int n = sprintf(tail, "%lu byte%s",
                            (unsigned long) sock->n_read,
                            sock->n_read == 1 ? "" : "s");
            if (sock->stype == eSOCK_Datagram  ||
                sock->n_in != sock->n_read) {
                sprintf(tail + n, "/%lu %s%s",
                        (unsigned long) sock->n_in,
                        sock->stype == eSOCK_Datagram ? "msg" : "total byte",
                        sock->n_in == 1 ? "" : "s");
            }
        }}
        CORE_TRACEF(("%s%s (out: %s, in: %s)", s_ID(sock, _id),
                     sock->stype == eSOCK_ServerSideKeep
                     ? "Leaving" : "Closing", head,tail));
        break;

    default:
        CORE_LOGF_X(1, eLOG_Error,
                    ("%s[SOCK::s_DoLog]  Invalid event %u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
        break;
    }
}


extern ESwitch SOCK_SetDataLoggingAPI(ESwitch log)
{
    ESwitch old = s_Log;
    if (log == eDefault)
        log = eOff;
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
 *  API Initialization and Shutdown/Cleanup
 */


extern void SOCK_AllowSigPipeAPI(void)
{
#ifdef NCBI_OS_UNIX
    s_AllowSigPipe = 1/*true - API will not mask SIGPIPE out at init*/;
#endif /*NCBI_OS_UNIX*/
    return;
}


#if 0/*defined(_DEBUG) && !defined(NDEBUG)*/

#  ifndef   SOCK_HAVE_SHOWDATALAYOUT
#    define SOCK_HAVE_SHOWDATALAYOUT 1
#  endif

#endif /*_DEBUG && !NDEBUG*/

#ifdef SOCK_HAVE_SHOWDATALAYOUT

#  if !defined(__GNUC__) && !defined(offsetof)
#    define offsetof(T, F) ((size_t)((char*) &(((T*) 0)->F) - (char*) 0))
#  endif

static void s_ShowDataLayout(void)
{
    CORE_LOGF_X(2, eLOG_Note,
                ("SOCK data layout:\n"
                 "    Sizeof(SOCK_struct) = %u, offsets follow\n"
                 "\tsock:      %u\n"
                 "\tid:        %u\n"
                 "\thost:      %u\n"
                 "\tport:      %u\n"
                 "\tbitfield:  16 bits\n"
                 "\tr_timeout: %u\n"
                 "\tr_tv:      %u\n"
                 "\tr_to:      %u\n"
                 "\tw_timeout: %u\n"
                 "\tw_tv:      %u\n"
                 "\tw_to:      %u\n"
                 "\tc_timeout: %u\n"
                 "\tc_tv:      %u\n"
                 "\tc_to:      %u\n"
                 "\tr_buf:     %u\n"
                 "\tw_buf:     %u\n"
                 "\tw_len:     %u\n"
                 "\tn_read:    %u\n"
                 "\tn_written: %u\n"
                 "\tn_in:      %u\n"
                 "\tn_out:     %u\n"
                 "\tmyport:    %u"
#  ifdef NCBI_OS_UNIX
                 "\n\tpath:      %u"
#  endif /*NCBI_OS_UNIX*/
                 , (unsigned int) sizeof(SOCK_struct),
                 (unsigned int) offsetof(SOCK_struct, sock),
                 (unsigned int) offsetof(SOCK_struct, id),
                 (unsigned int) offsetof(SOCK_struct, host),
                 (unsigned int) offsetof(SOCK_struct, port),
                 (unsigned int) offsetof(SOCK_struct, r_timeout),
                 (unsigned int) offsetof(SOCK_struct, r_tv),
                 (unsigned int) offsetof(SOCK_struct, r_to),
                 (unsigned int) offsetof(SOCK_struct, w_timeout),
                 (unsigned int) offsetof(SOCK_struct, w_tv),
                 (unsigned int) offsetof(SOCK_struct, w_to),
                 (unsigned int) offsetof(SOCK_struct, c_timeout),
                 (unsigned int) offsetof(SOCK_struct, c_tv),
                 (unsigned int) offsetof(SOCK_struct, c_to),
                 (unsigned int) offsetof(SOCK_struct, r_buf),
                 (unsigned int) offsetof(SOCK_struct, w_buf),
                 (unsigned int) offsetof(SOCK_struct, w_len),
                 (unsigned int) offsetof(SOCK_struct, n_read),
                 (unsigned int) offsetof(SOCK_struct, n_written),
                 (unsigned int) offsetof(SOCK_struct, n_in),
                 (unsigned int) offsetof(SOCK_struct, n_out),
                 (unsigned int) offsetof(SOCK_struct, myport)
#  ifdef NCBI_OS_UNIX
                 , (unsigned int) offsetof(SOCK_struct, path)
#  endif /*NCBI_OS_UNIX*/
                 ));
}

#endif /*SOCK_HAVE_SHOWDATALAYOUT*/


extern EIO_Status SOCK_InitializeAPI(void)
{
    CORE_LOCK_WRITE;
    if ( s_Initialized ) {
        CORE_UNLOCK;
        return eIO_Success;
    }

#ifdef SOCK_HAVE_SHOWDATALAYOUT
    s_ShowDataLayout();
#endif /*SOCK_HAVE_SHOWDATALAYOUT*/

#if defined(NCBI_OS_MSWIN)
    {{
        WSADATA wsadata;
        int x_errno = WSAStartup(MAKEWORD(1,1), &wsadata);
        if (x_errno) {
            CORE_UNLOCK;
            CORE_LOG_ERRNO_EXX(3, eLOG_Error,
                               x_errno, SOCK_STRERROR(x_errno),
                               "[SOCK::InitializeAPI]  Failed WSAStartup()");
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
#endif /*platform-specific init*/

    s_Initialized = 1/*true*/;
#ifdef NCBI_OS_MSWIN
    {{
        static int/*bool*/ s_AtExitSet = 0;
        if ( !s_AtExitSet ) {
            atexit((void (*)(void)) SOCK_ShutdownAPI);
            s_AtExitSet = 1;
        }
    }}
#endif

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
#ifdef NCBI_OS_MSWIN
    {{
        int x_errno = WSACleanup() ? SOCK_ERRNO : 0;
        CORE_UNLOCK;
        if ( x_errno ) {
            CORE_LOG_ERRNO_EXX(4, eLOG_Warning,
                               x_errno, SOCK_STRERROR(x_errno),
                               "[SOCK::ShutdownAPI]  Failed WSACleanup()");
            return eIO_Unknown;
        }
    }}
#else
    CORE_UNLOCK;
#endif /*NCBI_OS_MSWIN*/

    return eIO_Success;
}



/******************************************************************************
 *  LSOCK & SOCK AUXILIARIES
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

    tv->tv_sec  = to->usec / 1000000 + to->sec;
    tv->tv_usec = to->usec % 1000000;
    return tv;
}


/* Switch the specified socket I/O between blocking and non-blocking mode
 */
static int/*bool*/ s_SetNonblock(TSOCK_Handle sock, int/*bool*/ nonblock)
{
#if defined(NCBI_OS_MSWIN)
    unsigned long argp = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &argp) == 0;
#elif defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MAC)
    return fcntl(sock, F_SETFL,
                 nonblock ?
                 fcntl(sock, F_GETFL, 0) | O_NONBLOCK :
                 fcntl(sock, F_GETFL, 0) & (int) ~O_NONBLOCK) != -1;
#else
#   error "Unsupported platform"
#endif /*platform-specific ioctl*/
}


#ifdef NCBI_OS_UNIX
/* Set close-on-exec flag
 */
static int/*bool*/ s_SetCloexec(int fd, int/*bool*/ cloexec)
{
    return fcntl(fd, F_SETFD,
                 cloexec ?
                 fcntl(fd, F_GETFD, 0) | FD_CLOEXEC :
                 fcntl(fd, F_GETFD, 0) & (int) ~FD_CLOEXEC) != -1;
}
#endif /*NCBI_OS_UNIX*/


static int/*bool*/ s_SetReuseAddress(TSOCK_Handle x_sock, int/*bool*/ on_off)
{
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
#  ifdef NCBI_OS_MSWIN
    BOOL reuse_addr = on_off ? TRUE : FALSE;
#  else
    int  reuse_addr = on_off ? 1 : 0;
#  endif /*NCBI_OS_MSWIN*/
    return !setsockopt(x_sock, SOL_SOCKET, SO_REUSEADDR, 
                       (char*) &reuse_addr, sizeof(reuse_addr));
#else
    return 1;
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
}


static EIO_Status s_Status(SOCK sock, EIO_Event direction)
{
    assert(sock  &&  sock->sock != SOCK_INVALID);
    switch ( direction ) {
    case eIO_Read:
        return sock->stype != eSOCK_Datagram  &&  sock->eof
            ? eIO_Closed : (EIO_Status) sock->r_status;
    case eIO_Write:
        return (EIO_Status) sock->w_status;
    default:
        /*should never get here*/
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


/* compare 2 normalized timeval timeouts: "whether v1 is less than v2" */
static int/*bool*/ s_Less(const struct timeval* v1, const struct timeval* v2)
{
    if (!v1)
        return 0;
    if (!v2)
        return !!v1;
    if (v1->tv_sec > v2->tv_sec)
        return 0;
    if (v1->tv_sec < v2->tv_sec)
        return 1;
    return v1->tv_usec < v2->tv_usec;
}


/* Select on the socket I/O (multiple sockets).
 * "Event" field is not considered for entries, whose "sock" field is 0,
 * "revent" for those entries is always "eIO_Open". For all other entries
 * only those sockets will be considered, whose "revent" field does not
 * contain "eIO_Open" value. If at least one non-"eIO_Open" status found
 * in "revent", the call terminates with "eIO_Success" status (after,
 * however checking all other entries for validity). No additional checks
 * are made for the pre-ready entries. 
 *
 * This function does not check datagram sockets with the select() system call
 * at all if the number of requested sockets is more than 1 (cf. SOCK_Poll()).
 *
 * If "eIO_Write" event is inquired on a stream socket, and the socket is
 * marked for upread, then returned "revent" may also include "eIO_Read" to
 * indicate that some input is available on that socket.
 * If "eIO_Read" event is inquired on an array (n != 1) including stream
 * socket(s) and some sockets still have connection/data pending, those
 * "revent" field may then include "eIO_Write" to indicate that
 * connection can be completed/data sent.
 *
 * Return eIO_Success when at least one socket is found either ready 
 * (including "eIO_Read" event on "eIO_Write" for upreadable sockets
 * and "eIO_Write" on "eIO_Read" for sockets in pending state)
 * or failing ("revent" contains "eIO_Close").
 * Return "eIO_Timeout", if timeout expired before any socket became available.
 * Any other return code indicates some failure.
 */
static EIO_Status s_Select(size_t                n,
                           SSOCK_Poll            polls[],
                           const struct timeval* tv)
{
    int/*bool*/    write_only = 1;
    int/*bool*/    read_only = 1;
    int/*bool*/    ready = 0;
    int/*bool*/    bad = 0;
    fd_set         r_fds, w_fds, e_fds;
    int            n_fds;
    struct timeval x_tv;
    size_t         i;

    if ( tv )
        x_tv = *tv;

    for (;;) { /* (optionally) auto-resume if interrupted by a signal */
        struct timeval xx_tv;
        int x_errno;

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
                TSOCK_Handle fd = polls[i].sock->sock;
#if !defined(NCBI_OS_MSWIN) && defined(FD_SETSIZE)
                if (fd >= FD_SETSIZE) {
                    polls[i].revent = eIO_Close;
                    ready = 1;
                    continue;
                }
#endif /*!NCBI_MSWIN && FD_SETSIZE*/
                if (fd != SOCK_INVALID) {
                    TType type = polls[i].sock->type;
                    if (type == eTrigger  &&  polls[i].sock->host) {
                        polls[i].revent = polls[i].event;
                        ready = 1;
                        continue;
                    }
                    if (type != eListening  &&  n != 1  &&
                        /*FIXME*/
                        polls[i].sock->stype == eSOCK_Datagram)
                        continue;
                    if (ready  ||  bad)
                        continue;
                    switch (polls[i].event) {
                    case eIO_Write:
                    case eIO_ReadWrite:
                        if (type != eListening) {
                            if (polls[i].sock->stype == eSOCK_Datagram  ||
                                polls[i].sock->w_status != eIO_Closed) {
                                read_only = 0;
                                FD_SET(fd, &w_fds);
                                if (polls[i].sock->stype == eSOCK_Datagram  ||
                                    polls[i].sock->pending)
                                    break;
                                if (polls[i].event == eIO_Write  &&
                                    (polls[i].sock->r_on_w == eOff
                                     ||  (polls[i].sock->r_on_w == eDefault
                                          &&  s_ReadOnWrite != eOn)))
                                    break;
                            } else if (polls[i].event == eIO_Write)
                                break;
                        } else if (polls[i].event == eIO_Write)
                            break;
                        /*FALLTHRU*/
                    case eIO_Read:
                        if (polls[i].sock->stype != eSOCK_Datagram
                            &&  (polls[i].sock->r_status == eIO_Closed  ||
                                 polls[i].sock->eof))
                            break;
                        write_only = 0;
                        FD_SET(fd, &r_fds);
                        if (polls[i].sock->stype == eSOCK_Datagram  ||
                            polls[i].event != eIO_Read             ||
                            polls[i].sock->w_status == eIO_Closed  ||
                            n == 1  ||  (!polls[i].sock->pending  &&
                                         !polls[i].sock->w_len))
                            break;
                        read_only = 0;
                        FD_SET(fd, &w_fds);
                        break;
                    default:
                        /* should never get here */
                        assert(0);
                        break;
                    }
                    FD_SET(fd, &e_fds);
                    if (n_fds < (int) fd)
                        n_fds = (int) fd;
#ifdef NCBI_OS_MSWIN
                    /* Check whether FD_SETSIZE has been overcome */
                    if (!FD_ISSET(fd, &e_fds)) {
                        polls[i].revent = eIO_Close;
                        ready = 1;
                        continue;
                    }
#endif /*NCBI_OS_MSWIN*/
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

        if (!tv  ||  s_Less(s_SelectTimeout, &x_tv)) {
            if ( s_SelectTimeout ) {
                xx_tv = *s_SelectTimeout;
            }
        } else
            xx_tv = x_tv;

        n_fds = select(SOCK_NFDS((TSOCK_Handle) n_fds),
                       write_only ? 0 : &r_fds, read_only ? 0 : &w_fds,
                       &e_fds, tv || s_SelectTimeout ? &xx_tv : 0);

        /* timeout has expired */
        if (n_fds == 0) {
            if ( !tv )
                continue;
            if ( s_Less(s_SelectTimeout, &x_tv) ) {
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

        if (n_fds > 0)
            break;

        /* n_fds < 0 */
        if ((x_errno = SOCK_ERRNO) != SOCK_EINTR) {
            char _id[32];
            CORE_LOGF_ERRNO_EXX(5, eLOG_Trace,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[SOCK::s_Select]  Failed select()",
                                 n == 1 ? s_ID(polls[0].sock, _id) : ""));
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
            TSOCK_Handle fd = polls[i].sock->sock;
            assert(polls[i].revent == eIO_Open);
            if (fd != SOCK_INVALID) {
                if (!write_only  &&  FD_ISSET(fd, &r_fds))
                    polls[i].revent = eIO_Read;
                if (!read_only   &&  FD_ISSET(fd, &w_fds))
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
                if (!polls[i].revent  &&  FD_ISSET(fd, &e_fds))
                    polls[i].revent = eIO_Close;
            } else
                polls[i].revent = eIO_Close;
            if (polls[i].revent != eIO_Open)
                n_fds++;
        }
    }
    assert(n_fds != 0);

    /* success; can do I/O now */
    return eIO_Success;
}


/* connect() could be async/interrupted by a signal or just cannot be
 * established immediately;  yet, it must have been in progress
 * (asynchronous), so wait here for it to succeed (become writable).
 */
static EIO_Status s_IsConnected(SOCK                  sock,
                                const struct timeval* tv,
                                int*                  x_errno,
                                int/*bool*/           writeable)
{
    EIO_Status     status;
#if defined(NCBI_OS_UNIX) || defined(NCBI_OS_MSWIN)
    SOCK_socklen_t x_len = (SOCK_socklen_t) sizeof(*x_errno);
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
    SSOCK_Poll     poll;

    *x_errno = 0;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;
    if ( !writeable ) {
        poll.sock   = sock;
        poll.event  = eIO_Write;
        poll.revent = eIO_Open;
        status      = s_Select(1, &poll, tv);
        if (status == eIO_Timeout)
            return status;
    } else {
        status      = eIO_Success;
        poll.revent = eIO_Write;
    }
#if defined(NCBI_OS_UNIX) || defined(NCBI_OS_MSWIN)
    if (status == eIO_Success  &&
        (getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, (void*) x_errno, &x_len)
         ||  *x_errno != 0)) {
        status = eIO_Unknown;
    }
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    if (status == eIO_Success) {
#  ifdef NCBI_OS_UNIX
        if (!sock->path[0])
#  endif /*NCBI_OS_UNIX*/
            {
                struct sockaddr_in addr;
                SOCK_socklen_t addrlen = (SOCK_socklen_t) sizeof(addr);
                memset(&addr, 0, sizeof(addr));
#  ifdef HAVE_SIN_LEN
                addr.sin_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
                if (getsockname(sock->sock,
                                (struct sockaddr*) &addr, &addrlen) == 0) {
                    assert(addr.sin_family == AF_INET);
                    sock->myport = ntohs(addr.sin_port);
                }
            }
    }
#endif /*_DEBUG && !_NDEBUG*/
    if (status != eIO_Success  ||  poll.revent != eIO_Write) {
        if ( !*x_errno )
            *x_errno = SOCK_ERRNO;
        if (*x_errno == SOCK_ECONNREFUSED  ||  *x_errno == SOCK_ETIMEDOUT)
            sock->r_status = sock->w_status = status = eIO_Closed;
        else if (status == eIO_Success)
            status = eIO_Unknown;
    } else if (s_ReuseAddress  &&
#ifdef NCBI_OS_UNIX
               !sock->path[0]  &&
#endif /*NCBI_OS_UNIX*/
               !s_SetReuseAddress(sock->sock, 1/*true*/)) {
        int x_errno = SOCK_ERRNO;
        char _id[32];
        CORE_LOGF_ERRNO_EXX(6, eLOG_Note,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[SOCK::s_IsConnected]  Failed "
                             "setsockopt(REUSEADDR)", s_ID(sock, _id)));
    }
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    else {
        int  mtu;
        char _id[32];
#  ifdef IP_MTU
        SOCK_socklen_t mtulen = (SOCK_socklen_t) sizeof(mtu);
        if (getsockopt(sock->sock, SOL_IP, IP_MTU, &mtu, &mtulen) != 0)
#  endif
            mtu = -1;
        if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn)) {
            CORE_TRACEF(("%sConnection established, MTU = %d",
                         s_ID(sock, _id), mtu));
        }
    }
#endif /*_DEBUG && !NDEBUG*/
    return status;
}


/* To allow emulating "peek" using the NCBI data buffering.
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves.)
 * NOTE: This call is for stream sockets only.
 */
static int s_Recv(SOCK        sock,
                  void*       buffer,
                  size_t      size,
                  int/*bool*/ peek)
{
    size_t n_read;

    assert(sock->stype != eSOCK_Datagram  &&  !sock->pending);
    if ( !size ) {
        /* internal upread use only */
        assert(sock->r_status != eIO_Closed && !sock->eof && peek && !buffer);
        n_read = 0;
    } else {
        /* read (or peek) from the internal buffer */
        n_read = peek
            ? BUF_Peek(sock->r_buf, buffer, size)
            : BUF_Read(sock->r_buf, buffer, size);
        if ((n_read  &&  (n_read == size  ||  !peek))  ||
            sock->r_status == eIO_Closed  ||  sock->eof) {
            return (int) n_read;
        }
    }

    /* read from the socket */
    do {
        char   xx_buffer[SOCK_BUF_CHUNK_SIZE];
        char*  x_buffer;
        int    x_errno;
        size_t n_todo;
        int    x_read;

        if ( !size ) {
            /* internal upread call -- read out as much as possible */
            n_todo       = sizeof(xx_buffer);
            x_buffer     = xx_buffer;
        } else {
            if (!buffer  ||
                (n_todo  = size - n_read) < sizeof(xx_buffer)) {
                n_todo   = sizeof(xx_buffer);
                x_buffer = xx_buffer;
            } else
                x_buffer = (char*) buffer + n_read;
        }

        /* recv */
        x_read = recv(sock->sock, x_buffer, n_todo, 0);

        /* success */
        if (x_read >= 0  ||
            (x_read < 0  &&  ((x_errno = SOCK_ERRNO) == SOCK_ENOTCONN      ||
                              x_errno                == SOCK_ETIMEDOUT     ||
                              x_errno                == SOCK_ECONNRESET    ||
                              x_errno                == SOCK_ECONNABORTED  ||
                              x_errno                == SOCK_ENETRESET))) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Read, (x_read < 0 ? (void*) &x_errno :
                                         x_read > 0 ? x_buffer : 0),
                        (size_t)(x_read < 0 ? 0 : x_read), 0);
            }
            if (x_read <= 0) {
                /* catch EOF/failure */
                sock->eof = 1/*true*/;
                if (x_read)
                    sock->r_status = sock->w_status = eIO_Closed;
                else
                    sock->r_status = eIO_Success;
                break;
            }
        } else {
            /* some error */
            if ((x_errno = SOCK_ERRNO) != SOCK_EWOULDBLOCK  &&
                x_errno                != SOCK_EAGAIN       &&
                x_errno                != SOCK_EINTR) {
                /* catch unknown ERROR */
                sock->r_status = eIO_Unknown;
                CORE_LOGF_ERRNO_EXX(7, eLOG_Trace,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_Recv]  Failed recv()",
                                     s_ID(sock, xx_buffer)));
            }
            return n_read ? (int) n_read : -1;
        }
        assert(x_read > 0);
        sock->n_read  += x_read;
        sock->r_status = eIO_Success;

        n_todo = size - n_read;
        if (buffer  &&  (void*) x_buffer != buffer) {
            if (n_todo > (size_t) x_read)
                n_todo = (size_t) x_read;
            memcpy((char*) buffer + n_read, x_buffer, n_todo);
        }
        if (peek  ||  (size_t) x_read > n_todo) {
            /* store the newly read data in the internal input buffer */
            int/*bool*/ error = !BUF_Write(&sock->r_buf, peek
                                           ? x_buffer
                                           : x_buffer + n_todo, peek
                                           ? (size_t) x_read
                                           : (size_t)(x_read - n_todo));
            if ((size_t) x_read > n_todo)
                x_read = n_todo;
            if (error) {
                CORE_LOGF_ERRNO_X(8, eLOG_Error, errno,
                                  ("%s[SOCK::s_Recv]  Cannot store data in "
                                   "peek buffer", s_ID(sock, xx_buffer)));
                sock->eof      = 1/*failure*/;
                sock->r_status = eIO_Closed;
                if ( peek )
                    return n_read ? (int) n_read : -1;
                n_read += x_read;
                break;
            }
        }
        n_read += x_read;
    } while (!size  ||  (!buffer  &&  n_read < size));

    return (int) n_read;
}


static EIO_Status s_WritePending(SOCK, const struct timeval*, int, int);

/* s_Select() with stall protection: try pull incoming data from sockets.
 * This method returns array of polls, "revent"s of which are always
 * compatible with requested "event"s. That is, it always strips additional
 * events that s_Select() may have set to indicate additional I/O events
 * some sockets are ready for. Return eIO_Timeout if no compatible events
 * were found (all sockets are not ready for inquired respective I/O) within
 * the specified timeout (and no other socket error was flagged).
 * Return eIO_Success if at least one socket is ready. Return the number
 * of sockets that are ready via pointer argument "n_ready" (may be NULL).
 * Return other error code to indicate error condition.
 */
static EIO_Status s_SelectStallsafe(size_t                n,
                                    SSOCK_Poll            polls[],
                                    const struct timeval* tv,
                                    size_t*               n_ready)
{
    int/*bool*/ pending;
    EIO_Status  status;
    size_t      i, j;

    if ((status = s_Select(n, polls, tv)) != eIO_Success) {
        if ( n_ready )
            *n_ready = 0;
        return status;
    }

    j = 0;
    pending = 0;
    for (i = 0; i < n; i++) {
        if (polls[i].revent == eIO_Close)
            break;
        if (polls[i].revent & polls[i].event)
            break;
        if (polls[i].revent != eIO_Open  &&  !pending) {
            pending = 1;
            j = i;
        }
    }
    if (i >= n  &&  pending) {
        /* all sockets are not ready for the requested events */
        for (i = j; i < n; i++) {
            /* try to push pending writes */
            if (polls[i].event == eIO_Read  &&  polls[i].revent == eIO_Write) {
                assert(n != 1);
                assert(polls[i].sock->pending  ||  polls[i].sock->w_len);
                status = s_WritePending(polls[i].sock, tv, 1/*writeable*/, 0);
                if (status != eIO_Success  &&  status != eIO_Timeout) {
                    polls[i].revent = eIO_Close;
                    break;
                }
                continue;
            }
            /* try to find an immediately readable socket */
            if (polls[i].event != eIO_Write)
                continue;
            while (polls[i].revent == eIO_Read) {
                assert(polls[i].sock                           &&
                       polls[i].sock->sock != SOCK_INVALID     &&
                       polls[i].sock->stype != eSOCK_Datagram  &&
                       polls[i].sock->w_status != eIO_Closed   &&
                       polls[i].sock->r_status != eIO_Closed   &&
                       !polls[i].sock->eof                     &&
                       !polls[i].sock->pending                 &&
                       (polls[i].sock->r_on_w == eOn
                        ||  (polls[i].sock->r_on_w == eDefault
                             &&  s_ReadOnWrite == eOn)));

                /* try upread as much as possible data into internal buffer */
                s_Recv(polls[i].sock, 0, 0/*infinite*/, 1/*peek*/);

                /* then poll if writeable */
                polls[i].revent = eIO_Open;
                if ((status = s_Select(1, &polls[i], tv)) != eIO_Success)
                    break;
            }
            if (status != eIO_Success  ||  polls[i].revent == eIO_Write)
                break; /*error or can write now!*/
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

    return j ? eIO_Success : (status == eIO_Success ? eIO_Timeout : status);
}


static EIO_Status s_WipeRBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->r_buf);

    if (size  &&  BUF_Read(sock->r_buf, 0, size) != size) {
        char _id[32];
        CORE_LOGF_X(9, eLOG_Error,
                    ("%s[SOCK::s_WipeRBuf]  Cannot drop aux. data buf",
                     s_ID(sock, _id)));
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    return status;
}


/* For datagram sockets only! */
static EIO_Status s_WipeWBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->w_buf);

    assert(sock->stype == eSOCK_Datagram);
    if (size  &&  BUF_Read(sock->w_buf, 0, size) != size) {
        char _id[32];
        CORE_LOGF_X(10, eLOG_Error,
                    ("%s[SOCK::s_WipeWBuf]  Cannot drop aux. data buf",
                     s_ID(sock, _id)));
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    sock->eof = 0;
    return status;
}


#ifdef NCBI_OS_MSWIN
static void s_Add(struct timeval* tv, int ms_addend)
{
    tv->tv_usec += (ms_addend % 1000) * 1000;
    tv->tv_sec  +=  ms_addend / 1000;
    if (tv->tv_usec >= 10000000) {
        tv->tv_sec  += tv->tv_usec / 10000000;
        tv->tv_usec %= 10000000;
    }
}
#endif /*NCBI_OS_MSWIN*/


/* Write data to the socket "as is" (as many bytes at once as possible).
 * Return eIO_Success if at least some bytes were written successfully.
 * Otherwise (no bytes written) return an error code to indicate the problem.
 * NOTE: This call is for stream sockets only.
 */
static EIO_Status s_Send(SOCK        sock,
                         const void* buf,
                         size_t      size,
                         size_t*     n_written,
                         int/*bool*/ oob)
{
    char _id[32];
#ifdef NCBI_OS_MSWIN
    int no_buffer_wait = 0;
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
#endif /*NCBI_OS_MSWIN*/

    assert(size > 0  &&  sock->stype != eSOCK_Datagram  &&  *n_written == 0);
    for (;;) { /* optionally retry if interrupted by a signal */
        /* try to write */
        int x_written = send(sock->sock, (void*) buf, size, oob ? MSG_OOB : 0);
        int x_errno;

        if (x_written > 0) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(sock, eIO_Write, buf, (size_t) x_written, oob? "" : 0);
            sock->n_written += x_written;
            *n_written = x_written;
            sock->w_status = eIO_Success;
            break/*done*/;
        }
        x_errno = SOCK_ERRNO;
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = eIO_Unknown;
        if (oob)
            break;

        /* blocked -- retry if unblocked before the timeout expires */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN
#ifdef NCBI_OS_MSWIN
            ||  x_errno == WSAENOBUFS
#endif /*NCBI_OS_MSWIN*/
            ) {
            EIO_Status status;
            SSOCK_Poll poll;

#ifdef NCBI_OS_MSWIN
            if (x_errno == WSAENOBUFS) {
                s_Add(&timeout, no_buffer_wait);
                if (!s_Less(&timeout, sock->w_timeout))
                    return eIO_Timeout;
                if (no_buffer_wait)
                    Sleep(no_buffer_wait);
                if (no_buffer_wait == 0)
                    no_buffer_wait = 10;
                else if (no_buffer_wait < 160)
                    no_buffer_wait *= 2;
            } else {
                no_buffer_wait = 0;
                memset(&timeout, 0, sizeof(timeout));
            }
#endif /*NCBI_OS_MSWIN*/
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
            if (x_errno != SOCK_EPIPE      &&  x_errno != SOCK_ENOTCONN     &&
                x_errno != SOCK_ECONNRESET &&  x_errno != SOCK_ECONNABORTED &&
                x_errno != SOCK_ENETRESET  &&  x_errno != SOCK_ETIMEDOUT) {
                CORE_LOGF_ERRNO_EXX(11, eLOG_Trace,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_Send]  Failed send()",
                                     s_ID(sock, _id)));
                break;
            }
            sock->w_status = eIO_Closed;
            if (x_errno != SOCK_EPIPE)
                sock->r_status = eIO_Closed;
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(sock, eIO_Write, &x_errno, 0, 0);
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = eIO_Interrupt;
            break;
        }
    }

    return (EIO_Status) sock->w_status;
}


/* Wrapper for s_Send() that slices the output buffer for some brain-dead
 * systems (e.g. old Macs) that cannot handle large data chunks in "send()".
 * Return eIO_Success if some data were successfully sent; other
 * error code if no data were sent at all.
 */
#ifdef SOCK_WRITE_SLICE
static EIO_Status s_WriteSliced(SOCK        sock,
                                const void* buf,
                                size_t      size,
                                size_t*     n_written,
                                int/*bool*/ oob)
{
    /* split output buffer by slices (of size <= SOCK_WRITE_SLICE)
     * before writing to the socket
     */
    EIO_Status status;

    assert(size > 0  &&  *n_written == 0);
    do {
        size_t n_io = size > SOCK_WRITE_SLICE ? SOCK_WRITE_SLICE : size;
        size_t n_io_done = 0;
        status = s_Send(sock, (char*) buf + *n_written, n_io, &n_io_done, oob);
        if (status != eIO_Success)
            break;
        *n_written += n_io_done;
        if (n_io != n_io_done)
            break;
        size       -= n_io_done;
    } while ( size );

    return status;
}
#else
#  define s_WriteSliced s_Send
#endif /*SOCK_WRITE_SLICE*/


static EIO_Status s_WritePending(SOCK                  sock,
                                 const struct timeval* tv,
                                 int/*bool*/           writeable,
                                 int/*bool*/           oob)
{
    const struct timeval* x_tv;
    EIO_Status status;
    size_t off;

    assert(sock->stype != eSOCK_Datagram  &&  sock->sock != SOCK_INVALID);
    if ( sock->pending ) {
        int x_errno;
        status = s_IsConnected(sock, tv, &x_errno, writeable);
        if (status != eIO_Success) {
            if (status != eIO_Timeout) {
                char addr[80];
                char  _id[32];
#ifdef NCBI_OS_UNIX
                if ( sock->path[0] ) {
                    size_t      len = strlen(sock->path);
                    int/*bool*/ trunc = len > sizeof(addr) - 3 ? 1 : 0;
                    sprintf(addr, "\"%s%.*s\"", trunc ? "..." : "", 
                            (int)(trunc ? sizeof(addr) - 6 : len),
                            &sock->path[trunc ? len - sizeof(addr) + 6 : 0]);
                } else
#endif /*NCBI_OS_UNIX*/
                    SOCK_HostPortToString(sock->host, ntohs(sock->port),
                                          addr, sizeof(addr));
                CORE_LOGF_ERRNO_EXX(12, eLOG_Error,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_WritePending]  Failed pending"
                                     " connect() to %s",s_ID(sock, _id),addr));
                sock->w_status = status;
            }
            return status;
        }
        sock->pending = 0/*connected*/;
    }
    if (oob  ||  sock->w_len == 0  ||  sock->w_status == eIO_Closed)
        return eIO_Success;

    x_tv = sock->w_timeout;
    sock->w_timeout = tv;
    off = BUF_Size(sock->w_buf) - sock->w_len;
    do {
        char   buf[SOCK_BUF_CHUNK_SIZE];
        size_t n_written = 0;
        size_t n_write = BUF_PeekAt(sock->w_buf, off, buf, sizeof(buf));
        status = s_WriteSliced(sock, buf, n_write, &n_written, 0);
        if (status != eIO_Success)
            break;
        sock->w_len -= n_written;
        off         += n_written;
    } while ( sock->w_len );
    sock->w_timeout = x_tv;

    assert((sock->w_len != 0)  ==  (status != eIO_Success));
    return status;
}


/* Read/Peek data from the socket. Always return eIO_Success if some data
 * were read (regardless of socket conditions that may include EOF/error).
 * Return other (error) code only if no data at all could be obtained.
 */
static EIO_Status s_Read(SOCK        sock,
                         void*       buf,
                         size_t      size,
                         size_t*     n_read,
                         int/*bool*/ peek)
{
    EIO_Status status;

    *n_read = 0;

    if (sock->stype == eSOCK_Datagram) {
        *n_read = peek
            ? BUF_Peek(sock->r_buf, buf, size)
            : BUF_Read(sock->r_buf, buf, size);
        sock->r_status = *n_read || !size ? eIO_Success : eIO_Closed;
        return (EIO_Status) sock->r_status;
    }

    status = s_WritePending(sock, sock->r_timeout, 0, 0);
    if (sock->pending  ||  !size)
        return sock->pending ? status : s_Status(sock, eIO_Read);

    for (;;) { /* retry if either blocked or interrupted (optional) */
        /* try to read */
        int x_read = s_Recv(sock, buf, size, peek);
        int x_errno;

        if (x_read > 0) {
            assert((size_t) x_read <= size);
            *n_read = x_read;
            return eIO_Success;
        }

        if (sock->r_status == eIO_Unknown)
            break;

        if (sock->r_status == eIO_Closed  ||  sock->eof) {
            if ( !sock->eof ) {
                CORE_DEBUG_ARG(char _id[32]);
                CORE_TRACEF(("%s[SOCK::s_Read]  Socket has already "
                             "been shut down for reading",
                             s_ID(sock, _id)));
            }
            return eIO_Closed;
        }

        x_errno = SOCK_ERRNO;
        /* blocked -- wait for data to come;  exit if timeout/error */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            const struct timeval* tv = sock->r_timeout;
            SSOCK_Poll poll;

            if (tv  &&  !(tv->tv_sec | tv->tv_usec))
                return eIO_Timeout;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            if ((status = s_Select(1, &poll, tv)) != eIO_Success)
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
            sock->r_status = eIO_Interrupt;
            break;
        }
    }

    /* don't want to handle all possible errors... let them be "unknown" */
    return eIO_Unknown;
}


/* Write to the socket. Return eIO_Success if some data were written.
 * Return other (error) code only if no data at all could be written.
 */
static EIO_Status s_Write(SOCK        sock,
                          const void* buf,
                          size_t      size,
                          size_t*     n_written,
                          int/*bool*/ oob)
{
    EIO_Status status;

    *n_written = 0;

    if (sock->stype == eSOCK_Datagram) {
        if ( sock->eof )
            s_WipeWBuf(sock);
        if ( BUF_Write(&sock->w_buf, buf, size) ) {
            sock->w_status = eIO_Success;
            *n_written = size;
        } else
            sock->w_status = eIO_Unknown;
        return (EIO_Status) sock->w_status;
    }

    if (sock->w_status == eIO_Closed) {
        if (size != 0) {
            CORE_DEBUG_ARG(char _id[32]);
            CORE_TRACEF(("%s[SOCK::s_Write]  Socket has already "
                         "been shut down for writing",
                         s_ID(sock, _id)));
        }
        return eIO_Closed;
    }

    status = s_WritePending(sock, sock->w_timeout, 0, oob);
    if (status != eIO_Success) {
        if (status == eIO_Timeout  ||  status == eIO_Closed)
            return status;
        return size ? status : eIO_Success;
    }

    assert(sock->w_len == 0);
    return size ? s_WriteSliced(sock, buf, size, n_written, oob) : eIO_Success;
}


/* For non-datagram sockets only */
static EIO_Status s_Shutdown(SOCK                  sock,
                             EIO_Event             how,
                             const struct timeval* tv)
{
    EIO_Status status;
    char      _id[32];
    int        x_how;

    assert(sock->stype != eSOCK_Datagram);
    switch ( how ) {
    case eIO_Read:
        if ( sock->eof ) {
            /* hit EOF (and may be not yet shut down) -- so, flag it as been
             * shut down, but do not call the actual system shutdown(),
             * as it can cause smart OS'es like Linux to complain
             */
            sock->eof = 0/*false*/;
            sock->r_status = eIO_Closed;
        }
        if (sock->r_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        x_how = SOCK_SHUTDOWN_RD;
        sock->r_status = eIO_Closed;
#ifdef NCBI_OS_MSWIN
        /* see comments at the end of eIO_Write case */
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
        break;
    case eIO_Write:
        if (sock->w_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        if ((status = s_WritePending(sock, tv, 0, 0)) != eIO_Success) {
            CORE_LOGF_X(13, !tv  ||  (tv->tv_sec | tv->tv_usec)
                        ? eLOG_Warning : eLOG_Trace,
                        ("%s[SOCK::s_Shutdown]  Shutting down for "
                         "write with some output pending (%s)",
                         s_ID(sock, _id), IO_StatusStr(status)));
        }
        x_how = SOCK_SHUTDOWN_WR;
        sock->w_status = eIO_Closed;
#ifdef NCBI_OS_MSWIN
        /*  on MS-Win, socket shutdown on writing apparently messes up (?!)  *
         *  with the later reading, esp. when reading a lot of data...       */
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
        break;
    case eIO_ReadWrite:
#ifdef NCBI_OS_MSWIN
        if (sock->r_status == eIO_Closed  &&  sock->w_status == eIO_Closed)
            return eIO_Success;
        if (sock->w_status != eIO_Closed
            &&  (status = s_WritePending(sock, tv, 0, 0)) != eIO_Success) {
            CORE_LOGF_X(14, !tv  ||  (tv->tv_sec | tv->tv_usec)
                        ? eLOG_Warning : eLOG_Trace,
                        ("%s[SOCK::s_Shutdown]  Shutting down for "
                         "R/W with some output pending (%s)",
                         s_ID(sock, _id), IO_StatusStr(status)));
        }
        x_how = SOCK_SHUTDOWN_RDWR;
        sock->eof = 0/*false*/;
        sock->r_status = sock->w_status = eIO_Closed;
        break;
#else
        {{
            EIO_Status sw = s_Shutdown(sock, eIO_Write, tv);
            EIO_Status sr = s_Shutdown(sock, eIO_Read,  tv);
            if (sw != eIO_Success  ||  sr != eIO_Success)
                return (sw == eIO_Success ? sr :
                        sr == eIO_Success ? sw : eIO_Unknown);
        }}
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
    default:
        CORE_LOGF_X(15, eLOG_Error,
                    ("%s[SOCK::s_Shutdown]  Invalid direction %u",
                     s_ID(sock, _id), (unsigned int) how));
        return eIO_InvalidArg;
    }

    if (s_Initialized  &&  SOCK_SHUTDOWN(sock->sock, x_how) != 0) {
        int x_errno = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
        if (x_errno == WSANOTINITIALISED)
            s_Initialized = 0/*false*/;
        else
#endif /*NCBI_OS_MSWIN*/
        if (
#if   defined(NCBI_OS_LINUX)/*bug in the Linux kernel to report*/  || \
      defined(NCBI_OS_IRIX)                                        || \
      defined(NCBI_OS_OSF1)
            x_errno != SOCK_ENOTCONN
#else
            x_errno != SOCK_ENOTCONN  ||  sock->pending
#endif /*UNIX flavors*/
            )
            CORE_LOGF_ERRNO_EXX(16, eLOG_Warning,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[SOCK::s_Shutdown]  Failed shutdown(%s)",
                                 s_ID(sock, _id), how == eIO_Read ? "READ" :
                                 how == eIO_Write ? "WRITE" : "READ/WRITE"));
    }
    return eIO_Success;
}


/* Close the socket (orderly or abrupt)
 */
static EIO_Status s_Close(SOCK sock, int/*bool*/ abort)
{
    EIO_Status status;
    char      _id[32];

    /* reset the auxiliary data buffers */
    s_WipeRBuf(sock);
    if (sock->stype == eSOCK_Datagram) {
        s_WipeWBuf(sock);
    } else if (abort  ||  sock->stype != eSOCK_ServerSideKeep) {
        /* set the close()'s linger period be equal to the close timeout */
#if (defined(NCBI_OS_UNIX) && !defined(NCBI_OS_BEOS)) || defined(NCBI_OS_MSWIN)
        /* setsockopt() is not implemented for MAC (MIT socket emulation lib)*/
        if (sock->w_status != eIO_Closed) {
            const struct timeval* tv = sock->c_timeout;
            struct linger lgr;

            lgr.l_onoff = 0;
            if (abort) {
                lgr.l_linger = 0;   /* RFC 793, Abort */
                lgr.l_onoff  = 1;
            } else if (!tv) {
                lgr.l_linger = 120; /*this is standard TCP TTL, 2 minutes*/
                lgr.l_onoff  = 1;
            } else if (tv->tv_sec | tv->tv_usec) {
                unsigned int tmo = tv->tv_sec + (tv->tv_usec + 500000)/1000000;
                if (tmo) {
                    lgr.l_linger = tmo;
                    lgr.l_onoff  = 1;
                }
            }
            if (lgr.l_onoff
                &&  setsockopt(sock->sock, SOL_SOCKET, SO_LINGER,
                               (char*) &lgr, sizeof(lgr)) != 0) {
                int x_errno = SOCK_ERRNO;
                CORE_LOGF_ERRNO_EXX(17, eLOG_Trace,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_%s]  Failed setsockopt"
                                     "(SO_LINGER)", s_ID(sock, _id),
                                     abort ? "Abort" : "Close"));
            }
#  ifdef TCP_LINGER2
            if (abort  ||  (tv  &&  !(tv->tv_sec | tv->tv_usec))) {
                int no = -1;
                if (setsockopt(sock->sock, IPPROTO_TCP, TCP_LINGER2,
                               (char*) &no, sizeof(no)) != 0  &&  !abort) {
                    int x_errno = SOCK_ERRNO;
                    CORE_LOGF_ERRNO_EXX(18, eLOG_Trace,
                                        x_errno, SOCK_STRERROR(x_errno),
                                        ("%s[SOCK::s_%s]  Failed setsockopt"
                                         "(TCP_LINGER2)", s_ID(sock, _id),
                                         abort ? "Abort" : "Close"));
                }
            }
#  endif /*TCP_LINGER2*/
        }
#endif /*(NCBI_OS_UNIX && !NCBI_OS_BEOS) || NCBI_OS_MSWIN*/

        if (!abort) {
            /* shutdown in both directions */
            s_Shutdown(sock, eIO_ReadWrite, sock->c_timeout);
        } else
            sock->r_status = sock->w_status = eIO_Closed;
        /* set the socket back to blocking mode */
        if (s_Initialized && !s_SetNonblock(sock->sock, 0/*false*/) && !abort){
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EXX(19, eLOG_Trace,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[SOCK::s_Close]  Cannot set socket "
                                 "back to blocking mode", s_ID(sock, _id)));
        }
    } else {
        status = s_WritePending(sock, sock->c_timeout, 0, 0);
        if (status != eIO_Success) {
            CORE_LOGF_X(20, eLOG_Warning,
                        ("%s[SOCK::s_Close]  Leaving with some "
                         "output data pending (%s)",
                         s_ID(sock, _id), IO_StatusStr(status)));
        }
    }
    sock->w_len = 0;

    /* statistics & logging */
    if (sock->stype != eSOCK_Datagram) {
        sock->n_in  += sock->n_read;
        sock->n_out += sock->n_written;
    }
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Close, 0, 0, 0);

    status = eIO_Success;
    if (abort  ||  sock->stype != eSOCK_ServerSideKeep) {
        for (;;) { /* close persistently - retry if interrupted by a signal */
            int x_errno;
            if (SOCK_CLOSE(sock->sock) == 0)
                break;

            /* error */
            if (!s_Initialized)
                break;
            x_errno = SOCK_ERRNO;
#ifdef NCBI_OS_MSWIN
            if (x_errno == WSANOTINITIALISED) {
                s_Initialized = 0/*false*/;
                break;
            }
#endif /*NCBI_OS_MSWIN*/
            if (abort  ||  x_errno != SOCK_EINTR) {
                CORE_LOGF_ERRNO_EXX(21, abort > 1 ? eLOG_Error : eLOG_Warning,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_%s]  Failed close()",
                                     s_ID(sock, _id),
                                     abort ? "Abort" : "Close"));
                if (abort++ > 1  ||  x_errno != SOCK_EINTR) {
                    status = eIO_Unknown;
                    break;
                }
            }
        }
    }

    /* return */
    sock->sock = SOCK_INVALID;
    sock->myport = 0;
    return status;
}


/* Connect the (pre-allocated) socket to the specified "host:port"/"file" peer.
 * HINT: if "host" is NULL then assume(!) that the "sock" already exists,
 *       and connect to the same host;  the same is for zero "port".
 * NOTE: Client-side stream sockets only.
 */
static EIO_Status s_Connect(SOCK            sock,
                            const char*     host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    char               _id[32];
    int                x_errno;
    TSOCK_Handle       x_sock;
    union {
        struct sockaddr    sa;
        struct sockaddr_in sin;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un sun;
#endif /*NCBI_OS_UNIX*/
    } addr;
    SOCK_socklen_t     addrlen;
    int                n;
    const char*        c;
    char               s[80];

    assert(sock->stype == eSOCK_ClientSide);

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    memset(&addr, 0, sizeof(addr));
#ifdef NCBI_OS_UNIX
    if (sock->path[0]) {
        addrlen = (SOCK_socklen_t) sizeof(addr.sun);
        addr.sun.sun_family = AF_UNIX;
        strncpy0(addr.sun.sun_path, sock->path, sizeof(addr.sun.sun_path)-1);
        c = sock->path;
    } else
#endif /*NCBI_OS_UNIX*/
        {
            unsigned int   x_host;
            unsigned short x_port;
            addrlen = (SOCK_socklen_t) sizeof(addr.sin);
            addr.sin.sin_family = AF_INET;
            /* get address of the remote host (assume the same host if NULL) */
            x_host = host  &&  *host ? SOCK_gethostbyname(host) : sock->host;
            if ( !x_host ) {
                CORE_LOGF_X(22, eLOG_Error,
                            ("%s[SOCK::s_Connect]  Failed"
                             " SOCK_gethostbyname(\"%.64s\")",
                             s_ID(sock, _id), host));
                return eIO_Unknown;
            }
            addr.sin.sin_addr.s_addr = x_host;
            /* set the port to connect to (same port if "port" is zero) */
            x_port = (unsigned short) (port ? htons(port) : sock->port);
            addr.sin.sin_port = x_port;
#ifdef HAVE_SIN_LEN
            addr.sin.sin_len = addrlen;
#endif /*HAVE_SIN_LEN*/
            sock->host = x_host;
            sock->port = x_port;
            SOCK_HostPortToString(x_host, ntohs(x_port), s, sizeof(s));
            c = s;
        }

    /* check the new socket */
    if ((x_sock = socket(addr.sa.sa_family, SOCK_STREAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(23, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[SOCK::s_Connect]  Cannot create socket",
                             s_ID(sock, _id)));
        return eIO_Unknown;
    }
    sock->sock     = x_sock;
    sock->r_status = eIO_Success;
    sock->eof      = 0/*false*/;
    sock->w_status = eIO_Success;
    assert(sock->w_len == 0);

    /* set the socket I/O to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(24, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[SOCK::s_Connect]  Cannot set socket to"
                             " non-blocking mode", s_ID(sock, _id)));
        s_Close(sock, 1/*abort*/);
        return eIO_Unknown;
    }

    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, 0, 0, &addr.sa);

    /* establish connection to the peer */
    for (n = 0; ; n = 1) {
        if (connect(x_sock, &addr.sa, addrlen) == 0) {
            x_errno = 0;
            break;
        }
        x_errno = SOCK_ERRNO;
        if (x_errno != SOCK_EINTR  ||  sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal))
            break;
    }
    if (x_errno) {
        if ((n != 0  ||  x_errno != SOCK_EINPROGRESS)  &&
            (n == 0  ||  x_errno != SOCK_EALREADY)     &&
            x_errno != SOCK_EWOULDBLOCK) {
            if (x_errno != SOCK_EINTR) {
                CORE_LOGF_ERRNO_EXX(25, eLOG_Error,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_Connect]  Failed connect() "
                                     "to %s", s_ID(sock, _id), c));
            }
            s_Close(sock, 1/*abort*/);
            /* unrecoverable error */
            return x_errno == SOCK_EINTR ? eIO_Interrupt : eIO_Unknown;
        }

        if (!timeout  ||  (timeout->sec | timeout->usec)) {
            EIO_Status     status;
            struct timeval tv;

            status = s_IsConnected(sock, s_to2tv(timeout, &tv), &x_errno, 0);
            if (status != eIO_Success) {
                CORE_LOGF_ERRNO_EXX(26, eLOG_Error,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("%s[SOCK::s_Connect]  Failed pending "
                                     "connect() to %s (%s)", s_ID(sock, _id),
                                     c, IO_StatusStr(status)));
                s_Close(sock, 1/*abort*/);
                return status;
            }
            sock->pending = 0/*connected*/;
        } else
            sock->pending = 1/*not yet connected*/;
    } else
        sock->pending = 0/*connected*/;

    /* success: do not change any timeouts */
    sock->w_len = BUF_Size(sock->w_buf);
    return eIO_Success;
}


static EIO_Status s_Create(const char*     host,
                           unsigned short  port,
                           const STimeout* timeout,
                           SOCK*           sock,
                           const void*     data,
                           size_t          datalen,
                           ESwitch         log)
{
    unsigned int x_id = ++s_ID_Counter * 1000;
    size_t       x_n = port ? 0 : strlen(host);
    SOCK         x_sock;

    *sock = 0;
    /* allocate memory for the internal socket structure */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + x_n)))
        return eIO_Unknown;
    x_sock->sock  = SOCK_INVALID;
    x_sock->id    = x_id;
    x_sock->type  = eSocket;
    x_sock->log   = log;
    x_sock->stype = eSOCK_ClientSide;
#ifdef NCBI_OS_UNIX
    if (!port) {
        strcpy(x_sock->path, host);
    }
#endif /*NCBI_OS_UNIX*/

    /* setup the I/O data buffer properties */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    if (datalen) {
        if (!BUF_SetChunkSize(&x_sock->w_buf, datalen) ||
            !BUF_Write(&x_sock->w_buf, data, datalen)) {
            char _id[32];
            CORE_LOGF_ERRNO_X(27, eLOG_Error, errno,
                              ("%s[SOCK::CreateEx]  Cannot store initial data",
                               s_ID(x_sock, _id)));
            SOCK_Close(x_sock);
            return eIO_Unknown;
        }
    }

    /* connect */
    {{
        EIO_Status status;
        if ((status = s_Connect(x_sock, host, port, timeout)) != eIO_Success) {
            SOCK_Close(x_sock);
            return status;
        }
    }}

    /* success */
    x_sock->r_on_w   = eDefault;
    x_sock->i_on_sig = eDefault;
    *sock = x_sock;
    return eIO_Success;
}



/******************************************************************************
 *  UTILITY
 */

extern const STimeout* SOCK_SetSelectInternalRestartTimeout(const STimeout* t)
{
    static struct timeval s_NewTmo;
    static STimeout       s_OldTmo;
    const  STimeout*      retval;
    CORE_LOCK_WRITE;
    retval          = s_tv2to(s_SelectTimeout, &s_OldTmo);
    s_SelectTimeout = s_to2tv(t,               &s_NewTmo);
    CORE_UNLOCK;
    return retval;
}



/******************************************************************************
 *  TRIGGER
 */

extern EIO_Status TRIGGER_Create(TRIGGER* trigger, ESwitch log)
{
    unsigned int x_id = ++s_ID_Counter;

    *trigger = 0;
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

#ifdef NCBI_CXX_TOOLKIT

#  ifdef NCBI_OS_UNIX
    {{
        int fd[2];

        if (pipe(fd) < 0) {
            CORE_LOGF_ERRNO_X(28, eLOG_Error, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Cannot create pipe", x_id));
            return eIO_Closed;
        }

        if (!s_SetNonblock(fd[0], 1/*true*/)  ||
            !s_SetNonblock(fd[1], 1/*true*/)) {
            CORE_LOGF_ERRNO_X(29, eLOG_Warning, errno,
                              ("TRIGGER#%u[?]: [TRIGGER::Create] "
                               " Failed to set non-blocking mode", x_id));
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
        (*trigger)->fd   = fd[0];
        (*trigger)->id   = x_id;
        (*trigger)->type = eTrigger;
        (*trigger)->log  = log;
        (*trigger)->out  = fd[1];

        /* statistics & logging */
        if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
            CORE_TRACEF(("TRIGGER#%u[%u, %u]: Ready", x_id, fd[0], fd[1]));
        }
    }}

    return eIO_Success;

#  else

    CORE_LOGF_X(31, eLOG_Error, ("TRIGGER#%u[?]: [TRIGGER::Create] "
                                 " Not yet supported on this platform", x_id));
    return eIO_NotSupported;

#  endif /*NCBI_OS_UNIX*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Close(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

    /* statistics & logging */
    if (trigger->log == eOn  ||  (trigger->log == eDefault  &&  s_Log == eOn)){
        CORE_TRACEF(("TRIGGER#%u[%u]: Closing", trigger->id, trigger->fd));
    }

#  ifdef NCBI_OS_UNIX

    /* Prevent SIGPIPE by closing in this order:  writing end first */
    close(trigger->out);
    close(trigger->fd);

#  endif /*NCBI_OS_UNIX*/

    free(trigger);
    return eIO_Success;

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Set(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

#  ifdef NCBI_OS_UNIX

    void* one = (void*) 1;

    if (NCBI_SwapPointers(&trigger->lock, one))
        trigger->isset = 1/*true*/;
    else if (write(trigger->out, "", 1) < 0  &&  errno != EAGAIN)
        return eIO_Unknown;

    return eIO_Success;

#  else

    CORE_LOG_X(32, eLOG_Error,
               "[TRIGGER::Set]  Not yet supported on this platform");
    return eIO_NotSupported;

#  endif /*NCBI_OS_UNIX*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_IsSet(TRIGGER trigger)
{
#ifdef NCBI_CXX_TOOLKIT

#  ifdef NCBI_OS_UNIX

#    ifdef PIPE_SIZE
#      define MAX_TRIGGER_BUF PIPE_SIZE
#    else
#      define MAX_TRIGGER_BUF 8192
#    endif /*PIPE_SIZE*/

    char x_buf[MAX_TRIGGER_BUF];
    int n_read;

    trigger->lock = (void*) 1;

    while ((n_read = read(trigger->fd, x_buf, sizeof(x_buf))) > 0)
        trigger->isset = 1/*true*/;

    trigger->lock = 0;

    if (n_read == 0)
        return eIO_Unknown;

    return trigger->isset ? eIO_Success : eIO_Closed;

#  else

    CORE_LOG_X(33, eLOG_Error,
               "[TRIGGER::IsSet]  Not yet supported on this platform");
    return eIO_NotSupported;

#  endif /*NCBI_OS_UNIX*/

#else

    return eIO_NotSupported;

#endif /*NCBI_CXX_TOOLKIT*/
}


extern EIO_Status TRIGGER_Reset(TRIGGER trigger)
{
    EIO_Status status = TRIGGER_IsSet(trigger);
    trigger->isset = 0/*false*/;

    return status == eIO_Closed ? eIO_Success : status;
}



/******************************************************************************
 *  LISTENING SOCKET
 */

static EIO_Status s_CreateListening(const char*    path,
                                    unsigned short port,
                                    unsigned short backlog, 
                                    LSOCK*         lsock,
                                    TLSCE_Flags    flags)
{
    ESwitch        log = (ESwitch)(flags & 0xF);
    unsigned int   x_id = ++s_ID_Counter;
    TSOCK_Handle   x_lsock;
    SOCK_socklen_t addrlen;
    union {
        struct sockaddr    sa;
        struct sockaddr_in sin;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un sun;
#endif /*NCBI_OS_UNIX*/
    } addr;
    char        s[80];
    const char* c;

    *lsock = 0;
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);
    assert(!!path ^ !!port);

    if (path) {
#ifdef NCBI_OS_UNIX
        addr.sa.sa_family = AF_UNIX;
#else
        return eIO_NotSupported;
#endif
    } else
        addr.sa.sa_family = AF_INET;

    /* create new(listening) socket */
    if ((x_lsock = socket(addr.sa.sa_family, SOCK_STREAM, 0)) == SOCK_INVALID){
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(34, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[?]: [LSOCK::Create] "
                             " Cannot create socket", x_id));
        return eIO_Unknown;
    }

    if ( port ) {
        /*
         * It was confirmed(?) that at least on Solaris 2.5 this precaution:
         * 1) makes the address released immediately after the process
         *    termination;
         * 2) still issue EADDRINUSE error on the attempt to bind() to the
         *    same address being in-use by a living process (if SOCK_STREAM).
         */
        if ( !s_SetReuseAddress(x_lsock, 1/*true*/) ) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EXX(35, eLOG_Error,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("LSOCK#%u[%u]: [LSOCK::Create] "
                                 " Failed setsockopt(REUSEADDR)",
                                 x_id, (unsigned int) x_lsock));
            SOCK_CLOSE(x_lsock);
            return eIO_Unknown;
        }
    }

    /* bind */
    memset(&addr, 0, sizeof(addr));
#ifdef NCBI_OS_UNIX
    if ( path ) {
        addrlen = (SOCK_socklen_t) sizeof(addr.sun);
        addr.sun.sun_family = AF_UNIX;
        strncpy0(addr.sun.sun_path, path, sizeof(addr.sun.sun_path) - 1);
        c = path;
    } else
#endif /*NCBI_OS_UNIX*/
        {
            unsigned int ip =
                flags & fLSCE_BindLocal ? INADDR_LOOPBACK : INADDR_ANY;
            addrlen = sizeof(addr.sin);
            addr.sin.sin_family      = AF_INET;
            addr.sin.sin_addr.s_addr = htonl(ip);
            addr.sin.sin_port        = htons(port);
#ifdef HAVE_SIN_LEN
            addr.sin.sin_len         = addrlen;
#endif /*HAVE_SIN_LEN*/
            SOCK_HostPortToString(htonl(ip), port, s, sizeof(s));
            c = s;
        }
    if (bind(x_lsock, &addr.sa, addrlen) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(36, x_errno == SOCK_EADDRINUSE
                            ? eLOG_Trace : eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Create] "
                             " Failed bind(%s)", x_id,
                             (unsigned int) x_lsock, c));
        SOCK_CLOSE(x_lsock);
        return x_errno == SOCK_EADDRINUSE ? eIO_Closed : eIO_Unknown;
    }

    /* listen */
    if (listen(x_lsock, backlog) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(37, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Create]  Failed "
                             "listen(%hu) at %s", x_id, (unsigned int) x_lsock,
                             backlog, c));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* set to non-blocking mode */
    if ( !s_SetNonblock(x_lsock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(38, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Create] "
                             " Cannot set socket to non-blocking mode",
                             x_id, (unsigned int) x_lsock));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

#ifdef NCBI_OS_UNIX
    if ((flags & fLSCE_CloseOnExec)  &&  !s_SetCloexec(x_lsock, 1/*true*/)) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(110, eLOG_Warning,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Create] "
                             " Cannot set socket close-on-exec mode",
                             x_id, (unsigned int) x_lsock));
    }
#endif /*NCBI_OS_UNIX*/

    /* allocate memory for the internal socket structure */
    if (!(*lsock = (LSOCK)calloc(1, sizeof(**lsock) + (path?strlen(path):0)))){
        return eIO_Unknown;
    }
    (*lsock)->sock     = x_lsock;
    (*lsock)->port     = port;
    (*lsock)->type     = eListening;
    (*lsock)->id       = x_id;
    (*lsock)->log      = log;
    (*lsock)->i_on_sig = eDefault;
#ifdef NCBI_OS_UNIX
    if ( path )
        strcpy((*lsock)->path, path);
#endif /*NCBI_OS_UNIX*/

    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
        CORE_TRACEF(("LSOCK#%u[%u]: Listening on %s",
                     x_id, (unsigned int) x_lsock, c));
    }

    return eIO_Success;
}


extern EIO_Status LSOCK_Create(unsigned short port,
                               unsigned short backlog,
                               LSOCK*         lsock)
{
    return s_CreateListening(0, port, backlog, lsock, fLSCE_LogDefault);
}


extern EIO_Status LSOCK_CreateEx(unsigned short port,
                                 unsigned short backlog,
                                 LSOCK*         lsock,
                                 TLSCE_Flags    flags)
{
    return s_CreateListening(0, port, backlog, lsock, flags);
}


extern EIO_Status LSOCK_CreateUNIX(const char*    path,
                                   unsigned short backlog,
                                   LSOCK*         lsock,
                                   ESwitch        log)
{
    return s_CreateListening(path, 0, backlog, lsock, log);
}


extern EIO_Status LSOCK_Accept(LSOCK           lsock,
                               const STimeout* timeout,
                               SOCK*           sock)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in sin;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un sun;
#endif /*NCBI_OS_UNIX*/
    } addr;
    unsigned int    x_id;
    TSOCK_Handle    x_sock;
    SOCK_socklen_t  addrlen;
    struct SOCK_tag temp;

    if (lsock->sock == SOCK_INVALID) {
        CORE_LOGF_X(39, eLOG_Error,
                    ("LSOCK#%u[?]: [LSOCK::Accept]  Invalid socket",
                     lsock->id));
        assert(0);
        return eIO_Unknown;
    }

    {{ /* wait for the connection request to come (up to timeout) */
        EIO_Status     status;
        SSOCK_Poll     poll;
        struct timeval tv;

        poll.sock   = (SOCK) lsock;
        poll.event  = eIO_Read;
        poll.revent = eIO_Open;
        if ((status = s_Select(1, &poll, s_to2tv(timeout,&tv))) != eIO_Success)
            return status;
        if (poll.revent == eIO_Close)
            return eIO_Unknown;
        assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
    }}

    x_id = (lsock->id * 1000 + ++s_ID_Counter) * 1000;

    /* accept next connection */
    memset(&addr, 0, sizeof(addr));
#ifdef NCBI_OS_UNIX
    if ( lsock->path[0] ) {
        addrlen = (SOCK_socklen_t) sizeof(addr.sun);
    } else
#endif /*NCBI_OS_UNIX*/
        {
            addrlen = (SOCK_socklen_t) sizeof(addr.sin);
#ifdef HAVE_SIN_LEN
            addr.sin.sin_len = addrlen;
#endif /*HAVE_SIN_LEN*/
        }
    if ((x_sock = accept(lsock->sock, &addr.sa, &addrlen)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(40, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Accept] "
                             " Failed accept()", lsock->id,
                             (unsigned int) lsock->sock));
        return eIO_Unknown;
    }
    lsock->n_accept++;
    /* man accept(2) notes that non-blocking state may not be inherited */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(41, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("SOCK#%u[%u]: [LSOCK::Accept]  Cannot"
                             " set accepted socket to non-blocking mode",
                             x_id, (unsigned int) x_sock));
        memset(&temp, 0, sizeof(temp));
        temp.stype = eSOCK_ServerSideKeep;
        temp.type  = eSocket;
        temp.sock  = x_sock;
        s_Close(&temp, 1/*abort*/);
        return eIO_Unknown;
    }

    if (s_ReuseAddress  &&
#ifdef NCBI_OS_UNIX
        !lsock->path[0]  &&
#endif /*NCBI_OS_UNIX*/
        !s_SetReuseAddress(x_sock, 1/*true*/)) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(42, eLOG_Warning,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("SOCK#%u[%u]: [LSOCK::Accept] "
                             " Failed setsockopt(REUSEADDR)",
                             x_id, (unsigned int) x_sock));
    }

    /* create new SOCK structure */
#ifdef NCBI_OS_UNIX
    if ( lsock->path[0] )
        addrlen = strlen(lsock->path);
    else
#endif /*NCBI_OS_UNIX*/
        addrlen = 0;
    if ( !(*sock = (SOCK) calloc(1, sizeof(**sock) + addrlen)) ) {
        memset(&temp, 0, sizeof(temp));
        temp.stype = eSOCK_ServerSideKeep;
        temp.type  = eSocket;
        temp.sock  = x_sock;
        s_Close(&temp, 1/*abort*/);
        return eIO_Unknown;
    }

    /* success */
#ifdef NCBI_OS_UNIX
    if ( lsock->path[0] ) {
        strcpy((*sock)->path, lsock->path);
    } else
#endif /*NCBI_OS_UNIX*/
        {
            (*sock)->host = addr.sin.sin_addr.s_addr;
            (*sock)->port = addr.sin.sin_port;
        }
    (*sock)->sock     = x_sock;
    (*sock)->id       = x_id;
    (*sock)->type     = eSocket;
    (*sock)->log      = lsock->log;
    (*sock)->stype    = eSOCK_ServerSide;
    (*sock)->r_on_w   = eDefault;
    (*sock)->i_on_sig = eDefault;
    (*sock)->r_status = eIO_Success;
    (*sock)->eof      = 0/*false*/;
    (*sock)->w_status = eIO_Success;
    (*sock)->pending  = 0/*connected*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    /* w_buf is unused for accepted sockets */

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(*sock, eIO_Open, 0, 0, &addr.sa);

    return eIO_Success;
}


extern EIO_Status LSOCK_Close(LSOCK lsock)
{
    EIO_Status status;
    char s[80];

    if (lsock->sock == SOCK_INVALID) {
        CORE_LOGF_X(43, eLOG_Error,
                    ("LSOCK#%u[?]: [LSOCK::Close]  Invalid socket",
                     lsock->id));
        assert(0);
        return eIO_Unknown;
    }

    /* set the socket back to blocking mode */
    if (s_Initialized  &&  !s_SetNonblock(lsock->sock, 0/*false*/)) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(44, eLOG_Trace,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("LSOCK#%u[%u]: [LSOCK::Close] "
                             " Cannot set socket back to blocking mode",
                             lsock->id, (unsigned int) lsock->sock));
    }

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn)) {
        const char* c;
#ifdef NCBI_OS_UNIX
        if ( lsock->path[0] )
            c = lsock->path;
        else
#endif /*NCBI_OS_UNIX*/ 
            {
                SOCK_HostPortToString(0, lsock->port, s, sizeof(s));
                c = s;
            }
        CORE_TRACEF(("LSOCK#%u[%u]: Closing at %s "
                     "(%u accept%s total)", lsock->id,
                     (unsigned int) lsock->sock, c,
                     lsock->n_accept, lsock->n_accept == 1 ? "" : "s"));
    }

    status = eIO_Success;
    while (s_Initialized) { /* close persistently - retry if interrupted */
        /* success */
        if (SOCK_CLOSE(lsock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EXX(45, eLOG_Error,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("LSOCK#%u[%u]: [LSOCK::Close] "
                                 " Failed close()", lsock->id,
                                 (unsigned int) lsock->sock));
            status = eIO_Unknown;
            break;
        }
    }

    /* cleanup & return */
    lsock->sock = SOCK_INVALID;
#ifdef NCBI_OS_UNIX
    if ( lsock->path[0] )
        remove(lsock->path);
#endif /*NCBI_OS_UNIX*/

    free(lsock);
    return status;
}


extern EIO_Status LSOCK_GetOSHandle(LSOCK  lsock,
                                    void*  handle,
                                    size_t handle_size)
{
    if (!handle  ||  handle_size != sizeof(lsock->sock)) {
        CORE_LOGF_X(46, eLOG_Error,
                    ("LSOCK#%u[%u]: [LSOCK::GetOSHandle] "
                     " Invalid handle %s%lu", lsock->id,
                     (unsigned int) lsock->sock, handle? "size ": "",
                     handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle, &lsock->sock, handle_size);
    return (!s_Initialized  ||  lsock->sock == SOCK_INVALID
            ? eIO_Closed : eIO_Success);
}



/******************************************************************************
 *  SOCKET
 */

extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port, 
                              const STimeout* timeout,
                              SOCK*           sock)
{
    if (!host || !port)
        return eIO_InvalidArg;
    return s_Create(host, port, timeout, sock, 0, 0, eDefault);
}


extern EIO_Status SOCK_CreateEx(const char*     host,
                                unsigned short  port,
                                const STimeout* timeout,
                                SOCK*           sock,
                                const void*     data,
                                size_t          datalen,
                                ESwitch         log)
{
    if (!host || !port)
        return eIO_InvalidArg;
    return s_Create(host, port, timeout, sock, data, datalen, log);
}


extern EIO_Status SOCK_CreateUNIX(const char*     path,
                                  const STimeout* timeout,
                                  SOCK*           sock,
                                  const void*     data,
                                  size_t          datalen,
                                  ESwitch         log)
{
    if (!path || !*path)
        return eIO_InvalidArg;
#ifdef NCBI_OS_UNIX
    return s_Create(path, 0, timeout, sock, data, datalen, log);
#else
    return eIO_NotSupported;
#endif /*NCBI_OS_UNIX*/
}


extern EIO_Status SOCK_CreateOnTop(const void*   handle,
                                   size_t        handle_size,
                                   SOCK*         sock)
{
    return SOCK_CreateOnTopEx(handle, handle_size, sock, 0, 0,
                              eDefault, eSCOT_CloseOnClose);
}


extern EIO_Status SOCK_CreateOnTopEx(const void*   handle,
                                     size_t        handle_size,
                                     SOCK*         sock,
                                     const void*   data,
                                     size_t        datalen,
                                     ESwitch       log,
                                     ESCOT_OnClose on_close)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } peer;
    SOCK               x_sock;
    TSOCK_Handle       xx_sock;
    BUF                w_buf = 0;
    unsigned int       x_id = ++s_ID_Counter * 1000;
    SOCK_socklen_t     peerlen;
    size_t             socklen;

    *sock = 0;
    assert(!datalen  ||  data);

    if (!handle  ||  handle_size != sizeof(xx_sock)) {
        CORE_LOGF_X(47, eLOG_Error,
                    ("SOCK#%u[?]: [SOCK::CreateOnTopEx] "
                     " Invalid handle %s%lu", x_id,
                     handle ? "size " : "",
                     handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }
    memcpy(&xx_sock, handle, sizeof(xx_sock));

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* get peer's address */
    peerlen = (SOCK_socklen_t) sizeof(peer);
    memset(&peer, 0, sizeof(peer));
#ifdef HAVE_SIN_LEN
    peer.sa.sa_len = peerlen;
#endif
    if (getpeername(xx_sock, &peer.sa, &peerlen) < 0)
        return eIO_Closed;
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family != AF_INET  &&  peer.sa.sa_family != AF_UNIX)
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
        if (peer.sa.sa_family != AF_UNSPEC/*0*/)
#  endif /*NCBI_OS_???*/
            return eIO_InvalidArg;
#else
    if (peer.sa.sa_family != AF_INET)
        return eIO_InvalidArg;
#endif /*NCBI_OS_UNIX*/

#ifdef NCBI_OS_UNIX
    if (
#  if defined(NCBI_OS_BSD)     ||  \
      defined(NCBI_OS_DARWIN)  ||  \
      defined(NCBI_OS_IRIX)
        peer.sa.sa_family == AF_UNSPEC/*0*/  ||
#  endif /*NCBI_OS_???*/
        peer.sa.sa_family == AF_UNIX) {
        if (!peer.un.sun_path[0]) {
            peerlen = (SOCK_socklen_t) sizeof(peer);
            memset(&peer, 0, sizeof(peer));
#  ifdef HAVE_SIN_LEN
            peer.sa.sa_len = peerlen;
#  endif
            if (getsockname(xx_sock, &peer.sa, &peerlen) < 0)
                return eIO_Closed;
            assert(peer.sa.sa_family == AF_UNIX);
            if (!peer.un.sun_path[0]) {
                CORE_LOGF_X(48, eLOG_Error,
                            ("SOCK#%u[%u]: [SOCK::CreateOnTopEx] "
                             " Unbound UNIX socket",
                             x_id, (unsigned int) xx_sock));
                assert(0);
                return eIO_InvalidArg;
            }
        }
        socklen = strlen(peer.un.sun_path);
    } else
#endif /*NCBI_OS_UNIX*/
        socklen = 0;
    
    /* store initial data */
    if (datalen  &&  (!BUF_SetChunkSize(&w_buf, datalen)  ||
                      !BUF_Write(&w_buf, data, datalen))) {
        CORE_LOGF_ERRNO_X(49, eLOG_Error, errno,
                          ("SOCK#%u[%u]: [SOCK::CreateOnTopEx]  Cannot store "
                           "initial data", x_id, (unsigned int) xx_sock));
        BUF_Destroy(w_buf);
        return eIO_Unknown;
    }
    
    /* create and fill socket handle */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + socklen))) {
        BUF_Destroy(w_buf);
        return eIO_Unknown;
    }
    x_sock->sock     = xx_sock;
    x_sock->id       = x_id;
    x_sock->type     = eSocket;
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family != AF_UNIX) {
        x_sock->host = peer.in.sin_addr.s_addr;
        x_sock->port = peer.in.sin_port;
    } else
        strcpy(x_sock->path, peer.un.sun_path);
#else
    x_sock->host     = peer.in.sin_addr.s_addr;
    x_sock->port     = peer.in.sin_port;
#endif /*NCBI_OS_UNIX*/
    x_sock->log      = log;
    x_sock->stype     = (on_close != eSCOT_KeepOnClose
                        ? eSOCK_ServerSide
                        : eSOCK_ServerSideKeep);
    x_sock->r_on_w   = eDefault;
    x_sock->i_on_sig = eDefault;
    x_sock->r_status = eIO_Success;
    x_sock->eof      = 0/*false*/;
    x_sock->w_status = eIO_Success;
    x_sock->pending  = 1/*have to check at the nearest I/O*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    x_sock->w_buf    = w_buf;
    x_sock->w_len    = datalen;

    /* set to non-blocking mode */
    if ( !s_SetNonblock(xx_sock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        char _id[32];
        CORE_LOGF_ERRNO_EXX(50, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[SOCK::CreateOnTopEx]  Cannot set socket "
                             "to non-blocking mode", s_ID(x_sock, _id)));
        x_sock->sock = SOCK_INVALID;
        SOCK_Close(x_sock);
        return eIO_Unknown;
    }
    
    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn))
        s_DoLog(x_sock, eIO_Open, &peer, 0, &peer.sa);

    /* success */
    *sock = x_sock;
    return eIO_Success;
}


static EIO_Status s_Reconnect(SOCK            sock,
                              const char*     host,
                              unsigned short  port,
                              const STimeout* timeout)
{
    /* close the socket if necessary */
    if (sock->sock != SOCK_INVALID)
        s_Close(sock, 0/*orderly*/);

    /* special treatment for server-side socket */
    if (sock->stype & eSOCK_ServerSide) {
        char _id[32];
        if (!host  ||  !*host  ||  !port) {
            CORE_LOGF_X(51, eLOG_Error,
                        ("%s[SOCK::Reconnect]  Attempt to reconnect "
                         "server-side socket as the client one to "
                         "its peer address", s_ID(sock, _id)));
            return eIO_InvalidArg;
        }
        sock->stype = eSOCK_ClientSide;
    }

    /* connect */
    sock->id++;
    sock->n_read    = 0;
    sock->n_written = 0;
    return s_Connect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    char _id[32];

    if (sock->sock == eSOCK_Datagram) {
        CORE_LOGF_X(52, eLOG_Error,
                    ("%s[SOCK::Reconnect]  Datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (host  &&  !*host)
        host = 0;

#ifdef NCBI_OS_UNIX
    if (sock->path[0] && (host || port)) {
        CORE_LOGF_X(53, eLOG_Error,
                    ("%s[SOCK::Reconnect] "
                     " Cannot reconnect UNIX socket \"%s\" as INET",
                     s_ID(sock, _id), sock->path));
        assert(0);
        return eIO_InvalidArg;
    }
#endif /*NCBI_OS_UNIX*/
    return s_Reconnect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Shutdown(SOCK      sock,
                                EIO_Event how)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(54, eLOG_Error,
                    ("%s[SOCK::Shutdown]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->stype == eSOCK_Datagram) {
        CORE_LOGF_X(55, eLOG_Error,
                    ("%s[SOCK::Shutdown]  Datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    return s_Shutdown(sock, how, sock->w_timeout);
}


extern EIO_Status SOCK_CloseEx(SOCK sock, int/*bool*/ destroy)
{
    EIO_Status status;
    if (!s_Initialized) {
        sock->sock = SOCK_INVALID;
        status = eIO_Success;
    } else if (sock->sock == SOCK_INVALID) {
        status = eIO_Closed;
    } else
        status = s_Close(sock, 0/*orderly*/);

    if (destroy) {
        BUF_Destroy(sock->r_buf);
        BUF_Destroy(sock->w_buf);
        free(sock);
    }
    return status;
}


extern EIO_Status SOCK_Close(SOCK sock)
{
    return SOCK_CloseEx(sock, 1/*destroy*/);
}


extern EIO_Status SOCK_Wait(SOCK            sock,
                            EIO_Event       event,
                            const STimeout* timeout)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(56, eLOG_Error,
                    ("%s[SOCK::Wait]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    /* check against already shutdown socket there */
    switch ( event ) {
    case eIO_Open:
        if (sock->stype == eSOCK_Datagram)
            return eIO_Success/*always connected*/;
        if (sock->pending) {
            struct timeval tv;
            int x_errno/*unused*/;
            return s_IsConnected(sock, s_to2tv(timeout, &tv), &x_errno, 0);
        }
        if (sock->r_status == eIO_Success  &&  sock->w_status == eIO_Success)
            return eIO_Success;
        if (sock->r_status == eIO_Closed   &&  sock->w_status == eIO_Closed)
            return eIO_Closed;
        return eIO_Unknown;
    case eIO_Read:
        if (BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->stype == eSOCK_Datagram)
            return eIO_Closed;
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF_X(57, eLOG_Warning,
                        ("%s[SOCK::Wait(R)]  Socket has already been %s",
                         s_ID(sock, _id), sock->eof ? "closed" : "shut down"));
            return eIO_Closed;
        }
        if ( sock->eof )
            return eIO_Closed;
        break;
    case eIO_Write:
        if (sock->stype == eSOCK_Datagram)
            return eIO_Success;
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF_X(58, eLOG_Warning,
                        ("%s[SOCK::Wait(W)]  Socket has already "
                         "been shut down", s_ID(sock, _id)));
            return eIO_Closed;
        }
        break;
    case eIO_ReadWrite:
        if (sock->stype == eSOCK_Datagram  ||  BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if ((sock->r_status == eIO_Closed  ||  sock->eof)  &&
            (sock->w_status == eIO_Closed)) {
            if (sock->r_status == eIO_Closed) {
                CORE_LOGF_X(59, eLOG_Warning,
                            ("%s[SOCK::Wait(RW)]  Socket has "
                             "already been shut down", s_ID(sock, _id)));
            }
            return eIO_Closed;
        }
        if (sock->r_status == eIO_Closed  ||  sock->eof) {
            if (sock->r_status == eIO_Closed) {
                CORE_LOGF_X(60, eLOG_Note,
                            ("%s[SOCK::Wait(RW)]  Socket has already "
                             "been %s", s_ID(sock, _id), sock->eof
                             ? "closed" : "shut down for reading"));
            }
            event = eIO_Write;
            break;
        }
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF_X(61, eLOG_Note,
                        ("%s[SOCK::Wait(RW)]  Socket has already been "
                         "shut down for writing", s_ID(sock, _id)));
            event = eIO_Read;
            break;
        }
        break;
    default:
        CORE_LOGF_X(62, eLOG_Error,
                    ("%s[SOCK::Wait]  Invalid event %u",
                     s_ID(sock, _id), (unsigned int) event));
        return eIO_InvalidArg;
    }

    assert(sock->stype != eSOCK_Datagram);
    /* do wait */
    {{
        struct timeval        tv;
        SSOCK_Poll            poll;
        EIO_Status            status;
        const struct timeval* x_tv = s_to2tv(timeout, &tv);

        if ((status = s_WritePending(sock, x_tv, 0, 0)) != eIO_Success) {
            if (event == eIO_Write  ||  sock->pending)
                return status;
        }
        poll.sock   = sock;
        poll.event  = event;
        poll.revent = eIO_Open;
        if ((status = s_SelectStallsafe(1, &poll, x_tv, 0)) != eIO_Success)
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
            polls[x_n].sock->type != eListening    &&
            polls[x_n].sock->sock != SOCK_INVALID  &&
            (polls[x_n].event == eIO_Read  ||
             polls[x_n].event == eIO_ReadWrite)    &&
            BUF_Size(polls[x_n].sock->r_buf) != 0) {
            polls[x_n].revent = eIO_Read;
        } else
            polls[x_n].revent = eIO_Open;
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
    assert(!sock  ||  sock->type == eSocket);
    return (POLLABLE) sock;
}


extern TRIGGER POLLABLE_ToTRIGGER(POLLABLE poll)
{
    TRIGGER trigger = (TRIGGER) poll;
    return !trigger  ||  trigger->type == eTrigger ? trigger : 0;
}


extern LSOCK POLLABLE_ToLSOCK(POLLABLE poll)
{
    LSOCK lsock = (LSOCK) poll;
    return !lsock  ||  lsock->type == eListening ? lsock : 0;
}


extern SOCK  POLLABLE_ToSOCK(POLLABLE poll)
{
    SOCK sock = (SOCK) poll;
    return !sock  ||  sock->type == eSocket ? sock : 0;
}


extern EIO_Status SOCK_SetTimeout(SOCK            sock,
                                  EIO_Event       event,
                                  const STimeout* timeout)
{
    char _id[32];

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
        CORE_LOGF_X(63, eLOG_Error,
                    ("%s[SOCK::SetTimeout]  Invalid event %u",
                     s_ID(sock, _id), (unsigned int) event));
        assert(0);
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


extern const STimeout* SOCK_GetTimeout(SOCK      sock,
                                       EIO_Event event)
{
    const STimeout *tr, *tw;
    char _id[32];

    switch ( event ) {
    case eIO_Read:
        return s_tv2to(sock->r_timeout, &sock->r_to);
    case eIO_Write:
        return s_tv2to(sock->w_timeout, &sock->w_to);
    case eIO_ReadWrite:
        /* both timeouts come out normalized */
        tr = s_tv2to(sock->r_timeout, &sock->r_to);
        tw = s_tv2to(sock->w_timeout, &sock->w_to);
        if ( !tr )
            return tw;
        if ( !tw )
            return tr;
        if (tr->sec > tw->sec)
            return tw;
        if (tw->sec > tr->sec)
            return tr;
        assert(tr->sec == tw->sec);
        return tr->usec > tw->usec ? tw : tr;
    case eIO_Close:
        return s_tv2to(sock->c_timeout, &sock->c_to);
    default:
        CORE_LOGF_X(64, eLOG_Error,
                    ("%s[SOCK::GetTimeout]  Invalid event %u",
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
    char       _id[32];

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_ReadPlain:
            status = s_Read(sock, buf, size, &x_read, 0/*read*/);
            break;

        case eIO_ReadPeek:
            status = s_Read(sock, buf, size, &x_read, 1/*peek*/);
            break;

        case eIO_ReadPersist:
            x_read = 0;
            do {
                size_t xx_read;
                status = s_Read(sock, (char*)buf + (buf ? x_read : 0),
                                size, &xx_read, 0/*read*/);
                x_read += xx_read;
                size   -= xx_read;
            } while (size  &&  status == eIO_Success);
            break;

        default:
            CORE_LOGF_X(65, eLOG_Error,
                        ("%s[SOCK::Read]  Invalid read method %u",
                         s_ID(sock, _id), (unsigned int) how));
            assert(0);
            x_read = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOGF_X(66, eLOG_Error,
                    ("%s[SOCK::Read]  Invalid socket", s_ID(sock, _id)));
        x_read = 0;
        status = eIO_Closed;
    }

    if ( n_read )
        *n_read = x_read;
    return status;
}


extern EIO_Status SOCK_ReadLine(SOCK    sock,
                                char*   line,
                                size_t  size,
                                size_t* n_read)
{
    EIO_Status  status = eIO_Success;
    int/*bool*/ cr_seen = 0/*false*/;
    int/*bool*/ done = 0/*false*/;
    size_t      len = 0;

    do {
        size_t i;
        char   w[1024], c;
        size_t x_size = BUF_Size(sock->r_buf);
        char*  x_buf  = size - len < sizeof(w) - cr_seen ? w : line + len;
        if (!x_size  ||  x_size > sizeof(w) - cr_seen)
            x_size = sizeof(w) - cr_seen;
        status = s_Read(sock, x_buf + cr_seen, x_size, &x_size, 0/*read*/);
        if ( !x_size )
            done = 1/*true*/;
        else if ( cr_seen )
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
            if (SOCK_PushBack(sock, &c, 1) != eIO_Success)
                status = eIO_Unknown;
        }
        if (i < x_size
            &&  SOCK_PushBack(sock, &x_buf[i], x_size - i) != eIO_Success) {
            status = eIO_Unknown;
        }
    } while (!done  &&  status == eIO_Success);

    if (len < size)
        line[len] = '\0';
    if ( n_read )
        *n_read = len;

    return status;
}


extern EIO_Status SOCK_PushBack(SOCK        sock,
                                const void* buf,
                                size_t      size)
{
    if (sock->sock == SOCK_INVALID) {
        char _id[32];
        CORE_LOGF_X(67, eLOG_Error,
                    ("%s[SOCK::PushBack]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    return BUF_PushBack(&sock->r_buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_Write(SOCK            sock,
                             const void*     buf,
                             size_t          size,
                             size_t*         n_written,
                             EIO_WriteMethod how)
{
    EIO_Status status;
    char       _id[32];
    size_t     x_written;

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_WriteOutOfBand:
            if (sock->stype == eSOCK_Datagram) {
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
                        ("%s[SOCK::Write]  Invalid write method %u",
                         s_ID(sock, _id), (unsigned int) how));
            assert(0);
            x_written = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOGF_X(70, eLOG_Error,
                    ("%s[SOCK::Write]  Invalid socket", s_ID(sock, _id)));
        x_written = 0;
        status = eIO_Closed;
    }

    if ( n_written )
        *n_written = x_written;
    return status;
}


extern EIO_Status SOCK_Abort(SOCK sock)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(71, eLOG_Warning,
                    ("%s[SOCK::Abort]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }
    if (sock->stype == eSOCK_Datagram) {
        CORE_LOGF_X(72, eLOG_Error,
                    ("%s[SOCK::Abort]  Datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    sock->eof = 0;
    sock->w_len = 0;
    sock->pending = 0;
    return s_Close(sock, 1/*abort*/);
}


extern EIO_Status SOCK_Status(SOCK      sock,
                              EIO_Event direction)
{
    switch (direction) {
    case eIO_Open:
    case eIO_Read:
    case eIO_Write:
        if (sock->sock == SOCK_INVALID)
            return eIO_Closed;
        if (sock->pending)
            return eIO_Timeout;
        if (direction == eIO_Open)
            return eIO_Success;
        break;
    default:
        return eIO_InvalidArg;
    }
    return s_Status(sock, direction);
}


extern unsigned short SOCK_GetLocalPort(SOCK          sock,
                                        ENH_ByteOrder byte_order)
{
    if (sock->sock == SOCK_INVALID)
        return 0;

    if (!sock->myport) {
        struct sockaddr_in addr;
        SOCK_socklen_t addrlen = (SOCK_socklen_t) sizeof(addr);
        memset(&addr, 0, sizeof(addr));
#  ifdef HAVE_SIN_LEN
        addr.sin_len = addrlen;
#  endif /*HAVE_SIN_LEN*/
        if (getsockname(sock->sock,
                        (struct sockaddr*) &addr, &addrlen) == 0) {
            assert(addr.sin_family == AF_INET);
            sock->myport = ntohs(addr.sin_port);
        }
    }
    return byte_order != eNH_HostByteOrder? htons(sock->myport) : sock->myport;
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


extern char* SOCK_GetPeerAddressString(SOCK   sock,
                                       char*  buf,
                                       size_t buflen)
{
    if (!buf  ||  !buflen)
        return 0;
#ifdef NCBI_OS_UNIX
    if (sock->path[0])
        strncpy0(buf, sock->path, buflen - 1);
    else
#endif /*NCBI_OS_UNIX*/
        SOCK_HostPortToString(sock->host, ntohs(sock->port), buf, buflen);
    return buf;
}


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle,
                                   size_t handle_size)
{
    if (!handle  ||  handle_size != sizeof(sock->sock)) {
        char _id[32];
        CORE_LOGF_X(73, eLOG_Error,
                    ("%s[SOCK::GetOSHandle]  Invalid handle %s%lu",
                     s_ID(sock, _id), handle ? "size " : "",
                     handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle, &sock->sock, handle_size);
    return sock->sock == SOCK_INVALID ? eIO_Closed : eIO_Success;
}


extern ESwitch SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    ESwitch old = s_ReadOnWrite;
    if (on_off == eDefault)
        on_off = eOff;
    s_ReadOnWrite = on_off;
    return old;
}


extern ESwitch SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off)
{
    if (sock->stype != eSOCK_Datagram) {
        ESwitch old = (ESwitch) sock->r_on_w;
        sock->r_on_w = on_off;
        return old;
    }
    return eDefault;
}


extern ESwitch SOCK_SetInterruptOnSignalAPI(ESwitch on_off)
{
    ESwitch old = s_InterruptOnSignal;
    if (on_off == eDefault)
        on_off = eOff;
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
    int old = s_ReuseAddress;
    s_ReuseAddress = on_off == eOn ? 1 : 0;
    return old ? eOn : eOff;
}


extern void SOCK_SetReuseAddress(SOCK sock, int/*bool*/ on_off)
{
    if (sock->sock != SOCK_INVALID && !s_SetReuseAddress(sock->sock, on_off)) {
        int x_errno = SOCK_ERRNO;
        char _id[32];
        CORE_LOGF_ERRNO_EXX(74, eLOG_Warning,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[SOCK::SetReuseAddress] "
                             " Failed setsockopt(%sREUSEADDR)",
                             s_ID(sock, _id), on_off ? "" : "NO"));
    }
}


extern void SOCK_DisableOSSendDelay(SOCK sock, int/*bool*/ on_off)
{
#ifdef TCP_NODELAY
    if (sock->sock != SOCK_INVALID) {
        int n = (int) on_off;
        if (setsockopt(sock->sock, IPPROTO_TCP, TCP_NODELAY,
                       (void*)&n, sizeof(n)) != 0) {
            int x_errno = SOCK_ERRNO;
            char _id[32];
            CORE_LOGF_ERRNO_EXX(75, eLOG_Warning,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[SOCK::DisableOSSendDelay] "
                                 " Failed setsockopt(%sTCP_NODELAY)",
                                 s_ID(sock, _id), on_off ? "" : "!"));
        }
    }
#endif /*TCP_NODELAY*/
}


extern EIO_Status DSOCK_Create(SOCK* sock)
{
    return DSOCK_CreateEx(sock, eDefault);
}


extern EIO_Status DSOCK_CreateEx(SOCK* sock, ESwitch log)
{
    unsigned int x_id = ++s_ID_Counter * 1000;
    TSOCK_Handle x_sock;

    *sock = 0;
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* create new datagram socket */
    if ((x_sock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(76, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("SOCK#%u[?]: [DSOCK::Create] "
                             " Cannot create socket", x_id));
        return eIO_Unknown;
    }

    /* set to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(77, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("SOCK#%u[%u]: [DSOCK::Create]  Cannot set "
                             "socket to non-blocking mode", x_id, x_sock));
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    if ( !(*sock = (SOCK) calloc(1, sizeof(**sock))) ) {
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* success... */
    (*sock)->sock     = x_sock;
    (*sock)->id       = x_id;
    /* no host and port - not "connected" */
    (*sock)->type     = eSocket;
    (*sock)->log      = log;
    (*sock)->stype    = eSOCK_Datagram;
    (*sock)->r_on_w   = eOff;
    (*sock)->i_on_sig = eDefault;
    (*sock)->r_status = eIO_Success;
    (*sock)->eof      = 0/*false*/;
    (*sock)->w_status = eIO_Success;
    /* all timeouts cleared - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    BUF_SetChunkSize(&(*sock)->w_buf, SOCK_BUF_CHUNK_SIZE);

    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn))
        s_DoLog(*sock, eIO_Open, 0, 0, 0);

    return eIO_Success;
}


extern EIO_Status DSOCK_Bind(SOCK sock, unsigned short port)
{
    struct sockaddr_in addr;
    char _id[32];

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(78, eLOG_Error,
                    ("%s[DSOCK::Bind] "
                     " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(79, eLOG_Error,
                    ("%s[DSOCK::Bind]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    /* bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
#ifdef HAVE_SIN_LEN
    addr.sin_len         = sizeof(addr);
#endif /*HAVE_SIN_LEN*/
    if (bind(sock->sock, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EXX(80, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                           ("%s[DSOCK::Bind]  Failed bind()", s_ID(sock,_id)));
        return x_errno == SOCK_EADDRINUSE ? eIO_Closed : eIO_Unknown;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, 0, 0, (struct sockaddr*) &addr);
    return eIO_Success;
}


extern EIO_Status DSOCK_Connect(SOCK sock,
                                const char* host, unsigned short port)
{
    struct sockaddr_in peer;
    char _id[32];

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(81, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(82, eLOG_Error,
                    ("%s[DSOCK::Connect]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    /* drop all pending data */
    s_WipeRBuf(sock);
    s_WipeWBuf(sock);
    sock->id++;

    /* obtain host and port of the peer */
    if ( port )
        sock->port = htons(port);
    if (host  &&  *host) {
        if ((sock->host = SOCK_gethostbyname(host)) == 0) {
            CORE_LOGF_X(83, eLOG_Error,
                        ("%s[DSOCK::Connect]  Failed "
                         "SOCK_gethostbyname(\"%.64s\")",
                         s_ID(sock, _id), host));
            return eIO_Unknown;
        }
    }
    if (!sock->host  ||  !sock->port) {
        CORE_LOGF_X(84, eLOG_Error,
                    ("%s[DSOCK::Connect] "
                     " Address incomplete", s_ID(sock, _id)));
        return eIO_InvalidArg;
    }

    /* connect */
    memset(&peer, 0, sizeof(peer));
    peer.sin_family      = AF_INET;
    peer.sin_addr.s_addr = sock->host;
    peer.sin_port        = sock->port;
#ifdef HAVE_SIN_LEN
    peer.sin_len         = sizeof(peer);
#endif /*HAVE_SIN_LEN*/
    if (connect(sock->sock, (struct sockaddr*) &peer, sizeof(peer)) != 0) {
        char addr[80];
        int x_errno = SOCK_ERRNO;
        SOCK_HostPortToString(sock->host, ntohs(sock->port),addr,sizeof(addr));
        CORE_LOGF_ERRNO_EXX(85, eLOG_Error,
                            x_errno, SOCK_STRERROR(x_errno),
                            ("%s[DSOCK::Connect]  Failed connect() to %s",
                             s_ID(sock, _id), addr));
        return eIO_Unknown;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, &peer, 0, (struct sockaddr*) &peer);
    return eIO_Success;
}


extern EIO_Status DSOCK_SendMsg(SOCK            sock,
                                const char*     host,
                                unsigned short  port,
                                const void*     data,
                                size_t          datalen)
{
    size_t             x_msgsize;
    char               w[1536];
    EIO_Status         status;
    unsigned short     x_port;
    unsigned int       x_host;
    void*              x_msg;
    struct sockaddr_in addr;

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(86, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Not a datagram socket", s_ID(sock, w)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(87, eLOG_Error,
                    ("%s[DSOCK::SendMsg]  Invalid socket", s_ID(sock, w)));
        return eIO_Closed;
    }

    if ( datalen ) {
        s_Write(sock, data, datalen, &x_msgsize, 0);
        verify(x_msgsize == datalen);
    }
    sock->eof = 1/*true - finalized message*/;

    x_port = port ? htons(port) : sock->port;
    if (host  &&  *host) {
        if ( !(x_host = SOCK_gethostbyname(host)) ) {
            CORE_LOGF_X(88, eLOG_Error,
                        ("%s[DSOCK::SendMsg]  Failed "
                         "SOCK_gethostbyname(\"%.64s\")",
                         s_ID(sock, w), host));
            return eIO_Unknown;
        }
    } else
        x_host = sock->host;

    if (!x_host  ||  !x_port) {
        CORE_LOGF_X(89, eLOG_Error,
                    ("%s[DSOCK::SendMsg] "
                     " Address incomplete", s_ID(sock, w)));
        return eIO_Unknown;
    }

    if ((x_msgsize = BUF_Size(sock->w_buf)) != 0) {
        if (x_msgsize <= sizeof(w))
            x_msg = w;
        else if ( !(x_msg = malloc(x_msgsize)) )
            return eIO_Unknown;
        verify(BUF_Peek(sock->w_buf, x_msg, x_msgsize) == x_msgsize);
    } else
        x_msg = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = x_host;
    addr.sin_port        = x_port;
#ifdef HAVE_SIN_LEN
    addr.sin_len         = sizeof(addr);
#endif /*HAVE_SIN_LEN*/

    for (;;) { /* optionally auto-resume if interrupted by a signal */
        int  x_written;
        int  x_errno;
        if ((x_written = sendto(sock->sock, x_msg, x_msgsize, 0/*flags*/,
                                (struct sockaddr*) &addr, sizeof(addr))) >= 0){
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Write, x_msg, (size_t) x_written,
                        (struct sockaddr*) &addr);
            }
            sock->n_written += x_written;
            sock->n_out++;
            if ((size_t) x_written != x_msgsize) {
                sock->w_status = status = eIO_Closed;
                CORE_LOGF_X(90, eLOG_Error,
                            ("%s[DSOCK::SendMsg] "
                             " Partial datagram sent",s_ID(sock,w)));
                break;
            }
            sock->w_status = status = eIO_Success;
            break;
        }

        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = status = eIO_Unknown;
        x_errno = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pull incoming data from the socket */
            if ((status = s_Select(1, &poll, sock->w_timeout)) != eIO_Success)
                break;
            if (poll.revent == eIO_Close) {
                status = eIO_Unknown;
                break;
            }
            assert(poll.revent == eIO_Write);
            continue;
        }

        if (x_errno != SOCK_EINTR) {
            CORE_LOGF_ERRNO_EXX(91, eLOG_Trace,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[DSOCK::SendMsg] "
                                 " Failed sendto()", s_ID(sock, w)));
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = status = eIO_Interrupt;
            break;
        }
    }

    if (x_msg  &&  x_msg != w)
        free(x_msg);
    if (status == eIO_Success)
        sock->w_status = s_WipeWBuf(sock);
    return status;
}


extern EIO_Status DSOCK_RecvMsg(SOCK            sock,
                                void*           buf,
                                size_t          buflen,
                                size_t          msgsize,
                                size_t*         msglen,
                                unsigned int*   sender_addr,
                                unsigned short* sender_port)
{
    size_t     x_msgsize;
    char       w[1536];
    EIO_Status status;
    void*      x_msg;

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(92, eLOG_Error,
                    ("%s[DSOCK::RecvMsg] "
                     " Not a datagram socket", s_ID(sock, w)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(93, eLOG_Error,
                    ("%s[DSOCK::RecvMsg]  Invalid socket", s_ID(sock, w)));
        return eIO_Closed;
    }

    s_WipeRBuf(sock);
    if ( msglen )
        *msglen = 0;
    if ( sender_addr )
        *sender_addr = 0;
    if ( sender_port )
        *sender_port = 0;

    x_msgsize = (msgsize  &&  msgsize < ((1 << 16) - 1))
        ? msgsize : ((1 << 16) - 1);

    if ( !(x_msg = (x_msgsize <= buflen
                    ? buf : (x_msgsize <= sizeof(w)
                             ? w : malloc(x_msgsize)))) ) {
        return eIO_Unknown;
    }

    for (;;) { /* auto-resume if either blocked or interrupted (optional) */
        int                x_errno;
        int                x_read;
        struct sockaddr_in addr;
        SOCK_socklen_t     addrlen = (SOCK_socklen_t) sizeof(addr);
        memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SIN_LEN
        addr.sin_len = addrlen;
#endif
        x_read = recvfrom(sock->sock, x_msg, x_msgsize, 0,
                          (struct sockaddr*) &addr, &addrlen);
        if (x_read >= 0) {
            /* got a message */
            sock->r_status = status = eIO_Success;
            if ( x_read ) {
                if ( msglen )
                    *msglen = x_read;
                if ( sender_addr )
                    *sender_addr = addr.sin_addr.s_addr;
                if ( sender_port )
                    *sender_port = ntohs(addr.sin_port);
                if ((size_t) x_read > buflen  &&
                    !BUF_Write(&sock->r_buf,
                               (char*) x_msg  + buflen,
                               (size_t)x_read - buflen)) {
                    sock->r_status = eIO_Unknown;
                }
                if (buflen  &&  x_msgsize > buflen)
                    memcpy(buf, x_msg, buflen);
            }
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Read, x_msg, (size_t) x_read,
                        (struct sockaddr*) &addr);
            }
            sock->n_read += x_read;
            sock->n_in++;
            break;
        }

        x_errno = SOCK_ERRNO;
        sock->r_status = status = eIO_Unknown;
        if (x_errno != SOCK_EWOULDBLOCK  &&
            x_errno != SOCK_EAGAIN       &&
            x_errno != SOCK_EINTR) {
            /* catch unknown ERROR */
            CORE_LOGF_ERRNO_EXX(94, eLOG_Trace,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[DSOCK::RecvMsg] "
                                 " Failed recvfrom()", s_ID(sock, w)));
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

    if (x_msgsize > buflen && x_msg != w)
        free(x_msg);
    return status;
}


extern EIO_Status DSOCK_WaitMsg(SOCK sock, const STimeout* timeout)
{
    char           _id[32];
    EIO_Status     status;
    SSOCK_Poll     poll;
    struct timeval tv;

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(95, eLOG_Error,
                    ("%s[DSOCK::WaitMsg] "
                     " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(96, eLOG_Error,
                    ("%s[DSOCK::WaitMsg]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    poll.sock   = sock;
    poll.event  = eIO_Read;
    poll.revent = eIO_Open;
    if ((status = s_Select(1, &poll, s_to2tv(timeout, &tv))) != eIO_Success ||
        poll.revent == eIO_Read) {
        return status;
    }
    assert(poll.revent == eIO_Close);
    return eIO_Closed;
}


extern EIO_Status DSOCK_WipeMsg(SOCK sock, EIO_Event direction)
{
    char _id[32];
    EIO_Status status;

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(97, eLOG_Error,
                    ("%s[DSOCK::WipeMsg] "
                     " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(98, eLOG_Error,
                    ("%s[DSOCK::WipeMsg]  Invalid socket", s_ID(sock, _id)));
        return eIO_Closed;
    }

    switch (direction) {
    case eIO_Read:
        sock->r_status = status = s_WipeRBuf(sock);
        break;
    case eIO_Write:
        sock->w_status = status = s_WipeWBuf(sock);
        break;
    default:
        CORE_LOGF_X(99, eLOG_Error,
                    ("%s[DSOCK::WipeMsg]  Invalid direction %u",
                     s_ID(sock, _id), (unsigned int) direction));
        assert(0);
        status = eIO_InvalidArg;
        break;
    }

    return status;
}


extern EIO_Status DSOCK_SetBroadcast(SOCK sock, int/*bool*/ broadcast)
{
    char _id[32];

    if (sock->stype != eSOCK_Datagram) {
        CORE_LOGF_X(100, eLOG_Error,
                    ("%s[DSOCK::SetBroadcast] "
                     " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF_X(101, eLOG_Error,
                    ("%s[DSOCK::SetBroadcast]  Invalid socket",
                     s_ID(sock, _id)));
        return eIO_Closed;
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
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EXX(102, eLOG_Error,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("%s[DSOCK::SetBroadcast] "
                                 " Failed setsockopt(%sBROADCAST)",
                                 s_ID(sock, _id), bcast ? "" : "NO"));
            return eIO_Unknown;
        }
    }}
#else
    return eIO_NotSupported;
#endif /*NCBI_OS_UNIX || NXBI_OS_MSWIN*/
    return eIO_Success;
}


extern int/*bool*/ SOCK_IsDatagram(SOCK sock)
{
    return sock->sock != SOCK_INVALID  &&  sock->stype == eSOCK_Datagram;
}


extern int/*bool*/ SOCK_IsClientSide(SOCK sock)
{
    return sock->sock != SOCK_INVALID  &&  sock->stype == eSOCK_ClientSide;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock->sock != SOCK_INVALID  &&  (sock->stype & eSOCK_ServerSide);
}


extern int/*bool*/ SOCK_IsUNIX(SOCK sock)
{
#ifdef NCBI_OS_UNIX
    return sock->sock != SOCK_INVALID  &&  sock->path[0];
#else
    return 0/*false*/;
#endif /*NCBI_OS_UNIX*/
}


extern int SOCK_gethostname(char*  name,
                            size_t namelen)
{
    int error = 0;

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    name[0] = name[namelen - 1] = '\0';
    if (gethostname(name, (int) namelen) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EXX(103, eLOG_Error,
                           x_errno, SOCK_STRERROR(x_errno),
                           "[SOCK_gethostname]  Failed gethostname()");
        error = 1;
    } else if ( name[namelen - 1] ) {
        CORE_LOG_X(104, eLOG_Error,
                   "[SOCK_gethostname]  Buffer too small");
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
    char str[16/*sizeof("255.255.255.255")*/];
    int len;

    assert(buf  &&  buflen > 0);
    verify((len = sprintf(str, "%u.%u.%u.%u", b[0], b[1], b[2], b[3])) > 0);
    assert((size_t) len < sizeof(str));

    if ((size_t) len < buflen) {
        memcpy(buf, str, len + 1);
        return 0/*success*/;
    }

    buf[0] = '\0';
    return -1/*failed*/;
}


extern int/*bool*/ SOCK_isip(const char* host)
{
    return SOCK_isipEx(host, 0/*nofullquad*/);
}


extern int/*bool*/ SOCK_isipEx(const char* host, int/*bool*/ fullquad)
{
    const char* c = host;
    unsigned long val;
    int dots = 0;

    for (;;) {
        char* e;
        if (!isdigit((unsigned char)(*c)))
            return 0/*false*/;
        errno = 0;
        val = strtoul(c, &e, fullquad ? 10 : 0);
        if (c == e  ||  errno)
            return 0/*false*/;
        c = e;
        if (*c != '.')
            break;
        if (++dots > 3)
            return 0/*false*/;
        if (val > 255)
            return 0/*false*/;
        c++;
    }

    return !*c  &&
        (!fullquad  ||  dots == 3)  &&  val <= (0xFFFFFFFFUL >> (dots << 3));
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


extern unsigned int SOCK_gethostbyname(const char* hostname)
{
    unsigned int host;
    char buf[256];

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    if (!hostname || !*hostname) {
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
        hints.ai_family = AF_INET; /* currently, we only handle IPv4 */
        if ((x_errno = getaddrinfo(hostname, 0, &hints, &out)) == 0  &&  out) {
            struct sockaddr_in* addr = (struct sockaddr_in *) out->ai_addr;
            assert(addr->sin_family == AF_INET);
            host = addr->sin_addr.s_addr;
        } else {
            if (s_Log == eOn) {
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOGF_ERRNO_EXX(105, eLOG_Warning,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("[SOCK_gethostbyname]  Failed "
                                     "getaddrinfo(\"%.64s\")", hostname));
            }
            host = 0;
        }
        if ( out ) {
            freeaddrinfo(out);
        }
#else /* use some variant of gethostbyname */
        struct hostent* he;
#  ifdef HAVE_GETHOSTBYNAME_R
        static const char suffix[] = "_r";
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
#    endif /*HAVE_GETHOSTNBYNAME_R == N*/
#  else
        static const char suffix[] = "";

        CORE_LOCK_WRITE;
        he = gethostbyname(hostname);
#    ifdef NCBI_OS_MAC
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (he && he->h_addrtype == AF_INET && he->h_length == sizeof(host)) {
            memcpy(&host, he->h_addr, sizeof(host));
        } else {
            host = 0;
            if ( he )
                x_errno = EINVAL;
        }

#  ifndef HAVE_GETHOSTBYNAME_R
        CORE_UNLOCK;
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (!host  &&  s_Log == eOn) {
#  ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            CORE_LOGF_ERRNO_EXX(106, eLOG_Warning,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("[SOCK_gethostbyname]  Failed "
                                 "gethostbyname%s(\"%.64s\")",
                                 suffix, hostname));
        }

#endif /*HAVE_GETADDR_INFO*/
    }

    return host;
}


extern char* SOCK_gethostbyaddr(unsigned int host,
                                char*        name,
                                size_t       namelen)
{
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    if ( !host ) {
        host = SOCK_gethostbyname(0);
    }

    if ( host ) {
        int x_errno;
#if defined(HAVE_GETNAMEINFO) && defined(EAI_SYSTEM)
        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET; /* currently, we only handle IPv4 */
        addr.sin_addr.s_addr = host;
#  ifdef HAVE_SIN_LEN
        addr.sin_len = sizeof(addr);
#  endif /*HAVE_SIN_LEN*/
        if ((x_errno = getnameinfo((struct sockaddr*) &addr, sizeof(addr),
                                   name, namelen, 0, 0, 0)) != 0  ||  !*name) {
            if (SOCK_ntoa(host, name, namelen) != 0) {
                if (!x_errno) {
#ifdef ENOSPC
                    x_errno = ENOSPC;
#else
                    x_errno = ERANGE;
#endif /*ENOSPC*/
                }
                name[0] = '\0';
                name = 0;
            }
            if (!name  &&  s_Log == eOn) {
                char addr[16];
                SOCK_ntoa(host, addr, sizeof(addr));
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOGF_ERRNO_EXX(107, eLOG_Warning,
                                    x_errno, SOCK_STRERROR(x_errno),
                                    ("[SOCK_gethostbyaddr]  Failed "
                                     "getnameinfo(%s)", addr));
            }
        }
        return name;

#else /* use some variant of gethostbyaddr */
        struct hostent* he;
#  if defined(HAVE_GETHOSTBYADDR_R)
        static const char suffix[] = "_r";
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
#    endif /*HAVE_GETHOSTBYADDR_R == N*/
#  else /*HAVE_GETHOSTBYADDR_R*/
        static const char suffix[] = "";

        CORE_LOCK_WRITE;
        he = gethostbyaddr((char*) &host, sizeof(host), AF_INET);
#    ifdef NCBI_OS_MAC
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!he  ||  strlen(he->h_name) >= namelen) {
            if (he  ||  SOCK_ntoa(host, name, namelen) != 0) {
#ifdef ENOSPC
                x_errno = ENOSPC;
#else
                x_errno = ERANGE;
#endif /*ENOSPC*/
                name[0] = '\0';
                name = 0;
            }
        } else {
            strcpy(name, he->h_name);
        }

#  ifndef HAVE_GETHOSTBYADDR_R
        CORE_UNLOCK;
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!name  &&  s_Log == eOn) {
            char addr[16];
#  ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            SOCK_ntoa(host, addr, sizeof(addr));
            CORE_LOGF_ERRNO_EXX(108, eLOG_Warning,
                                x_errno, SOCK_STRERROR(x_errno),
                                ("[SOCK_gethostbyaddr]  Failed "
                                 "gethostbyaddr%s(%s)", suffix, addr));
        }

        return name;

#endif /*HAVE_GETNAMEINFO*/
    }

    name[0] = '\0';
    return 0;
}


extern unsigned int SOCK_GetLoopbackAddress(void)
{
    return htonl(INADDR_LOOPBACK);
}


extern unsigned int SOCK_GetLocalHostAddress(ESwitch reget)
{
    if (reget == eOn  ||  (!s_LocalHostAddress  &&  reget != eOff))
        s_LocalHostAddress = SOCK_gethostbyname(0);
    return s_LocalHostAddress;
}


extern const char* SOCK_StringToHostPort(const char*     str,
                                         unsigned int*   host,
                                         unsigned short* port)
{
    unsigned short p;
    unsigned int h;
    char abuf[256];
    const char* s;
    size_t alen;
    int n = 0;

    if (host)
        *host = 0;
    if (port)
        *port = 0;
    for (s = str;  *s;  s++) {
        if (isspace((unsigned char)(*s))  ||  *s == ':')
            break;
    }
    if ((alen = (size_t)(s - str)) > sizeof(abuf) - 1)
        return str;
    if (alen) {
        strncpy0(abuf, str, alen);
        if (!(h = SOCK_gethostbyname(abuf)))
            return str;
    } else
        h = 0;
    if (*s == ':') {
        if (isspace((unsigned char)(*s))
            ||  sscanf(++s, "%hu%n", &p, &n) < 1
            ||  (s[n]  &&  !isspace((unsigned char) s[n])))
            return alen ? 0 : str;
    } else
        p = 0;
    if (host)
        *host = h;
    if (port)
        *port = p;
    return s + n;
}


extern size_t SOCK_HostPortToString(unsigned int   host,
                                    unsigned short port,
                                    char*          buf,
                                    size_t         buflen)
{
    char   x_buf[16/*sizeof("255.255.255.255")*/ + 6/*:port#*/];
    size_t n;

    if (!buf  ||  !buflen)
        return 0;
    if (!host)
        *x_buf = '\0';
    else if (SOCK_ntoa(host, x_buf, sizeof(x_buf)) != 0) {
        *buf = '\0';
        return 0;
    }
    n = strlen(x_buf);
    if (port  ||  !host)
        n += sprintf(x_buf + n, ":%hu", port);
    assert(n < sizeof(x_buf));
    if (n >= buflen)
        n  = buflen - 1;
    memcpy(buf, x_buf, n);
    buf[n] = '\0';
    return n;
}
