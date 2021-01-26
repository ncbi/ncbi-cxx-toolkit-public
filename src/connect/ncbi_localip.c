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
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_file_connector.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_iprange.h>
#include <connect/ncbi_localip.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <sys/param.h>
#elif defined(NCBI_OS_MSWIN)
#  include <io.h>
#  ifndef   R_OK
#    define R_OK  2
#  endif /*!R_OK*/
#endif /*NCBI_OS_...*/

#define NCBI_USE_ERRCODE_X   Connect_LocalIP

#if defined(NCBI_OS_UNIX)
#  define NcbiSys_access    access
#elif defined(NCBI_OS_MSWIN)
#  define NcbiSys_access   _access
#endif /*NCBI_OS_...*/

#ifndef   INADDR_LOOPBACK
#  define INADDR_LOOPBACK  0x7F000001
#endif /*!INADDR_LOOPBACK*/

#if defined(NCBI_OS_MSWIN)  &&  !defined(PATH_MAX)
#  ifdef _MAX_PATH
#    define PATH_MAX  _MAX_PATH
#else
#    define PATH_MAX  1024
#  endif /*_MAX_PATH*/
#endif /*NCBI_OS_MSWIN && !PATH_MAX*/

#if PATH_MAX < 256
#  define BUFSIZE  256
#else
#  define BUFSIZE  PATH_MAX
#endif /*PATHMAX<256*/

#ifdef    SizeOf
#  undef  SizeOf
#endif  /*SizeOf*/
#define   SizeOf(a)  (sizeof(a) / sizeof((a)[0]))

#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  define NCBI_LOCALIP_DEBUG  1
#endif /*_DEBUG && !NDEBUG*/


static int/*bool*/ s_Inited = 0/*false*/;


static SIPRange s_LocalIP[256 + 1] = { { eIPRange_None } };


#ifdef HAVE_LIBCONNEXT


#  include "ext/ncbi_localip.c"


static const SIPRange* x_IsOverlappingRange(const SIPRange* start,
                                            const SIPRange* range,
                                            size_t n)
{
    size_t i;
    assert(n < SizeOf(s_LocalIP));
    for (i = start ? start - s_LocalIP : 0;  i < n;  ++i) {
        assert(s_LocalIP[i].type);
        if (s_LocalIP[i].type != eIPRange_Application
            &&  (NcbiIsInIPRange(&s_LocalIP[i], &range->a)  ||
                 NcbiIsInIPRange(range, &s_LocalIP[i].a))) {
            return &s_LocalIP[i];
        }
    }
    return 0/*no overlap*/;
}


static int/*bool*/ xx_LoadLocalIPs(CONN conn, const char* source)
{
    unsigned int domain;
    EIO_Status status;
    char buf[128+1];
    size_t n, len;
    int lineno;

    /* localnet */
    NcbiIPv4ToIPv6(&s_LocalIP[0].a, SOCK_HostToNetLong(kLocalIP[0].a), 0);
    s_LocalIP[0].b    = SOCK_HostToNetLong(kLocalIP[0].b);
    s_LocalIP[0].type = kLocalIP[0].t;
    /* IPv6 localhost, ::1 */
    memset(&s_LocalIP[1], 0, sizeof(s_LocalIP[1]));
    s_LocalIP[1].a.octet[sizeof(s_LocalIP[1].a.octet) - 1] = 1;
    s_LocalIP[1].type = eIPRange_Host;

    domain = 0;
    lineno = 0;
    n = 2/*localnet(s) always added*/;
    while ((status = CONN_ReadLine(conn, buf, sizeof(buf) - 1, &len))
           == eIO_Success) {
        SIPRange local;
        char* c, *err;

        ++lineno;
        if (!len)
            continue;
        if (buf[len - 1] == '\r')
            --len;
        buf[len] = '\0';
        if (!(len = strcspn(buf, "!#")))
            continue;
        if (!*(c = buf + strspn(buf, " \t")))
            continue;
        len = strcspn(c, " \t");
        err = c + len;
        if (*err  &&  !*(err += strspn(err, " \t")))
            c[len] = '\0';
        if (!*err  &&  len  &&  *c == '['  &&  c[--len] == ']') {
            c[len] = '\0';
            if (len > sizeof(local.a.octet) - 1)
                len = sizeof(local.a.octet) - 1;
            strncpy0((char*) local.a.octet, ++c, len);
            for (len = 2;  len < n;  ++len) {
                if (s_LocalIP[len].type == eIPRange_Application
                    &&  strcasecmp((const char*) s_LocalIP[len].a.octet,
                                   (const char*) local.a.octet) == 0) {
                    CORE_LOGF_X(2, eLOG_Error,
                                ("%s:%u: Ignoring duplicate domain '%s'",
                                 source, lineno, c));
                    break;
                }
            }
            if (len < n)
                continue;
            local.b = ++domain;
            local.type = eIPRange_Application;
        } else {
            const SIPRange* over;
            if (*err  ||  !NcbiParseIPRange(&local, c)
                ||  ((local.type == eIPRange_Host  ||
                      local.type == eIPRange_Range)
                     &&  NcbiIsEmptyIPv6(&local.a))) {
                if (!*err)
                    err = c;
                CORE_LOGF_X(3, eLOG_Error,
                            ("%s:%u: Ignoring invalid local IP spec '%s'",
                             source, lineno, err));
                continue;
            }
            for (over = 0;
                 (over = x_IsOverlappingRange(over, &local, n)) != 0;
                 ++over) {
                char buf[150];
                const char* s
                    = strchr(NcbiDumpIPRange(over, buf, sizeof(buf)), ' ');
                if (!s++)
                    s = buf;
                CORE_LOGF_X(4, eLOG_Warning,
                            ("%s:%u: Local IP spec '%s' overlaps with an"
                             " already defined one: %s", source, lineno, c,s));
            }
        }
        assert(local.type != eIPRange_None);
        if (n >= SizeOf(s_LocalIP)) {
            CORE_LOGF_X(5, eLOG_Error,
                        ("%s:%u: Too many local IP specs, max %u allowed",
                         source, lineno, (unsigned int)(n - 2/*localnet*/)));
            break;
        }
        s_LocalIP[n++] = local;
    }

    if (status == eIO_Closed  &&  n > 2) {
        if (n < SizeOf(s_LocalIP))
            s_LocalIP[n].type = eIPRange_None;
        n -= 2;  /* compensate for auto-added localnet(s) */
        CORE_LOGF(eLOG_Trace, ("%s: Done loading local IP specs, %u line%s,"
                               " %u entr%s (%u domain%s)", source,
                               lineno, &"s"[lineno == 1],
                               (unsigned int) n, n == 1 ? "y" : "ies",
                               domain, &"s"[domain == 1]));
        return 1/*true*/;
    }
    return 0/*false*/;
}


static int/*bool*/ x_LoadLocalIPs(CONNECTOR c, const char* source)
{
    CONN conn;
    int/*bool*/ loaded = 0/*false*/;
    CORE_LOGF(eLOG_Trace,
              ("Loading local IP specs from \"%s\"", source));
    if (c  &&  CONN_Create(c, &conn) == eIO_Success) {
        loaded = xx_LoadLocalIPs(conn, source);
        CONN_Close(conn);
    } else if (c  &&  c->destroy)
        c->destroy(c);
    return loaded;
}


static void s_LoadLocalIPs(void)
{
    static const char* kFile[] = {
        "/etc/ncbi/local_ips_v2",  /* obsolescent */
        "/etc/ncbi/local_ips"
    };
    size_t n;
    ELOG_Level level;
    char buf[PATH_MAX + 1];
    const char* file = ConnNetInfo_GetValueInternal(0, REG_CONN_LOCAL_IPS,
                                                    buf, sizeof(buf) - 1, "");
    if (file) {
        SConnNetInfo* net_info;
        for (n = 0;  n < SizeOf(kFile);  ++n) {
            if (n  ||  !*file)
                file = kFile[n];
            else if (strcasecmp(file, DEF_CONN_LOCAL_IPS_DISABLE) == 0)
                break;
            errno = 0;
            if (NcbiSys_access(file, R_OK) == 0  &&
                x_LoadLocalIPs(FILE_CreateConnector(file, 0), file)) {
                return;
            }
            if (errno == ENOENT  &&  file != buf) {
#  ifdef NCBI_OS_LINUX
                level = n ? eLOG_Warning : eLOG_Trace;
#  else
                level = eLOG_Trace;
#  endif /*NCBI_OS_LINUX*/
            } else
                level = eLOG_Error;
            CORE_LOGF_ERRNO_X(1, level, errno,
                              ("Cannot load local IP specs from \"%s\"",file));
            if (file == buf)
                break;
        }
        net_info = ConnNetInfo_Create(DEF_CONN_LOCAL_IPS);
        if (net_info)
            n = strcasecmp(net_info->svc, DEF_CONN_LOCAL_IPS_DISABLE) ? 1 : 0;
        else
            n = 0;
        /* Build a pass-thru HTTP connector here to save on a dispatcher hit */
        if (n  &&  ConnNetInfo_SetupStandardArgs(net_info, net_info->svc)  &&
            x_LoadLocalIPs(HTTP_CreateConnector(net_info,
                                                "User-Agent: ncbi_localip",
                                                fHTTP_NoAutoRetry |
                                                fHTTP_SuppressMessages),
                           net_info->svc)) {
            ConnNetInfo_Destroy(net_info);
            return;
        }
        ConnNetInfo_Destroy(net_info);
        if (net_info) {
#  ifdef NCBI_OS_LINUX
            level = n ? eLOG_Trace   : eLOG_Note;
#  else
            level = n ? eLOG_Warning : eLOG_Note;
#  endif /*NCBI_OS_LINUX*/
        } else
            level = eLOG_Error;
    } else
        level = eLOG_Critical;
    if (level != eLOG_Note) {
        CORE_LOG_X(1, level,
                   "Cannot load local IP specs from " DEF_CONN_LOCAL_IPS);
    }

    CORE_LOG(eLOG_Warning, "Using default local IPv4 specs");
    assert(SizeOf(s_LocalIP) > SizeOf(kLocalIP));
    for (n = 0;  n < SizeOf(kLocalIP);  ++n) {
        s_LocalIP[n].type = kLocalIP[n].t;
        NcbiIPv4ToIPv6(&s_LocalIP[n].a,
                       SOCK_HostToNetLong(kLocalIP[n].a), 0);
        s_LocalIP[n].b    = SOCK_HostToNetLong(kLocalIP[n].b);
    }
    s_LocalIP[n].type = eIPRange_None;
}


#else


#  define s_LoadLocalIPs()  /*void*/


#endif /*HAVE_LIBCONNEXT*/


extern void NcbiInitLocalIP(void)
{
    /*CORE_LOCK_WRITE;*/
    s_Inited = 0/*F*/;
    /*CORE_UNLOCK;*/
}


extern int/*bool*/ NcbiIsLocalIPEx(const TNCBI_IPv6Addr* addr,
                                   SNcbiDomainInfo*      info)
{
    size_t n;

    if (!s_Inited) {
        CORE_LOCK_WRITE;
        if (!s_Inited) {
            s_LoadLocalIPs();
            s_Inited = 1;
            CORE_UNLOCK;
#ifdef NCBI_LOCALIP_DEBUG
            for (n = 0;  n < SizeOf(s_LocalIP);  ++n) {
                char buf[150];
                const char* result;
                if (s_LocalIP[n].type == eIPRange_Application) {
                    assert(s_LocalIP[n].b);
                    sprintf(buf, "Domain %u: %s",
                            s_LocalIP[n].b, s_LocalIP[n].a.octet);
                    assert(strlen(buf) < sizeof(buf));
                    result = buf;
                } else
                    result = NcbiDumpIPRange(s_LocalIP + n, buf, sizeof(buf));
                if (result)
                    CORE_LOG_X(1, eLOG_Trace, result);
                if (s_LocalIP[n].type == eIPRange_None)
                    break;
            }
#endif /*NCBI_LOCALIP_DEBUG*/
        } else
            CORE_UNLOCK;
    }

    if (!NcbiIsEmptyIPv6(addr)) {
        SNcbiDomainInfo x_info;
        memset(&x_info, 0, sizeof(x_info));
        for (n = 0;  n < SizeOf(s_LocalIP);  ++n) {
            if (s_LocalIP[n].type == eIPRange_None)
                break;
            if (s_LocalIP[n].type == eIPRange_Application) {
                x_info.sfx = (const char*) s_LocalIP[n].a.octet;
                x_info.num = s_LocalIP[n].b;
                continue;
            }
            if (NcbiIsInIPRange(s_LocalIP + n, addr)) {
                if ( info )
                    *info = x_info;
                return 1/*true*/;
            }
        }
    }

    if (info)
        memset(info, 0, sizeof(*info));
    return 0/*false*/;
}


extern int/*bool*/ NcbiIsLocalIP(unsigned int ip)
{
    if (ip) {
        unsigned int addr = SOCK_NetToHostLong(ip);
        if (
#ifdef IN_BADCLASS
            !IN_BADCLASS(addr)
#else
            (addr & 0xF0000000) ^ 0xF0000000
#endif /*IN_BADCLASS*/
            ) {
            TNCBI_IPv6Addr temp;
            NcbiIPv4ToIPv6(&temp, ip, 0);
            return NcbiIsLocalIPEx(&temp, 0);
        }
    }
    return 0/*false*/;
}
