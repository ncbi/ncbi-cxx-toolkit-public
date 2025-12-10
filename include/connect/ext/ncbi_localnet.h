#ifndef CONNECT_EXT___NCBI_LOCALNET__H
#define CONNECT_EXT___NCBI_LOCALNET__H

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
 *   Get IP address of a CGI client and determine the IP locality
 *
 *   NOTE:  This is an internal NCBI-only API not for export to third parties.
 *
 */

#include <connect/ncbi_ipv6.h>
#include <connect/ncbi_localip.h>  /* for backward-compatibility */


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Return non-zero (true) if called from within a CGI that was invoked by an
 * NCBI local client;  zero otherwise.
 * 
 * @param tracking_env
 *   string array with the tracking environment, or if NULL then the process
 *   environment (as provided by the run-time library) is used
 */
extern int/*bool*/ NcbiIsLocalCgiClient
(const char* const* tracking_env);


/**
 * Return non-zero (true) if the IP address (in network byte order) provided as
 * an agrument, is a nonroutable private IP address (including local, loopback,
 * multicast, and RFC1918);  return zero (false) otherwise.
 */
extern int/*bool*/ NcbiIsPrivateIP
(unsigned int ip);


/**
 * Return non-zero (true) if the IP address (in network byte order) provided as
 * an agrument, is an Automatic Private IP Address (APIPA), RFC3927; return zero
 * (false) otherwise.
 */
extern int/*bool*/ NcbiIsAPIPA
(unsigned int ip);


/**
 * Return IP address (in network byte order) of the CGI client, and optionally
 * store the client hostname in a user-supplied buffer (if the size is not
 * adequate to accomodate the result, then it is not stored).
 * Return 0 if the IP address cannot be obtained;  however, the text form of
 * the host can still be stored in the provided buffer (check for '\0' at [0]).
 * @warning
 *   Result is generally undefined if called not from within a CGI executable.
 *
 * @param flag
 *   modifies the search algorithm:  most users would want to specify
 *   eCgiClientIP_TryAll (and those who are absolutely sure that the requests
 *   cannot be coming from the older NI facility of the C Toolkit, would want
 *   to use eCgiClientIP_TryMost);  eCgiClientIP_TryLeast causes the search
 *   to ignore the special "Client-Host:" tag that can indicate the real origin
 *   of the request (when this request is made on behalf of the indicated IP)
 * @param buf
 *   buffer where the client host (a hostname or textual representation of the
 *   network address as provided by the web-server) will be saved (may be NULL
 *   not to save)
 * @param buf_size
 *   the size of the buffer (must be large enough, truncation not allowed)
 * @param tracking_env
 *   string array with the tracking environment, or if NULL then the process
 *   environment (as provided by the run-time library) is used
 * @warning
 *   FCGI applications *must* use CCgiRequest::GetClientTrackingEnv() to obtain
 *   proper tracking environment per each request
 * @sa
 *   CCgiRequest::GetClientTrackingEnv(), NcbiGetCgiClientIPv6Ex
 */

typedef enum {
    eCgiClientIP_TryAll   = 0, /**< Try all env.vars (NI_CLIENT_IPADDR incl.)*/
    eCgiClientIP_TryMost  = 1, /**< Try most of known environment variables  */
    eCgiClientIP_TryLeast = 2  /**< Try to detect caller's IP only,not origin*/
} ECgiClientIP;

extern unsigned int NcbiGetCgiClientIPEx
(ECgiClientIP       flag,
 char*              buf,
 size_t             buf_size,
 const char* const* tracking_env
 );

/** Same as NcbiGetCgiClientIPEx(., NULL, 0, .)
 * @sa
 *   NcbiGetCgiClientIPEx
 */
extern unsigned int NcbiGetCgiClientIP
(ECgiClientIP       flag,
 const char* const* tracking_env
 );


/**
 * Same as NcbiGetCgiClientIPEx but IPv6-aware.  IPv4 addresses packed as IPv6.
 * Returns an empty IPv6 address when failed to figure out the address.
 * @sa
 *   NcbiGetCgiClientIPEx
 */
extern TNCBI_IPv6Addr NcbiGetCgiClientIPv6Ex
(ECgiClientIP       flag,
 char*              buf,
 size_t             buf_size,
 const char* const* tracking_env
 );

/** Same as NcbiGetCgiClientIPv6Ex(., NULL, 0, .)
 * @sa
 *   NcbiGetCgiClientIPv6Ex
 */
extern TNCBI_IPv6Addr NcbiGetCgiClientIPv6
(ECgiClientIP       flag,
 const char* const* tracking_env
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /*CONNECT_EXT___NCBI_LOCALNET__H*/
