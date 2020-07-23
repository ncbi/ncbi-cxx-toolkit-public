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

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include <connect/ncbi_iprange.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


extern int/*bool*/ NcbiIsInIPRange(const SIPRange*       range,
                                   const TNCBI_IPv6Addr* addr)
{
    if (range  &&  addr) {
        unsigned int a, b, ip;

        if (range->type == eIPRange_Host) {
            assert(!range->b);
            assert(sizeof(range->a) == sizeof(*addr));
            return !memcmp(&range->a, addr, sizeof(range->a));
        }
        switch (range->type) {
        case eIPRange_Range:
            assert(NcbiIsIPv4(&range->a));
            if (!NcbiIsIPv4(addr))
                return 0/*false*/;
            /* all host byte order */
            a  = SOCK_NetToHostLong(NcbiIPv6ToIPv4(&range->a, 0));
            b  = SOCK_NetToHostLong(                range->b    );
            ip = SOCK_NetToHostLong(NcbiIPv6ToIPv4( addr,     0));
            assert(a < b);
            return a <= ip  &&  ip <= b;
        case eIPRange_Network:
            a = (unsigned int) NcbiIsIPv4(&range->a);
            b = (unsigned int) NcbiIsIPv4(addr);
            if (!(a & b)) {
                return a ^ b
                    ? 0/*false*/
                    : NcbiIsInIPv6Network(&range->a, range->b, addr);
            }
            /* all network byte order */
            a  = NcbiIPv6ToIPv4(&range->a, 0);
            b  =                 range->b;
            ip = NcbiIPv6ToIPv4( addr,     0);
            assert(a  &&  b);
            return !((ip & b) ^ a);
        default:
            assert(0);
            /*FALLTHRU*/
        case eIPRange_None:
        case eIPRange_Application:
            break;
        }
    }
    return 0/*false*/;
}


extern SIPRange NcbiTrueIPRange(const SIPRange* range)
{
    unsigned int a;
    SIPRange retval;
    switch (range ? range->type : eIPRange_None) {
    default:
        assert(0);
        /*FALLTHRU*/
    case eIPRange_None:
    case eIPRange_Application:
        memset(&retval, 0, sizeof(retval));
        /*retval.type = eIPRange_None;*/
        return retval;
    case eIPRange_Host:
        assert(!range->b);
        if (!NcbiIsIPv4(&range->a)) {
            retval = *range;
            return retval;
        }
        retval.a =                 range->a;
        retval.b = NcbiIPv6ToIPv4(&range->a, 0);
        break;
    case eIPRange_Range:
        assert(NcbiIsIPv4(&range->a)  &&  range->b);
        assert(SOCK_NetToHostLong(NcbiIPv6ToIPv4(&range->a, 0)) <
               SOCK_NetToHostLong(range->b));
        retval.a = range->a;
        retval.b = range->b;
        break;
    case eIPRange_Network:
        assert(!NcbiIsEmptyIPv6(&range->a)  &&  range->b);
        if (!NcbiIsIPv4(&range->a)) {
            retval = *range;
            return retval;
        }
        a = NcbiIPv6ToIPv4(&range->a, 0);
        assert(a  &&  !(a & ~range->b));
        retval.a = range->a;
        retval.b =        a | ~range->b;
        break;
    }
    retval.type = eIPRange_Range;
    return retval;
}


#ifdef __GNUC__
inline
#endif  /*__GNUC__*/
static size_t x_size(const char* dst, size_t len, const char* ptr)
{
    return len > (size_t)(ptr - dst) ? len - (size_t)(ptr - dst) : 0; 
}
 

extern const char* NcbiDumpIPRange(const SIPRange* range,
                                   char* buf, size_t bufsize)
{
    char result[150];

    if (!buf  ||  !bufsize)
        return 0;
    if (!range) {
        *buf = '\0';
        return 0;
    }
    if (range->type == eIPRange_Application) {
        *buf = '\0';
        return buf;
    }

    if (range->type != eIPRange_None) {
        SIPRange temp = NcbiTrueIPRange(range);
        char* s = result;
        switch (range->type) {
        case eIPRange_Host:
            assert(!range->b);
            memcpy(s, "Host", 4);
            s += 4;
            break;
        case eIPRange_Range:
            assert(NcbiIsIPv4(&range->a));
            assert(range->b);
            memcpy(s, "Range", 5);
            s += 5;
            break;
        case eIPRange_Network:
            assert(range->b);
            memcpy(s, "Network", 7);
            s += 7;
            break;
        default:
            *buf = '\0';
            assert(0);
            return 0;
        }
        *s++ = ' ';
        if (temp.type != eIPRange_Range) {
            assert(!NcbiIsIPv4(&range->a));
            assert(memcmp(&temp, range, sizeof(temp)) == 0);
            s = NcbiIPv6ToString(s, x_size(result, sizeof(result), s),
                                 &temp.a);
            assert(s + 40 < result + sizeof(result));
            if (s  &&  temp.type == eIPRange_Network)
                sprintf(s, "/%u", temp.b);
        } else {
            assert(memcmp(&temp.a, &range->a, sizeof(temp.a)) == 0);
            if (SOCK_ntoa(NcbiIPv6ToIPv4(&temp.a, 0),
                          s, x_size(result, sizeof(result), s)) != 0) {
                strcpy(s++, "?");
            } else
                s += strlen(s);
            if (range->type != eIPRange_Host) {
                *s++ = '-';
                if (SOCK_ntoa(temp.b,
                              s, x_size(result, sizeof(result), s)) != 0) {
                    strcpy(s, "?");
                }
            }
        }
    } else
        strcpy(result, "None");

    return strncpy0(buf, result, bufsize - 1);
}


extern int/*bool*/ NcbiParseIPRange(SIPRange* range, const char* str)
{
    unsigned int addr, temp;
    const char* p;
    size_t len, n;
    int dots;
    char* e;
    long d;

    if (!range  ||  !str)
        return 0/*failure*/;

    if (!*str) {
        memset(range, 0, sizeof(*range));
        /*range->type = eIPRange_None;*/
        return 1/*success*/;
    }

    p = NcbiIPToAddr(&range->a, str, len = strlen(str));
    if (p) {
        assert(p > str);
        if (!*p /*p == str + len*/) {
            range->b    = 0;
            range->type = eIPRange_Host;
            return 1/*success*/;
        }
        if (*p++ == '/'  &&  !isspace((unsigned char)(*p))) {
            errno = 0;
            d = strtol(p, &e, 10);
            if (!errno  &&  p != e  &&  !*e  &&  d > 0) {
                int/*bool*/ ipv4 = NcbiIsIPv4(&range->a);
                if (ipv4  &&  d <= 32) {
                    if (!d  ||  d == 32) {
                        range->b    = 0;
                        range->type = eIPRange_Host;
                        return 1/*success*/;
                    }
                    addr = SOCK_NetToHostLong(NcbiIPv6ToIPv4(&range->a, 0));
                    temp = (unsigned int)(~0UL << (32 - d));
                    range->b    = SOCK_HostToNetLong(temp);
                    range->type = eIPRange_Network;
                    return addr  &&  !(addr & ~temp);
                } else if (!ipv4
                           &&  (size_t) d <= (sizeof(range->a.octet) << 3)) {
                    if (!d  ||  d == (sizeof(range->a.octet) << 3)) {
                        range->b    = 0;
                        range->type = eIPRange_Host;
                        return 1/*success*/;
                    }
                    if (NcbiIsEmptyIPv6(&range->a))
                        return 0/*failure*/;
                    range->b    = (unsigned int) d; /* d > 0 */
                    range->type = eIPRange_Network;
                    d = (long)(sizeof(range->a.octet) << 3) - d;
                    assert(d > 0);
                    for (n = sizeof(range->a.octet);  n > 0;  --n) {
                        if (d >= 8) {
                            if (range->a.octet[n - 1] & ~0)
                                return 0/*failure*/;
                            d -= 8;
                        } else if (d) {
                            if (range->a.octet[n - 1] & ~(~0 << d))
                                return 0/*failure*/;
                            break;
                        } else
                            break;
                    }
                    return 1/*success*/;
                } else
                    return 0/*failure*/;
            }
        }
    }
    if (SOCK_isip(str)) {
        /* forbid other misleading IP representations */
        assert(!SOCK_isipEx(str, 1/*fullquad*/));
        return 0/*failure*/;
    }

    p = str;
    dots = 0;
    range->type = eIPRange_Host;
    addr = 0/*not actually necessary*/;
    for (;;) {
        const char* t;
        if (*p != '*') {
            char s[4];
            errno = 0;
            d = strtol(p, &e, 10);
            if (errno  ||  p == e  ||  e - p > 3  ||  d < 0  ||  255 < d
                ||  sprintf(s, "%u", (unsigned int) d) != (int)(e - p)) {
                break/*goto out*/;
            }
            p = e;
        } else if (!*++p  &&  dots) {
            temp = (unsigned int)((4 - dots) << 3);
            addr <<= temp;
            NcbiIPv4ToIPv6(&range->a, SOCK_HostToNetLong(addr), 0);
            range->b    = SOCK_HostToNetLong((unsigned int)(~0UL << temp));
            range->type = eIPRange_Network;
            return 1/*success*/;
        } else
            return 0/*failure (asterisks not valid in hostnames)*/;
        switch (range->type) {
        case eIPRange_Host:
            addr <<= 8;
            addr  |= (unsigned int) d;
            if (*p != '.') {
                addr <<= (3 - dots) << 3;
                switch (*p) {
                case '/':
                    range->type = eIPRange_Network;
                    break;
                case '-':
                    range->type = eIPRange_Range;
                    p++;
                    continue;
                default:
                    goto out;
                }
            } else if (++dots <= 3) {
                p++;
                continue;
            } else
                goto out;
            assert(*p == '/'  &&  range->type == eIPRange_Network);
            t = NcbiStringToIPv4(&range->b, ++p, 0);
            if (!t  ||  *t)
                continue;
            NcbiIPv4ToIPv6(&range->a, SOCK_HostToNetLong(addr), 0);
            if (!range->b  ||  !(temp = ~SOCK_NetToHostLong(range->b))) {
                range->b    = 0;
                range->type = eIPRange_Host;
                return 1/*success*/;
            }
            return !(temp & (temp + 1))  &&  addr  &&  !(addr & temp);
        case eIPRange_Range:
            if (*p)
                goto out;
            temp  = dots > 0 ? addr : 0;
            temp &= (unsigned int)(~((1UL << ((4 - dots) << 3)) - 1));
            temp |= (unsigned int)   (d   << ((3 - dots) << 3));
            temp |= (unsigned int)  ((1U  << ((3 - dots) << 3)) - 1);
            NcbiIPv4ToIPv6(&range->a, SOCK_HostToNetLong(addr), 0);
            if (addr == temp) {
                range->b    = 0;
                range->type = eIPRange_Host;
                return 1/*success*/;
            }
            range->b = SOCK_HostToNetLong(temp);
            return addr < temp;
        case eIPRange_Network:
            if (*p  ||  d > 32)
                return 0/*failure (slashes not valid in hostnames)*/;
            NcbiIPv4ToIPv6(&range->a, SOCK_HostToNetLong(addr), 0);
            if (!d  ||  d == 32) {
                range->b    = 0;
                range->type = eIPRange_Host;
                return 1/*success*/;
            }
            temp = (unsigned int)(~0UL << (32 - d));
            range->b = SOCK_HostToNetLong(temp);
            return addr  &&  !(addr & ~temp);
        default:
            assert(0);
            return 0/*failure*/;
        }
    }
 out:

    /* last resort (and maybe expensive one): try as a regular host name */
    range->b    = 0;
    range->type = eIPRange_Host;
    p = NcbiDNSIPToAddr(&range->a, str, len);
    assert(!p  ||  p > str);
    if (p  &&  !*p)
        return 1/*success*/;
    if (!(addr = SOCK_gethostbyname(str)))
        return 0/*failure*/;
    NcbiIPv4ToIPv6(&range->a, addr, 0);
    return 1/*success*/;
}
