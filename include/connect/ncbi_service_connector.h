#ifndef NCBI_SERVICE_CONNECTOR__H
#define NCBI_SERVICE_CONNECTOR__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for a named NCBI service
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2001/06/04 17:00:18  lavr
 * Include files adjusted
 *
 * Revision 6.2  2000/12/29 17:48:34  lavr
 * Accepted server types included in SERVICE_CreateConnectorEx arguments.
 *
 * Revision 6.1  2000/10/07 22:15:12  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_connector.h>
#include <connect/ncbi_server_info.h>

#ifdef __cplusplus
extern "C" {
#endif


extern CONNECTOR SERVICE_CreateConnector(const char *service);

extern CONNECTOR SERVICE_CreateConnectorEx(const char *service,
                                           TSERV_Type types,
                                           const SConnNetInfo *info);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICE_CONNECTOR__H */
