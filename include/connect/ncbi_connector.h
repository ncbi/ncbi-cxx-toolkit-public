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
 * Revision 6.1  2000/03/24 22:52:48  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Function type definitions for the connector method table.
 * The arguments & the behaviour of "FConnector***" functions are mostly just
 * the same as those for their counterparts "CONN_***"
 * (see in "ncbi_connection.h").
 * First argument of these functions accepts a real connector handle
 * rather than a connection handle("CONN").
 */


/* Get name of the connector(must not be NULL!)
 */
typedef const char* (*FConnectorGetType)
(void* connector
 );


/* Connect if not connected yet.
 * Reconnect with the same parameters as original connect if already connected.
 */
typedef EIO_Status (*FConnectorConnect)
(void*           connector,
 const STimeout* timeout
 );


/* Wait until either read or write(dep. on the "direction" value) becomes
 * available, or until "timeout" is expired, or until error occures.
 * NOTE 1: FConnectorWait is guaranteed to be called after FConnectorConnect,
 *         and only if FConnectorConnect returned "eCONN_Success".
 * NOTE 2: "direction" is guaranteed to be either "eCONN_Read" or "eCONN_Write"
 */
typedef EIO_Status (*FConnectorWait)
(void*           connector,
 EIO_Event       direction,
 const STimeout* timeout
 );

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
(void*                   connector,
 FConnectorAsyncHandler  func,
 SConnectorAsyncHandler* data
 );
#endif /* IMPLEMENTED__CONN_WaitAsync */


/* The passed "n_written" always non-NULL, and "*n_written" always zero.
 * It must return Success status when(and only when) all requested data
 * have been succefully written.
 * It returns the # of succesfully written data(in bytes) in "*n_written".
 * NOTE: FConnectorWrite is guaranteed to be called after FConnectorConnect,
 *       and only if FConnectorConnect returned "eCONN_Success".
 */
typedef EIO_Status (*FConnectorWrite)
(void*           connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout
 );


/* Flush yet unwritten output data, if any.
 * NOTE: FConnectorFlush is guaranteed to be called after FConnectorConnect,
 *       and only if FConnectorConnect returned "eCONN_Success".
 */
typedef EIO_Status (*FConnectorFlush)
(void*           connector,
 const STimeout* timeout
 );


/* The passed "n_read" always non-NULL, and "*n_read" always zero.
 * NOTE: FConnectorRead is guaranteed to be called after FConnectorConnect,
 *       and only if FConnectorConnect returned "eCONN_Success".
 */
typedef EIO_Status (*FConnectorRead)
(void*           connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout
 );


/* "FLUSH" method gets called before "CLOSE" automatically.
 * It must cleanup all internal structures and the connetctor handle itself
 * (that's why it accepts "CONNECTOR" rather than just "CONNECTOR"'s handle).
 */
typedef EIO_Status (*FConnectorClose)
(CONNECTOR       connector,
 const STimeout* timeout
 );


/* Standard set of connector functions to handle a connection
 */
typedef struct {
  FConnectorGetType    get_type;
  FConnectorConnect    connect;
  FConnectorWait       wait;
#ifdef IMPLEMENTED__CONN_WaitAsync
  FConnectorWaitAsync  wait_async;
#endif
  FConnectorWrite      write;
  FConnectorFlush      flush;
  FConnectorRead       read;
  FConnectorClose      close;
} SConnectorVTable;


/* Connector specification
 */
typedef struct SConnectorTag {
  void*            handle;  /* handle of the connector    */
  SConnectorVTable vtable;  /* operations on the "handle" */
} SConnector;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CONNECTOR__H */
