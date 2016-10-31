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

#include "ncbi_assert.h"
#include "ncbi_ansi_ext.h"
#include <connect/ncbi_socket.h>
#include <connect/ncbi_ipv6.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct SIPDNSsfx {
    const char*  sfx;
    const size_t len;
};
static const struct SIPDNSsfx kIPv6DNS = { ".ip6.arpa",      8 };
static const struct SIPDNSsfx kIPv4DNS = { ".in-addr.arpa", 13 };


static int/*bool*/ x_NcbiIsIPv4(const TNCBI_IPv6Addr* addr, int/*bool*/ mapped)
{
    /* RFC 4291 2.1, 3
       NB: 2.5.5.1 and 2.5.5.2 - both obsoleted by RFC 6052 2.1 */
    unsigned short word;
    size_t i;
    for (i = 0;  i < 5;  ++i) {
        memcpy(&word, addr->octet + (i << 1), sizeof(word));
        if (word)
            return 0/*false*/;
    }
    memcpy(&word, addr->octet + (5 << 1), sizeof(word));
    if    (word == 0x0000) {
        /* IPv4-compatible IPv6 */
        unsigned int temp;
        memcpy(&temp, addr->octet + sizeof(addr->octet) - sizeof(temp),
               sizeof(temp));
        return SOCK_NetToHostLong(temp) & 0xFF000000 ? 1/*T*/ : 0/*F*/;
    }
    /* IPv6 mapped IPv4 */
    return word == 0xFFFF  &&  mapped ? 1/*true*/ : 0/*false*/;
}


extern int/*bool*/ NcbiIsIPv4(const TNCBI_IPv6Addr* addr)
{
    return x_NcbiIsIPv4(addr, 1/*mapped*/);
}


extern unsigned int NcbiIPv6ToIPv4(const TNCBI_IPv6Addr* addr, size_t pfxlen)
{
    unsigned int ipv4;
    static const size_t size = sizeof(ipv4);
    if (pfxlen == 0)
        pfxlen  = 96;
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
        assert(0);
        return (unsigned int)(-1L)/*failure*/;
    }
    return ipv4;
}


extern int/*bool*/ NcbiIPv4ToIPv6(TNCBI_IPv6Addr* addr,
                                  unsigned int ipv4, size_t pfxlen)
{
    static const size_t size = sizeof(ipv4);
    if (pfxlen == 0) {
        /* creates IPv6 mapped */
        memset(addr, 0, sizeof(*addr));
        memset(addr->octet + (5 << 1), '\xFF', sizeof(unsigned short));
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
    return 1/*success*/;
}


/* Parse "src" as an IP address, and return 0 if failed, otherwise a pointer to
 * the first non-parsed char (which is neither a digit nor a dot) and "dst"
 * updated with the just read IPv4 address in network byte order.
 */
static const char* x_StringToIPv4(unsigned int* dst,
                                  const char* src, size_t len)
{
    size_t n;
    int octets = 0;
    unsigned int tmp;
    int/*bool*/ was_digit = 0/*false*/;
    unsigned char *ptr = (unsigned char*) &tmp;

    *ptr = 0;
    for (n = 0;  n < len;  ++n) {
        unsigned char c = src[n];
        if ('0' <= c  &&  c <= '9') {
            unsigned int val;
            if (was_digit  &&  !*ptr)
                return 0/*leading "0" in octet*/;
            val = *ptr * 10 + (c - '0');
            if (val > 255)
                return 0;
            *ptr = val;
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
    return src + n;
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
    gap = ipv4 = 0/*false*/;
    token[t = 0].ptr = str + n;
    while (n <= len) {
        assert(t <= maxt);
        if (n == len  ||  str[n] == ':') {
            token[t].len = (size_t)(&str[n] - token[t].ptr);
            if (token[t].len) {
                if (n++ == len) {
                    ++t;
                    break;
                }
            } else {
                if (n++ == len)
                    break;
                if (gap++)  /*RFC 4291 2.2, 2*/
                    return 0/*failure*/;
            }
            /*str[n - 1] == ':'*/
            token[t].len++;
            if (++t > maxt)
                return 0/*failure*/;
            token[t].ptr = str + n;
            continue;
        }
        if (!isxdigit((unsigned char) str[n])) {
            token[t].len = (size_t)(&str[n] - token[t].ptr);
            if (token[t].len) {
                if (str[n] == '.') {
                    if (t < maxt - sizeof(ip) / sizeof(word)) {
                        const char* end
                            = x_StringToIPv4(&ip,
                                             token[t].ptr,
                                             token[t].len + (len - n));
                        if (end  &&  *end != ':') {
                            token[t].len = (size_t)(end - token[t].ptr);
                            maxt -= sizeof(ip) / sizeof(word);
                            ipv4 = 1/*true*/;
                            break;
                        }
                    }
                    return 0/*failure*/;
                }
                ++t;
            } else if (!gap)
                return 0/*failure*/;
            break;
        }
        n++;
    }

    if (!t)
        return 0/*failure*/;
    if (t < maxt  &&  !gap)
        return 0/*failure*/;
    if (t > maxt)
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
            if (errno  ||  val ^ (val & 0xFFFF))
                return 0/*failure*/;
            assert(end == token[n].ptr + token[n].len - (*end == ':'));
            if (*end == ':'  &&  n == t - !ipv4)
                return 0/*failure*/;
            word = SOCK_HostToNetShort((unsigned short) val);
            memcpy(dst, &word, sizeof(word));
            dst += sizeof(word);
        } else {
            gap = (maxt - t) * sizeof(word) + sizeof(word);
            memset(dst, '\0', gap);
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
                                    const char* src, size_t len)
{
    size_t n;
    if (!addr)
        return 0/*failure*/;
    *addr = 0;
    if (!src)
        return 0/*failure*/;
    if (!len)
        len = strlen(src);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) src[n]))
            break;
    }
    return x_StringToIPv4(addr, src + n, len - n);
}


extern const char* NcbiStringToIPv6(TNCBI_IPv6Addr* addr,
                                    const char* src, size_t len)
{
    size_t n;
    if (!addr)
        return 0/*failure*/;
    memset(addr, 0, sizeof(*addr));
    if (!src  ||  !*src)
        return 0/*failure*/;
    if (!len)
        len = strlen(src);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) src[n]))
            break;
    }
    return x_StringToIPv6(addr, src + n, len - n);
}


static char* x_IPv6ToString(char* buf, size_t bufsize,
                            const TNCBI_IPv6Addr* addr)
{
    char ipv6[64/*enough for sizeof(8 * "xxxx:")*/];
    char ipv4[sizeof("255.255.255.255")];
    unsigned short word;
    struct {
        size_t pos;
        size_t len;
    } gap[sizeof(addr->octet) / sizeof(word)];
    size_t i, n, z, zlen;
    char* ptr = ipv6;

    if (x_NcbiIsIPv4(addr, 0/*compat*/)) {
        unsigned int ip;
        n = sizeof(addr->octet) - sizeof(ip);
        memcpy(&ip, addr->octet + n, sizeof(ip));
        SOCK_ntoa(ip, ipv4, sizeof(ipv4));
        n /= sizeof(word);
    } else {
        n = sizeof(addr->octet) / sizeof(word);
        *ipv4 = '\0';
    }

    gap[0].pos = 0;
    i = z = zlen = 0;
    while (i <= n) {
        memcpy(&word, &addr->octet[i * sizeof(word)], sizeof(word));
        if (i == n  ||  word) {
            size_t len = i - gap[z].pos;
            if (len > 1) {  /*RFC 5952 4.2.2*/
                gap[z++].len = len;
                if (zlen < len)
                    zlen = len;  /*RFC 5952 4.2.1*/
            }
            if (i == n)
                break;
            assert(z < sizeof(gap) / sizeof(gap[0]));
            gap[z].pos = ++i;
        } else
            ++i;
    }

    i = z = 0;
    while (i < n) {
        if (zlen  &&  gap[z].pos == i) {
            assert(zlen > 1);
            if (zlen == gap[z].len) {
                *ptr++ = ':';
                if (zlen == n - i)
                    *ptr++ = ':';
                i += zlen;
                zlen = 0;  /*RFC 5952 4.2.3*/
                continue;
            }
            z++;
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
    z = n + i;
    if (z < bufsize) {
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

    return x_IPv6ToString(buf, bufsize, addr);
}


extern char* NcbiAddrToString(char* buf, size_t bufsize,
                              TNCBI_IPv6Addr* addr)
{
    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';

    if (NcbiIsIPv4(addr)) {
        unsigned int ipv4 = NcbiIPv6ToIPv4(addr, 0);
        return x_IPv4ToString(buf, bufsize, &ipv4);
    }
    return x_IPv6ToString(buf, bufsize, addr);
}


extern const char* NcbiAddrToDNS(char* buf, size_t bufsize,
                                 TNCBI_IPv6Addr* addr)
{
    char tmp[sizeof(addr->octet)*2 + 16/*slack*/], *dst = tmp;
    const struct SIPDNSsfx* sfx;
    unsigned char* src;
    size_t n, len;

    if (!buf  ||  !bufsize)
        return 0;
    *buf = '\0';

    len = 0;
    src = addr->octet + sizeof(addr->octet) - 1;
    if (NcbiIsIPv4(addr)) {
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
    if (len + sfx->len < bufsize) {
        memcpy(buf, tmp,               len);
        buf += len;
        memcpy(buf, sfx->sfx + 1, sfx->len);
        buf += sfx->len;
        return buf;
    }

    return 0;
}


static const char* x_DNSToIPv4(unsigned int* addr,
                               const char* src, size_t len)
{
    unsigned int temp;
    if (!(src = x_StringToIPv4(&temp, src, len)))
        return 0;
    *addr = SOCK_NetToHostLong(temp);
    return src;
}


static const char* x_DNSToIPv6(TNCBI_IPv6Addr* addr,
                               const char* src, size_t len)
{
    static const char xdigits[] = "0123456789abcdef";
    const char* end = src + len;
    TNCBI_IPv6Addr temp;
    unsigned char* dst;
    size_t n;
    if (len != 4 * sizeof(addr->octet) - 1)
        return 0;
    dst = temp.octet + sizeof(temp.octet) - 1;
    for (n = 0;  n < 2 * sizeof(addr->octet);  ++n) {
        const char* ptr = strchr(xdigits, tolower((unsigned char) *src));
        unsigned char val;
        if (!ptr)
            return 0;
        val = ptr - xdigits;
        if (n & 1) {
            val <<= 4;
            *dst |= val;
        } else
            *dst  = val;
        if (++src > end  ||  *src != '.')
            return 0;
        dst--;
    }
    *addr = temp;
    return src;
}


extern const char* NcbiStringToAddr(TNCBI_IPv6Addr* addr,
                                    const char* src, size_t len)
{
    unsigned int ipv4;
    const char* tmp;
    int dns_only;
    size_t n;

    if (!addr)
        return 0/*failure*/;
    memset(addr, 0, sizeof(*addr));
    if (!src  ||  !*src)
        return 0/*failure*/;

    if (!len)
        len = strlen(src);
    for (n = 0;  n < len;  ++n) {
        if (!isspace((unsigned char) src[n]))
            break;
    }
    src += n;
    len -= n;
    for (n = 0;  n < len;  ++n) {
        if (!src[n]  ||  isspace((unsigned char) src[n]))
            break;
    }
    if (!(len = n))
        return 0/*failure*/;

    dns_only = src[--n] == '.' ? 1/*true*/ : 0/*false*/;
    if (len > kIPv4DNS.len
        &&  strncasecmp(tmp = src + len - (kIPv4DNS.len + dns_only),
                        kIPv4DNS.sfx, kIPv4DNS.len) == 0) {
        if (x_DNSToIPv4(&ipv4, src, len - kIPv4DNS.len) == tmp) {
            NcbiIPv4ToIPv6(addr, ipv4, 0);
            return tmp + (kIPv4DNS.len + dns_only);
        } else if (dns_only)
            return 0/*failure*/;
    }
    if (len > kIPv6DNS.len
        &&  strncasecmp(tmp = src + len - (kIPv6DNS.len + dns_only),
                        kIPv6DNS.sfx, kIPv6DNS.len) == 0) {
        if (x_DNSToIPv6(addr, src, len - kIPv6DNS.len) == tmp)
            return tmp + (kIPv6DNS.len + dns_only);
        else if (dns_only)
            return 0/*failure*/;
    }

    if ((tmp = x_StringToIPv4(&ipv4, src, len)) != 0) {
        NcbiIPv4ToIPv6(addr, ipv4, 0);
        return tmp;        
    }
    return x_StringToIPv6(addr, src, len);
}


int/*bool*/ NcbiIsInIPv6Network(const TNCBI_IPv6Addr* base,
                                unsigned int          bits,
                                const TNCBI_IPv6Addr* addr)
{
    size_t n;

    if (bits > (sizeof(base->octet) << 3))
        return 0/*false*/;

    for (n = 0;  n < sizeof(addr->octet);  ++n) {
        unsigned char mask;
        if (!bits)
            mask  = 0;
        else if (8 > bits) {
            mask  = 0x0FFU << (8 - bits);
            bits  = 0;
        } else {
            mask  = '\xFF';
            bits -= 8;
        }
        if ((addr->octet[n] & mask) ^ base->octet[n])
            return 0/*false*/;
    }

    return 1/*true*/;
}
