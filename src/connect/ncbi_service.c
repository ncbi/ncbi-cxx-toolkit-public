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
#include "ncbi_local.h"
#ifdef NCBI_OS_UNIX
#  include "ncbi_lbsmd.h"
#endif /*NCBI_OS_UNIX*/
#ifdef NCBI_CXX_TOOLKIT
#  include "ncbi_lbnull.h"
#  include "ncbi_lbdns.h"
#  include "ncbi_linkerd.h"
#  include "ncbi_namerd.h"
#endif /*NCBI_CXX_TOOLKIT*/
#include "ncbi_once.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Service


#if 0
#  define x_getenv  NCBI_CORE_GETENV
#else
#  define x_getenv  getenv
#endif


 /*
 * FIXME FIXME FIXME FIXME FIXME ---->
 *
 * NOTE:  For fSERV_ReverseDns lookups the following rules apply to "skip"
 *        contents:  a service would not be selected if there is a same-named
 *        entry with matching host[:port] is found in the "skip" list, or
 *        there is a nameless...
 * NOTE:  Lookup by mask cancels fSERV_ReverseDns mode, if both are used.
 */


#define SERV_SERVICE_NAME_RECURSION_MAX  10
#define CONN_IMPLICIT_SERVER_TYPE        DEF_CONN_REG_SECTION               \
                                         "_" REG_CONN_IMPLICIT_SERVER_TYPE


static ESwitch s_Fast = eOff;


/* Replace any non-alpha / non-digit with '_' */
static int/*bool*/ x_mkenv(char* str, size_t len)
{
    int/*bool*/ mod = 0/*false*/;
    const char* end = str + len;
    while (str != end) {
        unsigned char c = (unsigned char)(*str);
        assert(!isspace(c));
        if (!isalpha(c)  &&  !isdigit(c)  &&  !(c == '_')) {
            mod  = 1/*true*/;
            *str = '_';
        }
        ++str;
    }
    return mod;
}


/* Return the service name length or 0 if the name is not valid.
 * A service name must start with a letter or underscore and be a sequence of
 * (one or more) alphanumeric [incl. underscores] identifiers (which may also
 * include non-consecutive interior minus signs, not adjacent to any underscore)
 * separated by single slashes.  The first identifier must include at least one
 * letter.  Any subsequent identifier(s) may be composed without them.
 * If DNS validation is on ("dns" != 0), then the underscore cannot
 * be the first character, nor can it be in sequence or trailing, or follow
 * a minus sign.  Also, exactly only one identifier is allowed (no slashes).
 * NB:  The NCBI C++ Toolkit registry allows [A_Za-z0-9_-/.] in section names
 * (case-insensitively, by default). */
static size_t x_CheckServiceName(const char* svc, size_t len, int/*bool*/ dns)
{
    int/*bool*/ alpha = 0/*false*/;  /* there was a letter */
    int/*bool*/ minus = 0/*false*/;  /* previous char is a minus sign */
    int/*bool*/ under = 0/*false*/;  /* previous char is an underscore */
    int/*bool*/ delim = 0/*false*/;  /* previous char is either [-/] */
    size_t n;
    for (n = 0;  n < len;  ++n) {
        unsigned char c = (unsigned char)(*svc++);
        if (!n) {
            if ((dns  ||  !(under = (c == '_')))  &&  !(alpha = isalpha(c)))
                return 0;   /* must start with one of the above */
            continue;
        }
        if (isalpha(c)) {
            alpha = 1/*true*/;
            under = delim = minus = 0/*false*/;
            continue;
        }
        if (isdigit(c)) {
            under = delim = minus = 0/*false*/;
            continue;
        }
        switch (c) {
        case '_':
            if (minus)
                return 0;   /* can't follow a '-' */
            if (dns  &&  (under  ||  n == len - 1))
                return 0;   /* for DNS, can't follow a '_' or be trailing */
            under = 1/*true*/;
            delim = minus = 0/*false*/;
            continue;
        case '-':
            if (under  ||  delim  ||  n == len - 1)
                return 0;   /* can't follow [_-/] or be trailing */
            delim = minus = 1/*true*/;
            under = 0/*false*/;
            continue;
        case '/':
            if (dns)
                return 0;   /* not allowed */
            if (!alpha)
                return 0;   /* must follow at least one alpha */
            if (delim  ||  n == len - 1)
                return 0;   /* can't follow [-/] or be trailing */
            under = minus = 0/*false*/;
            delim = 1/*true*/;
            continue;
        default:
            /* all other characters illegal */
            return 0;
        }
    }
    return alpha ? len : 0;
}


#define isdash(s)      ((s) == '-'  ||  (s) == '_')
#ifndef   NS_MAXLABEL
#  define NS_MAXLABEL  63  /* RFC1035, 2.3.4 */
#endif /*!NS_MAXLABEL*/

size_t SERV_CheckDomain(const char* domain)
{
    int/*bool*/ dot = *domain == '.' ? 1/*true*/ : 0/*false*/;
    const char* ptr = dot ? ++domain : domain;
    int/*bool*/ alpha = 0/*false*/;
    size_t len = 0;

    if (!*ptr)
        return 0/*failure: just dot(root) or empty*/;
    for ( ;  *ptr;  ++ptr) {
        if (*ptr == '.') {
            if (dot  ||  (alpha  &&  isdash(ptr[-1])))
                return 0/*failure: double dot or trailing dash*/;
            if (len > NS_MAXLABEL)
                return 0;
            dot = 1/*true*/;
            len = 0;
            continue;
        }
        if ((dot  ||  ptr == domain)  &&  isdash(*ptr))
            return 0/*failure: leading dash*/;
        dot = 0/*false*/;
        ++len;
        if (isdigit((unsigned char)(*ptr)))
            continue;
        if (!isalpha((unsigned char)(*ptr))  &&  !isdash(*ptr))
            return 0/*failure: bad character*/;
        /* at least one regular "letter" seen */
        alpha = 1/*true*/;
    }
    if (len > NS_MAXLABEL)
        return 0/*failure: DNS label too long*/;
    len = (size_t)(ptr - domain);
    assert(len);
    if (domain[len - 1] == '.')
        verify(--len);
    if (!alpha) {
        unsigned int temp;
        ptr = NcbiStringToIPv4(&temp, domain, len);
        assert(!ptr  ||  ptr > domain);
        if (ptr == domain + len)
            return 0/*failure: IPv4 instead of domain*/;
    } else if (isdash(ptr[-1]))
        return 0/*failure: trailing dash*/;
    return len;
}


/* "service" == the original input service name;
 * "svc"     == current service name (NULL at first, init to "service");
 * "url"     == opt ptr to store what "svc" converted to (thru env/reg)
 *              in case it's not a valid service name (retval == NULL);
 * "ismask"  = 1 if "service" is a wildcard pattern to match;
 * "*isfast" = 1 on input if not to perform any env/reg scan;
 * "*isfast" = 1 on output if the service was substituted with itself (but may
 *               be different case);  otherwise, "*isfast" == 0.  This is used
 *               (only) for namerd searches, which are case-sensitive.
 */
static char* x_ServiceName(unsigned int* depth,
                           const char* service, const char* svc, char** url,
                           int/*bool*/ ismask, int*/*bool*/ isfast)
{
    char   buf[128];
    size_t len = 0;

    assert(depth  &&  isfast);
    assert(sizeof(buf) > sizeof(REG_CONN_SERVICE_NAME));

    if (!*depth) {
        assert(!svc);
        svc = service;
        if ( url )
            *url = 0;
    } else
        assert(service  &&  svc  &&  service != svc  &&  !ismask  &&  (!url  ||  !*url));
    if (!svc  ||  (!ismask
                   &&  (!(len = strcspn(svc, "."))
                        ||  !x_CheckServiceName(svc, len, svc[len])
                        ||  (svc[len]  &&  !SERV_CheckDomain(svc + len))
                        ||  len >= sizeof(buf) - sizeof(REG_CONN_SERVICE_NAME)))) {
        if (!svc  ||  !len  ||  len >= sizeof(buf) - sizeof(REG_CONN_SERVICE_NAME)) {
            if (!*depth  ||  strcasecmp(service, svc) == 0)
                service = "";
            CORE_LOGF_X(7, eLOG_Error,
                        ("%s%s%s%s service name%s%s",
                         !svc  ||  !*svc ? "" : "[",
                         !svc ? "" : svc,
                         !svc  ||  !*svc ? "" : "]  ",
                         !svc ? "NULL" : !*svc ? "Empty" : !len ? "Invalid" : "Too long",
                         *service ? " for: " : "", service));
        } else if (url)
            *url = strdup(svc);
        return 0/*failure*/;
    }
    if (!ismask  &&  !*isfast) {
        char  tmp[sizeof(buf)];
        int/*bool*/ tr = x_mkenv((char*) memcpy(tmp, svc, len), len);
        char* s = tmp + len;
        *s++ = '_';
        memcpy(s, REG_CONN_SERVICE_NAME, sizeof(REG_CONN_SERVICE_NAME));
        len += 1 + sizeof(REG_CONN_SERVICE_NAME);
        /* Looking for "svc_CONN_SERVICE_NAME" in the environment */
        if ((!(s = x_getenv(strupr((char*) memcpy(buf, tmp, len--))))
             &&  (memcmp(buf, tmp, len) == 0  ||  !(s = x_getenv(tmp))))
            ||  !*s) {
            /* Looking for "CONN_SERVICE_NAME" in registry section "[svc]" */
            len -= sizeof(REG_CONN_SERVICE_NAME);
            if (tr)
                memcpy(buf, svc, len);  /* re-copy */
            buf[len++] = '\0';
            if (CORE_REG_GET(buf, buf + len, tmp, sizeof(tmp), 0))
                strcpy(buf, tmp);
            else
                *buf = '\0';
            s = buf;
        }
        if (*s) {
            CORE_TRACEF(("[%s]  SERV_ServiceName(\"%s\"): \"%s\"",
                         service, svc, s));
            if (strcasecmp(svc, s) != 0) {
                if (++(*depth) < SERV_SERVICE_NAME_RECURSION_MAX) {
                    char* rv = x_ServiceName(depth, service, s, url, ismask, isfast);
                    if (rv  ||  *depth >= SERV_SERVICE_NAME_RECURSION_MAX)
                        return rv;
                } else {
                    CORE_LOGF_X(8, eLOG_Error,
                                ("[%s]  Maximum service name recursion"
                                 " depth exceeded: %u", service, *depth));
                    return 0/*failure*/;
                }
            } else
                svc = s, *isfast = 1/*true*/;
        } else
            assert(*isfast == 0/*false*/);
    } else
        *isfast = 0/*false*/;
    return strdup(svc);
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static char* s_ServiceName(const char* service, char** url,
                           int/*bool*/ ismask, int*/*bool*/ isfast)
{
    unsigned int depth = 0;
    char* retval;
    CORE_LOCK_READ;
    retval = x_ServiceName(&depth, service, 0/*svc*/, url, ismask, isfast);
    CORE_UNLOCK;
    return retval;
}


char* SERV_ServiceName(const char* service)
{
    int dummy = 0;
    return s_ServiceName(service, 0/*url*/, 0/*ismask*/, &dummy/*isfast*/);
}


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* x_InternalGetNextInfo(SERV_ITER, HOST_INFO*);
static void        x_InternalReset      (SERV_ITER);
static void        x_InternalClose      (SERV_ITER);

static const SSERV_VTable kInternalOp = {
    x_InternalGetNextInfo, 0/*Feedback*/, 0/*Update*/, x_InternalReset, x_InternalClose, "INTERNAL"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SINTERNAL_Data {
    unsigned       reset:1;
    unsigned       reverse_dns:1;
    TSERV_TypeOnly type;
    SSERV_Info*    info;
    SConnNetInfo   net_info;
};


static int/*bool*/ x_InternalResolve(const char* name, struct SINTERNAL_Data* data)
{
    SConnNetInfo*  net_info = &data->net_info;
    unsigned short port = net_info->port;
    TNCBI_IPv6Addr ipv6;
    unsigned int   ipv4;
    SSERV_Info*    info;

    assert(!data->info);
    if (!SOCK_gethostbyname6(&ipv6, net_info->host))
        return 0/*failure*/;

    assert(!NcbiIsEmptyIPv6(&ipv6));
    if (!(ipv4 = NcbiIPv6ToIPv4(&ipv6, 0)))
        ipv4 = (unsigned int)(-1);

    if (data->reverse_dns
        ||  ((data->type & (fSERV_Dns | fSERV_Standalone)) == fSERV_Dns)) {
        info = SERV_CreateDnsInfo(ipv4);
    } else if (data->type & fSERV_Http) {
        char* args = strchr(net_info->path, '?');
        size_t len = SOCK_IsAddress(net_info->host) ? 0 : strlen(net_info->host);
        assert(len <= CONN_HOST_LEN);
        if (args)
            *args++ = '\0';
        info = SERV_CreateHttpInfoEx(data->type, ipv4, 0/*port*/,
                                     net_info->path[0] ? net_info->path : "/",
                                     args,
                                     len ? len + 7/*:port#\0*/ : 0);
        if (args)
            *--args = '?';
        if (info) {
            switch (net_info->scheme) {
            case eURL_Https:
                info->mode |=  fSERV_Secure;
                break;
            case eURL_Http:
                info->mode &= ~fSERV_Secure;
                break;
            default:
                break;
            }
            if (!port)
                port = info->mode & fSERV_Secure ? CONN_PORT_HTTPS : CONN_PORT_HTTP;
            if (len) {
                char* vhost = (char*) info + SERV_SizeOfInfo(info);
                memcpy(vhost, net_info->host, len + 1);
                if ((!(info->mode & fSERV_Secure)  &&  port != CONN_PORT_HTTP)  ||
                    ( (info->mode & fSERV_Secure)  &&  port != CONN_PORT_HTTPS)) {
                    len += sprintf(vhost + len, ":%hu", port);
                    if (len > CONN_HOST_LEN)
                        len = 0/*oops*/;
                }
                assert(len <= CONN_HOST_LEN);
                info->vhost = (unsigned char) len;
                assert((size_t) info->vhost == len);
            }
        }
    } else {
        assert(port);
        info = SERV_CreateStandaloneInfo(ipv4, 0/*port*/);
    }

    if (!info) {
        CORE_LOGF_ERRNO_X(13, eLOG_Error, errno,
                          ("[%s]  INTERNAL cannot create server info", name));
        return 0/*failure*/;
    }
    info->port = port;
    memcpy(&info->addr, &ipv6, sizeof(info->addr)); 

    if (!(data->info = SERV_CopyInfoEx(info, data->reverse_dns ? net_info->host : ""))) {
        CORE_LOGF_ERRNO_X(14, eLOG_Error, errno,
                          ("[%s]  INTERNAL cannot store server info", name));
    }
    free(info);

    return data->info ? 1/*success*/ : 0/*failure*/;
}


static SSERV_Info* x_InternalGetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SINTERNAL_Data* data = (struct SINTERNAL_Data*) iter->data;
    SSERV_Info*            info;

    assert(data);

    CORE_TRACEF(("Enter SERV::x_InternalGetNextInfo(\"%s\")", iter->name));
    if (!data->info  &&  !data->reset) {
        CORE_TRACEF(("Leave SERV::x_InternalGetNextInfo(\"%s\"): EOF", iter->name));
        return 0;
    }
    data->reset = 0/*false*/;

    if (!data->info  &&  !x_InternalResolve(iter->name, data)) {
        assert(!data->info);
        CORE_LOGF_X(12, eLOG_Error,
                    ("[%s]  Unable to resolve", iter->name));
        info = 0;
    } else {
        info = data->info;
        data->info = 0;
        assert(info);
        info->time = LBSM_DEFAULT_TIME + iter->time;
        info->rate = LBSM_DEFAULT_RATE;

        if ( host_info )
            *host_info = 0;
    }

    CORE_TRACEF(("Leave SERV::x_InternalGetNextInfo(\"%s\"): \"%s\" %p",
                 iter->name, info ? SERV_NameOfInfo(info) : "", info));
    return info;
}


static void x_InternalReset(SERV_ITER iter)
{
    struct SINTERNAL_Data* data = (struct SINTERNAL_Data*) iter->data;
    assert(data);
    CORE_TRACEF(("Enter SERV::x_InternalReset(\"%s\"): %u", iter->name,
                 data->info ? 1 : 0));
    if (data->info) {
        free(data->info);
        data->info = 0;
    }
    data->reset = 1/*true*/;
    CORE_TRACEF(("Leave SERV::x_InternalReset(\"%s\")", iter->name));
}


static void x_InternalClose(SERV_ITER iter)
{
    struct SINTERNAL_Data* data = (struct SINTERNAL_Data*) iter->data;
    assert(data  &&  !data->info); /*s_Reset() had to be called before*/
    CORE_TRACEF(("Enter SERV::x_InternalClose(\"%s\")", iter->name));
    iter->data = 0;
    free(data);
    CORE_TRACEF(("Leave SERV::x_InternalClose(\"%s\")", iter->name));
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsEmptyPath(const char* path)
{
    if (*path == '/')
        ++path;
    if (*path == '?')
        ++path;
    return !*path  ||  *path == '#' ? 1/*T*/ : 0/*F*/;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_NonHttpOkay(TSERV_TypeOnly types)
{
    return !types  ||   (types & ~fSERV_Http);
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_HttpNotOkay(TSERV_TypeOnly types)
{
    return  types  &&  !(types &  fSERV_Http);
}


static struct SINTERNAL_Data* s_InternalMapper(const char* name,
                                               TSERV_Type  types,
                                               const char* url)
{
    static int/*bool*/     do_internal = -1/*unassigned*/;
    static void* /*bool*/  s_Once = 0/*false*/;
    unsigned/*bool*/       reverse_dns;
    SConnNetInfo*          net_info;
    struct SINTERNAL_Data* data;
    char*                  frag;

    assert((!name  ||  *name)  &&  url  &&  *url);

    if (CORE_Once(&s_Once)) {
        char        buf[80];
        const char* str = ConnNetInfo_GetValueInternal(0, REG_CONN_INTERNAL_DISABLE,
                                                       buf, sizeof(buf), "0");
        if (str  &&  *str)
            do_internal = !ConnNetInfo_Boolean(str);
    }
    if (!do_internal)
        return 0;
    if (!name)
        name = url;

    reverse_dns = types & fSERV_ReverseDns ? 1/*T*/ : 0/*F*/;
    types &= fSERV_All;
    if (types  &&  !(types & (fSERV_Dns | fSERV_Http | fSERV_Standalone)))
        return 0;
    if (!(data = (struct SINTERNAL_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOGF_ERRNO_X(11, eLOG_Error, errno,
                          ("[%s]  SERV failed to allocate for SINTERNAL_Data", name));
        return 0;
    }
    net_info = &data->net_info;
    ConnNetInfo_MakeValid(net_info);
    if (!ConnNetInfo_ParseWebURL(net_info, url)
        ||  (net_info->scheme != eURL_Unspec  &&
             net_info->scheme != eURL_Https   &&
             net_info->scheme != eURL_Http)
        ||  !net_info->host[0]) {
        goto out;
    }
    data->type = fSERV_Http;
    if (!net_info->port) {
        switch (net_info->scheme) {
        case eURL_Https:
            net_info->port = CONN_PORT_HTTPS;
            break;
        case eURL_Http:
            net_info->port = CONN_PORT_HTTP;
            break;
        default:
            break;            
        }
    } else if (!net_info->scheme/*==eURL_Unspec*/) {
        int/*bool*/ bare = strncmp(url, "//", 2) != 0  &&  !net_info->path[0];
        if (( bare  &&  x_NonHttpOkay(types))  ||
            (!bare  &&  x_HttpNotOkay(types)  &&  x_IsEmptyPath(net_info->path))) {
            data->type = fSERV_Dns | fSERV_Standalone;
            net_info->req_method = eReqMethod_Connect;
            net_info->path[0] = '\0';
        }
    }
    if (types  &&  !(data->type &= types))
        goto out;
    if ((frag = strchr(net_info->path, '#')) != 0) {
        if (frag == net_info->path  &&  (data->type & fSERV_Http))
            *frag++ = '/';
        *frag = '\0';
    }
    data->reverse_dns = reverse_dns & 1;
    if (x_InternalResolve(name, data)) {
        size_t len = SERV_CheckDomain(net_info->host);
        assert(net_info->host[0] != '.');
        if (len)
            net_info->host[len] = '\0';
        return data;
    }

 out:
    free(data);
    return 0;
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
    int exact = s_Fast;
    int/*bool*/
        do_local,
#ifdef NCBI_OS_UNIX
        do_lbsmd   = -1/*unassigned*/,
#endif /*NCBI_OS_UNIX*/
#ifdef NCBI_CXX_TOOLKIT
        do_lbnull  = -1/*unassigned*/,
#  ifdef NCBI_OS_UNIX
        do_lbdns   = -1/*unassigned*/,
#  endif /*NCBI_OS_UNIX*/
        do_linkerd = -1/*unassigned*/,
        do_namerd  = -1/*unassigned*/,
#endif /*NCBI_CXX_TOOLKIT*/
        do_dispd   = -1/*unassigned*/;
    struct SINTERNAL_Data* data = 0;
    const SSERV_VTable* op;
    char* domain = 0;
    const char* svc;
    SERV_ITER iter;
    char* url;

    if ( host_info )
        *host_info = 0;

    svc = s_ServiceName(service, &url, ismask, &exact);
    assert((!svc  ||  *svc  ||  ismask)  &&  (!url  ||  (*url  &&  !ismask)));
    if (url  &&  (data = s_InternalMapper(svc, types, url)) != 0) {
        /* Service resolved to some parsable URL -- proceed internally */
        if (!svc) {
            char* tmp  = ConnNetInfo_URL(&data->net_info);
            if (!tmp)
                /*oops*/;
            else if (strncmp(tmp, "//", 2) == 0  &&  strncmp(url, "//", 2) != 0)
                memmove(tmp, tmp + 2, strlen(tmp + 2) + 1);
            else if ((data->type & (fSERV_Dns | fSERV_Standalone)) == fSERV_Dns)
                tmp[strcspn(tmp, ":")] = '\0';
            svc        = tmp;
        } else
            assert(*svc);
        assert(ismask == 0/*false*/);
        types         &= fSERV_ReverseDns | fSERV_All;
        preferred_host = 0;
        preferred_port = 0;
        preference     = 0.0;
        net_info       = 0;
        n_skip         = 0;
        external       = 0;
        arg = val      = 0;
        exact          = 0;
        if ( info )
            *info      = 0;
    } else if (svc  &&  !url) {
        /* Service name was entirely valid -- use resolver(s) */
        domain = strchr(svc, '.');
        if (domain) {
            /* disable all domain-unaware resolvers */
#ifdef NCBI_OS_UNIX
            do_lbsmd   =
#endif /*NCBI_OS_UNIX*/
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
            do_lbdns   =
#  endif /*NCBI_OS_UNIX*/
            do_linkerd =
            do_namerd  =
#endif /*NCBI_CXX_TOOLKIT*/
            do_dispd   = 0/*false*/;
            *domain++ = '\0';
            assert(*domain  &&  *domain != '.');
        }
        if (strchr(svc, '/')) {
            /* disable all resolvers that cannot handle compound service names*/
#ifdef NCBI_OS_UNIX
            do_lbsmd   =
#endif /*NCBI_OS_UNIX*/
#ifdef NCBI_CXX_TOOLKIT
            do_lbnull  =
#  ifdef NCBI_OS_UNIX
            do_lbdns   =
#  endif /*NCBI_OS_UNIX*/
#endif /*NCBI_CXX_TOOLKIT*/
            do_dispd   = 0/*false*/;
        }
    } else if (svc  ||  url) {
        /* Bad service name or bad URL -- prepare to bail out */
        if (svc) {
            free((void*) svc);
            svc = 0;
        }
    } /* else "service" was NULL/empty (already logged) or no memory, just bail out */
    if (url)
        free((void*) url);

    if (!svc  ||  !(iter = (SERV_ITER) calloc(1, sizeof(*iter)))) {
        if (svc)
            free((void*) svc);
        return 0;
    }
    assert(ismask  ||  *svc);

    iter->name              = svc;
    iter->host              = (preferred_host == SERV_LOCALHOST
                               ? SOCK_GetLocalHostAddress(eDefault)
                               : preferred_host);
    iter->port              =  preferred_port;
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
    iter->exact             = exact;
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
        size_t n;
        for (n = 0;  n < n_skip;  ++n) {
            const char* name = (ismask  ||  skip[n]->type == fSERV_Dns
                                ? SERV_NameOfInfo(skip[n]) : "");
            SSERV_Info* temp = SERV_CopyInfoEx(skip[n],
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

    if (data) {
        iter->data = data;
        op = &kInternalOp;
        goto done;
    }

    if (net_info) {
        if (net_info->external)
            iter->external = 1;
        if (net_info->firewall)
            iter->types |= fSERV_Firewall;
        if (net_info->stateless)
            iter->types |= fSERV_Stateless;
#ifdef NCBI_OS_UNIX
        /* special case of LBSM control */
        if (net_info->lb_disable)
            do_lbsmd = 0/*false*/;
#endif /*NCBI_OS_UNIX*/
    } else {
        /* disable all HTTP-based resolvers */
#ifdef NCBI_CXX_TOOLKIT
        do_linkerd =
        do_namerd  =
#endif /*NCBI_CXX_TOOLKIT*/
        do_dispd   = 0/*false*/;
    }
    if (iter->external) {
        /* disable all resolvers that can't do external */
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
        do_lbdns   =
#  endif /*NCBI_OS_UNIX*/
        do_lbnull  = 0/*false*/;
#endif /*NCBI_CXX_TOOLKIT*/
    }
    if (ismask) {
        /* disable all resolvers that cannot handle service wildcards */
#ifdef NCBI_CXX_TOOLKIT
        do_lbnull  =
#  ifdef NCBI_OS_UNIX
        do_lbdns   =
#  endif /*NCBI_OS_UNIX*/
        do_linkerd =
        do_namerd  = 0/*false*/;
#endif /*NCBI_CXX_TOOLKIT*/
        if (!*svc)
            do_dispd = 0/*false*/;
        svc = 0;
    }

    /* Ugly optimization not to access the registry more than necessary */
    if ((!(do_local  = s_IsMapperConfigured(svc, REG_CONN_LOCAL_ENABLE))   ||
         !(op = SERV_LOCAL_Open(iter, info)))

#ifdef NCBI_CXX_TOOLKIT
        &&
        (!do_lbnull                                                        ||
         !(do_lbnull = s_IsMapperConfigured(svc, REG_CONN_LBNULL_ENABLE))  ||
         !(op = SERV_LBNULL_Open(iter, info, domain)))
#endif /*NCBI_CXX_TOOLKIT*/

#ifdef NCBI_OS_UNIX
        &&
        (!do_lbsmd                                                         ||
         !(do_lbsmd  = !s_IsMapperConfigured(svc, REG_CONN_LBSMD_DISABLE)) ||
         !(op = SERV_LBSMD_Open(iter, info, host_info,
                                (!do_dispd                                 ||
                                 !(do_dispd   = !s_IsMapperConfigured
                                   (svc, REG_CONN_DISPD_DISABLE)))
#  ifdef NCBI_CXX_TOOLKIT
#    ifdef NCBI_OS_UNIX
                                &&
                                (!do_lbdns                                 ||
                                 !(do_lbdns   = s_IsMapperConfigured
                                   (svc, REG_CONN_LBDNS_ENABLE)))
#    endif /*NCBI_OS_UNIX*/
                                &&
                                (!do_linkerd                               ||
                                 !(do_linkerd = s_IsMapperConfigured
                                   (svc, REG_CONN_LINKERD_ENABLE)))
                                &&
                                (!do_namerd                                ||
                                 !(do_namerd  = s_IsMapperConfigured
                                   (svc, REG_CONN_NAMERD_ENABLE)))
#  endif /*NCBI_CXX_TOOLKIT*/
                                )))
#endif /*NCBI_OS_UNIX*/

#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
        &&
        (!do_lbdns                                                         ||
         (do_lbdns < 0    &&  !(do_lbdns   = s_IsMapperConfigured
                                (svc, REG_CONN_LBDNS_ENABLE)))             ||
         !(op = SERV_LBDNS_Open(iter, info)))
#  endif /*NCBI_OS_UNIX*/
        &&
        (!do_linkerd                                                       ||
         (do_linkerd < 0  &&  !(do_linkerd = s_IsMapperConfigured
                                (svc, REG_CONN_LINKERD_ENABLE)))           ||
         !(op = SERV_LINKERD_Open(iter, info, net_info, &do_namerd)))
        &&
        (!do_namerd                                                        ||
         (do_namerd < 0   &&  !(do_namerd  = s_IsMapperConfigured
                                (svc, REG_CONN_NAMERD_ENABLE)))            ||
         !(op = SERV_NAMERD_Open(iter, info, net_info)))
#endif /*NCBI_CXX_TOOLKIT*/

        &&
        (!do_dispd                                                         ||
         (do_dispd < 0    &&  !(do_dispd   = !s_IsMapperConfigured
                                (svc, REG_CONN_DISPD_DISABLE)))            ||
         !(op = SERV_DISPD_Open(iter, info, net_info)))) {
        if (!s_Fast  &&  net_info
            &&  !do_local
#ifdef NCBI_OS_UNIX
            &&  !do_lbsmd
#endif /*NXBI_OS_UNIX*/
#ifdef NCBI_CXX_TOOLKIT
#  ifdef NCBI_OS_UNIX
            &&  !do_lbdns
#  endif /*NCBI_OS_UNIX*/
            &&  !do_lbnull  &&  !do_linkerd  &&  !do_namerd
#endif /*NCBI_CXX_TOOLKIT*/
            &&  !do_dispd) {
            if (svc  &&  strcasecmp(service, svc) == 0)
                svc = 0;
            assert(*service  ||  !svc);
            CORE_LOGF_X(1, eLOG_Error,
                        ("%s%s%s%s%sNo service mappers available",
                         &"["[!*service], service,
                         &"|"[!svc], svc ? svc : "",
                         *service ? "]  " : ""));
        }
        SERV_Close(iter);
        return 0;
    }

 done:
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
            ++n;
    }
}


#if defined(_DEBUG)  &&  !defined(NDEBUG)

#define x_RETURN(retval)  return x_Return((SSERV_Info*) info, infostr, retval)

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
    const char*    infostr = SERV_WriteInfo(info);
    const char*    name = SERV_NameOfInfo(info);
    TSERV_TypeOnly types;
    size_t         n;

    if (!name) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  NULL name @%p\n%s", iter->name, info,
                   infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if (!(iter->ismask | iter->reverse_dns) != !*name) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  %s name \"%s\" @%p\n%s", iter->name,
                   *name ? "Unexpected" : "Empty", name,
                   info, infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if (!info->host  ||  !info->port) {
        if (info->type != fSERV_Dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Non-DNS server with empty %s @%p:\n%s",
                       iter->name, !(info->host | info->port) ? "host:port"
                       : info->host ? "port" : "host",
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (!info->host  &&  (iter->last  ||  iter->ismask)) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Interim DNS server w/o host @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }
    if (info->time) {
        /* allow up to 10% of "excessive expiration" */
        static const TNCBI_Time delta
            = (LBSM_DEFAULT_TIME * 11 + LBSM_DEFAULT_TIME % 10) / 10;
        if (info->time < iter->time) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Expired entry (%u < %u) @%p:\n%s", iter->name,
                       info->time, iter->time,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        /* for the case of n/w delay */
        iter->time = (TNCBI_Time) time(0);
        if (info->time > iter->time + delta) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Excessive expiration (%u) @%p:\n%s", iter->name,
                       info->time - iter->time,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }
    if (info->type == fSERV_Firewall) {
        if (info->u.firewall.type == fSERV_Dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Firewall DNS entry not allowed @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (info->vhost | info->extra) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Firewall entry with %s%s%s @%p:\n%s", iter->name,
                       info->vhost                  ? "vhost" : "",
                       info->extra  &&  info->vhost ? " and " : "",
                       info->extra                  ? "extra" : "",
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }
    if (info->type == fSERV_Dns) {
        if (info->site & fSERV_Private) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry cannot be private @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (info->mode & fSERV_Stateful) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry cannot be stateful @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (info->mode & fSERV_Secure) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry cannot be secure @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (info->mime_t != eMIME_T_Undefined  ||
            info->mime_s != eMIME_Undefined    ||
            info->mime_e != eENCOD_None) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry with MIME type @%p:\n%s", iter->name,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }
    if ((info->type & fSERV_Http)  &&  (info->mode & fSERV_Stateful)) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  HTTP entry cannot be stateful @%p:\n%s", iter->name,
                   info, infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if (!(types = iter->types &
          (TSERV_TypeOnly)(~(fSERV_Stateless | fSERV_Firewall)))) {
        if (info->type == fSERV_Dns  &&  !iter->reverse_dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  DNS entry unwarranted @%p:\n%s", iter->name,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    } else if ((info->type != fSERV_Firewall
                &&  !(types & info->type))  ||
               (info->type == fSERV_Firewall
                &&  !(types & info->u.firewall.type))) {
        if (info->type != fSERV_Dns  ||  !iter->reverse_dns) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Mismatched type 0x%X vs 0x%X @%p:\n%s",
                       iter->name, (int)(info->type == fSERV_Firewall
                                         ? info->u.firewall.type
                                         : info->type), (int) types,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }
    if (iter->external  &&  (info->site & (fSERV_Local | fSERV_Private))) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Local/private entry for external @%p:\n%s",
                   iter->name, info, infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if ((info->site & fSERV_Private)  &&  !iter->ok_private
        &&  iter->localhost  &&  info->host != iter->localhost) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Private entry unwarranted @%p:\n%s", iter->name,
                   info, infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if ((iter->types & fSERV_Stateless)  &&  (info->mode & fSERV_Stateful)) {
        CORE_LOGF(eLOG_Critical,
                  ("[%s]  Stateful entry in stateless search @%p:\n%s",
                   iter->name, info, infostr ? infostr : "<NULL>"));
        x_RETURN(0/*failure*/);
    }
    if (info->type != fSERV_Dns  ||  info->host) {
        if (!info->time  &&  !info->rate) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Off entry not allowed @%p:\n%s", iter->name,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (SERV_IfSuppressed(info)  &&  !iter->ok_suppressed) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Suppressed entry unwarranted @%p:\n%s",
                       iter->name, info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (SERV_IsDown(info)  &&  !iter->ok_down) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Down entry unwarranted @%p:\n%s", iter->name,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (SERV_IsReserved(info)  &&  !iter->ok_reserved) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Reserved entry unwarranted @%p:\n%s", iter->name,
                       info, infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
        if (SERV_IsStandby(info)  &&  !iter->ok_standby) {
            for (n = 0;  n < iter->n_skip;  ++n) {
                if (!SERV_IsStandby(iter->skip[n])) {
                    CORE_LOGF(eLOG_Critical,
                              ("[%s]  Standby entry unwarranted @%p:\n%s",
                               iter->name, info, infostr? infostr : "<NULL>"));
                    x_RETURN(0/*failure*/);
                }
            }
        }
    }
    for (n = 0;  n < iter->n_skip;  ++n) {
        const SSERV_Info* skip = iter->skip[n];
        if (strcasecmp(name, SERV_NameOfInfo(skip)) == 0
            &&  SERV_EqualInfo(info, skip)) {
            CORE_LOGF(eLOG_Critical,
                      ("[%s]  Entry is a duplicate of @%p and must be skipped"
                       " @%p:\n%s%s%s%s", iter->name, skip, info,
                       &"\""[!*name], name, *name ? "\" " : "",
                       infostr ? infostr : "<NULL>"));
            x_RETURN(0/*failure*/);
        }
    }

    CORE_LOGF(eLOG_Trace, ("[%s]  Consistency check passed @%p:\n%s%s%s%s",
                           iter->name, info, &"\""[!*name], name,
                           *name ? "\" " : "", infostr ? infostr : "<NULL>"));
    x_RETURN(1/*success*/);
}

#undef x_RETURN

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
            /* NB:  This should never be actually used for LBSM mapper,
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
    const char* name;
    if (!iter)
        return 0;
    assert(iter->name);
    name = SERV_NameOfInfo(iter->last);
    return name  &&  *name ? name : iter->name;
}


extern int/*bool*/ SERV_PenalizeEx(SERV_ITER iter, double fine, TNCBI_Time time)
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
    size_t n;
    if (!iter)
        return;
    SERV_Reset(iter);
    for (n = 0;  n < iter->n_skip;  ++n)
        free((void*) iter->skip[n]);
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

        if (len  &&  s[len - 1] == '\r')
            --len;
        if (!len  ||  !(q = (char*) malloc(len + 1)))
            continue;
        memcpy(q, s, len);
        q[len] = '\0';
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
        TSERV_TypeOnly types;
        if (iter->external) {
            /* External */
            if (!BUF_Write(&buf, kNcbiExternal, sizeof(kNcbiExternal)-1)) {
                BUF_Destroy(buf);
                return 0;
            }
        }
        if ((types = (iter->types & fSERV_All)) != fSERV_Any) {
            /* Accepted server types */
            TSERV_TypeOnly t;
            buflen = 0;
            for (t = 1;  t;  t <<= 1) {
                if (types & t) {
                    const char* name = SERV_TypeStr((ESERV_Type) t);
                    size_t namelen = strlen(name);
                    if (!namelen)
                        continue;
                    if (buflen + namelen + (1 + 2) > sizeof(buffer))
                        break;
                    buffer[buflen++] = ' ';
                    memcpy(buffer + buflen, name, namelen);
                    buflen += namelen;
                } else if (types < t)
                    break;
            }
            if (buflen) {
                memcpy(buffer + buflen, "\r\n", 2);
                if (!BUF_Write(&buf, kAcceptedServerTypes,
                               sizeof(kAcceptedServerTypes) - 1)
                    ||  !BUF_Write(&buf, buffer, buflen + 2)) {
                    BUF_Destroy(buf);
                    return 0;
                }
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
                                     "%\r\n") - buffer) + 3/*"%\r\n"*/;
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
    if (!(info = SERV_GetInfoP(name, fSERV_Promiscuous | fSERV_Standalone,
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
    char* buf, *svc = service  &&  *service ? SERV_ServiceName(service) : 0;
    const char* typ;
    size_t len;

    if (!svc  ||  !*(typ = SERV_TypeStr(type)))
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
    x_mkenv(strupr(buf), len);
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
#  ifdef NCBI_OS_MSWIN
#    define putenv _putenv
#  endif /*NCBI_OS_MSWIN*/
    len = !putenv(buf);
#endif /*HAVE_SETENV*/
    CORE_UNLOCK;

#ifndef HAVE_SETENV
    if (!len)
#endif /*!HAVE_SETENV*/
        free(buf);
    return len ? 1/*success*/ : 0/*failure*/;
}


#define _SERV_MERGE(a, b)  a ## b
#define  SERV_MERGE(a, b)  _SERV_MERGE(a, b)


#define SERV_GET_IMPLICIT_SERVER_TYPE(variant)                              \
    ESERV_Type SERV_MERGE(SERV_GetImplicitServerType,                       \
                          _SERV_MERGE(variant, Ex))                         \
        (const char* service, ESERV_Type default_type)                      \
    {                                                                       \
        ESERV_Type type;                                                    \
        const char *end;                                                    \
        char val[40];                                                       \
        /* Try to retrieve service-specific first, then global default */   \
        if (!_SERV_MERGE(ConnNetInfo_GetValue, variant)                     \
            (service, REG_CONN_IMPLICIT_SERVER_TYPE, val, sizeof(val), 0)   \
            ||  !*val  ||  !(end = SERV_ReadType(val, &type))  ||  *end) {  \
            return default_type;                                            \
        }                                                                   \
        return type;                                                        \
    }                                                                       \


/* Public API */
#define SERV_GET_IMPLICIT_SERVER_TYPE_PUBLIC_API
static  SERV_GET_IMPLICIT_SERVER_TYPE(SERV_GET_IMPLICIT_SERVER_TYPE_PUBLIC_API)
#undef  SERV_GET_IMPLICIT_SERVER_TYPE_PUBLIC_API

extern ESERV_Type SERV_GetImplicitServerType(const char* service)
{
    return SERV_GetImplicitServerTypeEx(service, SERV_GetImplicitServerTypeDefault());
}


/* Internal API */
SERV_GET_IMPLICIT_SERVER_TYPE(Internal)

ESERV_Type SERV_GetImplicitServerTypeInternal(const char* service)
{
    return SERV_GetImplicitServerTypeInternalEx(service, SERV_GetImplicitServerTypeDefault());
}


#undef SERV_GET_IMPLICIT_SERVER_TYPE

#undef  SERV_MERGE
#undef _SERV_MERGE


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


ESwitch SERV_DoFastOpens(ESwitch on)
{
    ESwitch retval = s_Fast;
    if (on != eDefault)
        s_Fast = on;
    return retval;
}
