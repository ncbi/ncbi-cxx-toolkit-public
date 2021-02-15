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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Auxiliary API, mostly CONN-, URL-, and MIME-related
 *   (see in "ncbi_connutil.h" for more details).
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_once.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_connutil.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Util


#define CONN_NET_INFO_MAGIC  0x600DCAFE

#define SizeOf(arr)  (sizeof(arr) / sizeof((arr)[0]))


static const char kDigits[] = "0123456789";


const STimeout g_NcbiDefConnTimeout = {
    (unsigned int)
      DEF_CONN_TIMEOUT,
    (unsigned int)
    ((DEF_CONN_TIMEOUT - (unsigned int) DEF_CONN_TIMEOUT) * 1.0e6)
};


static TNCBI_BigCount s_FWPorts[1024 / sizeof(TNCBI_BigCount)] = { 0 };


static int/*bool*/ x_StrcatCRLF(char** dstp, const char* src)
{
    char*  dst = *dstp;
    size_t dstlen = dst  &&  *dst ? strlen(dst) : 0;
    size_t srclen = src  &&  *src ? strlen(src) : 0;
    if (dstlen  &&  dst[dstlen - 1] == '\n') {
        if (--dstlen  &&  dst[dstlen - 1] == '\r')
            --dstlen;
    }
    while (srclen) {
        if (!isspace((unsigned char)(*src)))
            break;
        --srclen;
        ++src;
    }
    while (srclen) {
        if (!isspace((unsigned char) src[srclen - 1]))
            break;
        --srclen;
    }
    if (dstlen | srclen) {
        size_t len;
        char*  temp;
        len = (dstlen ? dstlen + 2 : 0) + (srclen ? srclen + 2 : 0) + 1;
        if (!(temp = (char*)(dst ? realloc(dst, len) : malloc (len))))
            return 0/*failure*/;
        dst = temp;
        if (dstlen) {
            temp += dstlen;
            memcpy(temp, "\r\n", 3);
            temp += 2;
        }
        if (srclen) {
            memcpy(temp, src, srclen);
            temp += srclen;
            memcpy(temp, "\r\n", 3);
        }
    }
    *dstp = dst;
    return 1/*success*/;
}


static const char* x_strncpy0(char* dst, const char* src, size_t dst_size)
{
    size_t src_len = strlen(src);
    const char* retval = dst;
    if (src_len >= dst_size) {
        src_len  = dst_size - 1;
        retval = 0;
    }
    strncpy0(dst, src, src_len);
    return retval;
}


static int/*bool*/ x_tr(char* str, char a, char b, size_t len)
{
    int/*bool*/ subst = 0/*false*/;
    char* end = str + len;
    while (str < end) {
        if (*str == a) {
            *str  = b;
            subst = 1/*true*/;
        }
        ++str;
    }
    return subst;
}


/* Last parameter "*generic":
 * [in]
 *    if "svclen" == 0: ignored;
 *    if "svclen" != 0, is a boolean "service_only" (if non-zero then no
 *    fallback to generic search in the "CONN_param" environment or the
 *    "[CONN]param" registry entry, gets performed);
 * [out]
 *    0 if the search ended up with a service-specific value returned (always
 *      so if the [in] value was non-zero);
 *    1 if the search ended up looking at the "CONN_param" enviroment or the
 *      "[CONN]param" registry entry (always so if "svclen" == 0);
 */
static const char* x_GetValue(const char* svc, size_t svclen,
                              const char* param, char* value,size_t value_size,
                              const char* def_value, int* /*bool*/ generic)
{
    const char* val, *rv;
    char        buf[128];
    size_t      parlen;
    char*       s;

    assert(!svclen  ||  (svclen == strlen(svc)));
    assert(value  &&  value_size  &&  !*value);
    assert(param  &&  *param);

    parlen = strlen(param) + 1;
    if (svclen) {
        /* Service-specific inquiry */
        size_t      len = svclen + 1 + parlen;
        char        tmp[sizeof(buf)];
        int/*bool*/ end, tr;

        if (strncasecmp(param, DEF_CONN_REG_SECTION "_",
                        sizeof(DEF_CONN_REG_SECTION)) != 0) {
            len += sizeof(DEF_CONN_REG_SECTION);
            end = 0/*false*/;
        } else
            end = 1/*true*/;
        if (len > sizeof(buf))
            return 0;

        /* First, search the environment for 'service_CONN_param' */
        s = (char*) memcpy(buf, svc, svclen) + svclen;
        tr = x_tr(buf, '-', '_', svclen);
        memcpy(tmp, buf, svclen);
        *s = '\0';
        strupr(buf);
        *s++ = '_';
        if (!end) {
            memcpy(s, DEF_CONN_REG_SECTION, sizeof(DEF_CONN_REG_SECTION) - 1);
            s += sizeof(DEF_CONN_REG_SECTION) - 1;
            *s++ = '_';
            end = *generic;
        }
        *generic = 0/*false*/;
        memcpy(s, param, parlen);
        strupr(s); /*FIXME: param all caps, so not needed*/
        CORE_LOCK_READ;
        if ((val = getenv(buf)) != 0
            ||  (memcmp(buf, tmp, svclen) != 0
                 &&  (val = getenv((char*) memcpy(buf, tmp, svclen))) != 0)) {
            rv = x_strncpy0(value, val, value_size);
            CORE_UNLOCK;
            return rv;
        }
        CORE_UNLOCK;
        assert(memcmp(buf, tmp, svclen) == 0);

        /* Next, search for 'CONN_param' in '[service]' registry section */
        if (tr)
            memcpy(buf, svc, svclen);  /* re-copy */
        buf[svclen++] = '\0';
        s = buf + svclen;
        rv = CORE_REG_GET(buf, s, value, value_size, end ? def_value : 0);
        if (*value  ||  end)
            return rv;
        assert(!*generic);
        *generic = 1/*true*/;
    } else {
        *generic = 1/*true*/;
        /* Common case. Form 'CONN_param' */
        if (strncasecmp(param, DEF_CONN_REG_SECTION "_",
                        sizeof(DEF_CONN_REG_SECTION)) != 0) {
            if (sizeof(DEF_CONN_REG_SECTION) + parlen > sizeof(buf))
                return 0;
            s = buf;
            memcpy(s, DEF_CONN_REG_SECTION, sizeof(DEF_CONN_REG_SECTION) - 1);
            s += sizeof(DEF_CONN_REG_SECTION) - 1;
            *s++ = '_';
        } else {
            if (parlen > sizeof(buf))
                return 0;
            s = buf;
        }
        memcpy(s, param, parlen);
        strupr(s); /*FIXME: param all caps, so not needed*/
        s = buf;
    }

    /* Environment search for 'CONN_param' */
    CORE_LOCK_READ;
    if ((val = getenv(s)) != 0) {
        rv = x_strncpy0(value, val, value_size);
        CORE_UNLOCK;
        return rv;
    }
    CORE_UNLOCK;

    /* Last resort: Search for 'param' in default registry section [CONN] */
    s += sizeof(DEF_CONN_REG_SECTION);
    return CORE_REG_GET(DEF_CONN_REG_SECTION, s, value, value_size, def_value);
}


static const char* s_GetValue(const char* svc, size_t svclen,
                              const char* param, char* value,size_t value_size,
                              const char* def_value, int* /*bool*/ generic)
{
    const char* retval = x_GetValue(svc, svclen, param,
                                    value, value_size, def_value, generic);
    assert(!retval  ||  retval == value);
    if (retval) {
        size_t len;
        /* strip enveloping quotes, if any */
        if (*value  &&  (len = strlen(value)) > 1
            &&  (*value == '"'  ||  *value == '\'')
            &&  value[--len] == *value) {
            if (--len)
                memmove(value, value + 1, len);
            value[len] = '\0';
        }
        assert(retval == value);
    }
    if (*value  ||  (retval  &&  *retval)) {
        CORE_TRACEF(("ConnNetInfo(%s%.*s%s%s=\"%s\"): %s%s%s", &"\""[!svclen],
                     (int) svclen, svc, svclen ? "\", " : "", param, value,
                     &"\""[!retval], retval ? retval : "NULL",
                     &"\""[!retval]));
    }
    return retval;
}


const char* ConnNetInfo_GetValueInternal(const char* service,const char* param,
                                         char* value, size_t value_size,
                                         const char* def_value)
{
    int/*bool*/ service_only = 0/*false*/;
    assert(!service  ||  !strpbrk(service, "?*["));
    assert(value  &&  value_size  &&  param  &&  *param);
    *value = '\0';
    return s_GetValue(service, service  &&  *service ? strlen(service) : 0,
                      param, value, value_size, def_value, &service_only);
}


/* No "CONN_param" environment or "[CONN]param" registry fallbacks */
const char* ConnNetInfo_GetValueService(const char* service, const char* param,
                                        char* value, size_t value_size,
                                        const char* def_value)
{
    int/*bool*/ service_only = 1/*true*/;
    const char* retval;

    assert(service  &&  *service  &&  !strpbrk(service, "?*["));
    assert(value  &&  value_size  &&  param  &&  *param);
    *value = '\0';
    retval = s_GetValue(service, strlen(service),
                        param, value, value_size, def_value, &service_only);
    assert(!service_only);
    return retval;
}


extern const char* ConnNetInfo_GetValue(const char* service, const char* param,
                                        char* value, size_t value_size,
                                        const char* def_value)
{
    int/*bool*/ service_only;
    const char* retval;
    size_t      svclen;

    if (!value  ||  !value_size)
        return 0;
    *value = '\0';
    if (!param  ||  !*param)
        return 0;

    if (service) {
        if (!*service  ||  strpbrk(service, "?*["))
            svclen = 0;
        else if (!(service = SERV_ServiceName(service)))
            return 0;
        else
            verify((svclen = strlen(service)) != 0);
    } else
        svclen = 0;

    service_only = 0/*false*/;
    retval = s_GetValue(service, svclen,
                        param, value, value_size, def_value, &service_only);
    if (svclen)
        free((void*) service);
    return retval;
}


extern int/*bool*/ ConnNetInfo_Boolean(const char* str)
{
    return str  &&  *str  &&  (strcmp    (str, "1")    == 0  ||
                               strcasecmp(str, "on")   == 0  ||
                               strcasecmp(str, "yes")  == 0  ||
                               strcasecmp(str, "true") == 0)
        ? 1/*true*/ : 0/*false*/;
}


static EURLScheme x_ParseScheme(const char* str, size_t len)
{
    assert(str  &&  (!str[len]  ||  str[len] == ':'));
    switch (len) {
    case 5:
        if (strncasecmp(str, "https", len) == 0)
            return eURL_Https;
        break;
    case 4:
        if (strncasecmp(str, "http",  len) == 0)
            return eURL_Http;
        if (strncasecmp(str, "file",  len) == 0)
            return eURL_File;
        break;
    case 3:
        if (strncasecmp(str, "ftp",   len) == 0)
            return eURL_Ftp;
        break;
    default:
        break;
    }
    return eURL_Unspec;
}


static EFWMode x_ParseFirewall(const char* str, int/*bool*/ generic)
{
    if (!*str) /*NB: not actually necessary but faster*/
        return eFWMode_Legacy;
    if (strcasecmp(str, "adaptive") == 0  ||  ConnNetInfo_Boolean(str))
        return eFWMode_Adaptive;
    if (strcasecmp(str, "firewall") == 0)
        return eFWMode_Firewall;
    if (strcasecmp(str, "fallback") == 0)
        return eFWMode_Fallback;
    for (;;) {
        int n;
        unsigned short port;
        if (sscanf(str, "%hu%n", &port, &n) < 1  ||  !port)
            break;
        if (generic)
            SERV_AddFirewallPort(port);
        str += n;
        if (!*(str += strspn(str, " \t")))
            return eFWMode_Fallback;
    }
    return eFWMode_Legacy;
}


/* Return -1 if nothing to do; 0 if failed; 1 if succeeded, */
static int/*tri-state*/ x_SetupHttpProxy(SConnNetInfo* info, const char* env)
{
    SConnNetInfo* x_info;
    const char* val;
    int parsed;

    assert(!info->http_proxy_host[0]  &&  !info->http_proxy_port);
    assert(env  &&  *env);
    CORE_LOCK_READ;
    if (!(val = getenv(env))  ||  !*val
        ||  strcmp(val, "''") == 0  ||  strcmp(val, "\"\"") == 0) {
        CORE_UNLOCK;
        return -1/*noop*/;
    }
    if (!(val = strdup(val))) {
        CORE_UNLOCK;
        return  0/*failure*/;
    }
    CORE_UNLOCK;
    if (!(x_info = ConnNetInfo_CloneInternal(info))) {
        free((void*) val);
        return  0/*fail*/;
    }
    if (*val == '"'  ||  *val == '\'') {
        /* strip enveloping quotes if any:  note that '' and ""  have already
         * been excluded, so the resulting string is always non-empty... */
        size_t len = strlen(val);
        assert(len);
        if (val[--len] == *val) {
            memmove((char*) val, val + 1, --len);
            ((char*) val)[len] = '\0';
            assert(len);
        }
    }
    x_info->req_method = eReqMethod_Any;
    x_info->scheme     = eURL_Unspec;
    x_info->user[0]    = '\0';
    x_info->pass[0]    = '\0';
    x_info->host[0]    = '\0';
    x_info->port       =   0;
    x_info->path[0]    = '\0';
    parsed = ConnNetInfo_ParseURL(x_info, val);
    assert(!(parsed & ~1)); /*0|1*/
    if (parsed  &&  (!x_info->scheme/*eURL_Unspec*/
                     ||  x_info->scheme == eURL_Http)
        &&  x_info->host[0]  &&  x_info->port
        &&  (!x_info->path[0]
             ||  (x_info->path[0] == '/'  &&  !x_info->path[1]))) {
        memcpy(info->http_proxy_user, x_info->user, strlen(x_info->user) + 1);
        memcpy(info->http_proxy_pass, x_info->pass, strlen(x_info->pass) + 1);
        memcpy(info->http_proxy_host, x_info->host, strlen(x_info->host) + 1);
        info->http_proxy_port = x_info->port;
        assert(!NCBI_HasSpaces(info->http_proxy_host,
                               strlen(info->http_proxy_host)));
        CORE_TRACEF(("ConnNetInfo(%s%s%s$%s): \"%s\"", &"\""[!*info->svc],
                     info->svc, *info->svc ? "\", " : "", env, val));
    } else {
        CORE_LOGF_X(10, info->http_proxy_leak ? eLOG_Warning : eLOG_Error,
                    ("ConnNetInfo(%s%s%s$%s): Unrecognized HTTP proxy"
                     " specification \"%s\"", &"\""[!*info->svc],
                     info->svc, *info->svc ? "\", " : "", env, val));
        parsed = info->http_proxy_leak ? -1/*noop*/ : 0/*fail*/;
    }
    ConnNetInfo_Destroy(x_info);
    free((void*) val);
    return parsed;
}


/* -1: not set; 0 error; 1 set okay */
static int/*tri-state*/ x_SetupSystemHttpProxy(SConnNetInfo* info)
{
    int rv;
    if (info->http_proxy_skip)
        return -1;
    rv = x_SetupHttpProxy(info, "http_proxy");
    return rv < 0 ? x_SetupHttpProxy(info, "HTTP_PROXY") : rv;
}


static void x_DestroyNetInfo(SConnNetInfo* info, unsigned int magic)
{
    assert(info);
    /* do not free() fields if there's a version skew (offsets maybe off) */
    if (magic == CONN_NET_INFO_MAGIC) {
        if (info->http_user_header) {
            free((void*) info->http_user_header);
            info->http_user_header = 0;
        }
        if (info->http_referer) {
            free((void*) info->http_referer);
            info->http_referer = 0;
        }
        info->magic++;
    }
    free(info);
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_InfoIsValid(const SConnNetInfo* info)
{
    if (!info)
        return 0/*false*/;
    assert(info->magic == CONN_NET_INFO_MAGIC);
    return info->magic == CONN_NET_INFO_MAGIC ? 1/*true*/ : 0/*false*/;
}



/****************************************************************************
 * ConnNetInfo API
 */
/*fwdecl*/
static int/*bool*/ x_SetArgs(SConnNetInfo* info, const char* args);

SConnNetInfo* ConnNetInfo_CreateInternal(const char* service)
{
#define REG_VALUE(name, value, def_value)                               \
    generic = 0;                                                        \
    *value = '\0';                                                      \
    if (!s_GetValue(service, len,                                       \
                    name, value, sizeof(value), def_value, &generic))   \
        goto err/*memory or truncation error*/

    char str[(CONN_PATH_LEN + 1)/2];
    int/*bool*/ generic;
    SConnNetInfo* info;
    size_t len;
    long   val;
    double dbl;
    char*  e;

    assert(!service  ||  !strpbrk(service, "?*["));
    len = service  &&  *service ? strlen(service) : 0;

    /* NB: created *NOT* cleared up with all 0s */
    if (!(info = (SConnNetInfo*) malloc(sizeof(*info) + len)))
        return 0/*failure*/;
    info->magic            = 0;
    info->unused           = 0/*MBZ*/;
    info->reserved         = 0/*MBZ*/;
    info->http_referer     = 0;
    info->http_user_header = 0;

    /* store the service name, which this structure is being created for */
    memcpy((char*) info->svc, service ? service : "", len + 1);

    /* client host: default */
    info->client_host[0] = '\0';

    /* request method */
    REG_VALUE(REG_CONN_REQ_METHOD, str, DEF_CONN_REQ_METHOD);
    if (!*str  ||  strcasecmp(str, "ANY") == 0)
        info->req_method = eReqMethod_Any;
    else if (strcasecmp(str, "POST") == 0)
        info->req_method = eReqMethod_Post;
    else if (strcasecmp(str, "GET") == 0)
        info->req_method = eReqMethod_Get;
    else /* NB: HEAD, CONNECT, etc not allowed here */
        goto err;

    /* scheme */
    info->scheme = eURL_Unspec;

    /* external mode */
    REG_VALUE(REG_CONN_EXTERNAL, str, DEF_CONN_EXTERNAL);
    info->external = ConnNetInfo_Boolean(str) ? 1 : 0;
    
    /* firewall mode */
    REG_VALUE(REG_CONN_FIREWALL, str, DEF_CONN_FIREWALL);
    info->firewall = x_ParseFirewall(str, generic);

    /* stateless client */
    REG_VALUE(REG_CONN_STATELESS, str, DEF_CONN_STATELESS);
    info->stateless = ConnNetInfo_Boolean(str) ? 1 : 0;

    /* prohibit use of the local load balancer */
    REG_VALUE(REG_CONN_LB_DISABLE, str, DEF_CONN_LB_DISABLE);
    info->lb_disable = ConnNetInfo_Boolean(str) ? 1 : 0;

    /* HTTP version */
    REG_VALUE(REG_CONN_HTTP_VERSION, str, DEF_CONN_HTTP_VERSION);
    info->http_version = *str  &&  atoi(str) == 1 ? 1 : 0;

    /* level of debug printout */
    REG_VALUE(REG_CONN_DEBUG_PRINTOUT, str, DEF_CONN_DEBUG_PRINTOUT);
    if (ConnNetInfo_Boolean(str)
        ||    (*str  &&   strcasecmp(str, "some") == 0)) {
        info->debug_printout = eDebugPrintout_Some;
    } else if (*str  &&  (strcasecmp(str, "data") == 0  ||
                          strcasecmp(str, "all")  == 0)) {
        info->debug_printout = eDebugPrintout_Data;
    } else
        info->debug_printout = eDebugPrintout_None;

    /* push HTTP auth tags */
    REG_VALUE(REG_CONN_HTTP_PUSH_AUTH, str, DEF_CONN_HTTP_PUSH_AUTH);
    info->http_push_auth = ConnNetInfo_Boolean(str) ? 1 : 0;

    /* HTTP proxy leakout */
    REG_VALUE(REG_CONN_HTTP_PROXY_LEAK, str, DEF_CONN_HTTP_PROXY_LEAK);
    info->http_proxy_leak = ConnNetInfo_Boolean(str) ? 1 : 0;

    /* HTTP proxy skip */
    REG_VALUE(REG_CONN_HTTP_PROXY_SKIP, str, DEF_CONN_HTTP_PROXY_SKIP);
    info->http_proxy_skip = ConnNetInfo_Boolean(str) ? 1 : 0;

    /* HTTP proxy only for HTTP (loaded from "$http_proxy" */
    info->http_proxy_only = 0/*false*/;

    /* username */
    REG_VALUE(REG_CONN_USER, info->user, DEF_CONN_USER);

    /* password */
    REG_VALUE(REG_CONN_PASS, info->pass, DEF_CONN_PASS);

    /* hostname */
    REG_VALUE(REG_CONN_HOST, info->host, DEF_CONN_HOST);
    if (NCBI_HasSpaces(info->host, strlen(info->host))) {
        CORE_LOGF_X(11, eLOG_Error,
                    ("[ConnNetInfo_Create%s%s%s]  Invalid host specification"
                     " \"%s\"", *info->svc ? "(\"" : "",
                     info->svc, *info->svc ? "\")" : "", info->host));
        goto err;
    }

    /* port # */
    REG_VALUE(REG_CONN_PORT, str, DEF_CONN_PORT);
    errno = 0;
    if (*str  &&  (val = (long) strtoul(str, &e, 10)) > 0  &&  !errno
        &&  !*e  &&  val < (1 << 16)) {
        info->port = (unsigned short) val;
    } else
        info->port = 0/*default*/;

    /* path */
    REG_VALUE(REG_CONN_PATH, info->path, DEF_CONN_PATH);

    /* HTTP proxy server: set all blank for now */
    info->http_proxy_host[0] = '\0';
    info->http_proxy_port    =   0;
    info->http_proxy_user[0] = '\0';
    info->http_proxy_pass[0] = '\0';

    /* max. # of attempts to establish connection */
    REG_VALUE(REG_CONN_MAX_TRY, str, 0);
    val = atoi(str);
    info->max_try = (unsigned short)(val > 0 ? val : DEF_CONN_MAX_TRY);

    /* connection timeout */
    REG_VALUE(REG_CONN_TIMEOUT, str, 0);
    val = *str ? (long) strlen(str) : 0;
    if (val < 3  ||  8 < val
        ||  strncasecmp(str, "infinite", (size_t) val) != 0) {
        if (*str && (dbl = NCBI_simple_atof(str, &e)) >= 0.0 && !errno && !*e){
            info->tmo.sec      = (unsigned int)  dbl;
            info->tmo.usec     = (unsigned int)((dbl - info->tmo.sec) * 1.0e6);
            if (dbl  &&  !(info->tmo.sec | info->tmo.usec))
                info->tmo.usec = 1/*protect from underflow*/;
        } else
            info->tmo          = g_NcbiDefConnTimeout;
        info->timeout = &info->tmo;
    } else
        info->timeout = kInfiniteTimeout/*0*/;

    /* HTTP user header */
    REG_VALUE(REG_CONN_HTTP_USER_HEADER, str, DEF_CONN_HTTP_USER_HEADER);
    if (!x_StrcatCRLF((char**) &info->http_user_header, str))
        goto err;

    /* default referer ([in] "generic" irrelevant), all error(s) ignored */
    *str = '\0';
    s_GetValue(0, 0, REG_CONN_HTTP_REFERER, str, sizeof(str),
               DEF_CONN_HTTP_REFERER, &generic);
    assert(generic);
    if (*str)
        info->http_referer = strdup(str);

    /* credentials */
    info->credentials = 0;

    /* magic: the "info" is fully inited and ready to use */
    info->magic = CONN_NET_INFO_MAGIC;

    /* args */
    REG_VALUE(REG_CONN_ARGS, str, DEF_CONN_ARGS);
    if (!x_SetArgs(info, str))
        goto err;

    /* HTTP proxy from process environment */
    if (!(val = x_SetupSystemHttpProxy(info)))
        goto err;
    if (val < 0) {
        /* proxy wasn't defined; try loading it from the registry */
        assert(!info->http_proxy_host[0]  &&  !info->http_proxy_port);

        /* HTTP proxy from legacy settings */
        REG_VALUE(REG_CONN_HTTP_PROXY_HOST, info->http_proxy_host,
                  DEF_CONN_HTTP_PROXY_HOST);
        if (NCBI_HasSpaces(info->http_proxy_host,
                           strlen(info->http_proxy_host))){
            CORE_LOGF_X(12, info->http_proxy_leak ? eLOG_Warning : eLOG_Error,
                        ("[ConnNetInfo_Create%s%s%s]  Invalid HTTP proxy"
                         " host specification \"%s\"",
                         *info->svc ? "(\"" : "", info->svc,
                         *info->svc ? "\")" : "", info->http_proxy_host));
            if (!info->http_proxy_leak)
                goto err;
            info->http_proxy_host[0] = '\0';
        }
        if (info->http_proxy_host[0]) {
            REG_VALUE(REG_CONN_HTTP_PROXY_PORT, str, DEF_CONN_HTTP_PROXY_PORT);
            errno = 0;
            if (*str  &&  (val = (long) strtoul(str, &e, 10)) > 0
                &&  !errno  &&  !*e  &&  val < (1 << 16)) {
                info->http_proxy_port = (unsigned short) val;
            } else
                info->http_proxy_port = 0/*none*/;
            /* HTTP proxy username */
            REG_VALUE(REG_CONN_HTTP_PROXY_USER, info->http_proxy_user,
                      DEF_CONN_HTTP_PROXY_USER);
            /* HTTP proxy password */
            REG_VALUE(REG_CONN_HTTP_PROXY_PASS, info->http_proxy_pass,
                      DEF_CONN_HTTP_PROXY_PASS);
        }
    } else
        info->http_proxy_only = 1/*true*/;
    return info;

 err:
    x_DestroyNetInfo(info, CONN_NET_INFO_MAGIC);
    return 0;
#undef REG_VALUE
}


extern SConnNetInfo* ConnNetInfo_Create(const char* service)
{
    const char* x_service;
    SConnNetInfo* retval;

    if (service  &&  *service  &&  !strpbrk(service, "?*[")) {
        if (!(x_service = SERV_ServiceName(service)))
            return 0;
        assert(*x_service);
    } else
        x_service = 0;

    retval = ConnNetInfo_CreateInternal(x_service);

    if (x_service)
        free((void*) x_service);
    return retval;
}


extern int/*bool*/ ConnNetInfo_SetUserHeader(SConnNetInfo* info,
                                             const char*   user_header)
{
    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (info->http_user_header) {
        free((void*) info->http_user_header);
        info->http_user_header = 0;
    }
    return x_StrcatCRLF((char**) &info->http_user_header, user_header);
}


extern int/*bool*/ ConnNetInfo_AppendUserHeader(SConnNetInfo* info,
                                                const char*   user_header)
{
    if (!s_InfoIsValid(info))
        return 0/*failure*/;
    return x_StrcatCRLF((char**) &info->http_user_header, user_header);
}


static int/*bool*/ x_TagValueMatches(const char* oldval, size_t oldvallen,
                                     const char* newval, size_t newvallen)
{
    assert(newvallen > 0);
    while (oldvallen > 0) {
        do {
            if (!isspace((unsigned char)(*oldval)))
                break;
            ++oldval;
        } while (--oldvallen > 0);
        if (oldvallen < newvallen)
            break;
        if (strncasecmp(oldval, newval, newvallen) == 0
            &&  (oldvallen == newvallen
                 ||  isspace((unsigned char) oldval[newvallen]))) {
            return 1/*true*/;
        }
        assert(oldvallen > 0);
        do {
            if ( isspace((unsigned char)(*oldval)))
                break;
            ++oldval;
        } while (--oldvallen > 0);
    }
    return 0/*false*/;
}


enum EUserHeaderOp {
    eUserHeaderOp_Delete,
    eUserHeaderOp_Extend,
    eUserHeaderOp_Override
};


static int/*bool*/ s_ModifyUserHeader(SConnNetInfo*      info,
                                      const char*        user_header,
                                      enum EUserHeaderOp op)
{
    int/*bool*/ retval;
    size_t newlinelen;
    size_t newhdrlen;
    char*  newline;
    char*  newhdr;
    size_t hdrlen;
    char*  hdr;

    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (!user_header  ||  !(newhdrlen = strlen(user_header)))
        return 1/*success*/;

    if (!(hdr = (char*) info->http_user_header)  ||  !(hdrlen = strlen(hdr))) {
        if (op == eUserHeaderOp_Delete)
            return 1/*success*/;
        if (!hdr  &&  !(hdr = strdup("")))
            return 0/*failure*/;
        hdrlen = 0;
    }
    assert(hdr);

    /* NB: "user_header" can be part of "info->user_header",
     * so create a copy of it even for delete operations! */
    if (!(newhdr = (char*) malloc(newhdrlen + 1))) {
        retval = 0/*failure*/;
        goto out;
    }
    memcpy(newhdr, user_header, newhdrlen + 1);

    retval = 1/*assume best: success*/;
    for (newline = newhdr;  *newline;  newline += newlinelen) {
        char*  eol = strchr(newline, '\n');
        char*  eot = strchr(newline,  ':');
        size_t newtaglen;
        char*  newtagval;
        size_t linelen;
        size_t newlen;
        char*  line;
        size_t off;

        /* line & taglen */
        newlinelen = (size_t)
            (eol ? eol - newline + 1 : newhdr + newhdrlen - newline);
        if (!eot  ||  eot >= newline + newlinelen)
            goto ignore;
        if (!(newtaglen = (size_t)(eot - newline)))
            goto ignore;

        /* tag value */
        newtagval = newline + newtaglen;
        while (++newtagval < newline + newlinelen) {
            if (!isspace((unsigned char)(*newtagval)))
                break;
        }
        switch (op) {
        case eUserHeaderOp_Override:
            newlen = newtagval < newline + newlinelen ? newlinelen : 0;
            break;
        case eUserHeaderOp_Delete:
            newlen = 0;
            break;
        case eUserHeaderOp_Extend:
            /* NB: how much additional space is required */
            if (!(newlen = newlinelen - (size_t)(newtagval - newline)))
                goto ignore;
            break;
        default:
            retval = 0/*failure*/;
            assert(0);
            goto out;
        }
        if (newlen  &&  eol) {
            if (eol[-1] == '\r')
                newlen -= 2;
            else
                newlen--;
            assert(newlen);
        }

        for (line = hdr;  *line;  line += linelen) {
            size_t taglen;
            char*  temp;
            size_t len;

            eol = strchr(line, '\n');
            eot = strchr(line,  ':');

            linelen = (size_t)(eol ? eol - line + 1 : hdr + hdrlen - line);
            if (!eot  ||  eot >= line + linelen)
                continue;

            taglen = (size_t)(eot - line);
            if (newtaglen != taglen || strncasecmp(newline, line, taglen) != 0)
                continue;
            assert(0 < taglen  &&  taglen <= linelen);

            if (newlen) {
                assert(op != eUserHeaderOp_Delete);
                off = !eol ? 0 : eol[-1] != '\r' ? 1 : 2;
                if (op == eUserHeaderOp_Extend) {
                    assert(line[taglen] == ':');
                    taglen++;
                    if (x_TagValueMatches(line + taglen, linelen-off - taglen,
                                          newtagval, newlen)) {
                        goto ignore;
                    }
                    line += linelen - off;
                    linelen = off;
                    newlen++;
                    len = 0;
                } else
                    len = linelen - off;
            } else
                len = 0/*==newlen*/;

            off  = (size_t)(line - hdr);
            if (len < newlen) {
                len = newlen - len;
                if (!(temp = (char*) realloc(hdr, hdrlen + len + 1))) {
                    retval = 0/*failure*/;
                    goto ignore;
                }
                hdr  = temp;
                line = temp + off;
                memmove(line + len, line, hdrlen - off + 1);
                hdrlen  += len;
                linelen += len;
                if (op == eUserHeaderOp_Extend) {
                    memcpy(line + 1, newtagval, newlen - 1);
                    *line = ' ';
                    newlen = 0;
                    break;
                }
            } else if (len > newlen) {
                assert(op == eUserHeaderOp_Override);
                hdrlen -= len;
                memmove(line + newlen, line + len, hdrlen - off + 1);
                hdrlen += newlen;
            }
            if (newlen) {
                assert(op == eUserHeaderOp_Override);
                memcpy(line, newline, newlen);
                newlen = 0;
                continue;
            }

            hdrlen -= linelen;
            memmove(line, line + linelen, hdrlen - off + 1);
            linelen = 0;
        }

        if (!newlen) {
        ignore:
            if (op == eUserHeaderOp_Delete)
                continue;
            off = (size_t)(newline - newhdr);
            newhdrlen -= newlinelen;
            memmove(newline, newline + newlinelen, newhdrlen - off + 1);
            newlinelen = 0;
        }
    }

 out:
    if (!*hdr) {
        assert(!hdrlen);
        free(hdr);
        hdr = 0;
    }
    info->http_user_header = hdr;
    if (retval  &&  op != eUserHeaderOp_Delete)
        retval = ConnNetInfo_AppendUserHeader(info, newhdr);
    if (newhdr)
        free(newhdr);

    return retval;
}


extern int/*bool*/ ConnNetInfo_OverrideUserHeader(SConnNetInfo* info,
                                                  const char*   header)
{
    return s_ModifyUserHeader(info, header, eUserHeaderOp_Override);
}


extern void ConnNetInfo_DeleteUserHeader(SConnNetInfo* info,
                                         const char*   header)
{
    verify(s_ModifyUserHeader(info, header, eUserHeaderOp_Delete));
}


extern int/*bool*/ ConnNetInfo_ExtendUserHeader(SConnNetInfo* info,
                                                const char*   header)
{
    return s_ModifyUserHeader(info, header, eUserHeaderOp_Extend);
}


extern int/*bool*/ ConnNetInfo_ParseURL(SConnNetInfo* info, const char* url)
{
    /* URL elements and their parsed lengths as passed */
    const char *user,   *pass,   *host,   *path,   *args;
    size_t     userlen, passlen, hostlen, pathlen, argslen;
    EURLScheme scheme;
    const char* s;
    size_t len;
    long port;
    char* p;

    if (!s_InfoIsValid(info)  ||  !url)
        return 0/*failure*/;

    if (!*url)
        return 1/*success*/;

    if ((info->req_method & ~eReqMethod_v1) == eReqMethod_Connect) {
        len = strlen(url);
        s = (const char*) memchr(url, ':', len);
        if (s) {
            if (!isdigit((unsigned char) s[1])  ||  s[1] == '0')
                return 0/*failure*/;
            len  = (size_t)(s - url);
        }
        if (len >= sizeof(info->host))
            return 0/*failure*/;
        if (NCBI_HasSpaces(url, len))
            return 0/*false*/;
        if (s) {
            errno = 0;
            port = strtol(++s, &p, 10);
            if (errno  ||  s == p  ||  *p)
                return 0/*failure*/;
            if (!port  ||  port ^ (port & 0xFFFF))
                return 0/*failure*/;
            info->port = (unsigned short) port;
        }
        if (len) {
            memcpy(info->host, url, len);
            info->host[len] = '\0';
        }
        return 1/*success*/;
    }

    /* "scheme://user:pass@host:port" first [any optional] */
    if ((s = strstr(url, "//")) != 0) {
        /* Authority portion present */
        port = -1L/*unassigned*/;
        if (s != url) {
            if (*--s != ':')
                return 0/*failure*/;
            len = (size_t)(s++ - url);
            if ((scheme = x_ParseScheme(url, len)) == eURL_Unspec)
                return 0/*failure*/;
        } else
            scheme = (EURLScheme) info->scheme;
        host    = s + 2/*"//"*/;
        hostlen = strcspn(host, "/?#");
        if (NCBI_HasSpaces(host, hostlen))
            return 0/*failure*/;
        path    = host + hostlen;

        /* username:password */
        if (!hostlen) {
            user    = pass    = host = (scheme == eURL_File ? "" : 0);
            userlen = passlen = 0;
        } else {
            if (!(s = (const char*) memrchr(host, '@', hostlen))) {
                user    = pass    = "";
                userlen = passlen = 0;
            } else {
                user    = host;
                userlen = (size_t)(s - user);
                host    = ++s;
                hostlen = (size_t)(path - s);
                if (!hostlen)
                    return 0/*failure*/;
                if (!(s = (const char*) memchr(user, ':', userlen))) {
                    pass    = "";
                    passlen = 0;
                } else {
                    userlen = (size_t)(s++ - user);
                    pass    = s++;
                    passlen = (size_t)(host - s);
                }
            }

            /* port, if any */
            if ((s = (const char*) memchr(host, ':', hostlen)) != 0) {
                hostlen = (size_t)(s - host);
                if (!hostlen  ||  !isdigit((unsigned char)s[1])  ||  s[1]=='0')
                    return 0/*failure*/;
                errno = 0;
                port = strtol(++s, &p, 10);
                if (errno  ||  s == p  ||  p != path)
                    return 0/*failure*/;
                if (!port  ||  port ^ (port & 0xFFFF))
                    return 0/*failure*/;
            } else
                port = 0/*default*/;

            if (userlen >= sizeof(info->user)  ||
                passlen >= sizeof(info->pass)  ||
                hostlen >= sizeof(info->host)) {
                return 0/*failure*/;
            }
        }
    } else {
        /* Authority portion not present */
        user    = pass    = 0;
        userlen = passlen = 0;
        s = (const char*) strchr(url, ':');
        if (s  &&  s != url  &&
            (scheme = x_ParseScheme(url, (size_t)(s - url))) != eURL_Unspec) {
            url = ++s;
            s = 0;
        } else
            scheme  = (EURLScheme) info->scheme;
        /* Check for special case: host:port[/path[...]] (see CXX-11455) */
        if (s  &&  s != url
            &&  *++s != '0'  &&  (hostlen/*NB:portlen*/ = strspn(s, kDigits))
            &&  memchr("/?#", s[hostlen/*NB:portlen*/], 4)
            &&  (errno = 0, (port = strtol(s, &p, 10)) != 0)  &&  !errno
            &&  p == s + hostlen  &&  !(port ^ (port & 0xFFFF))
            &&  !NCBI_HasSpaces(url, hostlen = (size_t)(--s - url))) {
            host = url;
            path = p;
        } else {
            port = -1L/*unassigned*/;
            hostlen = 0;
            host = 0;
            path = url;
        }
    }

    pathlen = (scheme == eURL_Https  ||  scheme == eURL_Http
               ? strcspn(path, "?#") : strlen(path));
    args    = path + pathlen;

    if ((!pathlen  &&  !*args)  ||  (pathlen  &&  *path == '/')) {
        /* absolute path */
        p = info->path;
        len = 0;
        if (!pathlen) {
            path    = "/";
            pathlen = 1;
        }
    } else {
        /* relative path */
        len = (scheme == eURL_Https  ||  scheme == eURL_Http
               ? strcspn(info->path, "?#") : strlen(info->path));
        if (!pathlen) {
            p = info->path + len;
            path = 0;
        } else if (!(p = (char*) memrchr(info->path, '/', len))) {
            p = info->path;
            len = 0;
        } else
            len = (size_t)(++p - info->path);
    }
    if (len + pathlen >= sizeof(info->path))
        return 0/*failure*/;

    /* arguments and fragment */
    if (*args) {
        const char* frag;
        assert(scheme == eURL_Https  ||  scheme == eURL_Http);
        argslen = strlen(args);
        if (*args == '#')
            frag = args;
        else if (!(frag = strchr(args + 1/*NB: args[0]=='?'*/, '#')))
            frag = args + argslen;
        assert(!*frag  ||  *frag == '#');

        if (*frag) {
            /* if there is a new fragment, the entire args get overridden */
            if (!frag[1])
                --argslen; /* although, don't store the empty fragment # */
            if ((size_t)(p - info->path)
                + pathlen + argslen >= sizeof(info->path)) {
                return 0/*failure*/;
            }
            len = 0;
        } else if ((frag = strchr(info->path, '#')) != 0) {
            /* there is no new fragment, but there was the old one: keep it */
            len = strlen(frag);
            if ((size_t)(p - info->path)
                + pathlen + argslen + len >= sizeof(info->path)) {
                return 0/*failure*/;
            }
            memmove(p + pathlen + argslen, frag, len);
        } else {
            if ((size_t)(p - info->path)
                + pathlen + argslen >= sizeof(info->path)) {
                return 0/*failure*/;
            }
            len = 0;
        }
        memcpy(p + pathlen, args, argslen);
        p[pathlen + argslen + len] = '\0';
    } else if ((scheme == eURL_Https  ||  scheme == eURL_Http)
               &&  (args = strchr(info->path, '#'))) {
        /* keep the old fragment, if any, but drop all args */
        memmove(p + pathlen, args, strlen(args) + 1/*EOL*/);
    } else
        p[pathlen] = '\0';
    if (path)
        memcpy(p, path, pathlen);
    if (user) {
        assert(pass);
        memcpy(info->user, user, userlen);
        info->user[userlen] = '\0';
        memcpy(info->pass, pass, passlen);
        info->pass[passlen] = '\0';
    }
    if (port >= 0  ||  scheme == eURL_File)
        info->port = (unsigned short)(port < 0 ? 0 : port);
    if (host) {
        assert(!NCBI_HasSpaces(host, hostlen));
        memcpy(info->host, host, hostlen);
        info->host[hostlen] = '\0';
    }
    info->scheme = scheme;
    return 1/*success*/;
}


static const char* x_SepAndLen(const char* str, const char* sep, size_t* len)
{
    size_t m = 0;
    while (*sep) {
        size_t n = strcspn(str, sep);
        if (!str[n]) {
            *len = m + n;
            return sep;
        }
        sep += (size_t)(strchr(sep, str[n++]) - sep) + 1;
        str += n;
        m   += n;
    }
    *len = m + strlen(str);
    return sep/*NB:""*/;
}


extern int/*bool*/ ConnNetInfo_SetPath(SConnNetInfo* info, const char* path)
{
    size_t plen, x_alen;
    const char* frag;
    const char* sep;
    char* x_args;

    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (!path) {
        info->path[0] = '\0';
        return 1/*success: all deleted*/;
    }

    sep = x_SepAndLen(path, "?#", &plen);

    x_args = info->path + strcspn(info->path, sep);

    if (!plen) {
        if (!*x_args)
            info->path[0] = '\0';
        else if (x_args != info->path)
            memmove(info->path, x_args, strlen(x_args) + 1/*EOL*/);
        return 1/*success: deleted*/;
    }

    x_alen = strlen(x_args);
    if ((frag = (const char*) memchr(path, '#', plen)) != 0  &&  !frag[1])
        --plen;
    if (plen + x_alen >= sizeof(info->path))
        return 0/*failure: too long*/;

    memmove(info->path + plen, x_args, x_alen + 1/*EOL*/);
    memcpy(info->path, path, plen);
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_AddPath(SConnNetInfo* info, const char* path)
{
    size_t plen, x_alen;
    const char* sep;
    char* x_path;
    char* x_args;

    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (!path  ||  !*path)
        return 1/*success: nothing to do*/;

    sep = x_SepAndLen(path, "?#", &plen);
    assert(plen);

    x_args = info->path + strcspn(info->path, sep);
    x_alen = strlen(x_args);

    if (*path == '?'  ||  *path == '#') {
        x_path = (char*) memchr (info->path,
                                 *path, (size_t)(x_args - info->path));
        if (!x_path)
            x_path = x_args;
        if (*path == '#'  &&  !path[1])
            --plen;
    } else if (*path != '/') {
        x_path = (char*) memrchr(info->path,
                                 '/',   (size_t)(x_args - info->path));
        if (!x_path)
            x_path = info->path;
        else
            x_path++;
    } else {
        x_path = info->path + strcspn(info->path, "?#");
        if (x_path != info->path  &&  x_path[-1] == '/')
            x_path--;
    }
    if ((size_t)(x_path - info->path) + plen + x_alen >= sizeof(info->path))
        return 0/*failure: too long*/;

    memmove(x_path + plen, x_args, x_alen + 1/*EOL*/);
    memcpy(x_path, path, plen);
    return 1/*success*/;
}


static int/*bool*/ x_SetArgs(SConnNetInfo* info, const char* args)
{
    size_t alen, x_flen, room;
    const char *x_frag;
    const char* frag;
    char* x_args;

    assert(s_InfoIsValid(info));

    alen = args ? strlen(args) : 0;

    x_args = info->path + strcspn(info->path, "?#");
    if (!alen) {
        if (!args) {
            *x_args = '\0';
            return 1/*success: all deleted*/;
        }
        if (*x_args == '?') {
            x_frag = (x_args + 1) + strcspn(x_args + 1, "#");
            if (!*x_frag)
                *x_args = '\0';
            else
                memmove(x_args, x_frag, strlen(x_frag) + 1/*EOL*/);
        }
        return 1/*success: deleted*/;
    }

    if (!(frag = (const char*) memchr(args, '#', alen))) {
        /* preserve frag, if any */
        x_frag = x_args + strcspn(x_args, "#");
        x_flen = strlen(x_frag);
    } else {
        if (!frag[1])
            --alen;
        x_frag = ""; /*NB: unused*/
        x_flen = 0;
    }
    room = !(*args == '#') + alen;
    if ((size_t)(x_args - info->path) + room + x_flen >= sizeof(info->path))
        return 0/*failure: too long*/;

    if (x_flen)
        memmove(x_args + room, x_frag, x_flen + 1/*EOL*/);
    if (!(*args == '#'))
        *x_args++ = '?';
    memcpy(x_args, args, alen);
    if (!x_flen)
        x_args[alen] = '\0';
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_SetArgs(SConnNetInfo* info, const char* args)
{
    return s_InfoIsValid(info) ? x_SetArgs(info, args) : 0/*failure*/;
}


extern int/*bool*/ ConnNetInfo_SetFrag(SConnNetInfo* info, const char* frag)
{
    char*  x_frag;
    size_t flen;

    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    flen = frag ? strlen(frag += !(*frag != '#')) : 0;

    x_frag = info->path + strcspn(info->path, "#");
    if (!flen) {
        *x_frag = '\0';
        return 1/*success: deleted*/;
    }
    ++flen;
    if ((size_t)(x_frag - info->path) + flen >= sizeof(info->path))
        return 0/*failure: too long*/;

    *x_frag++ = '#';
    memcpy(x_frag, frag, flen/*EOL incl'd*/);
    return 1/*success*/;
}


extern const char* ConnNetInfo_GetArgs(const SConnNetInfo* info)
{
    const char* args;

    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    args = info->path + strcspn(info->path, "?#");
    if (*args == '?')
        ++args;
    return args;
}


static int/*bool*/ x_AppendArg(SConnNetInfo* info,
                               const char*   arg,
                               const char*   val)
{
    size_t alen, vlen, x_alen, x_flen, room;
    char*  x_args, *x_frag;

    assert(s_InfoIsValid(info));

    if (!arg  ||  !(alen = strcspn(arg, "#")))
        return 1/*success*/;
    vlen = val ? 1/*'='*/ + strcspn(val, "#") : 0;

    x_args = info->path + strcspn(info->path, "?#");
    x_alen = strlen(x_args);

    if (*x_args == '?') {
        x_frag  = (x_args + 1) + strcspn(x_args + 1, "#");
        x_flen  = x_alen - (size_t)(x_frag - x_args);
        x_alen -= x_flen;
    } else {
        x_frag  = x_args;
        x_flen  = x_alen;
        x_alen  = 0;
    }
    room = (x_alen == 1 ? 0 : x_alen) + 1/*'?'|'&'*/ + (alen + vlen);
    if ((size_t)(x_args - info->path) + room + x_flen >= sizeof(info->path))
        return 0/*failure: too long*/;

    assert(!x_flen  ||  *x_frag == '#');
    if (x_flen)
        memmove(x_args + room, x_frag, x_flen + 1/*EOL*/);
    if (x_alen > 1) {
        assert(*x_args == '?');
        x_args += x_alen;
        *x_args++ = '&';
    } else
        *x_args++ = '?';
    memcpy(x_args, arg, alen);
    x_args += alen;
    if (vlen--) {
        *x_args++ = '=';
        memcpy(x_args, val, vlen);
        x_args += vlen;
    }
    if (!x_flen)
        *x_args = '\0';
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_AppendArg(SConnNetInfo* info,
                                         const char*   arg,
                                         const char*   val)
{
    return s_InfoIsValid(info) ? x_AppendArg(info, arg, val) : 0/*failure*/;
}


static int/*bool*/ x_PrependArg(SConnNetInfo* info,
                                const char*   arg,
                                const char*   val)
{
    size_t alen, vlen, x_alen, room;
    char*  x_args, *xx_args;

    assert(s_InfoIsValid(info));

    if (!arg  ||  !(alen = strcspn(arg, "#")))
        return 1/*success*/;
    vlen = val ? 1/*'='*/ + strcspn(val, "#") : 0;

    x_args = info->path + strcspn(info->path, "?#");
    x_alen = strlen(x_args);

    xx_args = x_args;
    if (*xx_args == '?'  &&  (!xx_args[1]  ||  xx_args[1] == '#')) {
        ++xx_args;
        --x_alen;
        room = 0;
    } else
        room = 1/*'?'|'&'*/;
    room += alen + vlen;
    if ((size_t)(x_args - info->path) + room + x_alen >= sizeof(info->path))
        return 0/*failure: too long*/;

    if (x_alen) {
        if (*xx_args == '?')
            *xx_args  = '&';
        memmove(xx_args + room, xx_args, x_alen + 1/*EOL*/);
    }
    *x_args++ = '?';
    memcpy(x_args, arg, alen);
    x_args += alen;
    if (vlen--) {
        *x_args++ = '=';
        memcpy(x_args, val, vlen);
        x_args += vlen;
    }
    if (!x_alen)
        *x_args = '\0';
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_PrependArg(SConnNetInfo* info,
                                          const char*   arg,
                                          const char*   val)
{
    return s_InfoIsValid(info) ? x_PrependArg(info, arg, val) : 0/*failure*/;
}


static int/*bool*/ x_DeleteArg(SConnNetInfo* info,
                               const char*   arg)
{
    int/*bool*/ deleted;
    size_t alen, x_alen;
    char*  x_args, *x_a;

    assert(s_InfoIsValid(info));

    if (!arg  ||  !(alen = strcspn(arg, "=&#")))
        return 0/*failure*/;

    deleted = 0/*false*/;
    x_args = info->path + strcspn(info->path, "?#");
    for (x_a = x_args;  *x_a  &&  *x_a != '#';  x_a += x_alen) {
        if (x_a == x_args  ||  *x_a == '&')
            ++x_a;
        x_alen = strcspn(x_a, "&#");
        if (x_alen < alen  ||  strncasecmp(x_a, arg, alen) != 0  ||
            (x_a[alen]  &&
             x_a[alen] != '='  &&  x_a[alen] != '&'  &&  x_a[alen] != '#')) {
            continue;
        }
        if (x_a[x_alen] == '&')
            ++x_alen;         /* inner arg: eat the following '&'      */
        else
            ++x_alen, --x_a;  /* last arg:  remove preceding '?' / '&' */
        memmove(x_a, x_a + x_alen, strlen(x_a + x_alen) + 1/*EOL*/);
        deleted = 1/*true*/;
        x_alen = 0;
    }
    return deleted;
}


extern int/*bool*/ ConnNetInfo_DeleteArg(SConnNetInfo* info,
                                         const char*   arg)
{
    return s_InfoIsValid(info) ? x_DeleteArg(info, arg) : 0/*failure*/;
}


static void x_DeleteAllArgs(SConnNetInfo* info,
                            const char*   args)
{
    assert(s_InfoIsValid(info)  &&  args  &&  *args);

    while (*args  &&  *args != '#') {
        size_t alen = strcspn(args, "&#");
        if (alen)
            x_DeleteArg(info, args);
        if (args[alen] == '&')
            ++args;
        args += alen;
    }
}


extern void ConnNetInfo_DeleteAllArgs(SConnNetInfo* info,
                                      const char*   args)
{
    if (s_InfoIsValid(info)  &&  args  &&  *args)
        x_DeleteAllArgs(info, args);
}
                    

extern int/*bool*/ ConnNetInfo_PreOverrideArg(SConnNetInfo* info,
                                              const char*   arg,
                                              const char*   val)
{
    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (!arg  ||  !*arg)
        return 1/*success*/;

    x_DeleteAllArgs(info, arg);
    return x_PrependArg(info, arg, val);
}


extern int/*bool*/ ConnNetInfo_PostOverrideArg(SConnNetInfo* info,
                                               const char*   arg,
                                               const char*   val)
{
    if (!s_InfoIsValid(info))
        return 0/*failure*/;

    if (!arg  ||  !*arg)
        return 1/*success*/;

    x_DeleteAllArgs(info, arg);
    return x_AppendArg(info, arg, val);
}


static int/*bool*/ x_IsSufficientAddress(const char* addr)
{
    const char* c;
    return !strchr(addr, ' ')
        &&  (SOCK_isip(addr)
             ||  ((c = strchr(addr,  '.')) != 0  &&  c[1]  &&
                  (c = strchr(c + 2, '.')) != 0  &&  c[1]));
}


static const char* x_ClientAddress(const char* client_host,
                                   int/*bool*/ local_host)
{
    const char* c = client_host;
    unsigned int ip;
    char addr[80];
    char* s;

    assert(client_host);
    strncpy0(addr, client_host, sizeof(addr) - 1);
    if (UTIL_NcbiLocalHostName(addr)  &&  (s = strdup(addr)) != 0)
        client_host = s;  /*NB: this usually makes client_host insufficient*/

    if ((client_host == c  &&  x_IsSufficientAddress(client_host))
        ||  !(ip = (*c  &&  !local_host
                    ? SOCK_gethostbyname(c)
                    : SOCK_GetLocalHostAddress(eDefault)))
        ||  SOCK_ntoa(ip, addr, sizeof(addr)) != 0
        ||  !(s = (char*) malloc(strlen(client_host) + strlen(addr) + 3))) {
        return client_host/*least we can do :-/*/;
    }

    sprintf(s, "%s(%s)", client_host, addr);
    if (client_host != c)
        free((void*) client_host);
    for (client_host = s;  *s;  ++s) {
        if (*s == ' ')
            *s  = '+';
    }
    return client_host;
}


extern int/*bool*/ ConnNetInfo_SetupStandardArgs(SConnNetInfo* info,
                                                 const char*   service)
{
    static const char kService[]  = "service";
    static const char kAddress[]  = "address";
    static const char kPlatform[] = "platform";
    int/*bool*/ local_host;
    const char* s;

    if (!s_InfoIsValid(info))
        return 0/*failed*/;

    s = CORE_GetAppName();
    if (s  &&  *s) {
        char ua[16 + 80];
        sprintf(ua, "User-Agent: %.80s", s);
        ConnNetInfo_ExtendUserHeader(info, ua);
    }

    /* Dispatcher CGI args (may sacrifice some if they don't fit altogether) */
    if (!(s = CORE_GetPlatform())  ||  !*s)
        ConnNetInfo_DeleteArg(info, kPlatform);
    else
        ConnNetInfo_PreOverrideArg(info, kPlatform, s);
    local_host = !info->client_host[0];
    if (local_host  &&
        !SOCK_gethostbyaddr(0, info->client_host, sizeof(info->client_host))) {
        SOCK_gethostname(info->client_host, sizeof(info->client_host));
    }
    if (!(s = x_ClientAddress(info->client_host, local_host))  ||  !*s)
        ConnNetInfo_DeleteArg(info, kAddress);
    else
        ConnNetInfo_PreOverrideArg(info, kAddress, s);
    if (s != info->client_host)
        free((void*) s);
    if (service) {
        if (!ConnNetInfo_PreOverrideArg(info, kService, service)) {
            ConnNetInfo_DeleteArg(info, kPlatform);
            if (!ConnNetInfo_PreOverrideArg(info, kService, service)) {
                ConnNetInfo_DeleteArg(info, kAddress);
                if (!ConnNetInfo_PreOverrideArg(info, kService, service))
                    return 0/*failed*/;
            }
        }
    }
    return 1/*succeeded*/;
}


SConnNetInfo* ConnNetInfo_CloneInternal(const SConnNetInfo* info)
{
    SConnNetInfo* x_info;
    size_t svclen;

    if (!s_InfoIsValid(info))
        return 0;

    svclen = strlen(info->svc);
    if (!(x_info = (SConnNetInfo*) malloc(sizeof(*info) + svclen)))
        return 0;

    strcpy(x_info->client_host,     info->client_host);
    x_info->req_method            = info->req_method;
    x_info->scheme                = info->scheme;
    x_info->external              = info->external;
    x_info->firewall              = info->firewall;
    x_info->stateless             = info->stateless;
    x_info->lb_disable            = info->lb_disable;
    x_info->http_version          = info->http_version;
    x_info->debug_printout        = info->debug_printout;
    x_info->http_push_auth        = info->http_push_auth;
    x_info->http_proxy_leak       = info->http_proxy_leak;
    x_info->http_proxy_skip       = info->http_proxy_skip;
    x_info->http_proxy_only       = info->http_proxy_only;
    x_info->reserved              = info->reserved;
    strcpy(x_info->user,            info->user);
    strcpy(x_info->pass,            info->pass);
    strcpy(x_info->host,            info->host);
    x_info->port                  = info->port;
    strcpy(x_info->path,            info->path);
    strcpy(x_info->http_proxy_host, info->http_proxy_host);
    x_info->http_proxy_port       = info->http_proxy_port;
    strcpy(x_info->http_proxy_user, info->http_proxy_user);
    strcpy(x_info->http_proxy_pass, info->http_proxy_pass);
    x_info->max_try               = info->max_try;
    x_info->unused                = info->unused;
    x_info->http_user_header      = 0;
    x_info->http_referer          = 0;
    x_info->credentials           = info->credentials;

    x_info->tmo                   = info->timeout ? *info->timeout : info->tmo;
    x_info->timeout               = info->timeout ? &x_info->tmo   : 0;
    memcpy((char*) x_info->svc,     info->svc, svclen + 1);

    x_info->magic                 = CONN_NET_INFO_MAGIC;
    return x_info;

}


extern SConnNetInfo* ConnNetInfo_Clone(const SConnNetInfo* info)
{
    SConnNetInfo* x_info = ConnNetInfo_CloneInternal(info);
    if (!x_info)
        return 0;
    assert(s_InfoIsValid(x_info));

    if (info->http_user_header  &&  *info->http_user_header
        &&  !(x_info->http_user_header = strdup(info->http_user_header))) {
        goto err;
    }
    if (info->http_referer  &&  *info->http_referer
        &&  !(x_info->http_referer     = strdup(info->http_referer))) {
        goto err;
    }
    return x_info;

 err:
    ConnNetInfo_Destroy(x_info);
    return 0;
}


static const char* x_BadMagic(unsigned int magic, char buf[])
{
    sprintf(buf, "0x%08lX (INVALID != 0x%08lX)",
            (unsigned long) magic, (unsigned long) CONN_NET_INFO_MAGIC);
    return buf;
}


static const char* x_Num(unsigned int num, char buf[])
{
    sprintf(buf, "(#%u)", num);
    return buf;
}


static const char* x_Scheme(EURLScheme scheme, char buf[])
{
    switch (scheme) {
    case eURL_Unspec:
        return "";
    case eURL_Https:
        return "HTTPS";
    case eURL_Http:
        return "HTTP";
    case eURL_File:
        return "FILE";
    case eURL_Ftp:
        return "FTP";
    default:
        break;
    }
    return buf ? x_Num(scheme, buf) : 0;
}


static const char* x_Port(unsigned short port, char buf[])
{
    assert(port);
    sprintf(buf, "%hu", port);
    return buf;
}


static const char* x_ReqMethod(TReqMethod req_method, char buf[])
{
    int/*bool*/ v1 = req_method & eReqMethod_v1 ? 1/*true*/ : 0/*false*/;
    req_method &= (TReqMethod)(~eReqMethod_v1);
    switch (req_method) {
    case eReqMethod_Any:
        return v1 ? "ANY/1.1"     : "ANY";
    case eReqMethod_Get:
        return v1 ? "GET/1.1"     : "GET";
    case eReqMethod_Post:
        return v1 ? "POST/1.1"    : "POST";
    case eReqMethod_Head:
        return v1 ? "HEAD/1.1"    : "HEAD";
    case eReqMethod_Connect:
        return v1 ? "CONNECT/1.1" : "CONNECT";
    case eReqMethod_Put:
        return "PUT";
    case eReqMethod_Patch:
        return "PATCH";
    case eReqMethod_Trace:
        return "TRACE";
    case eReqMethod_Delete:
        return "DELETE";
    case eReqMethod_Options:
        return "OPTIONS";
    default:
        break;
    }
    return buf ? x_Num(req_method, buf) : 0;
}


static const char* x_Firewall(unsigned int firewall)
{
    switch ((EFWMode) firewall) {
    case eFWMode_Adaptive:
        return "TRUE";
    case eFWMode_Firewall:
        return "FIREWALL";
    case eFWMode_Fallback:
        return "FALLBACK";
    default:
        assert(!firewall);
        break;
    }
    return "NONE";
}


static const char* x_CredInfo(NCBI_CRED cred, char buf[])
{
    unsigned int who, what;
    if (!cred)
        return "NULL";
    who  = (cred->type / 100) * 100;
    what =  cred->type % 100;
    switch (who) {
    case eNcbiCred_GnuTls:
        switch (what) {
        case 0:
            return "(GNUTLS X.509 Cert)";
        default:
            sprintf(buf, "(GNUTLS #%u)", what);
            return buf;
        }
    default:
        break;
    }
    return x_Num(cred->type, buf);
}


static void s_SaveStringQuot(char* s, const char* name,
                             const char* str, int/*bool*/ quote)
{
    sprintf(s + strlen(s), "%-16.16s: %s%s%s\n", name,
            str  &&  quote ? "\"" : "",
            str            ? str  : "NULL",
            str  &&  quote ? "\"" : "");
}

static void s_SaveString(char* s, const char* name, const char* str)
{
    s_SaveStringQuot(s, name, str, 1);
}

static void s_SaveKeyval(char* s, const char* name, const char* str)
{
    assert(str  &&  *str);
    s_SaveStringQuot(s, name, str, 0);
}

static void s_SaveBool(char* s, const char* name, unsigned int/*bool*/ bbb)
{
    s_SaveKeyval(s, name, bbb ? "TRUE" : "FALSE");
}

static void s_SaveULong(char* s, const char* name, unsigned long lll)
{
    sprintf(s + strlen(s), "%-16.16s: %lu\n", name, lll);
}

static void s_SaveUserHeader(char* s, const char* name,
                             const char* uh, size_t uhlen)
{
    s += strlen(s);
    s += sprintf(s, "%-16.16s: ", name);
    if (uh) {
        *s++ = '"';
        memcpy(UTIL_PrintableString(uh, uhlen, s, 0/*reduce*/), "\"\n", 3);
    } else
        memcpy(s, "NULL\n", 6);
}


extern void ConnNetInfo_Log(const SConnNetInfo* info, ELOG_Level sev, LOG lg)
{
    char   buf[80];
    size_t uhlen;
    size_t len;
    char*  s;

    if (!info) {
        LOG_Write(lg, NCBI_C_ERRCODE_X, 10, sev, 0, 0, 0, 0,
                  "ConnNetInfo_Log: NULL", 0, 0);
        return;
    }

    uhlen = info->http_user_header ? strlen(info->http_user_header) : 0;

    len = sizeof(*info) + 1024/*slack for all labels & keywords*/
        + UTIL_PrintableStringSize(info->http_user_header, uhlen)
        + (info->http_referer ? strlen(info->http_referer) : 0)
        + strlen(info->svc);

    if (!(s = (char*) malloc(len))) {
        LOG_WRITE(lg, NCBI_C_ERRCODE_X, 11,
                  sev == eLOG_Fatal ? eLOG_Fatal : eLOG_Error,
                  "ConnNetInfo_Log: Cannot allocate temporary buffer");
        return;
    }

    strcpy(s, "ConnNetInfo_Log\n"
           "#################### [BEGIN] SConnNetInfo:\n");
    if (!s_InfoIsValid(info))
        s_SaveKeyval(s, "magic",           x_BadMagic(info->magic, buf));
    if (*info->svc)
        s_SaveString(s, "service",         info->svc);
    else
        s_SaveKeyval(s, "service",         "NONE");
    if (*info->client_host)
        s_SaveString(s, "client_host",     info->client_host);
    else
        s_SaveKeyval(s, "client_host",     "(default)");
    s_SaveKeyval    (s, "req_method",      x_ReqMethod((TReqMethod)
                                                        (info->req_method
                                                         | (info->http_version
                                                            ? eReqMethod_v1
                                                            : 0)), buf));
    s_SaveKeyval    (s, "scheme",         (info->scheme
                                           ? x_Scheme((EURLScheme)
                                                      info->scheme, buf)
                                           : "(unspec)"));
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    s_SaveString    (s, "user",            info->user);
#else
    s_SaveKeyval    (s, "user",           *info->user ? "(set)" : "\"\"");
#endif /*_DEBUG && !NDEBUG*/
    if (*info->pass)
        s_SaveKeyval(s, "pass",           *info->user ? "(set)" : "(ignored)");
    else
        s_SaveString(s, "pass",            info->pass);
    s_SaveString    (s, "host",            info->host);
    s_SaveKeyval    (s, "port",           (info->port
                                           ? x_Port(info->port, buf)
                                           : *info->host
                                           ? "(default)"
                                           : "(none"));
    s_SaveString    (s, "path",            info->path);
    s_SaveString    (s, "http_proxy_host", info->http_proxy_host);
    s_SaveKeyval    (s, "http_proxy_port",(info->http_proxy_port
                                           ? x_Port(info->http_proxy_port, buf)
                                           : "(none)"));
#if defined(_DEBUG)  &&  !defined(NDEBUG)
    s_SaveString    (s, "http_proxy_user", info->http_proxy_user);
#else
    s_SaveKeyval    (s, "http_proxy_user",(info->http_proxy_user[0]
                                           ? "(set)" : "\"\""));
#endif /*_DEBUG && !NDEBUG*/
    if (*info->http_proxy_pass) {
        s_SaveKeyval(s, "http_proxy_pass",(info->http_proxy_user[0]
                                           ? "(set)" : "(ignored)"));
    } else
        s_SaveString(s, "http_proxy_pass", info->http_proxy_pass);
    s_SaveBool      (s, "http_proxy_leak", info->http_proxy_leak);
    s_SaveBool      (s, "http_proxy_skip", info->http_proxy_skip);
    s_SaveBool      (s, "http_proxy_only", info->http_proxy_only);
    s_SaveULong     (s, "max_try",         info->max_try);
    if (info->timeout) {
        s_SaveULong (s, "timeout(sec)",    info->timeout->sec);
        s_SaveULong (s, "timeout(usec)",   info->timeout->usec);
    } else
        s_SaveKeyval(s, "timeout",         "INFINITE");
    s_SaveBool      (s, "external",        info->external);
    s_SaveKeyval    (s, "firewall",        x_Firewall(info->firewall));
    s_SaveBool      (s, "stateless",       info->stateless);
    s_SaveBool      (s, "lb_disable",      info->lb_disable);
    s_SaveKeyval    (s, "debug_printout", (info->debug_printout
                                           == eDebugPrintout_None
                                           ? "NONE"
                                           : info->debug_printout
                                           == eDebugPrintout_Some
                                           ? "SOME"
                                           : info->debug_printout
                                           == eDebugPrintout_Data
                                           ? "DATA"
                                           : x_Num(info->debug_printout,buf)));
    s_SaveBool      (s, "http_push_auth",  info->http_push_auth);
    s_SaveUserHeader(s, "http_user_header",info->http_user_header, uhlen);
    s_SaveString    (s, "http_referer",    info->http_referer);
    if (info->credentials)
        s_SaveKeyval(s, "credentials",     x_CredInfo(info->credentials, buf));
    strcat(s, "#################### [END] SConnNetInfo\n");

    assert(strlen(s) < len);
    LOG_Write(lg, NCBI_C_ERRCODE_X, 12, sev, 0, 0, 0, 0, s, 0, 0);
    free(s);
}


extern char* ConnNetInfo_URL(const SConnNetInfo* info)
{
    TReqMethod  req_method;
    const char* scheme;
    size_t      schlen;
    const char* path;
    size_t      len;
    char*       url;

    if (!s_InfoIsValid(info))
        return 0/*failed*/;

    req_method = info->req_method & (TReqMethod)(~eReqMethod_v1);
    if (!(scheme = x_Scheme((EURLScheme) info->scheme, 0)))
        return 0/*failed*/;

    if (req_method == eReqMethod_Connect) {
        scheme = "";
        schlen = 0;
        path = 0;
        len = 0;
    } else {
        schlen = strlen(scheme);
        path = info->path;
        len = schlen + 4/*"://",'/'*/ + strlen(path);
    }
    len += strlen(info->host) + 7/*:port\0*/;

    url = (char*) malloc(len);
    if (url) {
        strlwr((char*) memcpy(url, scheme, schlen + 1));
        len = schlen;
        len += sprintf(url + len,
                       &"://%s"[schlen ? 0 : path ? 1 : 3], info->host);
        if (info->port  ||  !path/*req_method == eReqMethod_Connect*/)
            len += sprintf(url + len, ":%hu", info->port);
        sprintf(url + len,
                "%s%s", &"/"[!(path  &&  *path != '/')], path ? path : "");
    }
    assert(!url  ||  *url);
    return url;
}


extern int/*bool*/ ConnNetInfo_SetTimeout(SConnNetInfo*   info,
                                          const STimeout* timeout)
{
    if (!s_InfoIsValid(info)  ||  timeout == kDefaultTimeout)
        return 0/*failed*/;
    if (timeout) {
        info->tmo     = *timeout;
        info->timeout = &info->tmo;
    } else
        info->timeout = kInfiniteTimeout/*0,timeout*/;
    return 1/*succeeded*/;
}


extern void ConnNetInfo_Destroy(SConnNetInfo* info)
{
    if (info)
        x_DestroyNetInfo(info, info->magic);
}



/****************************************************************************
 * URL_Connect
 */


static EIO_Status x_URLConnectErrorReturn(SOCK sock, EIO_Status status)
{
    if (sock) {
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    return status;
}


extern EIO_Status URL_ConnectEx
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 TReqMethod      req_method,
 size_t          content_length,
 const STimeout* o_timeout,
 const STimeout* rw_timeout,
 const char*     user_hdr,
 NCBI_CRED       cred,
 TSOCK_Flags     flags,
 SOCK*           sock)
{
    static const char kHttp[][12] = { " HTTP/1.0\r\n",
                                      " HTTP/1.1\r\n" };
    SOCK        s;
    BUF         buf;
    char*       hdr;
    const char* str;  
    SSOCK_Init  init;
    const char* http;
    int/*bool*/ x_c_l;
    EIO_Status  status;
    size_t      hdr_len;
    size_t      path_len;
    size_t      args_len;
    char        temp[80];
    unsigned short x_port;
    EReqMethod  x_req_meth;
    size_t      user_hdr_len = user_hdr  &&  *user_hdr ? strlen(user_hdr) : 0;

    x_req_meth = (EReqMethod)(req_method & (TReqMethod)(~eReqMethod_v1));
    http = kHttp[!(req_method < eReqMethod_v1)];
    args_len = strcspn(path, "?#")/*temp!*/;
    path_len = path
        ? (x_req_meth != eReqMethod_Connect  &&  !args
           ? args_len
           : strlen(path))
        : 0;

    /* sanity checks */
    if (!sock  ||  !host  ||  !*host  ||  !path_len  ||  args_len < path_len) {
        CORE_LOG_X(2, eLOG_Critical, "[URL_Connect]  Bad argument(s)");
        if (sock) {
            s = *sock;
            *sock = 0;
        } else
            s = 0;
        return x_URLConnectErrorReturn(s, eIO_InvalidArg);
    }
    s = *sock;
    *sock = 0;

    if (path[path_len]/*NB: '?'/'#' && !args*/)
        args = &path[path_len] + !(path[path_len] != '?');

    /* trim user_hdr */
    while (user_hdr_len) {
        if (!isspace((unsigned char) user_hdr[0]))
            break;
        --user_hdr_len;
        ++user_hdr;
    }
    while (user_hdr_len) {
        if (!isspace((unsigned char) user_hdr[user_hdr_len - 1]))
            break;
        --user_hdr_len;
    }

    /* check C-L, select request method and its verbal representation */
    x_c_l = content_length  &&  content_length != (size_t)(-1L) ? 1 : 0;
    if (x_req_meth == eReqMethod_Any)
        x_req_meth  = content_length ? eReqMethod_Post : eReqMethod_Get;
    else if (x_c_l  &&  (x_req_meth == eReqMethod_Head  ||
                         x_req_meth == eReqMethod_Get)) {
        if (port)
            sprintf(temp, ":%hu", port);
        else
            *temp = '\0';
        CORE_LOGF_X(3, eLOG_Warning,
                    ("[URL_Connect; http%s://%s%s%s%.*s] "
                     " Content-Length (%lu) is ignored with request method %s",
                     &"s"[!(flags & fSOCK_Secure)], host, temp,
                     &"/"[path_len && *path == '/'], (int) path_len, path,
                     (unsigned long) content_length,
                     x_req_meth == eReqMethod_Get ? "GET" : "HEAD"));
        content_length  = (size_t)(-1L);
    }
    if (content_length != (size_t)(-1L)  &&  x_req_meth != eReqMethod_Connect){
        /* RFC7230 3.3.2 */
        x_c_l = content_length
            ||  x_req_meth == eReqMethod_Put
            ||  x_req_meth == eReqMethod_Post ? 1/*true*/ : 0/*false*/;
    } else
        x_c_l = 0/*false*/;

    if (!(str = x_ReqMethod(x_req_meth, 0))) {
        char tmp[40];
        if (port)
            sprintf(temp, ":%hu", port);
        else
            *temp = '\0';
        CORE_LOGF_X(4, eLOG_Error,
                    ("[URL_Connect; http%s://%s%s%s%.*s] "
                     " Unsupported request method %s",
                     &"s"[!(flags & fSOCK_Secure)], host, temp,
                     &"/"[path_len && *path == '/'], (int) path_len, path,
                     x_ReqMethod(req_method, tmp)));
        assert(0);
        return x_URLConnectErrorReturn(s, eIO_NotSupported);
    }

    x_port = port;
    if (x_req_meth != eReqMethod_Connect) {
        if (!x_port)
            x_port = flags & fSOCK_Secure ? CONN_PORT_HTTPS : CONN_PORT_HTTP;
        args_len = args ? strcspn(args, "#") : 0;
    } else
        args_len = 0;

    buf = 0;
    errno = 0;
    /* compose HTTP header */
    if (/* METHOD <path>[?<args>] HTTP/1.x\r\n */
        !BUF_Write(&buf, str,  strlen(str))                        ||
        !BUF_Write(&buf, " ",  1)                                  ||
        !BUF_Write(&buf, path, path_len)                           ||
        (args_len
         &&  (!BUF_Write(&buf, "?",  1)                            ||
              !BUF_Write(&buf, args, args_len)))                   ||
        !BUF_Write      (&buf, http, sizeof(kHttp[0]) - 1)         ||

        /* Content-Length: <content_length>\r\n */
        (x_c_l
         &&  !BUF_Write(&buf, temp, (size_t)
                        sprintf(temp, "Content-Length: %lu\r\n",
                                (unsigned long) content_length)))  ||

        /* <user_header> */
        (user_hdr_len
         &&  !BUF_Write(&buf, user_hdr, user_hdr_len))             ||

        /* header separator */
        !BUF_Write(&buf, "\r\n\r\n", user_hdr_len ? 4 : 2)         ||

        /* tunneled data */
        (x_req_meth == eReqMethod_Connect
         &&  content_length  &&  content_length != (size_t)(-1L)
         &&  !BUF_Write(&buf, args, content_length))) {
        int x_errno = errno;
        if (port)
            sprintf(temp, ":%hu", port);
        else
            *temp = '\0';
        CORE_LOGF_ERRNO_X(5, eLOG_Error, x_errno,
                          ("[URL_Connect; http%s://%s%s%s%.*s%s%.*s] "
                           " Cannot build HTTP header",
                           &"s"[!(flags & fSOCK_Secure)], host, temp,
                           &"/"[path_len && *path == '/'], (int)path_len, path,
                           &"?"[!args_len], (int) args_len, args));
        BUF_Destroy(buf);
        return x_URLConnectErrorReturn(s, eIO_Unknown);
    }

    if (!(hdr = (char*) malloc(hdr_len = BUF_Size(buf)))
        ||  BUF_Read(buf, hdr, hdr_len) != hdr_len) {
        int x_errno = errno;
        if (port)
            sprintf(temp, ":%hu", port);
        else
            *temp = '\0';
        CORE_LOGF_ERRNO_X(6, eLOG_Error, x_errno,
                          ("[URL_Connect; http%s://%s%s%s%.*s%s%.*s] "
                           " Cannot maintain HTTP header (%lu byte%s)",
                           &"s"[!(flags & fSOCK_Secure)], host, temp,
                           &"/"[path_len && *path == '/'], (int)path_len, path,
                           &"?"[!args_len], (int) args_len, args,
                           (unsigned long) hdr_len, &"s"[hdr_len == 1]));
        if (hdr)
            free(hdr);
        BUF_Destroy(buf);
        return x_URLConnectErrorReturn(s, eIO_Unknown);
    }
    BUF_Destroy(buf);

    memset(&init, 0, sizeof(init));
    init.data = hdr;
    init.size = hdr_len;
    init.cred = cred;

    if (s) {
        init.host = host;
        /* re-use existing connection */
        status = SOCK_CreateOnTopInternal(s/*old*/, 0, sock/*new*/,
                                          &init, flags);
        SOCK_Destroy(s);
    } else {
        /* connect to HTTPD */
        status = SOCK_CreateInternal(host, x_port, o_timeout, sock/*new*/,
                                     &init, flags);
        if (*sock)
            SOCK_DisableOSSendDelay(*sock, 1/*yes,disable*/);
    }
    free(hdr);

    if (status != eIO_Success) {
        char timeout[40];
        assert(!*sock);
        if (status == eIO_Timeout  &&  o_timeout) {
            sprintf(timeout, "[%u.%06u]",
                    (unsigned int)(o_timeout->sec + o_timeout->usec/1000000),
                    (unsigned int)                 (o_timeout->usec%1000000));
        } else
            *timeout = '\0';
        if (port)
            sprintf(temp, ":%hu", port);
        else
            *temp = '\0';
        CORE_LOGF_X(7, eLOG_Error,
                    ("[URL_Connect; http%s://%s%s%s%.*s%s%.*s] "
                     " Failed to %s: %s%s",
                     &"s"[!(flags & fSOCK_Secure)], host, temp,
                     &"/"[path_len && *path == '/'], (int) path_len, path,
                     &"?"[!args_len], (int) args_len, args,
                     s ? "use connection" : "connect",
                     IO_StatusStr(status), timeout));
    } else
        verify(SOCK_SetTimeout(*sock, eIO_ReadWrite, rw_timeout)==eIO_Success);
    return status;
}


extern SOCK URL_Connect
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 EReqMethod      req_method,
 size_t          content_length,
 const STimeout* o_timeout,
 const STimeout* rw_timeout,
 const char*     user_hdr,
 int/*bool*/     encode_args,
 TSOCK_Flags     flags)
{
    static const char kHost[] = "Host: ";
    const char* x_hdr = user_hdr;
    static void* s_Once = 0;
    char* x_args = 0;
    SOCK sock;

    if (!CORE_Once(&s_Once)) {
        CORE_LOG(eLOG_Warning, "[URL_Connect] "
                 " *DEPRECATED*!!!  DON'T USE IT!!  Update your code please!");
    }
    if (req_method >= eReqMethod_v1) {
        CORE_LOG_X(9, eLOG_Error,
                   "[URL_Connect]  Unsupported version of HTTP protocol");
        return 0;
    }

    if (req_method != eReqMethod_Connect) {
        size_t x_add = 1/*true*/;
        while (x_hdr  &&  *x_hdr) {
            if (x_hdr != user_hdr)
                x_hdr++;
            if (strncasecmp(x_hdr, kHost, sizeof(kHost) - 2) == 0) {
                x_add = 0/*false*/;
                break;
            }
            x_hdr = strchr(x_hdr, '\n');
        }
        if (x_add) {
            size_t x_len = host  &&  *host ? strlen(host) : 0; 
            char* x_host = x_len ? (char*) malloc(x_len + sizeof(kHost)+6) : 0;
            if (x_host) {
                memcpy(x_host, kHost, sizeof(kHost) - 1);
                memcpy(x_host + sizeof(kHost) - 1, host, x_len);
                x_len += sizeof(kHost) - 1;
                if (port)
                    sprintf(x_host + x_len, ":%hu", port);
                else
                    x_host[x_len] = '\0';
                if (!x_StrcatCRLF(&x_host, user_hdr)) {
                    x_hdr = user_hdr;
                    free(x_host);
                } else
                    x_hdr = x_host;
            } else
                x_hdr = user_hdr;
        } else
            x_hdr = user_hdr;

        if (args  &&  encode_args  &&  (x_add = strcspn(args, "#")) > 0) {
            /* URL-encode "args", if any specified */
            size_t size = 3 * x_add;
            size_t rd_len, wr_len;
            if (!(x_args = (char*) malloc(size + 1))) {
                CORE_LOGF_ERRNO_X(8, eLOG_Error, errno,
                                  ("[URL_Connect]  Out of memory (%lu)",
                                   (unsigned long)(size + 1)));
                if (x_hdr != user_hdr)
                    free((void*) x_hdr);
                return 0;
            }
            URL_Encode(args, x_add, &rd_len, x_args, size, &wr_len);
            assert(rd_len == x_add);
            assert(wr_len <= size);
            x_args[wr_len] = '\0';
            args = x_args;
        }
    }

    sock = 0;
    verify(URL_ConnectEx(host, port, path, args,
                         req_method, content_length,
                         o_timeout, rw_timeout,
                         x_hdr, 0/*cred*/, flags, &sock) == eIO_Success
           ||  !sock);

    if (x_args)
        free(x_args);
    if (x_hdr != user_hdr)
        free((void*) x_hdr);
    return sock;
}



/****************************************************************************
 * StripToPattern()
 */


typedef EIO_Status (*FDoIO)
(void*     stream,
 void*     buf,
 size_t    size,
 size_t*   n_read,
 EIO_Event what     /* eIO_Read | eIO_Write (to pushback) */
 );

static EIO_Status s_StripToPattern
(void*       stream,
 FDoIO       io_func,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded)
{
    char*      buf;
    size_t     n_read;
    size_t     buf_size;
    char       x_buf[4096];
    EIO_Status retval, status;

    /* check args */
    if ( n_discarded )
        *n_discarded = 0;
    if (!stream)
        return eIO_InvalidArg;

    if (!pattern_size) {
        pattern   = 0;
        buf_size  = sizeof(x_buf);
    } else
        buf_size  = pattern ? pattern_size << 1 : pattern_size;

    /* allocate a temporary read buffer */
    if (buf_size <= sizeof(x_buf)  &&  pattern) {
        buf_size  = sizeof(x_buf);
        buf = x_buf;
    } else if (!(buf = (char*) malloc(buf_size)))
        return eIO_Unknown;

    retval = eIO_Success;
    if (!pattern) {
        /* read/discard the specified # of bytes or until EOF */
        char* xx_buf = buf;
        for (;;) {
            status = io_func(stream, xx_buf, buf_size, &n_read, eIO_Read);
            assert(buf_size  &&  n_read <= buf_size);
            if (!n_read) {
                assert(status != eIO_Success);
                retval = status;
                break;
            }
            if (discard) {
                if (xx_buf == buf) {
                    /* enqueue the entire buffer when new */
                    if (BUF_AppendEx(discard, buf, buf_size, xx_buf, n_read))
                        buf = 0/*mark it not to be free()'d at the end*/;
                    else
                        retval = status = eIO_Unknown;
                } else /*NB:zero-copy,can't fail!*/
                    verify(BUF_Write(discard, xx_buf, n_read));
            }
            if ( n_discarded )
                *n_discarded += n_read;
            if (!(buf_size -= n_read)) {
                if (pattern_size)
                    break;
                if (status != eIO_Success) {
                    retval  = status;
                    break;
                }
                buf_size = sizeof(x_buf);
                if (!buf  &&  !(buf = (char*) malloc(buf_size))) {
                    retval  = eIO_Unknown;
                    break;
                }
                xx_buf  = buf;
                continue;
            }
            if (status != eIO_Success) {
                retval  = status;
                break;
            } else
                xx_buf += n_read;
        }
    } else {
        /* pattern search case */
        assert(pattern_size);
        --pattern_size;
        n_read = 0;
        for (;;) {
            /* read; search for the pattern; store/count the discarded data */
            size_t x_read, n_stored;

            assert(n_read <= pattern_size  &&  n_read < buf_size);
            status = io_func(stream, buf + n_read, buf_size - n_read,
                             &x_read, eIO_Read);
            assert(x_read <= buf_size - n_read);
            if (!x_read) {
                assert(status != eIO_Success);
                retval = status;
                break;
            }
            n_stored = n_read + x_read;

            if (n_stored > pattern_size) {
                /* search for the pattern */
                size_t n_check;
                const char* b = buf;
                for (n_check = n_stored - pattern_size;  n_check;  --n_check) {
                    if (*b++ != *((const char*)pattern))
                        continue;
                    if (!pattern_size)
                        break; /*found*/
                    if (memcmp(b, (const char*)pattern + 1, pattern_size) == 0)
                        break; /*found*/
                }
                if ( n_check ) {
                    /* pattern found */
                    size_t x_discarded = (size_t)(b - buf) + pattern_size;
                    if (discard  &&  !BUF_Write(discard, buf + n_read,
                                                x_discarded - n_read)) {
                        retval = status = eIO_Unknown;
                    }
                    if ( n_discarded )
                        *n_discarded += x_discarded - n_read;
                    /* return the unused portion to the stream */
                    status = io_func(stream, buf + x_discarded,
                                     n_stored - x_discarded, 0, eIO_Write);
                    if (retval == eIO_Success)
                        retval  = status;
                    break;
                }
            }

            /* pattern still not found */
            if ( n_discarded )
                *n_discarded += x_read;
            if (discard  &&  !BUF_Write(discard, buf + n_read, x_read)) {
                retval = eIO_Unknown;
                break;
            }
            if (status != eIO_Success) {
                retval  = status;
                break;
            }
            if (n_stored > pattern_size) {
                n_read   = pattern_size;
                memmove(buf, buf + n_stored - n_read, n_read);
            } else
                n_read   = n_stored;
        }
    }

    /* cleanup & exit */
    if (buf  &&  buf != x_buf)
        free(buf);
    return retval;
}


static EIO_Status s_CONN_IO
(void*     stream,
 void*     buf,
 size_t    size,
 size_t*   n_read,
 EIO_Event what)
{
    switch (what) {
    case eIO_Read:
        return CONN_Read((CONN) stream, buf, size, n_read, eIO_ReadPlain);
    case eIO_Write:
        return CONN_Pushback((CONN) stream, buf, size);
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (conn, s_CONN_IO, pattern, pattern_size, discard, n_discarded);
}


static EIO_Status s_SOCK_IO
(void*     stream,
 void*     buf,
 size_t    size,
 size_t*   n_read,
 EIO_Event what)
{
    switch (what) {
    case eIO_Read:
        return SOCK_Read((SOCK) stream, buf, size, n_read, eIO_ReadPlain);
    case eIO_Write:
        return SOCK_Pushback((SOCK) stream, buf, size);
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (sock, s_SOCK_IO, pattern, pattern_size, discard, n_discarded);
}


static EIO_Status s_BUF_IO
(void*     stream,
 void*     buf,
 size_t    size,
 size_t*   n_read,
 EIO_Event what)
{
    BUF b;
    switch (what) {
    case eIO_Read:
        *n_read = BUF_Read((BUF) stream, buf, size);
        return *n_read  ||  !size ? eIO_Success : eIO_Closed;
    case eIO_Write:
        assert(stream);
        b = (BUF) stream;
        return BUF_Pushback(&b, buf, size) ? eIO_Success : eIO_Unknown;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status BUF_StripToPattern
(BUF         buffer,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (buffer, s_BUF_IO, pattern, pattern_size, discard, n_discarded);
}



/****************************************************************************
 * URL- Encoding/Decoding
 */


/* Return integer (0..15) corresponding to the "ch" as a hex digit.
 * Return -1 on error.
 */
static int s_HexChar(char ch)
{
    unsigned int rc = (unsigned int)(ch - '0');
    if (rc <= 9)
        return (int) rc;
    rc = (unsigned int)((ch | ' ') - 'a');
    return rc <= 5 ? (int)(rc + 10) : -1;
}


/* The URL-encoding table
 */
static const char s_EncodeTable[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

#define VALID_URL_SYMBOL(ch)  (s_EncodeTable[(unsigned char)ch][0] != '%')


extern int/*bool*/ URL_DecodeEx
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 const char* allow_symbols)
{
    char* src = (char*) src_buf;
    char* dst = (char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return 1/*true*/;
    if (!src  ||  !dst)
        return 0/*false*/;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          ++(*src_read), ++(*dst_written), ++src, ++dst) {
        switch ( *src ) {
        case '+': {
            *dst = ' ';
            break;
        }
        case '%': {
            int i1, i2;
            if (*src_read + 2 < src_size) {
                if ((i1 = s_HexChar(src[1])) != -1  &&
                    (i2 = s_HexChar(src[2])) != -1) {
                    *dst = (char)((i1 << 4) + i2);
                    *src_read += 2;
                    src       += 2;
                    break;
                }
            } else if (src != src_buf) {
                assert(*dst_written);
                return 1/*true*/;
            }
            if (!allow_symbols  ||  *allow_symbols)
                return *dst_written ? 1/*true*/ : 0/*false*/;
            /*FALLTHRU*/
        }
        default: {
            if (VALID_URL_SYMBOL(*src)
                ||  (allow_symbols  &&  (!*allow_symbols
                                         ||  strchr(allow_symbols, *src)))) {
                *dst = *src;
            } else
                return *dst_written ? 1/*true*/ : 0/*false*/;
        }
        }/*switch*/
    }

    assert(src == (char*) src_buf + *src_read   );
    assert(dst == (char*) dst_buf + *dst_written);
    return 1/*true*/;
}


extern int/*bool*/ URL_Decode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    return URL_DecodeEx
        (src_buf, src_size, src_read, dst_buf, dst_size, dst_written, 0);
}


extern void URL_EncodeEx
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 const char* allow_symbols)
{
    char* src = (char*) src_buf;
    char* dst = (char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          ++(*src_read), ++(*dst_written), ++src, ++dst) {
        const char* subst = allow_symbols ? strchr(allow_symbols, *src) : 0;
        if (!subst)
            subst = s_EncodeTable[*((unsigned char*) src)];
        if (*subst != '%') {
            *dst = *subst;
        } else if (*dst_written < dst_size - 2) {
            *dst = '%';
            *(++dst) = *(++subst);
            *(++dst) = *(++subst);
            *dst_written += 2;
        } else
            return;
    }
    assert(src == (char*) src_buf + *src_read   );
    assert(dst == (char*) dst_buf + *dst_written);
}


extern void URL_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    URL_EncodeEx
        (src_buf, src_size, src_read, dst_buf, dst_size, dst_written, 0);
}



/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 */


static const char* kMIME_Type[eMIME_T_Unknown+1] = {
    "x-ncbi-data",
    "text",
    "application",
    "unknown"
};

static const char* kMIME_SubType[eMIME_Unknown+1] = {
    "x-dispatch",
    "x-asn-text",
    "x-asn-binary",
    "x-fasta",
    "x-www-form",
    "html",
    "plain",
    "xml",
    "xml+soap",
    "octet-stream",
    "x-unknown"
};

static const char* kMIME_Encoding[eENCOD_Unknown+1] = {
    "",
    "urlencoded",
    "encoded"
};


extern char* MIME_ComposeContentTypeEx
(EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         bufsize)
{
    static const char kContentType[] = "Content-Type: ";
    const char* x_type, *x_subtype, *x_encoding;
    char        x_buf[CONN_CONTENT_TYPE_LEN+1];
    size_t      len;
    char*       rv;

    assert(buf  &&  bufsize);

    *buf = '\0';
    if (type == eMIME_T_Undefined  ||  subtype == eMIME_Undefined)
        return 0;

    if (type >= eMIME_T_Unknown)
        type  = eMIME_T_Unknown;
    if (subtype >= eMIME_Unknown)
        subtype  = eMIME_Unknown;
    if (encoding >= eENCOD_Unknown)
        encoding  = eENCOD_Unknown;

    x_type     = kMIME_Type    [type];
    x_subtype  = kMIME_SubType [subtype];
    x_encoding = kMIME_Encoding[encoding];

    if ( *x_encoding ) {
        assert(sizeof(kContentType) + strlen(x_type) + strlen(x_subtype)
               + strlen(x_encoding) + 4 < sizeof(x_buf));
        sprintf(x_buf, "%s%s/%s-%s\r\n",
                kContentType, x_type, x_subtype, x_encoding);
    } else {
        assert(sizeof(kContentType) + strlen(x_type) + strlen(x_subtype)
               +                      3 < sizeof(x_buf));
        sprintf(x_buf, "%s%s/%s\r\n", kContentType, x_type, x_subtype);
    }
    len = strlen(x_buf);
    assert(len < sizeof(x_buf));
    if (len >= bufsize) {
        len  = bufsize - 1;
        rv   = 0;
    } else
        rv = buf;
    strncpy0(buf, x_buf, len);
    return rv;
}


extern int/*bool*/ MIME_ParseContentTypeEx
(const char*     str,
 EMIME_Type*     type,
 EMIME_SubType*  subtype,
 EMIME_Encoding* encoding)
{
    char*  x_buf;
    size_t x_size;
    char*  x_type;
    char*  x_subtype;
    int    i;

    if ( type )
        *type = eMIME_T_Undefined;
    if ( subtype )
        *subtype = eMIME_Undefined;
    if ( encoding )
        *encoding = eENCOD_None;

    x_size = str  &&  *str ? strlen(str) + 1 : 0;
    if (!x_size)
        return 0/*failure*/;

    if (!(x_buf = (char*) malloc(x_size << 1)))
        return 0/*failure*/;
    x_type = x_buf + x_size;

    strlwr(strcpy(x_buf, str));

    if ((sscanf(x_buf, " content-type: %s ", x_type) != 1  &&
         sscanf(x_buf, " %s ", x_type) != 1)  ||
        (x_subtype = strchr(x_type, '/')) == 0) {
        free(x_buf);
        return 0/*failure*/;
    }
    *x_subtype++ = '\0';
    x_size = strlen(x_subtype);

    if ( type ) {
        for (i = 0;  i < (int) eMIME_T_Unknown;  ++i) {
            if (strcmp(x_type, kMIME_Type[i]) == 0)
                break;
        }
        *type = (EMIME_Type) i;
    }

    for (i = 1;  i <= (int) eENCOD_Unknown;  ++i) {
        size_t len = strlen(kMIME_Encoding[i]);
        if (len < x_size) {
            char* x_encoding = x_subtype + x_size - len;
            if (x_encoding[-1] == '-'
                &&  strcmp(x_encoding, kMIME_Encoding[i]) == 0) {
                if ( encoding ) {
                    *encoding = (i == (int) eENCOD_Unknown
                                 ? eENCOD_None : (EMIME_Encoding) i);
                }
                x_encoding[-1] = '\0';
                break;
            }
        }
    }

    if ( subtype ) {
        for (i = 0;  i < (int) eMIME_Unknown;  ++i) {
            if (strcmp(x_subtype, kMIME_SubType[i]) == 0)
                break;
        }
        *subtype = (EMIME_SubType) i;
    }

    free(x_buf);
    return 1/*success*/;
}


void SERV_InitFirewallPorts(void)
{
    memset(s_FWPorts, 0, sizeof(s_FWPorts));
}


int/*bool*/ SERV_AddFirewallPort(unsigned short port)
{
    unsigned int n, m;
    if (!port--)
        return 0/*false*/;
    n = port / (sizeof(s_FWPorts[0]) << 3);
    m = port % (sizeof(s_FWPorts[0]) << 3);
    if ((size_t) n < SizeOf(s_FWPorts)) {
        s_FWPorts[n] |= (TNCBI_BigCount) 1 << m;
        return 1/*true*/;
    }
    return 0/*false*/;
}


int/*bool*/ SERV_IsFirewallPort(unsigned short port)
{
    unsigned int n, m;
    if (!port--)
        return 0/*false*/;
    n = port / (sizeof(s_FWPorts[0]) << 3);
    m = port % (sizeof(s_FWPorts[0]) << 3);
    if ((size_t) n < SizeOf(s_FWPorts)  &&
        s_FWPorts[n] & ((TNCBI_BigCount) 1 << m)) {
        return 1/*true*/;
    }
    return 0/*false*/;
}


void SERV_PrintFirewallPorts(char* buf, size_t bufsize, EFWMode mode)
{
    unsigned short m;
    size_t len, n;

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
    m = 0;
    len = 0;
    for (n = 0;  n < SizeOf(s_FWPorts);  ++n) {
        unsigned short p;
        TNCBI_BigCount mask = s_FWPorts[n];
        for (p = (unsigned short)(m + 1);  mask;  ++p, mask >>= 1) {
            if (mask & 1) {
                char port[10];
                size_t k = (size_t) sprintf(port, &" %hu"[!len], p);
                if (len + k < bufsize) {
                    memcpy(buf + len, port, k);
                    len += k;
                }
            }
        }
        m = (unsigned short)(m + (sizeof(s_FWPorts[0]) << 3));
    }
    buf[len] = '\0';
}
