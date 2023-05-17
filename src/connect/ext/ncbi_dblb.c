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
 *   Get DB service name via LB service name
 *
 *   NOTE:  This is an internal NCBI-only API not for export to third parties.
 *
 *   NOTE:  Non-UNIX platforms may experience some lack of functionality.
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_assert.h"
#include "../ncbi_servicep.h"
#include <connect/ext/ncbi_dblb.h>
#include <ctype.h>
#include <stdlib.h>


const char* DBLB_GetServerName(const char* lb_name,
                               char*       server_name_buf,
                               size_t      server_name_buflen,
                               const char* const skip_servers[],
                               char*       errmsg_buf,
                               size_t      errmsg_buflen)
{
    EDBLB_Status result;
    const char* retval =
        DBLB_GetServer(lb_name, fDBLB_None, 0/*preference*/, skip_servers,
                       0/*CP*/, server_name_buf, server_name_buflen,
                       &result);
    if (errmsg_buf) {
        const char* errstr;
        assert(errmsg_buflen);
        errstr = DBLB_StatusStr(result);
        strncpy0(errmsg_buf,
                 errstr ? errstr : "Unknown error", errmsg_buflen - 1);
    }
    return retval;
}


const char* DBLB_StatusStr(EDBLB_Status status)
{
    static const char* s_StatusStr[] = {
        "",
        "Bad service name",
        "Service name not found in LB",
        "No matching LB DNS entry found",
        "Service currently down/unavailable"
    };
    size_t i = (size_t) status;
    return i > sizeof(s_StatusStr)/sizeof(s_StatusStr[0]) ? 0 : s_StatusStr[i];
}


static int/*bool*/ s_IsSkipHost(unsigned int host, unsigned int skip_host)
{
    return skip_host == host
        ||  (skip_host == SERV_LOCALHOST
             &&  host == SOCK_GetLocalHostAddress(eDefault));
}


static void s_AddSkip(SSERV_InfoCPtr** skip, size_t* a_skip, size_t* n_skip,
                      SSERV_Info* info)
{
    assert(info);
    if (*a_skip == *n_skip) {
        size_t count = *a_skip + 10;
        SSERV_InfoCPtr* temp = (SSERV_InfoCPtr*)
            (*skip
             ? realloc((void*)(*skip), count * sizeof(*temp))
             : malloc (                count * sizeof(*temp)));
        if (temp) {
            *skip   = temp;
            *a_skip = count;
        }
    }
    if (*a_skip != *n_skip)
        (*skip)[(*n_skip)++] = info;
    else
        free(info);
}


const char* DBLB_GetServer(const char*             lb_name,
                           TDBLB_Flags             flags,
                           const SDBLB_Preference* preference,
                           const char* const       skip_servers[],
                           SDBLB_ConnPoint*        conn_point,
                           char*                   server_name_buf,
                           size_t                  server_name_buflen,
                           EDBLB_Status*           result)
{
    static const char kPrefix[] = "DB_IP__";
    size_t          len, n, a_skip, n_skip;
    SConnNetInfo*   net_info;
    int/*bool*/     failed;
    unsigned int    x_host;
    unsigned short  x_port;
    double          x_pref;
    SSERV_InfoCPtr* skip;
    SSERV_Info*     info;
    SDBLB_ConnPoint cp;
    EDBLB_Status    x;
    const char*     c;

    if (!result)
        result = &x;
    if (server_name_buf) {
        assert(server_name_buflen);
        server_name_buf[0] = '\0';
    }
    if (!conn_point)
        conn_point = &cp;
    memset(conn_point, 0, sizeof(*conn_point));
    if (!lb_name  ||  !*lb_name) {
        *result = eDBLB_BadName;
        return 0/*failure*/;
    }
    *result = eDBLB_Success;

    if (strchr(lb_name, '.')) {
        cp.host = SOCK_gethostbyname(lb_name);
        if (cp.host == SOCK_GetLoopbackAddress())
            cp.host = /*FIXME?*/SERV_LOCALHOST;
    } else
        cp.host = 0;

    skip = 0;
    n_skip = 0;
    a_skip = 0;
    net_info = 0;
    failed = 0/*false*/;
    if (skip_servers) {
        for (n = 0;  !failed  &&  skip_servers[n];  n++) {
            const char* server = skip_servers[n];
            if (!(len = strlen(server))) {
                continue;
            }
            if (strncasecmp(server, kPrefix, sizeof(kPrefix)-1) == 0
                &&  isdigit((unsigned char) server[sizeof(kPrefix)-1])) {
                c = strstr(server + sizeof(kPrefix)-1, "__");
                if (c) {
                    size_t i = (size_t)(c - server) - (sizeof(kPrefix)-1);
                    char* temp = strdup(server + sizeof(kPrefix)-1);
                    if (temp) {
                        char* s = temp + i;
                        *s++ = ':';
                        memmove(s, s + 1, strlen(s + 1) + 1);
                        server = temp;
                        while (++temp < s) {
                            if (*temp == '_')
                                *temp =  '.';
                        }
                        len -= sizeof(kPrefix);
                    }
                }
            }
            if (SOCK_StringToHostPort(server, &x_host, &x_port)
                != server + len) {
                int/*bool*/ resolved = 0/*false*/;
                const SSERV_Info* temp;
                SERV_ITER iter;
                if (!net_info)
                    net_info = ConnNetInfo_Create(lb_name);
                iter = SERV_Open(skip_servers[n],
                                 fSERV_Standalone | fSERV_Dns
                                 | fSERV_Promiscuous,
                                 0, net_info);
                do {
                    SSERV_Info* dns;
                    temp = SERV_GetNextInfo(iter);
                    if (temp) {
                        x_host = temp->host;
                        if (x_host  &&  s_IsSkipHost(x_host, cp.host)) {
                            failed = 1/*true*/;
                            break;
                        }
                        x_port = temp->port;
                    } else if (!resolved) {
                        x_host = 0;
                        x_port = 0;
                    } else
                        break;
                    if ((dns = SERV_CreateDnsInfo(x_host)) != 0) {
                        dns->port = x_port;
                        s_AddSkip(&skip, &a_skip, &n_skip, x_host
                                  ? dns
                                  : SERV_CopyInfoEx(dns, skip_servers[n]));
                        if (!x_host)
                            free(dns);
                    }
                    resolved = 1/*true*/;
                } while (temp);
                SERV_Close(iter);
                info = 0;
            } else if (s_IsSkipHost(x_host, cp.host)) {
                failed = 1/*true*/;
                info = 0;
            } else if (server != skip_servers[n]) {
                info = SERV_CreateStandaloneInfo(x_host, x_port);
            } else if ((info = SERV_CreateDnsInfo(x_host)) != 0)
                info->port = x_port;
            if (server != skip_servers[n])
                free((void*) server);
            if (info)
                s_AddSkip(&skip, &a_skip, &n_skip, info);
        }
    }

    if (!failed  &&  !cp.host) {
        if (preference) {
            x_host = preference->host;
            x_port = preference->port;
            if ((x_pref = preference->pref) < 0.0)
                x_pref  =  0.0;
            else if (x_pref >= 100.0)
                x_pref  = -1.0;
        } else {
            x_host = 0;
            x_port = 0;
            x_pref = 0.0;
        }

        if (!net_info)
            net_info = ConnNetInfo_Create(lb_name);
        info = SERV_GetInfoP(lb_name, fSERV_ReverseDns | fSERV_Standalone,
                             x_host, x_port, x_pref, net_info,
                             skip, n_skip, 0/*not external*/,
                             0, 0, 0);
        if (!info  &&  (flags & fDBLB_AllowFallbackToStandby)) {
            /*FIXME: eliminate second pass by fix in ordering in ncbi_lbsmd.c*/
            info = SERV_GetInfoP(lb_name, fSERV_ReverseDns | fSERV_Standalone
                                 | fSERV_IncludeInactive,
                                 x_host, x_port, x_pref, net_info,
                                 skip, n_skip, 0/*not external*/,
                                 0, 0, 0);
        }
    } else
        info = 0;

    if (!info) {
        if (!failed) {
            if (!cp.host) {
                if (n_skip  &&  (x_host = SOCK_gethostbyname(lb_name)) != 0) {
                    for (n = 0;  n < n_skip;  n++) {
                        if (x_host == skip[n]->host) {
                            failed = 1/*true*/;
                            break;
                        }
                    }
                }
                if (!failed  &&  skip_servers) {
                    for (n = 0;  (c = skip_servers[n]) != 0;  n++) {
                        if (strcasecmp(c, lb_name) == 0) {
                            failed = 1/*true*/;
                            break;
                        }
                    }
                }
            } else if (conn_point != &cp) {
                conn_point->host = cp.host;
                conn_point->time = NCBI_TIME_INFINITE;
            }
        }
        if (!failed  &&  server_name_buf)
            strncpy0(server_name_buf, lb_name, server_name_buflen - 1);
        *result = eDBLB_NotFound;
    } else {
        if (info->type != fSERV_Dns) {
            char* s, buf[80];
            strncpy0(buf, kPrefix, sizeof(buf) - 1);
            SOCK_HostPortToString(info->host, info->port,
                                  buf + sizeof(kPrefix) - 1,
                                  sizeof(buf) - sizeof(kPrefix));
            len = strlen(buf);
            if ((s = strchr(buf, ':')) != 0)
                memmove(s + 1, s, strlen(s) + 1);
            for (n = 0;  n < len;  n++) {
                if (buf[n] == '.'  ||  buf[n] == ':')
                    buf[n] = '_';
            }
            if (server_name_buf)
                strncpy0(server_name_buf, buf, server_name_buflen - 1);
            *result = eDBLB_NoDNSEntry;
        } else if (info->host) {
            c = SERV_NameOfInfo(info);
            assert(c);
            if (server_name_buf)
                strncpy0(server_name_buf, c, server_name_buflen - 1);
        } else {
            failed = 1/*true*/;
            *result = eDBLB_ServiceDown;
        }
        if (!failed) {
            conn_point->host = info->host;
            conn_point->port = info->port;
            conn_point->time = info->time;
        }
        free(info);
    }

    for (n = 0;  n < n_skip;  n++)
        free((void*) skip[n]);
    if (skip)
        free((void*) skip);

    if (net_info)
        ConnNetInfo_Destroy(net_info);

    return failed ? 0 : (server_name_buf ? server_name_buf : lb_name);
}
