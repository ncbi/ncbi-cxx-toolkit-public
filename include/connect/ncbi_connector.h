#ifndef NCBI_CONNECTOR__H
#define NCBI_CONNECTOR__H

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
 *   Specifications to implement a connector("CONNECTOR") to be used to open
 *   and handle connection("CONN", see also in "ncbi_connection.[ch]") to an
 *   abstract i/o service.
 *   This is generally not for the public use. It is to be used in the modules
 *   that implement a particular connector.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2000/12/29 17:45:07  lavr
 * Heavily modified to have a stack of connectors
 * VTable format changed; all virtual function now accept CONNECTOR as a
 * first argument (not void* as in previous versions)
 *
 * Revision 6.2  2000/04/07 19:59:49  vakatov
 * Moved forward-declaration of CONNECTOR from "ncbi_connection.h"
 * to "ncbi_connector.h"
 *
 * Revision 6.1  2000/03/24 22:52:48  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif


struct SConnectorTag;
typedef struct SConnectorTag* CONNECTOR;  /* connector handle */


/* Function type definitions for the connector method table.
 * The arguments & the behavior of "FConnector***" functions are mostly just
 * the same as those for their counterparts "CONN_***"
 * (see in "ncbi_connection.h").
 * First argument of these functions accepts a real connector handle
 * rather than a connection handle("CONN").
 */


/* Get name of the connector (must not be NULL!)
 */
typedef const char* (*FConnectorGetType)
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
 * available, or until "timeout" is expired, or until error occures.
 * NOTE 1: FConnectorWait is guaranteed to be called after FConnectorOpen,
 *         and only if FConnectorOpen returned "eIO_Success".
 * NOTE 2: "event" is guaranteed to be either "eIO_Read" or "eIO_Write".
 */
typedef EIO_Status (*FConnectorWait)
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout
 );


/* The passed "n_written" always non-NULL, and "*n_written" always zero.
 * It must return "eIO_Success" when (and only when) all requested data
 * have been succefully written.
 * It returns the # of succesfully written data (in bytes) in "*n_written".
 * NOTE: FConnectorWrite is guaranteed to be called after FConnectorOpen,
 *       and only if FConnectorOpen returned "eIO_Success".
 */
typedef EIO_Status (*FConnectorWrite)
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout
 );


/* Flush yet unwritten output data, if any.
 * NOTE: FConnectorFlush is guaranteed to be called after FConnectorOpen,
 *       and only if FConnectorOpen returned "eIO_Success".
 */
typedef EIO_Status (*FConnectorFlush)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* The passed "n_read" always non-NULL, and "*n_read" always zero.
 * NOTE: FConnectorRead is guaranteed to be called after FConnectorOpen,
 *       and only if FConnectorOpen returned "eIO_Success".
 */
typedef EIO_Status (*FConnectorRead)
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout
 );

/* Obtain last I/O completion code from the transport level (connector).
 * NOTE 1: FConnectorIOStatus is guaranteed to be called after FConnectorOpen,
 *         and only if FConnectorOpen returned "eIO_Success".
 * NOTE 2: "direction" is guaranteed to be either "eIO_Read" or "eIO_Write".
 * NOTE 3: Should return "eIO_Success" in case of inexistent (incomplete)
 *         low level transport, if any.
 */
typedef EIO_Status (*FConnectorIOStatus)
(CONNECTOR connector,
 EIO_Event direction
 );
          

/* "FConnectorFlush" gets called before "FConnectorClose" automatically.
 */
typedef EIO_Status (*FConnectorClose)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* Standard set of connector functions to handle a connection
 * (corresponding connectors are also here), part of CONNECTION.
 */
typedef struct {
    FConnectorGetType   get_type;    CONNECTOR c_get_type;
    FConnectorOpen      open;        CONNECTOR c_open;
    FConnectorWait      wait;        CONNECTOR c_wait;
#ifdef IMPLEMENTED__CONN_WaitAsync
    FConnectorWaitAsync wait_async;  CONNECTOR c_wait_async;
#endif
    FConnectorWrite     write;       CONNECTOR c_write;
    FConnectorFlush     flush;       CONNECTOR c_flush;
    FConnectorRead      read;        CONNECTOR c_read;
    FConnectorIOStatus  iostatus;    CONNECTOR c_iostatus;
    FConnectorClose     close;       CONNECTOR c_close;
    CONNECTOR           list;
} SMetaConnector;


#define CONN_TWO2ONE(a, b)   a##b

#define CONN_SET_METHOD(meta, method, function, connector) \
  do { \
    meta->method = function; \
    meta->CONN_TWO2ONE(c_,method) = connector; \
  } while (0);


extern EIO_Status METACONN_Add
(SMetaConnector* meta,
 CONNECTOR       connector
 );


extern EIO_Status METACONN_Remove
(SMetaConnector* meta,
 CONNECTOR       connector
 );


/* Upcall on request to setup virtual function table (e.g. from CONNECTION).
 */
typedef void (*FSetupVTable)
(SMetaConnector* meta,
 CONNECTOR       connector
 );

/* Destroy connector handle (NOT a close request!).
 */
typedef void (*FDestroy)
(CONNECTOR       connector
 );


/* Connector specification.
 */
typedef struct SConnectorTag {
    void*                 handle;    /* handle of the connector      */
    CONNECTOR             next;      /* linked list                  */
    SMetaConnector*       meta;      /* back link to CONNECTION      */
    FSetupVTable          setup;     /* used in CONNECTION init      */
    FDestroy              destroy;   /* destroys handle, can be NULL */
} SConnector;


#ifdef IMPLEMENTED__CONN_WaitAsync
typedef struct {
  CONN              conn;
  EIO_Event         event;
  FConnAsyncHandler handler;
  void*             data;
  FConnAsyncCleanup cleanup;
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

#endif /* NCBI_CONNECTOR__H */
