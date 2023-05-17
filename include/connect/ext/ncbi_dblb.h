#ifndef CONNECT_EXT___NCBI_DBLB__H
#define CONNECT_EXT___NCBI_DBLB__H

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
 *   Get DB service name via LB service name
 *
 *   NOTE:  This is an internal NCBI-only API not for export to third parties.
 *
 *   NOTE:  Non-UNIX platforms may experience some lack of functionality.
 *
 */

#include <connect/ncbi_connutil.h>


#ifdef __cplusplus
extern "C" {
#endif


/* On success, return 'server_name_buf' argument filled with a server name
 * corresponding to given LB name; return NULL on error.
 * 
 * NOTE: servers to skip can be expressed as names of servers alone,
 *       or as host[:port] pairs (with port part being optional).
 */
extern const char* DBLB_GetServerName
(const char* lb_name,                 /* LB name to translate to server name */
 char*       server_name_buf,         /* buffer to store and return result in*/
 size_t      server_name_buflen,      /* buffer size                         */
 const char* const skip_servers[],    /* servers to skip                     */
 char*       errmsg_buf,              /* buffer to store error message in    */
 size_t      errmsg_buflen            /* buffer size                         */
 );

/* Temp to keep backward compatibility */
#define DBLB_GetServerNameEx DBLB_GetServerName


typedef enum {
    eDBLB_Success = 0,                  /* No error                          */
    eDBLB_BadName,                      /* Empty service name not allowed    */
    eDBLB_NotFound,                     /* Service not found, fallback used  */
    eDBLB_NoDNSEntry,                   /* Service found but w/o a DNS entry */
    eDBLB_ServiceDown                   /* Service exists but not operational*/
} EDBLB_Status;


typedef enum {
    fDBLB_None                   = 0,
    fDBLB_AllowFallbackToStandby = 1
} EDBLB_Flags;
typedef unsigned int TDBLB_Flags;       /* Bitwise OR of "EDBLB_Flags"       */


typedef struct {
    unsigned int   host;
    unsigned short port;
    double         pref;                /* [0..100] with 100 causing latch   */
} SDBLB_Preference;


typedef struct {
    unsigned int   host;
    unsigned short port;
    TNCBI_Time     time;                /* this CP expiration time (time_t)  */
} SDBLB_ConnPoint;


/* Returns its server_name_buf argument filled with a server name
 * corresponding to the given LB name.  Both local (LBSM-based) and
 * network (DISPD.CGI-based) service locators (dispatchers) will be
 * consulted unless corresponding mappers are disabled (via registry
 * environment).
 */
extern const char* DBLB_GetServer
(const char*             lb_name,           /* [in]  LB name to look up      */
 TDBLB_Flags             flags,             /* [in]  search flags            */
 const SDBLB_Preference* preference,        /* [in]  NULL if no preference   */
 const char* const       skip_servers[],    /* [in]  servers to exclude      */
 SDBLB_ConnPoint*        conn_point,        /* [out] CP if known (NULL skips)*/
 char*                   server_name_buf,   /* [out] buffer to store result  */
 size_t                  server_name_buflen,/* [in]  buffer size             */
 EDBLB_Status*           result             /* [out] status result code      */
 );

/* Return text representation of a given status; 0 if out of range.
 * eDBLB_Success always maps to the empty string "".
 */
const char* DBLB_StatusStr(EDBLB_Status status);


#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /*CONNECT_EXT___NCBI_DBLB__H*/
