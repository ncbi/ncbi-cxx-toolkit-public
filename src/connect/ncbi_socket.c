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
 * ---------------------------------------------------------------------------
 * $Log$
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


/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC!"
#endif


/* Uncomment this (or specify "-DHAVE_GETHOSTBY***_R=") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "gethostbyname_r()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
 */

/*   Solaris: */
/* #define HAVE_GETHOSTBYNAME_R 5 */
/* #define HAVE_GETHOSTBYADDR_R 7 */

/*   Linux, IRIX: */
/* #define HAVE_GETHOSTBYNAME_R 6 */
/* #define HAVE_GETHOSTBYADDR_R 8 */



/* Platform-specific system headers
 */
#if defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <sys/time.h>
#  include <unistd.h>
#  include <netdb.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <signal.h>

#elif defined(NCBI_OS_MSWIN)
#  include <winsock2.h>

#elif defined(NCBI_OS_MAC)
#  include <sock_ext.h>
extern void bzero(char* target, long numbytes);
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
#include <stdlib.h>
#include <stdio.h>
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
#  define SOCK_CLOSE(s)       close(s)
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

/* (but see ni_lib.c line 2508 for gethostname substitute for Mac) */
#ifndef NCBI_OS_MAC
extern int gethostname(char* machname, long buflen);
#endif

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
    STimeout        r_to;       /* finite read timeout value (aux., temp.) */
    struct timeval* w_timeout;  /* zero (if infinite), or points to "w_tv" */
    struct timeval  w_tv;       /* finite write timeout value */
    STimeout        w_to;       /* finite write timeout value (aux., temp.) */
    struct timeval* c_timeout;  /* zero (if infinite), or points to "c_tv" */
    struct timeval  c_tv;       /* finite close timeout value */
    STimeout        c_to;       /* finite close timeout value (aux., temp.) */
    unsigned int    host;       /* peer host (in the network byte order) */
    unsigned short  port;       /* peer port (in the network byte order) */
    ESwitch         r_on_w;     /* enable/disable automatic read-on-write */
    BUF             buf;        /* read buffer */

    /* current status and EOF indicator */
    int/*bool*/  is_eof;    /* if EOF has been detected (on read) */
    EIO_Status   r_status;  /* read  status:  eIO_Closed if was shutdown */
    EIO_Status   w_status;  /* write status:  eIO_Closed if was shutdown */

    /* for the tracing/statistics */
    ESwitch      log_data;
    unsigned int id;        /* the internal ID (see also "s_ID_Counter") */
    size_t       n_read;
    size_t       n_written;

    int/*bool*/  is_server_side; /* was created by LSOCK_Accept() */
} SOCK_struct;


/* Global SOCK counter
 */
static unsigned int s_ID_Counter = 0;



/******************************************************************************
 *   Error Logging
 */

/* Global data logging switch.
 * NOTE: no data logging by default
 */
static ESwitch s_LogData = eDefault;


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLogData
(SOCK sock, EIO_Event event, const void* data, size_t size)
{
    char message[128];
    assert(event == eIO_Read  ||  event == eIO_Write);
    sprintf(message, "SOCK#%u -- %s at offset %lu",
            (unsigned int) sock->id,
            (event == eIO_Read) ? "read" : "written",
            (unsigned long) ((event == eIO_Read) ?
                             sock->n_read : sock->n_written));
    CORE_DATA(data, size, message);
}


extern void SOCK_SetDataLoggingAPI(ESwitch log_data)
{
    s_LogData = log_data;
}


extern void SOCK_SetDataLogging(SOCK sock, ESwitch log_data)
{
    sock->log_data = log_data;
}



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


/* Flag to indicate if the API should mask SIGPIPE (during initialization) */
#if defined(NCBI_OS_UNIX)
static int/*bool*/ s_AllowSigPipe = 0/*false*/;
#endif


extern void SOCK_AllowSigPipeAPI(void)
{
#if defined(NCBI_OS_UNIX)
    s_AllowSigPipe = 1/*true*/;
#endif
    return;
}


/* Flag to indicate if the API has been initialized */
static int/*bool*/ s_Initialized = 0/*false*/;


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
            CORE_LOG_ERRNO(x_errno, eLOG_Error,
                           "[SOCK_InitializeAPI]  Failed WSAStartup()");
            return eIO_Unknown;
        }
    }}
#elif defined(NCBI_OS_UNIX)
    if ( !s_AllowSigPipe ) {
        signal(SIGPIPE, SIG_IGN);
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
            CORE_LOG_ERRNO(x_errno, eLOG_Warning,
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
#	error "Unsupported platform"
#endif
}


/* Select on the socket i/o
 */
static EIO_Status s_Select(TSOCK_Handle          sock,
                           EIO_Event             event,
                           const struct timeval* timeout)
{
    int n_fds;
    fd_set fds, *r_fds, *w_fds;

    /* just checking */
    if (sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error,
                 "[SOCK::s_Select]  Attempted to wait on an invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    /* setup i/o descriptor to select */
    r_fds = (event == eIO_Read  ||  event == eIO_ReadWrite) ? &fds : 0;
    w_fds = (event == eIO_Write ||  event == eIO_ReadWrite) ? &fds : 0;

    do { /* auto-resume if interrupted by a signal */
        fd_set e_fds;
        struct timeval tmout;
        if ( timeout )
            tmout = *timeout;
        FD_ZERO(&fds);       FD_ZERO(&e_fds);
        FD_SET(sock, &fds);  FD_SET(sock, &e_fds);
        n_fds = select(SOCK_NFDS(sock), r_fds, w_fds, &e_fds,
                       timeout ? &tmout : 0);
        assert(-1 <= n_fds  &&  n_fds <= 2);
        if ((n_fds < 0  &&  SOCK_ERRNO != SOCK_EINTR)  ||
            FD_ISSET(sock, &e_fds)) {
            return eIO_Unknown;
        }
    } while (n_fds < 0);

    /* timeout has expired */
    if (n_fds == 0)
        return eIO_Timeout;

#if !defined(NCBI_OS_IRIX)
    /* funny thing -- on IRIX, it may set "n_fds" to e.g. 1, and
     * forget to set the bit in "fds" and/or "e_fds"!
     */
    assert(FD_ISSET(sock, &fds));
#endif

    /* success;  can i/o now */
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
        CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                       "[LSOCK_Create]  Cannot create listening socket");
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
        int reuse_addr = 1;
#  endif
        if (setsockopt(x_lsock, SOL_SOCKET, SO_REUSEADDR, 
                       (const char*) &reuse_addr, sizeof(reuse_addr)) != 0) {
            CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                           "[LSOCK_Create]  Failed setsockopt(REUSEADDR)");
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
    if (bind(x_lsock, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
        CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                       "[LSOCK_Create]  Failed bind()");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* Listen */
    if (listen(x_lsock, backlog) != 0) {
        CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                       "[LSOCK_Create]  Failed listen()");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* Set to non-blocking mode */
    if ( !s_SetNonblock(x_lsock, 1/*true*/) ) {
        CORE_LOG(eLOG_Error,
                 "[LSOCK_Create]  Cannot set socket to non-blocking mode");
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* Success... */
    *lsock = (LSOCK) calloc(1, sizeof(LSOCK_struct));
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

    {{ /* wait for the connection request to come (up to timeout) */
        struct timeval tv;
        EIO_Status status =
            s_Select(lsock->sock, eIO_Read, s_to2tv(timeout, &tv));
        if (status != eIO_Success)
            return status;
    }}

    {{ /* accept next connection */
#if defined(HAVE_SOCKLEN_T)
        typedef socklen_t SOCK_socklen_t;
#else
        typedef int       SOCK_socklen_t;
#endif
        struct sockaddr_in addr;
        SOCK_socklen_t addrlen = (SOCK_socklen_t) sizeof(addr);
        if ((x_sock = accept(lsock->sock, (struct sockaddr*) &addr, &addrlen))
            == SOCK_INVALID) {
            CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                           "[LSOCK_Accept]  Failed accept()");
            return eIO_Unknown;
        }
        x_host = addr.sin_addr.s_addr;
        x_port = addr.sin_port;
    }}

    /* success:  create new SOCK structure */
    *sock = (SOCK) calloc(1, sizeof(SOCK_struct));
    (*sock)->sock = x_sock;
    (*sock)->is_eof = 0/*false*/;
    (*sock)->r_status = eIO_Success;
    (*sock)->w_status = eIO_Success;
    verify(SOCK_SetTimeout(*sock, eIO_ReadWrite, 0) == eIO_Success);
    (*sock)->host = x_host;
    (*sock)->port = x_port;
    BUF_SetChunkSize(&(*sock)->buf, SOCK_BUF_CHUNK_SIZE);
    (*sock)->log_data = eDefault;
    (*sock)->r_on_w   = eDefault;
    (*sock)->id = ++s_ID_Counter * 1000;
    (*sock)->is_server_side = 1/*true*/;
    return eIO_Success;
}


extern EIO_Status LSOCK_Close(LSOCK lsock)
{
    /* Set the socket back to blocking mode */
    if ( !s_SetNonblock(lsock->sock, 0/*false*/) ) {
        CORE_LOG(eLOG_Warning,
                 "[LSOCK_Close]  Cannot set socket back to blocking mode");
    }

    /* Close (persistently -- retry if interrupted by a signal) */
    for (;;) {
        /* success */
        if (SOCK_CLOSE(lsock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
                           "[LSOCK_Close]  Failed close()");
            return eIO_Unknown;
        }
    }

    /* success:  cleanup & return */
    free(lsock);
    return eIO_Success;
}


extern EIO_Status LSOCK_GetOSHandle(LSOCK  lsock,
                                    void*  handle_buf,
                                    size_t handle_size)
{
    if (handle_size != sizeof(lsock->sock)) {
        CORE_LOG(eLOG_Error, "[LSOCK_GetOSHandle]  Invalid handle size");
        assert(0);
        return eIO_Unknown;
    }

    memcpy(handle_buf, &lsock->sock, handle_size);
    return eIO_Success;
}



/******************************************************************************
 *  SOCKET
 */


/* Read-while-writing switch.
 * NOTE: no read-while-writing by default
 */
static ESwitch s_ReadOnWrite = eDefault;



/* Connect the (pre-allocated) socket to the specified "host:port" peer.
 * HINT: if "host" is NULL then assume(!) that the "sock" already exists,
 *       and connect to the same host;  the same is for zero "port".
 */
static EIO_Status s_Connect(SOCK            sock,
                            const char*     host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    TSOCK_Handle   x_sock;
    unsigned int   x_host;
    unsigned short x_port;

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    /* Initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* Get address of the remote host (assume the same host if it is NULL) */
    assert(host  ||  sock->host);
    x_host = host ? SOCK_gethostbyname(host) : sock->host;
    if ( !x_host ) {
        if ( CORE_GetLOG() ) {
            char str[128];
            sprintf(str, "[SOCK::s_Connect]  Failed SOCK_gethostbyname(%.64s)",
                    host ? host : "");
            CORE_LOG(eLOG_Error, str);
        }
        return eIO_Unknown;
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
        CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error,
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

    /* Establish connection to the peer */
    if (connect(x_sock, (struct sockaddr*) &server, sizeof(server)) != 0) {
        if (SOCK_ERRNO != SOCK_EINTR  &&  SOCK_ERRNO != SOCK_EINPROGRESS  &&
            SOCK_ERRNO != SOCK_EWOULDBLOCK) {
            if ( CORE_GetLOG() ) {
                char str[256];
                sprintf(str,
                        "[SOCK::s_Connect]  Failed connect() to %.64s:%d",
                        host ? host : "???", (int)ntohs(x_port));
                CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Error, str);
            }
            SOCK_CLOSE(x_sock);
            return eIO_Unknown; /* unrecoverable error */
        }

        /* The connect could be interrupted by a signal or just cannot be
         * established immediately;  yet, the connect must have been in
         * progress (asynchronous), so wait here for it to succeed
         * (become writable).
         */
        {{
            struct timeval tv;
            EIO_Status status =
                s_Select(x_sock, eIO_Write, s_to2tv(timeout, &tv));
            if (status != eIO_Success  &&  CORE_GetLOG()) {
                char str[256];
                sprintf(str,
                        "[SOCK::s_Connect]  Failed pending connect to "
                        "%.64s:%d (%.32s)",
                        host ? host : "???",
                        (int) ntohs(x_port),
                        IO_StatusStr(status));
                CORE_LOG(eLOG_Error, str);
                SOCK_CLOSE(x_sock);
                return status;
            }
        }}
    }

    /* Success */
    /* NOTE:  it does not change the timeouts and the data buffer content */
    sock->sock   = x_sock;
    sock->host   = x_host;
    sock->port   = x_port;
    sock->is_eof   = 0/*false*/;
    sock->r_status = eIO_Success;
    sock->w_status = eIO_Success;

    return eIO_Success;
}


/* Shutdown and close the socket
 */
static EIO_Status s_Close(SOCK sock)
{
    /* Check against an invalid socket */
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Warning,
                 "[SOCK::s_Close]  Attempt to close an invalid socket");
        return eIO_Unknown;
    }

    {{ /* Reset the auxiliary data buffer */
        size_t buf_size = BUF_Size(sock->buf);
        if (BUF_Read(sock->buf, 0, buf_size) != buf_size) {
            CORE_LOG(eLOG_Error,
                     "[SOCK::s_Close]  Cannot reset aux. data buffer");
            assert(0);
        }
    }}

    /* Set the socket back to blocking mode */
    if ( !s_SetNonblock(sock->sock, 0/*false*/) ) {
        CORE_LOG(eLOG_Error,
                 "[SOCK::s_Close]  Cannot set socket to blocking mode");
    }

    /* Set the close()'s linger period be equal to the close timeout */
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    if ( sock->c_timeout ) {
        struct linger lgr;
        lgr.l_onoff  = 1;
        lgr.l_linger = sock->c_timeout->tv_sec ? sock->c_timeout->tv_sec : 1;
        if (setsockopt(sock->sock, SOL_SOCKET, SO_LINGER, 
                       (char*) &lgr, sizeof(lgr)) != 0) {
            CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Warning,
                           "[SOCK::s_Close]  Failed setsockopt(SO_LINGER)");
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

    /* Close (persistently retry if interrupted by a signal) */
    for (;;) {
        /* success */
        if (SOCK_CLOSE(sock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            CORE_LOG_ERRNO(SOCK_ERRNO, eLOG_Warning,
                           "[SOCK::s_Close]  Failed close()");
            sock->sock = SOCK_INVALID;
            return (SOCK_ERRNO == SOCK_ECONNRESET || SOCK_ERRNO == SOCK_EPIPE)
                ? eIO_Closed : eIO_Unknown;
        }
    }

    /* Success */
    sock->sock = SOCK_INVALID;
    return eIO_Success;
}


/* To allow emulating "peek" using the NCBI data buffering.
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves)
 */
static int s_NCBI_Recv(SOCK        sock,
                       void*       buffer,
                       size_t      size,
                       int/*bool*/ peek)
{
    char   xx_buffer[4096];
    char*  x_buffer = (char*) buffer;
    size_t n_read;

    /* read (or peek) from the internal buffer */
    n_read = peek ?
        BUF_Peek(sock->buf, x_buffer, size) :
        BUF_Read(sock->buf, x_buffer, size);
    if (n_read == size  ||  sock->r_status == eIO_Closed) {
        return (int) n_read;
    }

    /* read (not just peek) from the socket */
    do {
        /* recv */
        int    x_readsock;
        size_t n_todo = size - n_read;
        if ( buffer ) {
            /* read to the data buffer provided by user */
            x_buffer += n_read;
            x_readsock = recv(sock->sock, x_buffer, n_todo, 0);
        } else {
            /* read to the temporary buffer (to store or discard later) */
            if (n_todo > sizeof(xx_buffer)) {
                n_todo = sizeof(xx_buffer);
            }
            x_readsock = recv(sock->sock, xx_buffer, n_todo, 0);
            x_buffer = xx_buffer;
        }

        /* catch EOF */
        if (x_readsock == 0  ||
            (x_readsock < 0  &&  SOCK_ERRNO == SOCK_ENOTCONN)) {
            sock->r_status = eIO_Closed;
            sock->is_eof   = 1/*true*/;
            return n_read ? (int) n_read : 0;
        }
        /* catch unknown ERROR */
        if (x_readsock < 0) {
            int x_errno = SOCK_ERRNO;
            if (x_errno != SOCK_EWOULDBLOCK  &&
                x_errno != SOCK_EAGAIN  &&
                x_errno != SOCK_EINTR) {
                sock->r_status = eIO_Unknown;
            }
            return n_read ? (int) n_read : -1;
        }
        /* successful read */
        assert(x_readsock > 0);
        sock->r_status = eIO_Success;

        /* if "peek" -- store the new read data in the internal buffer */
        if ( peek ) {
            verify(BUF_Write(&sock->buf, x_buffer, (size_t) x_readsock));
        }

        /* statistics & logging */
        if (sock->log_data == eOn  ||
            (sock->log_data == eDefault  &&  s_LogData == eOn)) {
            s_DoLogData(sock, eIO_Read, x_buffer, (size_t) x_readsock);
        }
        sock->n_read += x_readsock;
        n_read       += x_readsock;
    }
    while (!buffer  &&  n_read < size);

    return (int) n_read;
}


/* Read/Peek data from the socket
 */
static EIO_Status s_Recv(SOCK        sock,
                         void*       buf,
                         size_t      size,
                         size_t*     n_read,
                         int/*bool*/ peek)
{
    int x_errno;

    /* Check against an invalid socket */
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error,
                 "[SOCK::s_Recv]  Attempted to read from an invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    *n_read = 0;
    if (size == 0)
        return sock->r_status;

    for (;;) {
        /* try to read */
        int x_read = s_NCBI_Recv(sock, buf, size, peek);
        if (x_read > 0) {
            assert(x_read <= (int) size);
            *n_read = x_read;
            return eIO_Success;  /* success */
        }

        if (sock->r_status == eIO_Unknown) {
            return eIO_Unknown;  /* some error */
        }
        if (sock->r_status == eIO_Closed) {
            return eIO_Closed;  /* hit EOF or shut down */
        }

        x_errno = SOCK_ERRNO;

        /* blocked -- wait for a data to come;  exit if timeout/error */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            EIO_Status status =
                s_Select(sock->sock, eIO_Read, sock->r_timeout);
            if (status != eIO_Success)
                return status;
            continue;
        }

        /* retry if interrupted by a signal */
        if (x_errno == SOCK_EINTR)
            continue;

        /* dont want to handle all possible errors... let them be "unknown" */
        break;
    }

    return eIO_Unknown;
}


/* Stall protection: try pull incoming data from the socket.
 * Available only for eIO_Write & eIO_ReadWrite events.
 * For event == eIO_Read, or if read-on-write is disabled, or if
 * the READ stream is closed, then this function 
 * is equivalent to s_Select().
 */
static EIO_Status s_SelectStallsafe(SOCK                  sock,
                                    EIO_Event             event,
                                    const struct timeval* timeout)
{
    EIO_Status status;
    static struct timeval s_ZeroTimeout = {0, 0};

    /* just checking */
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error,
                 "[SOCK::s_Select]  Attempted to stall protection on an "
                 "invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    /* check if to use a "regular" s_Select() */
    if (event == eIO_Read  ||
        sock->r_status == eIO_Closed  ||
        sock->r_on_w == eOff  ||
        (sock->r_on_w == eDefault  &&  s_ReadOnWrite != eOn)) {
        /* wait until event (up to timeout) */
        return s_Select(sock->sock, event, timeout);
    }

    /* check if immediately writable */
    status = s_Select(sock->sock, event, &s_ZeroTimeout);
    if (status != eIO_Timeout)
        return status;

    /* do wait (and try read data if it is not writable yet) */
    do {
        /* try upread data to the internal buffer */
        s_NCBI_Recv(sock, 0, 100000000/*read as much as possible*/, 1/*peek*/);

        /* wait for r/w */
        status = s_Select(sock->sock, eIO_ReadWrite, timeout);
        if (status == eIO_Success  &&
            s_Select(sock->sock, eIO_Write, &s_ZeroTimeout) == eIO_Success) {
            break;  /* can write now */
        }
    }
    while (status == eIO_Success);

    /* return status;*/
    return status;
}


/* Write data to the socket "as is" (the whole buffer at once)
 */
static EIO_Status s_WriteWhole(SOCK        sock,
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
        return eIO_Success;

    for (;;) {
        /* try to write */
        int x_written = send(sock->sock, (char*) x_buf, x_size, 0);
        if (x_written >= 0) {
            /* statistics & logging */
            if (sock->log_data == eOn  ||
                (sock->log_data == eDefault  &&  s_LogData == eOn)) {
                s_DoLogData(sock, eIO_Write, x_buf, (size_t) x_written);
            }
            sock->n_written += x_written;

            /* */
            if ( n_written )
                *n_written += x_written;
            if (x_written == x_size)
                return eIO_Success; /* all data has been successfully sent */
            x_buf  += x_written;
            x_size -= x_written;
            assert(x_size > 0);
            continue; /* there is unsent data */
        }
        x_errno = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout is expired */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            /* stall protection:  try pull incoming data from the socket */
            EIO_Status status = s_SelectStallsafe(sock, eIO_Write, 
                                                  sock->w_timeout);
            if (status != eIO_Success)
                return status;
            continue;
        }

        /* retry if interrupted by a signal */
        if (x_errno == SOCK_EINTR)
            continue;

        /* forcibly closed by peer, or shut down */
        if (x_errno == SOCK_ECONNRESET  ||  x_errno == SOCK_EPIPE)
            return eIO_Closed;

        /* dont want to handle all possible errors... let it be "unknown" */  
        break;
    }
    return eIO_Unknown;
}


#if defined(SOCK_WRITE_SLICE)
/* Split output buffer by slices (of size <= SOCK_WRITE_SLICE) before writing
 * to the socket
 */
static EIO_Status s_WriteSliced(SOCK        sock,
                                const void* buf,
                                size_t      size,
                                size_t*     n_written)
{
    EIO_Status status    = eIO_Success;
    size_t     x_written = 0;

    while (size  &&  status == eIO_Success) {
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



extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port,
                              const STimeout* timeout,
                              SOCK*           sock)
{
    /* Allocate memory for the internal socket structure */
    SOCK x_sock = (SOCK_struct*) calloc(1, sizeof(SOCK_struct));
    x_sock->sock = SOCK_INVALID;

    /* Connect */
    {{
        EIO_Status status;
        if ((status = s_Connect(x_sock, host, port, timeout)) != eIO_Success) {
            free(x_sock);
            return status;
        }
    }}

    /* Setup the input data buffer properties */
    BUF_SetChunkSize(&x_sock->buf, SOCK_BUF_CHUNK_SIZE);

    /* Success */
    x_sock->log_data = eDefault;
    x_sock->r_on_w   = eDefault;
    x_sock->id = ++s_ID_Counter * 1000;
    *sock = x_sock;
    return eIO_Success;
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    /* Close the socket, if necessary */
    if (sock->sock != SOCK_INVALID) {
        s_Close(sock);
    }

    /* Special treatment for server-side socket */
    if ( sock->is_server_side ) {
        if (!host  ||  !port) {
            CORE_LOG(eLOG_Error, "[SOCK_Reconnect]  Attempt to reconnect "
                     "server-side socket as client one to its peer address");
            return eIO_InvalidArg;
        }
        sock->is_server_side = 0/*false*/;
    }

    /* Connect */
    sock->id++;
    return s_Connect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Shutdown(SOCK      sock,
                                EIO_Event how)
{
    int x_how;
    switch ( how ) {
    case eIO_Read:
        if (sock->r_status == eIO_Closed) {
            if ( sock->is_eof ) {
                /* Hit EOF, but not shutdown yet. So, flag it as shutdown, but
                 * do not call the actual system shutdown(), as it can cause
                 * smart OS'es like Linux to complain. */
                sock->is_eof = 0/*false*/;
            }
            return eIO_Success;
        }
        x_how = SOCK_SHUTDOWN_RD;
        sock->r_status = eIO_Closed;
        break;
    case eIO_Write:
        if (sock->w_status == eIO_Closed) {
            return eIO_Success;  /* has been shutdown already */
        }
        x_how = SOCK_SHUTDOWN_WR;
        sock->w_status = eIO_Closed;
        break;
    case eIO_ReadWrite:
        verify(SOCK_Shutdown(sock, eIO_Write) == eIO_Success);
        verify(SOCK_Shutdown(sock, eIO_Read ) == eIO_Success);
        return eIO_Success;
    default:
        CORE_LOG(eLOG_Warning, "[SOCK_Shutdown]  Invalid argument");
        return eIO_InvalidArg;
    }

    if (SOCK_SHUTDOWN(sock->sock, x_how) != 0) {
        CORE_LOGF(eLOG_Warning, ("[SOCK_Shutdown]  shutdown(%s) failed",
                                 how == eIO_Read ? "read" : "write"));
    }
    return eIO_Success;
}


extern EIO_Status SOCK_Close(SOCK sock)
{
    EIO_Status status = s_Close(sock);
    BUF_Destroy(sock->buf);
    free(sock);
    return status;
}


extern EIO_Status SOCK_Wait(SOCK            sock,
                            EIO_Event       event,
                            const STimeout* timeout)
{
    /* Check against already shutdown socket */
    switch ( event ) {
    case eIO_Read:
        if (BUF_Size(sock->buf) != 0) {
            return eIO_Success;
        }
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Warning,
                      ("[SOCK_Wait(Read)]  Attempt to wait on %s socket",
                       sock->is_eof ? "closed" : "shutdown"));
            return eIO_Closed;
        }
        break;
    case eIO_Write:
        if (sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK_Wait(Write)]  Attempt to wait on shutdown socket");
            return eIO_Closed;
        }
        break;
    case eIO_ReadWrite:
        if (BUF_Size(sock->buf) != 0) {
            return eIO_Success;
        }
        if (sock->r_status == eIO_Closed  &&  sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Warning,
                     "[SOCK_Wait(RW)]  Attempt to wait on shutdown socket");
            return eIO_Closed;
        }
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Note,
                      ("[SOCK_Wait(RW)]  Attempt to wait on %s socket",
                       sock->is_eof ? "closed" : "R-shutdown"));
            event = eIO_Write;
            break;
        }
        if (sock->w_status == eIO_Closed) {
            CORE_LOG(eLOG_Note,
                     "[SOCK_Wait(RW)]  Attempt to wait on W-shutdown socket");
            event = eIO_Read;
            break;
        }
        break;
    default:
        CORE_LOG(eLOG_Error,
                 "[SOCK_Wait]  Invalid (not eIO_[Read][Write]) event type");
        return eIO_InvalidArg;
    }

    /* Do wait */
    {{
        struct timeval tv;
        return s_SelectStallsafe(sock, event, s_to2tv(timeout, &tv));
    }}
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
        CORE_LOG(eLOG_Error, "[SOCK_SetTimeout]  Invalid argument");
        assert(0);
        return eIO_Unknown;
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
    switch ( how ) {
    case eIO_Plain: {
        return s_Recv(sock, buf, size, n_read, 0/*false*/);
    }

    case eIO_Peek: {
        return s_Recv(sock, buf, size, n_read, 1/*true*/);
    }

    case eIO_Persist: {
        EIO_Status status = eIO_Success;
        *n_read = 0;
        do {
            size_t x_read;
            status = SOCK_Read(sock, (char*) buf + (buf ? *n_read : 0), size,
                               &x_read, eIO_Plain);
            *n_read += x_read;
            if (status != eIO_Success)
                return status;
            size    -= x_read;
        } while (size);
        return eIO_Success;
    }
    }

    assert(0);
    return eIO_Unknown;
}


extern EIO_Status SOCK_PushBack(SOCK        sock,
                                const void* buf,
                                size_t      size)
{
    return BUF_PushBack(&sock->buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_Status(SOCK      sock,
                              EIO_Event direction)
{
    switch ( direction ) {
    case eIO_Read:
        return sock->r_status;
    case eIO_Write:
        return sock->w_status;
    default:
        return eIO_InvalidArg;
    }
}


extern EIO_Status SOCK_Write(SOCK        sock,
                             const void* buf,
                             size_t      size,
                             size_t*     n_written)
{
    /* Check against an invalid socket */
    if (sock->sock == SOCK_INVALID) {
        CORE_LOG(eLOG_Error,
                 "[SOCK_Write]  Attempt to write to an invalid socket");
        assert(0);
        return eIO_Unknown;
    }

    /* Check against already shutdown socket */
    if (sock->w_status == eIO_Closed) {
        CORE_LOG(eLOG_Warning,
                 "[SOCK_Write]  Attempt to write to already shutdown socket");
        return eIO_Closed;
    }

    /* Do write */
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


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle_buf,
                                   size_t handle_size)
{
    if (handle_size != sizeof(sock->sock)) {
        CORE_LOG(eLOG_Error, "[SOCK_GetOSHandle]  Invalid handle size!");
        assert(0);
        return eIO_Unknown;
    }

    memcpy(handle_buf, &sock->sock, handle_size);
    return eIO_Success;
}


extern void SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    s_ReadOnWrite = on_off;
}


extern void SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off)
{
    sock->r_on_w = on_off;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock->is_server_side;
}


extern int SOCK_gethostname(char*  name,
                            size_t namelen)
{
    int x_errno;
    assert(namelen > 0);
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);
    name[0] = name[namelen - 1] = '\0';
    x_errno = gethostname(name, (int) namelen);
    if (x_errno  ||  name[namelen - 1]) {
        CORE_LOG_ERRNO(x_errno, eLOG_Error,
                       "[SOCK_gethostname]  Cannot get local hostname");
        name[0] = '\0';
        return 1/*failed*/;
    }
    return 0/*success*/;
}


extern int SOCK_ntoa(unsigned int host,
                     char*        buf,
                     size_t       buf_size)
{
    const unsigned char* b = (const unsigned char*) &host;
    char str[16];

    verify(sprintf(str, "%u.%u.%u.%u",
                   (unsigned) b[0], (unsigned) b[1],
                   (unsigned) b[2], (unsigned) b[3]) > 0);
    assert(strlen(str) < sizeof(str));

    if (strlen(str) >= buf_size) {
        CORE_LOG(eLOG_Error, "[SOCK_ntoa]  Buffer too small");
        buf[0] = '\0';
        return -1/*failed*/;
    }
        
    strcpy(buf, str);
    return 0/*success*/;
}


extern unsigned int SOCK_htonl(unsigned int value)
{
    return htonl(value);
}


extern unsigned int SOCK_gethostbyname(const char* hostname)
{
    unsigned int host;
    char buf[1024];

    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    if ( !hostname ) {
        if (SOCK_gethostname(buf, sizeof(buf)) != 0)
            return 0;
        hostname = buf;
    }

    host = inet_addr(hostname);
    if (host == htonl(INADDR_NONE)) {
        struct hostent* he;
#if defined(HAVE_GETHOSTBYNAME_R)
        struct hostent x_he;
        char           x_buf[1024];
        int            x_err;
#  if (HAVE_GETHOSTBYNAME_R == 5)
        he = gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf),
                             &x_err);
#  elif (HAVE_GETHOSTBYNAME_R == 6)
        if (gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf),
                            &he, &x_err) != 0) {
            assert(he == 0);
            he = 0;
        }
#  else
#    error "Unknown HAVE_GETHOSTBYNAME_R value"
#  endif
#else
        CORE_LOCK_WRITE;
        he = gethostbyname(hostname);
#endif
        if ( he ) {
            memcpy(&host, he->h_addr, sizeof(host));
        } else {
            host = 0;
        }

#if !defined(HAVE_GETHOSTBYNAME_R)
        CORE_UNLOCK;
#endif
    }

    return host;
}


extern char* SOCK_gethostbyaddr(unsigned int host,
                                char*        name,
                                size_t       namelen)
{
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    if (host  &&  name  &&  namelen) {
        struct hostent* he;
#if defined(HAVE_GETHOSTBYADDR_R)
        struct hostent x_he;
        char           x_buf[1024];
        int            x_errno;

#  if (HAVE_GETHOSTBYADDR_R == 7)
        he = gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                             x_buf, sizeof(x_buf), &x_errno);
#  elif (HAVE_GETHOSTBYADDR_R == 8)
        if (gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                            x_buf, sizeof(x_buf), &he, &x_errno) != 0) {
            assert(he == 0);
            he = 0;
        }
#  else
#    error "Unknown HAVE_GETHOSTBYADDR_R value"
#  endif
#else
        CORE_LOCK_WRITE;
        he = gethostbyaddr((char*) &host, sizeof(host), AF_INET);
#endif

        if (!he  ||  strlen(he->h_name) > namelen - 1) {
#if !defined(HAVE_GETHOSTBYADDR_R)
            CORE_UNLOCK;
#endif
            return 0;
        }
        strncpy(name, he->h_name, namelen - 1);
#if !defined(HAVE_GETHOSTBYADDR_R)
        CORE_UNLOCK;
#endif
        name[namelen - 1] = '\0';
        return name;
    }

    return 0;
}
