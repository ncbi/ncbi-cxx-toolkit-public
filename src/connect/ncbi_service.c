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
#ifdef NCBI_CXX_TOOLKIT
#  include "ncbi_lbdns.h"
#  include "ncbi_lbosp.h"
#  include "ncbi_linkerd.h"
#  include "ncbi_namerd.h"
#endif /*NCBI_CXX_TOOLKIT*/
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


#define SERV_SERVICE_NAME_RECURSION  10
#define CONN_IMPLICIT_SERVER_TYPE    DEF_CONN_REG_SECTION               \
                                     "_" REG_CONN_IMPLICIT_SERVER_TYPE

static ESwitch s_Fast = eOff;


ESwitch SERV_DoFastOpens(ESwitch on)
{
    ESwitch retval = s_Fast;
    if (on != eDefault)
        s_Fast = on;
    return retval;
}


static int/*bool*/ x_tr(char* str, char a, char b, size_t len)
{
    int/*bool*/ done = 0/*false*/;
    char* end = str + len;
    while (str < end) {
        if (*str == a) {
            *str  = b;
            done = 1/*true*/;
        }
        ++str;
    }
    return done;
}


static char* x_ServiceName(unsigned int depth,
                           const char* service, const char* svc,
                           int/*bool*/ ismask, int/*bool*/ isfast)
{
    char   buf[128];
    size_t len = 0;

    assert(!svc == !service);
    assert(sizeof(buf) > sizeof(REG_CONN_SERVICE_NAME));
    if (!svc  ||  (!ismask  &&  (!*svc  ||  strpbrk(svc, "?*[")))
        ||  (len = strlen(svc)) >= sizeof(buf)-sizeof(REG_CONN_SERVICE_NAME)
        ||  NCBI_HasSpaces(svc, len)) {
        if (!service  ||  strcasecmp(service, svc) == 0)
            service = "";
        CORE_LOGF_X(7, eLOG_Error,
                    ("%s%s%s%s service name%s%s",
                     !svc  ||  !*svc ? "" : "[",
                     !svc ? "" : svc,
                     !svc  ||  !*svc ? "" : "]  ",
                     !svc ? "NULL" : !*svc ? "Empty"
                     : len < sizeof(buf) - sizeof(REG_CONN_SERVICE_NAME)
                     ? "Invalid" : "Too long",
                     *service ? " for: " : "", service));
        return 0/*failure*/;
    }
    if (!ismask  &&  !isfast) {
        char  tmp[sizeof(buf)];
        int/*bool*/ tr = x_tr((char*) memcpy(tmp, svc, len), '-', '_', len);
        char* s = tmp + len;
        *s++ = '_';
        memcpy(s, REG_CONN_SERVICE_NAME, sizeof(REG_CONN_SERVICE_NAME));
        len += 1 + sizeof(REG_CONN_SERVICE_NAME);
        /* Looking for "svc_CONN_SERVICE_NAME" in the environment */
        if ((!(s = getenv(strupr((char*) memcpy(buf, tmp, len--))))
             &&  (memcmp(buf, tmp, len) == 0  ||  !(s = getenv(tmp))))
            ||  !*s) {
            /* Looking for "CONN_SERVICE_NAME" in registry section "[svc]" */
            len -= sizeof(REG_CONN_SERVICE_NAME);
            if (tr)
                memcpy(buf, svc, len);  /* re-copy */
            buf[len++] = '\0';
            if (!CORE_REG_GET(buf, buf + len, tmp, sizeof(tmp), 0))
                *tmp = '\0';
            s = tmp;
        }
        if (*s  &&  strcasecmp(s, svc) != 0) {
            if (depth++ < SERV_SERVICE_NAME_RECURSION)
                return x_ServiceName(depth, service, s, ismask, isfast);
            CORE_LOGF_X(8, eLOG_Error,
                        ("[%s]  Maximal service name recursion"
                         " depth reached: %u", service, depth));
            return 0/*failure*/;
        }
    }
    return strdup(svc);
}


static char* s_ServiceName(const char* service,
                           int/*bool*/ ismask, int/*bool*/ isfast)
{
    char* retval;
    CORE_LOCK_READ;
    retval = x_ServiceName(0/*depth*/, service, service, ismask, isfast);
    CORE_UNLOCK;
    return retval;
}


char* SERV_ServiceName(const char* service)
{
    return s_ServiceName(service, 0/*ismask*/, 0/*isfast*/);
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
static int/*bool*/ x_IsMapperConfigured(const char* svc,
                                        const char* key,
                                        int/*bool*/ fast)
{
    char val[40];
    return fast
        ? 0/*false*/
        : ConnNetInfo_Boolean(ConnNetInfo_GetValueInternal
                              (svc, key, val, sizeof(val), 0));
}


#define s_IsMapperConfigured(s, k)  x_IsMapperConfigured(s, k, s_Fast)


int/*bool*/ SERV_IsMapperConfiguredInternal(const char* svc, const char* key)
{
    return s_IsMapperConfigured(svc, key);
}


static SERV_ITER x_Open(const char*         service,
                        int/*bool*/         ismask,
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
                        SSERV_Info**        info,
                        HOST_INFO*          host_info)
{
    int/*bool*/
        do_local,
        do_lbsmd   = -1/*unassigned*/,
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
        do_lbdns   = -1/*unassigned*/,
#  endif /*NCBI_OS_UNIX*/
        do_linkerd = -1/*unassigned*/,
        do_namerd  = -1/*unassigned*/,
        do_lbos    = -1/*unassigned*/,
#endif /*NCBI_CXX_TOOLKIT*/
        do_dispd   = -1/*unassigned*/;
    const SSERV_VTable* op;
    const char* svc;
    SERV_ITER iter;

    if (!(svc = s_ServiceName(service, ismask, s_Fast)))
        return 0;
    assert(ismask  ||  *svc);
    if (!(iter = (SERV_ITER) calloc(1, sizeof(*iter)))) {
        free((void*) svc);
        return 0;
    }

    iter->name              = svc;
    iter->host              = (preferred_host == SERV_LOCALHOST
                               ? SOCK_GetLocalHostAddress(eDefault)
                               : preferred_host);
    iter->port              = preferred_port;
    iter->pref              = (preference < 0.0
                               ? -1.0
                               :  0.01 * (preference > 100.0
                                          ? 100.0
                                          : preference));
    iter->types             = (TSERV_TypeOnly) types;
    if (ismask)
        iter->ismask        = 1;
    if (types & fSERV_IncludeDown)
        iter->ok_down       = 1;
    if (types & fSERV_IncludeStandby)
        iter->ok_standby    = 1;
    if (types & fSERV_IncludePrivate)
        iter->ok_private    = 1;
    if (types & fSERV_IncludeReserved)
        iter->ok_reserved   = 1;
    if (types & fSERV_IncludeSuppressed)
        iter->ok_suppressed = 1;
    if (types & fSERV_ReverseDns)
        iter->reverse_dns   = 1;
    iter->external          = external ? 1 : 0;
    if (arg  &&  *arg) {
        iter->arg           = arg;
        iter->arglen        = strlen(arg);
        if (val) {
            iter->val       = val;
            iter->vallen    = strlen(val);
        }
    }
    iter->time              = (TNCBI_Time) time(0);
    if (ismask)
        svc = 0;

    if (n_skip) {
        size_t i;
        for (i = 0;  i < n_skip;  ++i) {
            const char* name = (iter->ismask  ||  skip[i]->type == fSERV_Dns
                                ? SERV_NameOfInfo(skip[i]) : "");
            SSERV_Info* temp = SERV_CopyInfoEx(skip[i],
                                               iter->reverse_dns  &&  !*name ?
                                               iter->name             : name );
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
    iter->o_skip = iter->n_skip;

    if (net_info) {
        if (net_info->external)
            iter->external = 1;
        if (net_info->firewall)
            iter->types |= fSERV_Firewall;
        if (net_info->stateless)
            iter->types |= fSERV_Stateless;
        if (net_info->lb_disable)
            do_lbsmd = 0/*false*/;
    } else {
#ifdef NCBI_CXX_TOOLKIT
        do_linkerd = do_namerd = do_lbos =
#endif /*NCBI_CXX_TOOLKIT*/
            do_dispd = 0/*false*/;
    }
    if (host_info)
        *host_info = 0;
    /* Ugly optimization not to access the registry more than necessary */
    if ((!(do_local = s_IsMapperConfigured(svc, REG_CONN_LOCAL_ENABLE))      ||
         !(op = SERV_LOCAL_Open(iter, info)))

        &&
        (!do_lbsmd                                                           ||
         !(do_lbsmd = !s_IsMapperConfigured(svc, REG_CONN_LBSMD_DISABLE))    ||
         !(op = SERV_LBSMD_Open(iter, info, host_info,
                                (!do_dispd                                   ||
                                 !(do_dispd = !s_IsMapperConfigured
                                   (svc, REG_CONN_DISPD_DISABLE)))
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
                                &&
                                !(do_lbdns = s_IsMapperConfigured
                                  (svc, REG_CONN_LBDNS_ENABLE))
#  endif /*NCBI_OS_UNIX*/
                                &&
                                (!do_linkerd                                 ||
                                 !(do_linkerd = s_IsMapperConfigured
                                   (svc, REG_CONN_LINKERD_ENABLE)))
                                &&
                                (!do_namerd                                  ||
                                 !(do_namerd = s_IsMapperConfigured
                                   (svc, REG_CONN_NAMERD_ENABLE)))
                                &&
                                (!do_lbos                                    ||
                                 !(do_lbos = s_IsMapperConfigured
                                   (svc, REG_CONN_LBOS_ENABLE)))
#endif /*NCBI_CXX_TOOLKIT*/
                                )))

#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
        &&
        (!do_lbdns                                                           ||
         (do_lbdns < 0  &&  !(do_lbdns = s_IsMapperConfigured
                              (svc, REG_CONN_LBDNS_ENABLE)))                 ||
         !(op = SERV_LBDNS_Open(iter, info)))
#  endif /*NCBI_OS_UNIX*/
        &&
        (!do_linkerd                                                         ||
         (do_linkerd < 0  &&  !(do_linkerd = s_IsMapperConfigured
                                (svc, REG_CONN_LINKERD_ENABLE)))             ||
         !(op = SERV_LINKERD_Open(iter, net_info, info)))
        &&
        (!do_namerd                                                          ||
         (do_namerd < 0  &&  !(do_namerd = s_IsMapperConfigured
                               (svc, REG_CONN_NAMERD_ENABLE)))               ||
         !(op = SERV_NAMERD_Open(iter, net_info, info)))
        &&
        (!do_lbos                                                            ||
         (do_lbos < 0  &&  !(do_lbos = s_IsMapperConfigured
                             (svc, REG_CONN_LBOS_ENABLE)))                   ||
         !(op = SERV_LBOS_Open(iter, net_info, info)))
#endif /*NCBI_CXX_TOOLKIT*/

        &&
        (!do_dispd                                                           ||
         (do_dispd < 0  &&  !(do_dispd = !s_IsMapperConfigured
                              (svc, REG_CONN_DISPD_DISABLE)))                ||
         !(op = SERV_DISPD_Open(iter, net_info, info)))) {
        if (!do_local  &&  !do_lbsmd  &&  !do_dispd
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
            &&  !do_lbdns
#  endif /*NCBI_OS_UNIX*/
            &&  !do_linkerd  &&  !do_namerd  &&  !do_lbos
#endif /*NCBI_CXX_TOOLKIT*/
            ) {
            if (svc  &&  strcasecmp(service, svc) == 0)
                svc = 0;
            assert(*service  ||  !svc);
            CORE_LOGF_X(1, eLOG_Error,
                        ("%s%s%s%s%sNo service mappers available",
                         &"["[!*service], service,
                         &"/"[!svc], svc ? svc : "",
                         *service ? "]  " : ""));
        }
        SERV_Close(iter);
        return 0;
    }

    assert(op != 0);
    iter->op = op;
    return iter;
}


static void s_SkipSkip(SERV_ITER iter)
{
    size_t n;
    if (iter->time  &&  (iter->ismask | iter->ok_down | iter->ok_suppressed))
        return;
    n = 0;
    while (n < iter->n_skip) {
        SSERV_InfoCPtr temp = iter->skip[n];
        if (temp != iter->last  &&  temp->time != NCBI_TIME_INFINITE
            &&  (!iter->time/*iterator reset*/
                 ||  ((temp->type != fSERV_Dns  ||  temp->host)
                      &&  temp->time < iter->time))) {
            if (--iter->n_skip > n) {
                SSERV_InfoCPtr* ptr = iter->skip + n;
                memmove((void*) ptr, (void*)(ptr + 1),
                        (iter->n_skip - n) * sizeof(*ptr));
            }
            free((void*) temp);
        } else
            n++;
    }
}


#if defined(_DEBUG)  &&  !defined(NDEBUG)

#define RETURN(retval)  return x_Return((SSERV_Info*) info, infostr, retval)


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int/*bool*/ x_Return(SSERV_Info* info,
                            const char* infostr,
                            int/*bool*/ retval)
{
    if (infostr)
        free((void*) infostr);
    if (!retval)
        free(info);
    return retval;
}


/* Do some basic set of consistency checks for the returned server info.
 * Return 0 if failed (also free(info) if so);  return 1 if passed. */
static int/*bool*/ x_ConsistencyCheck(SERV_ITER iter, const SSERV_Info* info)
{
    const char* name = SERV_NameOfInfo(info);
    const char* infostr = SERV_WriteInfo(info);
    TSERV_TypeOnly types;
    size_t n;

    if (!name) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  NULL name\n%s", iter->name,
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if (!(iter->ismask | iter->reverse_dns) != !*name) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  %s name\n%s", iter->name,
                   *name ? "Unexpected" : "Empty",
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if (!info->host  ||  !info->port) {
        if (info->type != fSERV_Dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Non-DNS server with empty %s:\n%s", iter->name,
                       !(info->host | info->port) ? "host:port"
                       : info->host ? "port" : "host",
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (!info->host  &&  (iter->last  ||  iter->ismask)) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Interim DNS server w/o host:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }
    if (info->time) {
        if (info->time < iter->time) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Expired entry (%u < %u):\n%s", iter->name,
                       info->time, iter->time,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (info->time > iter->time + LBSM_DEFAULT_TIME) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Excessive expiration (%u > %u):\n%s", iter->name,
                       info->time, iter->time + LBSM_DEFAULT_TIME,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }
    if (info->type == fSERV_Firewall) {
        if (info->u.firewall.type == fSERV_Dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Firewall DNS entry not allowed:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (info->vhost | info->extra) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Firewall entry with %s%s%s:\n%s", iter->name,
                       info->vhost                  ? "vhost" : "",
                       info->extra  &&  info->vhost ? " and " : "",
                       info->extra                  ? "extra" : "",
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }
    if (info->type == fSERV_Dns) {
        if (info->site & fSERV_Private) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry cannot be private:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (info->mode & fSERV_Stateful) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry cannot be stateful:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (info->mime_t != eMIME_T_Undefined  ||
            info->mime_s != eMIME_Undefined    ||
            info->mime_e != eENCOD_None) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry with MIME type:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }
    if ((info->type & fSERV_Http)  &&  (info->mode & fSERV_Stateful)) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  HTTP entry cannot be stateful:\n%s", iter->name,
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if (!(types = iter->types &
          (TSERV_TypeOnly)(~(fSERV_Stateless | fSERV_Firewall)))) {
        if (info->type == fSERV_Dns  &&  !iter->reverse_dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry unwarranted:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    } else if ((info->type != fSERV_Firewall
                &&  !(types & info->type))  ||
               (info->type == fSERV_Firewall
                &&  !(types & info->u.firewall.type))) {
        if (info->type != fSERV_Dns  ||  !iter->reverse_dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Mismatched type 0x%X vs 0x%X:\n%s", iter->name,
                       (int)(info->type == fSERV_Firewall
                             ? info->u.firewall.type
                             : info->type), (int) types,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }
    if (iter->external  &&  (info->site & (fSERV_Local | fSERV_Private))) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Local/private entry for external:\n%s", iter->name,
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if ((info->site & fSERV_Private)  &&  !iter->ok_private
        &&  iter->localhost  &&  info->host != iter->localhost) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Private entry unwarranted:\n%s", iter->name,
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if ((iter->types & fSERV_Stateless)  &&  (info->mode & fSERV_Stateful)) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Steteful entry in stateless search:\n%s", iter->name,
                   infostr ? infostr : "<NULL>"));
        RETURN(0/*failure*/);
    }
    if (info->type != fSERV_Dns  ||  info->host) {
        if (!info->time  &&  !info->rate) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Off entry returned:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (SERV_IsDown(info)  &&  !iter->ok_down) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Down entry unwarranted:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (SERV_IfSuppressed(info)  &&  !iter->ok_suppressed) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Suppressed entry unwarranted:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (SERV_IsReserved(info)  &&  !iter->ok_reserved) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Reserved entry unwarranted:\n%s", iter->name,
                       infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
        if (SERV_IsStandby(info)  &&  !iter->ok_standby) {
            for (n = 0;  n < iter->n_skip;  ++n) {
                if (!SERV_IsStandby(iter->skip[n])) {
                    CORE_LOGF(eLOG_Critical,
                              ("[%s]  Standby entry unwarranted:\n%s",
                               iter->name, infostr ? infostr : "<NULL>"));
                    RETURN(0/*failure*/);
                }
            }
        }
    }
    for (n = 0;  n < iter->n_skip;  ++n) {
        const SSERV_Info* skip = iter->skip[n];
        if (strcasecmp(name, SERV_NameOfInfo(skip)) == 0
            &&  SERV_EqualInfo(info, skip)) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Entry is a duplicate and must be skipped:\n"
                       "%s%s%s%s", iter->name, &"\""[!*name], name,
                       *name ? "\" " : "", infostr ? infostr : "<NULL>"));
            RETURN(0/*failure*/);
        }
    }

    CORE_LOGF(eLOG_Trace, ("[%s]  Consistency check passed:\n%s%s%s%s",
                           iter->name, &"\""[!*name], name,
                           *name ? "\" " : "", infostr ? infostr : "<NULL>"));
    RETURN(1/*success*/);
}


#undef RETURN

#endif /*_DEBUG && !NDEBUG*/


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
            int/*bool*/ go;
            CORE_DEBUG_ARG(if (!x_ConsistencyCheck(iter, info)) return 0);
            /* This should never be actually used for LBSMD dispatcher,
             * as all exclusion logic is already done in it internally. */
            go =
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


static SERV_ITER s_Open(const char*         service,
                        int/*bool*/         ismask,
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
                        SSERV_Info**        info,
                        HOST_INFO*          host_info)
{
    SSERV_Info* x_info;
    SERV_ITER iter = x_Open(service, ismask, types,
                            preferred_host, preferred_port, preference,
                            net_info, skip, n_skip,
                            external, arg, val,
                            &x_info, host_info);
    assert(!iter  ||  iter->op);
    if (!iter)
        x_info = 0;
    else if (!x_info)
        x_info = info ? s_GetNextInfo(iter, host_info, 1/*internal*/) : 0;
    else if (x_info == (SSERV_Info*)(-1L)) {
        SERV_Close(iter);
        x_info = 0;
        iter = 0;
    }
    if (info)
        *info = x_info;
    else if (x_info)
        free(x_info);
    return iter;
}


extern SERV_ITER SERV_OpenSimple(const char* service)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    SERV_ITER iter = s_Open(service, 0/*not mask*/, fSERV_Any,
                            SERV_ANYHOST,
                            0/*preferred_port*/, 0.0/*preference*/,
                            net_info, 0/*skip*/, 0/*n_skip*/,
                            0/*not external*/, 0/*arg*/, 0/*val*/,
                            0/*info*/, 0/*host_info*/);
    ConnNetInfo_Destroy(net_info);
    return iter;
}


extern SERV_ITER SERV_Open(const char*         service,
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


extern SERV_ITER SERV_OpenEx(const char*         service,
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
                  service  &&  (!*service  ||  strpbrk(service, "?*[")), types,
                  preferred_host, preferred_port, preference,
                  net_info, skip, n_skip,
                  external, arg, val,
                  0/*info*/, 0/*host_info*/);
}


extern SSERV_Info* SERV_GetInfoSimple(const char* service)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service);
    SSERV_Info* info = SERV_GetInfoP(service, fSERV_Any,
                                     SERV_ANYHOST/*preferred_host*/,
                                     0/*preferred_port*/, 0.0/*preference*/,
                                     net_info, 0/*skip*/, 0/*n_skip*/,
                                     0/*not external*/, 0/*arg*/, 0/*val*/,
                                     0/*host_info*/);
    ConnNetInfo_Destroy(net_info);
    return info;
}


extern SSERV_Info* SERV_GetInfo(const char*         service,
                                TSERV_Type          types,
                                unsigned int        preferred_host,
                                const SConnNetInfo* net_info)
{
    return SERV_GetInfoP(service, types,
                         preferred_host,
                         0/*preferred_port*/, 0.0/*preference*/,
                         net_info, 0/*skip*/, 0/*n_skip*/,
                         0/*not external*/, 0/*arg*/, 0/*val*/,
                         0/*host_info*/);
}


extern SSERV_Info* SERV_GetInfoEx(const char*         service,
                                  TSERV_Type          types,
                                  unsigned int        preferred_host,
                                  const SConnNetInfo* net_info,
                                  SSERV_InfoCPtr      skip[],
                                  size_t              n_skip,
                                  HOST_INFO*          host_info)
{
    return SERV_GetInfoP(service, types,
                         preferred_host,
                         0/*preferred_port*/, 0.0/*preference*/,
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
    SSERV_Info* info;
    SERV_ITER iter = s_Open(service, 0/*not mask*/, types,
                            preferred_host, preferred_port, preference,
                            net_info, skip, n_skip,
                            external, arg, val,
                            &info, host_info);
    assert(!info  ||  iter);
    SERV_Close(iter);
    return info;
}


extern SSERV_InfoCPtr SERV_GetNextInfoEx(SERV_ITER  iter,
                                         HOST_INFO* host_info)
{
    assert(!iter  ||  iter->op);
    return iter ? s_GetNextInfo(iter, host_info, 0) : 0;
}


extern SSERV_InfoCPtr SERV_GetNextInfo(SERV_ITER iter)
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


extern int/*bool*/ SERV_Penalize(SERV_ITER iter, double fine)
{
    return SERV_PenalizeEx(iter, fine, 0);
}


extern int/*bool*/ SERV_Rerate(SERV_ITER iter, double rate)
{
    assert(!iter  ||  iter->op);
    if (!iter  ||  !iter->op->Feedback  ||  !iter->last)
        return 0/*false*/;
    return iter->op->Feedback(iter, rate, 0/*i.e.rate*/);
}


extern void SERV_Reset(SERV_ITER iter)
{
    if (!iter)
        return;
    iter->last  = 0;
    iter->time  = 0;
    s_SkipSkip(iter);
    if (iter->op  &&  iter->op->Reset)
        iter->op->Reset(iter);
}


extern void SERV_Close(SERV_ITER iter)
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
    const char *c, *s;

    iter->time = (TNCBI_Time) time(0);
    for (s = text;  (c = strchr(s, '\n')) != 0;  s = c + 1) {
        size_t len = (size_t)(c - s);
        SSERV_Info* info;
        unsigned int d1;
        char *p, *q;
        int d2;

        if (!(q = (char*) malloc(len + 1)))
            continue;
        memcpy(q, s, len);
        if (q[len - 1] == '\r')
            q[len - 1]  = '\0';
        else
            q[len    ]  = '\0';
        p = q;
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
        free(q);
    }
    return retval;
}


char* SERV_Print(SERV_ITER iter, const SConnNetInfo* net_info, int but_last)
{
    static const char kAcceptedServerTypes[] = "Accepted-Server-Types:";
    static const char kClientRevision[] = "Client-Revision: %u.%u\r\n";
    static const char kUsedServerInfo[] = "Used-Server-Info: ";
    static const char kNcbiExternal[] = NCBI_EXTERNAL ": Y\r\n";
    static const char kNcbiFWPorts[] = "NCBI-Firewall-Ports: ";
    static const char kPreference[] = "Preference: ";
    static const char kSkipInfo[] = "Skip-Info-%u: ";
    static const char kAffinity[] = "Affinity: ";
    char buffer[128], *str;
    size_t buflen, i;
    BUF buf = 0;

    /* Put client version number */
    buflen = (size_t) sprintf(buffer, kClientRevision,
                              SERV_CLIENT_REVISION_MAJOR,
                              SERV_CLIENT_REVISION_MINOR);
    assert(buflen < sizeof(buffer));
    if (!BUF_Write(&buf, buffer, buflen)) {
        BUF_Destroy(buf);
        return 0;
    }
    if (iter) {
        TSERV_TypeOnly t, types
            = iter->types & (TSERV_TypeOnly)(~fSERV_Stateless);
        /* Accepted server types */
        buflen = sizeof(kAcceptedServerTypes) - 1;
        memcpy(buffer, kAcceptedServerTypes, buflen);
        for (t = 1;  t;  t = (TSERV_TypeOnly)(t << 1)) {
            if (types & t) {
                const char* name = SERV_TypeStr((ESERV_Type) t);
                size_t namelen = strlen(name);
                if (!namelen  ||  buflen + 1 + namelen + 2 >= sizeof(buffer))
                    break;
                buffer[buflen++] = ' ';
                memcpy(buffer + buflen, name, namelen);
                buflen += namelen;
            } else if (types < t)
                break;
        }
        if (buffer[buflen - 1] != ':') {
            strcpy(&buffer[buflen], "\r\n");
            assert(strlen(buffer) == buflen+2  &&  buflen+2 < sizeof(buffer));
            if (!BUF_Write(&buf, buffer, buflen + 2)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (iter->external) {
            /* External */
            if (!BUF_Write(&buf, kNcbiExternal, sizeof(kNcbiExternal)-1)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if (types & fSERV_Firewall) {
            /* Firewall */
            EFWMode mode
                = net_info ? (EFWMode) net_info->firewall : eFWMode_Legacy;
            SERV_PrintFirewallPorts(buffer, sizeof(buffer), mode);
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
        for (i = 0;  i < iter->n_skip;  ++i) {
            /* NB: all skip infos are now kept with names (perhaps, empty) */
            const char* name    = SERV_NameOfInfo(iter->skip[i]);
            size_t      namelen = name  &&  *name ? strlen(name) : 0;
            if (!(str = SERV_WriteInfo(iter->skip[i])))
                break;
            if (but_last  &&  iter->last == iter->skip[i]) {
                buflen = sizeof(kUsedServerInfo) - 1;
                memcpy(buffer, kUsedServerInfo, buflen);
            } else
                buflen = (size_t) sprintf(buffer, kSkipInfo, (unsigned) i + 1);
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


extern unsigned short SERV_ServerPort(const char*  name,
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
    if (!(info = SERV_GetInfoP(name, fSERV_Standalone | fSERV_Promiscuous,
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


extern int/*bool*/ SERV_SetImplicitServerType(const char* service,
                                              ESERV_Type  type)
{
    char* buf, *svc = SERV_ServiceName(service);
    const char* typ = SERV_TypeStr(type);
    size_t len;

    if (!svc)
        return 0/*failure*/;
    /* Store service-specific setting */
    if (CORE_REG_SET(svc, CONN_IMPLICIT_SERVER_TYPE, typ, eREG_Transient)) {
        free(svc);
        return 1/*success*/;
    }
    len = strlen(svc);
    if (!(buf = (char*) realloc(svc, len + sizeof(CONN_IMPLICIT_SERVER_TYPE)
                                + 2/*"=\0"*/) + strlen(typ))) {
        free(svc);
        return 0/*failure*/;
    }
    x_tr(strupr(buf), '-', '_', len);
    memcpy(buf + len,
           "_"    CONN_IMPLICIT_SERVER_TYPE,
           sizeof(CONN_IMPLICIT_SERVER_TYPE));
    len += sizeof(CONN_IMPLICIT_SERVER_TYPE);
#ifdef HAVE_SETENV
    buf[len++] = '\0';
#else
    buf[len++] = '=';
#endif /*HAVE_SETENV*/
    strcpy(buf + len, typ);

    CORE_LOCK_WRITE;
#ifdef HAVE_SETENV
    len = !setenv(buf, buf + len, 1/*overwrite*/);
#else
    /* NOTE that putenv() leaks memory if the environment is later replaced */
    len = !putenv(buf);
#endif /*HAVE_SETENV*/
    CORE_UNLOCK;

#ifndef HAVE_SETENV
    if (!len)
#endif /*!HAVE_SETENV*/
        free(buf);
    return len ? 1/*success*/ : 0/*failure*/;
}


#define SERV_MERGE(a, b)  a ## b

#define SERV_GET_IMPLICIT_SERVER_TYPE(version)                              \
    ESERV_Type SERV_MERGE(SERV_GetImplicitServerType, version)              \
        (const char* service)                                               \
    {                                                                       \
        ESERV_Type type;                                                    \
        const char *end;                                                    \
        char val[40];                                                       \
        /* Try to retrieve service-specific first, then global default */   \
        if (!SERV_MERGE(ConnNetInfo_GetValue, version)                      \
            (service, REG_CONN_IMPLICIT_SERVER_TYPE, val, sizeof(val), 0)   \
            ||  !*val  ||  !(end = SERV_ReadType(val, &type))  ||  *end) {  \
            return SERV_GetImplicitServerTypeDefault();                     \
        }                                                                   \
        return type;                                                        \
    }                                                                       \
    

extern SERV_GET_IMPLICIT_SERVER_TYPE()


SERV_GET_IMPLICIT_SERVER_TYPE(Internal)


ESERV_Type SERV_GetImplicitServerTypeDefault(void)
{
    return fSERV_Http;
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
