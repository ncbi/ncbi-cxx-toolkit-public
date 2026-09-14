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
 *   IPv4 and IPv6 address conversion and manipulation support
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


/** Test whether an address is empty, including a NULL pointer, an all-zero IPv6
 *  address, or an all-zero IPv4 address.
 * @return
 *  Non-zero if the address is empty; zero otherwise.
 * @sa
 *  NcbiIsIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/ NcbiIsEmptyIPv6(const TNCBI_IPv6Addr* addr);


/** Test whether the address is an IPv4-mapped IPv6 address.
 *
 *  This is equivalent to calling NcbiIsIPv4Ex(addr, 0).
 * @return
 *  Non-zero (true) if the address is IPv4-mapped; zero (false) otherwise,
 *  including when "addr" is NULL.
 * @sa
 *  NcbiIsIPv4Ex, NcbiIPv4ToIPv6, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsIPv4    (const TNCBI_IPv6Addr* addr);


/** Test whether the address is an IPv4-mapped IPv6 address or, when requested,
 *  an IPv4-compatible IPv6 address.
 * @param compat
 *  If non-zero, accept IPv4-compatible IPv6 addresses as IPv4 addresses.
 * @return
 *  Non-zero (true) if the address is accepted as IPv4; zero (false) otherwise,
 *  including when "addr" is NULL.
 * @note
 *  NcbiIsIPv4Ex(addr, 0) is equivalent to NcbiIsIPv4(addr).
 * @sa
 *  NcbiIsIPv4, NcbiIPv4ToIPv6, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsIPv4Ex  (const TNCBI_IPv6Addr* addr, int/*bool*/ compat);


/** Extract an embedded IPv4 address from an IPv6 address using the specified
 *  prefix length, as defined by RFC 6052, in network byte order.
 *
 *  A prefix length of 0 is a special case intended for the common operation of
 *  extracting an IPv4 address from an IPv4-mapped or IPv4-compatible IPv6
 *  address; in that case, a prefix length of 96 is used.
 * @return
 *  The extracted IPv4 address; INADDR_NONE (-1, or 255.255.255.255) if "addr"
 *  is NULL or "pfxlen" is invalid; or 0 if the address is neither IPv4-mapped
 *  nor IPv4-compatible when "pfxlen" is 0.
 * @sa
 *  NcbiIsIPv4, NcbiIPv4ToIPv6
 */
extern NCBI_XCONNECT_EXPORT
unsigned int NcbiIPv6ToIPv4(const TNCBI_IPv6Addr* addr, size_t pfxlen);


/** Embed the supplied network-byte-order IPv4 address in an IPv6 address using
 *  the specified prefix length, as defined by RFC 6052.
 *
 *  A prefix length of 0 is a special case intended for the common operation of
 *  creating an IPv4-mapped IPv6 address: "addr" is first cleared, and the IPv4
 *  address is then embedded as an IPv4-mapped address using a prefix length of
 *  96.
 * @return
 *  "addr" on success, or NULL if "addr" is NULL or "pfxlen" is invalid.
 * @sa
 *  NcbiIsIPv4, NcbiIPv6ToIPv4
 */
extern NCBI_XCONNECT_EXPORT
TNCBI_IPv6Addr* NcbiIPv4ToIPv6(TNCBI_IPv6Addr* addr,
                               unsigned int ipv4, size_t pfxlen);


/** Convert the first "len" bytes of "str" from dotted-decimal IPv4 notation to
 *  an IPv4 address in network byte order.  If "len" is 0, use strlen(str).
 * @return
 *  On success: A non-NULL pointer to the first character not consumed, either
 *  "str + len" or the first character that is neither a digit nor a dot.
 *  On failure: NULL, including when either "addr" or "str" is NULL.
 * @note
 *  Leading whitespace is skipped before conversion begins.
 * @note
 *  No input beyond the first '\0' is considered.  When "len" is non-zero, no
 *  character beyond the specified range is inspected.  If "str + len" is
 *  returned, all requested characters were accepted; "str[len]" may still be a
 *  digit or dot and, if inspected, that character could otherwise cause the
 *  conversion to fail.
 * @note
 *  Unlike SOCK_gethostbyname[Ex], this function correctly handles "0.0.0.0".
 * @sa
 *  NcbiIPToAddr, NcbiIPv4ToIPv6, NcbiStringToAddr, SOCK_StringToHostPort,
 *  SOCK_gethostbyname[Ex]
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv4(unsigned int* addr,
                              const char* str, size_t len);


/** Convert the first "len" bytes of "str" from colon-separated hexadecimal IPv6
 *  notation to an IPv6 address.  If "len" is 0, use strlen(str).  An IPv6
 *  address may include a trailing dotted-decimal IPv4 address.
 * @return
 *  On success: A non-NULL pointer to the first character not consumed, either
 *  "str + len" or the first character that is neither a hexadecimal digit, a
 *  colon, nor a dot.
 *  On failure: NULL, including when either "addr" or "str" is NULL.
 * @note
 *  Leading whitespace is skipped before conversion begins.
 * @note
 *  No input beyond the first '\0' is considered.  When "len" is non-zero, no
 *  character beyond the specified range is inspected.  If "str + len" is
 *  returned, all requested characters were accepted; "str[len]" may still be a
 *  hexadecimal digit, colon, or dot and, if inspected, that character could
 *  otherwise cause the conversion to fail.
 * @sa
 *  NcbiIPToAddr, NcbiStringToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToIPv6(TNCBI_IPv6Addr* addr,
                              const char* str, size_t len);


/** Convert the first "len" bytes of "str" from either dotted-decimal IPv4 or
 *  colon-separated hexadecimal IPv6 notation to an IPv6 address.  If "len" is
 *  0, use strlen(str).
 * @return
 *  On success: A non-NULL pointer to the first character not consumed, either
 *  "str + len" or the first character that is neither a hexadecimal digit, a
 *  colon, nor a dot.
 *  On failure: NULL, including when either "addr" or "str" is NULL.
 * @note
 *  Leading whitespace is skipped before conversion begins.
 * @note
 *  No input beyond the first '\0' is considered.  When "len" is non-zero, no
 *  character beyond the specified range is inspected.  If "str + len" is
 *  returned, all requested characters were accepted; "str[len]" may still be a
 *  hexadecimal digit, colon, or dot and, if inspected, that character could
 *  otherwise cause the conversion to fail.
 * @note
 *  An IPv4 address is accepted and represented as a valid IPv4-mapped IPv6
 *  address.
 * @sa
 *  NcbiStringToIPv4, NcbiStringToIPv6, NcbiStringToAddr, NcbiAddrToString
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiIPToAddr(TNCBI_IPv6Addr* addr,
                          const char* str, size_t len);


/** Convert the first "len" bytes of "str" from a reverse-DNS IPv4 or IPv6
 *  domain name to an IPv6 address.  If "len" is 0, use strlen(str).
 *
 *  The input may be an in-addr.arpa name for a complete IPv4 address or an
 *  ip6.arpa name for a complete IPv6 address, optionally followed by one dot.
 *  Within the inspected range, an alphanumeric continuation, or a '-' or '.'
 *  followed by an alphanumeric character, makes the name ambiguous and causes
 *  conversion to fail.  Otherwise, parsing stops at the first unconsumed
 *  character, leaving any extra input for the caller; for example, a second
 *  trailing dot will be returned.
 * @return
 *  On success: A non-NULL pointer to the first character not consumed.
 *  On failure: NULL, including when either "addr" or "str" is NULL.
 * @note
 *  Leading whitespace is skipped before conversion begins.
 * @note
 *  No input beyond the first '\0' is considered.  When "len" is non-zero, no
 *  character beyond the specified range is inspected.  If "str + len" is
 *  returned, all requested characters were accepted; "str[len]" may still make
 *  the extended input invalid and, if inspected, that character could otherwise
 *  cause the conversion to fail.
 * @sa
 *  NcbiAddrToDNS, NcbiStringToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiDNSIPToAddr(TNCBI_IPv6Addr* addr,
                             const char* str, size_t len);


/** Convert the first "len" bytes of "str" to an IPv6 address.  If "len" is 0,
 *  use strlen(str).  The input may be dotted-decimal IPv4, colon-separated
 *  hexadecimal IPv6, an in-addr.arpa name for a complete IPv4 address, or an
 *  ip6.arpa name for a complete IPv6 address.
 *
 *  For reverse-DNS input, an alphanumeric continuation, or a '-' or '.'
 *  followed by an alphanumeric character, makes the name ambiguous and causes
 *  conversion to fail; otherwise, parsing may stop after one trailing dot and
 *  leave the next character unconsumed.
 * @return
 *  On success: A non-NULL pointer to the first character not consumed.
 *  On failure: NULL, including when either "addr" or "str" is NULL.
 * @note
 *  Leading whitespace is skipped before conversion begins.
 * @note
 *  No input beyond the first '\0' is considered.  When "len" is non-zero, no
 *  character beyond the specified range is inspected.  If "str + len" is
 *  returned, all requested characters were accepted; "str[len]" may still make
 *  the extended input invalid and, if inspected, that character could otherwise
 *  cause the conversion to fail.
 * @sa
 *  NcbiAddrToString, NcbiAddrToDNS
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiStringToAddr(TNCBI_IPv6Addr* addr,
                              const char* str, size_t len);


/** Convert a network-byte-order IPv4 address to dotted-decimal text and store
 *  the result in "buf", whose size is "bufsize".
 *
 *  If "buf" is non-NULL and "bufsize" is at least 1, it is always \0-terminated
 *  and contains an empty string on failure.
 * @return
 *  A pointer to the terminating '\0' on success, or NULL if "buf" is NULL or
 *  the result does not fit in "buf".
 * @sa
 *  NcbiStringToIPv4, SOCK_ntoa, SOCK_HostPortToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv4ToString(char* buf, size_t bufsize,
                              unsigned int addr);


/** Convert an IPv6 address to colon-separated hexadecimal text and store the
 *  result in "buf", whose size is "bufsize".
 *
 *  If "buf" is non-NULL and "bufsize" is at least 1, it is always \0-terminated
 *  and contains an empty string on failure.
 * @return
 *  A pointer to the terminating '\0' on success, or NULL if either "addr" or
 *  "buf" is NULL, or if the result does not fit in "buf".
 * @sa
 *  NcbiStringToIPv6, NcbiStringToAddr, NcbiAddrToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiIPv6ToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr);


/** Convert an IPv6 address to text and store the result in "buf", whose size is
 *  "bufsize".  IPv4-mapped IPv6 addresses are rendered in dotted-decimal IPv4
 *  notation; all other addresses are rendered in colon-separated hexadecimal
 *  IPv6 notation.
 *
 *  If "buf" is non-NULL and "bufsize" is at least 1, it is always \0-terminated
 *  and contains an empty string on failure.
 * @return
 *  A pointer to the terminating '\0' on success, or NULL if either "addr" or
 *  "buf" is NULL, or if the result does not fit in "buf".
 * @sa
 *  NcbiStringToAddr, NcbiAddrToDNS, SOCK_ntoa, SOCK_HostPortToString
 */
extern NCBI_XCONNECT_EXPORT
char*        NcbiAddrToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr);


/** Convert an IPv6 address to a reverse-DNS domain name and store the result in
 *  "buf", whose size is "bufsize".  IPv4-mapped IPv6 addresses are rendered as
 *  in-addr.arpa names; all other addresses are rendered as ip6.arpa names.
 *
 *  If "buf" is non-NULL and "bufsize" is at least 1, it is always \0-terminated
 *  and contains an empty string on failure.
 * @return
 *  A pointer to the terminating '\0' on success, or NULL if either "addr" or
 *  "buf" is NULL, or if the result does not fit in "buf".
 * @sa
 *  NcbiAddrToString, NcbiDNSIPToAddr
 */
extern NCBI_XCONNECT_EXPORT
const char*  NcbiAddrToDNS(char* buf, size_t bufsize,
                           const TNCBI_IPv6Addr* addr);


/** Test whether "addr" belongs to CIDR network "base/bits".
 * @return
 *  Non-zero (true) if "addr" belongs to the network; zero (false) otherwise,
 *  including when either "base" or "addr" is NULL.
 * @note
 *  "base" is not explicitly checked for zero bits beyond the first "bits", as
 *  required for a canonical network address.  If any of those bits are set, the
 *  result is always false.
 * @sa
 *  NcbiIPv6Subnet
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIsInIPv6Network(const TNCBI_IPv6Addr* base,
                                 unsigned int          bits,
                                 const TNCBI_IPv6Addr* addr);


/** Retain the first "bits" bits of "addr" and clear all remaining bits.
 * @return
 *  Non-zero (true) if the resulting address is non-empty; zero (false)
 *  otherwise, including when "addr" is NULL.
 * @note
 *  "addr" remains unmodified when "bits" is greater than 127.
 * @sa
 *  NcbiIsEmptyIPv6, NcbiIsInIPv6Network, NcbiIPv6Suffix, NcbiIPv4Subnet
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv6Subnet(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


/** Retain the last "bits" bits of "addr" and clear all remaining bits.
 * @return
 *  Non-zero (true) if the resulting address is non-empty; zero (false)
 *  otherwise, including when "addr" is NULL.
 * @note
 *  "addr" remains unmodified when "bits" is greater than 127.
 * @sa
 *  NcbiIsEmptyIPv6, NcbiIPv6Subnet, NcbiIPv4Suffix
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv6Suffix(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


/** Retain the first "bits" bits of an IPv4 "addr" and clear the rest.  A
 *  non-NULL IPv4 input is stored as IPv4-mapped; a non-IPv4 input is cleared to
 *  IPv4-mapped 0.0.0.0.
 * @return
 *  Non-zero (true) if the resulting IPv4-mapped address is non-empty; zero
 *  (false) otherwise, including when "addr" is NULL.
 * @note
 *  When "bits" is greater than 31, the IPv4 address bits of a valid IPv4 input
 *  remain unchanged; an IPv4-compatible input is still converted to IPv4-mapped
 *  form.
 * @sa
 *  NcbiIsIPv4Ex, NcbiIPv6Subnet
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv4Subnet(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


/** Retain the last "bits" bits of an IPv4 "addr" and clear the rest.
 *  A non-NULL IPv4 input is stored as IPv4-mapped; a non-IPv4 input is
 *  cleared to IPv4-mapped 0.0.0.0.
 * @return
 *  Non-zero (true) if the resulting IPv4-mapped address is non-empty; zero
 *  (false) otherwise, including when "addr" is NULL.
 * @note
 *  When "bits" is greater than 31, the IPv4 address bits of a valid IPv4 input
 *  remain unchanged; an IPv4-compatible input is still converted to IPv4-mapped
 *  form.
 * @sa
 *  NcbiIsIPv4Ex, NcbiIPv6Suffix
 */
extern NCBI_XCONNECT_EXPORT
int/*bool*/  NcbiIPv4Suffix(TNCBI_IPv6Addr* addr,
                            unsigned int    bits);


#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif  /* CONNECT___NCBI_IPV6__H */
