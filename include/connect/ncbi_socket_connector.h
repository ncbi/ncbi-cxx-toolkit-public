#ifndef NCBI_SOCKET_CONNECTOR__H
#define NCBI_SOCKET_CONNECTOR__H

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
 *   Implement CONNECTOR for a network socket(based on the NCBI "SOCK").
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2001/01/23 23:09:18  lavr
 * Flags added to 'Ex' constructor
 *
 * Revision 6.1  2000/04/07 20:05:37  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_connector.h>

#ifdef __cplusplus
extern "C" {
#endif


/* This is equivalent to SOCK_CreateConnectorEx(host, port, conn_try, 0,0).
 */
extern CONNECTOR SOCK_CreateConnector
(const char*    host,     /* server:  host */
 unsigned short port,     /* server:  service port */
 unsigned int   max_try   /* max.number of attempts to establish conn */
 );


typedef enum {
    eSCC_DebugPrintout = 1
} ESCC_Flags;
typedef unsigned int TSCC_Flags;  /* binary OR of "ESCC_Flags */


/* Create new CONNECTOR structure to handle connection to a socket.
 * Make up to "conn_try" attempts to connect to the "host:port" before
 * giving up.
 * On successful connect, send the first "init_size" bytes from buffer
 * "init_data"(can be NULL -- then send nothing) to the opened connection.
 * NOTE:  the connector makes (and then uses) its own copy of the "init_data".
 * Return NULL on error.
 */
extern CONNECTOR SOCK_CreateConnectorEx
(const char*    host,      /* server:  host */
 unsigned short port,      /* server:  service port */
 unsigned int   max_try,   /* max.number of attempts to establish connection */
 const void*    init_data, /* data to send to server on connect */
 size_t         init_size, /* size of the "init_data" buffer */
 TSCC_Flags     flags
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SOCKET_CONNECTOR__H */

