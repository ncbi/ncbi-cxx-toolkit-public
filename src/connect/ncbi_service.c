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
 *   Top-level API to resolve NCBI service name to the server meta-address.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_dispd.h"
#include "ncbi_lbsmd.h"
#include "ncbi_local.h"
#include "ncbi_priv.h"
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#define NCBI_USE_ERRCODE_X   Connect_Service


/*
 * FIXME FIXME FIXME FIXME FIXME ---->
 *
 * NOTE: For fSERV_ReverseDns lookups the following rules apply to "skip"
 *       contents:  a service would not be selected if there is a same-named
 *       entry with matching host[:port] is found in the "skip" list, or
 *       there is a nameless...
 * NOTE:  Lookup by mask cancels fSERV_ReverseDns mode, if both are used.
 */


#define CONN_SERVICE_NAME  DEF_CONN_REG_SECTION "_" REG_CONN_SERVICE_NAME


#define SizeOf(arr)  (sizeof(arr) / sizeof((arr)[0]))


static ESwitch s_Fast = eOff;


static TNCBI_BigCount s_FWPorts[1024 / sizeof(TNCBI_BigCount)] = { 0 };


ESwitch SERV_DoFastOpens(ESwitch on)
{
    ESwitch retval = s_Fast;
    if (on != eDefault)
        s_Fast = on;
    return retval;
}


void SERV_InitFirewallMode(void)
{
    memset(s_FWPorts, 0, sizeof(s_FWPorts));
}


int/*bool*/ SERV_AddFirewallPort(unsigned short port)
{
    unsigned int n = port / (sizeof(s_FWPorts[0]) << 3);
    unsigned int m = port % (sizeof(s_FWPorts[0]) << 3);
    if ((size_t) n < SizeOf(s_FWPorts)) {
        s_FWPorts[n] |= (TNCBI_BigCount) 1 << m;
        return 1/*true*/;
    }
    return 0/*false*/;
}


int/*bool*/ SERV_IsFirewallPort(unsigned short port)
{
    unsigned int n = port / (sizeof(s_FWPorts[0]) << 3);
    unsigned int m = port % (sizeof(s_FWPorts[0]) << 3);
    if ((size_t) n < SizeOf(s_FWPorts)
        &&  (s_FWPorts[n] & ((TNCBI_BigCount) 1 << m))) {
        return 1/*true*/;
    }
    return 0/*false*/;
}


static char* s_ServiceName(const char* service,
                           int/*bool*/ ismask, unsigned int depth)
{
    char   buf[128];
    char   srv[128];
    size_t len;
    char*  s;

    if (depth > 7) {
        assert(service  &&  *service);
        CORE_LOGF_X(7, eLOG_Error,
                    ("[%s]  Maximal service name recursion depth reached: %u",
                     service, depth));
        return 0/*failure*/;
    }
    len = 0;
    assert(sizeof(buf) > sizeof(CONN_SERVICE_NAME));
    if (!service  ||  (!ismask  &&  (!*service  ||  strpbrk(service, "?*")))
        ||  (len = strlen(service)) >= sizeof(buf)-sizeof(CONN_SERVICE_NAME)) {
        CORE_LOGF_X(8, eLOG_Error,
                    ("%s%s%s%s service name",
                     !service  ||  !*service ? "" : "[",
                     !service ? "" : service,
                     !service  ||  !*service ? "" : "]  ",
                     !service ? "NULL" : !*service ? "Empty" :
                     len < sizeof(buf)-sizeof(CONN_SERVICE_NAME) ? "Invalid" :
                     "Too long"));
        return 0/*failure*/;
    }
    if (!s_Fast  &&  !ismask) {
        s = (char*) memcpy(buf, service, len) + len;
        *s++ = '_';
        memcpy(s, CONN_SERVICE_NAME, sizeof(CONN_SERVICE_NAME));
        /* Looking for "service_CONN_SERVICE_NAME" in the environment */
        if (!(s = getenv(strupr(buf)))  ||  !*s) {
            /* Looking for "CONN_SERVICE_NAME" in registry section [service] */
            buf[len++] = '\0';
            CORE_REG_GET(buf, buf + len, srv, sizeof(srv), 0);
            s = srv;
        }
        if (*s  &&  strcasecmp(s, service) != 0)
            return s_ServiceName(s, ismask, depth + 1);
    }
    return strdup(service);
}


char* SERV_ServiceName(const char* service)
{
    return s_ServiceName(service, 0, 0);
}


static int/*bool*/ s_AddSkipInfo(SERV_ITER      iter,
                                 const char*    name,
                                 SSERV_InfoCPtr info)
{
    size_t n;
    assert(name);
    for (n = 0;  n < iter->n_skip;  n++) {
        if (strcasecmp(name, SERV_NameOfInfo(iter->skip[n])) == 0
            &&  (SERV_EqualInfo(info, iter->skip[n])  ||
                 (iter->skip[n]->type == fSERV_Firewall  &&
                  iter->skip[n]->u.firewall.type == info->u.firewall.type))) {
            /* Replace older version */
            if (iter->last == iter->skip[n])
                iter->last  = info;
            free((void*) iter->skip[n]);
            iter->skip[n] = info;
            return 1;
        }
    }
    if (iter->n_skip == iter->a_skip) {
        SSERV_InfoCPtr* temp;
        n = iter->a_skip + 10;
        temp = (SSERV_InfoCPtr*)
            (iter->skip
             ? realloc((void*) iter->skip, n * sizeof(*temp))
             : malloc (                    n * sizeof(*temp)));
        if (!temp)
            return 0;
        iter->skip = temp;
        iter->a_skip = n;
    }
    iter->skip[iter->n_skip++] = info;
    return 1;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_IsMapperConfigured(const char* service, const char* key)
{
    char val[32];
    if (s_Fast)
        return 0;
    ConnNetInfo_GetValue(service, key, val, sizeof(val), 0);
    return ConnNetInfo_Boolean(val);
}


static SERV_ITER s_Open(const char*         service,
                        unsigned/*bool*/    ismask,
                        TSERV_Type          types,
                        unsigned int        preferred_host,
                        unsigned short      preferred_port,
                        double              preference,
                        const SConnNetInfo* net_info,
                        SSERV_InfoCPtr      skip[],
                        size_t              n_skip,
                        unsigned/*bool*/    external,
                        const char*         arg,
                        const char*         val,
                        SSERV_Info**        info,
                        HOST_INFO*          host_info)
{
    int/*bool*/ do_lbsmd = -1/*unassigned*/, do_dispd = -1/*unassigned*/;
    const SSERV_VTable* op;
    SERV_ITER iter;
    const char* s;

    if (!(s = s_ServiceName(service, ismask, 0)))
        return 0;
    if (!(iter = (SERV_ITER) calloc(1, sizeof(*iter)))) {
        free((void*) s);
        return 0;
    }
    assert(ismask  ||  *s);

    iter->name              = s;
    iter->host              = (preferred_host == SERV_LOCALHOST
                               ? SOCK_GetLocalHostAddress(eDefault)
                               : preferred_host);
    iter->port              = preferred_port;
    iter->pref              = (preference < 0.0
                               ? -1.0
                               :  0.01 * (preference > 100.0
                                          ? 100.0
                                          : preference));
    iter->types             = types;
    if (ismask)
        iter->ismask        = 1;
    if (types & fSERV_IncludeDown)
        iter->ok_down       = 1;
    if (types & fSERV_IncludeSuppressed)
        iter->ok_suppressed = 1;
    if (types & fSERV_ReverseDns)
        iter->reverse_dns   = 1;
    if (types & fSERV_Stateless)
        iter->stateless     = 1;
    iter->external          = external;
    if (arg  &&  *arg) {
        iter->arg           = arg;
        iter->arglen        = strlen(arg);
        if (val) {
            iter->val       = val;
            iter->vallen    = strlen(val);
        }
    }
    iter->time              = (TNCBI_Time) time(0);

    if (n_skip) {
        size_t i;
        for (i = 0;  i < n_skip;  i++) {
            const char* name = (iter->ismask  ||  skip[i]->type == fSERV_Dns
                                ? SERV_NameOfInfo(skip[i]) : "");
            SSERV_Info* temp = SERV_CopyInfoEx(skip[i],
                                               !iter->reverse_dns  ||  *name ?
                                               name : s);
            if (temp) {
                temp->time = NCBI_TIME_INFINITE;
                if (!s_AddSkipInfo(iter, name, temp)) {
                    free(temp);
                    temp = 0;
                }
            }
            if (!temp) {
                SERV_Close(iter);
                return 0;
            }
        }
    }
    assert(n_skip == iter->n_skip);

    if (net_info) {
        if (net_info->firewall)
            iter->types |= fSERV_Firewall;
        if (net_info->stateless)
            iter->stateless = 1;
        if (net_info->lb_disable)
            do_lbsmd = 0/*false*/;
    } else
        do_dispd = 0/*false*/;
    /* Ugly optimization not to access the registry more than necessary */
    if ((!s_IsMapperConfigured(service, REG_CONN_LOCAL_ENABLE)               ||
         !(op = SERV_LOCAL_Open(iter, info, host_info)))                 &&
        (!do_lbsmd                                                           ||
         !(do_lbsmd= !s_IsMapperConfigured(service, REG_CONN_LBSMD_DISABLE)) ||
         !(op = SERV_LBSMD_Open(iter, info, host_info,
                                !do_dispd                                    ||
                                !(do_dispd = !s_IsMapperConfigured
                                  (service, REG_CONN_DISPD_DISABLE)))))  &&
        (!do_dispd                                                           ||
         !(do_dispd= !s_IsMapperConfigured(service, REG_CONN_DISPD_DISABLE)) ||
         !(op = SERV_DISPD_Open(iter, net_info, info, host_info)))) {
        if (!do_lbsmd  &&  !do_dispd) {
            CORE_LOGF_X(1, eLOG_Error,
                        ("[%s]  No service mappers available", service));
        }
        SERV_Close(iter);
        return 0;
    }

    assert(op != 0);
    iter->op = op;
    return iter;
}


SERV_ITER SERV_OpenSimple(const char* service)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    SERV_ITER iter = SERV_Open(service, fSERV_Any, SERV_ANYHOST, net_info);
    ConnNetInfo_Destroy(net_info);
    return iter;
}


SERV_ITER SERV_Open(const char*         service,
                    TSERV_Type          types,
                    unsigned int        preferred_host,
                    const SConnNetInfo* net_info)
{
    return s_Open(service, 0/*not mask*/, types,
                  preferred_host, 0/*preferred_port*/, 0.0/*preference*/,
                  net_info, 0/*skip*/, 0/*n_skip*/,
                  0/*not external*/, 0/*arg*/, 0/*val*/,
                  0/*info*/, 0/*host_info*/);
}


SERV_ITER SERV_OpenEx(const char*         service,
                      TSERV_Type          types,
                      unsigned int        preferred_host,
                      const SConnNetInfo* net_info,
                      SSERV_InfoCPtr      skip[],
                      size_t              n_skip)
{
    return s_Open(service, 0/*not mask*/, types,
                  preferred_host, 0/*preferred_port*/, 0.0/*preference*/,
                  net_info, skip, n_skip,
                  0/*not external*/, 0/*arg*/, 0/*val*/,
                  0/*info*/, 0/*host_info*/);
}


SERV_ITER SERV_OpenP(const char*         service,
                     TSERV_Type          types,
                     unsigned int        preferred_host,
                     unsigned short      preferred_port,
                     double              preference,
                     const SConnNetInfo* net_info,
                     SSERV_InfoCPtr      skip[],
                     size_t              n_skip,
                     int/*bool*/         external,
                     const char*         arg,
                     const char*         val)
{
    return s_Open(service,
                  service  &&  (!*service  ||  strpbrk(service, "?*")), types,
                  preferred_host, preferred_port, preference,
                  net_info, skip, n_skip,
                  external, arg, val,
                  0/*info*/, 0/*host_info*/);
}


static void s_SkipSkip(SERV_ITER iter)
{
    size_t n;
    if (iter->time  &&  (iter->ismask | iter->ok_down | iter->ok_suppressed))
        return;
    n = 0;
    while (n < iter->n_skip) {
        SSERV_InfoCPtr temp = iter->skip[n];
        if (temp->time != NCBI_TIME_INFINITE
            &&  (!iter->time/*iterator reset*/
                 ||  ((temp->type != fSERV_Dns  ||  temp->host)
                      &&  temp->time < iter->time))) {
            if (--iter->n_skip > n) {
                SSERV_InfoCPtr* ptr = iter->skip + n;
                memmove((void*) ptr, (void*)(ptr + 1),
                        (iter->n_skip - n) * sizeof(*ptr));
            }
            if (iter->last == temp)
                iter->last  = 0;
            free((void*) temp);
        } else
            n++;
    }
}


static SSERV_Info* s_GetNextInfo(SERV_ITER   iter,
                                 HOST_INFO*  host_info,
                                 int/*bool*/ internal)
{
    SSERV_Info* info = 0;
    assert(iter  &&  iter->op);
    if (iter->op->GetNextInfo) {
        if (!internal) {
            iter->time = (TNCBI_Time) time(0);
            s_SkipSkip(iter);
        }
        /* Obtain a fresh entry from the actual mapper */
        while ((info = iter->op->GetNextInfo(iter, host_info)) != 0) {
            /* This should never actually be used for LBSMD dispatcher,
             * as all exclusion logic is already done in it internally. */
            int/*bool*/ go =
                !info->host  ||  iter->pref >= 0.0  ||
                !iter->host  ||  (iter->host == info->host  &&
                                  (!iter->port  ||  iter->port == info->port));
            if (go  &&  internal)
                break;
            if (!s_AddSkipInfo(iter, SERV_NameOfInfo(info), info)) {
                free(info);
                info = 0;
            }
            if (go  ||  !info)
                break;
        }
    }
    if (!internal)
        iter->last = info;
    return info;
}


static SSERV_Info* s_GetInfo(const char*         service,
                             TSERV_Type          types,
                             unsigned int        preferred_host,
                             unsigned short      preferred_port,
                             double              preference,
                             const SConnNetInfo* net_info,
                             SSERV_InfoCPtr      skip[],
                             size_t              n_skip,
                             int /*bool*/        external,
                             const char*         arg,
                             const char*         val,
                             HOST_INFO*          host_info)
{
    SSERV_Info* info = 0;
    SERV_ITER iter = s_Open(service, 0/*not mask*/, types,
                            preferred_host, preferred_port, preference,
                            net_info, skip, n_skip,
                            external, arg, val,
                            &info, host_info);
    assert(!iter  ||  iter->op);
    if (iter  &&  !info) {
        /* All LOCAL/DISPD searches end up here, but none LBSMD ones */
        info = s_GetNextInfo(iter, host_info, 1/*internal*/);
    }
    SERV_Close(iter);
    return info;
}


SSERV_Info* SERV_GetInfo(const char*         service,
                         TSERV_Type          types,
                         unsigned int        preferred_host,
                         const SConnNetInfo* net_info)
{
    return s_GetInfo(service, types,
                     preferred_host, 0/*preferred_port*/, 0.0/*preference*/,
                     net_info, 0/*skip*/, 0/*n_skip*/,
                     0/*not external*/, 0/*arg*/, 0/*val*/,
                     0/*host_info*/);
}


SSERV_Info* SERV_GetInfoEx(const char*         service,
                           TSERV_Type          types,
                           unsigned int        preferred_host,
                           const SConnNetInfo* net_info,
                           SSERV_InfoCPtr      skip[],
                           size_t              n_skip,
                           HOST_INFO*          host_info)
{
    return s_GetInfo(service, types,
                     preferred_host, 0/*preferred_host*/, 0.0/*preference*/,
                     net_info, skip, n_skip,
                     0/*not external*/, 0/*arg*/, 0/*val*/,
                     host_info);
}


SSERV_Info* SERV_GetInfoP(const char*         service,
                          TSERV_Type          types,
                          unsigned int        preferred_host,
                          unsigned short      preferred_port,
                          double              preference,
                          const SConnNetInfo* net_info,
                          SSERV_InfoCPtr      skip[],
                          size_t              n_skip,
                          int/*bool*/         external, 
                          const char*         arg,
                          const char*         val,
                          HOST_INFO*          host_info)
{
    return s_GetInfo(service, types,
                     preferred_host, preferred_port, preference,
                     net_info, skip, n_skip,
                     external, arg, val,
                     host_info);
}


SSERV_InfoCPtr SERV_GetNextInfoEx(SERV_ITER  iter,
                                  HOST_INFO* host_info)
{
    assert(!iter  ||  iter->op);
    return iter ? s_GetNextInfo(iter, host_info, 0) : 0;
}


SSERV_InfoCPtr SERV_GetNextInfo(SERV_ITER iter)
{
    assert(!iter  ||  iter->op);
    return iter ? s_GetNextInfo(iter, 0,         0) : 0;
}


const char* SERV_MapperName(SERV_ITER iter)
{
    assert(!iter  ||  iter->op);
    return iter ? iter->op->mapper : 0;
}


const char* SERV_CurrentName(SERV_ITER iter)
{
    const char* name = SERV_NameOfInfo(iter->last);
    return name  &&  *name ? name : iter->name;
}


int/*bool*/ SERV_PenalizeEx(SERV_ITER iter, double fine, TNCBI_Time time)
{
    assert(!iter  ||  iter->op);
    if (!iter  ||  !iter->op->Feedback  ||  !iter->last)
        return 0/*false*/;
    return iter->op->Feedback(iter, fine, time ? time : 1/*NB: always != 0*/);
}


int/*bool*/ SERV_Penalize(SERV_ITER iter, double fine)
{
    return SERV_PenalizeEx(iter, fine, 0);
}


int/*bool*/ SERV_Rerate(SERV_ITER iter, double rate)
{
    assert(!iter  ||  iter->op);
    if (!iter  ||  !iter->op->Feedback  ||  !iter->last)
        return 0/*false*/;
    return iter->op->Feedback(iter, rate, 0/*i.e.rate*/);
}


void SERV_Reset(SERV_ITER iter)
{
    if (!iter)
        return;
    iter->last  = 0;
    iter->time  = 0;
    s_SkipSkip(iter);
    if (iter->op  &&  iter->op->Reset)
        iter->op->Reset(iter);
}


void SERV_Close(SERV_ITER iter)
{
    size_t i;
    if (!iter)
        return;
    SERV_Reset(iter);
    for (i = 0;  i < iter->n_skip;  i++)
        free((void*) iter->skip[i]);
    iter->n_skip = 0;
    if (iter->op) {
        if (iter->op->Close)
            iter->op->Close(iter);
        iter->op = 0;
    }
    if (iter->skip)
        free((void*) iter->skip);
    free((void*) iter->name);
    free(iter);
}


int/*bool*/ SERV_Update(SERV_ITER iter, const char* text, int code)
{
    static const char used_server_info[] = "Used-Server-Info-";
    int retval = 0/*not updated yet*/;

    assert(!iter  ||  iter->op);
    if (iter  &&  text) {
        const char *c, *b;
        iter->time = (TNCBI_Time) time(0);
        for (b = text;  (c = strchr(b, '\n')) != 0;  b = c + 1) {
            size_t len = (size_t)(c - b);
            SSERV_Info* info;
            unsigned int d1;
            char* p, *t;
            int d2;

            if (!(t = (char*) malloc(len + 1)))
                continue;
            memcpy(t, b, len);
            if (t[len - 1] == '\r')
                t[len - 1]  = '\0';
            else
                t[len    ]  = '\0';
            p = t;
            if (iter->op->Update  &&  iter->op->Update(iter, p, code))
                retval = 1/*updated*/;
            if (!strncasecmp(p, used_server_info, sizeof(used_server_info) - 1)
                &&  isdigit((unsigned char) p[sizeof(used_server_info) - 1])) {
                p += sizeof(used_server_info) - 1;
                if (sscanf(p, "%u: %n", &d1, &d2) >= 1
                    &&  (info = SERV_ReadInfoEx(p + d2, "", 0)) != 0) {
                    if (!s_AddSkipInfo(iter, "", info))
                        free(info);
                    else
                        retval = 1/*updated*/;
                }
            }
            free(t);
        }
    }
    return retval;
}


static void s_PrintFirewallPorts(char* buf, size_t bufsize,
                                 const SConnNetInfo* net_info)
{
    EFWMode mode = net_info ? (EFWMode) net_info->firewall : eFWMode_Legacy;
    size_t  len, n;
    unsigned int m;

    assert(buf  &&  bufsize > 1);
    switch (mode) {
    case eFWMode_Legacy:
        *buf = '\0';
        return;
    case eFWMode_Firewall:
        memcpy(buf, "0", 2);
        return;
    default:
        break;
    }
    len = 0;
    for (n = m = 0; n < SizeOf(s_FWPorts); n++, m += sizeof(s_FWPorts[0])<<3) {
        unsigned short p;
        TNCBI_BigCount mask = s_FWPorts[n];
        for (p = m;  mask;  p++, mask >>= 1) {
            if (mask & 1) {
                char port[10];
                int  k = sprintf(port, " %hu" + !len, p);
                if (len + k < bufsize) {
                    memcpy(buf + len, port, k);
                    len += k;
                }
                if (!p)
                    break;
            }
        }
    }
    buf[len] = '\0';
}


static void s_SetDefaultReferer(SERV_ITER iter, SConnNetInfo* net_info)
{
    char* str, *referer = 0;

    if (strcasecmp(iter->op->mapper, "DISPD") == 0)
        referer = ConnNetInfo_URL(net_info);
    else if ((str = strdup(iter->op->mapper)) != 0) {
        const char* host = net_info->client_host;
        const char* args = net_info->args;
        const char* name = iter->name;

        if (!*net_info->client_host
            &&  !SOCK_gethostbyaddr(0, net_info->client_host,
                                    sizeof(net_info->client_host))) {
            SOCK_gethostname(net_info->client_host,
                             sizeof(net_info->client_host));
        }
        if (!(referer = (char*) malloc(3 + 1 + 1 + 1 + 2*strlen(strlwr(str)) +
                                       strlen(host) + (args[0]
                                                       ? strlen(args)
                                                       : 8 + strlen(name))))) {
            return;
        }
        strcat(strcat(strcat(strcat(strcpy
                                    (referer, str), "://"), host), "/"), str);
        if (args[0])
            strcat(strcat(referer, "?"),         args);
        else
            strcat(strcat(referer, "?service="), name);
        free(str);
    }
    assert(!net_info->http_referer);
    net_info->http_referer = referer;
}


char* SERV_Print(SERV_ITER iter, SConnNetInfo* net_info, int/*bool*/ but_last)
{
    static const char kClientRevision[] = "Client-Revision: %hu.%hu\r\n";
    static const char kAcceptedServerTypes[] = "Accepted-Server-Types:";
    static const char kUsedServerInfo[] = "Used-Server-Info: ";
    static const char kNcbiFWPorts[] = "NCBI-Firewall-Ports: ";
    static const char kServerCount[] = "Server-Count: ";
    static const char kPreference[] = "Preference: ";
    static const char kSkipInfo[] = "Skip-Info-%u: ";
    static const char kAffinity[] = "Affinity: ";
    char buffer[128], *str;
    TBSERV_TypeOnly t;
    size_t buflen, i;
    BUF buf = 0;

    /* Put client version number */
    buflen = sprintf(buffer, kClientRevision,
                     SERV_CLIENT_REVISION_MAJOR, SERV_CLIENT_REVISION_MINOR);
    assert(buflen < sizeof(buffer));
    if (!BUF_Write(&buf, buffer, buflen)) {
        BUF_Destroy(buf);
        return 0;
    }
    if (iter) {
        assert(iter->op);
        if (net_info  &&  !net_info->http_referer  &&  iter->op->mapper)
            s_SetDefaultReferer(iter, net_info);
        /* Accepted server types */
        buflen = sizeof(kAcceptedServerTypes) - 1;
        memcpy(buffer, kAcceptedServerTypes, buflen);
        for (t = 1;  t;  t <<= 1) {
            if (iter->types & t) {
                const char* name = SERV_TypeStr((ESERV_Type) t);
                size_t namelen = strlen(name);
                if (!namelen  ||  buflen + 1 + namelen + 2 >= sizeof(buffer))
                    break;
                buffer[buflen++] = ' ';
                memcpy(buffer + buflen, name, namelen);
                buflen += namelen;
            }
        }
        if (buffer[buflen - 1] != ':') {
            strcpy(&buffer[buflen], "\r\n");
            assert(strlen(buffer) == buflen+2  &&  buflen+2 < sizeof(buffer));
            if (!BUF_Write(&buf, buffer, buflen + 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (iter->ismask  ||  (iter->pref  &&  (iter->host | iter->port))) {
            /* FIXME: To obsolete? */
            /* How many server-infos for the dispatcher to send to us */
            if (!BUF_Write(&buf, kServerCount, sizeof(kServerCount) - 1)  ||
                !BUF_Write(&buf,
                           iter->ismask ? "10\r\n" : "ALL\r\n",
                           iter->ismask ?       4  :        5)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (iter->types & fSERV_Firewall) {
            /* Firewall */
            s_PrintFirewallPorts(buffer, sizeof(buffer), net_info);
            if (*buffer
                &&  (!BUF_Write(&buf, kNcbiFWPorts, sizeof(kNcbiFWPorts)-1)  ||
                     !BUF_Write(&buf, buffer, strlen(buffer))                ||
                     !BUF_Write(&buf, "\r\n", 2))) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (iter->pref  &&  (iter->host | iter->port)) {
            /* Preference */
            buflen = SOCK_HostPortToString(iter->host, iter->port,
                                           buffer, sizeof(buffer));
            buffer[buflen++] = ' ';
            buflen = (size_t)(strcpy(NCBI_simple_ftoa(buffer + buflen,
                                                      iter->pref * 100.0, 2),
                                     "\r\n") - buffer) + 2/*"\r\n"*/;
            if (!BUF_Write(&buf, kPreference, sizeof(kPreference) - 1)  ||
                !BUF_Write(&buf, buffer, buflen)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (iter->arglen) {
            /* Affinity */
            if (!BUF_Write(&buf, kAffinity, sizeof(kAffinity) - 1)           ||
                !BUF_Write(&buf, iter->arg, iter->arglen)                    ||
                (iter->val  &&  (!BUF_Write(&buf, "=", 1)                    ||
                                 !BUF_Write(&buf, iter->val, iter->vallen))) ||
                !BUF_Write(&buf, "\r\n", 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        /* Drop any outdated skip entries */
        iter->time = (TNCBI_Time) time(0);
        s_SkipSkip(iter);
        /* Put all the rest into rejection list */
        for (i = 0;  i < iter->n_skip;  i++) {
            /* NB: all skip infos are now kept with names (perhaps, empty) */
            const char* name    = SERV_NameOfInfo(iter->skip[i]);
            size_t      namelen = name  &&  *name ? strlen(name) : 0;
            if (!(str = SERV_WriteInfo(iter->skip[i])))
                break;
            if (but_last  &&  iter->last == iter->skip[i]) {
                buflen = sizeof(kUsedServerInfo) - 1;
                memcpy(buffer, kUsedServerInfo, buflen);
            } else
                buflen = sprintf(buffer, kSkipInfo, (unsigned) i + 1); 
            assert(buflen < sizeof(buffer) - 1);
            if (!BUF_Write(&buf, buffer, buflen)                ||
                (namelen  &&  !BUF_Write(&buf, name, namelen))  ||
                (namelen  &&  !BUF_Write(&buf, " ", 1))         ||
                !BUF_Write(&buf, str, strlen(str))              ||
                !BUF_Write(&buf, "\r\n", 2)) {
                free(str);
                break;
            }
            free(str);
        }
        if (i < iter->n_skip) {
            BUF_Destroy(buf);
            return 0;
        }
    }
    /* Ok then, we have filled the entire header, <CR><LF> terminated */
    if ((buflen = BUF_Size(buf)) != 0) {
        if ((str = (char*) malloc(buflen + 1)) != 0) {
            if (BUF_Read(buf, str, buflen) != buflen) {
                free(str);
                str = 0;
            } else
                str[buflen] = '\0';
        }
    } else
        str = 0;
    BUF_Destroy(buf);
    return str;
}


unsigned short SERV_ServerPort(const char*  name,
                               unsigned int host)
{
    SSERV_Info*    info;
    unsigned short port;

    /* FIXME:  SERV_LOCALHOST may not need to be resolved here,
     *         but taken from LBSMD table (or resolved later in DISPD/LOCAL
     *         if needed).
     */
    if (!host  ||  host == SERV_LOCALHOST)
        host = SOCK_GetLocalHostAddress(eDefault);
    if (!(info = s_GetInfo(name, fSERV_Standalone | fSERV_Promiscuous,
                           host, 0/*pref. port*/, -1.0/*latch host*/,
                           0/*net_info*/, 0/*skip*/, 0/*n_skip*/,
                           0/*not external*/, 0/*arg*/, 0/*val*/,
                           0/*host_info*/))) {
        return 0;
    }
    assert(info->host == host);
    port = info->port;
    free((void*) info);
    assert(port);
    return port;
}


#if 0
int/*bool*/ SERV_MatchesHost(const SSERV_Info* info, unsigned int host)
{
    if (host == SERV_ANYHOST)
        return 1/*true*/;
    if (host != SERV_LOCALHOST)
        return info->host == host ? 1/*true*/ : 0/*false*/;
    if (!info->host  ||  info->host == SOCK_GetLocalHostAddress(eDefault))
        return 1/*true*/;
    return 0/*false*/;
}
#endif
