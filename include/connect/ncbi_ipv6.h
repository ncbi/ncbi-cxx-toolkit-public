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
 * @file ncbi_ipv6.h
 *   IPv6 addressing support
 *
 */

#include <connect/connect_export.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


typedef struct {
    unsigned char octet[16];  /* assume no alignment */
} TNCBI_IPv6Addr;


/** Return non-zero if the address is empty (either as IPv6 or IPv4);  return
 *  zero otherwise.
 * @sa
 *  NcbiIsIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiIsEmptyIPv6(const TNCBI_IPv6Addr* addr);


/** Return non-zero(true) if the address is a true IPv4 address (a mapped IPv4
 *  address);  return zero(false) otherwise.
 * @sa
 *  NcbiIsIPv4Ex, NcbiIPv4ToIPv6, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsIPv4    (const TNCBI_IPv6Addr* addr);


/** Return non-zero(true) if the address is either a mapped IPv4 address or
 * (optionally) an IPv4-compatible IPv6 address;  return zero(false) otherwise.
 * @param compat
 *  non-zero causes IPv4-compatible IPv6 addresses to pass the test
 * @note
 *  NcbiIsIPv4Ex(, 0) is equivalent to NcbiIsIPv4()
 * @sa
 *  NcbiIsIPv4, NcbiIPv4ToIPv6, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsIPv4Ex  (const TNCBI_IPv6Addr* addr, int/*bool*/ compat);


/** Extract and return a network byte order IPv4 embedded address from an IPv6
 *  address, using the specified prefix length (RFC6052).  Return INADDR_NONE
 *  (-1 = 255.255.255.255) when the specified prefix length is not valid.  A
 *  special case (and the most anticipated common use-case) is to use prefix
 *  length 0, which checks that the passed IPv6 address is actually a mapped or
 *  compatible IPv4 address, then extracts it using the prefix length of 96.
 *  Return 0 if the extraction cannot be made (not an IPv4 mapped/compatible
 *  address).
 * @sa
 *  NcbiIsIPv4, NcbiIPv4ToIPv6
 */
extern NCBI_XCONNECT_EXPORT
unsigned int NcbiIPv6ToIPv4(const TNCBI_IPv6Addr* addr, size_t pfxlen);


/** Embed a passed network byte order IPv4 address into an IPv6 address using
 *  the specified prefix length (RFC6052).  Return zero when the specified
 *  prefix length is not valid, non-zero otherwise.  A special case (and the
 *  most anticipated common use-case) is to use prefix length 0, which first
 *  clears the passed IPv6 address, then embeds the IPv4 address as a mapped
 *  address using the prefix length of 96.
 * @sa
 *  NcbiIsIPv4, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv4ToIPv6(TNCBI_IPv6Addr* addr,
                            unsigned int ipv4, size_t pfxlen);


/** Convert into a network byte order IPv4 address, the first "len" (or
 *  "strlen(str)" if "len" is 0) bytes of "str" from a full-quad decimal
 *  notation.  Return a non-zero string pointer to the first non-converted
 *  character (which is neither a digit nor a dot);  return 0 if conversion
 *  failed and no IPv4 address had been found.
 * @sa
 *  NcbiIPToAddr, NcbiIPv4ToIPv6, NcbiStringToAddr,
 *  SOCK_StringToHostPort, SOCK_gethostbyname
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv4(unsigned int* addr,
                              const char* str, size_t len);


/** Convert into an IPv6 address, the first "len" (or "strlen(str)" if "len" is
 *  0) bytes of "str" from a hexadecimal colon-separated notation (including
 *  full-quad trailing IPv4).  Return a non-zero string pointer to the first
 *  non-converted character (which is neither a hex-digit, nor a colon, nor a
 *  dot);  return 0 if conversion failed and no IPv6 address had been found.
 * @sa
 *  NcbiIPToAddr, NcbiStringToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv6(TNCBI_IPv6Addr* addr,
                              const char* str, size_t len);


/** Convert into an IPv6 address, the first "len" (or "strlen(str)" if "len" is
 *  0) bytes of "str" from either a full-quad decimal IPv4 or a hexadecimal
 *  colon-separated IPv6.  Return a non-zero string pointer to the first
 *  non-converted character (which is neither a [hex-]digit, nor a colon, nor a
 *  dot);  return 0 if no conversion can be made.
 * @sa
 *  NcbiStringToIPv4, NcbiStringToIPv6, NcbiStingToAddr, NcbiAddrToString
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiIPToAddr(TNCBI_IPv6Addr* addr,
                          const char* str, size_t len);


/** Convert into an IPv6 address, the first "len" (or "strlen(str)" if "len" is
 *  0) bytes of "str", which can be either an .in-addr.arpa- or an
 *  .in6.arpa-domain names.  Return a non-zero string pointer to the first
 *  non-converted character;  return 0 if no conversion can be made.
 * @sa
 *  NcbiAddrToDNS, NcbiStringToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiDNSIPToAddr(TNCBI_IPv6Addr* addr,
                             const char* str, size_t len);


/** Convert into an IPv6 address, the first "len" (or "strlen(str)" if "len" is
 *  0) bytes of "str", which can be either of a full-quad decimal IPv4, a
 *  hexadecimal colon-separated IPv6, an .in-addr.arpa- or an .in6.arpa-domain
 *  names.  Return a non-zero string pointer to the first non-converted
 *  character (which is neither a [hex-]digit, nor a colon, nor a dot);  return
 *  0 if no conversion can be made.
 * @sa
 *  NcbiAddrToString, NcbiAddrToDNS
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToAddr(TNCBI_IPv6Addr* addr,
                              const char* str, size_t len);


/** Convert a network byte order IPv4 into a full-quad text form and store the
 *  result in the "buf" of size "bufsize".  Return non-zero string address
 *  past the stored result, or 0 when the conversion failed for buffer being
 *  too small.
 *  In either case, if "buf" is non-NULL and "bufsize" is at least one, "buf"
 *  gets '\0'-terminated:  in case of an error (NULL return) this results in
 *  an empty string at "buf", and in case of a non-NULL return value the
 *  returned pointer points to the terminating '\0' byte.
 * @sa
 *  NcbiStringToIPv4, SOCK_ntoa, SOCK_HostPortToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv4ToString(char* buf, size_t bufsize,
                              unsigned int addr);


/** Convert an IPv6 address into a hex colon-separated text form and store the
 *  result in the "buf" of size "bufsize".  Return non-zero string address
 *  past the stored result, or 0 when the conversion failed for buffer being
 *  too small.
 *  In either case, if "buf" is non-NULL and "bufsize" is at least one, "buf"
 *  gets '\0'-terminated:  in case of an error (NULL return) this results in
 *  an empty string at "buf", and in case of a non-NULL return value the
 *  returned pointer points to the terminating '\0' byte.
 * @sa
 *  NcbiStringToIPv6, NcbiStringToAddr, NcbiAddrToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv6ToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr);


/** Convert an IPv6 address into either a full-quad text IPv4 (for IPv4-mapped
 *  IPv6 addresses) or a hex colon-separated text form (for all other), and
 *  store the result in the "buf" of size "bufsize".  Return non-zero string
 *  address past the stored result, or 0 when the conversion failed for buffer
 *  being too small.
 *  In either case, if "buf" is non-NULL and "bufsize" is at least one, "buf"
 *  gets '\0'-terminated:  in case of an error (NULL return) this results in
 *  an empty string at "buf", and in case of a non-NULL return value the
 *  returned pointer points to the terminating '\0' byte.
 * @sa
 *  NcbiStringToAddr, NcbiAddrToDNS, SOCK_ntoa, SOCK_HostPortToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiAddrToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr);


/** Convert an IPv6 address into either .in-addr.arpa domain (for IPv4-mapped
 *  IPv6 addresses) or .ip6.arpa domain (for all other), and store the result
 *  in the "buf" of size "bufsize".  Return non-zero string address past the
 *  stored result, or 0 when the conversion failed for buffer being too small.
 *  In either case, if "buf" is non-NULL and "bufsize" is at least one, "buf"
 *  gets '\0'-terminated:  in case of an error (NULL return) this results in
 *  an empty string at "buf", and in case of a non-NULL return value the
 *  returned pointer points to the terminating '\0' byte.
 * @sa
 *  NcbiAddrToString, NcbiDNSIPToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiAddrToDNS(char* buf, size_t bufsize,
                           const TNCBI_IPv6Addr* addr);


/** Return non-zero(true) if "addr" belongs to the network specified as CIDR
 *  "base/bits";  return a zero(false) otherwise.
 * @note "base" is not checked to contain all zero bits beyond "bits" (as it
 *  should), but if it does not then the return value will always be a
 *  zero(false).
 * @sa
 *  NcbiIPv6Subnet
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsInIPv6Network(const TNCBI_IPv6Addr* base,
                                 unsigned int          bits,
                                 const TNCBI_IPv6Addr* addr);


/** Retain first "bits" in a given "addr", resetting all remaining bits to 0.
 *  Return non-zero(true) if the resultant "addr" is non-empty;  return a
 *  zero(false) otherwise.
 * @note "addr" remains unmodified for "bits" larger than 127, so this call
 *  becomes functionally (but less efficiently) equivalent to negation of
 *  NcbiIsEmptyIPv6().
 * @sa
 *  NcbiIsEmptyIPv6, NcbiIsInIPv6Network, NcbiIPv6Suffix
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv6Subnet(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


/** Retain last "bits" in a given "addr", resetting all remaining bits to 0.
 *  Return non-zero(true) if the resultant "addr" is non-empty;  return a
 *  zero(false) otherwise.
 * @note "addr" remains unmodified for "bits" larger than 127, so this call
 *  becomes functionally (but less efficiently) equivalent to negation of
 *  NcbiIsEmptyIPv6().
 * @sa
 *  NcbiIsEmptyIPv6, NcbiIPv6Subnet
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv6Suffix(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif  /* CONNECT___NCBI_IPV6__H */
