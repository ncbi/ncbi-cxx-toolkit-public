#ifndef CONNECT___NCBI_IPRANGE__H
#define CONNECT___NCBI_IPRANGE__H

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
 *   IP range manipulating API
 *
 */

#include <connect/ncbi_ipv6.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    eIPRange_None = 0,    /* invalid entry                               */
    eIPRange_Host,        /* a is set, b is 0                            */
    eIPRange_Range,       /* a is set, b is set, IPv4 only               */
    eIPRange_Network,     /* a is set, b is mask for IPv4, bits for IPv6 */
    eIPRange_Application  /* a and b application-specific                */
} EIPRangeType;


typedef struct {
    EIPRangeType   type;
    TNCBI_IPv6Addr a;     /* IPv4 | IPv6 */
    unsigned int   b;     /* IPv4 | bits */
} SIPRange;


extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiIsInIPRange(const SIPRange*       range,
                            const TNCBI_IPv6Addr* addr);


/* NB: NOP for IPv6 (which can be either "Host" or "Network") */
extern NCBI_XCONNECT_EXPORT
SIPRange    NcbiTrueIPRange(const SIPRange* range);


extern NCBI_XCONNECT_EXPORT
const char* NcbiDumpIPRange(const SIPRange* range, char* buf, size_t bufsize);


/** Acceptable forms (netmasked IPv4s may omit trailing .0s before '/'):
 * 4 quad full IP:  123.123.123.123
 * IPv4 range:      123.123.1-123   (meaning 123.123.1.0 thru 123.123.123.255)
 * IPv4 w/netmask:  123.123/255.255.240.0  (equivalent to the following line)
 * IPv4 CIDR:       123.123/20      (meaning 123.123.0.0 thru 123.123.15.255)
 * IPv4 wildcard:   123.123.*       (meaning 123.123.0.0 thru 123.123.255.255)
 * IPv6[/CIDR]:     aa:bb::cc/64    (IPv6 addr must be complete,/CIDR optional)
 * hostname:        DNS format
 * @note
 *   IP subnets (both IPv4 and IPv6) require that the unmasked "host" portion
 *   of the address was 0.
 * @return
 *   0 (false) if failed;  non-zero (true) if successful and the parsed range
 *   stored at the location pointed to by "range".
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiParseIPRange(SIPRange* range, const char* s);


#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /*CONNECT___NCBI_IPRANGE__H*/
