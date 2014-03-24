#ifndef CONNECT___NCBI_SERVER_INFOP__H
#define CONNECT___NCBI_SERVER_INFOP__H

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
 *   NCBI server meta-address info (private part)
 *
 */

#include "ncbi_host_infop.h"
#include <connect/ncbi_server_info.h>


#ifdef __cplusplus
extern "C" {
#endif


int/*bool*/ SERV_SetLocalServerDefault(int/*bool*/ onoff);


/* Constructors for the various types of NCBI server meta-addresses
 */
SSERV_Info* SERV_CreateNcbidInfoEx
(unsigned int      host,        /* network byte order                        */
 unsigned short    port,        /* host byte order                           */
 const char*       args,
 size_t            add
 );

SSERV_Info* SERV_CreateStandaloneInfoEx
(unsigned int      host,        /* network byte order                        */
 unsigned short    port,        /* host byte order                           */
 size_t            add
 );

SSERV_Info* SERV_CreateHttpInfoEx
(ESERV_Type        type,        /* verified, must be one of fSERV_Http*      */
 unsigned int      host,        /* network byte order                        */
 unsigned short    port,        /* host byte order                           */
 const char*       path,
 const char*       args,
 size_t            add
 );

SSERV_Info* SERV_CreateFirewallInfoEx
(unsigned int      host,        /* original server's host in net byte order  */
 unsigned short    port,        /* original server's port in host byte order */
 ESERV_Type        type,        /* type of original server, wrapped into     */
 size_t            add
 );

SSERV_Info* SERV_CreateDnsInfoEx
(unsigned int      host,        /* the only parameter                        */
 size_t            add
 );


SSERV_Info* SERV_ReadInfoEx
(const char*       info_str,
 const char*       name,
 int/*bool*/       lazy
 );


NCBI_XCONNECT_EXPORT
SSERV_Info* SERV_CopyInfoEx
(const SSERV_Info* orig,
 const char*       name
 );


NCBI_XCONNECT_EXPORT
const char* SERV_NameOfInfo
(const SSERV_Info* info
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_SERVER_INFOP__H */
