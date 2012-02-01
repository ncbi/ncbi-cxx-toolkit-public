#ifndef CONNECT___NCBI_SOCKET_CONNECTOR__H
#define CONNECT___NCBI_SOCKET_CONNECTOR__H

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
 *   Implement CONNECTOR for a network socket(based on the NCBI "SOCK").
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
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


/* Create new CONNECTOR, which handles connection to a network socket.
 * Make up to "max_try" attempts to connect to "host:port" before giving up.
 * On successful connect, send the first "init_size" bytes from buffer
 * "init_data" (can be NULL -- then send nothing, regardless of "init_size")
 * to the newly opened connection.
 * NOTE:  the connector makes (and then uses) its own copy of the "init_data".
 * Return NULL on error.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR SOCK_CreateConnectorEx
(const char*    host,      /* host to connect to                             */
 unsigned short port,      /* port to connect to                             */
 unsigned short max_try,   /* max.number of attempts to establish connection */
 const void*    init_data, /* data to send to server on connect              */
 size_t         init_size, /* size of the "init_data" buffer                 */
 TSOCK_Flags    flags      /* bitwise OR of additional socket flags          */
 );


/* Equivalent to SOCK_CreateConnectorEx(host, port, max_try, 0, 0, 0)
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR SOCK_CreateConnector
(const char*    host,      /* host to connect to                             */
 unsigned short port,      /* port to connect to                             */
 unsigned short max_try    /* max.number of attempts to establish connection */
 );


/* Create new CONNECTOR structure on top of existing socket handle (SOCK),
 * acquiring the ownership of the socket "sock" if "own_sock" passed non-zero,
 * and overriding all timeouts that might have been set already in it.
 * Timeout values will be taken from connection (CONN), after the connector
 * is used in the CONN_Create() call.
 * Non-owned socket will not be closed when the connection gets closed;
 * and may further be used, as necessary (including closing it explicitly).
 * Note that this call can build a (dysfunctional) connector on top of a NULL
 * sock, and that the delayed connection open will always result in eIO_Closed.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR SOCK_CreateConnectorOnTopEx
(SOCK                   sock,    /* existing socket handle (NULL is allowed) */
 unsigned short/*bool*/ own_sock,/* non-zero if connector is to own "sock"   */
 const char*            hostport /* connection point name taken verbatim     */
 );


/* Equivalent to SOCK_CreateConnectorOnTopEx(sock, own_sock, 0)
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR SOCK_CreateConnectorOnTop
(SOCK                   sock,    /* existing socket handle (NULL is allowed) */
 unsigned short/*bool*/ own_sock /* non-zero if connector is to own "sock"   */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SOCKET_CONNECTOR__H */
