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
 *   operations.  E.g. this API can be used to:
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
#include <connect/ncbi_socket.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


struct SConnectionTag;
typedef struct SConnectionTag* CONN;  /**< connection handle */


/** CONN flags should be kept compatible with CConn_IOStream::TConn_Flags.
 */
enum ECONN_Flag {
    fCONN_Untie      = 1,  /**< do not call flush method prior to every read */
    fCONN_Supplement = 64  /**< supplement I/O with extended return codes    */
};
typedef unsigned int TCONN_Flags;  /**< bitwise OR of ECONN_Flag */


/** Create all data necessary to establish a new connection (merely bind it to
 * the specified CONNECTOR).
 * Unsuccessful completion sets "*conn" to NULL, and leaves "connector" intact
 * (can be used again).
 *
 * @note Connection is not established right away but at the moment of the
 *       first call to one of "Flush", "Wait", "Write", or "Read" methods.
 *
 * @note "Connection establishment" at this level of abstraction may differ
 *       from actual link establishment at the underlying CONNECTOR's level.
 *
 * @note Initial timeout values are set to kDefaultTimeout, meaning
 *       that CONNECTOR-specific timeouts are in force for the connection.
 *
 * @note CONN does not buffer any output data, but passes it directly to the
 *       underlying CONNECTOR.  CONN may buffer input data internally, but
 *       only in the following circumstances:  CONN_Read() with "peek",
 *       CONN_ReadLine(), or CONN_Pushback().
 *
 * @sa
 *  CONN_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_CreateEx
(CONNECTOR   connector,  /**< [in]  connector                        */
 TCONN_Flags flags,      /**< [in]  connection flags                 */
 CONN*       conn        /**< [out] handle of the created connection */
 );

/** Same as CONN_CreateEx() called with 0 in the "flags" parameter */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Create
(CONNECTOR   connector,  /**< [in]  connector                        */
 CONN*       conn        /**< [out] handle of the created connection */
 );


/** Reinit using new "connector".
 * If "conn" is already opened, then close the current connection first, even
 * if "connector" is just the same as the current CONNECTOR.  If "connector" is
 * NULL, then close and destroy the incumbent, and leave the connection empty
 * (effective way to destroy CONNECTOR(s)).
 *
 * @note Although it closes the previous connection immediately, however it
 *       does not open the new connection right away:  see notes in "Create".
 *
 * @sa
 *  CONN_Create, CONN_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_ReInit
(CONN      conn,      /**< [in] connection handle */
 CONNECTOR connector  /**< [in] new connector     */
 );


/** Get verbal representation of connection type as a character string.
 * Note that the returned value is only valid until the next I/O operation in
 * the connection.  Return value NULL denotes unknown connection type.
 */
extern NCBI_XCONNECT_EXPORT const char* CONN_GetType
(CONN conn  /**< [in] connection handle */ 
 );


/** Return a human-readable description of the connection as a character
 * '\0'-terminated string.  The string is not guaranteed to have any particular
 * format, and is intended solely for something like logging and debugging.
 * Return NULL if the connection cannot provide any description information (or
 * if it is in a bad state).  Application program must call free() to
 * deallocate the space occupied by the returned string when the returned
 * description is no longer needed.
 */
extern NCBI_XCONNECT_EXPORT char* CONN_Description
(CONN conn  /**< [in] connection handle */
 );


/** Get read ("event" == eIO_Read) or write ("event" == eIO_Write) position
 * within the connection.  Positions are advanced from 0 on, for every
 * successive byte of data, and only consider I/O that has caused calling the
 * actual CONNECTOR's "read" (i.e. pushbacks never counted, and peeks -- not
 * always) and "write" methods.  Special case:  eIO_Open as "event" causes to
 * clear *both* positions with 0, and to return 0.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_BigCount CONN_GetPosition
(CONN      conn,  /**< [in] connection handle */ 
 EIO_Event event  /**< [in] see description   */
 );


/** Specify timeout for the connection I/O, including "Connect" (aka "Open")
 * and "Close".  May be called at any time during the connection lifetime.
 *
 * @param event
 *   Can be one of eIO_Open, eIO_Read, eIO_ReadWrite, eIO_Write, and eIO_Close.
 *   In case of eIO_ReadWrite, the timeout value updates both read and write
 *   timeouts simultaneously, effectively equivalent to making this call twice
 *   with eIO_Read and eIO_Write events, respectively.
 *
 * @note If "timeout" is NULL (aka kInfiniteTimeout) then set the timeout to
 *       be infinite.
 *
 * @note If "timeout" is kDefaultTimeout then an underlying,
 *       CONNECTOR-specific value is used.
 *
 * @sa
 *   CONN_GetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetTimeout
(CONN            conn,    /**< [in] connection handle */
 EIO_Event       event,   /**< [in] I/O direction     */
 const STimeout* timeout  /**< [in] new timeout       */
 );


/** Retrieve current timeout, return NULL(kInfiniteTimeout) if it is infinite.
 *
 * @param event
 *   Can be one of eIO_Open, eIO_Read, eIO_Write, and eIO_Close.
 *   In case of the event specified as eIO_ReadWrite, the current read timeout
 *   gets actually returned with a warning logged.
 *
 * @return
 *   The returned pointer is guaranteed to point to a valid timeout structure,
 *   or to be either NULL or kDefaultTimeout until next CONN_SetTimeout()
 *   or CONN_Close().
 *
 * @sa
 *   CONN_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT const STimeout* CONN_GetTimeout
(CONN      conn,  /**< [in] connection handle                   */
 EIO_Event event  /**< [in] I/O direction, not "eIO_ReadWrite"! */
 );


/** Block on the connection until it becomes available for either reading or
 * writing (depending on "event"), until timeout expires, or until any error.
 *
 * @note "timeout" can also be one of the two special values:
 *       * NULL (for infinite timeout, also known as kInfiniteTimeout);
 *       * kDefaultTimeout (CONNECTOR-specific).
 *
 * @sa
 *  CONN_Read, CONN_Write
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Wait
(CONN            conn,    /**< [in] connection handle                        */
 EIO_Event       event,   /**< [in] can only be either of eIO_Read,eIO_Write */
 const STimeout* timeout  /**< [in] the maximal wait time                    */
 );


/** Write up to "size" bytes from the buffer "buf" to the connection.
 * Return the number of actually written bytes in "*n_written".  May not return
 * eIO_Success if no data at all can be written before either the write timeout
 * expires or an error occurs.  Parameter "how" modifies the write behavior:
 * * @var eIO_WritePlain
 *                        return immediately after having written as little
 *                        as 1 byte of data (return eIO_Success), or if an
 *                        error has occurred (and "*n_written == 0");
 * * @var eIO_WritePersist
 *                        return only after having written all of the data from
 *                        "buf" (return eIO_Success), or if an error has
 *                        occurred (fewer bytes written, non-eIO_Success).
 *
 * @note if "conn" has the fCONN_Supplement flag set, then the return code can
 *       be non-eIO_Success even with some data written (as indicated by
 *       "*n_written"), to signify that an error has occurred _past_ the just
 *       written block of data.
 *
 * @note See CONN_SetTimeout() for how to set the write timeout.
 *
 * @sa
 *  CONN_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Write
(CONN            conn,       /**< [in]  connection handle                    */
 const void*     buf,        /**< [in]  pointer to the data buffer to write  */
 size_t          size,       /**< [in]  # of bytes to write                  */
 size_t*         n_written,  /**< [out] non-NULL, # of actually written bytes*/
 EIO_WriteMethod how         /**< [in]  eIO_WritePlain or eIO_WritePersist   */
 );


/** Push "size" bytes from the buffer "data" back into connection.
 * Return eIO_Success on success (including when pushing back nothing if
 * "size" is zero -- the "data" pointer is ignored then), other code on error.
 *
 * @note The data pushed back may not necessarily be the same as previously
 *       obtained from the connection.
 *
 * @note Upon a following read operation, the pushed back data are taken out
 *       first.
 *
 * @note As the pushback data do not actually go back into the underlying
 *       CONNECTOR (but stored internally into a pending input buffer of CONN),
 *       that can desynchronize the CONNECTOR (so use it wisely).
 *
 * @sa
 *  CONN_Read, CONN_Write
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Pushback
(CONN        conn,  /**< [in] connection handle                     */
 const void* data,  /**< [in] pointer to the data being pushed back */
 size_t      size   /**< [in] # of bytes to push back               */
 );


/** Explicitly flush connection from any pending data written by CONN_Write().
 *
 * @note CONN_Flush() effectively opens connection (if it wasn't open yet).
 *
 * @note Connection considered open if underlying CONNECTOR's "Open" method
 *       has successfully executed;  an actual data link may not yet exist.
 *
 * @note CONN_Read() always calls CONN_Flush() before proceeding (unless the
 *       connection was created with fCONN_Untie);  so does CONN_Close() but
 *       only if the connection is already open.
 *
 * @sa
 *  CONN_Read, CONN_Write, CONN_Close
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Flush
(CONN        conn  /**< [in] connection handle */
 );


/** Read up to "size" bytes from connection to the buffer pointed to by "buf".
 * Return the number of actually read bytes in "*n_read".  May not return
 * eIO_Success if no data at all can be read before either the read timeout
 * expires or an error occurs.  Parameter "how" modifies the read behavior:
 * * @var eIO_ReadPlain
 *                         return immediately after having read as many as just
 *                         1 byte from connection (return eIO_Success), or
 *                         if an error has occurred (and "*n_read == 0");
 * * @var eIO_ReadPeek
 *                         eIO_ReadPlain but don't discard data read from CONN,
 *                         the peek'ed data get extracted from CONNECTOR but
 *                         stored internally in the CONN buffer, a non-peek
 *                         read takes data (if any) from that buffer first;
 * * @var eIO_ReadPersist
 *                         return only after having filled full "buf" with data
 *                         (exactly "size" bytes, eIO_Success), or if an error
 *                         has occurred (fewer bytes, non-eIO_Success).
 *
 * @note when reading from internal CONN's pending input buffer (that keeps
 *       data from previous peeks or pushbacks, eIO_ReadPlain and eIO_ReadPeek
 *       behave differently:  if there was at least some data obtained from
 *       the buffer, then eIO_ReadPlain returns immediately with that data,
 *       and does not call CONNECTOR's "Read" method;  on the other hand,
 *       if eIO_ReadPeek received less than the requested amount of data from
 *       the buffer, then it proceeds with CONNECTOR's "Read" method to obtain
 *       more data -- this behavior is to prevent peeks from spinning, because
 *       all the same data are otherwise persistenly available from the pending
 *       buffer, and I/O cannot progress with getting more data.
 *
 * @note if "conn" has the fCONN_Supplement flag set, then the return code can
 *       be non-eIO_Success even with some read data (indicated by "*n_read"),
 *       to signify that an error has been _following_ the just read block of
 *       data (e.g. eIO_Closed for EOF encountered).
 *
 * @note See CONN_SetTimeout() for how to set the read timeout.
 *
 * @sa
 *  CONN_SetTimeout, CONN_ReadLine
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Read
(CONN           conn,    /**< [in]  connection handle                  */
 void*          buf,     /**< [out] memory buffer to read to           */
 size_t         size,    /**< [in]  max. # of bytes to read            */
 size_t*        n_read,  /**< [out] non-NULL, # of actually read bytes */
 EIO_ReadMethod how      /**< [in]  peek/read/persist                  */
 );


/** Read up to "size" bytes from connection into a string buffer pointed to by
 * "line".  Stop reading if either '\n' or an error is encountered.  Replace
 * '\n' with '\0'.  Upon return, "*n_read" contains the number of characters
 * stored in "line", not including the terminating '\0'.  If there was not
 * enough space provided in "line" to accomodate the '\0'-termination, then
 * all "size" bytes are used up, and "*n_read" is equal to "size" upon return -
 * this is the _only_ case when "line" will _not_ be '\0'-terminated.
 *
 * Return code advises the caller whether another read can be attempted:
 *   * eIO_Success -- read completed successfully, keep reading;
 *   * other code  -- an error occurred, and further read attempt may fail.
 *
 * @note
 *   This call may cause some data read from the underlying CONNECTOR to be
 *   stored in an internal pending input buffer of CONN.
 *
 * This call utilizes eIO_Read timeout as set by CONN_SetTimeout().
 *
 * @sa
 *  CONN_Read, CONN_SetTimeout
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_ReadLine
(CONN    conn,   /**< [in]  connection handle */
 char*   line,   /**< [out] buffer to read to */
 size_t  size,   /**< [in]  buffer size       */
 size_t* n_read  /**< [out] line length       */
 );


/** Obtain status of the last I/O operation.  This is NOT a completion code of
 * the last CONN call, but rather some status from a lower level  CONNECTOR's
 * layer (if available).
 *
 * @note eIO_Open as "dir" checks whether the connection is in an open state
 *       (which means the underlying CONNECTOR has been successfully opened,
 *       but does not assure/check for availability of any data or I/O), and
 *       returns eIO_Success if it is open, or an error code otherwise.
 *
 * @par A special case of eIO_ReadWrite clears internally cached read and write
 *      status (so that any following CONN_Status() call with either eIO_Read
 *      or eIO_Write would have to access the underlying CONNECTOR), and
 *      otherwise is equivalent to eIO_Open.
 *
 * @sa
 *  CONN_Create, CONN_Read, CONN_Write, CONN_Flush
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Status
(CONN      conn,  /**< [in] connection handle                */
 EIO_Event dir    /**< [in] eIO_Open, eIO_Read, or eIO_Write */
 );


/** Close the connection and destroy all relevant internal data.
 * @note  Whatever the error code is returned, the connection handle "conn"
 *        will have become invalid (so, it should not be used anymore).
 *
 * @sa
 *  CONN_Create
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_Close
(CONN conn  /**< [in] connection handle */
 );


/** Set user callback function to be invoked upon an event specified by the
 * callback type.  Note that the callback function gets always called prior to
 * the event to happen, e.g. the eCONN_OnClose callback is called when the
 * connection is about to close, but has not yet been closed.
 *
 * @par
 * The callback function is supplied with 3 arguments:  the connection handle,
 * a type of event (except for eCONN_OnTimeout), and a user data (specified
 * when the callback was set).
 *
 * @par
 * When eCONN_OnTimeout callback occurs, the callback type eCONN_OnTimeout gets
 * OR'ed with I/O direction, which has timed out:  eIO_Open (when opening),
 * eIO_Read, eIO_Write, or both (when flushing), then passed in the "type"
 * argument.
 *
 * @par
 * CONN_SetCallback() stores previous callback in "old_cb" (if it is not NULL).
 *
 * @par
 * The callbacks remain valid until they are explicitly changed / de-activated
 * or the connection becomes closed.
 *
 * @par
 * This also means that if a callback is intercepted and then relayed to an old
 * handler, the interceptor may not always assume the callback remains set, and
 * so in general must reinstate itself upon each upcall of the old handler.
 *
 * @par
 * Normally, callback would return eIO_Success and let the operation continue;
 * non-eIO_Success return value causes it to be returned to the caller level
 * (but possibly with some processing already completed by then, e.g. such as
 * a partial read for eCONN_OnRead from an internal connection buffer).
 *
 * @note eIO_Interrupt returned from a callback switches connection into a
 *       cancelled state irreversibly, causing any further I/O for this handle
 *       to fail with eIO_Interrupt.
 *
 * @note non-eIO_Success from an eCONN_OnClose callback cannot postpone the
 *       connection closure (but the error code is still passed through to the
 *       caller).
 *
 * @note by returning eIO_Success, eCONN_OnTimeout restarts the I/O that has
 *       timed out (with possibly eCONN_OnOpen, eCONN_OnRead, eCONN_OnWrite, or
 *       eCONN_OnFlush callbacks invoked again, if they are set).
 *
 * @note eCONN_OnTimeout for connection open is only called if eCONN_OnOpen
 *       was also set for that connection.
 *
 * @note there's no timeout callback on connection close -- if timeout occurs,
 *       the connection is still closed with eIO_Timeout status.
 *
 * @sa
 *  CONN_Read, CONN_Write, CONN_Close
 */
typedef enum {
    eCONN_OnClose   = 0,  /**< NB: CONN has been flushed prior to the call   */
    eCONN_OnRead    = 1,  /**< Read from CONNECTOR is about to occur         */
    eCONN_OnWrite   = 2,  /**< Write to CONNECTOR is about to occur          */
    eCONN_OnFlush   = 3,  /**< About to be flushed (NB: == eIO_ReadWrite)    */
    eCONN_OnTimeout = 4,  /**< CONNECTOR I/O has timed out                   */
    eCONN_OnOpen    = 8   /**< Call prior to open (NB: "conn" still closed)  */
} ECONN_Callback;
#define CONN_N_CALLBACKS  6

typedef unsigned int TCONN_Callback;  /* Bitwise ECONN_Callback for time-out */

typedef EIO_Status (*FCONN_Callback)(CONN conn,TCONN_Callback type,void* data);

typedef struct {
    FCONN_Callback func;  /**< function address to call on the event         */
    void*          data;  /**< data to pass to the callback as its last arg  */
} SCONN_Callback;

extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetCallback
(CONN                  conn,    /**< [in]  connection to set callback for    */
 ECONN_Callback        type,    /**< [in]  callback type                     */
 const SCONN_Callback* new_cb,  /**< [in]  callback to set (NULL to reset)   */
       SCONN_Callback* old_cb   /**< [out] to save old callback at (may be 0)*/
);


/** Get an underlying SOCK handle for connection that is implemented as a
 * socket.  Non-eIO_Success return code guarantees "*sock" is NULL.  Set
 * "*sock" to NULL when no socket handle can be obtained.
 *
 * @warning The returned SOCK object remains in use by the connection.
 *
 * @sa
 *  SOCK, SOCK_GetOSHandleEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_GetSOCK
(CONN  conn,  /**< [in]  connection handle            */
 SOCK* sock   /**< [out] non-NULL, to get the SOCK to */
 );
    

/** Set connection processing flags.
 *
 * @return
 *  eIO_Success on success, other error code on error.
 *
 * @sa
 *  CONN_CreateEx, CONN_GetFlags
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetFlags
(CONN        conn,  /**< [in]  connection handle    */
 TCONN_Flags flags  /**< [in]  new connection flags */
 );


/** Get connection processing flags currently in effect.
 *
 * @return
 *  Current value of the flags.
 *
 * @sa
 *  CONN_CreateEx, CONN_SetFlags
 */
extern NCBI_XCONNECT_EXPORT TCONN_Flags CONN_GetFlags
(CONN conn  /**< [in]  connection handle */
 );


/** Associate an arbitraty user data pointer with the connection.
 * The pointer is not used by the connection itself but is retrievable with
 * CONN_GetUserData() in the user's code as long as the CONN handle remains
 * valid.  Successive calls to CONN_SetUserData() replace the pointer value.
 *
 * @return
 *  eIO_Success on success, other error code on error.
 *
 * @sa
 *  CONN_Create, CONN_GetUserData
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_SetUserData
(CONN  conn,  /**< [in]  connection handle */
 void* data   /**< [in]  user data pointer */
 );


/** Get current value of the user's data pointer last associated with the
 * connection, or NULL (if CONN is NULL or no pointer is currently set).
 *
 * @return
 *  Current value of the user pointer.
 *
 * @sa
 *  CONN_Create, CONN_SetUserData
 */
extern NCBI_XCONNECT_EXPORT void* CONN_GetUserData
(CONN conn  /**< [in]  connection handle */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNECTION__H */
