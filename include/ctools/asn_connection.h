#ifndef CTOOLS___ASN_CONNECTION__H
#define CTOOLS___ASN_CONNECTION__H

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *    Build C Toolkit ASN.1 streams on top of CONN (connection).
 *
 */

#include <connect/ncbi_connection.h>
#include <connect/ncbi_service_connector.h>
#include <asn.h>


/** @addtogroup CToolsASNConn
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    eAsnConn_Input,
    eAsnConn_Output
} EAsnConn_Direction;


typedef enum {
    eAsnConn_Binary,
    eAsnConn_Text
} EAsnConn_Format;


/* Build a ASN.1 stream on top of a CONN (connection) handle.
 * According to arguments, the stream is created for either reading or writing,
 * and is capable to handle either binary or text ASN.1.
 * Return ASN.1 stream pointer on success, or 0 on error.
 * NOTE: Returned stream is valid while the underlying conn exists.  After call
 *       to CONN_Close() the stream becomes invalid, and should not be used.
 *       Don't destroy the ASN.1 stream explicitly using AsnIoFree()!
 */
AsnIoPtr CreateAsnConn
(CONN               conn,
 EAsnConn_Direction direction,
 EAsnConn_Format    fmt
 );


/* Create a service connection using the service name, type, and connection
 * parameters (use default connection parameters if 'net_info' is passed NULL).
 * Create two ASN.1 streams based on the connection:  one stream for input, and
 * another one for output.  Return pointers to the streams via the 'input' and
 * 'output' arguments.  No corresponding stream is created if either pointer is
 * passed NULL.
 * On success, return the created CONN handle; otherwise, return 0.
 * NOTE: Both returned ASN.1 streams are valid as long as the connection handle
 *       exists; that is, after the connection handle is passed to CONN_Close()
 *       either pointer becomes invalid, and should not be used.
 *       Don't destroy the ASN.1 streams explicitly using AsnIoFree()!
 */
CONN CreateAsnConn_ServiceEx
(const char*           service,
 EAsnConn_Format       input_fmt,
 AsnIoPtr*             input,
 EAsnConn_Format       output_fmt,
 AsnIoPtr*             output,
 TSERV_Type            type,
 const SConnNetInfo*   net_info,
 const SSERVICE_Extra* params
 );


/* Equivalent of CreateAsnConn_ServiceEx with zeroes in last three arguments.
 */
CONN CreateAsnConn_Service
(const char*     service,
 EAsnConn_Format input_fmt,
 AsnIoPtr*       input,
 EAsnConn_Format output_fmt,
 AsnIoPtr*       output
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


#endif /* CTOOLS___ASN_CONNECTION__H */
