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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Auxiliary API, mostly CONN-, URL-, and MIME-related
 *   (see in "ncbi_connutil.h" for more details).
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_connutil.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>


static const char* s_GetValue(const char* service, const char* param,
                              char* value, size_t value_size,
                              const char* def_value)
{
    char        key[256];
    char*       sec;
    const char* val;

    if (!param  ||  !value  ||  value_size <= 0)
        return 0;
    *value = '\0';

    if (service  &&  *service) {
        /* Service-specific inquiry */
        if (strlen(service) + 1 + sizeof(DEF_CONN_REG_SECTION) +
            strlen(param) + 1 > sizeof(key))
            return 0;
        /* First, environment search for 'service_CONN_param' */
        sprintf(key, "%s_" DEF_CONN_REG_SECTION "_%s", service, param);
        strupr(key);
        if ((val = getenv(key)) != 0)
            return strncpy0(value, val, value_size - 1);
        /* Next, search for 'CONN_param' in '[service]' registry section */
        sprintf(key, DEF_CONN_REG_SECTION "_%s", param);
        sec = key + strlen(key) + 1;
        strupr(key);
        strcpy(sec, service);
        strupr(sec);
        CORE_REG_GET(sec, key, value, value_size, 0);
        if (*value)
            return value;
    } else {
        /* Common case. Form 'CONN_param' */
        if (sizeof(DEF_CONN_REG_SECTION) + strlen(param) + 1 > sizeof(key))
            return 0;
        sprintf(key, DEF_CONN_REG_SECTION "_%s", param);
        strupr(key);
    }

    /* Environment search for 'CONN_param' */
    if ((val = getenv(key)) != 0)
        return strncpy0(value, val, value_size - 1);

    /* Last resort: Search for 'param' in default registry section */
    strcpy(key, param);
    strupr(key);
    CORE_REG_GET(DEF_CONN_REG_SECTION, key, value, value_size, def_value);

    return value;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/


extern SConnNetInfo* ConnNetInfo_Create(const char* service)
{
#define REG_VALUE(name, value, def_value) \
    s_GetValue(service, name, value, sizeof(value), def_value)

    SConnNetInfo* info = (SConnNetInfo*) malloc(sizeof(*info) +
                                                (service  &&  *service
                                                 ? strlen(service) + 1 : 0));
    /* aux. storage for the string-to-int conversions, etc. */
    char   str[256];
    int    val;
    double dbl;
    char*  s;

    if (!info)
        return 0/*failure*/;

    /* client host */
    if (!SOCK_gethostbyaddr(0, info->client_host, sizeof(info->client_host)))
        SOCK_gethostname(info->client_host, sizeof(info->client_host));

    /* dispatcher host name */
    REG_VALUE(REG_CONN_HOST, info->host, DEF_CONN_HOST);

    /* dispatcher port number */
    REG_VALUE(REG_CONN_PORT, str, 0);
    val = atoi(str);
    info->port = (unsigned short)(val > 0 ? val : DEF_CONN_PORT);

    /* service path */
    REG_VALUE(REG_CONN_PATH, info->path, DEF_CONN_PATH);

    /* service args */
    REG_VALUE(REG_CONN_ARGS, info->args, DEF_CONN_ARGS);

    /* request method */
    REG_VALUE(REG_CONN_REQ_METHOD, str, DEF_CONN_REQ_METHOD);
    if (!*str  ||  strcasecmp(str, "ANY") == 0)
        info->req_method = eReqMethod_Any;
    else if (strcasecmp(str, "POST") == 0)
        info->req_method = eReqMethod_Post;
    else if (strcasecmp(str, "GET") == 0)
        info->req_method = eReqMethod_Get;

    /* connection timeout */
    REG_VALUE(REG_CONN_TIMEOUT, str, 0);
    if (strlen(str) > 2  &&  strncasecmp(str, "infinite", strlen(str)) == 0) {
        info->timeout = 0;
    } else {
        info->timeout = &info->tmo;
        dbl = atof(str);
        if (dbl <= 0.0)
            dbl = DEF_CONN_TIMEOUT;
        info->timeout->sec  = (unsigned int) dbl;
        info->timeout->usec = (unsigned int)
            ((dbl - info->timeout->sec) * 1000000);
    }

    /* max. # of attempts to establish connection */
    REG_VALUE(REG_CONN_MAX_TRY, str, 0);
    val = atoi(str);
    info->max_try = (unsigned short)(val > 0 ? val : DEF_CONN_MAX_TRY);

    /* HTTP proxy server? */
    REG_VALUE(REG_CONN_HTTP_PROXY_HOST, info->http_proxy_host,
              DEF_CONN_HTTP_PROXY_HOST);
    if (*info->http_proxy_host) {
        /* yes, use the specified HTTP proxy server */
        REG_VALUE(REG_CONN_HTTP_PROXY_PORT, str, 0);
        val = atoi(str);
        info->http_proxy_port = (unsigned short)
            (val > 0 ? val : DEF_CONN_HTTP_PROXY_PORT);
    } else
        info->http_proxy_port = DEF_CONN_HTTP_PROXY_PORT;

    /* non-transparent CERN-like firewall proxy server? */
    REG_VALUE(REG_CONN_PROXY_HOST, info->proxy_host, DEF_CONN_PROXY_HOST);

    /* turn on debug printout? */
    REG_VALUE(REG_CONN_DEBUG_PRINTOUT, str, DEF_CONN_DEBUG_PRINTOUT);
    if (*str  &&
        (strcmp(str, "1") == 0  ||
         strcasecmp(str, "true") == 0  ||
         strcasecmp(str, "yes" ) == 0  ||
         strcasecmp(str, "some") == 0)) {
        info->debug_printout = eDebugPrintout_Some;
    } else if (*str  &&
               (strcasecmp(str, "data") == 0  ||
                strcasecmp(str, "all" ) == 0)) {
        info->debug_printout = eDebugPrintout_Data;
    } else
        info->debug_printout = eDebugPrintout_None;

    /* stateless client? */
    REG_VALUE(REG_CONN_STATELESS, str, DEF_CONN_STATELESS);
    info->stateless = (*str  &&
                       (strcmp(str, "1") == 0  ||
                        strcasecmp(str, "true") == 0  ||
                        strcasecmp(str, "yes" ) == 0));

    /* firewall mode? */
    REG_VALUE(REG_CONN_FIREWALL, str, DEF_CONN_FIREWALL);
    info->firewall = (*str  &&
                      (strcmp(str, "1") == 0  ||
                       strcasecmp(str, "true") == 0  ||
                       strcasecmp(str, "yes" ) == 0));

    /* prohibit the use of local load balancer? */
    REG_VALUE(REG_CONN_LB_DISABLE, str, DEF_CONN_LB_DISABLE);
    info->lb_disable = (*str  &&
                        (strcmp(str, "1") == 0  ||
                         strcasecmp(str, "true") == 0  ||
                         strcasecmp(str, "yes" ) == 0));

    /* user header (with optional '\r\n' added automagically) */
    REG_VALUE(REG_CONN_HTTP_USER_HEADER, str, DEF_CONN_HTTP_USER_HEADER);
    if (*str) {
        size_t len = strlen(str);
        if (str[len - 1] != '\n'  &&  len < sizeof(str) - 2) {
            str[len++] = '\r';
            str[len++] = '\n';
            str[len]   = '\0';
        }
        info->http_user_header = strdup(str);
    } else
        info->http_user_header = 0;

    /* not adjusted yet... */
    info->http_proxy_adjusted = 0/*false*/;
    /* store service name for which this structure has been created */
    if (service  &&  *service) {
        s = (char*) info + sizeof(*info);
        strcpy(s, service);
    } else
        s = 0;
    info->service = s;

    /* done */
    return info;
#undef REG_VALUE
}


extern int/*bool*/ ConnNetInfo_AdjustForHttpProxy(SConnNetInfo* info)
{
    if (info->http_proxy_adjusted  ||  !*info->http_proxy_host)
        return 0/*false*/;

    if (strlen(info->host) + strlen(info->path) + 16 > sizeof(info->path)) {
        CORE_LOG(eLOG_Error,
                 "[ConnNetInfo_AdjustForHttpProxy]  Adjusted path too long");
        assert(0);
        return 0/*false*/;
    }

    {{
        char x_path[sizeof(info->path)];
        sprintf(x_path, "http://%s:%hu%s%s", info->host, info->port,
                *info->path == '/' ? "" : "/", info->path);
        assert(strlen(x_path) < sizeof(x_path));
        strcpy(info->path, x_path);
    }}

    assert(sizeof(info->host) >= sizeof(info->http_proxy_host));
    strncpy0(info->host, info->http_proxy_host, sizeof(info->host) - 1);
    info->port = info->http_proxy_port;
    info->http_proxy_adjusted = 1/*true*/;
    return 1/*true*/;
}


extern int/*bool*/ ConnNetInfo_ParseURL(SConnNetInfo* info, const char* url)
{
    const char *s, *a;
    char* p;

    if (info->http_proxy_adjusted) {
        /* undo proxy adjustment */
        SConnNetInfo* temp = ConnNetInfo_Create(info->service);
        if (!ConnNetInfo_ParseURL(temp, info->path)) {
            ConnNetInfo_Destroy(temp);
            return 0/*failure*/;
        }
        memcpy(info->host, temp->host, sizeof(info->host));
        info->port = temp->port;
        memcpy(info->path, temp->path, sizeof(info->path));
        ConnNetInfo_Destroy(temp);
        info->http_proxy_adjusted = 0/*false*/;
    }

    /* host & port first [both optional] */
    if ((s = strstr(url, "://")) != 0) {
        const char* h = s + 3; /* host starts here */

        if (strncasecmp(url, "http://", 7) != 0)
            return 0/*failure*/;
        if (!(s = strchr(h, '/')))
            s = h + strlen(h);
        /* host ends at "a" */
        if ((a = strchr(h, ':')) != 0 && a < s) {
            unsigned short port;
            int n;

            if (sscanf(a, ":%hu%n", &port, &n) < 1 || a + n != s)
                return 0/*failure*/;
            info->port = port;
        } else
            a = s;
        if ((size_t)(a - h) < sizeof(info->host)) {
            memcpy(info->host, h, (size_t)(a - h));
            info->host[(size_t)(a - h)] = '\0';
        } else {
            memcpy(info->host, h, sizeof(info->host) - 1);
            info->host[sizeof(info->host) - 1] = '\0';
        }
    } else
        s = url;

    /* arguments */
    if ((a = strchr(s, '?')) != 0)
        strncpy0(info->args, a + 1, sizeof(info->args) - 1);
    else
        a = s + strlen(s);

    /* path (NB: can be relative) */
    if (s != url || *s == '/' || !(p = strrchr(info->path, '/'))) {
        /* absolute path */
        p = info->path;
        if (!*s) {
            s = "/";   /* in case of an empty path we take the root '/' */
            a = s + 1;
        }
    } else
        p++;
    if ((size_t)(a - s) < sizeof(info->path) - (size_t)(p - info->path)) {
        memcpy(p, s, (size_t)(a - s));
        p[(size_t)(a - s)] = '\0';
    } else {
        memcpy(p, s, sizeof(info->path) - (size_t)(p - info->path) - 1);
        info->path[sizeof(info->path) - 1] = '\0';
    }

    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_SetUserHeader(SConnNetInfo* info,
                                             const char*   user_header)
{
    if (info->http_user_header)
        free((void*) info->http_user_header);
    if (user_header && *user_header) {
        info->http_user_header = strdup(user_header);
        return info->http_user_header ? 1/*success*/ : 0/*failure*/;
    } else
        info->http_user_header = 0;
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_AppendUserHeader(SConnNetInfo* info,
                                                const char*   user_header)
{
    size_t oldlen, newlen;
    char* new_header;

    if (!info->http_user_header || !(oldlen = strlen(info->http_user_header)))
        return ConnNetInfo_SetUserHeader(info, user_header);

    if (!user_header || !(newlen = strlen(user_header)))
        return 1/*success*/;

    new_header = (char*)
        realloc((void*) info->http_user_header, oldlen + newlen + 1);
    if (!new_header)
        return 0/*failure*/;

    memcpy(&new_header[oldlen], user_header, newlen + 1);
    info->http_user_header = new_header;
    return 1/*success*/;
}


typedef enum {
    eUserHeaderOp_Delete,
    eUserHeaderOp_Extend,
    eUserHeaderOp_Override
} EUserHeaderOp;


static int/*bool*/ s_ModifyUserHeader(SConnNetInfo* info,
                                      const char*   user_header,
                                      EUserHeaderOp op)
{
    int/*bool*/ retval;
    char*  new_header;
    size_t newlinelen;
    size_t newhdrlen;
    char*  newline;
    size_t hdrlen;
    char*  hdr;

    if (!user_header || !(newhdrlen = strlen(user_header)))
        return 1/*success*/;

    if (!(hdr = (char*) info->http_user_header) || !(hdrlen = strlen(hdr))) {
        if (op == eUserHeaderOp_Delete)
            return 1/*success*/;
        if (!hdr && !(hdr = strdup("")))
            return 0/*failure*/;
        hdrlen = 0;
    }

    if (op != eUserHeaderOp_Delete) {
        if (!(new_header = (char*) malloc(newhdrlen + 1)))
            return 0/*failure*/;
        memcpy(new_header, user_header, newhdrlen + 1);
    } else
        new_header = (char*) user_header; /* we actually won't modify it! */

    retval = 1/*assume best: success*/;
    for (newline = new_header; *newline; newline += newlinelen) {
        char*  eol = strchr(newline, '\n');
        char*  eot = strchr(newline,  ':');
        int/*bool*/ used = 0;
        size_t newtaglen;
        char*  newtagval;
        size_t linelen;
        char*  line;
        size_t len;
        size_t l;

        newlinelen = (size_t)
            (eol ? eol - newline + 1 : new_header + newhdrlen - newline);
        if (!eot || eot >= newline + newlinelen ||
            !(newtaglen = (size_t)(eot - newline)))
            continue;

        newtagval = newline + newtaglen + 1;
        while (newtagval < newline + newlinelen) {
            if (isspace((unsigned char)(*newtagval)))
                newtagval++;
            else
                break;
        }
        switch (op) {
        case eUserHeaderOp_Delete:
            len = 0;
            break;
        case eUserHeaderOp_Extend:
            len = newlinelen - (size_t)(newtagval - newline);
            break;
        case eUserHeaderOp_Override:
            len = newtagval < newline + newlinelen ? newlinelen : 0;
            break;
        default:
            assert(0);
            retval = 0/*failure*/;
            len = 0;
            break;
        }

        for (line = hdr; *line; line += linelen) {
            size_t taglen;

            eol = strchr(line, '\n');
            eot = strchr(line,  ':');

            linelen = (size_t)(eol ? eol - line + 1 : hdr + hdrlen - line);
            if (!eot || eot >= line + linelen)
                continue;

            taglen = (size_t)(eot - line);
            if (newtaglen != taglen || strncasecmp(newline, line, taglen) != 0)
                continue;

            if (op == eUserHeaderOp_Extend) {
                l = linelen + len;
                if (len && linelen > 1 && line[linelen - 2] == '\r')
                    --l;
            } else
                l = len;
            if (l != linelen) {
                size_t off  = (size_t)(line - hdr);
                if (l > linelen) {
                    char* temp = (char*)realloc(hdr, hdrlen + l - linelen + 1);
                    if (!temp) {
                        retval = 0/*failure*/;
                        continue;
                    }
                    hdr  = temp;
                    line = temp + off;
                }
                hdrlen -= linelen;
                memmove(line + l, line + linelen, hdrlen - off + 1);
                hdrlen += l;
            }

            if (len) {
                if (op == eUserHeaderOp_Extend) {
                    char* s = &line[l - len - 1];
                    *s++ = ' ';
                    memcpy(s, newtagval, len);
                } else
                    memcpy(line, newline, len);
                linelen = l;
                used = 1;
            }
        }

        if (op == eUserHeaderOp_Delete)
            continue;

        if (used || !len) {
            memmove(newline, newline + newlinelen,
                    newhdrlen - (size_t)(newline-new_header) - newlinelen + 1);
            newhdrlen -= newlinelen;
            newlinelen = 0;
        }
    }

    info->http_user_header = hdr;
    if (op != eUserHeaderOp_Delete) {
        if (!ConnNetInfo_AppendUserHeader(info, new_header))
            retval = 0/*failure*/;
        free(new_header);
    }
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


extern int/*bool*/ ConnNetInfo_AppendArg(SConnNetInfo* info,
                                         const char*   arg,
                                         const char*   val)
{
    const char* amp = "&";

    if (!arg || !*arg)
        return 1/*success*/;
    if (!info->args[0])
        amp = "";
    if (strlen(info->args) + strlen(amp) + strlen(arg) +
        (val ? 1 + strlen(val) : 0) >= sizeof(info->args))
        return 0/*failure*/;
    strcat(info->args, amp);
    strcat(info->args, arg);
    if (!val || !*val)
        return 1/*success*/;
    strcat(info->args, "=");
    strcat(info->args, val);
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_PrependArg(SConnNetInfo* info,
                                          const char*   arg,
                                          const char*   val)
{
    char amp = '&';
    size_t off;

    if (!arg || !*arg)
        return 1/*success*/;
    if (!info->args[0])
        amp = '\0';
    off = strlen(arg) + (val && *val ? 1 + strlen(val) : 0) + (amp ? 1 : 0);
    if (off + strlen(info->args) >= sizeof(info->args))
        return 0/*failure*/;
    if (amp)
        memmove(&info->args[off], info->args, strlen(info->args) + 1);
    strcpy(info->args, arg);
    if (val && *val) {
        strcat(info->args, "=");
        strcat(info->args, val);
    }
    if (amp)
        info->args[off - 1] = amp;
    return 1/*success*/;
}


extern void ConnNetInfo_DeleteArg(SConnNetInfo* info,
                                  const char*   arg)
{
    size_t argnamelen;
    size_t arglen;
    char*  a;

    if (!arg || !(argnamelen = strcspn(arg, "=&")))
        return;
    for (a = info->args; *a; a += arglen) {
        if (*a == '&')
            a++;
        arglen = strcspn(a, "&");
        if (arglen < argnamelen || strncmp(a, arg, argnamelen) != 0 ||
            (a[argnamelen] && a[argnamelen] != '=' && a[argnamelen] != '&'))
            continue;
        if (a[arglen]) {
            arglen++;     /* for intermediary args, eat '&' separator, too */
            memmove(a, a + arglen, strlen(a + arglen) + 1);
        } else if (a != info->args) {
            *--a = '\0';  /* last argument in a list: remove trailing '&' */
        } else {
            *a = '\0';    /* last and the only argument removed */
        }
        arglen = 0;
    }
}


extern int/*bool*/ ConnNetInfo_PreOverrideArg(SConnNetInfo* info,
                                              const char*   arg,
                                              const char*   val)
{
    if (!arg || !*arg)
        return 1/*success*/;
    ConnNetInfo_DeleteArg(info, arg);
    return ConnNetInfo_PrependArg(info, arg, val);
}


extern int/*bool*/ ConnNetInfo_PostOverrideArg(SConnNetInfo* info,
                                               const char*   arg,
                                               const char*   val)
{
    if (!arg || !*arg)
        return 1/*success*/;
    ConnNetInfo_DeleteArg(info, arg);
    return ConnNetInfo_AppendArg(info, arg, val);
}


extern SConnNetInfo* ConnNetInfo_Clone(const SConnNetInfo* info)
{
    SConnNetInfo* x_info;
    if (!info)
        return 0;

    x_info = (SConnNetInfo*) malloc(sizeof(SConnNetInfo) +
                                    (info->service
                                     ? strlen(info->service) + 1 : 0));
    *x_info = *info;
    if (info->timeout  &&  info->timeout != kDefaultTimeout) {
        x_info->tmo     = *info->timeout;
        x_info->timeout = &x_info->tmo;
    }
    if (info->service) {
        char* s = (char*) x_info + sizeof(*x_info);
        strcpy(s, info->service);
        x_info->service = s;
    }
    x_info->http_user_header = 0;
    ConnNetInfo_SetUserHeader(x_info, info->http_user_header);
    return x_info;
}


static void s_SaveString(char* s, const char* name, const char* str) {
    sprintf(s + strlen(s), "%-16.16s: %s%s%s\n", name,
            str ? "\"" : "", str ? str : "NULL", str ? "\"" : "");
}
static void s_SaveULong(char* s, const char* name, unsigned long lll) {
    sprintf(s + strlen(s), "%-16.16s: %lu\n", name, lll);
}
static void s_SaveBool(char* s, const char* name, int/*bool*/ bbb) {
    sprintf(s + strlen(s), "%-16.16s: %s\n", name, bbb ? "TRUE" : "FALSE");
}

extern void ConnNetInfo_Log(const SConnNetInfo* info, LOG lg)
{
    char* s;

    if (!lg)
        return;

    if (!info) {
        LOG_Write(lg, eLOG_Trace, 0, 0, 0, "ConnNetInfo_Log: NULL info");
        return;
    }

    if (!(s = (char*) malloc(sizeof(*info) + 4096 +
                             (info->service ? strlen(info->service) : 0) +
                             (info->http_user_header
                              ? strlen(info->http_user_header) : 0)))) {
        LOG_WRITE(lg, eLOG_Error, "ConnNetInfo_Log: Cannot alloc temp buffer");
        return;
    }

    strcpy(s, "ConnNetInfo_Log\n"
           "#################### [BEGIN] SConnNetInfo:\n");
    s_SaveString    (s, "service",         info->service);
    s_SaveString    (s, "client_host",     info->client_host);
    s_SaveString    (s, "host",            info->host);
    s_SaveULong     (s, "port",            info->port);
    s_SaveString    (s, "path",            info->path);
    s_SaveString    (s, "args",            info->args);
    s_SaveString    (s, "req_method",     (info->req_method == eReqMethod_Any
                                           ? DEF_CONN_REQ_METHOD
                                           : (info->req_method
                                              == eReqMethod_Get
                                              ? "GET"
                                              : (info->req_method
                                                 == eReqMethod_Post
                                                 ? "POST" : "Unknown"))));
    if (info->timeout) {
        s_SaveULong (s, "timeout(sec)",    info->timeout->sec);
        s_SaveULong (s, "timeout(usec)",   info->timeout->usec);
    } else
        s_SaveString(s, "timeout",         "infinite");
    s_SaveULong     (s, "max_try",         info->max_try);
    s_SaveString    (s, "http_proxy_host", info->http_proxy_host);
    s_SaveULong     (s, "http_proxy_port", info->http_proxy_port);
    s_SaveString    (s, "proxy_host",      info->proxy_host);
    s_SaveString    (s, "debug_printout", (info->debug_printout
                                           == eDebugPrintout_None
                                           ? "NONE"
                                           : (info->debug_printout
                                              == eDebugPrintout_Some
                                              ? "SOME"
                                              : (info->debug_printout
                                                 == eDebugPrintout_Data
                                                 ? "DATA" : "Unknown"))));
    s_SaveBool      (s, "stateless",       info->stateless);
    s_SaveBool      (s, "firewall",        info->firewall);
    s_SaveBool      (s, "lb_disable",      info->lb_disable);
    s_SaveString    (s, "user_header",     info->http_user_header);
    s_SaveBool      (s, "proxy_adjusted",  info->http_proxy_adjusted);
    strcat(s, "#################### [END] SConnNetInfo\n");

    LOG_Write(lg, eLOG_Trace, 0, 0, 0, s);
    free(s);
}


extern void ConnNetInfo_Destroy(SConnNetInfo* info)
{
    if (!info)
        return;
    ConnNetInfo_SetUserHeader(info, 0);
    free(info);
}


extern SOCK URL_Connect
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 EReqMethod      req_method,
 size_t          content_length,
 const STimeout* c_timeout,
 const STimeout* rw_timeout,
 const char*     user_hdr,
 int/*bool*/     encode_args,
 ESwitch         log)
{
    static const char X_REQ_Q[] = "?";
    static const char X_REQ_E[] = " HTTP/1.0\r\n";
    static const char X_HOST[]  = "Host: ";

    EIO_Status  st;
    BUF         buf;
    SOCK        sock;
    char*       header;
    char        buffer[80];
    size_t      headersize;
    const char* x_args = 0;
    const char* x_req_r; /* "POST "/"GET " */

    /* check the args */
    if (!host  ||  !*host  ||  !port  ||  !path  ||  !*path  ||
        (user_hdr  &&  *user_hdr  &&  user_hdr[strlen(user_hdr)-1] != '\n')) {
        CORE_LOG(eLOG_Error, "[URL_Connect]  Bad arguments");
        assert(0);
        return 0/*error*/;
    }

    switch (req_method) {
    case eReqMethod_Any:
        x_req_r = DEF_CONN_REQ_METHOD " ";
        break;
    case eReqMethod_Post:
        x_req_r = "POST ";
        break;
    case eReqMethod_Get:
        x_req_r = "GET ";
        break;
    default:
        CORE_LOGF(eLOG_Error, ("[URL_Connect]  Unrecognized request method"
                               " (%d)", (int) req_method));
        assert(0);
        return 0/*error*/;
    }
    if (content_length  &&  strcasecmp(x_req_r, "GET ") == 0) {
        CORE_LOG(eLOG_Warning,
                 "[URL_Connect]  Content length ignored with GET");
        content_length = 0;
    }

    /* URL-encode "args", if any specified */
    if (args  &&  *args) {
        size_t src_size = strlen(args);
        if ( encode_args ) {
            size_t dst_size = 3 * src_size;
            size_t src_read, dst_written;
            char* xx_args = (char*) malloc(dst_size + 1);
            if (!xx_args)
                return 0/*failure: no memory*/;
            URL_Encode(args,    src_size, &src_read,
                       xx_args, dst_size, &dst_written);
            xx_args[dst_written] = '\0';
            assert(src_read == src_size);
            x_args = xx_args;
        } else
            x_args = args;
    }

    buf = 0;
    errno = 0;
    /* compose HTTP header */
    if (/* {POST|GET} <path>?<args> HTTP/1.0\r\n */
        !BUF_Write(&buf, x_req_r, strlen(x_req_r))            ||
        !BUF_Write(&buf, path,    strlen(path))               ||
        (x_args
         &&  (!BUF_Write(&buf, X_REQ_Q, sizeof(X_REQ_Q) - 1)  ||
              !BUF_Write(&buf, x_args,  strlen(x_args))))     ||
        !BUF_Write(&buf,       X_REQ_E, sizeof(X_REQ_E) - 1)  ||

        /* Host: host\r\n */
        !BUF_Write(&buf, X_HOST, sizeof(X_HOST) - 1)          ||
        !BUF_Write(&buf, host,   strlen(host))                ||
        !BUF_Write(&buf, "\r\n", 2)                           ||

        /* <user_header> */
        (user_hdr
         &&  !BUF_Write(&buf, user_hdr, strlen(user_hdr)))    ||

        /* Content-Length: <content_length>\r\n\r\n */
        (req_method != eReqMethod_Get
         &&  (sprintf(buffer, "Content-Length: %lu\r\n",
                      (unsigned long) content_length) <= 0    ||
              !BUF_Write(&buf, buffer, strlen(buffer))))      ||

        !BUF_Write(&buf, "\r\n", 2)) {
        CORE_LOGF(eLOG_Error, ("[URL_Connect]  Error composing HTTP header for"
                               " %s:%hu%s%s", host, port, errno ? ": " : "",
                               errno ? strerror(errno) : ""));
        BUF_Destroy(buf);
        if (x_args  &&  x_args != args)
            free((void*) x_args);
        return 0/*error*/;
    }
    if (x_args  &&  x_args != args)
        free((void*) x_args);

    if (!(header = (char*) malloc(headersize = BUF_Size(buf))) ||
        BUF_Read(buf, header, headersize) != headersize) {
        CORE_LOGF(eLOG_Error, ("[URL_Connect]  Error storing HTTP header for"
                               " %s:%hu: %s", host, port,
                               errno ? strerror(errno) : "Unknown error"));
        if (header)
            free(header);
        BUF_Destroy(buf);
        return 0/*error*/;
    }
    BUF_Destroy(buf);

    /* connect to HTTPD */
    st = SOCK_CreateEx(host, port, c_timeout, &sock, header, headersize, log);
    free(header);
    if (st != eIO_Success) {
        CORE_LOGF(eLOG_Error,
                  ("[URL_Connect]  Socket connect to %s:%hu failed: %s",
                   host, port, IO_StatusStr(st)));
        return 0/*error*/;
    }

    /* setup I/O timeout for the connection */
    if (SOCK_SetTimeout(sock, eIO_ReadWrite, rw_timeout) != eIO_Success) {
        CORE_LOG(eLOG_Error, "[URL_Connect]  Cannot set connection timeout");
        SOCK_Close(sock);
        return 0/*error*/;
    }

    /* success */
    return sock;
}


/* Code for the "*_StripToPattern()" functions
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
 BUF*        buf,
 size_t*     n_discarded)
{
    EIO_Status status;
    char*      buffer;
    size_t     buffer_size;
    size_t     n_read = 0;

    /* check args */
    if ( n_discarded )
        *n_discarded = 0;
    if (!stream  ||  (pattern != 0) != (pattern_size != 0))
        return eIO_InvalidArg;

    /* allocate a temporary read buffer */
    buffer_size = 2 * pattern_size;
    if (buffer_size < 4096)
        buffer_size = 4096;
    if ( !(buffer = (char*) malloc(buffer_size)) )
        return eIO_Unknown;

    if ( !pattern ) {
        /* read/discard until EOF */
        do {
            status = io_func(stream, buffer, buffer_size, &n_read, eIO_Read);
            if ( buf )
                BUF_Write(buf, buffer, n_read);
            if ( n_discarded )
                *n_discarded += n_read;
        } while (status == eIO_Success);
    } else {
        for (;;) {
            /* read; search for the pattern; store the discarded data */
            size_t x_read, n_stored;

            assert(n_read < pattern_size);
            status = io_func(stream, buffer + n_read, buffer_size - n_read,
                             &x_read, eIO_Read);
            if ( !x_read ) {
                assert(status != eIO_Success);
                break; /*error*/
            }
            n_stored = n_read + x_read;

            if (n_stored >= pattern_size) {
                /* search for the pattern */
                size_t n_check = n_stored - pattern_size + 1;
                const char* b;
                for (b = buffer;  n_check;  b++, n_check--) {
                    if (*b != *((const char*) pattern))
                        continue;
                    if (memcmp(b, pattern, pattern_size) == 0)
                        break; /*found*/
                }
                /* pattern found */
                if ( n_check ) {
                    size_t x_discarded = (size_t)(b - buffer) + pattern_size;
                    if ( buf )
                        BUF_Write(buf, buffer + n_read, x_discarded - n_read);
                    if ( n_discarded )
                        *n_discarded += x_discarded;
                    /* return unused portion to the stream */
                    status = io_func(stream, buffer + x_discarded,
                                     n_stored - x_discarded, 0, eIO_Write);
                    break; /*finished*/
                }
            }

            /* pattern not found yet */
            if ( buf )
                BUF_Write(buf, buffer + n_read, x_read);
            if ( n_discarded )
                *n_discarded += x_read;
            n_read = n_stored;

            if (n_read > pattern_size) {
                size_t n_cut = n_read - pattern_size + 1;
                n_read = pattern_size - 1;
                memmove(buffer, buffer + n_cut, n_read);
            }
        }
    }

    /* cleanup & exit */
    free(buffer);
    return status;
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
        assert(stream);
        return CONN_PushBack((CONN) stream, buf, size);
    default:
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (conn, s_CONN_IO, pattern, pattern_size, buf, n_discarded);
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
        return SOCK_PushBack((SOCK) stream, buf, size);
    default:
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (sock, s_SOCK_IO, pattern, pattern_size, buf, n_discarded);
}


static EIO_Status s_BUF_IO
(void*     stream,
 void*     buf,
 size_t    size,
 size_t*   n_read,
 EIO_Event what)
{
    switch (what) {
    case eIO_Read:
        *n_read = BUF_Read((BUF) stream, buf, size);
        return *n_read ? eIO_Success : eIO_Closed;
    case eIO_Write:
        assert(stream);
        return BUF_PushBack((BUF*) &stream, buf, size)
            ? eIO_Success : eIO_Unknown;
    default:
        break;
    }
    return eIO_InvalidArg;
}

extern EIO_Status BUF_StripToPattern
(BUF         buffer,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (buffer, s_BUF_IO, pattern, pattern_size, buf, n_discarded);
}


/* Return integer (0..15) corresponding to the "ch" as a hex digit
 * Return -1 on error
 */
static int s_HexChar(char ch)
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    if ('A' <= ch  &&  ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}

/* The URL-encoding table
 */
static const char s_Encode[256][4] = {
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
#define VALID_URL_SYMBOL(ch)  (s_Encode[(unsigned char)ch][0] != '%')


extern int/*bool*/ URL_DecodeEx
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 const char* allow_symbols)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return 1/*true*/;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
        switch ( *src ) {
        case '%': {
            int i1, i2;
            if (*src_read + 2 > src_size)
                return 1/*true*/;
            if ((i1 = s_HexChar(*(++src))) == -1)
                return *dst_written ? 1/*true*/ : 0/*false*/;
            if (*src_read + 3 > src_size)
                return 1/*true*/;
            if ((i2 = s_HexChar(*(++src))) == -1)
                return *dst_written ? 1/*true*/ : 0/*false*/;

            *dst = (unsigned char)((i1 << 4) + i2);
            *src_read += 2;
            break;
        }
        case '+': {
            *dst = ' ';
            break;
        }
        default: {
            if (VALID_URL_SYMBOL(*src)  ||
                (allow_symbols  &&  strchr(allow_symbols, *src)))
                *dst = *src;
            else
                return *dst_written ? 1/*true*/ : 0/*false*/;
        }
        }/*switch*/
    }

    assert(src == (unsigned char*) src_buf + *src_read   );
    assert(dst == (unsigned char*) dst_buf + *dst_written);
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


extern void URL_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
        const char* subst = s_Encode[*src];
        if (*subst != '%') {
            *dst = *subst;
        } else if (*dst_written < dst_size - 2) {
            *dst = '%';
            *(++dst) = *(++subst);
            *(++dst) = *(++subst);
            *dst_written += 2;
        } else {
            return;
        }
    }
    assert(src == (unsigned char*) src_buf + *src_read   );
    assert(dst == (unsigned char*) dst_buf + *dst_written);
}



extern void BASE64_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 size_t*     line_len)
{
    static const char syms[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /*26*/
        "abcdefghijklmnopqrstuvwxyz" /*52*/
        "0123456789+/";              /*64*/
    const size_t max_len = line_len ? *line_len : 76;
    const size_t max_src =
        ((dst_size - (max_len ? dst_size/(max_len + 1) : 0)) >> 2) * 3;
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t len = 0, i = 0, j = 0;
    unsigned char shift = 2;
    unsigned char temp = 0;
    if (!max_src) {
        *src_read    = 0;
        *dst_written = 0;
        if (dst_size > 0) {
            *dst = '\0';
        }
        return;
    }
    if (src_size > max_src) {
        src_size = max_src;
    }
    for (;;) {
        unsigned char c = i < src_size ? src[i] : 0;
        unsigned char bits = (c >> shift) & 0x3F;
        assert((temp | bits) < sizeof(syms) - 1);
        dst[j++] = syms[temp | bits];
        if (max_len  &&  ++len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
        if (i >= src_size) {
            break;
        }
        shift += 2;
        shift &= 7;
        if (shift) {
            i++;
        }
        temp = (c << (8 - shift)) & 0x3F;
    }
    assert(j <= dst_size);
    *src_read = i;
    for (i = 0; i < (3 - src_size % 3) % 3; i++) {
        dst[j++] = '=';
        if (max_len  &&  ++len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
    }
    assert(j <= dst_size);
    *dst_written = j;
    if (j < dst_size) {
        dst[j] = '\0';
    }
}


extern int/*bool*/ BASE64_Decode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t i = 0, j = 0, k = 0, l;
    unsigned int temp = 0;
    if (src_size < 4  ||  dst_size < 3) {
        *src_read    = 0;
        *dst_written = 0;
        return 0/*false*/;
    }
    for (;;) {
        unsigned char c = i < src_size ? src[i++] : '=';
        if (c == '=') {
            c  = 64; /*end*/
        } else if (c >= 'A'  &&  c <= 'Z') {
            c -= 'A';
        } else if (c >= 'a'  &&  c <= 'z') {
            c -= 'a' - 26;
        } else if (c >= '0'  &&  c <= '9') {
            c -= '0' - 52;
        } else if (c == '+') {
            c  = 62;
        } else if (c == '/') {
            c  = 63;
        } else {
            continue;
        }
        temp <<= 6;
        temp  |= c & 0x3F;
        if (!(++k & 3)  ||  c == 64) {
            if (c == 64) {
                if (k < 2) {
                    --i;
                    break;
                }
                switch (k) {
                case 2:
                    temp >>= 4;
                    break;
                case 3:
                    temp >>= 10;
                    break;
                case 4:
                    temp >>= 8;
                    break;
                default:
                    break;
                }
                for (l = k; l < 4; l++) {
                    if (i < src_size  &&  src[i] == '=') {
                        i++;
                    }
                }
            } else {
                k = 0;
            }
            switch (k) {
            case 0:
                dst[j++] = (temp & 0xFF0000) >> 16;
                /*FALLTHRU*/;
            case 4:
                dst[j++] = (temp & 0xFF00) >> 8;
                /*FALLTHRU*/
            case 3:
                dst[j++] = (temp & 0xFF);
                break;
            default:
                break;
            }
            if (j + 3 >= dst_size  ||  c == 64) {
                break;
            }
            temp = 0;
        }
    }
    *src_read    = i;
    *dst_written = j;
    return i ? 1/*true*/ : 0/*false*/;
}



/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 */

static const char* s_MIME_Type[eMIME_T_Unknown+1] = {
    "x-ncbi-data",
    "text",
    "application",
    "unknown"
};

static const char* s_MIME_SubType[eMIME_Unknown+1] = {
    "x-dispatch",
    "x-asn-text",
    "x-asn-binary",
    "x-fasta",
    "x-www-form",
    "html",
    "plain",
    "xml",
    "xml+soap",
    "x-unknown"
};

static const char* s_MIME_Encoding[eENCOD_Unknown+1] = {
    "",
    "urlencoded",
    "encoded"
};


extern char* MIME_ComposeContentTypeEx
(EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen)
{
    static const char s_ContentType[] = "Content-Type: ";
    const char*       x_Type          = s_MIME_Type    [(int) type];
    const char*       x_SubType       = s_MIME_SubType [(int) subtype];
    const char*       x_Encoding      = s_MIME_Encoding[(int) encoding];
    char              x_buf[MAX_CONTENT_TYPE_LEN];

    if ( *x_Encoding ) {
        assert(sizeof(s_ContentType) + strlen(x_Type) + strlen(x_SubType)
               + strlen(x_Encoding) + 4 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s-%s\r\n",
                s_ContentType, x_Type, x_SubType, x_Encoding);
    } else {
        assert(sizeof(s_ContentType) + strlen(x_Type) + strlen(x_SubType)
               + 3 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s\r\n", s_ContentType, x_Type, x_SubType);
    }

    assert(strlen(x_buf) < sizeof(x_buf));
    assert(strlen(x_buf) < buflen);
    strncpy0(buf, x_buf, buflen - 1);
    return buf;
}


extern char* MIME_ComposeContentType
(EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen)
{
    return MIME_ComposeContentTypeEx(eMIME_T_NcbiData,
                                     subtype, encoding, buf, buflen);
}


extern int/*bool*/ MIME_ParseContentTypeEx
(const char*     str,
 EMIME_Type*     type,
 EMIME_SubType*  subtype,
 EMIME_Encoding* encoding)
{
    char* x_buf;
    char* x_type;
    char* x_subtype;
    int   i;

    if ( type )
        *type = eMIME_T_Unknown;
    if ( subtype )
        *subtype = eMIME_Unknown;
    if ( encoding )
        *encoding = eENCOD_Unknown;

    if (!str  ||  !*str)
        return 0/*false*/;

    {{
        size_t x_size = strlen(str) + 1;
        x_buf  = (char*) malloc(2 * x_size);
        x_type = x_buf  + x_size;
    }}

    strcpy(x_buf, str);
    strlwr(x_buf);

    if ((sscanf(x_buf, " content-type: %s ", x_type) != 1  &&
         sscanf(x_buf, " %s ", x_type) != 1)  ||
        (x_subtype = strchr(x_type, '/')) == 0) {
        free(x_buf);
        return 0/*false*/;
    }
    *x_subtype++ = '\0';

    if ( type ) {
        for (i = 0;  i < (int) eMIME_T_Unknown;  i++) {
            if ( !strcmp(x_type, s_MIME_Type[i]) ) {
                *type = (EMIME_Type) i;
                break;
            }
        }
    }

    for (i = 0;  i < (int) eENCOD_Unknown;  i++) {
        char* x_encoding = strstr(x_subtype, s_MIME_Encoding[i]);
        if (x_encoding  &&  *x_encoding  &&
            x_encoding != x_subtype  &&  *(x_encoding - 1) == '-'  &&
            strcmp(x_encoding, s_MIME_Encoding[i]) == 0) {
            if ( encoding ) {
                *encoding = (EMIME_Encoding) i;
            }
            *(x_encoding - 1) = '\0';
            break;
        }
    }
    if (encoding  &&  *encoding == eENCOD_Unknown)
        *encoding = eENCOD_None;

    if ( subtype ) {
        for (i = 0;  i < (int) eMIME_Unknown;  i++) {
            if ( !strcmp(x_subtype, s_MIME_SubType[i]) ) {
                *subtype = (EMIME_SubType) i;
                break;
            }
        }
    }

    free(x_buf);
    return 1/*true*/;
}


extern int/*bool*/ MIME_ParseContentType
(const char*     str,
 EMIME_SubType*  subtype,
 EMIME_Encoding* encoding)
{
    EMIME_Type type;
    if ( !MIME_ParseContentTypeEx(str, &type, subtype, encoding) )
        return 0/*false*/;

    if (type != eMIME_T_NcbiData) {
        if ( subtype )
            *subtype  = eMIME_Unknown;
        if ( encoding )
            *encoding = eENCOD_Unknown;
        return 0/*false*/;
    }

    return 1/*true*/;
}



/****************************************************************************
 * Reading and writing [host][:port] addresses
 */

extern const char* StringToHostPort(const char*     str,
                                    unsigned int*   host,
                                    unsigned short* port)
{
    unsigned short p;
    unsigned int h;
    char abuf[256];
    const char* s;
    size_t alen;
    int n = 0;

    if (host)
        *host = 0;
    if (port)
        *port = 0;
    for (s = str; *s; s++) {
        if (isspace((unsigned char)(*s)) || *s == ':')
            break;
    }
    if ((alen = (size_t)(s - str)) > sizeof(abuf) - 1)
        return str;
    if (alen) {
        strncpy0(abuf, str, alen);
        if (!(h = SOCK_gethostbyname(abuf)))
            return str;
    } else
        h = 0;
    if (*s == ':') {
        if (sscanf(++s, "%hu%n", &p, &n) < 1 ||
            (s[n] && !isspace((unsigned char) s[n])))
            return alen ? 0 : str;
    } else
        p = 0;
    if (host)
        *host = h;
    if (port)
        *port = p;
    return s + n;
}


extern size_t HostPortToString(unsigned int   host,
                               unsigned short port,
                               char*          buf,
                               size_t         buflen)
{
    char   x_buf[16/*sizeof("255.255.255.255")*/ + 8/*:port*/];
    size_t n;

    if (!buf || !buflen)
        return 0;
    if (!host)
        *x_buf = 0;
    else if (SOCK_ntoa(host, x_buf, sizeof(x_buf)) != 0) {
        *buf = 0;
        return 0;
    }
    n = strlen(x_buf);
    if (port || !host)
        n += sprintf(x_buf + n, ":%hu", port);
    assert(n < sizeof(x_buf));
    if (n >= buflen)
        n = buflen - 1;
    memcpy(buf, x_buf, n);
    buf[n] = 0;
    return n;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.71  2005/03/21 17:10:03  lavr
 * BASE64_Decode(): unnecessary re-init of "k" removed
 *
 * Revision 6.70  2005/03/21 17:04:35  lavr
 * BASE64_{En|De}code few bugs fixed
 *
 * Revision 6.69  2005/03/19 02:13:55  lavr
 * +BASE64_{En|De}code
 *
 * Revision 6.68  2005/02/28 17:58:31  lavr
 * Cosmetics
 *
 * Revision 6.67  2005/02/24 19:03:11  lavr
 * Always init SConnNetInfo::http_user_header
 *
 * Revision 6.66  2005/02/24 19:00:45  lavr
 * +CONN_HTTP_USER_HEADER
 *
 * Revision 6.65  2004/10/14 13:51:42  lavr
 * StringToHostPort() to allow NULL ptrs
 *
 * Revision 6.64  2004/04/06 19:25:56  lavr
 * Fix ConnNetInfo_DeleteArg() to remove arg's trailing '&'
 *
 * Revision 6.63  2004/01/14 18:52:39  lavr
 * Recognize eMIME_XmlSoap and corresponding text representation "xml+soap"
 *
 * Revision 6.62  2004/01/07 19:23:29  lavr
 * "xml" added as a MIME subtype
 *
 * Revision 6.61  2003/11/12 17:46:12  lavr
 * HostPortToString() changed to be a little more efficient
 *
 * Revision 6.60  2003/08/27 16:27:37  lavr
 * Add "Host:" tag to be able to take advantage of Apache VHosts
 *
 * Revision 6.59  2003/08/25 14:44:43  lavr
 * Employ new k..Timeout constants
 * URL_Connect():  get rid of one avoidable malloc()
 * User header manipulation routines:  factor out some arithmetics for speed
 *
 * Revision 6.58  2003/05/31 05:13:38  lavr
 * Replace bitwise XOR with inequality [of the same effect]
 *
 * Revision 6.57  2003/05/20 21:25:24  lavr
 * Limit SConnNetInfo::max_try by reasonable "short" value
 *
 * Revision 6.56  2003/05/19 16:44:45  lavr
 * Minor style adjustements
 *
 * Revision 6.55  2003/05/14 03:51:54  lavr
 * URL_Connect() rewritten to submit HTTP header as SOCK's initial buffer
 *
 * Revision 6.54  2003/04/30 17:02:11  lavr
 * Name collision resolved in ConnNetInfo_ParseURL()
 *
 * Revision 6.53  2003/03/06 21:55:31  lavr
 * s_ModifyUserHeader(): Heed uninitted usage warning
 * HostPortToString():   Do not append :0 (for zero port) if host is not empty
 *
 * Revision 6.52  2003/02/28 14:47:41  lavr
 * Bugfix: proper bool -> eIO_Status conversion in s_BUF_IO()
 *
 * Revision 6.51  2003/01/31 21:17:04  lavr
 * More robust search for sheme in URL
 *
 * Revision 6.50  2003/01/17 19:44:46  lavr
 * Reduce dependencies
 *
 * Revision 6.49  2003/01/15 19:52:25  lavr
 * *_StripToPattern() calls modified to use Read/PushBack instead of Peek/Read
 *
 * Revision 6.48  2002/12/13 21:19:13  lavr
 * Separate header tag values with spaces as most commonly required (rfc1945)
 *
 * Revision 6.47  2002/12/10 17:34:15  lavr
 * Remove errno decoding on failed connect in URL_Connect()
 *
 * Revision 6.46  2002/12/05 21:43:31  lavr
 * Fix in assignment and compare in URL_Connect()
 *
 * Revision 6.45  2002/12/04 16:50:47  lavr
 * Use SOCK_CreateEx() in URL_Connect()
 *
 * Revision 6.44  2002/11/19 19:19:57  lavr
 * +ConnNetInfo_ExtendUserHeader()
 *
 * Revision 6.43  2002/11/13 19:54:13  lavr
 * ConnNetInfo_DeleteArg(): fix initial argument calculation size
 *
 * Revision 6.42  2002/11/12 05:50:33  lavr
 * Modify client host name discovery
 *
 * Revision 6.41  2002/11/01 20:13:50  lavr
 * Remove MAXHOSTNAMELEN and MAX_IP_ADDR_LEN macros
 *
 * Revision 6.40  2002/10/28 15:42:38  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.39  2002/10/21 18:30:59  lavr
 * +ConnNetInfo_AppendArg()
 * +ConnNetInfo_PrependArg()
 * +ConnNetInfo_DeleteArg()
 * +ConnNetInfo_PreOverrideArg()
 * +ConnNetInfo_PostOverrideArg()
 *
 * Revision 6.38  2002/10/11 19:42:06  lavr
 * +ConnNetInfo_AppendUserHeader()
 * +ConnNetInfo_OverrideUserHeader()
 * +ConnNetInfo_DeleteUserHeader()
 *
 * Revision 6.37  2002/09/24 18:08:34  lavr
 * Avoid compiler warning in positioning on first but one array element
 *
 * Revision 6.36  2002/08/12 15:12:01  lavr
 * Use persistent SOCK_Write()
 *
 * Revision 6.35  2002/08/07 16:32:47  lavr
 * Changed EIO_ReadMethod enums accordingly; log moved to end
 *
 * Revision 6.34  2002/05/06 19:12:57  lavr
 * -ConnNetInfo_Print(); +ConnNetInfo_Log()
 * Addition: *_StripToPattern() now can strip until EOF (or error)
 *
 * Revision 6.33  2002/04/26 16:31:41  lavr
 * Add more space between functions to separate them better
 *
 * Revision 6.32  2002/04/13 06:36:36  lavr
 * Fix for empty path parsing in HTTP URL
 *
 * Revision 6.31  2002/03/19 22:13:09  lavr
 * Minor tweak in recognizing "infinite" (and part) as a special timeout
 *
 * Revision 6.30  2002/03/11 21:53:18  lavr
 * Recognize ALL in CONN_DEBUG_PRINTOUT; bugfix for '//' in proxy adjustement
 *
 * Revision 6.29  2002/02/20 19:12:17  lavr
 * Swapped eENCOD_Url and eENCOD_None; eENCOD_Unknown introduced
 *
 * Revision 6.28  2002/02/08 22:22:17  lavr
 * BUGFIX: sizeof(info) -> sizeof(*info) in ConnNetInfo_Create()
 *
 * Revision 6.27  2002/01/30 20:14:48  lavr
 * URL_Connect(): Print error code in some failure messages
 *
 * Revision 6.26  2002/01/28 20:21:46  lavr
 * Do not store "" as a user_header
 *
 * Revision 6.25  2001/12/30 19:40:32  lavr
 * +ConnNetInfo_ParseURL()
 * Added recordkeeping of service name for which the info was created
 *
 * Revision 6.24  2001/12/04 15:56:28  lavr
 * Use strdup() instead of explicit strcpy(malloc(...), ...)
 *
 * Revision 6.23  2001/09/24 20:27:00  lavr
 * Message corrected: "Adjusted path too long"
 *
 * Revision 6.22  2001/09/10 21:14:58  lavr
 * Added functions: StringToHostPort()
 *                  HostPortToString()
 *
 * Revision 6.21  2001/05/31 21:30:57  vakatov
 * MIME_ParseContentTypeEx() -- a more accurate parsing
 *
 * Revision 6.20  2001/05/29 21:15:43  vakatov
 * + eMIME_Plain
 *
 * Revision 6.19  2001/04/24 21:29:43  lavr
 * Special text value "infinite" accepted as infinite timeout from environment
 *
 * Revision 6.18  2001/03/26 18:37:09  lavr
 * #include <ctype.h> not used, removed
 *
 * Revision 6.17  2001/03/02 20:08:05  lavr
 * Typo fixed
 *
 * Revision 6.16  2001/01/25 16:58:33  lavr
 * ConnNetInfo_SetUserHeader now used throughout to set/reset http_user_header
 *
 * Revision 6.15  2001/01/23 23:06:18  lavr
 * SConnNetInfo.debug_printout converted from boolean to enum
 * BUF_StripToPattern() introduced
 *
 * Revision 6.14  2001/01/12 00:01:27  lavr
 * CONN_PROXY_HOST was forgotten to init in ConnNetInfo_Create
 *
 * Revision 6.13  2001/01/11 23:07:08  lavr
 * ConnNetInfo_Print() prints user-header 'as is'; pretty-printing undone
 *
 * Revision 6.12  2001/01/08 23:46:27  lavr
 * REQUEST_METHOD -> REQ_METHOD to be consistent with SConnNetInfo
 *
 * Revision 6.11  2001/01/08 22:35:56  lavr
 * Client-Mode removed; replaced by 2 separate boolean fields:
 * stateless and firewall
 *
 * Revision 6.10  2000/12/29 17:54:11  lavr
 * NCBID stuff removed; ConnNetInfo_SetUserHeader added;
 * modifications to ConnNetInfo_Print output.
 *
 * Revision 6.9  2000/11/07 23:23:18  vakatov
 * In-sync with the C Toolkit "connutil.c:R6.15", "connutil.h:R6.13"
 * (with "eMIME_Dispatch" added).
 *
 * Revision 6.8  2000/10/20 17:08:40  lavr
 * All keys capitalized for registry access
 * Search for some keywords made case insensitive
 *
 * Revision 6.7  2000/10/11 22:29:44  lavr
 * Forgotten blank added after {GET|POST} request in URL_Connect
 *
 * Revision 6.6  2000/10/05 22:35:24  lavr
 * SConnNetInfo modified to contain 'client_mode' instead of just
 * 'firewall' boolean
 *
 * Revision 6.5  2000/09/27 19:37:40  lavr
 * ncbi_ansi_ext.h included
 *
 * Revision 6.4  2000/09/26 22:01:33  lavr
 * Registry entries changed, HTTP request method added
 *
 * Revision 6.3  2000/04/21 19:42:35  vakatov
 * Several minor typo/bugs fixed
 *
 * Revision 6.2  2000/03/29 16:36:09  vakatov
 * MIME_ParseContentType() -- a fix
 *
 * Revision 6.1  2000/03/24 22:53:34  vakatov
 * Initial revision
 *
 * ==========================================================================
 */
