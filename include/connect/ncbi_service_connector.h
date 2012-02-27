#ifndef CONNECT___NCBI_SERVICE_CONNECTOR__H
#define CONNECT___NCBI_SERVICE_CONNECTOR__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for a named NCBI service
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_service.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif

    
typedef void              (*FSERVICE_ResetData)  (void* data);
typedef void              (*FSERVICE_CleanupData)(void* data);
typedef const SSERV_Info* (*FSERVICE_GetNextInfo)(void* data, SERV_ITER iter);


typedef struct {
    void*                data;          /* User-supplied callback data       */
    FSERVICE_ResetData   reset;         /* Called prior to each iter reset   */
    FSERVICE_CleanupData cleanup;       /* Called prior to connector close   */
    FSERVICE_GetNextInfo get_next_info; /* Called to get conn point          */
    FHTTP_ParseHeader    parse_header;  /* Called if data source is HTTP     */
    THTTP_Flags          flags;         /* fHTTP_Flushable|fHTTP_NoAutoRetry */
} SSERVICE_Extra;


extern NCBI_XCONNECT_EXPORT CONNECTOR SERVICE_CreateConnectorEx
(const char*           service,
 TSERV_Type            types,
 const SConnNetInfo*   net_info,
 const SSERVICE_Extra* params
 );

#define SERVICE_CreateConnector(service) \
    SERVICE_CreateConnectorEx(service, fSERV_Any, 0, 0)


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SERVICE_CONNECTOR__H */
