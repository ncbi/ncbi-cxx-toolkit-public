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

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_socket.h>
#include <connect/ncbi_ipv6.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct SIPDNSsfx {
    const char*  sfx;  /* special domain suffix */
    const size_t ssz;  /* sizeof(sfx)           */
};
#define NCBI_IPV6_DNS_SIZE  9
static const struct SIPDNSsfx kIPv6DNS = { "ip6.arpa",     NCBI_IPV6_DNS_SIZE };
#define NCBI_IPV4_DNS_SIZE  13
static const struct SIPDNSsfx kIPv4DNS = { "in-addr.arpa", NCBI_IPV4_DNS_SIZE };


static int/*bool*/ x_NcbiIsIPv4(const TNCBI_IPv6Addr* addr, int/*bool*/ compat)
{
    /* RFC 4291 2.1, 3
       NB: 2.5.5.1 and 2.5.5.2 - both obsoleted by RFC 6052 2.1 */
    unsigned short word;
    unsigned int   temp;
    if (memcchr(addr->octet, 0, 10 * sizeof(addr->octet[0])))
        return 0/*false*/;
    memcpy(&word, &addr->octet[10], sizeof(word));
    if (word == 0xFFFF)
        return 1/*true: mapped IPv4 */;
    if (word != 0x0000  ||  !compat)
        return 0/*false*/;
    /* IPv4-compatible IPv6 */
    memcpy(&temp, &addr->octet[12], sizeof(temp));
    return SOCK_NetToHostLong(temp) & 0xFF000000 ? 1/*true*/ : 0/*false*/;
}


extern int/*bool*/ NcbiIsEmptyIPv6(const TNCBI_IPv6Addr* addr)
{
    unsigned short word;
    unsigned int   temp;
    if (!addr)
        return 1/*true*/;
    if (memcchr(addr->octet, 0, 10 * sizeof(addr->octet[0])))
        return 0/*false*/;
    memcpy(&word, &addr->octet[10], sizeof(word));
    if (word != 0x0000  &&  word != 0xFFFF)
        return 0/*false*/;
    memcpy(&temp, &addr->octet[12], sizeof(temp));
    return !temp;
}


extern int/*bool*/ NcbiIsIPv4(const TNCBI_IPv6Addr* addr)
{
    return addr  &&  x_NcbiIsIPv4(addr, 0/*mapped*/) ? 1/*true*/ : 0/*false*/;
}


extern int/*bool*/ NcbiIsIPv4Ex(const TNCBI_IPv6Addr* addr, int/*bool*/ compat)
{
    return addr  &&  x_NcbiIsIPv4(addr, compat) ? 1/*true*/ : 0/*false*/;
}


extern unsigned int NcbiIPv6ToIPv4(const TNCBI_IPv6Addr* addr, size_t pfxlen)
{
    unsigned int ipv4;
    static const size_t size = sizeof(ipv4);
    if (!addr)
        return (unsigned int)(-1L)/*bad*/;
    if (pfxlen == 0) {
        if (!x_NcbiIsIPv4(addr, 1/*compat*/))
            return 0;
        pfxlen  = 96;
    }
    switch (pfxlen) {  /*RFC 6052 2.2*/
    case 32:
        memcpy(        &ipv4,            &addr->octet[4],  size);
        break;
    case 40:
        memcpy(        &ipv4,            &addr->octet[5],  size - 1);
        memcpy((char*) &ipv4 + size - 1, &addr->octet[9],         1);
        break;
    case 48:
        memcpy(        &ipv4,            &addr->octet[6],  size - 2);
        memcpy((char*) &ipv4 + size - 2, &addr->octet[9],         2);
        break;
    case 56:
        memcpy(        &ipv4,            &addr->octet[7],  size - 3);
        memcpy((char*) &ipv4 + size - 3, &addr->octet[9],         3);
        break;
    case 64:
        memcpy(        &ipv4,            &addr->octet[9],  size);
        break;
    case 96:
        memcpy(        &ipv4,            &addr->octet[12], size);
        break;
    default:
        ipv4 = (unsigned int)(-1L)/*failure*/;
        assert(0);
        break;
    }
    return ipv4;
}


extern TNCBI_IPv6Addr* NcbiIPv4ToIPv6(TNCBI_IPv6Addr* addr,
                                      unsigned int ipv4, size_t pfxlen)
{
    static const size_t size = sizeof(ipv4);
    if (!addr)
        return 0/*failure*/;
    if (pfxlen == 0) {
        static const size_t word = sizeof(unsigned short);
        /* creates IPv6 mapped */
        memset(addr, 0, sizeof(*addr) - (word + size));
        memset(addr->octet + (5 << 1), '\xFF', word);
        pfxlen  = 96;
    }
    switch (pfxlen) {  /*RFC 6052 2.2*/
    case 32:
        memcpy(&addr->octet[4],         &ipv4,            size);
        break;
    case 40:
        memcpy(&addr->octet[5],         &ipv4,            size - 1);
        memcpy(&addr->octet[9], (char*) &ipv4 + size - 1,        1);
        break;
    case 48:
        memcpy(&addr->octet[6],         &ipv4,            size - 2);
        memcpy(&addr->octet[9], (char*) &ipv4 + size - 2,        2);
        break;
    case 56:
        memcpy(&addr->octet[7],         &ipv4,            size - 3);
        memcpy(&addr->octet[9], (char*) &ipv4 + size - 3,        3);
        break;
    case 64:
        memcpy(&addr->octet[9],         &ipv4,            size);
        break;
    case 96:
        memcpy(&addr->octet[12],        &ipv4,            size);
        break;
    default:
        assert(0);
        return 0/*failure*/;
    }
    return addr;
}


/* Parse "str" as an IPv4 address, and return 0 if failed, otherwise a pointer
 * to the first non-parsed char (which is neither a digit nor a dot) and "dst"
 * updated with the just read IPv4 address in network byte order.
 */
static const char* x_StringToIPv4(unsigned int* dst,
                                  const char* str, size_t len)
{
    size_t n;
    int octets = 0;
    unsigned int tmp;
    int/*bool*/ was_digit = 0/*false*/;
    unsigned char *ptr = (unsigned char*) &tmp;

    *ptr = 0;
    for (n = 0;  n < len;  ++n) {
        char c = str[n];
        if ('0' <= c  &&  c <= '9') {
            unsigned int val;
            if (was_digit  &&  !*ptr)
                return 0/*leading "0" in octet*/;
            val = (unsigned int)(*ptr * 10 + (c - '0'));
            if (val > 255)
                return 0;
            *ptr = (unsigned char) val;
            if (!was_digit) {
                ++octets;
                assert(octets <= 4);
                was_digit = 1/*true*/;
            }
        } else if (c == '.') {
            if (!was_digit  ||  octets >= 4)
                return 0;
            was_digit = 0/*false*/;
            *++ptr = 0;
        } else
            break;
    }
    if (octets != 4)
        return 0/*failure*/;

    *dst = tmp;
    return str + n;
}


static char* x_IPv4ToString(char* buf, size_t bufsize, const void* src)
{
    char tmp[sizeof("255.255.255.255")];
    unsigned char* ptr = (unsigned char*) src;
    size_t len
        = (size_t) sprintf(tmp, "%u.%u.%u.%u", ptr[0], ptr[1], ptr[2], ptr[3]);
    return len < bufsize ? (char*) memcpy(buf, tmp, len + 1/*EOS*/) + len : 0;
}


/* Returns ptr past read (0 on error) */
static const char* x_StringToIPv6(TNCBI_IPv6Addr* addr,
                                  const char* str, size_t len)
{
    unsigned short word;
    struct {
        const char* ptr;
        size_t      len;
    } token[sizeof(addr->octet) / sizeof(word) + 1];
    size_t maxt = sizeof(token) / sizeof(token[0]) - 1, t, n;
    TNCBI_IPv6Addr temp;
    unsigned char* dst;
    int/*bool*/ ipv4;
    unsigned int ip;
    size_t gap;

    if (len < 2  ||  (str[n = 0] == ':'  &&  str[++n] != ':'))
        return 0/*failure*/;
    gap = 0;
    ipv4 = 0/*false*/;
    token[t = 0].ptr = str + n;
    do {
        assert(t <= maxt);
        if (n == len  ||  str[n] == ':') {
            token[t].len = (size_t)(&str[n] - token[t].ptr);
            if (!token[t].len) {
                if (n == len)
                    break;
                if (gap++)  /*RFC 4291 2.2, 2*/
                    return 0/*more than one contraction*/;
            } else if (token[t].len <= 4) {
                if (n == len) {
                    if (++t > maxt)
                        return 0/*too many groups*/;
                    break;
                }
            } else
                return 0/*too many digits*/;
            assert(str[n] == ':');
            token[t].len++;
            if (++t > maxt)
                return 0/*too many groups*/;
            token[t].ptr = &str[n + 1];
            continue;
        }
        if (!isxdigit((unsigned char) str[n])) {
            token[t].len = (size_t)(&str[n] - token[t].ptr);
            if (token[t].len) {
                if (str[n] == '.') {
                    if (t <= maxt - sizeof(ip) / sizeof(word)) {
                        const char* end
                            = x_StringToIPv4(&ip,
                                             token[t].ptr,
                                             token[t].len + (len - n));
                        if (end  &&  *end != ':'
                            &&  t <= (maxt -= sizeof(ip) / sizeof(word))) {
                            token[t].len = (size_t)(end - token[t].ptr);
                            ipv4 = 1/*true*/;
                            break;
                        }
                    }
                    return 0/*failure*/;
                }
                if (++t > maxt)
                    return 0/*failure*/;
            }
            break;
        }
    } while (++n <= len);

    assert(t <= maxt);
    if (t < maxt  &&  !gap)
        return 0/*failure*/;

    dst = temp.octet;
    for (n = 0;  n < t;  ++n) {
        assert(token[n].len);
        if (*token[n].ptr != ':') {
            char* end;
            long  val;
            assert(isxdigit((unsigned char) token[n].ptr[0]));
            errno = 0;
            val = strtol(token[n].ptr, &end, 16);
            if (errno  ||  (val ^ (val & 0xFFFF)))
                return 0/*failure*/;
            assert(end == token[n].ptr + token[n].len - (*end == ':'));
            if (*end == ':'  &&  n == t - !ipv4)
                return 0/*failure*/;
            word = SOCK_HostToNetShort((unsigned short) val);
            memcpy(dst, &word, sizeof(word));
            dst += sizeof(word);
        } else {
            gap = (maxt - t) * sizeof(word) + sizeof(word);
            memset(dst, 0, gap);
            dst += gap;
        }
    }
    if (ipv4) {
        memcpy(dst, &ip, sizeof(ip));
        ++t;
    }

    *addr = temp;
    return token[t - 1].ptr + token[t - 1].len;
}


extern const char* NcbiStringToIPv4(unsigned int* addr,
                                    const char* str, size_t len)
{
    size_t n;
    if (!addr)
        return 0/*failure*/;
    *addr = 0;
    if (!str)
        return 0/*failure*/;
    if (!len)
        len = strlen(str);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) str[n]))
            break;
    }
    return x_StringToIPv4(addr, str + n, len - n);
}


extern const char* NcbiStringToIPv6(TNCBI_IPv6Addr* addr,
                                    const char* str, size_t len)
{
    size_t n;
    if (!addr)
        return 0/*failure*/;
    memset(addr, 0, sizeof(*addr));
    if (!str  ||  !*str)
        return 0/*failure*/;
    if (!len)
        len = strlen(str);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) str[n]))
            break;
    }
    return x_StringToIPv6(addr, str + n, len - n);
}


static char* x_IPv6ToString(char* buf, size_t bufsize,
                            const TNCBI_IPv6Addr* addr)
{
    char ipv6[64/*enough for sizeof(8 * "xxxx:")*/];
    char ipv4[sizeof("255.255.255.255")];
    size_t i, n, pos, len, zpos, zlen;
    unsigned short word;
    char* ptr = ipv6;

    if (x_NcbiIsIPv4(addr, 1/*compat*/)) {
        unsigned int ip;
        n = sizeof(addr->octet) - sizeof(ip);
        memcpy(&ip, addr->octet + n, sizeof(ip));
        SOCK_ntoa(ip, ipv4, sizeof(ipv4));
        n /= sizeof(word);
    } else {
        n = sizeof(addr->octet) / sizeof(word);
        *ipv4 = '\0';
    }

    pos = i = zpos = zlen = 0;
    for (;;) {
        if (i < n) {
            memcpy(&word, &addr->octet[i * sizeof(word)], sizeof(word));
            if (!word) {
                ++i;
                continue;
            }
        }
        len = i - pos;
        if (len > 1) {  /*RFC 5952 4.2.2*/
            if (zlen < len) {
                zlen = len;  /*RFC 5952 4.2.1*/
                zpos = pos;
            }
        }
        if (i == n)
            break;
        pos = ++i;
    }

    i = 0;
    while (i < n) {
        if (zlen  &&  zpos == i) {
            assert(zlen > 1);
            *ptr++ = ':';
            if (zlen == n - i)
                *ptr++ = ':';
            i += zlen;
            zlen = 0;  /*RFC 5952 4.2.3*/
            continue;
        }
        memcpy(&word, &addr->octet[i * sizeof(word)], sizeof(word));
        ptr += sprintf(ptr, &":%x"[!i],  /*RFC 5952 4.1, 4.3*/
                       SOCK_NetToHostShort(word));
        ++i;
    }
    assert(ptr > ipv6);

    i = strlen(ipv4);
    if (i) {
        if (ptr[-1] != ':')
            *ptr++ = ':';
    }
    n = (size_t)(ptr - ipv6);
    len = n + i;
    if (len < bufsize) {
        memcpy(buf, ipv6, n);
        buf += n;
        memcpy(buf, ipv4, i);
        buf += i;
        *buf = '\0';
    } else
        buf = 0;
    return buf;
}


/* Returns ptr past written (points to '\0'); 0 on error */
extern char* NcbiIPv4ToString(char* buf, size_t bufsize,
                              unsigned int addr)
{
    /* sanity */
    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';

    return x_IPv4ToString(buf, bufsize, &addr);
}


/* Returns ptr past written (points to '\0'); 0 on error */
extern char* NcbiIPv6ToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr)
{
    /* sanity */
    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';
    if (!addr)
        return 0;

    return x_IPv6ToString(buf, bufsize, addr);
}


extern char* NcbiAddrToString(char* buf, size_t bufsize,
                              const TNCBI_IPv6Addr* addr)
{
    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';
    if (!addr)
        return 0;

    if (x_NcbiIsIPv4(addr, 0/*mapped*/)) {
        unsigned int ipv4 = NcbiIPv6ToIPv4(addr, 0);
        return x_IPv4ToString(buf, bufsize, &ipv4);
    }
    return x_IPv6ToString(buf, bufsize, addr);
}


extern const char* NcbiAddrToDNS(char* buf, size_t bufsize,
                                 const TNCBI_IPv6Addr* addr)
{
    char tmp[sizeof(addr->octet)*4 + 16/*slack*/], *dst = tmp;
    const struct SIPDNSsfx* sfx;
    const unsigned char* src;
    size_t n, len;

    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';
    if (!addr)
        return 0;

    len = 0;
    src = addr->octet + sizeof(addr->octet) - 1;
    if (x_NcbiIsIPv4(addr, 0/*mapped*/)) {
        sfx = &kIPv4DNS;
        for (n = 0;  n < sizeof(unsigned int);  ++n) {
            size_t off = (size_t)sprintf(dst, "%d.",    *src--);
            dst += off;
            len += off;
        }
    } else {
        sfx = &kIPv6DNS;
        for (n = 0;  n < sizeof(addr->octet);  ++n) {
            size_t off = (size_t)sprintf(dst, "%x.%x.", *src & 0xF, *src >> 4);
            dst += off;
            len += off;
            --src;
        }
    }
    if (len + sfx->ssz <= bufsize) {
        memcpy(buf, tmp,      len);
        buf += len;
        memcpy(buf, sfx->sfx, sfx->ssz);
        buf += sfx->ssz;
    } else
        buf = 0;
    return buf;
}


/* NB: "str" is actually bounded by the ".in-addr.apra" suffix */
static const char* x_DNSToIPv4(unsigned int* addr,
                               const char* str, size_t len)
{
    unsigned char* dst = (unsigned char*) addr + sizeof(*addr);
    CORE_DEBUG_ARG(const char* end = str + len;)
    size_t n;

    assert(*end == '.'
           &&  7/*"x.x.x.x"*/ <= len  &&  len <= 15/*xxx.xxx.xxx.xxx*/);

    for (n = 0;  n < sizeof(*addr);  ++n) {
        char  s[4];
        char* e;
        long  d;
        errno = 0;
        d = strtol(str, &e, 10);  /*NB: "str" may be at "in-addr" safely here*/
        if (errno  ||  str == e  ||  e - str > 3  ||  *e != '.'
            ||  d < 0  ||  255 < d
            ||  sprintf(s, "%u", (unsigned int) d) != (int)(e - str)) {
            return 0/*failure*/;
        }
        *--dst = (unsigned char) d;
        assert(e <= end);
        str = ++e;
    }
    return --str;
}


/* NB: "str" is actually bounded by the ".ip6.arpa" suffix */
static const char* x_DNSToIPv6(TNCBI_IPv6Addr* addr,
                               const char* str, size_t len)
{
    unsigned char* dst = addr->octet + sizeof(addr->octet) - 1;
    CORE_DEBUG_ARG(const char* end = str + len;)
    size_t n;

    assert(*end == '.'  &&  len == 4 * sizeof(addr->octet) - 1);

    for (n = 0;  n < 2 * sizeof(addr->octet);  ++n) {
        static const char xdigits[] = "0123456789abcdef";
        int c = tolower((unsigned char)(*str));
        unsigned char val;
        const char*   ptr ;
        assert(c  &&  str < end);
        if (*++str != '.'  ||  !(ptr = strchr(xdigits, c)))
            return 0/*failure*/;
        val = (unsigned char)(ptr - xdigits);
        if (n & 1) {
            val   <<= 4;
            *dst-- |= val;
        } else
            *dst    = val;
        ++str;
    }
    return --str;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_OkDNSEnd(const char* end)
{
    if (isalnum((unsigned char) end[0]))
        return 0/*F*/;
    if (end[0] != '.')
        return 1/*T*/;
    if (isalnum((unsigned char) end[1]))
        return 0/*F*/;
    if (end[1] != '.')
        return 1/*T*/;
    return 0/*F*/;
}


enum ENcbiIP_Form {
    eNcbiIP_Dot = 1,  /* Accept dotted notation */
    eNcbiIP_Dns = 2   /* Accept DNS notation    */
};
typedef unsigned int TNcbiIP_Form;  /* Bitwise OR of ENcbiIP_Form */

static const char* s_StringToAddr(TNCBI_IPv6Addr* addr,
                                  const char* str, size_t len,
                                  TNcbiIP_Form how)
{
    /* IPv6 DNS addresses always require 4*16 = 64 char leading numeric portion;
     * IPv4 DNS addresses require between 7 and 15 char leading numeric portion
     * -- all that while IPv4 literal suffix is longer than that of IPv6. */
    static const size_t kMaxDnsLen = 4 * sizeof(addr->octet) + NCBI_IPV6_DNS_SIZE;
    static const size_t kMinDnsLen = 7 + NCBI_IPV4_DNS_SIZE;
    unsigned int ipv4;
    const char* tmp;
    size_t n;

    assert(how);
    if (!addr)
        return 0/*failure*/;
    memset(addr, 0, sizeof(*addr));
    if (!str  ||  !*str)
        return 0/*failure*/;

    if (!len)
        len = strlen(str);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) str[n]))
            break;
    }
    str += n;
    len -= n;
    for (n = 0;  n < len;  ++n) {
        if (!str[n]  ||  isspace((unsigned char) str[n]))
            break;
    }
    if (!(len = n))
        return 0/*failure*/;

    /* these are static assert()s, actually */
    assert(kMinDnsLen   <= kMaxDnsLen);
    assert(kIPv6DNS.ssz <= kIPv4DNS.ssz);
    if ((how & eNcbiIP_Dns)  &&  len >= kMinDnsLen) {
        /* scan matching from the end, for better efficiency and performance */
        size_t m = NCBI_IPV6_DNS_SIZE;  /* tracks tail length (increasing)   */
        n   = len > kMaxDnsLen ? kMaxDnsLen : len;
        n  -= m;                        /* tracks head length (decreasing)   */
        tmp = str + n;                  /* tracks domain suffix starting dot */
        do {
            if (*tmp == '.') {
                const char*    end;
                TNCBI_IPv6Addr temp;
                assert(len - n >= m);
                /* CORE_TRACEF(("%.*s %.*s", (int) n, str, (int)(len - n), tmp)); */
                if (m >= NCBI_IPV4_DNS_SIZE
                    &&  7/*"x.x.x.x"*/ <= n  &&  n <= 15/*xxx.xxx.xxx.xxx*/
                    &&  x_OkDNSEnd(end = tmp + NCBI_IPV4_DNS_SIZE)
                    &&  strncasecmp(tmp + 1, kIPv4DNS.sfx, NCBI_IPV4_DNS_SIZE - 1) == 0
                    &&  x_DNSToIPv4(&ipv4, str, n) == tmp) {
                    NcbiIPv4ToIPv6(addr, ipv4, 0);
                    return &end[!(*end != '.')];
                }
                if (m >= NCBI_IPV6_DNS_SIZE
                    &&  n == 4 * sizeof(temp.octet) - 1
                    &&  x_OkDNSEnd(end = tmp + NCBI_IPV6_DNS_SIZE)
                    &&  strncasecmp(tmp + 1, kIPv6DNS.sfx, NCBI_IPV6_DNS_SIZE - 1) == 0
                    &&  x_DNSToIPv6(&temp, str, n) == tmp) {
                    *addr = temp;
                    return &end[!(*end != '.')];
                }
            }
            --tmp;
            ++m;
        } while (--n >= 7);
    }
    if (!(how & eNcbiIP_Dot))
        return 0/*failure*/;

    if ((tmp = x_StringToIPv4(&ipv4, str, len)) != 0) {
        NcbiIPv4ToIPv6(addr, ipv4, 0);
        return tmp;        
    }
    return x_StringToIPv6(addr, str, len);
}


extern const char* NcbiIPToAddr(TNCBI_IPv6Addr* addr,
                                const char* str, size_t len)
{
    const char* rv = s_StringToAddr(addr, str, len, eNcbiIP_Dot);
    assert(!rv  ||  rv > str);
    return rv;
}


extern const char* NcbiStringToAddr(TNCBI_IPv6Addr* addr,
                                    const char* str, size_t len)
{
    const char* rv = s_StringToAddr(addr, str, len, eNcbiIP_Dns | eNcbiIP_Dot);
    assert(!rv  ||  rv > str);
    return rv;
}


extern const char* NcbiDNSIPToAddr(TNCBI_IPv6Addr* addr,
                                   const char* str, size_t len)
{
    const char* rv = s_StringToAddr(addr, str, len, eNcbiIP_Dns);    
    assert(!rv  ||  rv > str);
    return rv;
}


extern int/*bool*/ NcbiIsInIPv6Network(const TNCBI_IPv6Addr* base,
                                       unsigned int          bits,
                                       const TNCBI_IPv6Addr* addr)
{
    size_t n;

    if (!base  ||  !addr)
        return 0/*false*/;

    if (bits > (sizeof(base->octet) << 3))
        return 0/*false*/;

    for (n = 0;  n < sizeof(addr->octet);  ++n) {
        unsigned char mask;
        if (!bits) {
            mask  =                  0;
        } else if (8 > bits) {
            mask  = (unsigned char)(~0U << (8 - bits));
            bits  =                  0;
        } else {
            mask  = (unsigned char)(~0U);
            bits -=                  8;
        }
        if ((addr->octet[n] & mask) ^ base->octet[n])
            return 0/*false*/;
    }

    return 1/*true*/;
}


extern int/*bool*/ NcbiIPv6Subnet(TNCBI_IPv6Addr* addr, unsigned int bits)
{
    int/*bool*/ zero = 1/*true*/;

    if (addr) {
        size_t n;
        for (n = 0;  n < sizeof(addr->octet);  ++n) {
            if (!bits) {
                addr->octet[n] = 0;
            } else if (8 > bits) {
                unsigned char mask = (unsigned char)(~0U << (8 - bits));
                if (addr->octet[n] &= mask)
                    zero = 0/*false*/;
                bits  = 0;
            } else {
                bits -= 8;
                if (addr->octet[n])
                    zero = 0/*false*/;
            }
        }
    }

    return !zero;
}


extern int/*bool*/ NcbiIPv6Suffix(TNCBI_IPv6Addr* addr, unsigned int bits)
{
    int/*bool*/ zero = 1/*true*/;

    if (addr) {
        size_t n;
        if (bits < sizeof(addr->octet) * 8)
            bits = sizeof(addr->octet) * 8 - bits;
        else
            bits = 0;
        for (n = 0;  n < sizeof(addr->octet);  ++n) {
            if (!bits) {
                if (addr->octet[n])
                    zero = 0/*false*/;
            } else if (8 > bits) {
                unsigned char mask = (unsigned char)(~0U << (8 - bits));
                if (addr->octet[n] &= ~mask)
                    zero = 0/*false*/;
                bits  = 0;
            } else {
                bits -= 8;
                addr->octet[n] = 0;
            }
        }
    }

    return !zero;
}


extern int/*bool*/ NcbiIPv4Subnet(TNCBI_IPv6Addr* addr, unsigned int bits)
{
    int/*bool*/ zero = 1/*true*/;
    unsigned int ipv4 = 0;

    if (NcbiIsIPv4Ex(addr, 1/*compat okay*/)) {
        if (bits > sizeof(ipv4) * 8)
            bits = sizeof(ipv4) * 8;
        if (NcbiIPv6Subnet(addr, bits + 96)
            &&  (ipv4 = NcbiIPv6ToIPv4(addr, 96)) != 0) {
            zero = 0/*false*/;
        }
    }
    if (addr)
        NcbiIPv4ToIPv6(addr, ipv4, 0);

    return !zero;
}


extern int/*bool*/ NcbiIPv4Suffix(TNCBI_IPv6Addr* addr, unsigned int bits)
{
    int/*bool*/ zero = 1/*true*/;
    unsigned int ipv4 = 0;

    if (NcbiIsIPv4Ex(addr, 1/*compat okay*/)) {
        if (bits > sizeof(ipv4) * 8)
            bits = sizeof(ipv4) * 8;
        if (NcbiIPv6Suffix(addr, bits)
            &&  (ipv4 = NcbiIPv6ToIPv4(addr, 96)) != 0) {
            zero = 0/*false*/;
        }
    }
    if (addr)
        NcbiIPv4ToIPv6(addr, ipv4, 0);

    return !zero;
}
