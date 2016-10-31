#ifndef CONNECT___NCBI_IPV6__H
#define CONNECT___NCBI_IPV6__H

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
 *   IPv6 addressing support
 *
 */

#include <connect/connect_export.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


typedef struct {
    unsigned char octet[16];  /* no alignment assumed */
} TNCBI_IPv6Addr;


extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsIPv4    (const TNCBI_IPv6Addr* addr);


extern NCBI_XCONNECT_EXPORT
unsigned int NcbiIPv6ToIPv4(const TNCBI_IPv6Addr* addr, size_t pfxlen);


extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv4ToIPv6(TNCBI_IPv6Addr* addr,
                            unsigned int ipv4, size_t pfxlen);


extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv4(unsigned int* addr,
                              const char* src, size_t len);


extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv6(TNCBI_IPv6Addr* addr,
                             const char* buf, size_t len);


extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToAddr(TNCBI_IPv6Addr* addr,
                              const char* src, size_t len);


extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv4ToString(char* buf, size_t bufsize,
                              unsigned int addr);


extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv6ToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr);


extern NCBI_XCONNECT_EXPORT
char*        NcbiAddrToString(char* buf, size_t bufsize,
                              TNCBI_IPv6Addr* addr);


extern NCBI_XCONNECT_EXPORT
const char*  NcbiAddrToDNS(char* buf, size_t bufsize,
                           TNCBI_IPv6Addr* addr);


extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsInIPv6Network(const TNCBI_IPv6Addr* base,
                                 unsigned int          bits,
                                 const TNCBI_IPv6Addr* addr);


#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif  /* CONNECT___NCBI_IPV6__H */
