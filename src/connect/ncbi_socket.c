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
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  1999/10/19 22:21:48  vakatov
 * Put the call to "gethostbyname()" into a CRITICAL_SECTION
 *
 * Revision 6.2  1999/10/19 16:16:01  vakatov
 * Try the NCBI C and C++ headers only if NCBI_OS_{UNIX, MSWIN, MAC} is
 * not #define'd
 *
 * Revision 6.1  1999/10/18 15:36:38  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 * ===========================================================================
 */


/* Kludge to compile with the NCBI C or C++ toolkits -- when the platform
 * (NCBI_OS_{UNIX, MSWIN, MAC}) is not specified in the command-line
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  if defined(NCBI_C)
#    include <ncbilcl.h>
#    if defined(OS_UNIX)
#      define NCBI_OS_UNIX 1
#      if !defined(HAVE_GETHOSTBYNAME_R)  &&  defined(OS_UNIX_SOL)
#        define HAVE_GETHOSTBYNAME_R 1
#      endif
#    elif defined(OS_MSWIN)
#      define NCBI_OS_MSWIN 1
#    elif defined(OS_MAC)
#      define NCBI_OS_MAC 1
#    else
#      error "Unknown OS, must be one of OS_UNIX, OS_MSWIN, OS_MAC!"
#    endif
#  else /* else!NCBI_C */
#    include <ncbiconf.h>
#  endif /* NCBI_C */
#endif /* !NCBI_OS_{UNIX, MSWIN, MAC} */


/* Platform-specific system headers
 */
#if defined(NCBI_OS_UNIX)

#  include <errno.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <arpa/inet.h>
#  include <signal.h>

#elif defined(NCBI_OS_MSWIN)

#  include <winsock.h>

#elif defined(NCBI_OS_MAC)

#  include <macsockd.h>
#  define __TYPES__       /* avoid Mac <Types.h> */
#  define __MEMORY__      /* avoid Mac <Memory.h> */
#  define ipBadLapErr     /* avoid Mac <MacTCPCommonTypes.h> */
#  define APPL_SOCK_DEF
#  define SOCK_DEFS_ONLY
#  include <sock_ext.h>
extern void bzero(char* target, long numbytes);
#  include <netdb.h>
#  include <s_types.h>
#  include <s_socket.h>
#  include <s_ioctl.h>
#  include <neti_in.h>
#  include <a_inet.h>
#  include <s_time.h>
#  include <s_fcntl.h>
#  include <neterrno.h> /* missing error numbers on Mac */

/* cannot write more than SOCK_WRITE_SLICE at once on Mac; so we have to split
 * big output buffers into smaller(<SOCK_WRITE_SLICE) slices before writing
 * them to the socket
 */
#  define SOCK_WRITE_SLICE 2048

#else

#  error "Unsupported platform, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC !!!"

#endif /* platform-specific #include (UNIX, MSWIN, MAC) */


/* Portable standard C headers
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* Run-time debug macro
 */
#include <assert.h>
#if defined(assert)
#  define verify(expr)  assert(expr)
#else
#  define verify(expr)  (void)(expr)
#endif


/* NCBI headers
 */
#include "ncbi_buffer.h"
#include "ncbi_socket.h"



/******************************************************************************
 *  TYPEDEF & MACRO
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE 4096


/* Macro #def for the platform-dependent constants, error codes and functions
 */
#if defined(NCBI_OS_MSWIN)

typedef SOCKET TSOCK_Handle;
#  define SOCK_INVALID     INVALID_SOCKET
#  define SOCK_ERRNO       WSAGetLastError()
#  define SOCK_EINTR       WSAEINTR
#  define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#  define SOCK_ECONNRESET  WSAECONNRESET
#  define SOCK_EPIPE       WSAESHUTDOWN
#  define SOCK_EAGAIN      WSAEINPROGRESS
#  define SOCK_EINPROGRESS WSAEINPROGRESS
#  define SOCK_NFDS(s)     0
#  define SOCK_CLOSE(s)    closesocket(s)
/* NCBI_OS_MSWIN */

#elif defined(NCBI_OS_UNIX)

typedef int TSOCK_Handle;
#  define SOCK_INVALID     (-1)
#  define SOCK_ERRNO       errno
#  define SOCK_EINTR       EINTR
#  define SOCK_EWOULDBLOCK EWOULDBLOCK
#  define SOCK_ECONNRESET  ECONNRESET
#  define SOCK_EPIPE       EPIPE
#  define SOCK_EAGAIN      EAGAIN
#  define SOCK_EINPROGRESS EINPROGRESS
#  define SOCK_NFDS(s)     (s + 1)
#  define SOCK_CLOSE(s)    close(s)
#  ifndef INADDR_NONE
#    define INADDR_NONE    (unsigned int)(-1)
#  endif
/* NCBI_OS_UNIX */

#elif defined(NCBI_OS_MAC)

typedef int TSOCK_Handle;
#  define SOCK_INVALID     (-1)
#  ifndef SOCK_ERRNO
#    define SOCK_ERRNO     errno
#  endif
#  define SOCK_EINTR       EINTR
#  define SOCK_EWOULDBLOCK EWOULDBLOCK
#  define SOCK_ECONNRESET  ECONNRESET
#  define SOCK_EPIPE       EPIPE
#  define SOCK_EAGAIN      EAGAIN
#  define SOCK_EINPROGRESS EINPROGRESS
#  define SOCK_NFDS(s)     (s + 1)
#  define SOCK_CLOSE(s)    close(s)

/* (but see ni_lib.c line 2508 for gethostname substitute for Mac) */
extern int gethostname(char* machname, long buflen);

#endif /* NCBI_OS_MSWIN, NCBI_OS_UNIX, NCBI_OS_MAC */


/* Listening socket
 */
typedef struct LSOCK_tag {
    TSOCK_Handle sock;  /* OS-specific socket handle */
} LSOCK_struct;


/* Socket
 */
typedef struct SOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle */
    struct timeval* r_timeout;  /* zero (if infinite), or points to "r_tv" */
    struct timeval  r_tv;       /* finite read timeout value */
    SSOCK_Timeout   r_to;       /* finite read timeout value (aux., temp.) */
    struct timeval* w_timeout;  /* zero (if infinite), or points to "w_tv" */
    struct timeval  w_tv;       /* finite write timeout value */
    SSOCK_Timeout   w_to;       /* finite write timeout value (aux., temp.) */
    unsigned int    host;       /* peer host (in the network byte order) */
    unsigned short  port;       /* peer port (in the network byte order) */
    BUF             buf;        /* read buffer */
    int/*bool*/     is_eof;     /* if EOF has been detected (on read) */
} SOCK_struct;



/******************************************************************************
 *  Multi-Thread SAFETY
 */

static FSOCK_MT_CSection s_MT_CS_Func;
static void*             s_MT_CS_Data;

extern void SOCK_MT_SetCriticalSection
(FSOCK_MT_CSection func,
 void*             data)
{
  if (s_MT_CS_Func  &&  data != s_MT_CS_Data)
    (*s_MT_CS_Func)(s_MT_CS_Data, eSOCK_MT_Cleanup);

  if (s_MT_CS_Func != func)
    s_MT_CS_Func = func;
  if (s_MT_CS_Data != data)
    s_MT_CS_Data = data;
}


#define BEGIN_CRITICAL_SECTION  do { \
  if (s_MT_CS_Func)  (*s_MT_CS_Func)(s_MT_CS_Data, eSOCK_MT_Lock  ); } while(0)
#define END_CRITICAL_SECTION  do { \
  if (s_MT_CS_Func)  (*s_MT_CS_Func)(s_MT_CS_Data, eSOCK_MT_Unlock); } while(0)



/******************************************************************************
 *  ERROR HANDLING
 */


extern const char* SOCK_StatusStr(ESOCK_Status status)
{
    static const char* s_StatusStr[eSOCK_Unknown+1] = {
        "Success",
        "Timeout",
        "Closed",
        "Unknown"
    };

    return s_StatusStr[status];
}


extern const char* SOCK_PostSeverityStr(ESOCK_PostSeverity sev)
{
    static const char* s_PostSeverityStr[eSOCK_PostError+1] = {
        "<WARNING>",
        "<ERROR>"
    };

    return s_PostSeverityStr[sev];
}


static FSOCK_ErrHandler s_ErrHandler;
static void*            s_ErrData;
static FSOCK_ErrCleanup s_ErrCleanup;


static void s_PostStr(ESOCK_PostSeverity sev,
                      const char*        message)
{ /* message */
    BEGIN_CRITICAL_SECTION;

    if ( s_ErrHandler )
        (*s_ErrHandler)(s_ErrData, message, sev);

    END_CRITICAL_SECTION;
}


static void s_PostErrno(ESOCK_PostSeverity sev,
                        const char*        message,
                        int                x_errno)
{ /* message: errno */
    BEGIN_CRITICAL_SECTION;

    if ( !s_ErrHandler )
        return;

    {{
        char   str[256];
        size_t len = strlen(message);
        if (len > sizeof(str) - 32)
            len = sizeof(str) - 32;

        memcpy(str, message, len);
        sprintf(str + len, ": errno = %d", x_errno);
        (*s_ErrHandler)(s_ErrData, str, sev);
    }}

    END_CRITICAL_SECTION;
}


extern void SOCK_SetErrHandler(FSOCK_ErrHandler func,
                               void*            data,
                               FSOCK_ErrCleanup cleanup)
{
    BEGIN_CRITICAL_SECTION;

    /* cleanup -- if the callback data has changed */
    if (s_ErrCleanup  &&  data != s_ErrData) {
        (*s_ErrCleanup)(s_ErrData);
        s_ErrCleanup = 0;
        s_ErrData    = 0;
    }
    /* set new err handling properties */
    s_ErrHandler = func;
    s_ErrData    = data;
    s_ErrCleanup = cleanup;

    END_CRITICAL_SECTION;
}

#if defined(__cplusplus)
extern "C" {
  static void s_StreamErrHandler(void*              data,
                                 const char*        message,
                                 ESOCK_PostSeverity sev);
}
#endif /* __cplusplus */

static void s_StreamErrHandler(void*              data,
                               const char*        message,
                               ESOCK_PostSeverity sev)
{
    FILE* stream = (FILE*)data;
    fprintf(stream, "%9s %s\n", SOCK_PostSeverityStr(sev), message);
    fflush(stream);
}


extern void SOCK_SetErrStream(FILE*            stream,
                              FSOCK_ErrCleanup cleanup)
{
    SOCK_SetErrHandler(s_StreamErrHandler, stream, cleanup);
}



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


static int/*bool*/ s_Initialized = 0/*false*/;


extern ESOCK_Status SOCK_InitializeAPI(void)
{
    ESOCK_Status status = eSOCK_Success;

    BEGIN_CRITICAL_SECTION;
    if ( !s_Initialized ) {
        s_Initialized = 1 /*true*/;
#if defined(NCBI_OS_MSWIN)
        {{
            WSADATA wsadata;
            int x_errno = WSAStartup(MAKEWORD(1,1), &wsadata);
            if (x_errno != 0) {
                s_PostErrno(eSOCK_PostError,
                            "[SOCK_InitializeAPI]  Failed WSAStartup()",
                            x_errno);
                status = eSOCK_Unknown;
            }
        }}
#elif defined(NCBI_OS_UNIX)
        signal(SIGPIPE, SIG_IGN);
#endif
    }
    END_CRITICAL_SECTION;

    return status;
}


extern ESOCK_Status SOCK_ShutdownAPI(void)
{
    ESOCK_Status status = eSOCK_Success;

    BEGIN_CRITICAL_SECTION;
    if ( s_Initialized ) {
        s_Initialized = 0 /*false*/;
#if defined(NCBI_OS_MSWIN)
        if (WSACleanup() != 0) {
            s_PostErrno(eSOCK_PostWarning,
                        "[SOCK_ShutdownAPI]  Failed WSACleanup()", SOCK_ERRNO);
            status = eSOCK_Unknown;
        }
#endif
    }
    END_CRITICAL_SECTION;

    SOCK_SetErrHandler(0,0,0);
    SOCK_MT_SetCriticalSection(0,0);
    return status;
}



/******************************************************************************
 *  LSOCK & SOCK AUXILIARES
 */


/* SSOCK_Timeout <--> struct timeval  conversions
 */
static SSOCK_Timeout *s_tv2to(const struct timeval* tv,
                              SSOCK_Timeout*        to)
{
    if ( !tv )
        return 0;

    to->sec  = (unsigned int) tv->tv_sec;
    to->usec = (unsigned int) tv->tv_usec;
    return to;
}

static struct timeval* s_to2tv(const SSOCK_Timeout* to,
                               struct timeval*      tv)
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
#elif defined(NCBI_OS_UNIX)
    return (fcntl(sock, F_SETFL,
                  nonblock ?
                  fcntl(sock, F_GETFL, 0) | O_NONBLOCK :
                  fcntl(sock, F_GETFL, 0) & (int)~O_NONBLOCK) != -1);
#elif defined(NCBI_OS_MAC)
    return (fcntl(sock, F_SETFL,
                  nonblock ?
                  fcntl(sock, F_GETFL, 0) | O_NDELAY :
                  fcntl(sock, F_GETFL, 0) & (int)~O_NDELAY) != -1);
#else
    assert(0);
    return 0/*false*/;
#endif
}


/* Select on the socket i/o
 */
static ESOCK_Status s_Select(TSOCK_Handle          sock,
                             ESOCK_Direction       dir,
                             const struct timeval* timeout)
{
    int n_dfs;
    fd_set fds, *r_fds, *w_fds;

    /* just checking */
    if (sock == SOCK_INVALID) {
        s_PostStr(eSOCK_PostError,
                  "[SOCK::s_Select]  Attempted to wait on an invalid socket");
        assert(0);
        return eSOCK_Unknown;
    }

    /* setup i/o descriptor to select */
    r_fds = (dir == eSOCK_OnRead  ||  dir == eSOCK_OnReadWrite) ? &fds : 0;
    w_fds = (dir == eSOCK_OnWrite ||  dir == eSOCK_OnReadWrite) ? &fds : 0;

    do { /* auto-resume if interrupted by a signal */
        fd_set e_fds;
        struct timeval tmout;
        if ( timeout )
            tmout = *timeout;
        FD_ZERO(&fds);       FD_ZERO(&e_fds);
        FD_SET(sock, &fds);  FD_SET(sock, &e_fds);
        n_dfs = select(SOCK_NFDS(sock), r_fds, w_fds, &e_fds,
                       timeout ? &tmout : 0);
        assert(-1 <= n_dfs  &&  n_dfs <= 2);
        if ((n_dfs < 0  &&  SOCK_ERRNO != SOCK_EINTR)  ||
            FD_ISSET(sock, &e_fds))
            return eSOCK_Unknown;
    } while (n_dfs < 0);

    /* timeout has expired */
    if (n_dfs == 0)
        return eSOCK_Timeout;

    /* success;  can i/o now */
    assert(FD_ISSET(sock, &fds));
    return eSOCK_Success;
}



/******************************************************************************
 *  LISTENING SOCKET
 */


extern ESOCK_Status LSOCK_Create(unsigned short port,
                                 unsigned short backlog,
                                 LSOCK*         lsock)
{
    TSOCK_Handle       x_lsock;
    struct sockaddr_in addr;

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eSOCK_Success);

    /* Create new(listening) socket */
    if ((x_lsock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        s_PostErrno(eSOCK_PostError,
                    "[LSOCK_Create]  Cannot create listening socket",
                    SOCK_ERRNO);
        return eSOCK_Unknown;
    }

#ifdef NCBI_OS_UNIX
    {{
        /* Let more than one "bind()" to use the same address.
         * It was affirmed(?) that at least under Solaris 2.5 this precaution:
         * 1) makes the address to be released immediately after the process
         *    termination
         * 2) still issue EADDINUSE error on the attempt to bind() to the
         *    same address being in-use by a living process(if SOCK_STREAM)
         */
        int reuse_addr = 1;
        if (setsockopt(x_lsock, SOL_SOCKET, SO_REUSEADDR, 
                       (const char*) &reuse_addr, sizeof(reuse_addr)) != 0) {
            s_PostErrno(eSOCK_PostError,
                        "[LSOCK_Create]  Failed setsockopt(REUSEADDR)",
                        SOCK_ERRNO);
            SOCK_CLOSE(x_lsock);
            return eSOCK_Unknown;
        }
    }}
#endif

    /* Bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
    if (bind(x_lsock, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
        s_PostErrno(eSOCK_PostError,
                    "[LSOCK_Create]  Failed bind()", SOCK_ERRNO);
        SOCK_CLOSE(x_lsock);
        return eSOCK_Unknown;
    }

    /* Listen */
    if (listen(x_lsock, backlog) != 0) {
        s_PostErrno(eSOCK_PostError,
                    "[LSOCK_Create]  Failed listen()", SOCK_ERRNO);
        SOCK_CLOSE(x_lsock);
        return eSOCK_Unknown;
    }

    /* Set to non-blocking mode */
    if ( !s_SetNonblock(x_lsock, 1/*true*/) ) {
        s_PostStr(eSOCK_PostError,
                  "[LSOCK_Create]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_lsock);
        return eSOCK_Unknown;
    }

    /* Success... */
    *lsock = (LSOCK) calloc(1, sizeof(LSOCK_struct));
    (*lsock)->sock = x_lsock;
    return eSOCK_Success;
}


extern ESOCK_Status LSOCK_Accept(LSOCK                lsock,
                                 const SSOCK_Timeout* timeout,
                                 SOCK*                sock)
{
    TSOCK_Handle   x_sock;
    unsigned int   x_host;
    unsigned short x_port;

    {{ /* wait for the connection request to come (up to timeout) */
        struct timeval tv;
        ESOCK_Status status =
            s_Select(lsock->sock, eSOCK_OnRead, s_to2tv(timeout, &tv));
        if (status != eSOCK_Success)
            return status;
    }}

    {{ /* accept next connection */
        struct sockaddr_in addr;
#ifdef NCBI_OS_MAC
        long addrlen = sizeof(struct sockaddr);
#else
        int addrlen = sizeof(struct sockaddr);
#endif
        if ((x_sock = accept(lsock->sock, (struct sockaddr *)&addr, &addrlen))
            == SOCK_INVALID) {
            s_PostErrno(eSOCK_PostError,
                        "[LSOCK_Accept]  Failed accept()", SOCK_ERRNO);
            return eSOCK_Unknown;
        }
        x_host = addr.sin_addr.s_addr;
        x_port = addr.sin_port;
    }}

    /* success:  create new SOCK structure */
    *sock = (SOCK) calloc(1, sizeof(SOCK_struct));
    (*sock)->sock = x_sock;
    verify(SOCK_SetTimeout(*sock, eSOCK_OnReadWrite, 0) == eSOCK_Success);
    (*sock)->host = x_host;
    (*sock)->port = x_port;
    BUF_SetChunkSize(&(*sock)->buf, SOCK_BUF_CHUNK_SIZE);
    return eSOCK_Success;
}


extern ESOCK_Status LSOCK_Close(LSOCK lsock)
{
    /* Set the socket back to blocking mode */
    if ( !s_SetNonblock(lsock->sock, 0/*false*/) ) {
        s_PostStr(eSOCK_PostWarning,
                  "[LSOCK_Close]  Cannot set socket back to blocking mode");
    }

    /* Close (persistently -- retry if interrupted by a signal) */
    for (;;) {
        /* success */
        if (SOCK_CLOSE(lsock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            s_PostErrno(eSOCK_PostError,
                        "[LSOCK_Close]  Failed close()", SOCK_ERRNO);
            return eSOCK_Unknown;
        }
    }

    /* success:  cleanup & return */
    free(lsock);
    return eSOCK_Success;
}


extern ESOCK_Status LSOCK_GetOSHandle
(LSOCK  lsock,
 void*  handle_buf,
 size_t handle_size)
{
    if (handle_size != sizeof(lsock->sock)) {
        s_PostStr(eSOCK_PostError, "[LSOCK_GetOSHandle]  Invalid handle size");
        assert(0);
        return eSOCK_Unknown;
    }

    memcpy(handle_buf, &lsock->sock, handle_size);
    return eSOCK_Success;
}



/******************************************************************************
 *  SOCKET
 */


/* Connect the (pre-allocated) socket to the specified "host:port" peer.
 * HINT: if "host" is NULL then assume(!) that the "sock" already exists,
 *       and connect to the same host;  the same is for zero "port".
 */
static ESOCK_Status s_Connect(SOCK                 sock,
                              const char*          host,
                              unsigned short       port,
                              const SSOCK_Timeout* timeout)
{
    TSOCK_Handle   x_sock;
    unsigned int   x_host;
    unsigned short x_port;

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eSOCK_Success);

    /* Get address of the remote host (assume the same host if it is NULL) */
    assert(host  ||  sock->host);
    x_host = host ? inet_addr(host) : sock->host;
    if (x_host == INADDR_NONE) {
        struct hostent *hp;
#if defined(HAVE_GETHOSTBYNAME_R)
        struct hostent x_hp;
        char x_buf[1024];
        int  x_err;
        hp = gethostbyname_r(host, &x_hp, x_buf, sizeof(x_buf), &x_err);
        if ( hp )
          memcpy(&x_host, hp->h_addr, sizeof(x_host));
#else
        BEGIN_CRITICAL_SECTION;
        hp = gethostbyname(host);
        if ( hp )
          memcpy(&x_host, hp->h_addr, sizeof(x_host));
        END_CRITICAL_SECTION;
#endif
        if ( !hp ) {
            if ( s_ErrHandler ) {
                char str[128];
                sprintf(str, "[SOCK::s_Connect]  Failed gethostbyname(%.64s)",
                        host ? host : "");
                s_PostStr(eSOCK_PostError, str);
            }
            return eSOCK_Unknown;
        }
    }

    /* Set the port to connect to(assume the same port if "port" is zero) */
    assert(port  ||  sock->port);
    x_port = (unsigned short) (port ? htons(port) : sock->port);

    /* Fill out the "server" struct */
    memcpy(&server.sin_addr, &x_host, sizeof(x_host));
    server.sin_family = AF_INET;
    server.sin_port   = x_port;

    /* Create new socket */
    if ((x_sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        s_PostErrno(eSOCK_PostError,
                    "[SOCK::s_Connect]  Cannot create socket", SOCK_ERRNO);
        return eSOCK_Unknown;
    }

    /* Set the socket i/o to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        s_PostStr(eSOCK_PostError,
                  "[SOCK::s_Connect]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_sock);
        return eSOCK_Unknown;
    }

    /* Establish connection to the peer */
    if (connect(x_sock, (struct sockaddr*) &server, sizeof(server)) != 0) {
        if (SOCK_ERRNO != SOCK_EINTR  &&  SOCK_ERRNO != SOCK_EINPROGRESS  &&
            SOCK_ERRNO != SOCK_EWOULDBLOCK) {
            if ( s_ErrHandler ) {
                char str[256];
                sprintf(str,
                        "[SOCK::s_Connect]  Failed connect() to %.64s:%d",
                        host ? host : "???", (int)ntohs(x_port));
                s_PostErrno(eSOCK_PostError, str, SOCK_ERRNO);
            }
            SOCK_CLOSE(x_sock);
            return eSOCK_Unknown; /* unrecoverable error */
        }

        /* The connect could be interrupted by a signal or just cannot be
         * established immediately;  yet, the connect must have been in progess
         * (asynchroneous), so wait here for it to succeed (become writeable).
         */
        {{
            struct timeval tv;
            ESOCK_Status status =
                s_Select(x_sock, eSOCK_OnWrite,s_to2tv(timeout, &tv));
            if (status != eSOCK_Success) {
                if ( s_ErrHandler ) {
                    char str[256];
                    sprintf(str,
                            "[SOCK::s_Connect]  Failed pending connect to "
                            "%.64s:%d (%.32s)",
                            host ? host : "???", (int)ntohs(x_port),
                            SOCK_StatusStr(status));
                    s_PostStr(eSOCK_PostError, str);
                    SOCK_CLOSE(x_sock);
                    return status;
                }
            }
        }}
    }

    /* Success */
    /* NOTE:  it does not change the timeouts and the data buffer content */
    sock->sock   = x_sock;
    sock->host   = x_host;
    sock->port   = x_port;
    sock->is_eof = 0/*false*/;

    return eSOCK_Success;
}


/* Shutdown the socket (close its system file descriptor)
 */
static ESOCK_Status s_Shutdown(SOCK sock)
{
    /* set EOF flag */
    sock->is_eof = 1/*true*/;

  /* Just checking */
    if (sock->sock == SOCK_INVALID) {
        s_PostStr(eSOCK_PostWarning,
                  "[SOCK::s_Shutdown]  Attempt to shutdown an invalid socket");
        return eSOCK_Unknown;
    }

    {{ /* Reset the auxiliary data buffer */
        size_t buf_size = BUF_Size(sock->buf);
        if (BUF_Read(sock->buf, 0, buf_size) != buf_size) {
            s_PostStr(eSOCK_PostError,
                      "[SOCK::s_Shutdown]  Cannot reset aux. data buffer");
            return eSOCK_Unknown;
        }
    }}

    /* Set the socket back to blocking mode */
    if ( !s_SetNonblock(sock->sock, 0/*false*/) ) {
        s_PostStr(eSOCK_PostError,
                  "[SOCK::s_Shutdown]  Cannot set socket to blocking mode");
    }

    /* Set the close()'s linger period be equal to the write timeout */
    if ( sock->w_timeout ) {
        struct linger lgr;
        lgr.l_onoff  = 1;
        lgr.l_linger = sock->w_timeout->tv_sec ? sock->w_timeout->tv_sec : 1;
        if (setsockopt(sock->sock, SOL_SOCKET, SO_LINGER, 
                       (char*) &lgr, sizeof(lgr)) != 0) {
            s_PostErrno(eSOCK_PostWarning,
                        "[SOCK::s_Shutdown]  Failed setsockopt()", SOCK_ERRNO);
        }
    }   

    /* Close (persistently -- retry if interrupted by a signal) */
    for (;;) {
        /* success */
        if (SOCK_CLOSE(sock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            s_PostErrno(eSOCK_PostWarning,
                        "[SOCK::s_Shutdown]  Failed close()", SOCK_ERRNO);
            sock->sock = SOCK_INVALID;
            return (SOCK_ERRNO == SOCK_ECONNRESET || SOCK_ERRNO == SOCK_EPIPE)
                ? eSOCK_Closed : eSOCK_Unknown;
        }
    }

    /* Success */
    sock->sock = SOCK_INVALID;
    return eSOCK_Success;
}


/* Emulate "peek" using the NCBI data buffering.
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves)
 */
static int s_NCBI_Recv(SOCK          sock,
                       void*         buffer,
                       size_t        size,
                       int  /*bool*/ peek,
                       int* /*bool*/ read_error)
{
    char*  x_buffer = (char*) buffer;
    size_t n_readbuf;
    int    n_readsock;

    *read_error = 0/*false*/;

    /* read(or peek) from the internal buffer */
    n_readbuf = peek ?
        BUF_Peek(sock->buf, x_buffer, size) :
        BUF_Read(sock->buf, x_buffer, size);
    if (n_readbuf == size)
        return (int)n_readbuf;
    x_buffer += n_readbuf;
    size     -= n_readbuf;

    /* read(dont peek) from the socket */
    n_readsock = recv(sock->sock, x_buffer, size, 0);
    if (n_readsock <= 0) {
        *read_error = 1/*true*/;
        return (int)(n_readbuf ? n_readbuf : n_readsock);
    }

    /* if "peek" -- store the new read data in the internal buffer */
    if ( peek )
        verify(BUF_Write(&sock->buf, x_buffer, n_readsock));

    return (int)(n_readbuf + n_readsock);
}


/* Read/Peek data from the socket
 */
static ESOCK_Status s_Recv(SOCK        sock,
                           void*       buf,
                           size_t      size,
                           size_t*     n_read,
                           int/*bool*/ peek)
{
    int/*bool*/ read_error;
    int         x_errno;

    /* just checking */
    if (sock->sock == SOCK_INVALID) {
        s_PostStr(eSOCK_PostError,
                  "[SOCK::s_Recv]  Attempted to read from an invalid socket");
        assert(0);
        return eSOCK_Unknown;
    }

    *n_read = 0;
    for (;;) {
        /* try to read */
        int buf_read = s_NCBI_Recv(sock, buf, size, peek, &read_error);
        if (buf_read > 0) {
            assert(buf_read <= (int)size);
            *n_read = buf_read;
            if ( read_error ) {
                x_errno = SOCK_ERRNO;
                if (x_errno != SOCK_EWOULDBLOCK  &&  x_errno != SOCK_EAGAIN  &&
                    x_errno != SOCK_EINTR)
                    sock->is_eof = 1/*true*/;
            }
            return eSOCK_Success; /* success */
        }

        x_errno = SOCK_ERRNO;

        if (buf_read == 0) {
            /* NOTE: an empty message may cause the same effect, and it does */
            /* not (!) get discarded from the input queue; therefore, the    */
            /* subsequent attempts to read will cause just the same effect)  */
            sock->is_eof = 1/*true*/;
            return eSOCK_Closed;  /* the connection has been closed by peer */
        }

        /* blocked -- wait for a data to come;  exit if timeout/error */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            ESOCK_Status status =
                s_Select(sock->sock, eSOCK_OnRead, sock->r_timeout);
            if (status != eSOCK_Success)
                return status;
            continue;
        }

        /* retry if interrupted by a signal */
        if (x_errno == SOCK_EINTR)
            continue;

        /* forcibly closed by peer, or shut down */
        if (x_errno == SOCK_ECONNRESET  ||  x_errno == SOCK_EPIPE) {
            sock->is_eof = 1/*true*/;
            return eSOCK_Closed;
        }

#if defined(NCBI_OS_MAC)
        if (buf_read == -1) {
            sock->is_eof = 1/*true*/;
            return eSOCK_Closed;
        }
#endif

        /* dont want to handle all possible errors... let them be "unknown" */
        /* and assume EOF */
        sock->is_eof = 1/*true*/;
        break;
    }

    return eSOCK_Unknown;
}


/* Write data to the socket "as is" (the whole buffer at once)
 */
static ESOCK_Status s_WriteWhole(SOCK        sock,
                                 const void* buf,
                                 size_t      size,
                                 size_t*     n_written)
{
    const char* x_buf  = (const char*) buf;
    int         x_size = (int) size;
    int         x_errno;

    if ( n_written )
        *n_written = 0;
    if ( !size )
        return eSOCK_Success;

    for (;;) {
        /* try to write */
        int buf_written = send(sock->sock, (char*)x_buf, x_size, 0);
        if (buf_written >= 0) {
            if ( n_written )
                *n_written += buf_written;
            if (buf_written == x_size)
                return eSOCK_Success; /* all data has been successfully sent */
            x_buf  += buf_written;
            x_size -= buf_written;
            assert(x_size > 0);
            continue; /* there is unsent data */
        }
        x_errno = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout is expired */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            ESOCK_Status status =
                s_Select(sock->sock, eSOCK_OnWrite, sock->w_timeout);
            if (status != eSOCK_Success)
                return status;
            continue;
        }

        /* retry if interrupted by a signal */
        if (x_errno == SOCK_EINTR)
            continue;

        /* forcibly closed by peer, or shut down */
        if (x_errno == SOCK_ECONNRESET  ||  x_errno == SOCK_EPIPE)
            return eSOCK_Closed;

        /* dont want to handle all possible errors... let it be "unknown" */  
        break;
    }
    return eSOCK_Unknown;
}


#if defined(SOCK_WRITE_SLICE)
/* Split output buffer by slices (of size <= SOCK_WRITE_SLICE) before writing
 * to the socket
 */
static ESOCK_Status s_WriteSliced(SOCK        sock,
                                  const void* buf,
                                  size_t      size,
                                  size_t*     n_written)
{
    ESOCK_Status status    = eSOCK_Success;
    size_t       x_written = 0;

    while (size  &&  status == eSOCK_Success) {
        size_t n_io = (size > SOCK_WRITE_SLICE) ? SOCK_WRITE_SLICE : size;
        size_t n_io_done;
        status = s_WriteWhole(sock, (char*)buf + x_written, n_io, &n_io_done);
        if ( n_io_done ) {
            x_written += n_io_done;
            size      -= n_io_done;
        }
    }

    if ( n_written )
        *n_written = x_written;
    return status;
}
#endif /* SOCK_WRITE_SLICE */



extern ESOCK_Status SOCK_Create(const char*          host,
                                unsigned short       port,
                                const SSOCK_Timeout* timeout,
                                SOCK*                sock)
{
    /* Allocate memory for the internal socket structure */
    SOCK x_sock = (SOCK_struct*) calloc(1, sizeof(SOCK_struct));

  /* Connect */
    ESOCK_Status status = s_Connect(x_sock, host, port, timeout);
    if (status != eSOCK_Success) {
        free(x_sock);
        return status;
    }

    /* Setup the input data buffer properties */
    BUF_SetChunkSize(&x_sock->buf, SOCK_BUF_CHUNK_SIZE);

    /* Success */
    *sock = x_sock;
    return eSOCK_Success;
}


extern ESOCK_Status SOCK_Reconnect(SOCK                 sock,
                                   const char*          host,
                                   unsigned short       port,
                                   const SSOCK_Timeout* timeout)
{
    /* Close the socket, if necessary */
    if (sock->sock != SOCK_INVALID)
        s_Shutdown(sock);

  /* Connect */
    return s_Connect(sock, host, port, timeout);
}


extern ESOCK_Status SOCK_Close(SOCK sock)
{
    ESOCK_Status status = s_Shutdown(sock);
    BUF_Destroy(sock->buf);
    free(sock);
    return status;
}


extern ESOCK_Status SOCK_Wait(SOCK                 sock,
                              ESOCK_Direction      dir,
                              const SSOCK_Timeout* timeout)
{
    struct timeval tv;
    return s_Select(sock->sock, dir, s_to2tv(timeout, &tv));
}


extern ESOCK_Status SOCK_SetTimeout(SOCK                 sock,
                                    ESOCK_Direction      dir,
                                    const SSOCK_Timeout* timeout)
{
    switch ( dir ) {
    case eSOCK_OnRead:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        break;
    case eSOCK_OnWrite:
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    case eSOCK_OnReadWrite:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    default:
        assert(0);  return eSOCK_Unknown;
    }

    return eSOCK_Success;
}


extern const SSOCK_Timeout* SOCK_GetReadTimeout(SOCK sock)
{
    return s_tv2to(sock->r_timeout, &sock->r_to);
}


extern const SSOCK_Timeout* SOCK_GetWriteTimeout(SOCK sock)
{
    return s_tv2to(sock->w_timeout, &sock->w_to);
}


extern ESOCK_Status SOCK_Read(SOCK       sock,
                              void*      buf,
                              size_t     size,
                              size_t*    n_read,
                              ESOCK_Read how)
{
    switch ( how ) {
    case eSOCK_Read: {
        return s_Recv(sock, buf, size, n_read, 0/*false*/);
    }

    case eSOCK_Peek: {
        return s_Recv(sock, buf, size, n_read, 1/*true*/);
    }

    case eSOCK_Persist: {
        ESOCK_Status status = eSOCK_Success;
        for (*n_read = 0;  size; ) {
            size_t x_read;
            status = SOCK_Read(sock, (char*)buf + *n_read, size, &x_read,
                               eSOCK_Read);
            if (status != eSOCK_Success)
                return status;

            *n_read += x_read;
            size    -= x_read;
        }
        return eSOCK_Success;
    }
    }

    assert(0);
    return eSOCK_Unknown;
}


extern ESOCK_Status SOCK_PushBack(SOCK        sock,
                                  const void* buf,
                                  size_t      size)
{
    return BUF_PushBack(&sock->buf, buf, size) ? eSOCK_Success : eSOCK_Unknown;
}


extern int/*bool*/ SOCK_Eof(SOCK sock)
{
    return sock->is_eof;
}


extern ESOCK_Status SOCK_Write(SOCK        sock,
                               const void* buf,
                               size_t      size,
                               size_t*     n_written)
{
    /* just checking */
    if (sock->sock == SOCK_INVALID) {
        s_PostStr(eSOCK_PostError,
                  "[SOCK_Write]  Attempted to write to an invalid socket");
        assert(0);
        return eSOCK_Unknown;
    }

    /* write */
#if defined(SOCK_WRITE_SLICE)
    return s_WriteSliced(sock, buf, size, n_written);
#else
    return s_WriteWhole (sock, buf, size, n_written);
#endif
}


extern void SOCK_GetAddress(SOCK            sock,
                            unsigned int*   host,
                            unsigned short* port,
                            int/*bool*/     network_byte_order) {
    if ( host ) {
        *host = (unsigned int)
            (network_byte_order ? sock->host : ntohl(sock->host));
    }

    if ( port ) {
        *port = (unsigned short)
            (network_byte_order ? sock->port : ntohs(sock->port));
    }
}


extern ESOCK_Status SOCK_GetOSHandle
(SOCK   sock,
 void*  handle_buf,
 size_t handle_size)
{
    if (handle_size != sizeof(sock->sock)) {
        s_PostStr(eSOCK_PostError, "[SOCK_GetOSHandle]  Invalid handle size!");
        assert(0);
        return eSOCK_Unknown;
    }

    memcpy(handle_buf, &sock->sock, handle_size);
    return eSOCK_Success;
}



extern int/*bool*/ SOCK_gethostname(char*  name,
                                    size_t namelen)
{
    int x_errno;
    assert(namelen > 0);
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eSOCK_Success);
    name[0] = name[namelen-1] = '\0';
    x_errno = gethostname(name, (int)namelen);
    if (x_errno  ||  name[namelen-1]) {
        s_PostErrno(eSOCK_PostError,
                    "[SOCK_gethostname]  Cannot get local hostname", x_errno);
        name[0] = '\0';
        return 1 /*error*/;
    }
    return 0 /*success*/;
}


extern int/*bool*/ SOCK_host2inaddr(unsigned int host,
                                    char*        buf,
                                    size_t       buflen)
{
    struct in_addr addr_struct;
    char*          addr_string;

    if ( !buf )
        return 0/*false*/;

    BEGIN_CRITICAL_SECTION;
    addr_struct.s_addr = host;
    addr_string = inet_ntoa(addr_struct);
    if (!addr_string  ||  strlen(addr_string) >= buflen) {
        char str[96];
        sprintf(str, "[SOCK_host2inaddr]  Cannot convert %x", host);
        s_PostStr(eSOCK_PostError, str);
        buf[0] = '\0';
        return 0/*false*/;
    }
    strcpy(buf, addr_string);
    END_CRITICAL_SECTION;

    return 1/*true*/;
}


extern unsigned int SOCK_htonl(unsigned int value)
{
  return htonl(value);
}
