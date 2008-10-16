#ifndef CONNECT___NCBI_CONNECTOR__H
#define CONNECT___NCBI_CONNECTOR__H

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
 *   Specifications to implement a connector("CONNECTOR") to be used to open
 *   and handle connection("CONN", see also in "ncbi_connection.[ch]") to an
 *   abstract I/O service. This is generally not for the public use.
 *   It is to be used in the modules that implement a particular connector.
 *
 */

#include <connect/ncbi_core.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


struct SConnectorTag;
typedef struct SConnectorTag* CONNECTOR;  /* connector handle */


/* Function type definitions for the connector method table.
 * The arguments & the behavior of "FConnector***" functions are mostly just
 * the same as those for their counterparts "CONN_***" in ncbi_connection.h.
 * First argument of these functions accepts a real connector handle
 * rather than a connection handle("CONN").
 * In every call, which has STimeout as an argument, this argument
 * can be either NULL, CONN_DEFAULT_TIMEOUT, or other non-NULL pointer,
 * pointing to finite timeout structure. NULL is treated as an infinite
 * timeout, while CONN_DEFAULT_TIMEOUT means to use any timeout, which
 * is somehow pre-defined by the connector itself.
 */


/* Get the name of the connector (may be NULL on error)
 */
typedef const char* (*FConnectorGetType)
(CONNECTOR       connector
 );


/* Get the human readable connector's description (may be NULL on error)
 */
typedef char* (*FConnectorDescr)
(CONNECTOR       connector
 );


/* Open connection. Used to setup all related data structures,
 * but not necessarily has to actually open the data channel.
 */
typedef EIO_Status (*FConnectorOpen)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* Wait until either read or write (dep. on the "direction" value) becomes
 * available, or until "timeout" expires, or until error occurs.
 * NOTE 1:  FConnectorWait is guaranteed to be called after FConnectorOpen,
 *          and only if FConnectorOpen returned "eIO_Success".
 * NOTE 2:  "event" is guaranteed to be either "eIO_Read" or "eIO_Write".
 */
typedef EIO_Status (*FConnectorWait)
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout
 );


/* The passed "n_written" is always non-NULL, and "*n_written" is always zero.
 * It returns "eIO_Success" if at least some data have been successfully
 * written; other error code if no data at all have been written.
 * It returns the # of successfully written data (in bytes) in "*n_written".
 * NOTE:  FConnectorWrite is guaranteed to be called after FConnectorOpen,
 *        and only if the latter succeeded (returned "eIO_Success").
 */
typedef EIO_Status (*FConnectorWrite)
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout
 );


/* Flush yet unwritten output data, if any.
 * NOTE:  FConnectorFlush is guaranteed to be called after FConnectorOpen,
 *        and only if the latter succeeded (returned "eIO_Success").
 */
typedef EIO_Status (*FConnectorFlush)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* The passed "n_read" is always non-NULL, and "*n_read" is always zero.
 * It returns "eIO_Success" if at least some data have been successfully
 * read; other error code if no data at all have been read.
 * The number of successfully read bytes returned in "*n_read".
 * NOTE:  FConnectorRead is guaranteed to be called after FConnectorOpen,
 *        and only if the latter succeeded (returned "eIO_Success").
 */
typedef EIO_Status (*FConnectorRead)
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout
 );

/* Obtain last I/O completion code from the transport level (connector).
 * NOTE 1:  FConnectorStatus is guaranteed to be called after FConnectorOpen,
 *          and only if the latter succeeded (returned "eIO_Success").
 * NOTE 2:  "direction" is guaranteed to be either "eIO_Read" or "eIO_Write".
 * NOTE 3:  Should return "eIO_Success" in case of inexistent (incomplete)
 *          low level transport, if any.
 */
typedef EIO_Status (*FConnectorStatus)
(CONNECTOR       connector,
 EIO_Event       direction
 );
          

/* Close data link (if any) and cleanup related data structures.
 * NOTE 1:  FConnectorClose is guaranteed to be called after FConnectorOpen,
 *          and only if the latter succeeded (returned "eIO_Success").
 * NOTE 2:  FConnectorFlush gets called before FConnectorClose automatically.
 */
typedef EIO_Status (*FConnectorClose)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* Standard set of connector methods to handle a connection (corresponding
 * connectors are also here), part of connection handle("CONN").
 */
typedef struct {
    FConnectorGetType   get_type;    CONNECTOR c_get_type;
    FConnectorDescr     descr;       CONNECTOR c_descr;
    FConnectorOpen      open;        CONNECTOR c_open;
    FConnectorWait      wait;        CONNECTOR c_wait;
#ifdef IMPLEMENTED__CONN_WaitAsync
    FConnectorWaitAsync wait_async;  CONNECTOR c_wait_async;
#endif
    FConnectorWrite     write;       CONNECTOR c_write;
    FConnectorFlush     flush;       CONNECTOR c_flush;
    FConnectorRead      read;        CONNECTOR c_read;
    FConnectorStatus    status;      CONNECTOR c_status;
    FConnectorClose     close;       CONNECTOR c_close;
    const STimeout*     default_timeout; /* default timeout pointer     */
    STimeout            default_tmo;     /* storage for default_timeout  */
    CONNECTOR           list;
} SMetaConnector;


#define CONN_TWO2ONE(a, b)   a##b

#define CONN_SET_METHOD(meta, method, function, connector) \
    do {                                                   \
        meta->method = function;                           \
        meta->CONN_TWO2ONE(c_,method) = connector;         \
    } while (0);


#define CONN_SET_DEFAULT_TIMEOUT(meta, timeout)            \
    do {                                                   \
        if (timeout) {                                     \
            meta->default_timeout = &meta->default_tmo;    \
            meta->default_tmo     = *timeout;              \
        } else                                             \
            meta->default_timeout = 0;                     \
    } while (0);


/* Insert a connector in the beginning of the connection's list of connectors.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status METACONN_Add
(SMetaConnector* meta,
 CONNECTOR       connector
 );


/* Delete given "connector" all its descendants (all connectors if "connector"
 * is 0) from the connections's list of connectors. FConnectorDestroy is
 * called for each removed connector.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status METACONN_Remove
(SMetaConnector* meta,
 CONNECTOR       connector
 );


/* Upcall on request to setup virtual function table (called from connection).
 */
typedef void (*FSetupVTable)
(SMetaConnector* meta,
 CONNECTOR       connector
 );


/* Destroy connector and its data handle. This is NOT a close request!
 * Should not to be used on open connectors (that is, for those
 * FConnectorClose has to be called first prior to this call).
 */
typedef void (*FDestroy)
(CONNECTOR       connector
 );


/* Connector specification.
 */
typedef struct SConnectorTag {
    SMetaConnector*      meta;      /* back link to CONNECTION      */
    FSetupVTable         setup;     /* used in CONNECTION init      */
    FDestroy             destroy;   /* destroys handle, can be NULL */
    void*                handle;    /* data handle of the connector */
    CONNECTOR            next;      /* linked list                  */
} SConnector;


#ifdef IMPLEMENTED__CONN_WaitAsync
typedef struct {
  CONN                   conn;
  EIO_Event              event;
  FConnAsyncHandler      handler;
  void*                  data;
  FConnAsyncCleanup      cleanup;
} SConnectorAsyncHandler;

typedef void (*FConnectorAsyncHandler)
(SConnectorAsyncHandler* data,
 EIO_Event               event,
 EIO_Status              status
 );

typedef EIO_Status (*FConnectorWaitAsync)
(CONNECTOR               connector,
 FConnectorAsyncHandler  func,
 SConnectorAsyncHandler* data
 );
#endif /* IMPLEMENTED__CONN_WaitAsync */


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNECTOR__H */
