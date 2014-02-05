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
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_connutil.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Util


static char* x_StrcatCRLF(char* dst, const char* src)
{
    size_t dstlen = dst  &&  *dst ? strlen(dst) : 0;
    size_t srclen = src  &&  *src ? strlen(src) : 0;
    if (dstlen  ||  srclen) {
        size_t len;
        char*  temp;
        if (dstlen  &&  dst[dstlen - 1] == '\n') {
            if (--dstlen  &&  dst[dstlen - 1] == '\r')
                --dstlen;
        }
        if (srclen  &&  src[srclen - 1] == '\n') {
            if (--srclen  &&  src[srclen - 1] == '\r')
                --srclen;
        }
        len = (dstlen ? dstlen + 2 : 0) + (srclen ? srclen + 2 : 0) + 1;
        if (!(temp = (char*)(dst ? realloc(dst, len) : malloc (len))))
            return 0;
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
    return dst;
}


static const char* x_GetValue(const char* service, const char* param,
                              char* value, size_t value_size,
                              const char* def_value, int* /*bool*/ generic)
{
    char        buf[128];
    const char* val;
    char*       s;

    if (!value  ||  value_size <= 0)
        return 0;
    *value = '\0';

    if (!param  ||  !*param)
        return 0;

    *generic = 0/*false*/;
    if (service  &&  *service) {
        /* Service-specific inquiry */
        int/*bool*/ end;
        char        temp[sizeof(buf)];
        size_t      slen = strlen(service);
        size_t      plen = strlen(param) + 1;
        size_t       len = slen + 1 + plen;

        if (strncasecmp(param, DEF_CONN_REG_SECTION "_",
                        sizeof(DEF_CONN_REG_SECTION)) != 0) {
            len += sizeof(DEF_CONN_REG_SECTION);
            end = 0/*false*/;
        } else
            end = 1/*true*/;
        if (len > sizeof(buf))
            return 0;

        /* First, environment search for 'service_CONN_param' */
        s = (char*) memcpy(buf, service, slen) + slen;
        *s++ = '_';
        if (!end) {
            memcpy(s, DEF_CONN_REG_SECTION, sizeof(DEF_CONN_REG_SECTION) - 1);
            s += sizeof(DEF_CONN_REG_SECTION) - 1;
            *s++ = '_';
        }
        memcpy(s, param, plen);
        if ((val = getenv(strupr((char*) memcpy(temp, buf, len)))) != 0
            ||  (memcmp(temp, buf, len) != 0  &&  (val = getenv(buf)) != 0)) {
            return strncpy0(value, val, value_size - 1);
        }

        /* Next, search for 'CONN_param' in '[service]' registry section */
        buf[slen++] = '\0';
        s = buf + slen;
        CORE_REG_GET(buf, s, value, value_size, end ? def_value : 0);
        if (*value  ||  end)
            return value;
    } else {
        /* Common case. Form 'CONN_param' */
        size_t plen = strlen(param) + 1;
        if (strncasecmp(param, DEF_CONN_REG_SECTION "_",
                        sizeof(DEF_CONN_REG_SECTION)) != 0) {
            if (sizeof(DEF_CONN_REG_SECTION) + plen > sizeof(buf))
                return 0;
            s = buf;
            memcpy(s, DEF_CONN_REG_SECTION, sizeof(DEF_CONN_REG_SECTION) - 1);
            s += sizeof(DEF_CONN_REG_SECTION) - 1;
            *s++ = '_';
        } else {
            if (plen > sizeof(buf))
                return 0;
            s = buf;
        }
        memcpy(s, param, plen);
        s = strupr(buf);
    }

    *generic = 1/*true*/;
    /* Environment search for 'CONN_param' */
    if ((val = getenv(s)) != 0)
        return strncpy0(value, val, value_size - 1);

    /* Last resort: Search for 'param' in default registry section */
    s += sizeof(DEF_CONN_REG_SECTION);
    CORE_REG_GET(DEF_CONN_REG_SECTION, s, value, value_size, def_value);
    return value;
}


static const char* s_GetValue(const char* service, const char* param,
                              char* value, size_t value_size,
                              const char* def_value, int* /*bool*/ generic)
{
    const char* retval = x_GetValue(service, param,
                                    value, value_size, def_value, generic);
    if (retval) {
        /*strip enveloping quotes*/
        size_t len = strlen(value);
        if (len > 1  &&  (value[0] == '"'  ||  value[0] == '\'')
            &&  strchr(value + 1, value[0]) == value + len - 1) {
            if (len -= 2)
                memcpy(value, value + 1, len);
            value[len] = '\0';
        }
        assert(retval == value);
    }
    return retval;
}


extern const char* ConnNetInfo_GetValue(const char* service, const char* param,
                                        char* value, size_t value_size,
                                        const char* def_value)
{
    int/*bool*/ dummy;
    return s_GetValue(service, param, value, value_size, def_value, &dummy);
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
    if (len == 5  &&  strncasecmp(str, "https", len) == 0)
        return eURL_Https;
    if (len == 4  &&  strncasecmp(str, "http",  len) == 0)
        return eURL_Http;
    if (len == 4  &&  strncasecmp(str, "file",  len) == 0)
        return eURL_File;
    if (len == 3  &&  strncasecmp(str, "ftp",   len) == 0)
        return eURL_Ftp;
    return eURL_Unspec;
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
        return 0;
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
    return x_Num(scheme, buf);
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
        str += n;
        if (generic)
            SERV_AddFirewallPort(port);
        if (!*(str += strspn(str, " \t")))
            return eFWMode_Fallback;
    }
    return eFWMode_Legacy;
}


/****************************************************************************
 * ConnNetInfo API
 */


extern SConnNetInfo* ConnNetInfo_Create(const char* service)
{
#define REG_VALUE(name, value, def_value)                               \
    s_GetValue(service, name, value, sizeof(value), def_value, &generic)

    int/*bool*/ generic;
    SConnNetInfo* info;
    /* aux. storage */
    char   str[1024];
    size_t len;
    int    val;
    double dbl;
    char*  e;

    len = service ? strlen(service) : 0;

    /* NB: *NOT* cleared up with all 0s */
    if (!(info = (SConnNetInfo*) malloc(sizeof(*info) + len)))
        return 0/*failure*/;

    /* client host: default */
    info->client_host[0] = '\0';

    /* scheme */
    info->scheme = eURL_Unspec;

    /* request method */
    REG_VALUE(REG_CONN_REQ_METHOD, str, DEF_CONN_REQ_METHOD);
    if (!*str  ||  strcasecmp(str, "ANY") == 0)
        info->req_method = eReqMethod_Any;
    else if (strcasecmp(str, "POST") == 0)
        info->req_method = eReqMethod_Post;
    else if (strcasecmp(str, "GET") == 0)
        info->req_method = eReqMethod_Get;
    /* NB: HEAD, CONNECT, etc not allowed here */

    /* compatibility */
    info->version = 0;

    /* firewall mode */
    REG_VALUE(REG_CONN_FIREWALL, str, DEF_CONN_FIREWALL);
    info->firewall = x_ParseFirewall(str, generic);

    /* stateless client? */
    REG_VALUE(REG_CONN_STATELESS, str, DEF_CONN_STATELESS);
    info->stateless = ConnNetInfo_Boolean(str);

    /* prohibit use of the local load balancer? */
    REG_VALUE(REG_CONN_LB_DISABLE, str, DEF_CONN_LB_DISABLE);
    info->lb_disable = ConnNetInfo_Boolean(str);

    /* turn on debug printout? */
    REG_VALUE(REG_CONN_DEBUG_PRINTOUT, str, DEF_CONN_DEBUG_PRINTOUT);
    if (ConnNetInfo_Boolean(str)
        ||    (*str  &&   strcasecmp(str, "some") == 0)) {
        info->debug_printout = eDebugPrintout_Some;
    } else if (*str  &&  (strcasecmp(str, "all")  == 0  ||
                          strcasecmp(str, "data") == 0)) {
        info->debug_printout = eDebugPrintout_Data;
    } else
        info->debug_printout = eDebugPrintout_None;

    /* username */
    REG_VALUE(REG_CONN_USER, info->user, DEF_CONN_USER);

    /* password */
    REG_VALUE(REG_CONN_PASS, info->pass, DEF_CONN_PASS);

    /* hostname */
    REG_VALUE(REG_CONN_HOST, info->host, DEF_CONN_HOST);

    /* port # */
    REG_VALUE(REG_CONN_PORT, str, DEF_CONN_PORT);
    errno = 0;
    if (*str  &&  (val = strtoul(str, &e, 10)) > 0  &&  !errno
        &&  !*e  &&  val < (1 << 16)) {
        info->port = val;
    } else
        info->port = 0/*default*/;

    /* path */
    REG_VALUE(REG_CONN_PATH, info->path, DEF_CONN_PATH);

    /* args */
    REG_VALUE(REG_CONN_ARGS, info->args, DEF_CONN_ARGS);

    /* HTTP proxy server? */
    REG_VALUE(REG_CONN_HTTP_PROXY_HOST, info->http_proxy_host,
              DEF_CONN_HTTP_PROXY_HOST);
    if (*info->http_proxy_host) {
        /* yes, use the specified HTTP proxy server */
        REG_VALUE(REG_CONN_HTTP_PROXY_PORT, str, DEF_CONN_HTTP_PROXY_PORT);
        errno = 0;
        if (*str  &&  (val = strtoul(str, &e, 10)) > 0
            &&  !errno  &&  !*e  &&  val < (1 << 16)) {
            info->http_proxy_port = val;
        } else
            info->http_proxy_port =  0/*none*/;
        /* HTTP proxy username */
        REG_VALUE(REG_CONN_HTTP_PROXY_USER, info->http_proxy_user,
                  DEF_CONN_HTTP_PROXY_USER);
        /* HTTP proxy password */
        REG_VALUE(REG_CONN_HTTP_PROXY_PASS, info->http_proxy_pass,
                  DEF_CONN_HTTP_PROXY_PASS);
        /* HTTP proxy leakout */
        REG_VALUE(REG_CONN_HTTP_PROXY_LEAK, str, DEF_CONN_HTTP_PROXY_LEAK);
        info->http_proxy_leak    =   ConnNetInfo_Boolean(str);
    } else {
        info->http_proxy_port    =   0;
        info->http_proxy_user[0] = '\0';
        info->http_proxy_pass[0] = '\0';
        info->http_proxy_leak    =   0;
    }

    /* non-transparent CERN-like firewall proxy server? */
    REG_VALUE(REG_CONN_PROXY_HOST, info->proxy_host, DEF_CONN_PROXY_HOST);

    /* max. # of attempts to establish connection */
    REG_VALUE(REG_CONN_MAX_TRY, str, 0);
    val = atoi(str);
    info->max_try = (unsigned short)(val > 0 ? val : DEF_CONN_MAX_TRY);

    /* connection timeout */
    REG_VALUE(REG_CONN_TIMEOUT, str, 0);
    len = strlen(str);
    if (len < 3  ||  8 < len  ||  strncasecmp(str, "infinite", len) != 0) {
        if (!*str || (dbl = NCBI_simple_atof(str, &e)) < 0.0 || errno || *e)
            dbl = DEF_CONN_TIMEOUT;
        info->tmo.sec      = (unsigned int)  dbl;
        info->tmo.usec     = (unsigned int)((dbl - info->tmo.sec) * 1000000.0);
        if (dbl  &&  !(info->tmo.sec | info->tmo.usec))
            info->tmo.usec = 1/*protect underflow*/;
        info->timeout      = &info->tmo;
    } else
        info->timeout      = kInfiniteTimeout/*0*/;

    /* HTTP user header */
    REG_VALUE(REG_CONN_HTTP_USER_HEADER, str, DEF_CONN_HTTP_USER_HEADER);
    info->http_user_header = *str ? x_StrcatCRLF(NULL, str) : 0;

    /* default referer */
    ConnNetInfo_GetValue(0, REG_CONN_HTTP_REFERER, str, sizeof(str),
                         DEF_CONN_HTTP_REFERER);
    info->http_referer = *str ? strdup(str) : 0;

    /* credentials */
    info->credentials = 0;

    /* store the service name, which this structure has been created for */
    strcpy((char*) info->svc, service ? service : "");

    /* done */
    return info;
#undef REG_VALUE
}


extern int/*bool*/ ConnNetInfo_ParseURL(SConnNetInfo* info, const char* url)
{
    /* URL elements and their parsed lengths as passed */
    const char *user,   *pass,   *host,   *path,   *args;
    size_t     userlen, passlen, hostlen, pathlen, argslen;
    unsigned short port;
    EURLScheme scheme;
    const char* s;
    size_t len;
    char* p;

    if (!url)
        return 0/*failure*/;
    if (!*url)
        return 1/*success*/;

    if ((info->req_method & ~eReqMethod_v1) == eReqMethod_Connect) {
        len = strlen(url);
        s = (const char*) memchr(url, ':', len);
        if (s)
            len = (size_t)(s - url);
        if (len >= sizeof(info->host))
            return 0/*failure*/;
        if (s) {
            long i;
            errno = 0;
            i = strtol(++s, &p, 10);
            if (errno  ||  s == p  ||  !i  ||  i ^ (i & 0xFFFF)  ||  *p)
                return 0/*failure*/;
            info->port = (unsigned short) i;
        }
        if (len) {
            memcpy(info->host, url, len);
            info->host[len] = '\0';
        }
        return 1/*success*/;
    }

    /* "user:pass@host:port" first [any optional] */
    if ((s = strstr(url, "://")) != 0) {
        len = (size_t)(s - url);
        if ((scheme = x_ParseScheme(url, len)) == eURL_Unspec)
            return 0/*failure*/;

        host    = s + 3;
        hostlen = strcspn(host, "/?#");
        path    = host + hostlen;

        /* username:password */
        if (!hostlen) {
            if (scheme != eURL_File)
                return 0/*failure*/;
            user = pass = host = "";
            userlen = passlen = 0;
            port = 0/*none*/;
        } else {
            if (!(s = (const char*) memrchr(host, '@', hostlen))) {
                user    = pass    = "";
                userlen = passlen = 0;
            } else {
                user    = host;
                userlen = (size_t)(s - user);
                host    = ++s;
                hostlen = (size_t)(path - s);
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
                long i;
                hostlen = (size_t)(s - host);
                errno = 0;
                i = strtol(++s, &p, 10);
                if (errno  ||  s == p  || !i || i ^ (i & 0xFFFF) ||  p != path)
                    return 0/*failure*/;
                port = (unsigned short) i;
            } else
                port = 0/*default*/;

            if (userlen >= sizeof(info->user)  ||
                passlen >= sizeof(info->pass)  ||
                hostlen >= sizeof(info->host)) {
                return 0/*failure*/;
            }
        }
    } else {
        scheme  = (EURLScheme) info->scheme;
        user    = pass    = host    = 0;
        userlen = passlen = hostlen = 0;
        path    = url;
        port    = 0;
    }

    pathlen = (scheme == eURL_Https  ||  scheme == eURL_Http
               ? strcspn(path, "?#") : strlen(path));
    args    = path + pathlen;

    if (path != url  ||  *path == '/') {
        /* absolute path */
        p = info->path;
        len = 0;
        if (!pathlen) {
            path    = "/";
            pathlen = 1;
        }
    } else {
        /* relative path */
        if (!(p = strrchr(info->path, '/'))) {
            p = info->path;
            len = 0;
        } else
            len = (size_t)(++p - info->path);
        if (!pathlen)
            path = 0;
    }
    if (pathlen + len >= sizeof(info->path))
        return 0/*failure*/;

    /* arguments and fragment */
    if (*args) {
        const char* frag;
        argslen = strlen(args);
        if (*args == '#')
            frag = args;
        else if (!(frag = strchr(++args/*NB: args[0]=='?'*/, '#')))
            frag = args + --argslen;
        else
            argslen--;
        assert(!*frag  ||  *frag == '#');

        if (*frag) {
            /* if there is a new fragment, the entire args get overridden */
            if (!frag[1])
                argslen--; /* don't store the empty fragment # */
            if (argslen >= sizeof(info->args))
                return 0/*failure*/;
            len = 0;
        } else if ((s = strchr(info->args, '#')) != 0) {
            /* there is no new fragment, but there was the old one: keep it */
            len = strlen(s);
            if (argslen + len >= sizeof(info->args))
                return 0/*failure*/;
            memmove(info->args + argslen, s, len);
        } else {
            if (argslen >= sizeof(info->args))
                return 0/*failure*/;
            len = 0;
        }
        memcpy(info->args, args, argslen);
        info->args[argslen + len] = '\0';
    } else if ((scheme == eURL_Https  ||  scheme == eURL_Http)
               &&  (args = strchr(info->args, '#'))) {
        /* keep the old fragment, if any, but drop all args */
        memmove(info->args, args, strlen(args) + 1);
    } else
        info->args[0] = '\0';
    if (path) {
        memcpy(p, path, pathlen);
        p[pathlen] = '\0';
    }
    if (user) {
        assert(pass);
        memcpy(info->user, user, userlen);
        info->user[userlen] = '\0';
        memcpy(info->pass, pass, passlen);
        info->pass[passlen] = '\0';
    }
    if (host) {
        memcpy(info->host, host, hostlen);
        info->host[hostlen] = '\0';
        info->port = port;
    }
    info->scheme = scheme;
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_SetUserHeader(SConnNetInfo* info,
                                             const char*   user_header)
{
    if (info->http_user_header)
        free((void*) info->http_user_header);
    if (!user_header  ||  !*user_header)
        info->http_user_header = 0;
    else if (!(info->http_user_header = x_StrcatCRLF(NULL, user_header)))
        return 0/*failure*/;
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_AppendUserHeader(SConnNetInfo* info,
                                                const char*   user_header)
{
    char* new_header;

    if (!info->http_user_header  ||  !*info->http_user_header)
        return ConnNetInfo_SetUserHeader(info, user_header);

    new_header = (char*) info->http_user_header;
    if (!(new_header = x_StrcatCRLF(new_header, user_header)))
        return 0/*failure*/;

    info->http_user_header = new_header;
    return 1/*success*/;
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

    if (!user_header || !(newhdrlen = strlen(user_header)))
        return 1/*success*/;

    if (!(hdr = (char*) info->http_user_header) || !(hdrlen = strlen(hdr))) {
        if (op == eUserHeaderOp_Delete)
            return 1/*success*/;
        if (!hdr && !(hdr = strdup("")))
            return 0/*failure*/;
        hdrlen = 0;
    }

    /* NB: "user_header" can be part of "info->user_header",
     * so create a copy of it even for delete operations! */
    if (!(newhdr = (char*) malloc(newhdrlen + 1)))
        return 0/*failure*/;
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
            assert(0);
            retval = 0/*failure*/;
            newlen = 0;
            break;
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
                    line += linelen-off;
                    linelen = off;
                    newlen++;
                    len = 0;
                } else
                    len = linelen-off;
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

    info->http_user_header = hdr;
    if (retval  &&  op != eUserHeaderOp_Delete)
        retval = ConnNetInfo_AppendUserHeader(info, newhdr);
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


extern int/*bool*/ ConnNetInfo_AppendArg(SConnNetInfo* info,
                                         const char*   arg,
                                         const char*   val)
{
    size_t len, used;

    if (!arg || !*arg)
        return 1/*success*/;

    used = strlen(info->args);
    len  = strlen(arg);
    
    if (used + (used ? 1/*&*/ : 0) + len +
        (val && *val ? 1/*=*/ + strlen(val) : 0) >= sizeof(info->args)) {
        return 0/*failure*/;
    }

    if (used)
        info->args[used++] = '&';
    strcpy(info->args + used, arg);
    if (val && *val) {
        used += len;
        info->args[used++] = '=';
        strcpy(info->args + used, val);
    }
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_PrependArg(SConnNetInfo* info,
                                          const char*   arg,
                                          const char*   val)
{
    size_t len, off, used;

    if (!arg || !*arg)
        return 1/*success*/;

    used = strlen(info->args);
    len  = strlen(arg);
    off  = len + (val && *val ? 1/*=*/ + strlen(val) : 0) + (used? 1/*&*/ : 0);

    if (off + used >= sizeof(info->args))
        return 0/*failure*/;

    if (used)
        memmove(info->args + off, info->args, used + 1);
    strcpy(info->args, arg);
    if (val && *val) {
        info->args[len++] = '=';
        strcpy(info->args + len, val);
    }
    if (used)
        info->args[off - 1] = '&';
    return 1/*success*/;
}


extern int/*bool*/ ConnNetInfo_DeleteArg(SConnNetInfo* info,
                                         const char*   arg)
{
    int/*bool*/ deleted;
    size_t argnamelen;
    size_t arglen;
    char*  a;

    if (!arg || !(argnamelen = strcspn(arg, "=&")))
        return 0/*false*/;
    deleted = 0/*false*/;
    for (a = info->args; *a; a += arglen) {
        if (*a == '&')
            a++;
        arglen = strcspn(a, "&");
        if (arglen < argnamelen || strncasecmp(a, arg, argnamelen) != 0 ||
            (a[argnamelen] && a[argnamelen] != '=' && a[argnamelen] != '&')) {
            continue;
        }
        if (!a[arglen]) {
            if (a == info->args)
                *a = '\0';    /* the only argument removed */
            else
                *--a = '\0';  /* last argument: also remove trailing '&' */
            return 1/*true*/;
        }
        arglen++;  /* for intermediary args, eat the following '&' separator */
        memmove(a, a + arglen, strlen(a + arglen) + 1);
        deleted = 1/*true*/;
        arglen = 0;
    }
    return deleted;
}


extern void ConnNetInfo_DeleteAllArgs(SConnNetInfo* info,
                                      const char*   args)
{
    if (!args)
        return;
    while (*args) {
        const char* a = strchr(args, '&');
        if (!a)
            a = args + strlen(args);
        else
            a++;
        ConnNetInfo_DeleteArg(info, args);
        args = a;
    }
}


extern int/*bool*/ ConnNetInfo_PreOverrideArg(SConnNetInfo* info,
                                              const char*   arg,
                                              const char*   val)
{
    if (!arg || !*arg)
        return 1/*success*/;
    ConnNetInfo_DeleteAllArgs(info, arg);
    return ConnNetInfo_PrependArg(info, arg, val);
}


extern int/*bool*/ ConnNetInfo_PostOverrideArg(SConnNetInfo* info,
                                               const char*   arg,
                                               const char*   val)
{
    if (!arg || !*arg)
        return 1/*success*/;
    ConnNetInfo_DeleteAllArgs(info, arg);
    return ConnNetInfo_AppendArg(info, arg, val);
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
        client_host = s;

    if (x_IsSufficientAddress(client_host)       ||
        !(ip = *client_host  &&  !local_host
          ? SOCK_gethostbyname(client_host)
          : SOCK_GetLocalHostAddress(eDefault))  ||
        SOCK_ntoa(ip, addr, sizeof(addr)) != 0   ||
        !(s = (char*) malloc(strlen(client_host) + strlen(addr) + 3))) {
        return client_host/*least we can do :-/*/;
    }

    sprintf(s, "%s(%s)", client_host, addr);
    if (client_host != c)
        free((void*) client_host);
    client_host = s;
    for (;  *s;  ++s) {
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

    if (!info)
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


extern SConnNetInfo* ConnNetInfo_Clone(const SConnNetInfo* info)
{
    SConnNetInfo* x_info;

    if (!info)
        return 0;

    if (!(x_info = (SConnNetInfo*) malloc(sizeof(*info) + strlen(info->svc))))
        return 0;

    memcpy(x_info, info, sizeof(*x_info));
    x_info->http_user_header = 0;
    x_info->http_referer = 0;

    if (info->timeout)
        x_info->timeout = &x_info->tmo;
    if (info->http_user_header
        &&  !(x_info->http_user_header = strdup(info->http_user_header))) {
        ConnNetInfo_Destroy(x_info);
        return 0;
    }
    if (x_info->http_referer
        &&  !(x_info->http_referer = strdup(info->http_referer))) {
        ConnNetInfo_Destroy(x_info);
        return 0;
    }
    strcpy((char*) x_info->svc, info->svc);
    return x_info;
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
    req_method &= ~eReqMethod_v1;
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
    case eReqMethod_Trace:
        return "TRACE";
    case eReqMethod_Delete:
        return "DELETE";
    case eReqMethod_Options:
        return "OPTIONS";
    default:
        break;
    }
    return x_Num(req_method, buf);
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
    int who, what;
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
    assert(str);
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
                  "ConnNetInfo_Log: NULL info", 0, 0);
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
    if (*info->svc)
        s_SaveString(s, "service",         info->svc);
    else
        s_SaveKeyval(s, "service",         "NONE");
    if (*info->client_host)
        s_SaveString(s, "client_host",     info->client_host);
    else
        s_SaveKeyval(s, "client_host",     "(default)");
    s_SaveKeyval    (s, "scheme",         (info->scheme
                                           ? x_Scheme((EURLScheme)info->scheme,
                                                      buf)
                                           : "(unspec)"));
    s_SaveKeyval    (s, "req_method",      x_ReqMethod(info->req_method
                                                       | (info->version
                                                          ? eReqMethod_v1
                                                          : 0), buf));
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
    s_SaveString    (s, "args",            info->args);
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
        s_SaveKeyval(s, "http_proxy_pass",(info->http_proxy_pass[0]
                                           ? "(set)" : "(ignored)"));
    } else
        s_SaveString(s, "http_proxy_pass", info->http_proxy_pass);
    s_SaveBool      (s, "http_proxy_leak", info->http_proxy_leak);
    s_SaveString    (s, "proxy_host",      info->proxy_host);
    s_SaveULong     (s, "max_try",         info->max_try);
    if (info->timeout) {
        s_SaveULong (s, "timeout(sec)",    info->timeout->sec);
        s_SaveULong (s, "timeout(usec)",   info->timeout->usec);
    } else
        s_SaveKeyval(s, "timeout",         "INFINITE");
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
    const char* args;
    size_t      len;
    char*       url;
    char        buf[40];

    if (!info)
        return 0/*failed*/;

    req_method = info->req_method & ~eReqMethod_v1;
    scheme = x_Scheme((EURLScheme) info->scheme, buf);
    if ((!scheme  &&  req_method != eReqMethod_Connect)  ||
        ( scheme  &&  !isalpha((unsigned char)(*scheme)))) {
        return 0/*failed*/;
    }

    if (req_method == eReqMethod_Connect) {
        scheme = "";
        schlen = 0;
        path = 0;
        args = "";
        len = 0;
    } else {
        assert(scheme);
        schlen = strlen(scheme);
        path = info->path;
        args = info->args;
        len = schlen+3/*://*/ + strlen(path) + (*args ? strlen(args) + 2 : 1);
    }
    len += strlen(info->host) + 7/*:port\0*/;

    url = (char*) malloc(len);
    if (url) {
        assert(scheme  &&  args);
        strlwr((char*) memcpy(url, scheme, schlen + 1));
        len  = schlen;
        len += sprintf(url + len, "://%s" + (schlen ? 0 : 3), info->host);
        if (info->port  ||  !path/*req_method == eReqMethod_Connect*/)
            len += sprintf(url + len, ":%hu", info->port);
        sprintf(url + len, "%s%s%s%s",
                &"/"[! path  ||  *path == '/'], path ? path : "",
                &"?"[!*args  ||  *args == '#'], args);
    }
    assert(!url  ||  *url);
    return url;
}


extern int/*bool*/ ConnNetInfo_SetTimeout(SConnNetInfo*   info,
                                          const STimeout* timeout)
{
    if (!info  ||  timeout == kDefaultTimeout)
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
    if (info) {
        ConnNetInfo_SetUserHeader(info, 0);
        if (info->http_referer) {
            free((void*) info->http_referer);
            info->http_referer = 0;
        }
        free(info);
    }
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
    static const char* kHttp[] = { " HTTP/1.0\r\n",
                                   " HTTP/1.1\r\n" };
    static const char  kHost[] = "Host: ";
    SOCK        s;
    BUF         buf;
    char*       hdr;
    unsigned    ver;
    SSOCK_Init  init;
    EIO_Status  status;
    int         add_hdr;
    size_t      hdr_len;
    char        hdr_buf[80];
    size_t      args_len;
    EReqMethod  x_req_meth;
    const char* x_req_meth_str;
    size_t      user_hdr_len = user_hdr  &&  *user_hdr ? strlen(user_hdr) : 0;

    /* sanity check first */
    if (!sock  ||  !host  ||  !*host  ||  !path  ||  !*path
        ||  (user_hdr  &&  *user_hdr  &&  user_hdr[user_hdr_len - 1] != '\n')){
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

    /* FIXME:  check to be removed */
    if (cred  &&  cred < (NCBI_CRED) 4096) {
        CORE_LOG(eLOG_Critical, "[URL_ConnectEx]  Obsolete feature"
                 " 'encode_args' is no longer supported");
        return x_URLConnectErrorReturn(s, eIO_InvalidArg);
    }

    ver = req_method >= eReqMethod_v1 ? 1 : 0;
    x_req_meth = (EReqMethod)(req_method & ~eReqMethod_v1);
    /* select request method and its verbal representation */
    if (x_req_meth == eReqMethod_Any)
        x_req_meth  = content_length ? eReqMethod_Post : eReqMethod_Get;
    else if (content_length  &&  (x_req_meth == eReqMethod_Get  ||
                                  x_req_meth == eReqMethod_Head)) {
        CORE_LOGF_X(3, eLOG_Warning,
                    ("[URL_Connect] "
                     " Content-Length (%lu) is ignored with request method %s",
                     (unsigned long) content_length,
                     x_req_meth == eReqMethod_Get ? "GET" : "HEAD"));
        content_length = 0;
    }

    switch (x_req_meth | (ver ? eReqMethod_v1 : 0)/*FIXME: to remove*/) {
    case eReqMethod_Get:
        x_req_meth_str = "GET ";
        add_hdr = 1;
        break;
    case eReqMethod_Post:
        x_req_meth_str = "POST ";
        add_hdr = 1;
        break;
    case eReqMethod_Head:
        x_req_meth_str = "HEAD ";
        add_hdr = 1;
        break;
    case eReqMethod_Connect:
        x_req_meth_str = "CONNECT ";
        add_hdr = 0;
        break;
    default:
        CORE_LOGF_X(4, eLOG_Error,
                    ("[URL_Connect; //%s:%hu%s%s] "
                     " Unsupported request method %s",
                     host, port, &"/"[*path == '/'], path,
                     x_ReqMethod(req_method, hdr_buf)));
        assert(0);
        return x_URLConnectErrorReturn(s, eIO_NotSupported);
    }

    hdr_len = 0;
    if (add_hdr) {
        const char* temp;
        assert(x_req_meth != eReqMethod_Connect);
        /* FIXME: This to be moved to URL_Connect() */
        for (temp = user_hdr;  temp  &&  *temp;  temp = strchr(temp, '\n')) {
            if (temp != user_hdr)
                temp++;
            if (strncasecmp(temp, kHost, sizeof(kHost) - 2) == 0) {
                add_hdr = 0;
                break;
            }
        }
        args_len = args ? strcspn(args, "#") : 0;
        if (!port)
            port = flags & fSOCK_Secure ? CONN_PORT_HTTPS : CONN_PORT_HTTP;
        else if (add_hdr)
            hdr_len = (size_t) sprintf(hdr_buf, ":%hu", port);
    } else
        args_len = 0;

    buf = 0;
    errno = 0;
    /* compose HTTP header */
    if (/* METHOD <path>[?<args>] HTTP/1.x\r\n */
        !BUF_Write(&buf, x_req_meth_str, strlen(x_req_meth_str))  ||
        !BUF_Write(&buf, path,           strlen(path))            ||
        (args_len
         &&  (!BUF_Write(&buf, "?",        1)                     ||
              !BUF_Write(&buf, args,       args_len)))            ||
        !BUF_Write      (&buf, kHttp[ver], strlen(kHttp[ver]))    ||

        (add_hdr
         /* Host: host[:port]\r\n */
         &&  (!BUF_Write(&buf, kHost,   sizeof(kHost) - 1)        ||
              !BUF_Write(&buf, host,    strlen(host))             ||
              !BUF_Write(&buf, hdr_buf, hdr_len)                  ||
              !BUF_Write(&buf, "\r\n",  2)))                      ||

        /* Content-Length: <content_length>\r\n */
        (x_req_meth == eReqMethod_Post/*NB: HTTP/1.0 only*/  &&  !ver
         &&  ((add_hdr
               = sprintf(hdr_buf, "Content-Length: %lu\r\n",
                         (unsigned long) content_length)) <= 0    ||
              !BUF_Write(&buf, hdr_buf,  (size_t) add_hdr)))      ||

        /* <user_header> */
        (user_hdr_len
         &&  !BUF_Write (&buf, user_hdr, user_hdr_len))           ||

        /* header separator */
        !BUF_Write(&buf, "\r\n", 2)                               ||

        /* tunneled data */
        (x_req_meth == eReqMethod_Connect  &&  content_length
         &&  !BUF_Write (&buf, args,     content_length))) {
        int x_errno = errno;
        CORE_LOGF_ERRNO_X(5, eLOG_Error, x_errno,
                          ("[URL_Connect; //%s:%hu%s%s%s%.*s] "
                           " Cannot build HTTP header",
                           host, port, &"/"[*path == '/'], path,
                           &"?"[!args_len], (int) args_len, args));
        BUF_Destroy(buf);
        return x_URLConnectErrorReturn(s, eIO_Unknown);
    }

    if (!(hdr = (char*) malloc(hdr_len = BUF_Size(buf)))
        ||  BUF_Read(buf, hdr, hdr_len) != hdr_len) {
        int x_errno = errno;
        CORE_LOGF_ERRNO_X(6, eLOG_Error, x_errno,
                          ("[URL_Connect; //%s:%hu%s%s%s%.*s] "
                           " Cannot maintain HTTP header",
                           host, port, &"/"[*path == '/'], path,
                           &"?"[!args_len], (int) args_len, args));
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
        /* re-use existing connection */
        status = SOCK_CreateOnTopInternal(s/*old*/, 0, sock/*new*/,
                                          &init, flags);
        SOCK_Destroy(s);
    } else {
        /* connect to HTTPD */
        status = SOCK_CreateInternal(host, port, o_timeout, sock/*new*/,
                                     &init, flags);
    }
    free(hdr);

    if (status != eIO_Success) {
        assert(!*sock);
        if (status == eIO_Timeout  &&  o_timeout) {
            sprintf(hdr_buf, "[%u.%06u]",
                    (unsigned int)(o_timeout->sec + o_timeout->usec/1000000),
                    (unsigned int)                 (o_timeout->usec%1000000));
        } else
            *hdr_buf = '\0';
        CORE_LOGF_X(7, eLOG_Error,
                    ("[URL_Connect; //%s:%hu%s%s%s%.*s] "
                     " Failed to %s: %s%s",
                     host, port, &"/"[*path == '/'], path,
                     &"?"[!args_len], (int) args_len, args,
                     s ? "use connection" : "connect",
                     IO_StatusStr(status), hdr_buf));
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
    SOCK   sock;
    char*  temp;
    size_t args_len;

    if (req_method >= eReqMethod_v1)
        req_method  = (EReqMethod)((int) eReqMethod_v1 - 1)/*bad*/;
    if (req_method != eReqMethod_Connect  &&  args  &&  encode_args
        &&  (args_len = strcspn(args, "#")) > 0) {
        /* URL-encode "args", if any specified */
        size_t rd_len, wr_len;
        size_t size = 3 * args_len;
        if (!(temp = (char*) malloc(size + 1))) {
            CORE_LOGF_ERRNO_X(8, eLOG_Error, errno,
                              ("[URL_Connect]  Out of memory (%lu)",
                               (unsigned long)(size + 1)));
            return 0;
        }
        URL_Encode(args, args_len, &rd_len, temp, size, &wr_len);
        assert(rd_len == args_len);
        assert(wr_len <= size);
        temp[wr_len] = '\0';
        args = temp;
    } else
        temp = 0;

    sock = 0;
    verify(URL_ConnectEx(host, port, path, args,
                         req_method, content_length,
                         o_timeout, rw_timeout,
                         user_hdr, 0/*cred*/, flags, &sock) == eIO_Success
           ||  !sock);

    if (temp)
        free(temp);
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
    EIO_Status status;
    size_t     n_read;
    size_t     buf_size;
    char       x_buf[4096];

    /* check args */
    if ( n_discarded )
        *n_discarded = 0;
    if (!stream  ||  (pattern != 0) != (pattern_size != 0))
        return eIO_InvalidArg;

    /* allocate a temporary read buffer */
    buf_size = pattern_size << 1;
    if (buf_size <= sizeof(x_buf)) {
        buf_size  = sizeof(x_buf);
        buf = x_buf;
    } else if ( !(buf = (char*) malloc(buf_size)) )
        return eIO_Unknown;

    if ( !pattern ) {
        /* read/discard until EOF */
        do {
            status = io_func(stream, buf, buf_size, &n_read, eIO_Read);
            if ( discard )
                BUF_Write(discard, buf, n_read);
            if ( n_discarded )
                *n_discarded += n_read;
        } while (status == eIO_Success);
    } else {
        n_read = 0;
        for (;;) {
            /* read; search for the pattern; store the discarded data */
            size_t x_read, n_stored;

            assert(n_read < pattern_size);
            status = io_func(stream, buf + n_read, buf_size - n_read,
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
                for (b = buf;  n_check;  b++, n_check--) {
                    if (*b != *((const char*) pattern))
                        continue;
                    if (memcmp(b, pattern, pattern_size) == 0)
                        break; /*found*/
                }
                /* pattern found */
                if ( n_check ) {
                    size_t x_discarded = (size_t)(b - buf) + pattern_size;
                    if ( discard )
                        BUF_Write(discard, buf + n_read, x_discarded - n_read);
                    if ( n_discarded )
                        *n_discarded += x_discarded - n_read;
                    /* return unused portion to the stream */
                    status = io_func(stream, buf + x_discarded,
                                     n_stored - x_discarded, 0, eIO_Write);
                    break; /*finished*/
                }
            }

            /* pattern not found yet */
            if ( discard )
                BUF_Write(discard, buf + n_read, x_read);
            if ( n_discarded )
                *n_discarded += x_read;

            if (n_stored >= pattern_size) {
                n_read    = pattern_size - 1;
                memmove(buf, buf + n_stored - n_read, n_read);
            } else
                n_read    = n_stored;
        }
    }

    /* cleanup & exit */
    if (buf != x_buf)
        free(buf);
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
        return *n_read ? eIO_Success : eIO_Closed;
    case eIO_Write:
        assert(stream);
        b = (BUF) stream;
        return BUF_PushBack(&b, buf, size) ? eIO_Success : eIO_Unknown;
    default:
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


/* Return integer (0..15) corresponding to the "ch" as a hex digit
 * Return -1 on error
 */
static int s_HexChar(char ch)
{
    unsigned int rc = ch - '0';
    if (rc <= 9)
        return rc;
    rc = (ch | ' ') - 'a';
    return rc <= 5 ? (int) rc + 10 : -1;
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
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return 1/*true*/;
    if (!src  ||  !dst)
        return 0/*false*/;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
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
                    *dst = (unsigned char)((i1 << 4) + i2);
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


extern void URL_EncodeEx
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
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
        const char* subst = allow_symbols ? strchr(allow_symbols, *src) : 0;
        if (!subst)
            subst = s_EncodeTable[*src];
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
    "octet-stream",
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
    const char* x_type;
    const char* x_subtype;
    const char* x_encoding;
    char        x_buf[MAX_CONTENT_TYPE_LEN];

    assert(buf  &&  buflen);

    if (type == eMIME_T_Undefined  ||  subtype == eMIME_Undefined)
        return 0;
    if (type >= eMIME_T_Unknown)
        type  = eMIME_T_Unknown;
    if (subtype >= eMIME_Unknown)
        subtype  = eMIME_Unknown;
    if (encoding >= eENCOD_Unknown)
        encoding  = eENCOD_Unknown;

    x_type     = s_MIME_Type    [type];
    x_subtype  = s_MIME_SubType [subtype];
    x_encoding = s_MIME_Encoding[encoding];

    if ( *x_encoding ) {
        assert(sizeof(s_ContentType) + strlen(x_type) + strlen(x_subtype)
               + strlen(x_encoding) + 4 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s-%s\r\n",
                s_ContentType, x_type, x_subtype, x_encoding);
    } else {
        assert(sizeof(s_ContentType) + strlen(x_type) + strlen(x_subtype)
               + 3 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s\r\n", s_ContentType, x_type, x_subtype);
    }
    assert(strlen(x_buf) < sizeof(x_buf));
    assert(strlen(x_buf) < buflen);
    strncpy0(buf, x_buf, buflen - 1);
    return buf;
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
        return 0/*false*/;

    if (!(x_buf = (char*) malloc(x_size << 1)))
        return 0/*false*/;
    x_type = x_buf + x_size;

    strlwr(strcpy(x_buf, str));

    if ((sscanf(x_buf, " content-type: %s ", x_type) != 1  &&
         sscanf(x_buf, " %s ", x_type) != 1)  ||
        (x_subtype = strchr(x_type, '/')) == 0) {
        free(x_buf);
        return 0/*false*/;
    }
    *x_subtype++ = '\0';
    x_size = strlen(x_subtype);

    if ( type ) {
        for (i = 0;  i < (int) eMIME_T_Unknown;  i++) {
            if (strcmp(x_type, s_MIME_Type[i]) == 0)
                break;
        }
        *type = (EMIME_Type) i;
    }

    for (i = 1;  i <= (int) eENCOD_Unknown;  i++) {
        size_t len = strlen(s_MIME_Encoding[i]);
        if (len < x_size) {
            char* x_encoding = x_subtype + x_size - len;
            if (x_encoding[-1] == '-'
                &&  strcmp(x_encoding, s_MIME_Encoding[i]) == 0) {
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
        for (i = 0;  i < (int) eMIME_Unknown;  i++) {
            if (strcmp(x_subtype, s_MIME_SubType[i]) == 0)
                break;
        }
        *subtype = (EMIME_SubType) i;
    }

    free(x_buf);
    return 1/*true*/;
}
