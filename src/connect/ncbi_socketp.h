#ifndef CONNECT___NCBI_SOCKETP__H
#define CONNECT___NCBI_SOCKETP__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Private API to define socket structure
 *
 */

#include "ncbi_config.h"
/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_MSWIN)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN!"
#endif /*supported platforms*/

#include <connect/ncbi_socket.h>
#include <connect/ncbi_buffer.h>


/* Pull in a minial set of platform-specific system headers here.
 */

#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#else /*NCBI_OS_UNIX*/
#  include <sys/socket.h>
#  include <sys/time.h>
#endif /*NCBI_OS_MSWIN*/

/* Portable error codes.
 */
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#ifdef NCBI_OS_MSWIN

typedef SOCKET TSOCK_Handle;    /* NB: same as HANDLE   */
typedef HANDLE TRIGGER_Handle;  /* NB: same as WSAEVENT */

#  ifdef _WIN64
#    pragma pack(push, 4)
#  endif /*_WIN64*/

#  define SOCK_EINTR          WSAEINTR
#  define SOCK_EWOULDBLOCK    WSAEWOULDBLOCK/*EAGAIN*/
#  define SOCK_EADDRINUSE     WSAEADDRINUSE
#  define SOCK_ECONNRESET     WSAECONNRESET
#  define SOCK_EPIPE          WSAESHUTDOWN
#  define SOCK_EAGAIN         WSAEINPROGRESS/*special-case missing in WSA*/
#  define SOCK_EINPROGRESS    WSAEINPROGRESS
#  define SOCK_EALREADY       WSAEALREADY
#  define SOCK_ENOTCONN       WSAENOTCONN
#  define SOCK_ECONNABORTED   WSAECONNABORTED
#  define SOCK_ECONNREFUSED   WSAECONNREFUSED
#  define SOCK_ENETRESET      WSAENETRESET
#  define SOCK_ETIMEDOUT      WSAETIMEDOUT
#  define SOCK_SHUTDOWN_RD    SD_RECEIVE
#  define SOCK_SHUTDOWN_WR    SD_SEND
#  define SOCK_SHUTDOWN_RDWR  SD_BOTH

#else /*NCBI_OS_UNIX*/

typedef int TSOCK_Handle;
typedef int TRIGGER_Handle;

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

#endif /*NCBI_OS_MSWIN*/

#if   defined(ENFILE)
#  define SOCK_ETOOMANY       ENFILE
#elif defined(EMFILE)
#  define SOCK_ETOOMANY       EMFILE
#elif defined(WSAEMFILE)
#  define SOCK_ETOOMANY       WSAEMFILE
#elif defined(EINVAL)
#  define SOCK_ETOOMANY       EINVAL
#else
#  define SOCK_ETOOMANY       0
#endif


#if 0/*defined(__GNUC__)*/
typedef ESwitch    EBSwitch;
typedef EIO_Status EBIO_Status;
#else
typedef unsigned   EBSwitch;
typedef unsigned   EBIO_Status;
#endif


typedef enum {
    eListening = 0,
    eTrigger   = 1,
    eSocket    = 2,
    eDatagram  = 3/*2|1*/
} ESOCK_Type;

typedef unsigned TBSOCK_Type;


/* Event trigger
 */
typedef struct TRIGGER_tag {
    TRIGGER_Handle     fd;      /* OS-specific trigger handle                */
    unsigned int       id;      /* the internal ID (cf. "s_ID_Counter")      */

    union {
        volatile void* ptr;     /* trigger state (UNIX only, otherwise MBZ)  */
        int            int_[2]; /* pointer storage area w/proper alignment   */
    } isset;

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    TBSOCK_Type         type:2; /* eTrigger                                  */
    EBSwitch             log:2; /* how to log events                         */
    EBSwitch          r_on_w:2; /* MBZ                                       */
    EBSwitch        i_on_sig:2; /* eDefault                                  */

    EBIO_Status     r_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/     eof:1; /* MBZ                                       */
    EBIO_Status     w_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/ pending:1; /* MBZ                                       */

    unsigned        reserved:16;/* MBZ                                       */

#ifdef NCBI_OS_UNIX
    int                out;     /* write end of the pipe                     */
#endif /*NCBI_OS_UNIX*/
} TRIGGER_struct;


/* Sides of socket
 */
typedef enum {
    eSOCK_Server = 0,
    eSOCK_Client = 1
} ESOCK_Side;

typedef unsigned EBSOCK_Side;


/* Listening socket [must be in one-2-one binary correspondene with TRIGGER]
 */
typedef struct LSOCK_tag {
    TSOCK_Handle     sock;      /* OS-specific socket handle                 */
    unsigned int     id;        /* the internal ID (see also "s_ID_Counter") */

    unsigned int     n_accept;  /* total number of accepted clients          */
    unsigned short   away;      /* MSWIN: run-away connect warning counter   */
    unsigned short   port;      /* port on which listening (host byte order) */

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    TBSOCK_Type         type:2; /* eListening                                */
    EBSwitch             log:2; /* how to log events and data for this socket*/
    EBSwitch          r_on_w:2; /* MBZ                                       */
    EBSwitch        i_on_sig:2; /* eDefault                                  */

    EBIO_Status     r_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/     eof:1; /* MBZ                                       */
    EBIO_Status     w_status:3; /* MBZ (NB: eIO_Success)                     */
    unsigned/*bool*/ pending:1; /* MBZ                                       */

    EBSOCK_Side         side:1; /* MBZ (NB: eSOCK_Server)                    */
    unsigned/*bool*/    keep:1; /* whether to keep OS handle upon close      */
#ifndef NCBI_OS_MSWIN
    unsigned        reserved:14;/* MBZ                                       */
#else
    unsigned        reserved:11;/* MBZ                                       */
    unsigned        readable:1; /* =1 if known to have a pending accept      */
    unsigned          unused:2; /* MBZ                                       */

    WSAEVENT         event;     /* event bound to I/O                        */
#endif /*!NCBI_OS_MSWIN*/

    void*            context;   /* per-server session/credentials (not impl) */

#ifdef NCBI_OS_UNIX
    char             path[1];   /* must go last                              */
#endif /*NCBI_OS_UNIX*/
} LSOCK_struct;


/* Socket [it must be in 1-2-1 binary correspondence with LSOCK above]
 */
typedef struct SOCK_tag {
    TSOCK_Handle     sock;      /* OS-specific socket handle                 */
    unsigned int     id;        /* the internal ID (see also "s_ID_Counter") */

    /* connection point */
    unsigned int     host;      /* peer host (network byte order)            */
    unsigned short   port;      /* peer port (host byte order)               */
    unsigned short   myport;    /* this socket's port number, host byte order*/

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    TBSOCK_Type         type:2; /* |= eSocket ({ eSocket | eDatagram })      */
    EBSwitch             log:2; /* how to log events and data for this socket*/
    EBSwitch          r_on_w:2; /* enable/disable automatic read-on-write    */
    EBSwitch        i_on_sig:2; /* enable/disable I/O restart on signals     */

    EBIO_Status     r_status:3; /* read  status:  eIO_Closed if was shut down*/
    unsigned/*bool*/     eof:1; /* Stream sockets: 'End of file' seen on read
                                   Datagram socks: 'End of message' written  */
    EBIO_Status     w_status:3; /* write status:  eIO_Closed if was shut down*/
    unsigned/*bool*/ pending:1; /* =1 if connection is still initing         */

    EBSOCK_Side         side:1; /* socket side: client- or server-side       */
    unsigned/*bool*/    keep:1; /* whether to keep OS handle upon close      */
    unsigned       crossexec:1; /* =1 if close-on-exec must NOT be set       */
    unsigned       connected:1; /* =1 if remote end-point is fully connected */
    unsigned        r_tv_set:1; /* =1 if read  timeout is set (i.e. finite)  */
    unsigned        w_tv_set:1; /* =1 if write timeout is set (i.e. finite)  */
    unsigned        c_tv_set:1; /* =1 if close timeout is set (i.e. finite)  */
    unsigned       keepalive:1; /* =1 if needs to be kept alive (if OS supp.)*/
#ifndef NCBI_OS_MSWIN
    unsigned        reserved:8; /* MBZ                                       */
#else
    unsigned        reserved:5; /* MBZ                                       */
    unsigned        readable:1; /* =1 if known to be readable                */
    unsigned        writable:1; /* =1 if known to be writeable               */
    unsigned         closing:1; /* =1 if FD_CLOSE posted                     */

    WSAEVENT         event;     /* event bound to I/O                        */
#endif /*!NCBI_OS_MSWIN*/

    void*            session;   /* secure session id if secure, else 0       */
    NCBI_CRED        cred;      /* secure session credentials, 0 if none     */

    /* timeouts */
    struct timeval   r_tv;      /* finite read  timeout value                */
    struct timeval   w_tv;      /* finite write timeout value                */
    struct timeval   c_tv;      /* finite close timeout value                */
    STimeout         r_to;      /* finite read  timeout value (aux., temp.)  */
    STimeout         w_to;      /* finite write timeout value (aux., temp.)  */
    STimeout         c_to;      /* finite close timeout value (aux., temp.)  */

    /* aux I/O data */
    BUF              r_buf;     /* read  buffer                              */
    BUF              w_buf;     /* write buffer                              */
    size_t           r_len;     /* DSOCK: size of last message received      */
    size_t           w_len;     /* SOCK: how much data is pending for output */

    /* statistics */
    TNCBI_BigCount   n_read;    /* DSOCK: total # of bytes read (in all msgs)
                                   SOCK:  # of bytes read since last connect
                                */
    TNCBI_BigCount   n_written; /* DSOCK: total # of bytes written (all msgs)
                                   SOCK:  # of bytes written since last connect
                                */
    TNCBI_BigCount   n_in;      /* DSOCK: total # of messages received
                                   SOCK:  total # of bytes read in all
                                   completed connections in this SOCK so far
                                */
    TNCBI_BigCount   n_out;     /* DSOCK: total # of messages sent
                                   SOCK:  total # of bytes written in all
                                   completed connections in this SOCK so far
                                */
#ifdef NCBI_OS_UNIX
    /* pathname for UNIX socket */
    char             path[1];   /* must go last                              */
#endif /*NCBI_OS_UNIX*/
} SOCK_struct;


/*
 * The following implementation details are worth noting:
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


#if defined(NCBI_OS_MSWIN)  &&  defined(_WIN64)
#  pragma pack(pop)
#endif /*NCBI_OS_MSWIN && _WIN64*/


/* initial socket data & respective private ctors */
typedef struct {
    const void* data;
    size_t      size;
    NCBI_CRED   cred;
} SSOCK_Init;


EIO_Status SOCK_CreateInternal(const char*     host,
                               unsigned short  port,
                               const STimeout* timeout,
                               SOCK*           sock,
                               SSOCK_Init*     init,
                               TSOCK_Flags     flags);


EIO_Status SOCK_CreateOnTopInternal(const void* handle,
                                    size_t      handle_size,
                                    SOCK*       sock,
                                    SSOCK_Init* init,
                                    TSOCK_Flags flags);


/* Global data */
extern const char g_kNcbiSockNameAbbr[];


#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


#endif /* CONNECT___NCBI_SOCKETP__H */
