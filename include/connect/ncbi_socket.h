#ifndef NCBI_SOCKET__H
#define NCBI_SOCKET__H

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
 *   Platform-specific library requirements:
 *     [UNIX ]   -DOS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DOS_MSWIN    wsock32.lib
 *     [MacOS]   -DOS_MAC      NCSASOCK -- BSD-style socket emulation library 
 *
 *********************************
 * Generic:
 *
 *  SOCK, LSOCK
 *  SSOCK_Timeout, ESOCK_Direction, ESOCK_Status, ESOCK_Read
 *  FSOCK_ErrHandler, FSOCK_ErrCleanup
 *
 *  SOCK_MT_SetCriticalSection, FSOCK_MT_CSection, ESOCK_MT_Locking
 *
 *  SOCK_InitializeAPI
 *  SOCK_ShutdownAPI
 *  SOCK_ErrCodeStr
 *  SOCK_SetErrHandler
 *  SOCK_SetErrStream
 *
 * Listening socket (handle LSOCK):
 *
 *  LSOCK_Create
 *  LSOCK_Accept
 *  LSOCK_Close
 *  
 * I/O Socket (handle SOCK):
 *
 *  SOCK_Create (see also LSOCK_Accept)
 *  SOCK_Reconnect
 *  SOCK_Close
 *  SOCK_Wait
 *  SOCK_SetTimeout
 *  SOCK_GetReadTimeout
 *  SOCK_GetWriteTimeout
 *  SOCK_Read (including "peek" and "persistent read")
 *  SOCK_PushBack
 *  SOCK_Eof
 *  SOCK_Write
 *  SOCK_GetAddress
 *
 * Auxiliary:
 *
 *  SOCK_gethostname
 *  SOCK_host2inaddr
 *  SOCK_htonl
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  1999/10/18 15:36:39  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 *
 * ===========================================================================
 */

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  CONSISTENCY CHECK
 */

#if defined(NCBISOCK__H)
#  error "<ncbisock.h> and <ncbi_socket.h> must never be #include'd together"
#endif



/******************************************************************************
 *  TYPEDEF & MACRO
 */


/* Forward declarations of the hidden socket internal structure, and
 * their upper-level handles to use by the LSOCK_*() and SOCK_*() API
 */
struct LSOCK_tag;                /* listening socket:  internal storage  */
typedef struct LSOCK_tag* LSOCK; /* listening socket:  handle */

struct SOCK_tag;                 /* socket:  internal storage  */
typedef struct SOCK_tag* SOCK;   /* socket:  handle */


/* Timeout structure
 */
typedef struct {
  unsigned int sec;  /* seconds (gets truncated to platform-dep. max. limit) */
  unsigned int usec; /* microseconds (always get truncated by mod 1,000,000) */
} SSOCK_Timeout;


/* I/O direction
 */
typedef enum {
  eSOCK_OnRead,
  eSOCK_OnWrite,
  eSOCK_OnReadWrite
} ESOCK_Direction;


/* Error code
 */
typedef enum {
  eSOCK_Success = 0, /* everything is fine, no errors occured          */
  eSOCK_Timeout,     /* timeout expired before the data could be i/o'd */
  eSOCK_Closed,      /* peer has closed the connection                 */
  eSOCK_Unknown      /* unknown (maybe fatal) error                    */
} ESOCK_Status;


/* Error severity
 */
typedef enum {
  eSOCK_PostWarning = 0,
  eSOCK_PostError
} ESOCK_PostSeverity;



/******************************************************************************
 *  Multi-Thread SAFETY
 *
 * SOCK_InitializeAPI(), SOCK_ShutdownAPI(),  SOCK_SetErrHandler() and
 * SOCK_SetErrStream() manipulate with the API-wide static data, and thus
 * you must take care to avoid different threads to call them at the same
 * time.
 *
 * For this reason, in multi-thread applications where there are more than
 * one thread using this API, it is usually better to call
 * SOCK_InitializeAPI() explicitely in the beginning of your main thread,
 * before you run any other threads, and to call SOCK_ShutdownAPI() after
 * all threads are exited.  SOCK_SetErrHandler() and SOCK_SetErrStream()
 * should be called under the critical section(e.g. mutex) protection.
 *
 * As soon as the API is initialized it becomes relatively MT-safe, but
 * you must not handle the same LSOCK or SOCK from different threads
 * simultaneously.
 *
 * Another way of dealing with this is to provide your own MT locking
 * function (see SOCK_MT_SetGlobalCriticalSection). This will also
 * guarantee the proper MT protection should some other SOCK functions
 * start to access any static data in the future.
 */


/* Set the MT critical section lock/unlock callback function (and its data).
 * This function will be used to protect the package's static variables
 * used by SOCK_InitializeAPI(), SOCK_ShutdownAPI(),  SOCK_SetErrHandler()
 * and SOCK_SetErrStream() (and maybe other in the future) from being
 * accessed/changed by several threads simultaneously.
 * It also makes sure that your error posting callback is called within
 * this critical section.
 * NOTE:  this function itself is so NOT MT-safe!..
 */
typedef enum {
  eSOCK_MT_Lock,   /* lock    critical section        */
  eSOCK_MT_Unlock, /* unlock  critical section        */
  eSOCK_MT_Cleanup /* cleanup critical section's data */
} ESOCK_MT_Locking;

typedef void (*FSOCK_MT_CSection)(void* data, ESOCK_MT_Locking what);

extern void SOCK_MT_SetCriticalSection
(FSOCK_MT_CSection func,
 void*             data
);



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


/* Initialize all internal/system data & resources to be used by the SOCK API.
 * NOTE: you can safely call it more than once; just, all calls after the first
 *       one will have no result. 
 * NOTE:
 *  Usually, SOCK API does not require an explicit initialization -- as it is
 *  guaranteed to initialize itself automagically, in one of API functions,
 *  when necessary. But, see the "MT-safety" remark above.
 */
extern ESOCK_Status SOCK_InitializeAPI(void);


/* Cleanup; destroy all internal/system data & resources used by the SOCK API.
 * ATTENTION:  no function from the SOCK API should be called after this call!
 * NOTE: it also calls SOCK_SetErrHandler(0,0,0) and then
 *                     SOCK_MT_SetCriticalSection(0,0).
 * NOTE: you can safely call it more than once; just, all calls after the first
 *       one will have no result. 
 */
extern ESOCK_Status SOCK_ShutdownAPI(void);



/******************************************************************************
 *  ERROR HANDLING
 */


/* Return verbal description of an error code
 */
extern const char* SOCK_StatusStr(ESOCK_Status err_code);


/* Return verbal description of an error severity
 */
extern const char* SOCK_PostSeverityStr(ESOCK_PostSeverity sev);


/* Set global error handling callback(no err.handling if "func" is passed zero)
 * The "cleanup" function(in non-NULL) gets called with the previously set
 * "data"(or "stream") whenever the "data" is changed as a result of call to
 * SOCK_SetErrHandler() or SOCK_SetErrStream().
 */
typedef void (*FSOCK_ErrHandler)(void* data, const char* message,
                                 ESOCK_PostSeverity sev);
typedef void (*FSOCK_ErrCleanup)(void* data);

extern void SOCK_SetErrHandler
(FSOCK_ErrHandler func,    /* callback function */
 void*            data,    /* "data" arg for FSOCK_ErrHandler/Cleanup */
 FSOCK_ErrCleanup cleanup  /* cleanup function */
 );


/* Auxiliary -- to post errors to "stream" (no err.posting if passed zero)
 */
extern void SOCK_SetErrStream
(FILE*            stream,
 FSOCK_ErrCleanup cleanup /* cleanup function ("data" <--> "stream")*/
 );



/******************************************************************************
 *  LISTENING SOCKET
 */


/* [SERVER-side]  Create and initialize the server-side(listening) socket
 * (socket() + bind() + listen())
 * NOTE:  on some systems, "backlog" can be silently limited down to 128(or 5).
 */
extern ESOCK_Status LSOCK_Create
(unsigned short port,    /* [in]  the port to listen at                  */
 unsigned short backlog, /* [in]  maximal # of pending connections       */
 LSOCK*         lsock    /* [out] handle of the created listening socket */
 );


/* [SERVER-side]  Accept connection from a client.
 * NOTE: the "*timeout" is for this accept() only.  To set i/o timeout,
 *       use SOCK_SetTimeout();  the timeout is infinite by default.
 */
extern ESOCK_Status LSOCK_Accept
(LSOCK                lsock,    /* [in]  handle of a listening socket */
 const SSOCK_Timeout* timeout,  /* [in]  timeout (infinite if NULL)   */
 SOCK*                sock      /* [out] handle of the created socket */
 );


/* [SERVER-side]  Close the listening socket, destroy relevant internal data.
 */
extern ESOCK_Status LSOCK_Close(LSOCK lsock);


/* Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 */
extern ESOCK_Status LSOCK_GetOSHandle
(LSOCK  lsock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* exact(!) size of the expected OS handle */
 );



/******************************************************************************
 *  SOCKET
 */


/* [CLIENT-side]  Connect client to another(server-side, listening) socket
 * (socket() + connect() [+ select()])
 */
extern ESOCK_Status SOCK_Create
(const char*          host,    /* [in]  host to connect to           */
 unsigned short       port,    /* [in]  port to connect to           */
 const SSOCK_Timeout* timeout, /* [in]  the connect timeout          */
 SOCK*                sock     /* [out] handle of the created socket */
);


/* [CLIENT-side]  Close the socket referred by "sock" and then connect
 * it to another "host:port";  fail if it takes more than "timeout".
 * (close() + connect() [+ select()])
 *
 * HINT: if "host" is NULL then connect to the same host address as before;
 *       if "port" is zero then connect to the same port # as before.
 * NOTE: "new" socket inherits the old i/o timeouts,
 */
extern ESOCK_Status SOCK_Reconnect
(SOCK                 sock,    /* [in] handle of the socket to reconnect */
 const char*          host,    /* [in] host to connect to                */
 unsigned short       port,    /* [in] port to connect to                */
 const SSOCK_Timeout* timeout  /* [in] the connect timeout               */
 );


/* [CLIENT-side]  Close the connection, destroy relevant internal data.
 * The "sock" handle goes invalid after this function call, regardless
 * of whether the call was successful or not.
 * NOTE: if write timeout was specified then it blocks until either all
 *       unsent data are sent or until the timeout expires.
 */
extern ESOCK_Status SOCK_Close(SOCK sock);


/* Block on the socket until either read/write (dep. on "dir") is
 * available or timeout expires (if "timeout" is NULL then assume it infinite)
 */
extern ESOCK_Status SOCK_Wait
(SOCK                 sock,
 ESOCK_Direction      dir,    /* what direction to wait i/o event(s) in */
 const SSOCK_Timeout* timeout
 );


/* Specify timeout for the connection i/o (see SOCK_[Read|Write|Close] funcs).
 * If "timeout" is NULL then set the timeout to be infinite;
 * NOTE: the default timeout is infinite (wait "ad infinitum" on i/o).
 */
extern ESOCK_Status SOCK_SetTimeout
(SOCK                 sock,
 ESOCK_Direction      dir,     /* [in]  i/o events to specify t-out for */
 const SSOCK_Timeout* timeout  /* [in]  new timeout value to set        */
 );


/* Get the connection's i/o timeout (NULL if the timeout is infinite).
 * NOTE:  the returned timeout is guaranteed to be valid at least until
 *        the SOCK is closed or SOCK_SetTimeout is called for this SOCK.
 */
extern const SSOCK_Timeout* SOCK_GetReadTimeout (SOCK sock);
extern const SSOCK_Timeout* SOCK_GetWriteTimeout(SOCK sock);


/* Read up to "size" bytes from "sock" to the mem.buffer pointed by "buf".
 * In "*n_read", return the number of succesfully read bytes.
 * If there is no data available to read (also, if eSOCK_Persist and cannot
 * read exactly "size" bytes) and the timeout(see SOCK_SetTimeout) is expired
 * then return eSOCK_ETimeout.
 * NOTE: Theoretically, eSOCK_Closed may indicate an empty message
 *       rather than a real closure of the connection...
 */
typedef enum {
  eSOCK_Read,   /* read the presently available data */
  eSOCK_Peek,   /* eSOCK_Read, but dont discard the read data from inp.queue */
  eSOCK_Persist /* try to read/peek exactly "size" bytes -- read again */
} ESOCK_Read;

extern ESOCK_Status SOCK_Read
(SOCK       sock,
 void*      buf,    /* [out] data buffer to read to                          */
 size_t     size,   /* [in]  max # of bytes to read to "buf"                 */
 size_t*    n_read, /* [out] # of bytes read (0 < n_read <= size on success) */
 ESOCK_Read how     /* [in]  how to read                                     */
 );


/* Push the specified data back to the socket input queue(to the socket's
 * internal read buffer). These can be any data, not necessarily the data
 * previously read from the socket.
 */
extern ESOCK_Status SOCK_PushBack
(SOCK        sock,
 const void* buf,   /* [in] data to push back to the socket's local buffer */
 size_t      size   /* [in] # of bytes (starting at "buf") to pushback     */
 );


/* Return a non-zero value if the last input operation (Read, ReadPersist,
 * or Peek) has detected EOF reading the "sock".
 * NOTE:  the input operations does not return SOCK_eClosed unless there
 *        is no more data to read/peek;  thus, in the case of Peek, this is
 *        the only "non-destructive" way to check whether we already hit
 *        the EOF or we still can expect more data to come.
 */
extern int SOCK_Eof(SOCK sock);


/* Write "size" bytes from the mem.buffer "buf" to "sock".
 * In "*n_written", return the number of successfully written bytes.
 * If cannot write all data and the timeout(see SOCK_SetTimeout()) is expired
 * then return eSOCK_ETimeout.
 */
extern ESOCK_Status SOCK_Write
(SOCK        sock,
 const void* buf,       /* [in]  data to write to the socket             */
 size_t      size,      /* [in]  # of bytes (starting at "buf") to write */
 size_t*     n_written  /* [out] # of successfully written bytes         */
 );


/* Get host and port of the socket's peer.
 * If "network_byte_order" is true(non-zero) then return the host/port in the
 * network byte order; otherwise return them in the local host byte order.
 */
extern void SOCK_GetAddress
(SOCK            sock,
 unsigned int*   host,               /* [out] the peer's host (can be NULL) */
 unsigned short* port,               /* [out] the peer's port (can be NULL) */
 int             network_byte_order  /* [in]  host/port byte order          */
 );


/* Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 */
extern ESOCK_Status SOCK_GetOSHandle
(SOCK   sock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* exact(!) size of the expected OS handle */
 );



/******************************************************************************
 *  AUXILIARY network-specific functions (added for the portability reasons)
 */


/* Return zero on success, non-zero on error.  See BSD gethostname().
 */
extern int SOCK_gethostname
(char*  name,    /* [out] (guaranteed to be '\0'-terminated)      */
 size_t namelen  /* [in]  max # of bytes allowed to put to "name" */
 );


/* Return zero on success.  Vaguely related to the BSD inet_ntoa().
 */
extern int SOCK_host2inaddr
(unsigned int host,   /* [in]  must be in the network byte-order           */
 char*        buf,    /* [out] to be filled by smth. like "123.45.67.89\0" */
 size_t       buflen  /* [in]  max # of bytes allowed to put to "buf"      */
 );

/* See man for the BSD htonl().
 */
extern unsigned int SOCK_htonl
(unsigned int value
 );


#ifdef __cplusplus
}
#endif


#endif /* NCBI_SOCKET__H */
