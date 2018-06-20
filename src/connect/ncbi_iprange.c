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
            assert(ip  &&  a < b);
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
            assert(a  &&  b  &&  ip);
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
    if (range) {
        switch (range->type) {
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
    } else
        memset(&retval, 0, sizeof(retval));
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

    if (!range  ||  !buf  ||  !bufsize)
        return 0;

    if (range->type == eIPRange_Application) {
        *buf = '\0';
        return buf;
    }

    if (range->type != eIPRange_None) {
        char* s = result;
        SIPRange temp = NcbiTrueIPRange(range);
        switch (range->type) {
        case eIPRange_Host:
            assert(!range->b);
            strcpy(s, "Host");
            s += 4;
            break;
        case eIPRange_Range:
            assert(range->b);
            assert(NcbiIsIPv4(&range->a));
            strcpy(s, "Range");
            s += 5;
            break;
        case eIPRange_Network:
            assert(range->b);
            strcpy(s, "Network");
            s += 7;
            break;
        default:
            assert(0);
            return 0;
        }
        *s++ = ' ';
        if (!NcbiIsIPv4(&range->a)) {
            assert(range->type != eIPRange_Range);
            s = NcbiIPv6ToString(s, x_size(result, sizeof(result), s),
                                 &range->a);
            assert(s + 40 < result + sizeof(result));
            if (s  &&  range->type == eIPRange_Network)
                sprintf(s, "/%u", range->b);
        } else {
            if (SOCK_ntoa(NcbiIPv6ToIPv4(&range->a, 0),
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
    const char* t;
    size_t n;
    char* e;
    long d;

    if (!range  ||  !str)
        return 0/*failure*/;

    if (!*str) {
        memset(range, 0, sizeof(*range));
        /*range->type = eIPRange_None;*/
        return 1/*success*/;
    }

    t = NcbiStringToAddr(&range->a, str, strlen(str));
    if (t  &&  t != str) {
        if (!*t /*t == str + strlen(str)*/) {
            range->b    = 0;
            range->type = eIPRange_Host;
            return 1/*success*/;
        }
        if (*t++ == '/'  &&  !isspace((unsigned char)(*t))) {
            errno = 0;
            d = strtol(t, &e, 10);
            if (!errno  &&  t != e  &&  !*e  &&  d > 0) {
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
                    range->b    = (unsigned int) d; /* d > 0 */
                    range->type = eIPRange_Network;
                    d = (long)(sizeof(range->a.octet) << 3) - d;
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
                    return !NcbiIsEmptyIPv6(&range->a);
                } else
                    return 0/*failure*/;
            }
        }
    }

    if (!SOCK_isip(str)) {
        int dots = 0;
        const char* p = str;
        range->type = eIPRange_Host;
        addr = 0/*not actually necessary*/;
        for (;;) {
            char s[4];
            if (*p != '*') {
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
                temp &= (unsigned int)(~((1 << ((4 - dots) << 3)) - 1));
                temp |= (unsigned int)   (d << ((3 - dots) << 3));
                temp |= (unsigned int)  ((1 << ((3 - dots) << 3)) - 1);
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
        ;
    } else { /* NB: SOCK_gethostbyname() returns 0 on an unknown host */
        if (strcasecmp(str, "255.255.255.255") == 0) {
            addr = (unsigned int)(~0UL);
            NcbiIPv4ToIPv6(&range->a, addr, 0);
            range->b    = 0;
            range->type = eIPRange_Host;
            return 1/*success*/;
        }
        for (n = 0;  n < 4;  ++n) {
            size_t len = 1 + (n << 1);
            if (strncmp(str, "0.0.0.0", len) == 0  &&  !str[len]) {
                NcbiIPv4ToIPv6(&range->a, 0, 0);
                range->b    = 0;
                range->type = eIPRange_Host;
                return 1/*success*/;
            }
        }
        /* forbid other misleading IP representations */
        if (!SOCK_isipEx(str, 1/*fullquad*/))
            return 0/*failure*/;
    }

    if (!(addr = SOCK_gethostbyname(str)))
        return 0/*failure*/;
    NcbiIPv4ToIPv6(&range->a, addr, 0);
    range->b    = 0;
    range->type = eIPRange_Host;
    return 1/*success*/;
}
