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
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    wsock32.lib
 *     [MacOS]   -DNCBI_OS_MAC      NCSASOCK -- BSD-style socket emulation lib
 *
 *********************************
 * Generic:
 *
 *  SOCK, LSOCK
 *
 *  SOCK_InitializeAPI
 *  SOCK_ShutdownAPI
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
 *  SOCK_ntoa
 *  SOCK_htonl
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2000/05/30 23:31:37  vakatov
 * SOCK_host2inaddr() renamed to SOCK_ntoa(), the home-made out-of-scratch
 * implementation to work properly with GCC on IRIX64 platforms
 *
 * Revision 6.3  2000/03/24 23:12:04  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.2  2000/02/23 22:33:38  vakatov
 * Can work both "standalone" and as a part of NCBI C++ or C toolkits
 *
 * Revision 6.1  1999/10/18 15:36:39  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 *
 * ===========================================================================
 */

#include <connect/ncbi_core.h>


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
typedef struct SOCK_tag*  SOCK;  /* socket:  handle */



/******************************************************************************
 *  Multi-Thread safety
 *
 * If you are using this API in a multi-thread application, and there is
 * more than one thread using this API, it is safe to call SOCK_InitializeAPI()
 * explicitely in the beginning of your main thread, before you run any other
 * threads, and to call SOCK_ShutdownAPI() after all threads are exited.
 *
 * As soon as the API is initialized it becomes relatively MT-safe, however
 * you still must not operate with the same LSOCK or SOCK objects from
 * different threads simultaneously.
 *
 * A MUCH BETTER WAY of dealing with this issue is to provide your own MT
 * locking callback (see CORE_SetLOCK in "ncbi_core.h"). This will also
 * guarantee the proper MT protection should some other SOCK functions
 * start to access any static data in the future.
 */



/******************************************************************************
 *   Error Logging
 *
 * Use CORE_SetLOG() from "ncbi_core.h" to setup the log handler.
 */



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


/* Initialize all internal/system data & resources to be used by the SOCK API.
 * NOTE:
 *  You can safely call it more than once; just, all calls after the first
 *  one will have no result. 
 * NOTE:
 *  Usually, SOCK API does not require an explicit initialization -- as it is
 *  guaranteed to initialize itself automagically, in one of API functions,
 *  when necessary. Yet, see the "Multi Thread safety" remark above.
 */
extern EIO_Status SOCK_InitializeAPI(void);


/* Cleanup; destroy all internal/system data & resources used by the SOCK API.
 * ATTENTION:  no function from the SOCK API should be called after this call!
 * NOTE: you can safely call it more than once; just, all calls after the first
 *       one will have no result. 
 */
extern EIO_Status SOCK_ShutdownAPI(void);



/******************************************************************************
 *  LISTENING SOCKET
 */


/* [SERVER-side]  Create and initialize the server-side(listening) socket
 * (socket() + bind() + listen())
 * NOTE:  on some systems, "backlog" can be silently limited down to 128(or 5).
 */
extern EIO_Status LSOCK_Create
(unsigned short port,    /* [in]  the port to listen at                  */
 unsigned short backlog, /* [in]  maximal # of pending connections       */
 LSOCK*         lsock    /* [out] handle of the created listening socket */
 );


/* [SERVER-side]  Accept connection from a client.
 * NOTE: the "*timeout" is for this accept() only.  To set i/o timeout,
 *       use SOCK_SetTimeout();  the timeout is infinite by default.
 */
extern EIO_Status LSOCK_Accept
(LSOCK           lsock,    /* [in]  handle of a listening socket */
 const STimeout* timeout,  /* [in]  timeout (infinite if NULL)   */
 SOCK*           sock      /* [out] handle of the created socket */
 );


/* [SERVER-side]  Close the listening socket, destroy relevant internal data.
 */
extern EIO_Status LSOCK_Close(LSOCK lsock);


/* Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 */
extern EIO_Status LSOCK_GetOSHandle
(LSOCK  lsock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* the exact(!) size of the expected OS handle */
 );



/******************************************************************************
 *  SOCKET
 */


/* [CLIENT-side]  Connect client to another(server-side, listening) socket
 * (socket() + connect() [+ select()])
 */
extern EIO_Status SOCK_Create
(const char*     host,    /* [in]  host to connect to           */
 unsigned short  port,    /* [in]  port to connect to           */
 const STimeout* timeout, /* [in]  the connect timeout          */
 SOCK*           sock     /* [out] handle of the created socket */
 );


/* [CLIENT-side]  Close the socket referred by "sock" and then connect
 * it to another "host:port";  fail if it takes more than "timeout".
 * (close() + connect() [+ select()])
 *
 * HINT: if "host" is NULL then connect to the same host address as before;
 *       if "port" is zero then connect to the same port # as before.
 * NOTE: "new" socket inherits the old i/o timeouts,
 */
extern EIO_Status SOCK_Reconnect
(SOCK            sock,    /* [in] handle of the socket to reconnect */
 const char*     host,    /* [in] host to connect to                */
 unsigned short  port,    /* [in] port to connect to                */
 const STimeout* timeout  /* [in] the connect timeout               */
 );


/* [CLIENT-side]  Close the connection, destroy relevant internal data.
 * The "sock" handle goes invalid after this function call, regardless
 * of whether the call was successful or not.
 * NOTE: if eIO_Close timeout was specified then it blocks until
 *       either all unsent data are sent or the timeout expires.
 */
extern EIO_Status SOCK_Close(SOCK sock);


/* Block on the socket until either read/write (dep. on the "event" arg) is
 * available or timeout expires (if "timeout" is NULL then assume it infinite).
 */
extern EIO_Status SOCK_Wait
(SOCK            sock,
 EIO_Event       event,  /* [in] one of:  eIO_Read, eIO_Write, eIO_ReadWrite */
 const STimeout* timeout
 );


/* Specify timeout for the connection i/o (see SOCK_[Read|Write|Close] funcs).
 * If "timeout" is NULL then set the timeout to be infinite;
 * NOTE: the default timeout is infinite (wait "ad infinitum" on i/o).
 */
extern EIO_Status SOCK_SetTimeout
(SOCK            sock,
 EIO_Event       event,   /* [in]  one of:  eIO_[Read/Write/ReadWrite/Close] */
 const STimeout* timeout  /* [in]  new timeout value to set                  */
 );


/* Get the connection's i/o timeout (or NULL, if the timeout is infinite).
 * NOTE:  the returned timeout is guaranteed to be pointing to a valid
 *        (and correct) structure in memory at least until the SOCK is closed
 *        or SOCK_SetTimeout is called for this "sock".
 */
extern const STimeout* SOCK_GetTimeout
(SOCK      sock,
 EIO_Event event  /* [in]  one of:  eIO_[Read/Write/Close] */
 );


/* Read up to "size" bytes from "sock" to the mem.buffer pointed by "buf".
 * In "*n_read", return the number of succesfully read bytes.
 * If there is no data available to read (also, if eIO_Persist and cannot
 * read exactly "size" bytes) and the timeout(see SOCK_SetTimeout) is expired
 * then return eSOCK_ETimeout.
 * NOTE: Theoretically, eSOCK_Closed may indicate an empty message
 *       rather than a real closure of the connection...
 */
extern EIO_Status SOCK_Read
(SOCK           sock,
 void*          buf,    /* [out] data buffer to read to          */
 size_t         size,   /* [in]  max # of bytes to read to "buf" */
 size_t*        n_read, /* [out] # of bytes read                 */
 EIO_ReadMethod how     /* [in]  how to read the data            */
 );


/* Push the specified data back to the socket input queue (in the socket's
 * internal read buffer). These can be any data, not necessarily the data
 * previously read from the socket.
 */
extern EIO_Status SOCK_PushBack
(SOCK        sock,
 const void* buf,  /* [in] data to push back to the socket's local buffer */
 size_t      size  /* [in] # of bytes (starting at "buf") to pushback     */
 );


/* Return a non-zero value if EOF was hit (detected) during the last
 * call to SOCK_Read("sock", ...).
 * NOTE:  the input operations do not return SOCK_eClosed unless there
 *        is no more data to read/peek;  thus, in the case of Peek, this is
 *        the only "non-destructive" way to check whether we already hit
 *        the EOF or there is still more data to come.
 */
extern int SOCK_Eof(SOCK sock);


/* Write "size" bytes from the mem.buffer "buf" to "sock".
 * In "*n_written", return the number of successfully written bytes.
 * If cannot write all data and the timeout(see SOCK_SetTimeout()) is expired
 * then return eSOCK_ETimeout.
 */
extern EIO_Status SOCK_Write
(SOCK        sock,
 const void* buf,       /* [in]  data to write to the socket                */
 size_t      size,      /* [in]  # of bytes (starting at "buf") to write    */
 size_t*     n_written  /* [out] # of successf. written bytes (can be NULL) */
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
extern EIO_Status SOCK_GetOSHandle
(SOCK   sock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* the exact(!) size of the expected OS handle */
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
extern int SOCK_ntoa
(unsigned int host,     /* [in]  must be in the network byte-order           */
 char*        buf,      /* [out] to be filled by smth. like "123.45.67.89\0" */
 size_t       buf_size  /* [in]  max # of bytes to put to "buf", >= 16 */
 );


/* See man for the BSD htonl().
 */
extern unsigned int SOCK_htonl
(unsigned int value
 );


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* NCBI_SOCKET__H */
