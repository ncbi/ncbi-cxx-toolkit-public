#ifndef CONNECT___NCBI_CONNECTION__H
#define CONNECT___NCBI_CONNECTION__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Generic API to open and handle connection to an abstract I/O service.
 *   Several methods can be used to establish the connection, and each of them
 *   yields in a simple handle(of type "CONN") that contains a handle(of type
 *   "CONNECTOR") to a data and methods implementing the generic connection I/O
 *   operations. E.g. this API can be used to:
 *     1) connect using HTTPD-based dispatcher (e.g. to NCBI services);
 *     2) hit a CGI script;
 *     3) connect to a bare socket at some "host:port";
 *     4) whatever else can fit this paradigm -- see the SConnectorTag-related
 *        structures;  e.g. it could be a plain file I/O or even a memory area.
 *
 *  See in "ncbi_connector.h" for the detailed specification of the underlying
 *  connector("CONNECTOR", "SConnectorTag") methods and data structures.
 *
 */

#include <connect/ncbi_connector.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


struct SConnectionTag;
typedef struct SConnectionTag* CONN;      /* connection handle */


/* Compose all data necessary to establish a new connection
 * (merely bind it to the specified connector). Unsuccessful completion
 * sets conn to 0, and leaves connector intact (can be used again).
 * NOTE1:  The real connection will not be established right away. Instead,
 *         it will be established at the moment of the first call to one of
 *         "Flush", "Wait", "WaitAsync", "Write", or "Read" methods.
 * NOTE2:  "Connection establishment" at this level of abstraction may differ
 *         from actual link establishment at the underlying connector's level.
 * NOTE3:  Initial timeout values are set to CONN_DEFAULT_TIMEOUT, meaning
 *         that connector-specific timeouts are in force for the connection.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Create
(CONNECTOR connector,  /* [in]  connector                        */
 CONN*     conn        /* [out] handle of the created connection */
 );


/* Reinit, using "connector".
 * If "conn" is already opened then close the current connection at first,
 * even if "connector" is just the same as the current connector.
 * If "connector" is NULL then close and destroy current connector, and leave
 * connection empty (effective way to destroy connector(s)).
 * NOTE:  Although it closes the previous connection immediately, however it
 *        does not open the new connection right away -- see notes to "Create".
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_ReInit
(CONN      conn,      /* [in] connection handle */
 CONNECTOR connector  /* [in] new connector     */
 );


/* Get verbal representation of connection type as a character string.
 * Note that the returned value is only valid until the next
 * I/O operation in the connection. Return value NULL denotes
 * unknown connection type.
 */
extern NCBI_XCONNECT_EXPORT const char* CONN_GetType
(CONN conn  /* [in]  connection handle */ 
 );


/* Return human-readable description of the connection as a character
 * 0-terminated string. The string is not guaranteed to have any
 * particular format and is intended solely for something like
 * logging and debugging. Return NULL if the connection cannot
 * provide any description information (or if it is in a bad state).
 * Application program is responsible to deallocate the space occupied
 * by the returned string calling free() when the description is no longer
 * needed.
 */
extern NCBI_XCONNECT_EXPORT char* CONN_Description
(CONN conn  /* [in]  connection handle */
 );


/* Specify timeout for the connection I/O (including "Connect" (aka "Open")
 * and "Close"). May be called at any time during the connection lifetime.
 * NOTE1:  if "new_timeout" is NULL then set the timeout to be infinite.
 * NOTE2:  if "new_timeout" is CONN_DEFAULT_TIMEOUT then underlying
 *         connector-specific value is used (this is the default).
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetTimeout
(CONN            conn,        /* [in] connection handle */
 EIO_Event       event,       /* [in] I/O direction     */
 const STimeout* new_timeout  /* [in] new timeout       */
 );


/* Retrieve current timeout (return NULL if it is infinite).
 * The returned pointer is guaranteed to point to the valid timeout structure
 * (or be either NULL or CONN_DEFAULT_TIMEOUT) until the next "SetTimeout"
 * or "Close" method's call.
 */
extern NCBI_XCONNECT_EXPORT const STimeout* CONN_GetTimeout
(CONN      conn,  /* [in] connection handle                  */
 EIO_Event event  /* [in] I/O direction, not "eIO_ReadWrite" */
 );


/* Block on the connection until it becomes available for either read or
 * write (dep. on "event"), or until the timeout expires, or until any error.
 * NOTE:  "timeout" can also be one of two special values:
 *         NULL (means infinite), CONN_DEFAULT_TIMEOUT (connector-defined).
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Wait
(CONN            conn,    /* [in] connection handle                  */
 EIO_Event       event,   /* [in] can be eIO_Read or eIO_Write only! */
 const STimeout* timeout  /* [in] the maximal wait time              */
 );


/* Write "size" bytes from the mem.buffer "buf" to the connection.
 * In "*n_written", return the number of successfully written bytes.
 * Parameter "how" modifies behavior of CONN_Write():
 * eIO_WritePlain   -- return eIO_Success if some data were written and
 *                     yet write timeout had not occurred, error otherwise;
 * eIO_WritePersist -- return eIO_Success only if all data were written and
 *                     yet write timeout had not occurred, error otherwise.
 * NOTE:  See CONN_SetTimeout() hoe to set write timeout.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Write
(CONN            conn,      /* [in]  connection handle                     */ 
 const void*     buf,       /* [in]  pointer to the data buffer to write   */ 
 size_t          size,      /* [in]  # of bytes to write                   */ 
 size_t*         n_written, /* [out, non-NULL] # of actually written bytes */
 EIO_WriteMethod how        /* [in]  eIO_WritePlain or eIO_WritePersist    */
 );


/* Push back "size" bytes from the mem.buffer "buf" into connection.
 * Return eIO_Success on success, other code on an error.
 * NOTE1:  Data pushed back are not necessarily those taken from the
 *         connection before.
 * NOTE2:  Upon successive read operation, the pushed back data are
 *         taken out first.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_PushBack
(CONN        conn,  /* [in]  connection handle                     */
 const void* buf,   /* [in]  pointer to the data being pushed back */
 size_t      size   /* [in]  # of bytes to push back               */
 );


/* Explicitly flush to the connection all data written by "CONN_Write()".
 * NOTE1:  CONN_Flush() effectively opens connection (if it wasn't open yet).
 * NOTE2:  Connection considered open if underlying connector's "Open" method
 *         has successfully executed; actual data link may not yet exist.
 * NOTE3:  CONN_Read() always calls CONN_Flush() before proceeding.
 *         So does CONN_Close() but only if connection is was open before.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Flush
(CONN        conn   /* [in] connection handle                      */
 );


/* Read up to "size" bytes from a connection to the buffer to pointed by "buf".
 * In "*n_read", return the number of successfully read bytes.
 * If there is absolutely no data available to read and the timeout (see
 * CONN_SetTimeout()) is expired then return eIO_Timeout (and "*n_read" := 0).
 * The arg "how" means:
 *   eIO_ReadPlain   -- read presently available data only and return
 *   eIO_ReadPeek    -- eIO_ReadPlain but dont discard read data from inp.queue
 *   eIO_ReadPersist -- try to read exactly "n" bytes;  return eIO_Timeout if
 *                      could not read the requested # of bytes, and read
 *                      timeout has expired.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Read
(CONN           conn,   /* [in]  connection handle                  */
 void*          buf,    /* [out] memory buffer to read to           */
 size_t         size,   /* [in]  max. # of bytes to read            */
 size_t*        n_read, /* [out, non-NULL] # of actually read bytes */
 EIO_ReadMethod how     /* [in]  read/peek | persist                */
 );


/* Read up to "size" bytes from a connection into the string buffer pointed
 * to by "line".  Stop reading if either '\n' or an error is encountered.
 * Replace '\n' with '\0'.  Upon return "*n_read" contains the number
 * of characters written to "line", not including the terminating '\0'.
 * If not enough space provided in "line" to accomodate the '\0'-terminated
 * line, then all "size" bytes are used and "*n_read" equals "size" on return.
 * This is the only case when "line" will not be '\0'-terminated.
 * Return code advises the caller whether another line read can be attempted:
 *   eIO_Success -- read completed successfully, keep reading;
 *   other code  -- an error occurred, and further attempt may fail.
 *
 * This call utilizes eIO_Read timeout as set by CONN_SetTimeout().
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_ReadLine
(CONN    conn,
 char*   line,
 size_t  size,
 size_t* n_read
 );


/* Obtain status of the last IO operation. This is NOT a completion
 * code of the last CONN-call, but rather a status from the lower level
 * connector's layer.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Status
(CONN      conn,   /* [in]  connection handle       */
 EIO_Event dir     /* [in] = {eIO_Read | eIO_Write} */
 );


/* Close the connection, destroy relevant internal data.
 * NOTE:  whatever error code is returned, the connection handle "conn"
 *        will become invalid (so, you should not use it anymore).
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Close
(CONN conn  /* [in] connection handle */
 );


/* Set user callback function to be called upon an event specified by the
 * callback type. Note that the callback function is always called prior to
 * the event to happen, e.g. the eCONN_OnClose callback is called when
 * the connection is about to close, but not closed yet.
 * The callback function is supplied with 3 arguments: connection handle,
 * type of event, and user data (specified when the callback was set).
 * CONN_SetCallback stores previous callback in "old_cb" (if it is not NULL).
 */
typedef enum {
    eCONN_OnClose = 0
} ECONN_Callback;
#define CONN_N_CALLBACKS 1

typedef void (*FConnCallback)(CONN conn, ECONN_Callback type, void* data);

typedef struct {
    FConnCallback func;  /* Function to call on event                */
    void*         data;  /* Data to pass to the callback as last arg */
} SCONN_Callback;

extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetCallback
(CONN                  conn,    /* [in]  connection to set callback for     */
 ECONN_Callback        type,    /* [in]  callback type                      */
 const SCONN_Callback* new_cb,  /* [in]  callback to set (may be 0)         */
 SCONN_Callback*       old_cb   /* [out] to save old callback at (may be 0) */
);


#ifdef IMPLEMENTED__CONN_WaitAsync
/* Wait for an asynchronous I/O event, then call the specified handler.
 * In the "handler" function:
 *   "event"  -- is the I/O direction where the async. event happened
 *   "status" -- must be "eIO_Success" if it is ready for I/O
 *   "data"   -- callback data (passed as "data" in CONN_WaitAsync())
 * If "handler" is NULL then discard the current handler, if any.
 * The "cleanup" function to be called right after the call to "handler" or
 * by CONN_Close(), or if the handler is reset by calling CONN_WaitAsync()
 * again -- whichever happens first.
 */
typedef void (*FConnAsyncHandler)
(CONN       conn,
 EIO_Event  event,
 EIO_Status status,
 void*      data
);
typedef void (*FConnAsyncCleanup)(void* data);

extern EIO_Status CONN_WaitAsync
(CONN              conn,      /* [in] connection handle */
 EIO_Event         event,     /* [in] I/O direction     */
 FConnAsyncHandler handler,   /* [in] callback function */
 void*             data,      /* [in] callback data     */
 FConnAsyncCleanup cleanup    /* [in] cleanup procedure */
 );
#endif /* IMPLEMENTED__CONN_WaitAsync */


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNECTION__H */
