#ifndef CONNECT___NCBI_LOCALIP__H
#define CONNECT___NCBI_LOCALIP__H

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
 *   Determine IP locality (within NCBI) of a given address
 *
 * @warning
 *   Use <connect/ncbi_localip.hpp> in C++ code!
 *
 * @note
 *   This API works correctly only if used within NCBI.
 *
 */

#include <connect/ncbi_ipv6.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Return non-zero (true) if the IP address (in network byte order) provided as
 * an agrument, is a local one (i.e. belongs to NCBI);  return zero (false)
 * otherwise.
 * @warning Loopback addresses are generally considered "local".  If that is a
 *   security issue, analyze the address prior to passing it in to this call.
 * @sa
 *   NcbiIsLocalIPEx, SOCK_IsLoopbackAddress
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiIsLocalIP(unsigned int ip);


typedef struct {
    const char*  sfx;  /* textual domain suffix (may be truncated) */
    unsigned int num;  /* numerical domain sequence number (non-0) */
} SNcbiDomainInfo;


/**
 * Return non-zero (true) if the IP address (in network byte order) provided as
 * an agrument, is a local one (i.e. belongs to NCBI), and update domain info
 * (when passed non-NULL) of the address, if available;  return zero (false)
 * otherwise.
 * @note Domain information remains valid until a call for NcbiInitLocalIP().
 * @warning Loopback addresses are generally considered "local".  If that is a
 *   security issue, analyze the address prior to passing it in to this call.
 * @sa
 *   NcbiIsLocalIP, NcbiInitLocalIP
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiIsLocalIPEx
(const TNCBI_IPv6Addr* addr,   /**< Either true IPv6 or IPv6-mapped IPv4     */
 SNcbiDomainInfo*      info);  /**< Optional, can be NULL for no info to get */


/**
 * Init local IP classification.
 * @note Generally the required initialization is done automagically, so this
 *   call need *not* to be used explicitly.
 * @warning This call invalidates any domain information returned to client
 *   via the NcbiIsLocalIPEx() calls.
 * @sa
 *   NcbiIsLocalIPEx, NcbiIsLocalIP
 */
extern NCBI_XCONNECT_EXPORT
void NcbiInitLocalIP(void);


#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /*CONNECT___NCBI_LOCALIP__H*/
