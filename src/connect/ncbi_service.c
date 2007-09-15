/*  $Id$
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

#define CONN_SERVICE_NAME  DEF_CONN_REG_SECTION "_" REG_CONN_SERVICE_NAME


static ESwitch s_Fast = eOff;


ESwitch SERV_DoFastOpens(ESwitch on)
{
    ESwitch retval = s_Fast;
    if (on != eDefault)
        s_Fast = on;
    return retval;
}


static char* s_ServiceName(const char* service, size_t depth)
{
    char*  s;
    char   buf[128];
    char   srv[128];
    size_t len;

    if (++depth > 8 || !service || !*service ||
        (len = strlen(service)) + sizeof(CONN_SERVICE_NAME) >= sizeof(buf)) {
        return 0/*failure*/;
    }
    s = (char*) memcpy(buf, service, len) + len;
    *s++ = '_';
    memcpy(s, CONN_SERVICE_NAME, sizeof(CONN_SERVICE_NAME));
    /* Looking for "service_CONN_SERVICE_NAME" in environment */
    if (!(s = getenv(strupr(buf)))) {
        /* Looking for "CONN_SERVICE_NAME" in registry's section [service] */
        buf[len++] = '\0';
        CORE_REG_GET(buf, buf + len, srv, sizeof(srv), 0);
        if (!*srv)
            return strdup(service);
    } else
        strncpy0(srv, s, sizeof(srv) - 1);
    return s_ServiceName(srv, depth);
}


char* SERV_ServiceName(const char* service)
{
    return s_ServiceName(service, 0);
}


static int/*bool*/ s_AddSkipInfo(SERV_ITER   iter,
                                 const char* name,
                                 SSERV_Info* info)
{
    size_t n;
    assert(name);
    for (n = 0; n < iter->n_skip; n++) {
        if (strcasecmp(name, SERV_NameOfInfo(iter->skip[n])) == 0  &&
            (SERV_EqualInfo(info, iter->skip[n])  ||
             (iter->skip[n]->type == fSERV_Firewall  &&
              iter->skip[n]->u.firewall.type == info->u.firewall.type))) {
            /* Replace older version */
            if (iter->last == iter->skip[n])
                iter->last = info;
            free(iter->skip[n]);
            iter->skip[n] = info;
            return 1;
        }
    }
    if (iter->n_skip == iter->a_skip) {
        SSERV_Info** temp;
        n = iter->a_skip + 10;
        temp = (SSERV_Info**)
            (iter->skip
             ? realloc(iter->skip, n * sizeof(*temp))
             : malloc (            n * sizeof(*temp)));
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
    char str[80];
    if (s_Fast)
        return 0;
    ConnNetInfo_GetValue(service, key, str, sizeof(str), 0);
    return *str  &&  (strcmp(str, "1") == 0  ||
                      strcasecmp(str, "true") == 0  ||
                      strcasecmp(str, "yes" ) == 0  ||
                      strcasecmp(str, "on"  ) == 0);
}


static SERV_ITER s_Open(const char*          service,
                        unsigned/*bool*/     ismask,
                        TSERV_Type           types,
                        unsigned int         preferred_host,
                        unsigned short       preferred_port,
                        double               preference,
                        const SConnNetInfo*  net_info,
                        const SSERV_InfoCPtr skip[],
                        size_t               n_skip,
                        unsigned/*bool*/     external,
                        const char*          arg,
                        const char*          val,
                        SSERV_Info**         info,
                        HOST_INFO*           host_info)
{
    int/*bool*/ do_lbsmd = -1/*unassigned*/, do_dispd = -1/*unassigned*/;
    const SSERV_VTable* op;
    SERV_ITER iter;
    const char* s;
    
    if (!service || !*service)
        return 0;
    if (!(s = ismask || s_Fast ? strdup(service) : s_ServiceName(service, 0)))
        return 0;
    if (!*s || !(iter = (SERV_ITER) calloc(1, sizeof(*iter)))) {
        free((void*) s);
        return 0;
    }

    iter->name              = s;
    iter->type              = types & fSERV_All;
    iter->host              = (preferred_host == SERV_LOCALHOST
                               ? SOCK_GetLocalHostAddress(eDefault)
                               : preferred_host);
    iter->port              = preferred_port;
    iter->pref              = (preference < 0.0
                               ? -1.0
                               :  0.01 * (preference > 100.0
                                          ? 100.0
                                          : preference));
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
    if (arg) {
        iter->arg           = arg;
        iter->arglen        = strlen(arg);
    }
    if (iter->arglen  &&  val) {
        iter->val           = val;
        iter->vallen        = strlen(val);
    }
    iter->time              = (TNCBI_Time) time(0);

    if (n_skip) {
        size_t i;
        for (i = 0; i < n_skip; i++) {
            const char* name = (iter->ismask  ||  skip[i]->type == fSERV_Dns
                                ? SERV_NameOfInfo(skip[i])
                                : iter->reverse_dns ? s : "");
            SSERV_Info* temp = SERV_CopyInfoEx(skip[i], name);
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
            iter->type |= fSERV_Firewall;
        if (net_info->stateless)
            iter->stateless = 1;
        if (net_info->lb_disable)
            do_lbsmd = 0/*false*/;
    } else
        do_dispd = 0/*false*/;
    /* Ugly optimization not to access the registry more than necessary */
    if ((!s_IsMapperConfigured(service, REG_CONN_LOCAL_ENABLE)  ||
         !(op = SERV_LOCAL_Open(iter, info, host_info)))  &&
        (!do_lbsmd  ||
         !(do_lbsmd= !s_IsMapperConfigured(service, REG_CONN_LBSMD_DISABLE)) ||
         !(op = SERV_LBSMD_Open(iter, info, host_info,
                                !do_dispd  ||
                                !(do_dispd = !s_IsMapperConfigured
                                  (service, REG_CONN_DISPD_DISABLE)))))  &&
        (!do_dispd  ||
         !(do_dispd= !s_IsMapperConfigured(service, REG_CONN_DISPD_DISABLE)) ||
         !(op = SERV_DISPD_Open(iter, net_info, info, host_info)))) {
        if (!do_lbsmd  &&  !do_dispd) {
            CORE_LOGF(eLOG_Warning,
                      ("[SERV]  No service mappers available for `%s'",
                       service));
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
    SERV_ITER iter = SERV_Open(service, fSERV_Any, 0, net_info);
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


SERV_ITER SERV_OpenEx(const char*          service,
                      TSERV_Type           types,
                      unsigned int         preferred_host,
                      const SConnNetInfo*  net_info,
                      const SSERV_InfoCPtr skip[],
                      size_t               n_skip)
{
    return s_Open(service, 0/*not mask*/, types,
                  preferred_host, 0/*preferred_port*/, 0.0/*preference*/,
                  net_info, skip, n_skip,
                  0/*not external*/, 0/*arg*/, 0/*val*/,
                  0/*info*/, 0/*host_info*/);
}


SERV_ITER SERV_OpenP(const char*          service,
                     TSERV_Type           types,
                     unsigned int         preferred_host,
                     unsigned short       preferred_port,
                     double               preference,
                     const SConnNetInfo*  net_info,
                     const SSERV_InfoCPtr skip[],
                     size_t               n_skip,
                     int/*bool*/          external,
                     const char*          arg,
                     const char*          val)
{
    return s_Open(service, strpbrk(service, "?*") != 0, types,
                  preferred_host, preferred_port, preference,
                  net_info, skip, n_skip,
                  external, arg, val,
                  0/*info*/, 0/*host_info*/);
}


static void s_SkipSkip(SERV_ITER iter)
{
    size_t n;
    if (!iter->n_skip)
        return;
    if (iter->time  &&  (iter->ismask  ||  (iter->type & fSERV_Promiscuous)))
        return;
    n = 0;
    while (n < iter->n_skip) {
        SSERV_Info* temp = iter->skip[n];
        if (temp->time != NCBI_TIME_INFINITE  &&
            (!iter->time  ||  temp->time < iter->time)) {
            if (n < --iter->n_skip) {
                memmove(iter->skip + n, iter->skip + n + 1,
                        sizeof(*iter->skip)*(iter->n_skip - n));
            }
            if (iter->last == temp)
                iter->last = 0;
            free(temp);
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
        while ((info = (*iter->op->GetNextInfo)(iter, host_info)) != 0) {
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


static SSERV_Info* s_GetInfo(const char*          service,
                             TSERV_Type           types,
                             unsigned int         preferred_host,
                             unsigned short       preferred_port,
                             double               preference,
                             const SConnNetInfo*  net_info,
                             const SSERV_InfoCPtr skip[],
                             size_t               n_skip,
                             int /*bool*/         external,
                             const char*          arg,
                             const char*          val,
                             HOST_INFO*           host_info)
{
    SSERV_Info* info = 0;
    SERV_ITER iter = s_Open(service, 0/*not mask*/, types,
                            preferred_host, preferred_port, preference,
                            net_info, skip, n_skip,
                            external, arg, val,
                            &info, host_info);
    if (iter  &&  iter->op  &&  !info) {
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


SSERV_Info* SERV_GetInfoEx(const char*          service,
                           TSERV_Type           types,
                           unsigned int         preferred_host,
                           const SConnNetInfo*  net_info,
                           const SSERV_InfoCPtr skip[],
                           size_t               n_skip,
                           HOST_INFO*           host_info)
{
    return s_GetInfo(service, types,
                     preferred_host, 0/*preferred_host*/, 0.0/*preference*/,
                     net_info, skip, n_skip,
                     0/*not external*/, 0/*arg*/, 0/*val*/,
                     host_info);
}


SSERV_Info* SERV_GetInfoP(const char*          service,
                          TSERV_Type           types,
                          unsigned int         preferred_host,
                          unsigned short       preferred_port,
                          double               preference,
                          const SConnNetInfo*  net_info,
                          const SSERV_InfoCPtr skip[],
                          size_t               n_skip,
                          int/*bool*/          external, 
                          const char*          arg,
                          const char*          val,
                          HOST_INFO*           host_info)
{
    return s_GetInfo(service, types,
                     preferred_host, preferred_port, preference,
                     net_info, skip, n_skip,
                     external, arg, val,
                     host_info);
}


const SSERV_Info* SERV_GetNextInfoEx(SERV_ITER  iter,
                                     HOST_INFO* host_info)
{
    return iter  &&  iter->op ? s_GetNextInfo(iter, host_info, 0) : 0;
}


const SSERV_Info* SERV_GetNextInfo(SERV_ITER iter)
{
    return iter  &&  iter->op ? s_GetNextInfo(iter, 0, 0) : 0;
}


const char* SERV_MapperName(SERV_ITER iter)
{
    return iter  &&  iter->op ? iter->op->name : 0;
}


const char* SERV_CurrentName(SERV_ITER iter)
{
    const char* name = SERV_NameOfInfo(iter->last);
    return name  &&  *name ? name : iter->name;
}


int/*bool*/ SERV_Penalize(SERV_ITER iter, double fine)
{
    if (!iter  ||  !iter->op  ||  !iter->op->Penalize  ||  !iter->last)
        return 0/*false*/;
    return (*iter->op->Penalize)(iter, fine);
}


void SERV_Reset(SERV_ITER iter)
{
    if (!iter)
        return;
    iter->last  = 0;
    iter->time  = 0;
    s_SkipSkip(iter);
    if (iter->op && iter->op->Reset)
        (*iter->op->Reset)(iter);
}


void SERV_Close(SERV_ITER iter)
{
    size_t i;
    if (!iter)
        return;
    SERV_Reset(iter);
    for (i = 0; i < iter->n_skip; i++)
        free(iter->skip[i]);
    iter->n_skip = 0;
    if (iter->op && iter->op->Close)
        (*iter->op->Close)(iter);
    if (iter->skip)
        free(iter->skip);
    if (iter->name)
        free((void*) iter->name);
    free(iter);
}


int/*bool*/ SERV_Update(SERV_ITER iter, const char* text, int code)
{
    static const char used_server_info[] = "Used-Server-Info-";
    int retval = 0/*not updated yet*/;

    if (iter && iter->op && text) {
        const char *c, *b;
        iter->time = (TNCBI_Time) time(0);
        for (b = text; (c = strchr(b, '\n')) != 0; b = c + 1) {
            size_t len = (size_t)(c - b);
            SSERV_Info* info;
            unsigned int d1;
            char* p, *t;
            int d2;

            if (!(t = (char*) malloc(len + 1)))
                continue;
            memcpy(t, b, len);
            if (t[len - 1] == '\r')
                t[len - 1] = '\0';
            else
                t[len] = '\0';
            p = t;
            if (iter->op->Update  &&  (*iter->op->Update)(iter, p, code))
                retval = 1/*updated*/;
            if (strncasecmp(p, used_server_info,
                            sizeof(used_server_info) - 1) == 0) {
                p += sizeof(used_server_info) - 1;
                if (sscanf(p, "%u: %n", &d1, &d2) >= 1  &&
                    (info = SERV_ReadInfoEx(p + d2, "")) != 0) {
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


static void s_SetDefaultReferer(SERV_ITER iter, SConnNetInfo* net_info)
{
    char* str, *referer = 0;

    if (strcasecmp(iter->op->name, "DISPD") == 0) {
        const char* host = net_info->host;
        const char* path = net_info->path;
        const char* args = net_info->args;
        char        port[8];

        if (net_info->port && net_info->port != DEF_CONN_PORT)
            sprintf(port, ":%hu", net_info->port);
        else
            *port = '\0';
        if (!(referer = (char*) malloc(7 + 1 +1 + strlen(host) + strlen(port)
                                       + strlen(path) + strlen(args)))) {
            return;
        }
        strcat(strcat(strcpy(strcpy(referer, "http://")+7, host), port), path);
        if (args[0])
            strcat(strcat(referer, "?"), args);
    } else if ((str = strdup(iter->op->name)) != 0) {
        const char* host = net_info->client_host;
        const char* args = net_info->args;
        const char* name = iter->name;

        if (!*net_info->client_host  &&
            !SOCK_gethostbyaddr(0, net_info->client_host,
                                sizeof(net_info->client_host))) {
            SOCK_gethostname(net_info->client_host,
                             sizeof(net_info->client_host));
        }
        if (!(referer = (char*) malloc(3 + 1 + 9 + 1 +1 + 2*strlen(strlwr(str))
                                       + strlen(host) + strlen(name)
                                       + strlen(args)))) {
            return;
        }
        strcat(strcat(strcat(strcpy(referer, str), "://"), host), "/");
        strcat(strcat(strcat(referer, str), "?service="), name);
        if (args[0])
            strcat(strcat(referer, "&"), args);
        free(str);
    }
    assert(!net_info->http_referer);
    net_info->http_referer = referer;
}


char* SERV_Print(SERV_ITER iter, SConnNetInfo* net_info)
{
    static const char client_revision[] = "Client-Revision: %hu.%hu\r\n";
    static const char accepted_types[] = "Accepted-Server-Types:";
    static const char server_count[] = "Server-Count: ";
    char buffer[128], *str;
    size_t buflen, i;
    TSERV_Type t;
    BUF buf = 0;

    /* Put client version number */
    buflen = sprintf(buffer, client_revision,
                     SERV_CLIENT_REVISION_MAJOR, SERV_CLIENT_REVISION_MINOR);
    assert(buflen < sizeof(buffer));
    if (!BUF_Write(&buf, buffer, buflen)) {
        BUF_Destroy(buf);
        return 0;
    }
    if (iter) {
        if (net_info && !net_info->http_referer && iter->op && iter->op->name)
            s_SetDefaultReferer(iter, net_info);
        /* Form accepted server types */
        buflen = sizeof(accepted_types) - 1;
        memcpy(buffer, accepted_types, buflen);
        for (t = 1; t; t <<= 1) {
            if (iter->type & t) {
                const char* name = SERV_TypeStr((ESERV_Type) t);
                size_t namelen = strlen(name);
                if (!namelen || buflen + 1 + namelen + 2 >= sizeof(buffer))
                    break;
                buffer[buflen++] = ' ';
                strcpy(&buffer[buflen], name);
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
        /* How many server-infos for the dispatcher to send to us */
        if (iter->ismask  ||  (iter->pref  &&  iter->host)) {
            if (!BUF_Write(&buf, server_count, sizeof(server_count) - 1)  ||
                !BUF_Write(&buf,
                           iter->ismask ? "10" : "ALL",
                           iter->ismask ?   2  :    3)                    ||
                !BUF_Write(&buf, "\r\n", 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        /* Drop any outdated skip entries */
        iter->time = (TNCBI_Time) time(0);
        s_SkipSkip(iter);
        /* Put all the rest into rejection list */
        for (i = 0; i < iter->n_skip; i++) {
            /* NB: all skip infos are now kept with names (perhaps, empty) */
            const char* name = SERV_NameOfInfo(iter->skip[i]);
            if (!(str = SERV_WriteInfo(iter->skip[i])))
                break;
            buflen = sprintf(buffer, "Skip-Info-%u: ", (unsigned) i + 1); 
            assert(buflen < sizeof(buffer) - 1);
            if (!BUF_Write(&buf, buffer, buflen) ||
                (name  &&  !BUF_Write(&buf, name, strlen(name))) ||
                (name  &&  *name  &&  !BUF_Write(&buf, " ", 1)) ||
                !BUF_Write(&buf, str, strlen(str)) ||
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


/*
 * Note parameters' ranges here:
 * 0.0 <= pref <= 1.0
 * 0.0 <  gap  <= 1.0
 * n >= 2
 * Hence, the formula below always yields a value in the range [0.0 .. 1.0].
 */
double SERV_Preference(double pref, double gap, unsigned int n)
{
    double spread;
    assert(0.0 <= pref && pref <= 1.0);
    assert(0.0 <  gap  && gap  <= 1.0);
    assert(n >= 2);
    if (gap >= pref)
        return gap;
    spread = 14.0/(n + 12.0);
    if (gap >= spread*(1.0/(double) n))
        return pref;
    else
        return 2.0/spread*gap*pref;
}
