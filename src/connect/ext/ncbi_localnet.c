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
 *   Get IP address of CGI client and determine the IP locality
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <connect/ncbi_socket.h>
#include <connect/ext/ncbi_localnet.h>
#include <stdlib.h>
#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#  include <netinet/in.h>
#endif /*NCBI_OS_UNIX*/


#if defined(_DEBUG)  &&  !defined(NDEBUG)
/*#  define NCBI_LOCALNET_DEBUG  1*/
#endif /*_DEBUG && !NDEBUG*/


#ifndef   INADDR_LOOPBACK
#  define INADDR_LOOPBACK  0x7F000001
#endif /*!INADDR_LOOPBACK*/


/* Extract local net if Class A */
#ifdef IN_CLASSA_NET
#  define X_LOCALNET(addr)  ((addr) & IN_CLASSA_NET)
#else
#  define X_LOCALNET(addr)  ((addr) & 0xFF000000)
#endif 


/* 0/8 -- "This network", RFC1122 3.2.1.3 (a)-(b), RFC5735 4 */
#define X_THISNET(addr)     (!X_LOCALNET(addr))


/* 127/8 -- Loopback, RFC1122 3.2.1.3 (g), RFC5735 4 */
#ifndef IN_LOOPBACK
#  if defined(IN_LOOPBACKNET)  &&  defined(IN_CLASSA_NSHIFT)
#    define IN_LOOPBACK(addr)   (!(X_LOCALNET(addr) ^ (IN_LOOPBACKNET << IN_CLASSA_NSHIFT)))
#  else
#    define IN_LOOPBACK(addr)   (!(X_LOCALNET(addr) ^ (INADDR_LOOPBACK-1)))
#  endif
#endif /*!IN_LOOPBACK*/


/* 224/4 -- Class D: Multicast, RFC3171, RFC5735 4 */
#ifndef IN_MULTICAST
#  if defined(IN_CLASSD)
#    define IN_MULTICAST(addr)  IN_CLASSD(addr)
#  else
#    define IN_MULTICAST(addr)  (!(((addr) & 0xF0000000) ^ 0xE0000000))
#  endif
#endif /*!IN_MULTICAST*/


/* Combined multicast IP range (224/4: 224.0.0.0-239.255.255.255) and
 * IN_BADCLASS (Class E, nonroutable IPs, 240/4), which includes the
 * "limited broadcast" INADDR_NONE, RFC1122 3.2.1.3 (c), RFC5735 4:
 * IN_MULTICAST(addr)  ||  IN_BADCLASS(addr)
 */
#ifdef IN_EXPERIMENTAL
#  define X_CLASS_DEF(addr)     IN_EXPERIMENTAL(addr)
#else
#  define X_CLASS_DEF(addr)     (!(((addr) & 0xE0000000) ^ 0xE0000000))
#endif


#ifdef    SizeOf
#  undef  SizeOf
#endif  /*SizeOf*/
#define   SizeOf(a)  (sizeof(a) / sizeof((a)[0]))


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsPrivateIP(unsigned int addr)
{
    return X_THISNET(addr)  ||  IN_LOOPBACK(addr)
        /* private [non-routable] IP ranges, according to RFC1918, RFC5735 4 */
        ||  !((addr & 0xFF000000) ^ 0x0A000000) /* 10/8                      */
        ||  !((addr & 0xFFF00000) ^ 0xAC100000) /* 172.16.0.0-172.31.255.255 */
        ||  !((addr & 0xFFFF0000) ^ 0xC0A80000) /* 192.168/16                */
        /* non-routable ranges: multicast, Class E and "limited broadcast"   */
        ||  X_CLASS_DEF(addr);
}


extern int/*bool*/ NcbiIsPrivateIP(unsigned int ip)
{
    return x_IsPrivateIP(SOCK_NetToHostLong(ip));
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsAPIPA(unsigned int addr)
{
    /* RFC3927, RFC5735 4 */
    return !((addr & 0xFFFF0000) ^ 0xA9FE0000); /* 169.254/16 per IANA */
}


extern int/*bool*/ NcbiIsAPIPA(unsigned int ip)
{
    return x_IsAPIPA(SOCK_NetToHostLong(ip));
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_IsPrivateIP(unsigned int ip)
{
    unsigned int addr = SOCK_NetToHostLong(ip);
    return x_IsPrivateIP(addr)  ||  x_IsAPIPA(addr);
}


/* NB: "str" is either NULL or non-empty */
static const TNCBI_IPv6Addr* x_StringToAddr(TNCBI_IPv6Addr* addr,
                                            const char*     str)
{
    const char* end;
    assert(addr);
    if (!str) {
        memset(addr, 0, sizeof(*addr));
        return 0;
    }
    assert(*str);
    end = NcbiStringToAddr(addr, str, 0);
    return end  &&  !*end ? addr : SOCK_gethostbyname6(addr, str);
}


/* NB: returns either NULL or non-empty value for the "name" */
static const char* s_SearchTrackingEnv(const char*        name,
                                       const char* const* tracking_env)
{
    const char* result;

    if (!tracking_env) {
        // CORE_LOCK_READ;
        result = getenv(name);
        // CORE_UNLOCK;
#ifdef NCBI_LOCALNET_DEBUG
        CORE_LOGF(eLOG_Trace, ("Getenv('%s') = %s%s%s", name,
                               result ? "\""   : "",
                               result ? result : "NULL",
                               result ? "\""   : ""));
#endif /*NCBI_LOCALNET_DEBUG*/
    } else {
        size_t len = strlen(name);
        const char* const* str;
        result = 0;
        for (str = tracking_env;  *str;  ++str) {
            if (strncasecmp(*str, name, len) == 0  &&  (*str)[len] == '=') {
                result = &(*str)[++len];
                break;
            }
        }
#ifdef NCBI_LOCALNET_DEBUG
        CORE_LOGF(eLOG_Trace, ("Tracking '%s' = %s%s%s", name,
                               result ? "\""   : "",
                               result ? result : "NULL",
                               result ? "\""   : ""));
#endif /*NCBI_LOCALNET_DEBUG*/
    }
    return result  &&  *(result += strspn(result, " \t")) ? result : 0;
}


static const char* s_SearchForwardedFor(TNCBI_IPv6Addr*    addr,
                                        const char* const* tracking_env)
{
    const char* f = s_SearchTrackingEnv("HTTP_X_FORWARDED_FOR", tracking_env);
    TNCBI_IPv6Addr xaddr;
    int/*bool*/ external;
    char *p, *q, *r, *s;

    if (!f)
        return 0;
    r = 0;
    external = !(s = strdup(f)) ? 1/*T*/ : 0/*F*/;
    for (p = s;  p  &&  *p;  p = q + strspn(q, ", \t")) {
        int/*bool*/ private_ip;
        q = p + strcspn(p, ", \t");
        if (*q)
            *q++ = '\0';
        if (!*p  ||  NcbiIsEmptyIPv6(x_StringToAddr(&xaddr, p))) {
#ifdef NCBI_LOCALNET_DEBUG
            CORE_LOG(eLOG_Trace, "Forcing external");
#endif /*NCBI_LOCALNET_DEBUG*/
            external = 1/*true*/;
            r        = 0/*error*/;
        } else if (!(private_ip = s_IsPrivateIP(NcbiIPv6ToIPv4(&xaddr, 0)))
                   &&  !NcbiIsLocalIPEx(&xaddr, 0)) {
            r        = p;
            *addr    = xaddr;
            break;
        } else if (!external
                   &&  (!r
                        ||  (!private_ip
                             &&  s_IsPrivateIP(NcbiIPv6ToIPv4(addr, 0))))) {
            r        = p;
            *addr    = xaddr;
        }
    }
    if (r) {
        memmove(s, r, strlen(r) + 1);
        assert(*s  &&  !NcbiIsEmptyIPv6(addr));
        return s;
    }
    if (s)
        free(s);
    memset(addr, 0, sizeof(*addr));
    return external ? "" : 0;
}


/* The environment checked here must be in correspondence with the tracking
 * environment created by CTrackingEnvHolder::CTrackingEnvHolder()
 * (header: <cgi/ncbicgi.hpp>, source: cgi/ncbicgi.cpp, library: xcgi)
 */
extern TNCBI_IPv6Addr NcbiGetCgiClientIPv6Ex(ECgiClientIP       flag,
                                             char*              buf,
                                             size_t             buf_size,
                                             const char* const* tracking_env)
{
    struct {
        const char*    host;
        TNCBI_IPv6Addr addr;
    } probe[4];
    int/*bool*/ external = 0/*false*/;
    const char* host = 0;
    TNCBI_IPv6Addr addr;
    size_t i;

    memset(&addr, 0, sizeof(addr));
    memset(probe, 0, sizeof(probe));
    for (i = 0;  i < SizeOf(probe);  ++i) {
        int/*bool*/ bad_addr = 1/*true*/;
        switch (i) {
        case 0:
            probe[i].host = s_SearchTrackingEnv("HTTP_CAF_PROXIED_HOST",
                                                tracking_env);
            break;
        case 1:
            /* NB: sets probe[i].addr */
            probe[i].host = s_SearchForwardedFor(&probe[i].addr,
                                                 tracking_env);
            bad_addr = 0/*false*/;
            break;
        case 2:
            probe[i].host = s_SearchTrackingEnv("PROXIED_IP",
                                                tracking_env);
            break;
        case 3:
            probe[i].host = s_SearchTrackingEnv("HTTP_X_FWD_IP_ADDR",
                                                tracking_env);
            break;
        default:
            assert(0);
            continue;
        }
        if (!probe[i].host)
            continue;
        assert(*probe[i].host  ||  !bad_addr/*forwarded_for*/);
        if ( bad_addr)
            x_StringToAddr(&probe[i].addr, probe[i].host);
        bad_addr = NcbiIsEmptyIPv6(&probe[i].addr);
        if (!bad_addr  &&  NcbiIsLocalIPEx(&probe[i].addr, 0))
            continue;
#ifdef NCBI_LOCALNET_DEBUG
        CORE_LOG(eLOG_Trace, "External on");
#endif /*NCBI_LOCALNET_DEBUG*/
        external = 1/*true*/;
        if (bad_addr/*unresolvable*/
            ||  (bad_addr = !s_IsPrivateIP(NcbiIPv6ToIPv4(&probe[i].addr, 0)))
            ||  !host/*1st occurrence*/) {
            assert(probe[i].host);
            host = probe[i].host;
            addr = probe[i].addr;
            if (bad_addr)
                break;
        }
    }
    if (!external) {
        assert(NcbiIsEmptyIPv6(&addr));
        for (i = flag == eCgiClientIP_TryLeast ? 1 : 0;
             i <= (flag == eCgiClientIP_TryAll ? 8 : 7);  ++i) {
            switch (i) {
            case 0:
                host = s_SearchTrackingEnv("HTTP_CLIENT_HOST",
                                           tracking_env);
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                host = probe[i - 1].host;
                break;
            case 5:
                host = s_SearchTrackingEnv("HTTP_X_REAL_IP",
                                           tracking_env);
                break;
            case 6:
                host = s_SearchTrackingEnv("REMOTE_HOST",
                                           tracking_env);
                if (host) {
                    /* consider as a special case per semantics in RFC-3875 */
                    x_StringToAddr(&addr, s_SearchTrackingEnv("REMOTE_ADDR",
                                                              tracking_env));
                }
                break;
            case 7:
                host = s_SearchTrackingEnv("REMOTE_ADDR",
                                           tracking_env);
                break;
            case 8:
                host = s_SearchTrackingEnv("NI_CLIENT_IPADDR",
                                           tracking_env);
                break;
            default:
                assert(0);
                continue;
            }
            if (host) {
                assert(*host);
                if (1 <= i  &&  i <= 4)
                    addr = probe[i - 1].addr;
                else if (i != 6/*REMOTE_HOST*/  ||  NcbiIsEmptyIPv6(&addr))
                    x_StringToAddr(&addr, host);
                break;
            }
        }
    }
    if (buf  &&  buf_size) {
        if (host  &&  (i = strlen(host)) < buf_size)
            memcpy(buf, host, ++i);
        else
            *buf = '\0';
    }
    if (probe[1].host  &&  *probe[1].host)
        free((void*) probe[1].host /*forwarded_for*/);
    return addr;
}


extern TNCBI_IPv6Addr NcbiGetCgiClientIPv6(ECgiClientIP       flag,
                                           const char* const* tracking_env)
{
    return NcbiGetCgiClientIPv6Ex(flag, 0, 0, tracking_env);
}


extern unsigned int NcbiGetCgiClientIPEx(ECgiClientIP       flag,
                                         char*              buf,
                                         size_t             buf_size,
                                         const char* const* tracking_env)
{
    TNCBI_IPv6Addr
        addr = NcbiGetCgiClientIPv6Ex(flag, buf, buf_size, tracking_env);
    return NcbiIPv6ToIPv4(&addr, 0);
}


extern unsigned int NcbiGetCgiClientIP(ECgiClientIP       flag,
                                       const char* const* tracking_env)
{
    return NcbiGetCgiClientIPEx(flag, 0, 0, tracking_env);
}


extern int/*bool*/ NcbiIsLocalCgiClient(const char* const* tracking_env)
{
    TNCBI_IPv6Addr addr;
    if (s_SearchTrackingEnv("HTTP_CAF_EXTERNAL", tracking_env))
        return 0/*false*/;
    if (s_SearchTrackingEnv("HTTP_NCBI_EXTERNAL", tracking_env))
        return 0/*false*/;
    addr = NcbiGetCgiClientIPv6(eCgiClientIP_TryAll, tracking_env);
    return NcbiIsLocalIPEx(&addr, 0);
}
