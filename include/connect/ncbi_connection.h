#ifndef NCBI_CONNECTION__H
#define NCBI_CONNECTION__H

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
 *   Generic API to open and handle connection to an abstract i/o service.
 *   Several methods can be used to establish the connection, and each of them
 *   yields in a simple handle(of type "CONN") that contains a handle(of type
 *   "CONNECTOR") to a data and methods implementing the generic connection i/o
 *   operations. E.g. this API can be used to:
 *     1) connect using HTTPD-based dispatcher (to NCBI services only!);
 *     2) hit a CGI script;
 *     3) connect to a bare socket at some "host:port";
 *     4) whatever else can fit this paradigm -- see the
 *        SConnectorTag-related structures;  e.g. it could be a plain file i/o
 *        or even a memory area.
 *
 *  See in "ncbi_connector.h" for the detailed specification of the underlying
 *  connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.8  2001/06/28 22:00:31  lavr
 * Added function: CONN_SetCallback
 * Added callback: eCONN_OnClose
 *
 * Revision 6.7  2001/04/24 21:19:29  lavr
 * Introduced CONN_DEFAULT_TIMEOUT for use as a CONNECTOR-specific timeout
 *
 * Revision 6.6  2001/03/02 20:07:33  lavr
 * Typo fixed
 *
 * Revision 6.5  2001/02/09 17:33:38  lavr
 * CONN_GetType added
 *
 * Revision 6.4  2001/01/03 22:29:22  lavr
 * Changed IOStatus -> Status
 *
 * Revision 6.3  2000/12/29 17:43:42  lavr
 * Pretty printed;
 * Reconnect renamed to ReInit with ability to close current connector
 *
 * Revision 6.2  2000/04/07 19:59:47  vakatov
 * Moved forward-declaration of CONNECTOR from "ncbi_connection.h"
 * to "ncbi_connector.h"
 *
 * Revision 6.1  2000/03/24 22:52:20  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_connector.h>


#ifdef __cplusplus
extern "C" {
#endif


struct SConnectionTag;
typedef struct SConnectionTag* CONN;      /* connection handle */


/* Compose all data necessary to establish a new connection
 * (merely bind it to the specified connector). Unsuccessful completion
 * sets conn to 0, and leaves connector intact (can be used again).
 * NOTE:  The real connection will not be established right away. Instead,
 *        it will be established at the moment of the first call to one of
 *        "Wait", "WaitAsync", "Write" or "Read".
 * NOTE:  Initial timeout values are set to CONN_DEFAULT_TIMEOUT, meaning
 *        that connector-specific timeouts are in force for the connection.
 */
extern EIO_Status CONN_Create
(CONNECTOR connector,  /* [in]  connector                        */
 CONN*     conn        /* [out] handle of the created connection */
 );


/* Reinit, using "connector".
 * If "conn" is already opened then close the current connection at first,
 * even if "connector" is just the same as the current connector.
 * If "connector" is NULL then close and destroy current connector, and leave
 * connection empty (effective way to destroy connector(s)).
 * NOTE:  Although it closes the previous connection immediately, however it
 *        does not establish the new connection right away -- it postpones
 *        until the first call to "Wait", "WaitAsync", "Write" or "Read".
 */
extern EIO_Status CONN_ReInit
(CONN      conn,      /* [in] connection handle */
 CONNECTOR connector  /* [in] new connector     */
 );


/* Specify timeout for the connection i/o (including "CONNECT" and "CLOSE").
 * This function can be called at any time during the connection lifetime.
 * NOTE: if "new_timeout" is NULL then set the timeout to the maximum.
 * NOTE: if "new_timeout" is CONN_DEFAULT_TIMEOUT then underlying
 *       connector-specific value is used (this is the default).
 */
extern void CONN_SetTimeout
(CONN            conn,        /* [in] connection handle */
 EIO_Event       event,       /* [in] i/o direction     */
 const STimeout* new_timeout  /* [in] new timeout       */
 );


/* Retrieve current timeout (return NULL if it is infinite).
 * The returned pointer is guaranteed to point to the valid timeout structure
 * (or NULL, or CONN_DEFAULT_TIMEOUT) until the next CONN_SetTimeout or
 * CONN_Close function call.
 */
extern const STimeout* CONN_GetTimeout
(CONN      conn,  /* [in]  connection handle                  */
 EIO_Event event  /* [in]  i/o direction, not "eIO_ReadWrite" */
 );


/* Block on the connection until it becomes available for either read or
 * write (dep. on "event"), or until the timeout expires, or until any error.
 * NOTE:  "timeout" can also be one of two special values:
 *         NULL (means infinite), CONN_DEFAULT_TIMEOUT (connector-defined).
 */
extern EIO_Status CONN_Wait
(CONN            conn,    /* [in] connection handle                  */
 EIO_Event       event,   /* [in] can be eIO_Read or eIO_Write only! */
 const STimeout* timeout  /* [in] the maximal wait time              */
 );


/* Write "size" bytes from the mem.buffer "buf" to the connection.
 * In "*n_written", return the number of successfully written bytes.
 * If cannot write all data and the timeout (see CONN_SetTimeout()) is expired
 * then return eIO_Timeout.
 */
extern EIO_Status CONN_Write
(CONN        conn,      /* [in]  connection handle                     */ 
 const void* buf,       /* [in]  pointer to the data buffer to write   */ 
 size_t      size,      /* [in]  # of bytes to write                   */ 
 size_t*     n_written  /* [out, non-NULL] # of actually written bytes */
 );


/* Explicitly flush to the connection all data written by "CONN_Write()".
 * NOTE:  CONN_Close() and CONN_Read() always call CONN_Flush() before reading.
 */
extern EIO_Status CONN_Flush
(CONN conn  /* [in] connection handle */ 
 );


/* Read up to "size" bytes from the connection to mem.buffer pointed by "buf".
 * In "*n_read", return the number of successfully read bytes.
 * If there is absolutely no data available to read and the timeout (see
 * CONN_SetTimeout()) is expired then return eIO_Timeout (and "*n_read" := 0).
 * The arg "how" means:
 *   eIO_Plain   -- read the presently available data only and return
 *   eIO_Peek    -- eREAD_Plain, but dont discard the read data from inp. queue
 *   eIO_Persist -- try to read exactly "size" bytes;  return eIO_Timeout if
 *                  could not read the requested # of bytes, and the timeout
 *                  has expired.
 */
extern EIO_Status CONN_Read
(CONN           conn,   /* [in]  connection handle                  */
 void*          buf,    /* [out] memory buffer to read to           */
 size_t         size,   /* [in]  max. # of bytes to read            */
 size_t*        n_read, /* [out, non-NULL] # of actually read bytes */
 EIO_ReadMethod how     /* [in]  read/peek | persist                */
 );


/* Obtain status of the last IO operation. This is NOT a completion
 * code of the last CONN-call, but rather a status from the low-level
 * connector layer.
 */
extern EIO_Status CONN_Status
(CONN      conn,   /* [in]  connection handle       */
 EIO_Event dir     /* [in] = {eIO_Read | eIO_Write} */
 );


/* Close the connection, destroy relevant internal data.
 * NOTE: whatever error code is returned, the connection handle "conn"
 *       will become invalid (so, you should not use it anymore).
 */
extern EIO_Status CONN_Close
(CONN conn  /* [in] connection handle */
 );


/* Get a verbal connection type as a character string.
 * Note that the returned value is only valid until the next
 * I/O operation in the connection. Return value 0 denotes
 * unknown connection type.
 */
extern const char* CONN_GetType
(CONN conn  /* [in]  connection handle */ 
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
#define CONN_N_CALLBACKS 1
} ECONN_Callback;

typedef void (*FConnCallback)(CONN conn, ECONN_Callback type, void* data);

typedef struct {
    FConnCallback func;  /* Function to call on event                */
    void*         data;  /* Data to pass to the callback as last arg */
} SCONN_Callback;

EIO_Status CONN_SetCallback
(CONN                  conn,    /* [in]  connection to set callback for     */
 ECONN_Callback        type,    /* [in]  callback type                      */
 const SCONN_Callback* new_cb,  /* [in]  callback to set (may be 0)         */
 SCONN_Callback*       old_cb   /* [out] to save old callback at (may be 0) */
);


#ifdef IMPLEMENTED__CONN_WaitAsync
/* Wait for an asynchronous i/o event, then call the specified handler.
 * In the "handler" function:
 *   "event" - is the i/o direction where the async. event happened
 *   "status"    - must be "eIO_Success" if it is ready for i/o
 *   "data"      - callback data(passed as "data" in CONN_WaitAsync)
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
 EIO_Event         event,     /* [in] i/o direction     */
 FConnAsyncHandler handler,   /* [in] callback function */
 void*             data,      /* [in] callback data     */
 FConnAsyncCleanup cleanup    /* [in] cleanup procedure */
 );
#endif /* IMPLEMENTED__CONN_WaitAsync */


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CONNECTION__H */
