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
#include "ncbi_priv.h"
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#define SERV_SERVICE_NAME "SERVICE_NAME"


static char* s_ServiceName(const char* service, size_t depth)
{
    char *s, *p;
    char key[128];
    char srv[128];

    if (++depth > 8 || !service || !*service ||
        sizeof(DEF_CONN_REG_SECTION) + sizeof(SERV_SERVICE_NAME) +
        strlen(service) >= sizeof(key))
        return 0/*failure*/;
    s = key;
    strcpy(s, DEF_CONN_REG_SECTION);
    s += sizeof(DEF_CONN_REG_SECTION) - 1;
    *s++ = '_';
    strcpy(s, SERV_SERVICE_NAME);
    s += sizeof(SERV_SERVICE_NAME) - 1;
    *s++ = '_';
    strcpy(s, service);
    strupr(key);
    /* Looking for "CONN_SERVICE_NAME_service" in environment */
    if (!(p = getenv(key))) {
        *--s = '\0';
        /* Looking for "CONN_SERVICE_NAME" in registry's section [service] */
        CORE_REG_GET(service, key, srv, sizeof(srv), 0);
        if (!*srv)
            return strdup(service);
    } else
        strncpy0(srv, p, sizeof(srv) - 1);

    /* No cycle detection in service name redefinition */
    return s_ServiceName(srv, depth);
}


char* SERV_ServiceName(const char* service)
{
    return s_ServiceName(service, 0);
}


SERV_ITER SERV_OpenSimple(const char* service)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    SERV_ITER iter = SERV_Open(service, fSERV_Any, 0, net_info);
    ConnNetInfo_Destroy(net_info);
    return iter;
}


static int/*bool*/ s_AddSkipInfo(SERV_ITER iter, SSERV_Info* info)
{
    if (info->type == fSERV_Firewall) {
        size_t n;
        for (n = 0; n < iter->n_skip; n++) {
            SSERV_Info* temp = iter->skip[n];
            if (temp->type == fSERV_Firewall &&
                temp->u.firewall.type == info->u.firewall.type) {
                if (n < --iter->n_skip) {
                    memmove(iter->skip + n, iter->skip + n + 1,
                            sizeof(*iter->skip)*(iter->n_skip - n));
                }
                if (iter->last == temp)
                    iter->last = 0;
                free(temp);
                break;
            }
        }
    }
    if (iter->n_skip == iter->n_max_skip) {
        SSERV_Info** temp;
        size_t n = iter->n_max_skip + 10;

        if (iter->skip)
            temp = (SSERV_Info**) realloc(iter->skip, sizeof(*temp) * n);
        else
            temp = (SSERV_Info**) malloc(sizeof(*temp) * n);
        if (!temp)
            return 0;

        iter->skip = temp;
        iter->n_max_skip = n;
    }
    iter->skip[iter->n_skip++] = info;
    return 1;
}


static SERV_ITER s_Open(const char* service, TSERV_Type types,
                        unsigned int preferred_host, double preference,
                        const SConnNetInfo* net_info,
                        const SSERV_Info* const skip[], size_t n_skip,
                        SSERV_Info** info, HOST_INFO* host_info,
                        int/*bool*/ external, unsigned int origin,
                        const char* arg, const char* val)
{
    const char* s = s_ServiceName(service, 0);
    const SSERV_VTable* op;
    SERV_ITER iter;
    
    if (!s || !*s || !(iter = (SERV_ITER) malloc(sizeof(*iter))))
        return 0;

    iter->service        = s;
    iter->current        = 0;
    iter->types          = types;
    iter->preferred_host = preferred_host == SERV_LOCALHOST
        ? SOCK_gethostbyname(0) : preferred_host;
    iter->preference = 0.01*(preference < 0.0   ? 0.0   :
                             preference > 100.0 ? 100.0 : preference);
    iter->n_skip         = iter->n_max_skip = 0;
    iter->skip           = 0;
    iter->last           = 0;
    iter->op             = 0;
    iter->data           = 0;
    iter->external       = external;
    iter->origin         = origin;
    iter->arg            = arg;
    iter->arglen         = arg ? strlen(arg) : 0;
    iter->val            = val;
    iter->vallen         = val ? strlen(val) : 0;

    if (n_skip) {
        size_t i;
        for (i = 0; i < n_skip; i++) {
            size_t   skipinfolen = SERV_SizeOfInfo(skip[i]);
            SSERV_Info* skipinfo = (SSERV_Info*) malloc(skipinfolen);
            if (!skipinfo) {
                SERV_Close(iter);
                return 0;
            }
            memcpy(skipinfo, skip[i], skipinfolen);
            skipinfo->time = (TNCBI_Time)(-1);
            if (!s_AddSkipInfo(iter, skipinfo)) {
                free(skipinfo);
                SERV_Close(iter);
            }
        }
    }
    assert(n_skip == iter->n_skip);

    if (!net_info) {
        if (!(op = SERV_LBSMD_Open(iter, info, host_info))) {
            /* LBSMD failed in non-DISPD mapping */
            SERV_Close(iter);
            return 0;
        }
    } else {
        if (net_info->stateless)
            iter->types |= fSERV_StatelessOnly;
        if (net_info->firewall)
            iter->types |= fSERV_Firewall;
        if ((net_info->lb_disable ||
             !(op = SERV_LBSMD_Open(iter, info, host_info))) &&
            !(op = SERV_DISPD_Open(iter, net_info, info, host_info))) {
            SERV_Close(iter);
            return 0;
        }
    }

    assert(op != 0);
    iter->op = op;
    return iter;
}


SERV_ITER SERV_OpenEx(const char* service,
                      TSERV_Type types, unsigned int preferred_host,
                      const SConnNetInfo* net_info,
                      const SSERV_Info* const skip[], size_t n_skip)
{
    return s_Open(service, types, preferred_host, 0.0/*preference*/,
                  net_info, skip, n_skip, 0/*info*/, 0/*host_info*/,
                  0/*not external*/, 0/*origin*/, 0/*arg*/, 0/*val*/);
}


SERV_ITER SERV_OpenP(const char* service, TSERV_Type types,
                     unsigned int preferred_host, double preference,
                     int/*bool*/ external, unsigned int origin,
                     const char* arg, const char* val)
{
    return s_Open(service, types, preferred_host, preference,
                  0/*net_info*/, 0/*skip*/, 0/*n_skip*/, 0/*info*/, 0/*hinfo*/,
                  external, origin, arg, val);
}


static SSERV_Info* s_GetInfo(const char* service, TSERV_Type types,
                             unsigned int preferred_host, double preference,
                             const SConnNetInfo* net_info,
                             const SSERV_Info* const skip[], size_t n_skip,
                             HOST_INFO* host_info,
                             int/*bool*/ external, unsigned int origin,
                             const char* arg, const char* val)
{
    SSERV_Info* info = 0;
    SERV_ITER iter= s_Open(service, types, preferred_host, preference,
                           net_info, skip, n_skip, &info, host_info,
                           external, origin, arg, val);
    if (iter && !info && iter->op && iter->op->GetNextInfo)
        info = (*iter->op->GetNextInfo)(iter, host_info);
    SERV_Close(iter);
    return info;
}


SSERV_Info* SERV_GetInfoEx(const char* service, TSERV_Type types,
                           unsigned int preferred_host,
                           const SConnNetInfo* net_info,
                           const SSERV_Info* const skip[], size_t n_skip,
                           HOST_INFO* host_info)
{
    return s_GetInfo(service, types, preferred_host, 0.0/*preference*/,
                     net_info, skip, n_skip, host_info,
                     0/*not external*/, 0/*origin*/, 0/*arg*/, 0/*val*/);
}


SSERV_Info* SERV_GetInfoP(const char* service, TSERV_Type types,
                          unsigned int preferred_host, double preference,
                          int/*bool*/ external, unsigned int origin,
                          const char* arg, const char* val)
{
    return s_GetInfo(service, types, preferred_host, preference,
                     0/*net_info*/, 0/*skip*/, 0/*n_skip*/, 0/*host_info*/,
                     external, origin, arg, val);
}


static void s_SkipSkip(SERV_ITER iter)
{
    if (iter->n_skip) {
        TNCBI_Time t = (TNCBI_Time) time(0);
        size_t n = 0;

        while (n < iter->n_skip) {
            SSERV_Info* temp = iter->skip[n];
            if (temp->time < t) {
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
}


const SSERV_Info* SERV_GetNextInfoEx(SERV_ITER iter, HOST_INFO* host_info)
{
    SSERV_Info* info = 0;

    if (iter && iter->op) {
        /* First, remove all outdated entries from our skip list */
        s_SkipSkip(iter);
        /* Next, obtain a fresh entry from the actual mapper */
        if (iter->op->GetNextInfo &&
            (info = (*iter->op->GetNextInfo)(iter, host_info)) != 0 &&
            !s_AddSkipInfo(iter, info)) {
            free(info);
            info = 0;
        }
        iter->last = info;
    }
    return info;
}


const char* SERV_MapperName(SERV_ITER iter)
{
    return iter && iter->op ? iter->op->name : 0;
}


const char* SERV_GetCurrentName(SERV_ITER iter)
{
    return iter->current ? iter->current : iter->service;
}


int/*bool*/ SERV_Penalize(SERV_ITER iter, double fine)
{
    if (!iter || !iter->op || !iter->op->Penalize || !iter->last)
        return 0;
    return (*iter->op->Penalize)(iter, fine);
}


char* SERV_GetConfig(void)
{
    return SERV_LBSMD_GetConfig();
}


void SERV_Reset(SERV_ITER iter)
{
    size_t i;
    if (!iter)
        return;
    if (iter->current) {
        free((void*) iter->current);
        iter->current = 0;
    }
    for (i = 0; i < iter->n_skip; i++)
        free(iter->skip[i]);
    iter->n_skip = 0;
    iter->last = 0;
    if (iter->op && iter->op->Reset)
        (*iter->op->Reset)(iter);
}


void SERV_Close(SERV_ITER iter)
{
    if (!iter)
        return;
    SERV_Reset(iter);
    if (iter->op && iter->op->Close)
        (*iter->op->Close)(iter);
    if (iter->skip)
        free(iter->skip);
    if (iter->service)
        free((void*) iter->service);
    free(iter);
}


int/*bool*/ SERV_Update(SERV_ITER iter, const char* text)
{
    static const char used_server_info[] = "Used-Server-Info-";
    int retval = 0/*not updated yet*/;

    if (iter && iter->op && text) {
        TNCBI_Time now = (TNCBI_Time) time(0);
        const char *c, *b;
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
                t[len - 1] = 0;
            else
                t[len] = 0;
            p = t;
            if (iter->op->Update && (*iter->op->Update)(iter, now, p))
                retval = 1/*updated*/;
            if (strncasecmp(p, used_server_info,
                            sizeof(used_server_info) - 1) == 0) {
                p += sizeof(used_server_info) - 1;
                if (sscanf(p, "%u: %n", &d1, &d2) >= 1 &&
                    (info = SERV_ReadInfo(p + d2)) != 0) {
                    if (!s_AddSkipInfo(iter, info))
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


char* SERV_PrintEx(SERV_ITER iter, const SConnNetInfo* referrer)
{
    static const char referrer_header[] = "Referer: "/*standard misspelling*/;
    static const char client_revision[] = "Client-Revision: %hu.%hu\r\n";
    static const char accepted_types[] = "Accepted-Server-Types:";
    char buffer[128], *str;
    TSERV_Type types, t;
    size_t buflen, i;
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
        int/*bool*/ refer = referrer && iter->op && iter->op->name;
        if (refer && strcasecmp(iter->op->name, "DISPD") == 0) {
            const char* host = referrer->host;
            const char* path = referrer->path;
            const char* args = referrer->args;
            const char* service = iter->service;
            char port[8];
            if (referrer->port && referrer->port != DEF_CONN_PORT)
                sprintf(port, ":%hu", referrer->port);
            else
                port[0] = '\0';
            if (!BUF_Write(&buf, referrer_header, sizeof(referrer_header)-1) ||
                !BUF_Write(&buf, "http://", 7)                               ||
                !BUF_Write(&buf, host, strlen(host))                         ||
                !BUF_Write(&buf, port, strlen(port))                         ||
                !BUF_Write(&buf, path, strlen(path))                         ||
                !BUF_Write(&buf, "?service=", 9)                             ||
                !BUF_Write(&buf, service, strlen(service))                   ||
                (args[0] && (!BUF_Write(&buf, "&", 1)                        ||
                             !BUF_Write(&buf, args, strlen(args))))          ||
                !BUF_Write(&buf, "\r\n", 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        } else if (refer && strcasecmp(iter->op->name, "LBSMD") == 0) {
            const char* host = referrer->client_host;
            const char* service = iter->service;
            if (!BUF_Write(&buf, referrer_header, sizeof(referrer_header)-1) ||
                !BUF_Write(&buf, "lbsm://", 7)                               ||
                !BUF_Write(&buf, host, strlen(host))                         ||
                !BUF_Write(&buf, "/dispatch?service=", 18)                   ||
                !BUF_Write(&buf, service, strlen(service))                   ||
                !BUF_Write(&buf, "\r\n", 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        /* Form accepted server types */
        buflen = sizeof(accepted_types) - 1;
        memcpy(buffer, accepted_types, buflen);
        types = (TSERV_Type) (iter->types & ~fSERV_StatelessOnly);
        for (t = 1; t; t <<= 1) {
            if (types & t) {
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
        /* Drop any outdated skip entries */
        s_SkipSkip(iter);
        /* Put all the rest into rejection list */
        for (i = 0; i < iter->n_skip; i++) {
            if (!(str = SERV_WriteInfo(iter->skip[i])))
                break;
            buflen = sprintf(buffer, "Skip-Info-%u: ", (unsigned) i + 1); 
            assert(buflen < sizeof(buffer)-1);
            if (!BUF_Write(&buf, buffer, buflen) ||
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


#ifdef SERV_Print
#  undef SERV_Print
#endif
char* SERV_Print(SERV_ITER iter)
{
    return SERV_PrintEx(iter, 0);
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
 * +SERV_PrintEx(): "Referer:" tag impelemented
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
