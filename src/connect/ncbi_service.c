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


static int/*bool*/ s_IsMapperDisabled(const char* service, const char* key)
{
    char str[80];
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
    const TSERV_Type special_flags =
        fSERV_Promiscuous | fSERV_ReverseDns | fSERV_Stateless;
    int/*bool*/ no_lbsmd, no_dispd;
    const SSERV_VTable* op;
    SERV_ITER iter;
    const char* s;
    
    if (!service || !*service)
        return 0;
    if (!(s = ismask ? strdup(service) : s_ServiceName(service, 0)))
        return 0;
    if (!*s || !(iter = (SERV_ITER) calloc(1, sizeof(*iter)))) {
        free((void*) s);
        return 0;
    }

    iter->name            = s;
    iter->type            = types & ~special_flags;
    iter->host            = (preferred_host == SERV_LOCALHOST
                             ? SOCK_gethostbyname(0) : preferred_host);
    iter->port            = preferred_port;
    iter->pref            = (preference < 0.0
                             ? -1.0
                             :  0.01 * (preference > 100.0
                                        ? 100.0
                                        : preference));
    if (ismask)
        iter->ismask      = 1;
    if (types & fSERV_Promiscuous)
        iter->promiscuous = 1;
    if (types & fSERV_ReverseDns)
        iter->reverse_dns = 1;
    if (types & fSERV_Stateless)
        iter->stateless   = 1;
    iter->external        = external;
    if (arg) {
        iter->arg         = arg;
        iter->arglen      = strlen(arg);
    }
    if (iter->arglen  &&  val) {
        iter->val         = val;
        iter->vallen      = strlen(val);
    }
    iter->time            = (TNCBI_Time) time(0);

    if (n_skip) {
        size_t i;
        for (i = 0; i < n_skip; i++) {
            const char* name = (iter->ismask  ||  skip[i]->type == fSERV_Dns ?
                                SERV_NameOfInfo(skip[i]) : "");
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
        no_dispd = s_IsMapperDisabled(service, REG_CONN_DISPD_DISABLE);
        if (net_info->firewall)
            iter->type |= fSERV_Firewall;
        if (net_info->stateless)
            iter->stateless = 1;
        if (net_info->lb_disable)
            no_lbsmd = 1/*true*/;
        else
            no_lbsmd = s_IsMapperDisabled(service, REG_CONN_LBSMD_DISABLE);
    } else {
        no_dispd = 1/*true*/;
        no_lbsmd = s_IsMapperDisabled(service, REG_CONN_LBSMD_DISABLE);
    }

    if ((s_IsMapperDisabled(service, REG_CONN_LOCAL_DISABLE)
         ||  !(op = SERV_LOCAL_Open(iter, info, host_info)))             &&
        (no_lbsmd
         ||  !(op = SERV_LBSMD_Open(iter, info, host_info, !no_dispd)))  &&
        (no_dispd
         ||  !(op = SERV_DISPD_Open(iter, net_info, info, host_info)))) {
        if (no_lbsmd  &&  no_dispd) {
            CORE_LOGF(eLOG_Warning,
                      ("[SERV]  No service mappers found for `%s'", service));
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
    assert(iter && iter->op);
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
            if (go || !info)
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
    if (iter && iter->op && !info) {
        /* All DISPD searches end up here, but none LBSMD ones */
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
    return iter && iter->op ? s_GetNextInfo(iter, host_info, 0) : 0;
}


const SSERV_Info* SERV_GetNextInfo(SERV_ITER iter)
{
    return iter && iter->op ? s_GetNextInfo(iter, 0, 0) : 0;
}


const char* SERV_MapperName(SERV_ITER iter)
{
    return iter && iter->op ? iter->op->name : 0;
}


const char* SERV_CurrentName(SERV_ITER iter)
{
    const char* name = SERV_NameOfInfo(iter->last);
    return name  &&  *name ? name : iter->name;
}


int/*bool*/ SERV_Penalize(SERV_ITER iter, double fine)
{
    if (!iter || !iter->op || !iter->op->Penalize || !iter->last)
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
        if (!(referer = malloc(9 + strlen(host) + strlen(port)
                               + strlen(path) + strlen(args)))) {
            return;
        }
        strcat(strcat(strcpy(strcpy(referer, "http://")+7, host), port), path);
        if (args[0])
            strcat(strcat(referer, "?"), args);
    } else if ((str = strdup(iter->op->name)) != 0) {
        const char* host = net_info->client_host;
        const char* name = iter->name;

        if (!(referer = malloc(3 + 1 + 9 + 1 + 2*strlen(strlwr(str)) +
                               strlen(host) + strlen(name)))) {
            return;
        }
        strcat(strcat(strcat(strcpy(referer, str), "://"), host), "/");
        strcat(strcat(strcat(referer, str), "?service="), name);
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


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.85  2006/10/18 17:23:38  lavr
 * Elaborated comments
 *
 * Revision 6.84  2006/06/07 20:19:57  lavr
 * Formatting;  also note that minor revision number has been bumped up to 220
 *
 * Revision 6.83  2006/06/07 20:17:29  lavr
 * Slight rearrangement: s_FillOutReferer -> s_SetDefaultReferer
 *
 * Revision 6.82  2006/06/07 20:06:27  lavr
 * +s_FillOutReferer()
 *
 * Revision 6.81  2006/05/19 23:25:43  lavr
 * Reformat HTTP Referer: to account for all types of mappers
 *
 * Revision 6.80  2006/04/21 14:40:07  lavr
 * Use reinstated SConnNetInfo::lb_disable as before
 *
 * Revision 6.79  2006/04/20 19:22:49  lavr
 * Warn when open fails because no major mapper is enabled for the service
 *
 * Revision 6.78  2006/04/20 14:00:31  lavr
 * Use new mappers' switching scheme; faster service name lookup
 *
 * Revision 6.77  2006/03/28 18:28:29  lavr
 * Open now calls to see whether local (registry-conf) is available
 *
 * Revision 6.76  2006/03/05 17:47:55  lavr
 * +SERV_ITER::time, new VT::Update proto, SERV_OpenP() to return HINFO
 *
 * Revision 6.75  2006/01/27 17:10:08  lavr
 * Explicit casts that heed signed/unsigned comparison warning
 *
 * Revision 6.74  2006/01/17 20:27:42  lavr
 * Fix addition of skipped FIREWALL infos
 *
 * Revision 6.73  2006/01/11 20:27:51  lavr
 * Better generation of referral header in SERV_Print()
 *
 * Revision 6.72  2006/01/11 16:27:36  lavr
 * SERV_Update() and SERV_ITER's VT::Update() have got addt'l "code" argument
 * Changes to fix SERV_Reset() behavior as documented
 * Improve sticking to preference in case of malfuctioning services
 *
 * Revision 6.71  2005/12/23 18:13:19  lavr
 * New bitfields in SERV_ITER (corresponding to special service flags)
 * Better control of Server-Count in SERV_Print()
 *
 * Revision 6.70  2005/12/14 21:29:46  lavr
 * SERV_GetInfoP() and SERV_OpenP():  New signatures (changed parameteres)
 * SERV_CurrentName(), s_Open(): to use new SERV_NameOfInfo()
 * s_Open(): to use new SERV_CopyInfoEx()
 *
 * Revision 6.69  2005/12/08 03:54:15  lavr
 * Allow negative preference for internal "latch" mode
 *
 * Revision 6.68  2005/08/31 19:25:16  lavr
 * Fix number of bytes written in new referer
 *
 * Revision 6.67  2005/08/31 19:02:17  lavr
 * Change LBSMD referer HTTP tag
 *
 * Revision 6.66  2005/07/11 18:49:11  lavr
 * Hashed preference generation algorithm retired (proven to fail often)
 *
 * Revision 6.65  2005/07/11 18:16:00  lavr
 * Revised to allow wildcard searches thru service iterator
 *
 * Revision 6.64  2005/07/06 18:54:54  lavr
 * +enum ESERV_SpecialType to hold special server type bits (instead of macros)
 *
 * Revision 6.63  2005/07/06 18:28:20  lavr
 * Eliminate macro calls -- replaced with real function calls
 *
 * Revision 6.62  2005/05/04 16:14:48  lavr
 * -SERV_GetConfig()
 *
 * Revision 6.61  2005/04/25 18:47:29  lavr
 * Private API to accept SConnNetInfo* for network dispatching to work too
 *
 * Revision 6.60  2005/04/19 16:32:19  lavr
 * Cosmetics
 *
 * Revision 6.59  2005/03/05 21:05:26  lavr
 * +SERV_ITER::current;  +SERV_GetCurrentName()
 *
 * Revision 6.58  2005/01/31 17:09:55  lavr
 * Argument affinity moved into service iterator
 *
 * Revision 6.57  2005/01/05 19:15:26  lavr
 * Do not use C99-compliant declaration, kills MSVC compilation
 *
 * Revision 6.56  2005/01/05 17:39:07  lavr
 * SERV_Preference() modified to use better load distribution
 *
 * Revision 6.55  2004/08/19 15:48:35  lavr
 * SERV_ITER::type renamed into SERV_ITER::types to reflect its bitmask nature
 *
 * Revision 6.54  2004/07/01 16:28:19  lavr
 * +SERV_Print(): "Referer:" tag impelemented
 *
 * Revision 6.53  2004/06/14 16:37:09  lavr
 * Allow no more than one firewall server info in the skip list
 *
 * Revision 6.52  2004/05/17 18:19:43  lavr
 * Mark skip infos with maximal time instead of calculating 1 year from now
 *
 * Revision 6.51  2004/03/23 02:28:21  lavr
 * Limit service name resolution recursion by 8
 *
 * Revision 6.50  2004/01/30 14:37:26  lavr
 * Client revision made independent of CVS revisions
 *
 * Revision 6.49  2003/09/02 21:17:15  lavr
 * Clean up included headers
 *
 * Revision 6.48  2003/06/26 15:20:46  lavr
 * Additional parameter "external" in implementation of generic methods
 *
 * Revision 6.47  2003/06/09 19:53:01  lavr
 * +SERV_OpenP()
 *
 * Revision 6.46  2003/04/30 17:00:47  lavr
 * Name collision resolved
 *
 * Revision 6.45  2003/02/28 14:49:04  lavr
 * SERV_Preference(): redeclare last argument 'unsigned'
 *
 * Revision 6.44  2003/02/13 22:04:16  lavr
 * Document SERV_Preference() domain, change last argument, tweak formula
 *
 * Revision 6.43  2003/01/31 21:19:30  lavr
 * +SERV_GetInfoP(), preference measure for preferred host, SERV_Preference()
 *
 * Revision 6.42  2002/11/12 05:53:01  lavr
 * Fit a long line within 80 chars
 *
 * Revision 6.41  2002/10/28 20:16:00  lavr
 * Take advantage of host info API
 *
 * Revision 6.40  2002/10/28 15:43:49  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.39  2002/10/11 19:48:10  lavr
 * +SERV_GetConfig()
 * const dropped in return value of SERV_ServiceName()
 *
 * Revision 6.38  2002/09/04 15:11:41  lavr
 * Log moved to end
 *
 * Revision 6.37  2002/05/06 19:16:50  lavr
 * +#include <stdio.h>, +SERV_ServiceName() - translation of service name
 *
 * Revision 6.36  2002/04/15 20:07:09  lavr
 * Use size_t for iterating over skip_info's
 *
 * Revision 6.35  2002/04/13 06:35:11  lavr
 * Fast track routine SERV_GetInfoEx(), many syscalls optimizations
 *
 * Revision 6.34  2002/03/22 19:51:28  lavr
 * Do not explicitly include <assert.h>: included from ncbi_core.h
 *
 * Revision 6.33  2002/03/11 21:59:32  lavr
 * 'Client version' changed into 'Client revision'
 *
 * Revision 6.32  2001/11/09 20:03:14  lavr
 * Minor fix to remove a trailing space in client version tag
 *
 * Revision 6.31  2001/10/01 19:52:38  lavr
 * Call update directly; do not remove time from server specs in SERV_Print()
 *
 * Revision 6.30  2001/09/29 19:33:04  lavr
 * BUGFIX: SERV_Update() requires VT bound (was not the case in constructor)
 *
 * Revision 6.29  2001/09/28 22:03:12  vakatov
 * Included missing <connect/ncbi_ansi_ext.h>
 *
 * Revision 6.28  2001/09/28 20:50:16  lavr
 * SERV_Update() modified to capture Used-Server-Info tags;
 * Update VT method changed - now called on per-line basis;
 * Few bugfixes related to keeping last info correct
 *
 * Revision 6.27  2001/09/24 20:28:48  lavr
 * +SERV_Reset(); SERV_Close() changed to utilize SERV_Reset()
 *
 * Revision 6.26  2001/09/10 21:19:48  lavr
 * SERV_Print():  Client version tag added
 * SERV_OpenEx(): Firewall type handling
 *
 * Revision 6.25  2001/08/20 21:58:19  lavr
 * Parameter change for clarity: info -> net_info if type is SConnNetInfo
 *
 * Revision 6.24  2001/06/25 15:35:54  lavr
 * Added function: SERV_GetNextInfoEx
 *
 * Revision 6.23  2001/06/20 17:27:49  kans
 * include <time.h> for Mac compiler
 *
 * Revision 6.22  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.21  2001/05/24 21:28:12  lavr
 * Timeout for skip servers increased to 1 year period
 *
 * Revision 6.20  2001/05/17 15:02:51  lavr
 * Typos corrected
 *
 * Revision 6.19  2001/04/24 21:37:26  lavr
 * New code for: SERV_MapperName() and SERV_Penalize().
 *
 * Revision 6.18  2001/03/21 21:23:30  lavr
 * Explicit type conversion size_t -> unsigned in printf
 *
 * Revision 6.17  2001/03/20 22:03:32  lavr
 * BUGFIX in SERV_Print (miscalculation of buflen for accepted server types)
 *
 * Revision 6.16  2001/03/06 23:55:25  lavr
 * SOCK_gethostaddr -> SOCK_gethostbyname
 *
 * Revision 6.15  2001/03/05 23:10:29  lavr
 * SERV_WriteInfo takes only one argument now
 *
 * Revision 6.14  2001/03/02 20:09:51  lavr
 * Support added for SERV_LOCALHOST as preferred_host.
 *
 * Revision 6.13  2001/03/01 00:31:23  lavr
 * SERV_OpenSimple now builds SConnNetInfo to use both LBSMD and DISPD.CGI
 *
 * Revision 6.12  2001/02/09 17:33:06  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 *
 * Revision 6.11  2001/01/25 17:05:32  lavr
 * Bugfix in SERV_OpenEx: op was not inited to 0
 *
 * Revision 6.10  2001/01/08 23:47:29  lavr
 * (unsigned char) conversion in isspace
 *
 * Revision 6.9  2001/01/08 22:38:34  lavr
 * Numerous patches to code after debugging
 *
 * Revision 6.8  2000/12/29 18:01:27  lavr
 * SERV_Print introduced; pool of skipped services now follows
 * expiration times of services and is updated on that basis.
 *
 * Revision 6.7  2000/12/06 22:20:30  lavr
 * Skip info list is not maintained forever; instead the entries get
 * deleted in accordance with their expiration period
 *
 * Revision 6.6  2000/10/20 17:19:04  lavr
 * SConnNetInfo made 'const' as a parameter to 'SERV_Open*'
 * 'SERV_Update' added as a private interface
 *
 * Revision 6.5  2000/10/05 22:36:21  lavr
 * Additional parameters in call to DISPD mapper
 *
 * Revision 6.4  2000/05/31 23:12:23  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.3  2000/05/22 16:53:11  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.2  2000/05/12 21:42:59  lavr
 * Cleaned up for the C++ compilation, etc.
 *
 * Revision 6.1  2000/05/12 18:50:20  lavr
 * First working revision
 *
 * ==========================================================================
 */
